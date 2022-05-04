/*
htop - OpenBSDProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#include "ProcessList.h"
#include "OpenBSDProcessList.h"
#include "OpenBSDProcess.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/proc.h>
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
   unsigned long long int totalPeriod;
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

static long fscale;

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList, uid_t userId) {
   int mib[] = { CTL_HW, HW_NCPU };
   int fmib[] = { CTL_KERN, KERN_FSCALE };
   int i, e;
   OpenBSDProcessList* opl;
   ProcessList* pl;
   size_t size;
   char errbuf[_POSIX2_LINE_MAX];

   opl = xCalloc(1, sizeof(OpenBSDProcessList));
   pl = (ProcessList*) opl;
   size = sizeof(pl->cpuCount);
   ProcessList_init(pl, Class(OpenBSDProcess), usersTable, pidWhiteList, userId);

   e = sysctl(mib, 2, &pl->cpuCount, &size, NULL, 0);
   if (e == -1 || pl->cpuCount < 1) {
      pl->cpuCount = 1;
   }
   opl->cpus = xRealloc(opl->cpus, pl->cpuCount * sizeof(CPUData));

   size = sizeof(fscale);
   if (sysctl(fmib, 2, &fscale, &size, NULL, 0) < 0) {
      err(1, "fscale sysctl call failed");
   }

   for (i = 0; i < pl->cpuCount; i++) {
      opl->cpus[i].totalTime = 1;
      opl->cpus[i].totalPeriod = 1;
   }

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

   pl->totalMem = uvmexp.npages * PAGE_SIZE_KB;

   // Taken from OpenBSD systat/iostat.c, top/machine.c and uvm_sysctl(9)
   static int bcache_mib[] = {CTL_VFS, VFS_GENERIC, VFS_BCACHESTAT};
   struct bcachestats bcstats;
   size_t size_bcstats = sizeof(bcstats);

   if (sysctl(bcache_mib, 3, &bcstats, &size_bcstats, NULL, 0) < 0) {
      err(1, "cannot get vfs.bcachestat");
   }

   pl->cachedMem = bcstats.numbufpages * PAGE_SIZE_KB;
   pl->freeMem = uvmexp.free * PAGE_SIZE_KB;
   pl->usedMem = (uvmexp.npages - uvmexp.free - uvmexp.paging) * PAGE_SIZE_KB;

   /*
   const OpenBSDProcessList* opl = (OpenBSDProcessList*) pl;

   size_t len = sizeof(pl->totalMem);
   sysctl(MIB_hw_physmem, 2, &(pl->totalMem), &len, NULL, 0);
   pl->totalMem /= 1024;
   sysctl(MIB_vm_stats_vm_v_wire_count, 4, &(pl->usedMem), &len, NULL, 0);
   pl->usedMem *= PAGE_SIZE_KB;
   pl->freeMem = pl->totalMem - pl->usedMem;
   sysctl(MIB_vm_stats_vm_v_cache_count, 4, &(pl->cachedMem), &len, NULL, 0);
   pl->cachedMem *= PAGE_SIZE_KB;

   struct kvm_swap swap[16];
   int nswap = kvm_getswapinfo(opl->kd, swap, sizeof(swap)/sizeof(swap[0]), 0);
   pl->totalSwap = 0;
   pl->usedSwap = 0;
   for (int i = 0; i < nswap; i++) {
      pl->totalSwap += swap[i].ksw_total;
      pl->usedSwap += swap[i].ksw_used;
   }
   pl->totalSwap *= PAGE_SIZE_KB;
   pl->usedSwap *= PAGE_SIZE_KB;

   pl->sharedMem = 0;  // currently unused
   pl->buffersMem = 0; // not exposed to userspace
   */
}

static void OpenBSDProcessList_readProcessName(kvm_t* kd, struct kinfo_proc* kproc, char **name, char **command, int* basenameEnd) {
   char **arg;
   size_t len = 0, n;
   int i;

   *name = xStrdup(kproc->p_comm);

   /*
    * Like OpenBSD's top(1), we try to fall back to the command name
    * (argv[0]) if we fail to construct the full command.
    */
   arg = kvm_getargv(kd, kproc, 500);
   if (arg == NULL || *arg == NULL) {
      *basenameEnd = strlen(kproc->p_comm);
      *command = xStrdup(kproc->p_comm);
      return;
   }
   for (i = 0; arg[i] != NULL; i++) {
      len += strlen(arg[i]) + 1;   /* room for arg and trailing space or NUL */
   }
   /* don't use xMalloc here - we want to handle huge argv's gracefully */
   if ((*command = malloc(len)) == NULL) {
      *basenameEnd = strlen(kproc->p_comm);
      *command = xStrdup(kproc->p_comm);
      return;
   }

   **command = 0;

   for (i = 0; arg[i] != NULL; i++) {
      n = strlcat(*command, arg[i], len);
      if (i == 0) {
         /* TODO: rename all basenameEnd to basenameLen, make size_t */
         *basenameEnd = MINIMUM(n, len-1);
      }
      /* the trailing space should get truncated anyway */
      strlcat(*command, " ", len);
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

void ProcessList_goThroughEntries(ProcessList* this) {
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

   OpenBSDProcessList_scanMemoryInfo(this);

   struct kinfo_proc* kprocs = kvm_getprocs(opl->kd,
#ifdef KERN_PROC_SHOW_THREADS
      KERN_PROC_SHOW_THREADS |
#endif
      KERN_PROC_KTHREAD,
      0, sizeof(struct kinfo_proc), &count);

   gettimeofday(&now, NULL);

   for (i = 0; i < count; i++) {
      struct kinfo_proc *kproc = kprocs + i;
      if((hide_high_level_processes || kproc->p_pid == 0) && kproc->p_tid == -1) continue;
      pid_t pid = kproc->p_tid == -1 ? kproc->p_pid : kproc->p_tid - THREAD_PID_OFFSET;
      bool preExisting;
      Process *proc = ProcessList_getProcess(this, pid, &preExisting, (Process_New) OpenBSDProcess_new);
      OpenBSDProcess *openbsd_proc = (OpenBSDProcess *)proc;

      if (!preExisting) {
         struct tm date;
         proc->tgid = kproc->p_pid;
         proc->ppid = kproc->p_ppid;
         proc->tpgid = kproc->p_tpgid;
         proc->session = kproc->p_sid;
         proc->tty_nr = kproc->p_tdev;
         proc->pgrp = kproc->p__pgid;
         proc->ruid = kproc->p_ruid;
         proc->euid = kproc->p_uid;
         proc->real_user = UsersTable_getRef(this->usersTable, proc->ruid);
         proc->effective_user = UsersTable_getRef(this->usersTable, proc->euid);
         proc->starttime_ctime = kproc->p_ustart_sec;
         openbsd_proc->is_kernel_process = (kproc->p_flag & P_SYSTEM);
         openbsd_proc->is_main_thread = !(kproc->p_flag & P_THREAD);
         ProcessList_add((ProcessList*)this, proc);
         OpenBSDProcessList_readProcessName(opl->kd, kproc, &proc->name, &proc->comm, &proc->basenameOffset);
         (void) localtime_r((time_t*) &kproc->p_ustart_sec, &date);
         strftime(proc->starttime_show, 7, ((proc->starttime_ctime > now.tv_sec - 86400) ? "%R " : "%b%d "), &date);
      } else if (this->settings->updateProcessNames) {
         free(proc->name);
         free(proc->comm);
         OpenBSDProcessList_readProcessName(opl->kd, kproc, &proc->name, &proc->comm, &proc->basenameOffset);
      }

      proc->m_size = kproc->p_vm_dsize;
      proc->m_resident = kproc->p_vm_rssize;
      proc->percent_mem = (proc->m_resident * PAGE_SIZE_KB) / (double)(this->totalMem) * 100.0;
      proc->percent_cpu = CLAMP(getpcpu(kproc), 0.0, this->cpuCount*100.0);
      proc->nice = kproc->p_nice - NZERO;
      proc->time = kproc->p_rtime_sec + ((kproc->p_rtime_usec + 500000) / 1000000);
      proc->time *= 100;
      proc->priority = kproc->p_priority - PZERO;

      switch (kproc->p_stat) {
         case SIDL:    proc->state = 'I'; break;
         case SRUN:    proc->state = 'R'; break;
         case SSLEEP:  proc->state = 'S'; break;
         case SSTOP:   proc->state = 'T'; break;
         case SZOMB:   proc->state = 'Z'; break;
         case SDEAD:   proc->state = 'D'; break;
         case SONPROC: proc->state = 'P'; break;
         default:      proc->state = '?';
      }

      if(kproc->p_tid != -1) {
         this->totalTasks++;
         this->thread_count++;
         if (Process_isKernelProcess(proc)) {
            this->kernel_process_count++;
            this->kernel_thread_count++;
         }
 
         // SRUN ('R') means runnable, not running
         if (proc->state == 'P') {
            this->running_process_count++;
            this->running_thread_count++;
         }
      }

      proc->show =
#ifdef KERN_PROC_SHOW_THREADS
         !(Process_isKernelProcess(proc) && kproc->p_tid == -1) &&
#endif
            (!((hide_kernel_processes && Process_isKernelProcess(proc)) ||
               (hide_thread_processes && Process_isExtraThreadProcess(proc))));

      proc->updated = true;
   }
}
