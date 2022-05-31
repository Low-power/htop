/*
htop - solaris/SolarisProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2017,2018 Guy M. Broome
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#include "ProcessList.h"
#include "SolarisProcessList.h"
#include "Platform.h"
#include "IOUtils.h"
#include "CRT.h"
#include <sys/param.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/sysconf.h>
#include <sys/sysinfo.h>
#include <sys/swap.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <procfs.h>
#include <errno.h>
#include <pwd.h>
#include <math.h>
#include <time.h>
#ifndef HAVE_LIBPROC
#include <fcntl.h>
#endif

#define MAXCMDLINE 255

/*{

#include "SolarisProcess.h"
#include <stdint.h>
#include <kstat.h>
#ifdef HAVE_LIBPROC
#include <libproc.h>
#else
#include <dirent.h>
#endif

typedef struct CPUData_ {
   double userPercent;
   double nicePercent;
   double systemPercent;
   double irqPercent;
   double idlePercent;
   double systemAllPercent;
   uint64_t luser;
   uint64_t lkrnl;
   uint64_t lintr;
   uint64_t lidle;
} CPUData;

typedef struct SolarisProcessList_ {
   ProcessList super;
   kstat_ctl_t* kd;
   int online_cpu_count;
   CPUData* cpus;
#ifndef HAVE_LIBPROC
   DIR *proc_dir;
#endif
   time_t last_updated;
} SolarisProcessList;

}*/

#if !defined HAVE_DIRFD && !defined dirfd
#define dirfd(D) ((D)->dd_fd)
#endif

#ifdef HAVE_ZONE_H
static char *SolarisProcessList_readZoneName(kstat_ctl_t* kd, SolarisProcess* sproc) {
	if(sproc->zoneid == 0) return xStrdup("global");
	if(!kd) return xStrdup("unknown");
	kstat_t *ks = kstat_lookup(kd, "zones", sproc->zoneid, NULL);
	return xStrdup(ks ? ks->ks_name : "unknown");
}
#endif

ProcessList* ProcessList_new(UsersTable* usersTable, const Hashtable *pidWhiteList, uid_t userId) {
   SolarisProcessList* spl = xCalloc(1, sizeof(SolarisProcessList));
   ProcessList* pl = (ProcessList*) spl;
   ProcessList_init(pl, Class(SolarisProcess), usersTable, pidWhiteList, userId);
   spl->kd = kstat_open();
   spl->online_cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
   pl->cpuCount = sysconf(_SC_NPROCESSORS_CONF);
   if (pl->cpuCount > 1) {
      spl->cpus = xMalloc((pl->cpuCount + 1) * sizeof(CPUData));
   } else {
      spl->cpus = xMalloc(sizeof(CPUData));
   }
   return pl;
}

static inline void SolarisProcessList_scanCPUTime(ProcessList* pl) {
   SolarisProcessList *spl = (SolarisProcessList *)pl;

   int cpus = pl->cpuCount;
   double idlebuf = 0;
   double intrbuf = 0;
   double krnlbuf = 0;
   double userbuf = 0;
   int arrskip = 0;

   assert(spl->kd != NULL);

   int online_cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
   if(online_cpu_count != spl->online_cpu_count) {
      kstat_chain_update(spl->kd);
      spl->online_cpu_count = online_cpu_count;
   }

   assert(cpus > 0);
   if (cpus > 1) {
       // Store values for the stats loop one extra element up in the array
       // to leave room for the average to be calculated afterwards
       arrskip++;
   }

   // Calculate per-CPU statistics first
   for (int i = 0; i < cpus; i++) {
      CPUData *cpuData = spl->cpus + i + arrskip;
      kstat_t *cpuinfo = kstat_lookup(spl->kd, "cpu", i, "sys");
      if(cpuinfo && kstat_read(spl->kd, cpuinfo, NULL) != -1) {
         kstat_named_t *idletime = kstat_data_lookup(cpuinfo, "cpu_nsec_idle");
         kstat_named_t *intrtime = kstat_data_lookup(cpuinfo, "cpu_nsec_intr");
         kstat_named_t *krnltime = kstat_data_lookup(cpuinfo, "cpu_nsec_kernel");
         kstat_named_t *usertime = kstat_data_lookup(cpuinfo, "cpu_nsec_user");
         uint64_t totaltime =
            (idletime->value.ui64 - cpuData->lidle) +
            (intrtime->value.ui64 - cpuData->lintr) +
            (krnltime->value.ui64 - cpuData->lkrnl) +
            (usertime->value.ui64 - cpuData->luser);
         // Calculate percentages of deltas since last reading
         cpuData->userPercent      = ((usertime->value.ui64 - cpuData->luser) / (double)totaltime) * 100.0;
         cpuData->nicePercent      = (double)0.0; // Not implemented on Solaris
         cpuData->systemPercent    = ((krnltime->value.ui64 - cpuData->lkrnl) / (double)totaltime) * 100.0;
         cpuData->irqPercent       = ((intrtime->value.ui64 - cpuData->lintr) / (double)totaltime) * 100.0;
         cpuData->systemAllPercent = cpuData->systemPercent + cpuData->irqPercent;
         cpuData->idlePercent      = ((idletime->value.ui64 - cpuData->lidle) / (double)totaltime) * 100.0;
         // Store current values to use for the next round of deltas
         cpuData->luser            = usertime->value.ui64;
         cpuData->lkrnl            = krnltime->value.ui64;
         cpuData->lintr            = intrtime->value.ui64;
         cpuData->lidle            = idletime->value.ui64;
         // Accumulate the current percentages into buffers for later average calculation
         if (cpus > 1) {
            userbuf               += cpuData->userPercent;
            krnlbuf               += cpuData->systemPercent;
            intrbuf               += cpuData->irqPercent;
            idlebuf               += cpuData->idlePercent;
         }
      } else {
         // An off-line processor
         cpuData->userPercent = 0;
         cpuData->nicePercent = 0;
         cpuData->systemPercent = 0;
         cpuData->irqPercent = 0;
         cpuData->systemAllPercent = 0;
         cpuData->idlePercent = 100;
      }
   }
   if (cpus > 1) {
      CPUData* cpuData          = &(spl->cpus[0]);
      cpuData->userPercent      = userbuf / cpus;
      cpuData->nicePercent      = (double)0.0; // Not implemented on Solaris
      cpuData->systemPercent    = krnlbuf / cpus;
      cpuData->irqPercent       = intrbuf / cpus;
      cpuData->systemAllPercent = cpuData->systemPercent + cpuData->irqPercent;
      cpuData->idlePercent      = idlebuf / cpus;
   }
}

static inline void SolarisProcessList_scanMemoryInfo(ProcessList* pl) {
   SolarisProcessList* spl = (SolarisProcessList*) pl;

   // Part 1 - memory
   kstat_t             *meminfo = NULL;
   int                 ksrphyserr = -1;
   if (spl->kd != NULL) {
      meminfo = kstat_lookup(spl->kd,"unix",0,"system_pages");
      if(meminfo) ksrphyserr = kstat_read(spl->kd,meminfo,NULL);
   }
   if (ksrphyserr != -1) {
      kstat_named_t *totalmem_pgs  = kstat_data_lookup(meminfo, "physmem");
      kstat_named_t *lockedmem_pgs = kstat_data_lookup(meminfo, "pageslocked");
      kstat_named_t *pages         = kstat_data_lookup(meminfo, "pagestotal");

      pl->totalMem   = totalmem_pgs->value.ui64 * CRT_page_size_kib;
      pl->usedMem    = lockedmem_pgs->value.ui64 * CRT_page_size_kib;
      // Not sure how to implement this on Solaris - suggestions welcome!
      pl->cachedMem  = 0;
      // Not really "buffers" but the best Solaris analogue that I can find to
      // "memory in use but not by programs or the kernel itself"
      pl->buffersMem = (totalmem_pgs->value.ui64 - pages->value.ui64) * CRT_page_size_kib;
   } else {
      // Fall back to basic sysconf if kstat isn't working
      pl->totalMem = sysconf(_SC_PHYS_PAGES) * CRT_page_size;
      pl->buffersMem = 0;
      pl->cachedMem  = 0;
      pl->usedMem    = pl->totalMem - (sysconf(_SC_AVPHYS_PAGES) * CRT_page_size);
   }
   // Part 2 - swap
   struct swaptable    *sl = NULL;
   struct swapent      *swapdev = NULL;
   char                *spathbase = NULL;
   uint64_t            totalswap = 0;
   uint64_t            totalfree = 0;
   int nswap = swapctl(SC_GETNSWP, NULL);
   if (nswap > 0) {
      sl = xMalloc((nswap * sizeof(swapent_t)) + sizeof(int));
      if(sl) spathbase = xMalloc(nswap * MAXPATHLEN);
   }
   if (spathbase != NULL) {
      char *spath = spathbase;
      swapdev = sl->swt_ent;
      for (int i = 0; i < nswap; i++, swapdev++) {
         swapdev->ste_path = spath;
         spath += MAXPATHLEN;
      }
      sl->swt_n = nswap;
      nswap = swapctl(SC_LIST, sl);
      if (nswap > 0) {
         swapdev = sl->swt_ent;
         for (int i = 0; i < nswap; i++, swapdev++) {
            totalswap += swapdev->ste_pages;
            totalfree += swapdev->ste_free;
         }
      }
   }
   free(spathbase);
   free(sl);
   pl->totalSwap = totalswap * CRT_page_size_kib;
   pl->usedSwap  = pl->totalSwap - (totalfree * CRT_page_size_kib);
}

void ProcessList_delete(ProcessList* pl) {
   SolarisProcessList* spl = (SolarisProcessList*) pl;
   ProcessList_done(pl);
   free(spl->cpus);
   if (spl->kd) kstat_close(spl->kd);
#ifndef HAVE_LIBPROC
   if(spl->proc_dir) closedir(spl->proc_dir);
#endif
   free(spl);
}

static void fill_from_psinfo(Process *proc, const psinfo_t *_psinfo) {
   SolarisProcess *sproc = (SolarisProcess *)proc;
   sproc->taskid            = _psinfo->pr_taskid;
   sproc->projid            = _psinfo->pr_projid;
#ifdef HAVE_PSINFO_T_PR_POOLID
   sproc->poolid            = _psinfo->pr_poolid;
#endif
#ifdef HAVE_PSINFO_T_PR_CONTRACT
   sproc->contid            = _psinfo->pr_contract;
#endif
   // NOTE: This 'percentage' is a 16-bit BINARY FRACTIONS where 1.0 = 0x8000
   // Source: https://docs.oracle.com/cd/E19253-01/816-5174/proc-4/index.html
   // (accessed on 18 November 2017)
   proc->percent_mem        = ((uint16_t)_psinfo->pr_pctmem / (double)32768) * (double)100;
   proc->ruid               = _psinfo->pr_uid;
   proc->euid               = _psinfo->pr_euid;
   proc->pgrp               = _psinfo->pr_pgid;
   proc->nlwp               = _psinfo->pr_nlwp;
   proc->tty_nr             = _psinfo->pr_ttydev;
   proc->m_resident         = _psinfo->pr_rssize / CRT_page_size_kib;
   proc->m_size             = _psinfo->pr_size / CRT_page_size_kib;
   sproc->argv_offset       = _psinfo->pr_argv > MAX_VALUE_OF(off_t) ? -1 : (off_t)_psinfo->pr_argv;
   sproc->envv_offset       = _psinfo->pr_envp > MAX_VALUE_OF(off_t) ? -1 : (off_t)_psinfo->pr_envp;
   sproc->data_model        = _psinfo->pr_dmodel;
}

static void fill_last_pass(Process *proc, time_t now) {
	SolarisProcess *sproc = (SolarisProcess *)proc;
	sproc->kernel = sproc->realppid <= 0 && sproc->realpid != 1;
}

#ifdef HAVE_LIBPROC

/* NOTE: the following is a callback function of type proc_walk_f
 *       and MUST conform to the appropriate definition in order
 *       to work.  See libproc(3LIB) on a Solaris or Illumos
 *       system for more info.
 */
static int SolarisProcessList_walkproc(psinfo_t *_psinfo, lwpsinfo_t *_lwpsinfo, void *listptr) {
   bool preExisting;
   pid_t getpid;

   // Setup process list
   ProcessList *pl = (ProcessList*) listptr;
   SolarisProcessList *spl = (SolarisProcessList*) listptr;

   id_t lwpid_real = _lwpsinfo->pr_lwpid;
   if (lwpid_real > 1023) return 0;
   pid_t lwpid   = (_psinfo->pr_pid * 1024) + lwpid_real;
   bool onMasterLWP = (_lwpsinfo->pr_lwpid == _psinfo->pr_lwp.pr_lwpid);
   if (onMasterLWP) {
      getpid = _psinfo->pr_pid * 1024;
   } else {
      getpid = lwpid;
   }
   Process *proc             = ProcessList_getProcess(pl, getpid, &preExisting, (Process_New) SolarisProcess_new);
   SolarisProcess *sproc     = (SolarisProcess*) proc;

   // Common code pass 1
   if(preExisting) {
      if(proc->ruid != _psinfo->pr_uid) proc->real_user = NULL;
      if(proc->euid != _psinfo->pr_euid) proc->effective_user = NULL;
   }

   fill_from_psinfo(proc, _psinfo);
   proc->show               = false;
   proc->priority           = _lwpsinfo->pr_pri;
   proc->nice               = _lwpsinfo->pr_nice - NZERO;
   proc->processor          = _lwpsinfo->pr_onpro;
   proc->state              = _lwpsinfo->pr_sname;

   if (!preExisting) {
      sproc->realpid        = _psinfo->pr_pid;
      sproc->lwpid          = lwpid_real;
#ifdef HAVE_ZONE_H
      sproc->zoneid         = _psinfo->pr_zoneid;
      sproc->zname          = SolarisProcessList_readZoneName(spl->kd, sproc);
#endif
      proc->name            = xStrdup(_psinfo->pr_fname);
      proc->comm            = xStrdup(_psinfo->pr_psargs);
      proc->commLen         = strnlen(_psinfo->pr_psargs, PRFNSZ);
   }
   if(!proc->real_user) {
      proc->real_user       = UsersTable_getRef(pl->usersTable, proc->ruid);
   }
   if(!proc->effective_user) {
      proc->effective_user  = UsersTable_getRef(pl->usersTable, proc->euid);
   }

   // End common code pass 1

   if (onMasterLWP) { // Are we on the representative LWP?
      proc->ppid            = (_psinfo->pr_ppid * 1024);
      proc->tgid            = (_psinfo->pr_ppid * 1024);
      sproc->realppid       = _psinfo->pr_ppid;
      // See note above (in common section) about this BINARY FRACTION
      proc->percent_cpu     = ((uint16_t)_psinfo->pr_pctcpu/(double)32768) * spl->online_cpu_count * 100;
      proc->time            = _psinfo->pr_time.tv_sec * 100 + _psinfo->pr_time.tv_nsec / 10000000;
      if(!preExisting) { // Tasks done only for NEW processes
         sproc->is_lwp = false;
         proc->starttime_ctime = _psinfo->pr_start.tv_sec;
      }

      // Update proc and thread counts based on settings
      pl->totalTasks++;
      pl->thread_count += proc->nlwp;
      if (sproc->kernel) {
	 pl->kernel_process_count++;
         pl->kernel_thread_count += proc->nlwp;
      }
      if (proc->state == 'O') pl->running_process_count++;
      proc->show = !(pl->settings->hide_kernel_processes && sproc->kernel);
   } else { // We are not in the master LWP, so jump to the LWP handling code
      proc->percent_cpu     = ((uint16_t)_lwpsinfo->pr_pctcpu/(double)32768) * spl->online_cpu_count * 100;
      proc->time            = _lwpsinfo->pr_time.tv_sec * 100 + _lwpsinfo->pr_time.tv_nsec / 10000000;
      if (!preExisting) { // Tasks done only for NEW LWPs
         sproc->is_lwp         = true;
         proc->ppid            = _psinfo->pr_pid * 1024;
         proc->tgid            = _psinfo->pr_pid * 1024;
         sproc->realppid       = _psinfo->pr_pid;
         proc->starttime_ctime = _lwpsinfo->pr_start.tv_sec;
      }

      if(!pl->settings->hide_thread_processes) {
         proc->show = !(pl->settings->hide_kernel_processes && sproc->kernel);
      }

      if (proc->state == 'O') pl->running_thread_count++;
   } // Top-level LWP or subordinate LWP

   // Common code pass 2

   if (!preExisting) {
      fill_last_pass(proc, spl->last_updated);
      ProcessList_add(pl, proc);
   }

   proc->updated = true;

   // End common code pass 2

   return 0;
}

void ProcessList_goThroughEntries(ProcessList* this) {
   SolarisProcessList_scanCPUTime(this);
   SolarisProcessList_scanMemoryInfo(this);
   ((SolarisProcessList *)this)->last_updated = time(NULL);
   this->kernel_process_count = 1;
   proc_walk(SolarisProcessList_walkproc, this, PR_WALK_LWP);
}

#else

void ProcessList_goThroughEntries(ProcessList *super) {
	SolarisProcessList *this = (SolarisProcessList *)super;

	SolarisProcessList_scanCPUTime(super);
	SolarisProcessList_scanMemoryInfo(super);

	if(this->proc_dir) rewinddir(this->proc_dir);
	else {
		this->proc_dir = opendir("/proc");
		if(!this->proc_dir) return;
	}

	this->last_updated = time(NULL);

	struct dirent *e;
	while((e = readdir(this->proc_dir))) {
		bool is_existing;
		char *end_p;
		long int pid = strtol(e->d_name, &end_p, 10);
		if(*end_p) continue;
		size_t len = end_p - e->d_name;
		char path[len + 9];
		memcpy(path, e->d_name, len);
		strcpy(path + len, "/psinfo");
		int fd = openat(dirfd(this->proc_dir), path, O_RDONLY);
		if(fd == -1) continue;

		psinfo_t info;
		int s = xread(fd, &info, sizeof info);
		close(fd);
		assert((int)sizeof info > 0);
		if(s < (int)sizeof info) continue;
		if(info.pr_pid != pid) continue;

		Process *proc = ProcessList_getProcess(super, pid, &is_existing, (Process_New)SolarisProcess_new);
		SolarisProcess *sol_proc = (SolarisProcess *)proc;

		if(is_existing) {
			if(proc->ruid != info.pr_uid) proc->real_user = NULL;
			if(proc->euid != info.pr_euid) proc->effective_user = NULL;
		}
		fill_from_psinfo(proc, &info);
		if(!is_existing) {
			proc->tgid = info.pr_pid;
			sol_proc->realpid = info.pr_pid;
			sol_proc->is_lwp = false;
			sol_proc->lwpid = -1;
#ifdef HAVE_ZONE_H
			sol_proc->zoneid = info.pr_zoneid;
			sol_proc->zname = SolarisProcessList_readZoneName(this->kd, sol_proc);
#endif
			proc->real_user = UsersTable_getRef(super->usersTable, proc->ruid);
			proc->effective_user = UsersTable_getRef(super->usersTable, proc->euid);
			proc->name = xStrdup(info.pr_fname);
			proc->comm = xStrdup(info.pr_psargs);
			//proc->commLen = strnlen(info.pr_psargs, PRFNSZ);
			proc->starttime_ctime = info.pr_start.tv_sec;
		}
		proc->ppid = info.pr_ppid;
		sol_proc->realppid = info.pr_ppid;
		proc->percent_cpu = ((uint16_t)info.pr_pctcpu/(double)32768) * this->online_cpu_count * 100;
		proc->time = info.pr_time.tv_sec * 100 + info.pr_time.tv_nsec / 10000000;
		proc->nlwp = info.pr_nlwp;
		proc->state = info.pr_lwp.pr_sname;
		proc->nice = info.pr_lwp.pr_nice - NZERO;
		proc->priority = info.pr_lwp.pr_pri;
		proc->processor = info.pr_lwp.pr_onpro;
		if(!proc->real_user) {
			proc->real_user = UsersTable_getRef(super->usersTable, proc->ruid);
		}
		if(!proc->effective_user) {
			proc->effective_user = UsersTable_getRef(super->usersTable, proc->euid);
		}

		if(!is_existing) {
			fill_last_pass(proc, this->last_updated);
			ProcessList_add(super, proc);
		}

		super->totalTasks++;
		super->thread_count += proc->nlwp;
		if(sol_proc->kernel) {
			super->kernel_process_count++;
			super->kernel_thread_count += proc->nlwp;
		}
		if(proc->state == 'O') {
			super->running_process_count++;
			super->running_thread_count++;
		}

		proc->show = !(super->settings->hide_kernel_processes && sol_proc->kernel);

		proc->updated = true;
	}
}

#endif
