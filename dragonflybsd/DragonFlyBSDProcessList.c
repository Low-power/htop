/*
htop - dragonflybsd/DragonFlyBSDProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Copyright 2015-2026 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "ProcessList.h"
#include "Hashtable.h"
#include <sys/types.h>
#include <kvm.h>

typedef struct CPUData_ {
   double userPercent;
   double nicePercent;
   double systemPercent;
   double irqPercent;
   double idlePercent;
   double systemAllPercent;
} CPUData;

typedef struct DragonFlyBSDProcessList_ {
   ProcessList super;

   kvm_t* kd;

   unsigned long long int memWire;
   unsigned long long int memActive;
   unsigned long long int memInactive;
   unsigned long long int memFree;
   unsigned long long int buffers_size;

   CPUData* cpus;

   long int *cp_time_o;
   long int *cp_time_n;

   long int *cp_times_o;
   long int *cp_times_n;

   Hashtable *jails;
} DragonFlyBSDProcessList;
}*/

#include <DragonFlyBSDProcessList.h>
#include <DragonFlyBSDProcess.h>
#include <Platform.h>
#include <CRT.h>
#include <sys/sysctl.h>
#include <sys/kinfo.h>
#include <sys/jail.h>
#include <sys/uio.h>
#include <sys/resource.h>
#include <sys/user.h>
#include <kinfo.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <err.h>

#define _UNUSED_ __attribute__((unused))

static int kernelFScale;

ProcessList* ProcessList_new(UsersTable* usersTable, const Hashtable *pidWhiteList, uid_t userId) {
   DragonFlyBSDProcessList* dfpl = xCalloc(1, sizeof(DragonFlyBSDProcessList));
   ProcessList* pl = (ProcessList*) dfpl;
   ProcessList_init(pl, Class(DragonFlyBSDProcess), usersTable, pidWhiteList, userId);

   unsigned int page_size;
   size_t len = sizeof page_size;
   if (sysctlbyname("vm.stats.vm.v_page_size", &page_size, &len, NULL, 0) == 0 && page_size != CRT_page_size) {
      // How can this happen?
      CRT_page_size = page_size;
      CRT_page_size_kibibyte = page_size / ONE_BINARY_K;
   }

   int cpus = 1;
   len = sizeof(cpus);
   if (sysctlbyname("hw.ncpu", &cpus, &len, NULL, 0) != 0) {
      cpus = 1;
   }

   size_t sizeof_cp_time_array = sizeof(long int) * CPUSTATES;
   dfpl->cp_time_o = xCalloc(cpus, sizeof_cp_time_array);
   dfpl->cp_time_n = xCalloc(cpus, sizeof_cp_time_array);
   len = sizeof_cp_time_array;

   // fetch initial single (or average) CPU clicks from kernel
   sysctl(platform.kern_cp_time_mib, 2, dfpl->cp_time_o, &len, NULL, 0);

   // on smp box, fetch rest of initial CPU's clicks
   if (cpus > 1) {
      dfpl->cp_times_o = xCalloc(cpus, sizeof_cp_time_array);
      dfpl->cp_times_n = xCalloc(cpus, sizeof_cp_time_array);
      len = cpus * sizeof_cp_time_array;
      sysctl(platform.kern_cp_times_mib, 2, dfpl->cp_times_o, &len, NULL, 0);
   }

   pl->cpuCount = MAX(cpus, 1);

   if (cpus == 1 ) {
     dfpl->cpus = xRealloc(dfpl->cpus, sizeof(CPUData));
   } else {
     // on smp we need CPUs + 1 to store averages too (as kernel kindly provides that as well)
     dfpl->cpus = xRealloc(dfpl->cpus, (pl->cpuCount + 1) * sizeof(CPUData));
   }

   len = sizeof(kernelFScale);
   if (sysctlbyname("kern.fscale", &kernelFScale, &len, NULL, 0) == -1) {
      //sane default for kernel provided CPU percentage scaling, at least on x86 machines, in case this sysctl call failed
      kernelFScale = 2048;
   }

   char errbuf[_POSIX2_LINE_MAX];
   dfpl->kd = kvm_openfiles(NULL, "/dev/null", NULL, 0, errbuf);
   if (dfpl->kd == NULL) {
      errx(1, "kvm_open: %s", errbuf);
   }

   return pl;
}

void ProcessList_delete(ProcessList* this) {
   const DragonFlyBSDProcessList* dfpl = (DragonFlyBSDProcessList*) this;
   if (dfpl->kd) kvm_close(dfpl->kd);

   if (dfpl->jails) {
      Hashtable_delete(dfpl->jails);
   }
   free(dfpl->cp_time_o);
   free(dfpl->cp_time_n);
   free(dfpl->cp_times_o);
   free(dfpl->cp_times_n);
   free(dfpl->cpus);

   ProcessList_done(this);
   free(this);
}

static inline void DragonFlyBSDProcessList_scanCPUTime(ProcessList* pl) {
   const DragonFlyBSDProcessList* dfpl = (DragonFlyBSDProcessList*) pl;

   int cpus   = pl->cpuCount;   // actual CPU count
   int maxcpu = cpus;           // max iteration (in case we have average + smp)
   int cp_times_offset;

   assert(cpus > 0);

   size_t sizeof_cp_time_array;

   long int *cp_time_n; // old clicks state
   long int *cp_time_o; // current clicks state

   long int cp_time_d[CPUSTATES];
   double cp_time_p[CPUSTATES];

   // get averages or single CPU clicks
   sizeof_cp_time_array = sizeof(long int) * CPUSTATES;
   if(sysctl(platform.kern_cp_time_mib, 2, dfpl->cp_time_n, &sizeof_cp_time_array, NULL, 0) < 0) {
      return;
   }

   // get rest of CPUs
   if (cpus > 1) {
      // on smp systems DragonFlyBSD kernel concats all CPU states into one long array in
      // kern.cp_times sysctl OID
      // we store averages in dfpl->cpus[0], and actual cores after that
      maxcpu = cpus + 1;
      sizeof_cp_time_array = cpus * sizeof(long int) * CPUSTATES;
      if(sysctl(platform.kern_cp_times_mib, 2, dfpl->cp_times_n, &sizeof_cp_time_array, NULL, 0) < 0) {
         return;
      }
   }

   for (int i = 0; i < maxcpu; i++) {
      if (cpus == 1) {
         // single CPU box
         cp_time_n = dfpl->cp_time_n;
         cp_time_o = dfpl->cp_time_o;
      } else {
         if (i == 0 ) {
           // average
           cp_time_n = dfpl->cp_time_n;
           cp_time_o = dfpl->cp_time_o;
         } else {
           // specific smp cores
           cp_times_offset = i - 1;
           cp_time_n = dfpl->cp_times_n + (cp_times_offset * CPUSTATES);
           cp_time_o = dfpl->cp_times_o + (cp_times_offset * CPUSTATES);
         }
      }

      // diff old vs new
      unsigned long long total_o = 0;
      unsigned long long total_n = 0;
      unsigned long long total_d = 0;
      for (int s = 0; s < CPUSTATES; s++) {
        cp_time_d[s] = cp_time_n[s] - cp_time_o[s];
        total_o += cp_time_o[s];
        total_n += cp_time_n[s];
      }

      // totals
      total_d = total_n - total_o;
      if (total_d < 1 ) total_d = 1;

      // save current state as old and calc percentages
      for (int s = 0; s < CPUSTATES; ++s) {
        cp_time_o[s] = cp_time_n[s];
        cp_time_p[s] = ((double)cp_time_d[s]) / ((double)total_d) * 100;
      }

      CPUData* cpuData = &(dfpl->cpus[i]);
      cpuData->userPercent      = cp_time_p[CP_USER];
      cpuData->nicePercent      = cp_time_p[CP_NICE];
      cpuData->systemPercent    = cp_time_p[CP_SYS];
      cpuData->irqPercent       = cp_time_p[CP_INTR];
      cpuData->systemAllPercent = cp_time_p[CP_SYS] + cp_time_p[CP_INTR];
      // this one is not really used, but we store it anyway
      cpuData->idlePercent      = cp_time_p[CP_IDLE];
   }
}

static inline void DragonFlyBSDProcessList_scanMemoryInfo(ProcessList* pl) {
   DragonFlyBSDProcessList* dfpl = (DragonFlyBSDProcessList*) pl;

   union {
      unsigned int v_uint;
      long int v_long;
      unsigned long int v_ulong;
   } buffer;
   size_t len;

   // memory counter relationships seem to be these:
   //  total = active + wired + inactive + cache + free
   //  htop_used (unavail to anybody) = active + wired + inactive - buffers
   //  htop_buffers (for cache meter) = buffers
   //  htop_cache (for cache meter)   = cache
   //  user_free (avail to procs)     = buffers + cache + free

   // disabled for now, as it is always smaller than phycal amount of memory...
   // ...to avoid "where is my memory?" questions
   //len = sizeof buffer.v_uint;
   //sysctl(platform.vm_stats_vm_v_page_count_mib, 4, &buffer, &len, NULL, 0);
   //pl->totalMem = buffer.v_uint * CRT_page_size_kibibyte;
   len = sizeof buffer.v_ulong;
   if(sysctl(platform.hw_physmem_mib, 2, &buffer, &len, NULL, 0) < 0) goto fail;
   pl->totalMem = buffer.v_ulong / 1024;

   len = sizeof buffer.v_uint;
   if(sysctl(platform.vm_stats_vm_v_active_count_mib, 4, &buffer, &len, NULL, 0) < 0) goto fail;
   dfpl->memActive = buffer.v_uint * CRT_page_size_kibibyte;

   len = sizeof buffer.v_uint;
   if(sysctl(platform.vm_stats_vm_v_wire_count_mib, 4, &buffer, &len, NULL, 0) < 0) goto fail;
   dfpl->memWire = buffer.v_uint * CRT_page_size_kibibyte;

   len = sizeof buffer.v_uint;
   sysctl(platform.vm_stats_vm_v_inactive_count_mib, 4, &buffer, &len, NULL, 0);
   dfpl->memInactive = buffer.v_uint * CRT_page_size_kibibyte;

   len = sizeof buffer.v_long;
   if(sysctl(platform.vfs_bufspace_mib, 2, &buffer, &len, NULL, 0) < 0) goto fail;
   dfpl->buffers_size = buffer.v_long / 1024;

   len = sizeof buffer.v_uint;
   if(sysctl(platform.vm_stats_vm_v_cache_count_mib, 4, &buffer, &len, NULL, 0) < 0) goto fail;
   pl->cachedMem = buffer.v_uint * CRT_page_size_kibibyte;

   pl->usedMem = dfpl->memActive + dfpl->memWire + dfpl->memInactive - dfpl->buffers_size;


   struct kvm_swap swap[16];
   int nswap = kvm_getswapinfo(dfpl->kd, swap, sizeof(swap)/sizeof(swap[0]), 0);
   pl->totalSwap = 0;
   pl->usedSwap = 0;
   for (int i = 0; i < nswap; i++) {
      pl->totalSwap += swap[i].ksw_total;
      pl->usedSwap += swap[i].ksw_used;
   }
   pl->totalSwap *= CRT_page_size_kibibyte;
   pl->usedSwap *= CRT_page_size_kibibyte;

   return;

fail:
   pl->totalMem = 0;
   pl->cachedMem = 0;
   pl->usedMem = 0;
   pl->totalSwap = 0;
   pl->usedSwap = 0;
   dfpl->buffers_size = 0;
}

static void DragonFlyBSDProcessList_readProcessName(kvm_t* kd, struct kinfo_proc* kproc, char **name, char **command, int *argv0_len) {
   *name = xStrdup(kproc->kp_comm);
   char** argv = kvm_getargv(kd, kproc, 0);
   if (!argv || !*argv) {
      *command = xStrdup(kproc->kp_comm);
      *argv0_len = strlen(kproc->kp_comm);
      return;
   }
   int len = 0;
   for (int i = 0; argv[i]; i++) {
      if(i) {
         len += strlen(argv[i]) + 1;
      } else {
         len = strlen(argv[i]);
         *argv0_len = len++;
      }
   }
   *command = xMalloc(len);
   char* at = *command;
   for (int i = 0; argv[i]; i++) {
      if(i) *at++ = ' ';
      at = stpcpy(at, argv[i]);
   }
}

static inline void DragonFlyBSDProcessList_scanJails(DragonFlyBSDProcessList* dfpl) {
   size_t len;
   char *jls; /* Jail list */
   char *curpos;
   char *nextpos;

retry:
   if (sysctlbyname("jail.list", NULL, &len, NULL, 0) == -1) {
      fprintf(stderr, "initial sysctlbyname / jail.list failed\n");
      exit(3);
   }
   if (len == 0)
      return;

   jls = xMalloc(len);
   if (jls == NULL) {
      fprintf(stderr, "xMalloc failed\n");
      exit(4);
   }
   if (sysctlbyname("jail.list", jls, &len, NULL, 0) == -1) {
      if (errno == ENOMEM) {
         free(jls);
         goto retry;
      }
      fprintf(stderr, "sysctlbyname / jail.list failed\n");
      exit(5);
   }

   if (dfpl->jails) {
      Hashtable_delete(dfpl->jails);
   }
   dfpl->jails = Hashtable_new(20, true);
   curpos = jls;
   while (curpos) {
      int jailid;
      char *str_hostname;
      nextpos = strchr(curpos, '\n');
      if (nextpos)
         *nextpos++ = 0;

      jailid = atoi(strtok(curpos, " "));
      str_hostname = strtok(NULL, " ");

      char *jname = Hashtable_get(dfpl->jails, jailid);
      if (jname == NULL) {
         jname = xStrdup(str_hostname);
         Hashtable_put(dfpl->jails, jailid, jname);
      }

      curpos = nextpos;
  }
  free(jls);
}

static char *DragonFlyBSDProcessList_readJailName(DragonFlyBSDProcessList* dfpl, int jailid) {
   char *hostname;
   if (jailid != 0 && dfpl->jails && (hostname = Hashtable_get(dfpl->jails, jailid))) {
      return xStrdup(hostname);
   } else {
      return xStrdup("-");
   }
}

void ProcessList_goThroughEntries(ProcessList* this, bool skip_processes) {
   DragonFlyBSDProcessList* dfpl = (DragonFlyBSDProcessList*) this;

   DragonFlyBSDProcessList_scanMemoryInfo(this);
   DragonFlyBSDProcessList_scanCPUTime(this);
   DragonFlyBSDProcessList_scanJails(dfpl);

   if(skip_processes) return;

   int count = 0;
   struct kinfo_proc* kprocs = kvm_getprocs(dfpl->kd, KERN_PROC_ALL, 0, &count);
   for (int i = 0; i < count; i++) {
      struct kinfo_proc* kproc = &kprocs[i];
      bool preExisting;

      // note: dragonflybsd kernel processes all have the same pid, so we misuse the kernel thread address to give them a unique identifier
      pid_t pid = kproc->kp_pid != 1 && (kproc->kp_flags & P_SYSTEM) && kproc->kp_ktaddr ?
         (pid_t)kproc->kp_ktaddr : kproc->kp_pid;
      Process* proc = ProcessList_getProcess(this, pid, &preExisting, (Process_New) DragonFlyBSDProcess_new);
      DragonFlyBSDProcess* dfp = (DragonFlyBSDProcess*) proc;

      proc->ppid = kproc->kp_ppid;		// parent process id
      proc->tpgid = kproc->kp_tpgid;		// tty process group id
      proc->tgid = kproc->kp_pid;		// thread group id
      proc->pgrp = kproc->kp_pgid;		// process group id
      proc->session = kproc->kp_sid;
      proc->tty_nr = kproc->kp_tdev;		// control terminal device number

      if (!preExisting) {
         dfp->jid = kproc->kp_jailid;
         dfp->kernel = kproc->kp_pid != 1 && (kproc->kp_flags & P_SYSTEM);
         proc->ruid = kproc->kp_ruid;		// real user ID
         proc->euid = kproc->kp_uid;		// effective user ID
         proc->processor = kproc->kp_lwp.kl_origcpu;
         proc->real_user = UsersTable_getRef(this->usersTable, proc->ruid);
         proc->effective_user = UsersTable_getRef(this->usersTable, proc->euid);
         proc->starttime_ctime = kproc->kp_start.tv_sec;

         ProcessList_add((ProcessList*)this, proc);
         DragonFlyBSDProcessList_readProcessName(dfpl->kd, kproc, &proc->name, &proc->comm, &proc->argv0_length);
         dfp->jname = DragonFlyBSDProcessList_readJailName(dfpl, kproc->kp_jailid);
      } else {
         proc->processor = kproc->kp_lwp.kl_cpuid;
         if(dfp->jid != kproc->kp_jailid) {	// process can enter jail anytime
            dfp->jid = kproc->kp_jailid;
            free(dfp->jname);
            dfp->jname = DragonFlyBSDProcessList_readJailName(dfpl, kproc->kp_jailid);
         }
         // some processes change users (eg. to lower privs)
         if(proc->ruid != kproc->kp_ruid) {
            proc->ruid = kproc->kp_ruid;
            proc->real_user = UsersTable_getRef(this->usersTable, proc->ruid);
         }
         if(proc->euid != kproc->kp_uid) {
            proc->euid = kproc->kp_uid;
            proc->effective_user = UsersTable_getRef(this->usersTable, proc->euid);
         }
         if (ProcessList_shouldUpdateProcessNames(this)) {
            free(proc->name);
            free(proc->comm);
            DragonFlyBSDProcessList_readProcessName(dfpl->kd, kproc, &proc->name, &proc->comm, &proc->argv0_length);
         }
      }

      proc->m_size = kproc->kp_vm_map_size / CRT_page_size;
      proc->m_resident = kproc->kp_vm_rssize;
      proc->percent_mem =
         (double)proc->m_resident / (double)(this->totalMem / CRT_page_size_kibibyte) * 100;
      proc->nlwp = kproc->kp_nthreads;		// number of lwp thread
      proc->time = 
         (kproc->kp_lwp.kl_uticks + kproc->kp_lwp.kl_sticks + kproc->kp_lwp.kl_iticks) / 10000;
      proc->percent_cpu = (double)kproc->kp_lwp.kl_pctcpu / (double)kernelFScale * 100;

      proc->priority = kproc->kp_lwp.kl_pid == -1 ? -kproc->kp_lwp.kl_tdprio : kproc->kp_lwp.kl_prio;
      switch(kproc->kp_lwp.kl_rtprio.type) {
         case RTP_PRIO_REALTIME:
            proc->nice = PRIO_MIN - 1 - RTP_PRIO_MAX + kproc->kp_lwp.kl_rtprio.prio;
            break;
         case RTP_PRIO_IDLE:
            proc->nice = PRIO_MAX + 1 + kproc->kp_lwp.kl_rtprio.prio;
            break;
         case RTP_PRIO_THREAD:
            proc->nice = PRIO_MIN - 1 - RTP_PRIO_MAX - kproc->kp_lwp.kl_rtprio.prio;
            break;
         default:
            proc->nice = kproc->kp_nice;
            break;
      }

      // would be nice if we could store multiple states in proc->state (as enum) and have writeField render them
      switch (kproc->kp_stat) {
         case SIDL:
            proc->state = 'I';
            break;
         case SACTIVE:
            switch (kproc->kp_lwp.kl_stat) {
               case LSSLEEP:
                  if (kproc->kp_lwp.kl_flags & LWP_SINTR) {
                     // interruptable wait long/short
                     proc->state = kproc->kp_lwp.kl_slptime > MAXSLP ? 'I' : 'S';
                  } else if (kproc->kp_lwp.kl_tdflags & TDF_SINTR) {
                     // interruptable lwkt wait
                     proc->state = 'S';
                  } else if (kproc->kp_paddr) {
                     // uninterruptable wait
                     proc->state = 'D';
                  } else {
                     // uninterruptable lwkt wait
                     proc->state = 'B';
                  }
                  break;
               case LSRUN:
                  // running or runnable
                  //proc->state = (kproc->kp_lwp.kl_tdflags & (TDF_RUNNING | TDF_RUNQ)) ? 'O' : 'R';
                  proc->state = 'R';
                  break;
               case LSSTOP:
                  proc->state = 'T';
                  break;
               default:
                  proc->state = 'A';
                  break;
            }
            break;
         case SSTOP:
            proc->state = 'T';
            break;
         case SZOMB:
            proc->state = 'Z';
            break;
         case SCORE:
            proc->state = 'C';
            break;
         default:
            proc->state = '?';
            break;
      }

      this->totalTasks++;
      this->thread_count += proc->nlwp;
      if (Process_isKernelProcess(proc)) {
         this->kernel_process_count++;
         this->kernel_thread_count += proc->nlwp;
      }
      if (proc->state == 'R') {
         this->running_process_count++;
         this->running_thread_count++;
      }
      proc->show = !(this->settings->hide_kernel_processes && Process_isKernelProcess(proc));
      proc->updated = true;
   }
}
