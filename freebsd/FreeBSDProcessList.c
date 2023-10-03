/*
htop - freebsd/FreeBSDProcessList.c
(C) 2014 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "config.h"
#ifdef HAVE_LIBKVM
#include <kvm.h>
#endif
#include <sys/param.h>
#include <sys/jail.h>
#include <sys/uio.h>
#include <sys/resource.h>

typedef struct CPUData_ {
   double userPercent;
   double nicePercent;
   double systemPercent;
   double irqPercent;
   double idlePercent;
   double systemAllPercent;
} CPUData;

typedef struct FreeBSDProcessList_ {
   ProcessList super;
#ifdef HAVE_LIBKVM
   kvm_t* kd;
#else
   struct kinfo_proc *kip_buffer;
   size_t kip_buffer_size;
#endif
   unsigned long long int memWire;
   unsigned long long int memActive;
   unsigned long long int memInactive;
   unsigned long long int memFree;
   unsigned long long int laundry_size;

   CPUData* cpus;

   long int *cp_time_o;
   long int *cp_time_n;

   long int *cp_times_o;
   long int *cp_times_n;

   int arg_max;
} FreeBSDProcessList;

}*/

#include "ProcessList.h"
#include "FreeBSDProcessList.h"
#include "FreeBSDProcess.h"
#include "CRT.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/vmmeter.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

static int MIB_hw_physmem[2];
static int MIB_vm_stats_vm_v_page_count[4];
static int MIB_vm_stats_vm_v_wire_count[4];
static int MIB_vm_stats_vm_v_active_count[4];
static int *MIB_vm_stats_vm_v_cache_count;
static int MIB_vm_stats_vm_v_inactive_count[4];
static int MIB_vm_stats_vm_v_free_count[4];
static int *v_laundry_count_mib;
static int MIB_vfs_bufspace[2];
static int MIB_kern_cp_time[2];
static int MIB_kern_cp_times[2];
static int kernelFScale;

#ifdef __GLIBC__
// GNU C Library defines NZERO to 20, which is incorrect for kFreeBSD
#undef NZERO
#define NZERO 0
#endif

ProcessList* ProcessList_new(UsersTable* usersTable, const Hashtable *pidWhiteList, uid_t userId) {
   size_t len;
   FreeBSDProcessList* fpl = xCalloc(1, sizeof(FreeBSDProcessList));
   ProcessList* pl = (ProcessList*) fpl;
   ProcessList_init(pl, Class(FreeBSDProcess), usersTable, pidWhiteList, userId);

   // physical memory in system: hw.physmem
   // physical page size: hw.pagesize
   // usable pagesize : vm.stats.vm.v_page_size
   len = 2; sysctlnametomib("hw.physmem", MIB_hw_physmem, &len);

   unsigned int page_size;
   len = sizeof page_size;
   if (sysctlbyname("vm.stats.vm.v_page_size", &page_size, &len, NULL, 0) == 0 && page_size != CRT_page_size) {
      // How can this happen?
      CRT_page_size = page_size;
      CRT_page_size_kib = page_size / ONE_BINARY_K;
   }

   int mib[4];

   // usable page count vm.stats.vm.v_page_count
   // actually usable memory : vm.stats.vm.v_page_count * vm.stats.vm.v_page_size
   len = 4; sysctlnametomib("vm.stats.vm.v_page_count", MIB_vm_stats_vm_v_page_count, &len);

   len = 4; sysctlnametomib("vm.stats.vm.v_wire_count", MIB_vm_stats_vm_v_wire_count, &len);
   len = 4; sysctlnametomib("vm.stats.vm.v_active_count", MIB_vm_stats_vm_v_active_count, &len);

   len = 4;
   if(sysctlnametomib("vm.stats.vm.v_cache_count", mib, &len) == 0) {
      assert(len == 4);
      MIB_vm_stats_vm_v_cache_count = xMalloc(sizeof mib);
      memcpy(MIB_vm_stats_vm_v_cache_count, mib, sizeof mib);
   }

   len = 4; sysctlnametomib("vm.stats.vm.v_inactive_count", MIB_vm_stats_vm_v_inactive_count, &len);
   len = 4; sysctlnametomib("vm.stats.vm.v_free_count", MIB_vm_stats_vm_v_free_count, &len);

   len = 2; sysctlnametomib("vfs.bufspace", MIB_vfs_bufspace, &len);

   len = 4;
   if(sysctlnametomib("vm.stats.vm.v_laundry_count", mib, &len) == 0) {
      assert(len == 4);
      v_laundry_count_mib = xMalloc(sizeof mib);
      memcpy(v_laundry_count_mib, mib, sizeof mib);
   }

   int smp = 0;
   len = sizeof(smp);
   if (sysctlbyname("kern.smp.active", &smp, &len, NULL, 0) != 0 || len != sizeof(smp)) {
      smp = 0;
   }

   int cpus;
   len = sizeof(cpus);
   if(!smp || sysctlbyname("kern.smp.cpus", &cpus, &len, NULL, 0) < 0) cpus = 1;

   size_t sizeof_cp_time_array = sizeof(long int) * CPUSTATES;
   len = 2; sysctlnametomib("kern.cp_time", MIB_kern_cp_time, &len);
   fpl->cp_time_o = xCalloc(cpus, sizeof_cp_time_array);
   fpl->cp_time_n = xCalloc(cpus, sizeof_cp_time_array);
   len = sizeof_cp_time_array;

   // fetch initial single (or average) CPU clicks from kernel
   sysctl(MIB_kern_cp_time, 2, fpl->cp_time_o, &len, NULL, 0);

   // on smp box, fetch rest of initial CPU's clicks
   if (cpus > 1) {
      len = 2; sysctlnametomib("kern.cp_times", MIB_kern_cp_times, &len);
      fpl->cp_times_o = xCalloc(cpus, sizeof_cp_time_array);
      fpl->cp_times_n = xCalloc(cpus, sizeof_cp_time_array);
      len = cpus * sizeof_cp_time_array;
      sysctl(MIB_kern_cp_times, 2, fpl->cp_times_o, &len, NULL, 0);
   }

   pl->cpuCount = MAX(cpus, 1);

   if (cpus == 1 ) {
     fpl->cpus = xRealloc(fpl->cpus, sizeof(CPUData));
   } else {
     // on smp we need CPUs + 1 to store averages too (as kernel kindly provides that as well)
     fpl->cpus = xRealloc(fpl->cpus, (pl->cpuCount + 1) * sizeof(CPUData));
   }


   len = sizeof(kernelFScale);
   if (sysctlbyname("kern.fscale", &kernelFScale, &len, NULL, 0) == -1) {
      //sane default for kernel provided CPU percentage scaling, at least on x86 machines, in case this sysctl call failed
      kernelFScale = 2048;
   }

#ifdef HAVE_LIBKVM
   char errbuf[_POSIX2_LINE_MAX];
   fpl->kd = kvm_openfiles(NULL, "/dev/null", NULL, 0, errbuf);
   if (fpl->kd == NULL) {
      errx(1, "kvm_open: %s", errbuf);
   }
#else
   fpl->kip_buffer = NULL;
   fpl->kip_buffer_size = 0;
#endif

   int arg_max_mib[] = { CTL_KERN, KERN_ARGMAX };
   len = sizeof fpl->arg_max;
   if(sysctl(arg_max_mib, 2, &fpl->arg_max, &len, NULL, 0) < 0) fpl->arg_max = ARG_MAX;

   return pl;
}

void ProcessList_delete(ProcessList* this) {
   FreeBSDProcessList* fpl = (FreeBSDProcessList*) this;
#ifdef HAVE_LIBKVM
   if (fpl->kd) kvm_close(fpl->kd);
#else
   free(fpl->kip_buffer);
#endif
   free(fpl->cp_time_o);
   free(fpl->cp_time_n);
   free(fpl->cp_times_o);
   free(fpl->cp_times_n);
   free(fpl->cpus);

   ProcessList_done(this);
   free(this);
}

static inline void FreeBSDProcessList_scanCPUTime(ProcessList* pl) {
   const FreeBSDProcessList* fpl = (FreeBSDProcessList*) pl;

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
   if(sysctl(MIB_kern_cp_time, 2, fpl->cp_time_n, &sizeof_cp_time_array, NULL, 0) < 0) return;

   // get rest of CPUs
   if (cpus > 1) {
      // on smp systems FreeBSD kernel concats all CPU states into one long array in
      // kern.cp_times sysctl OID
      // we store averages in fpl->cpus[0], and actual cores after that
      maxcpu = cpus + 1;
      sizeof_cp_time_array = cpus * sizeof(long int) * CPUSTATES;
      if(sysctl(MIB_kern_cp_times, 2, fpl->cp_times_n, &sizeof_cp_time_array, NULL, 0) < 0) {
         return;
      }
   }

   for (int i = 0; i < maxcpu; i++) {
      if (cpus == 1) {
         // single CPU box
         cp_time_n = fpl->cp_time_n;
         cp_time_o = fpl->cp_time_o;
      } else {
         if (i == 0 ) {
           // average
           cp_time_n = fpl->cp_time_n;
           cp_time_o = fpl->cp_time_o;
         } else {
           // specific smp cores
           cp_times_offset = i - 1;
           cp_time_n = fpl->cp_times_n + (cp_times_offset * CPUSTATES);
           cp_time_o = fpl->cp_times_o + (cp_times_offset * CPUSTATES);
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

      CPUData* cpuData = &(fpl->cpus[i]);
      cpuData->userPercent      = cp_time_p[CP_USER];
      cpuData->nicePercent      = cp_time_p[CP_NICE];
      cpuData->systemPercent    = cp_time_p[CP_SYS];
      cpuData->irqPercent       = cp_time_p[CP_INTR];
      cpuData->systemAllPercent = cp_time_p[CP_SYS] + cp_time_p[CP_INTR];
      // this one is not really used, but we store it anyway
      cpuData->idlePercent      = cp_time_p[CP_IDLE];
   }
}

static inline void FreeBSDProcessList_scanMemoryInfo(ProcessList* pl) {
   FreeBSDProcessList* fpl = (FreeBSDProcessList*) pl;

   union {
      unsigned int v_uint;
      long int v_long;
      unsigned long int v_ulong;
   } buffer;
   size_t len;

   // @etosan:
   // memory counter relationships seem to be these:
   //  total = active + wired + inactive + cache + free
   //  htop_used (unavail to anybody) = active + wired
   //  htop_cache (for cache meter)   = buffers + cache
   //  user_free (avail to procs)     = buffers + inactive + cache + free
   //
   // with ZFS ARC situation becomes bit muddled, as ARC behaves like "user_free"
   // and belongs into cache, but is reported as wired by kernel
   //
   // htop_used   = active + (wired - arc)
   // htop_cache  = buffers + cache + arc

   //disabled for now, as it is always smaller than phycal amount of memory...
   //...to avoid "where is my memory?" questions
   //len = sizeof buffer.v_uint;
   //sysctl(MIB_vm_stats_vm_v_page_count, 4, &buffer, &len, NULL, 0);
   //pl->totalMem = buffer.v_uint * CRT_page_size_kib;
   len = sizeof buffer.v_ulong;
   if(sysctl(MIB_hw_physmem, 2, &buffer, &len, NULL, 0) < 0) goto fail;
   pl->totalMem = buffer.v_ulong / 1024;

   len = sizeof buffer.v_uint;
   if(sysctl(MIB_vm_stats_vm_v_active_count, 4, &buffer, &len, NULL, 0) < 0) goto fail;
   fpl->memActive = buffer.v_uint * CRT_page_size_kib;

   len = sizeof buffer.v_uint;
   if(sysctl(MIB_vm_stats_vm_v_wire_count, 4, &buffer, &len, NULL, 0) < 0) goto fail;
   fpl->memWire = buffer.v_uint * CRT_page_size_kib;

   len = sizeof buffer.v_long;
   if(sysctl(MIB_vfs_bufspace, 2, &buffer, &len, NULL, 0) < 0) goto fail;
   pl->buffersMem = buffer.v_long / 1024;

   if(MIB_vm_stats_vm_v_cache_count) {
      len = sizeof buffer.v_uint;
      if(sysctl(MIB_vm_stats_vm_v_cache_count, 4, &buffer, &len, NULL, 0) < 0) {
         pl->cachedMem = 0;
      } else {
         pl->cachedMem = buffer.v_uint * CRT_page_size_kib;
      }
   }

   if(v_laundry_count_mib) {
      // Exists since kFreeBSD 11.1
      len = sizeof buffer.v_uint;
      if(sysctl(v_laundry_count_mib, 4, &buffer, &len, NULL, 0) < 0) {
         fpl->laundry_size = 0;
      } else {
         fpl->laundry_size = buffer.v_uint * CRT_page_size_kib;
      }
   }

   // ZFS ARC size is now handled in ProcessList.c

   pl->usedMem = fpl->memActive + fpl->memWire + fpl->laundry_size;

   // currently unused, same as with arc, custom meter perhaps
   //len = sizeof buffer.v_uint;
   //sysctl(MIB_vm_stats_vm_v_inactive_count, 4, &buffer, &len, NULL, 0);
   //fpl->memInactive = buffer.v_uint * CRT_page_size_kib;
   //len = sizeof buffer.v_uint;
   //sysctl(MIB_vm_stats_vm_v_free_count, 4, &buffer, &len, NULL, 0);
   //fpl->memFree = buffer.v_uint * CRT_page_size_kib;
   //pl->freeMem = fpl->memInactive + fpl->memFree;

#ifdef HAVE_LIBKVM
   struct kvm_swap swap[16];
   int nswap = kvm_getswapinfo(fpl->kd, swap, sizeof(swap)/sizeof(swap[0]), 0);
   pl->totalSwap = 0;
   pl->usedSwap = 0;
   for (int i = 0; i < nswap; i++) {
      pl->totalSwap += swap[i].ksw_total;
      pl->usedSwap += swap[i].ksw_used;
   }
   pl->totalSwap *= CRT_page_size_kib;
   pl->usedSwap *= CRT_page_size_kib;
#else
   pl->totalSwap = 0;
   pl->usedSwap = 0;
#endif

   return;

fail:
   pl->totalMem = 0;
   pl->buffersMem = 0;
   pl->cachedMem = 0;
   pl->usedMem = 0;
   pl->totalSwap = 0;
   pl->usedSwap = 0;
}

static void FreeBSDProcessList_readProcessName(FreeBSDProcessList *this, struct kinfo_proc* kproc, char **name, char **command, int *argv0_len) {
   *name = xStrdup(kproc->ki_comm);
#ifdef KERN_PROC_ARGS
   int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_ARGS, kproc->ki_pid };
   char buffer[this->arg_max];
   size_t len = this->arg_max;
   if(sysctl(mib, 4, buffer, &len, NULL, 0) < 0 || len < 1) {
      *command = xStrdup(kproc->ki_comm);
      *argv0_len = strlen(kproc->ki_comm);
      return;
   }
   *command = xMalloc(len);
   (*command)[--len] = 0;
   *argv0_len = len;
   while(len-- > 0) {
      (*command)[len] = buffer[len] ? : (*argv0_len = len, ' ');
   }
#elif defined HAVE_LIBKVM
   char** argv = kvm_getargv(this->kd, kproc, 0);
   if (!argv || !*argv) {
      *command = xStrdup(kproc->ki_comm);
      *argv0_len = strlen(kproc->ki_comm);
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
#else
   *command = xStrdup(kproc->ki_comm);
   *argv0_len = strlen(kproc->ki_comm);
#endif
}

#define JAIL_ERRMSGLEN 1024

static char *FreeBSDProcessList_readJailName(struct kinfo_proc* kproc) {
#ifdef HAVE_STRUCT_KINFO_PROC_KI_JID
   if (kproc->ki_jid != 0) {
      struct iovec jiov[6];
      char jnamebuf[MAXHOSTNAMELEN];
      char errmsg[JAIL_ERRMSGLEN];
      memset(jnamebuf, 0, sizeof(jnamebuf));
      *(const void **)&jiov[0].iov_base = "jid";
      jiov[0].iov_len = sizeof("jid");
      jiov[1].iov_base = &kproc->ki_jid;
      jiov[1].iov_len = sizeof(kproc->ki_jid);
      *(const void **)&jiov[2].iov_base = "name";
      jiov[2].iov_len = sizeof("name");
      jiov[3].iov_base = jnamebuf;
      jiov[3].iov_len = sizeof(jnamebuf);
      *(const void **)&jiov[4].iov_base = "errmsg";
      jiov[4].iov_len = sizeof("errmsg");
      jiov[5].iov_base = errmsg;
      jiov[5].iov_len = JAIL_ERRMSGLEN;
      errmsg[0] = 0;
      int jid = jail_get(jiov, 6, 0);
      if (jid < 0) {
         if (!errmsg[0]) {
            xSnprintf(errmsg, JAIL_ERRMSGLEN, "jail_get: %s", strerror(errno));
            // TODO: Make use of errmsg
         }
         return NULL;
      }
      return jid == kproc->ki_jid ? xStrdup(jnamebuf) : NULL;
   }
#endif
   return xStrdup("-");
}

void ProcessList_goThroughEntries(ProcessList* this, bool skip_processes) {
   FreeBSDProcessList* fpl = (FreeBSDProcessList*) this;
   bool hide_kernel_processes = this->settings->hide_kernel_processes;

   FreeBSDProcessList_scanMemoryInfo(this);
   FreeBSDProcessList_scanCPUTime(this);

   if(skip_processes) return;

#ifdef HAVE_LIBKVM
   int count = 0;
   struct kinfo_proc* kprocs = kvm_getprocs(fpl->kd, KERN_PROC_PROC, 0, &count);
   if(!kprocs) return;
#else
   int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PROC };
   size_t buffer_size;
   if(sysctl(mib, 3, NULL, &buffer_size, NULL, 0) < 0) return;
   if(fpl->kip_buffer_size < buffer_size) {
      fpl->kip_buffer = xRealloc(fpl->kip_buffer, buffer_size);
      fpl->kip_buffer_size = buffer_size;
   }
   if(sysctl(mib, 3, fpl->kip_buffer, &buffer_size, NULL, 0) < 0 && errno != ENOMEM) return;
   struct kinfo_proc *kprocs = fpl->kip_buffer;
   int count = buffer_size / sizeof(struct kinfo_proc);
#endif

   for (int i = 0; i < count; i++) {
      struct kinfo_proc* kproc = &kprocs[i];
      bool is_existing;
      Process* proc = ProcessList_getProcess(this, kproc->ki_pid, &is_existing, (Process_New) FreeBSDProcess_new);
      FreeBSDProcess* fp = (FreeBSDProcess*) proc;

      proc->ppid = kproc->ki_ppid;
      proc->tpgid = kproc->ki_tpgid;
      proc->tgid = kproc->ki_pid;
      proc->session = kproc->ki_sid;
      proc->tty_nr = kproc->ki_tdev;
      proc->pgrp = kproc->ki_pgid;

      if (!is_existing) {
         fp->kernel = kproc->ki_pid != 1 && (kproc->ki_flag & P_SYSTEM);
         proc->ruid = kproc->ki_ruid;
         proc->euid = kproc->ki_uid;
         proc->real_user = UsersTable_getRef(this->usersTable, proc->ruid);
         proc->effective_user = UsersTable_getRef(this->usersTable, proc->euid);
         proc->starttime_ctime = kproc->ki_start.tv_sec;
         ProcessList_add((ProcessList*)this, proc);
         FreeBSDProcessList_readProcessName(fpl, kproc, &proc->name, &proc->comm, &proc->argv0_length);
      } else {
#ifdef HAVE_STRUCT_KINFO_PROC_KI_JID
         if(fp->jid != kproc->ki_jid) {
            // process can enter jail anytime
            free(fp->jname);
            fp->jname = NULL;
         }
#endif
         // some processes change users (eg. to lower privs)
         if(proc->ruid != kproc->ki_ruid) {
            proc->ruid = kproc->ki_ruid;
            proc->real_user = UsersTable_getRef(this->usersTable, proc->ruid);
         }
         if(proc->euid != kproc->ki_uid) {
            proc->euid = kproc->ki_uid;
            proc->effective_user = UsersTable_getRef(this->usersTable, proc->euid);
         }
         if (ProcessList_shouldUpdateProcessNames(this)) {
            free(proc->name);
            free(proc->comm);
            FreeBSDProcessList_readProcessName(fpl, kproc, &proc->name, &proc->comm, &proc->argv0_length);
            if(this->settings->flags & PROCESS_FLAG_EMULATION) {
               free(fp->emulation);
               fp->emulation = xStrdup(kproc->ki_emul);
            }
         }
      }

      proc->m_size = kproc->ki_size / CRT_page_size;
      proc->m_resident = kproc->ki_rssize;
      proc->percent_mem =
         (double)proc->m_resident / (double)(this->totalMem / CRT_page_size_kib) * 100;
      proc->nlwp = kproc->ki_numthreads;
      proc->time = kproc->ki_runtime / 10000;
      proc->percent_cpu = (double)kproc->ki_pctcpu / (double)kernelFScale * 100;

      proc->priority = kproc->ki_pri.pri_level - PZERO;
      if ((kproc->ki_flag & P_SYSTEM) && strcmp(kproc->ki_comm, "intr") == 0) {
         proc->nice = 0; //@etosan: intr kernel process (not thread) has weird nice value
      } else if (kproc->ki_pri.pri_class == PRI_TIMESHARE) {
         proc->nice = kproc->ki_nice - NZERO;
      } else if (PRI_IS_REALTIME(kproc->ki_pri.pri_class)) {
         proc->nice = PRIO_MIN - 1 - (PRI_MAX_REALTIME - kproc->ki_pri.pri_level);
      } else {
         proc->nice = PRIO_MAX + 1 + kproc->ki_pri.pri_level - PRI_MIN_IDLE;
      }

      int on_processor = -1;

      switch (kproc->ki_stat) {
         case SIDL:
            proc->state = 'I';
            break;
         case SRUN:
            proc->state = 'R';
            if(kproc->ki_oncpu != NOCPU) on_processor = kproc->ki_oncpu;
            break;
         case SSLEEP:
            proc->state = (kproc->ki_tdflags & TDF_SINTR) ?
               (kproc->ki_slptime > MAXSLP ? 'I' : 'S') : 'D';
            break;
         case SSTOP:
            proc->state = 'T';
            break;
         case SZOMB:
            proc->state = 'Z';
            break;
         case SWAIT:
            proc->state = 'W';
            break;
         case SLOCK:
            proc->state = 'L';
            break;
         default:
            proc->state = '?';
            break;
      }

      if(on_processor < 0) {
         if(kproc->ki_lastcpu != NOCPU) proc->processor = kproc->ki_lastcpu;
      } else {
         proc->processor = on_processor;
      }

#ifdef HAVE_STRUCT_KINFO_PROC_KI_JID
      fp->jid = kproc->ki_jid;
#endif
      if((this->settings->flags & PROCESS_FLAG_JAIL) && !fp->jname) {
         fp->jname = FreeBSDProcessList_readJailName(kproc);
      }

      if((this->settings->flags & PROCESS_FLAG_EMULATION) && !fp->emulation) {
         fp->emulation = xStrdup(kproc->ki_emul);
      }

      this->totalTasks++;
      this->thread_count += proc->nlwp;
      if (Process_isKernelProcess(proc)) {
         this->kernel_process_count++;
         this->kernel_thread_count += proc->nlwp;
      }
      if (proc->state == 'R' && !(kproc->ki_tdflags & TDF_IDLETD)) {
         this->running_process_count++;
         this->running_thread_count++;
      }

      proc->show = !(hide_kernel_processes && Process_isKernelProcess(proc));

      proc->updated = true;
   }
}
