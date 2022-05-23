/*
htop - openbsd/OpenBSDProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#include "ProcessList.h"
#include "OpenBSDProcessList.h"
#include "OpenBSDProcess.h"
#include "CRT.h"
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <err.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

/*{

#include <kvm.h>

typedef struct CPUData_ {
   unsigned long long int totalTime;
   unsigned long long int userTime;
   unsigned long long int niceTime;
   unsigned long long int sysTime;
   unsigned long long int sysAllTime;
   unsigned long long int spinTime;
   unsigned long long int intrTime;
   unsigned long long int idleTime;

   unsigned long long int totalPeriod;
   unsigned long long int userPeriod;
   unsigned long long int nicePeriod;
   unsigned long long int sysPeriod;
   unsigned long long int sysAllPeriod;
   unsigned long long int spinPeriod;
   unsigned long long int intrPeriod;
   unsigned long long int idlePeriod;
} CPUData;

typedef struct OpenBSDProcessList_ {
   ProcessList super;
   kvm_t* kd;

   CPUData* cpus;

} OpenBSDProcessList;

}*/

/*
 * avoid relying on or conflicting with MIN() and MAX() in sys/param.h
 */
#ifndef MINIMUM
#define MINIMUM(x, y)		((x) > (y) ? (y) : (x))
#endif

#ifndef MAXIMUM
#define MAXIMUM(x, y)		((x) > (y) ? (x) : (y))
#endif

#ifndef CLAMP
#define CLAMP(x, low, high)	(((x) > (high)) ? (high) : MAXIMUM(x, low))
#endif

static int fscale;
static unsigned int maxslp;

ProcessList* ProcessList_new(UsersTable* usersTable, const Hashtable *pidWhiteList, uid_t userId) {
   int i;

   OpenBSDProcessList *opl = xCalloc(1, sizeof(OpenBSDProcessList));
   ProcessList *pl = (ProcessList*) opl;
   ProcessList_init(pl, Class(OpenBSDProcess), usersTable, pidWhiteList, userId);

   int mib[] = { CTL_HW, HW_NCPU };
   size_t size = sizeof(pl->cpuCount);
   if(sysctl(mib, 2, &pl->cpuCount, &size, NULL, 0) < 0 || pl->cpuCount < 1) {
      pl->cpuCount = 1;
   }
   opl->cpus = xCalloc(1 + pl->cpuCount, sizeof(CPUData));

   mib[0] = CTL_KERN;
   mib[1] = KERN_FSCALE;
   size = sizeof(fscale);
   if (sysctl(mib, 2, &fscale, &size, NULL, 0) < 0) {
      err(1, "fscale sysctl call failed");
   }

   mib[0] = CTL_VM;
   mib[1] = VM_MAXSLP;
   size = sizeof maxslp;
   if(sysctl(mib, 2, &maxslp, &size, NULL, 0) < 0) maxslp = 20;

   for (i = 0; i <= pl->cpuCount; i++) {
      CPUData *d = opl->cpus + i;
      d->totalTime = 1;
      d->totalPeriod = 1;
   }

   char errbuf[_POSIX2_LINE_MAX];
   opl->kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
   if (opl->kd == NULL) {
      errx(1, "kvm_open: %s", errbuf);
   }

   return pl;
}

void ProcessList_delete(ProcessList* this) {
   const OpenBSDProcessList* opl = (OpenBSDProcessList*) this;

   if (opl->kd) {
      kvm_close(opl->kd);
   }

   free(opl->cpus);

   ProcessList_done(this);
   free(this);
}

static inline void OpenBSDProcessList_scanMemoryInfo(ProcessList* pl) {
   static int uvmexp_mib[] = {CTL_VM, VM_UVMEXP};
   struct uvmexp uvmexp;
   size_t size_uvmexp = sizeof(uvmexp);

   if (sysctl(uvmexp_mib, 2, &uvmexp, &size_uvmexp, NULL, 0) < 0) {
      err(1, "uvmexp sysctl call failed");
   }

   pl->totalMem = uvmexp.npages * CRT_page_size_kib;

   // Taken from OpenBSD systat/iostat.c, top/machine.c and uvm_sysctl(9)
   static int bcache_mib[] = {CTL_VFS, VFS_GENERIC, VFS_BCACHESTAT};
   struct bcachestats bcstats;
   size_t size_bcstats = sizeof(bcstats);

   if (sysctl(bcache_mib, 3, &bcstats, &size_bcstats, NULL, 0) < 0) {
      err(1, "cannot get vfs.bcachestat");
   }

   pl->cachedMem = bcstats.numbufpages * CRT_page_size_kib;
   pl->freeMem = uvmexp.free * CRT_page_size_kib;
   pl->usedMem = (uvmexp.npages - uvmexp.free - uvmexp.paging) * CRT_page_size_kib;
}

static void OpenBSDProcessList_readProcessName(kvm_t* kd, struct kinfo_proc* kproc, char **name, char **command, int *argv0_len) {
   *name = xStrdup(kproc->p_comm);

   /*
    * Like OpenBSD's top(1), we try to fall back to the command name
    * if we failed to construct the full command.
    */
   char **arg = kvm_getargv(kd, kproc, 500);
   if (arg == NULL || *arg == NULL) {
fallback_to_name_only:
      *command = xStrdup(kproc->p_comm);
      *argv0_len = strlen(kproc->p_comm);
      return;
   }

   // Initialize 'len' because GCC 4.2.1 reports:
   // warning: 'len' may be used uninitialized in this function
   size_t len = 0;
   unsigned int i = 0;
   while(arg[i]) {
      if(i) {
         size_t arg_len = strlen(arg[i]) + 1;
         char *p = realloc(*command, len + arg_len);
         if(!p) return;
         *command = p;
         (*command)[len - 1] = ' ';
         memcpy(*command + len, arg[i], arg_len);
         len += arg_len;
      } else {
         len = strlen(arg[i]);
         if(!len) goto fallback_to_name_only;
         *argv0_len = len++;
         /* don't use xMalloc here - we want to handle huge argv's gracefully */
         *command = malloc(len);
         if (!*command) goto fallback_to_name_only;
         memcpy(*command, arg[i], len);
      }
      i++;
   }
}

/*
 * Taken from OpenBSD's ps(1).
 */
static double getpcpu(const struct kinfo_proc *kp) {
   if (fscale == 0)
      return (0.0);

#define   fxtofl(fixpt)   ((double)(fixpt) / fscale)

   return (100.0 * fxtofl(kp->p_pctcpu));
}

#ifdef PID_AND_MAIN_THREAD_ID_DIFFER
static pid_t get_main_tid(const struct kinfo_proc *procs, unsigned int count, pid_t pid) {
	for(unsigned int i = 0; i < count; i++) {
		const struct kinfo_proc *p = procs + i;
		if(p->p_tid == -1) continue;
		if(p->p_pid != pid) continue;
		if(p->p_flag & P_THREAD) continue;
		return p->p_tid - THREAD_PID_OFFSET;
	}
	return -1;
}
#endif

static inline void OpenBSDProcessList_scanProcs(ProcessList *this) {
   OpenBSDProcessList* opl = (OpenBSDProcessList*) this;
   bool hide_kernel_processes = this->settings->hide_kernel_processes;
   bool hide_thread_processes = this->settings->hide_thread_processes;
#ifdef PID_AND_MAIN_THREAD_ID_DIFFER
   bool hide_high_level_processes = this->settings->hide_high_level_processes;
#else
   bool hide_high_level_processes = false;
#endif
   struct timeval now;
   int count = 0;
   int i;

   struct kinfo_proc* kprocs = kvm_getprocs(opl->kd,
#ifdef KERN_PROC_SHOW_THREADS
      KERN_PROC_SHOW_THREADS |
#endif
      KERN_PROC_KTHREAD,
      0, sizeof(struct kinfo_proc), &count);

   gettimeofday(&now, NULL);

#ifdef PID_AND_MAIN_THREAD_ID_DIFFER
   pid_t last_pid = -1;
   pid_t last_main_tid = -1;
#endif

   for (i = 0; i < count; i++) {
      struct kinfo_proc *kproc = kprocs + i;
#ifdef HAVE_STRUCT_KINFO_PROC_P_TID
      if((hide_high_level_processes || kproc->p_pid == 0) && kproc->p_tid == -1) continue;
      pid_t pid = kproc->p_tid == -1 ? kproc->p_pid : kproc->p_tid - THREAD_PID_OFFSET;
#else
      pid_t pid = kproc->p_pid;
#endif
      bool preExisting;
      Process *proc = ProcessList_getProcess(this, pid, &preExisting, (Process_New) OpenBSDProcess_new);
      OpenBSDProcess *openbsd_proc = (OpenBSDProcess *)proc;

      if (!preExisting) {
         proc->tgid = kproc->p_pid;
         proc->starttime_ctime = kproc->p_ustart_sec;
         openbsd_proc->is_kernel_process = (kproc->p_flag & P_SYSTEM);
         openbsd_proc->is_main_thread = !(kproc->p_flag & P_THREAD);
         ProcessList_add((ProcessList*)this, proc);
         OpenBSDProcessList_readProcessName(opl->kd, kproc, &proc->name, &proc->comm, &proc->argv0_length);
      } else {
         if(proc->ruid != kproc->p_ruid) proc->real_user = NULL;
         if(proc->euid != kproc->p_uid) proc->effective_user = NULL;
         if (this->settings->updateProcessNames) {
            free(proc->name);
            free(proc->comm);
            OpenBSDProcessList_readProcessName(opl->kd, kproc, &proc->name, &proc->comm, &proc->argv0_length);
         }
      }

#ifdef PID_AND_MAIN_THREAD_ID_DIFFER
      if(hide_high_level_processes) {
         if(openbsd_proc->is_main_thread) {
            int remain_count = count - i - 1;
            if(remain_count > 0) {
               proc->ppid = get_main_tid(kprocs + i + 1, remain_count, kproc->p_ppid);
               last_pid = kproc->p_pid;
               last_main_tid = kproc->p_tid - THREAD_PID_OFFSET;
            } else {
               proc->ppid = 0;
            }
         } else if(last_pid == kproc->p_pid) {
            proc->ppid = last_main_tid;
         } else {
            proc->ppid = get_main_tid(kprocs, i, kproc->p_pid);
         }
      } else
#endif
      proc->ppid = kproc->p_ppid;

      proc->tpgid = kproc->p_tpgid;
      proc->session = kproc->p_sid;
      proc->tty_nr = kproc->p_tdev;
      proc->pgrp = kproc->p__pgid;
      proc->ruid = kproc->p_ruid;
      proc->euid = kproc->p_uid;

      if(!proc->real_user) {
         proc->real_user = UsersTable_getRef(this->usersTable, proc->ruid);
      }
      if(!proc->effective_user) {
         proc->effective_user = UsersTable_getRef(this->usersTable, proc->euid);
      }

      proc->m_size = kproc->p_vm_dsize;
      proc->m_resident = kproc->p_vm_rssize;
      proc->percent_mem = (proc->m_resident * CRT_page_size_kib) / (double)(this->totalMem) * 100.0;
      proc->percent_cpu = CLAMP(getpcpu(kproc), 0.0, this->cpuCount*100.0);
      proc->nice = kproc->p_nice - NZERO;
      proc->time = kproc->p_rtime_sec + ((kproc->p_rtime_usec + 500000) / 1000000);
      proc->time *= 100;
      proc->priority = kproc->p_priority - PZERO;

      switch (kproc->p_stat) {
         case SIDL:
            proc->state = 'I';
            break;
         case SRUN:
            proc->state = 'R';
            break;
         case SSLEEP:
            proc->state = (kproc->p_flag & P_SINTR) ? (kproc->p_slptime > maxslp ? 'I' : 'S') : 'D';
            break;
         case SSTOP:
            proc->state = 'T';
            break;
         case SZOMB:
         case SDEAD:
            proc->state = 'Z';
            break;
         case SONPROC:
            proc->state = 'O';
            break;
         default:
            proc->state = '?';
            break;
      }

#ifdef HAVE_STRUCT_KINFO_PROC_P_TID
      if(kproc->p_tid != -1) {
#endif
         this->totalTasks++;
         this->thread_count++;
         if (Process_isKernelProcess(proc)) {
            this->kernel_process_count++;
            this->kernel_thread_count++;
         }
         // SRUN ('R') means runnable, not running
         if (proc->state == 'O') {
            this->running_process_count++;
            this->running_thread_count++;
         }
#ifdef HAVE_STRUCT_KINFO_PROC_P_TID
      }
#endif

      proc->show =
#ifdef KERN_PROC_SHOW_THREADS
         !(Process_isKernelProcess(proc) && kproc->p_tid == -1) &&
#endif
            (!((hide_kernel_processes && Process_isKernelProcess(proc)) ||
               (hide_thread_processes && Process_isExtraThreadProcess(proc))));

      proc->updated = true;
   }
}

static unsigned long long saturatingSub(unsigned long long a, unsigned long long b) {
   return a > b ? a - b : 0;
}

static void getKernelCPUTimes(int cpuId, uint64_t *times) {
   int mib[] = { CTL_KERN, KERN_CPTIME2, cpuId };
   size_t length = sizeof(uint64_t) * CPUSTATES;
   if (sysctl(mib, 3, times, &length, NULL, 0) == -1 ||
         length != sizeof(uint64_t) * CPUSTATES) {
      CRT_fatalError("sysctl kern.cp_time2 failed");
   }
}

static void kernelCPUTimesToHtop(const uint64_t *times, CPUData* cpu) {
   unsigned long long totalTime = 0;
   for (int i = 0; i < CPUSTATES; i++) {
      totalTime += times[i];
   }

   unsigned long long sysAllTime = times[CP_INTR] + times[CP_SYS];

   // XXX Not sure if CP_SPIN should be added to sysAllTime.
   // See https://github.com/openbsd/src/commit/531d8034253fb82282f0f353c086e9ad827e031c
   #ifdef CP_SPIN
   sysAllTime += times[CP_SPIN];
   #endif

   cpu->totalPeriod = saturatingSub(totalTime, cpu->totalTime);
   cpu->userPeriod = saturatingSub(times[CP_USER], cpu->userTime);
   cpu->nicePeriod = saturatingSub(times[CP_NICE], cpu->niceTime);
   cpu->sysPeriod = saturatingSub(times[CP_SYS], cpu->sysTime);
   cpu->sysAllPeriod = saturatingSub(sysAllTime, cpu->sysAllTime);
   #ifdef CP_SPIN
   cpu->spinPeriod = saturatingSub(times[CP_SPIN], cpu->spinTime);
   #endif
   cpu->intrPeriod = saturatingSub(times[CP_INTR], cpu->intrTime);
   cpu->idlePeriod = saturatingSub(times[CP_IDLE], cpu->idleTime);

   cpu->totalTime = totalTime;
   cpu->userTime = times[CP_USER];
   cpu->niceTime = times[CP_NICE];
   cpu->sysTime = times[CP_SYS];
   cpu->sysAllTime = sysAllTime;
   #ifdef CP_SPIN
   cpu->spinTime = times[CP_SPIN];
   #endif
   cpu->intrTime = times[CP_INTR];
   cpu->idleTime = times[CP_IDLE];
}

static void OpenBSDProcessList_scanCPUTime(OpenBSDProcessList* this) {
   uint64_t kernelTimes[CPUSTATES] = {0};
   uint64_t avg[CPUSTATES] = {0};

   for (int i = 0; i < this->super.cpuCount; i++) {
      getKernelCPUTimes(i, kernelTimes);
      CPUData* cpu = this->cpus + i + 1;
      kernelCPUTimesToHtop(kernelTimes, cpu);

      avg[CP_USER] += cpu->userTime;
      avg[CP_NICE] += cpu->niceTime;
      avg[CP_SYS] += cpu->sysTime;
      #ifdef CP_SPIN
      avg[CP_SPIN] += cpu->spinTime;
      #endif
      avg[CP_INTR] += cpu->intrTime;
      avg[CP_IDLE] += cpu->idleTime;
   }

   for (int i = 0; i < CPUSTATES; i++) {
      avg[i] /= this->super.cpuCount;
   }

   kernelCPUTimesToHtop(avg, this->cpus);
}

void ProcessList_goThroughEntries(ProcessList* this) {
   OpenBSDProcessList_scanMemoryInfo(this);
   OpenBSDProcessList_scanProcs(this);
   OpenBSDProcessList_scanCPUTime((OpenBSDProcessList*)this);
}
