/*
htop - aix/AixProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2018 Calvin Buckley
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "AixProcess.h"
#include "AixProcessList.h"
#include "CRT.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <procinfo.h>
#ifdef __PASE__
#include <sys/vminfo.h>
#endif

/*{
#include "ProcessList.h"
#ifndef __PASE__
#define Hyp_Name __Hyp_Name_dummy(struct __Hyp_Name_dummy); static const char *const __Hyp_Name__
#include <libperfstat.h>
#endif
#include <sys/time.h>

// publically consumed
typedef struct CPUData_ {
   // per libperfstat.h, clock ticks
   unsigned long long utime, stime, itime, wtime;
   // warning: doubles are 4 bytes in structs on AIX
   // ...not like we need precision here anyways
   double utime_p, stime_p, itime_p, wtime_p;
} CPUData;

typedef struct AixProcessList_ {
   ProcessList super;
   CPUData* cpus;
#ifndef __PASE__
   perfstat_cpu_t* ps_cpus;
   perfstat_cpu_total_t ps_ct;
#endif
   struct timeval last_updated;
} AixProcessList;
}*/

ProcessList* ProcessList_new(UsersTable* usersTable, const Hashtable *pidWhiteList, uid_t userId) {
   AixProcessList* apl = xCalloc(1, sizeof(AixProcessList));
   ProcessList* this = (ProcessList*) apl;
   ProcessList_init(this, Class(AixProcess), usersTable, pidWhiteList, userId);

#ifndef __PASE__
   perfstat_cpu_total(NULL, &apl->ps_ct, sizeof apl->ps_ct, 1);
   this->cpuCount = apl->ps_ct.ncpus;
   apl->ps_cpus = xCalloc(this->cpuCount, sizeof(perfstat_cpu_t));
   perfstat_memory_total_t mt;
   perfstat_memory_total(NULL, &mt, sizeof(mt), 1);
   // It is in 4 KiB sized pages
   this->totalMem = mt.real_total * 4;
#else
   this->cpuCount = sysconf(_SC_NPROCESSORS_CONF);
   this->totalMem = sysconf(_SC_AIX_REALMEM); /* XXX: returns bogus value on PASE */
#endif

   // smp requires avg + cpus as the 0th entry is avg
   apl->cpus = xCalloc (this->cpuCount + (this->cpuCount > 1), sizeof (CPUData));

   return this;
}

void ProcessList_delete(ProcessList* this) {
   AixProcessList* apl = (AixProcessList*)this;
   free (apl->cpus);
#ifndef __PASE__
   free (apl->ps_cpus);
#endif
   ProcessList_done(this);
   free(this);
}

static void AixProcessList_scanCpuInfo (AixProcessList *apl) {
#ifndef __PASE__
    ProcessList *super;
    // delay is in tenths of a second
    // XXX: may not be precise enough
    double delay, nproc;
    perfstat_id_t id;
    int i;
    // old values
    unsigned long long oct_utime, oct_stime, oct_itime, oct_wtime;

    super = (ProcessList*) apl;
    nproc = super->cpuCount;
    delay = (double)super->settings->delay / 10;
    strcpy (id.name, FIRST_CPU);

    // acquire deltas to get a percentage;
    // 1s captures = you get 100%, 2s -> 200%, and so on
    // so divide by the delay we're given in settings
    // for the average/total, we'll have to divide by nproc for avg
    oct_utime = apl->ps_ct.user;
    oct_stime = apl->ps_ct.sys;
    oct_itime = apl->ps_ct.idle;
    oct_wtime = apl->ps_ct.wait;
    perfstat_cpu_total (NULL, &apl->ps_ct, sizeof (perfstat_cpu_total_t), 1);
    apl->cpus [0].utime_p = ((double)(apl->ps_ct.user - oct_utime) / delay) / nproc;
    apl->cpus [0].stime_p = ((double)(apl->ps_ct.sys  - oct_stime) / delay) / nproc;
    apl->cpus [0].itime_p = ((double)(apl->ps_ct.idle - oct_itime) / delay) / nproc;
    apl->cpus [0].wtime_p = ((double)(apl->ps_ct.wait - oct_wtime) / delay) / nproc;

    // untested on a uniproc system; i assume avg is good enough for that
    if (nproc < 2)
        return;

    perfstat_cpu (&id, apl->ps_cpus, sizeof (perfstat_cpu_t), super->cpuCount);
    for (i = 0; i < super->cpuCount; i++) {
        // use old times for delta to calc percentage
        apl->cpus [i+1].utime_p = (double)(apl->ps_cpus [i].user - apl->cpus [i+1].utime) / delay;
        apl->cpus [i+1].stime_p = (double)(apl->ps_cpus [i].sys  - apl->cpus [i+1].stime) / delay;
        apl->cpus [i+1].itime_p = (double)(apl->ps_cpus [i].idle - apl->cpus [i+1].itime) / delay;
        apl->cpus [i+1].wtime_p = (double)(apl->ps_cpus [i].wait - apl->cpus [i+1].wtime) / delay;
        // uptime new times
        apl->cpus [i+1].utime = apl->ps_cpus [i].user;
        apl->cpus [i+1].stime = apl->ps_cpus [i].sys;
        apl->cpus [i+1].itime = apl->ps_cpus [i].idle;
        apl->cpus [i+1].wtime = apl->ps_cpus [i].wait;
    }
#endif
}

static void AixProcessList_scanMemoryInfo (ProcessList *pl) {
#ifndef __PASE__
    perfstat_memory_total_t mt;
    perfstat_memory_total(NULL, &mt, sizeof(mt), 1);
    pl->freeMem = mt.real_free * 4;
    pl->cachedMem = mt.numperm * 4;
    pl->buffersMem = 0;
    pl->usedMem = mt.real_inuse * 4;
    pl->totalSwap = mt.pgsp_total * 4;
    pl->freeSwap = mt.pgsp_free * 4;
    pl->usedSwap = pl->totalSwap - pl->freeSwap;
#else
    /* XXX: vmgetinfo won't work on PASE */
#endif
}

static void AixProcessList_readProcessName(struct procentry64 *pe, char **name, char **command, int *argv0_len) {
	*name = xStrdup(pe->pi_pid == 0 && !*pe->pi_comm ? "swapper" : pe->pi_comm);

	char argvbuf[256];
	if (getargs(pe, sizeof (struct procentry64), argvbuf, sizeof argvbuf) == 0 && *argvbuf) {
		// args are seperated by NUL, double NUL terminates
		unsigned int i = 0;
		*argv0_len = 0;
		while(argvbuf[i] || argvbuf[i + 1]) {
			if(!argvbuf[i]) {
				argvbuf[i] = ' ';
				if(!*argv0_len) *argv0_len = i;
			}
			if(++i >= sizeof argvbuf - 2) {
				argvbuf[i] = 0;
			}
		}
		*command = xStrdup(argvbuf);
		if(!*argv0_len) *argv0_len = i;
	} else {
		*command = xStrdup(*name);
		*argv0_len = strlen(*name);
	}
}

void ProcessList_goThroughEntries(ProcessList* super, bool skip_processes) {
	AixProcessList *this = (AixProcessList *)super;

	bool hide_kernel_processes = super->settings->hide_kernel_processes;

	struct timeval now;
	gettimeofday(&now, NULL);
	double interval = (double)(now.tv_sec - this->last_updated.tv_sec) +
		(double)(now.tv_usec - this->last_updated.tv_usec) / 1000000;
	if(interval < 0) interval = 0.000001;
	this->last_updated = now;

	AixProcessList_scanMemoryInfo(super);
	AixProcessList_scanCpuInfo(this);

	if(skip_processes) return;

	pid_t pid = 0;
	// 1000000 is what IBM ps uses; instead of rerunning getprocs with
	// a PID cookie, get one big clump.
	int count = getprocs64 (NULL, 0, NULL, 0, &pid, 1000000);
	if (count < 1) {
		fprintf (stderr, "htop: ProcessList_goThroughEntries failed; during count: %s\n", strerror (errno));
		_exit (1);
	}
	count += 25; // it's not atomic, new processes could spawn in next call
	struct procentry64 *pes = xCalloc(count, sizeof (struct procentry64));
	pid = 0;
	count = getprocs64 (pes, sizeof (struct procentry64), NULL, 0, &pid, count);
	if (count < 1) {
		fprintf (stderr, "htop: ProcessList_goThroughEntries failed; during listing\n");
		_exit (1);
	}

	for (int i = 0; i < count; i++) {
		bool preExisting;
		struct procentry64 *pe = pes + i;
		Process *proc = ProcessList_getProcess(super, pe->pi_pid, &preExisting, (Process_New) AixProcess_new);
		AixProcess *ap = (AixProcess *)proc;

		proc->ppid = pe->pi_ppid;
		proc->session = pe->pi_sid;
		proc->tty_nr = pe->pi_ttyd;
		proc->pgrp = pe->pi_pgrp;

		if (!preExisting) {
			ap->kernel = pe->pi_flags & SKPROC;
			proc->tgid = pe->pi_pid;
			proc->starttime_ctime = pe->pi_start;
			AixProcessList_readProcessName(pe, &proc->name, &proc->comm, &proc->argv0_length);
			ProcessList_add(super, proc);
		} else {
			if(proc->ruid != pe->pi_uid) proc->real_user = NULL;
			if(proc->euid != pe->pi_uid) proc->effective_user = NULL;	// XXX
			if (super->settings->updateProcessNames) {
				free(proc->name);
				free(proc->comm);
				AixProcessList_readProcessName(pe, &proc->name, &proc->comm, &proc->argv0_length);
			}
		}

		proc->ruid = pe->pi_uid;
		proc->euid = pe->pi_uid;	// XXX

		if(!proc->real_user) {
			proc->real_user = UsersTable_getRef(super->usersTable, proc->ruid);
		}
		if(!proc->effective_user) {
			proc->effective_user = UsersTable_getRef(super->usersTable, proc->euid);
		}

		ap->cid = pe->pi_cid;

		proc->m_resident = pe->pi_drss + pe->pi_trss;
		proc->m_size = pe->pi_size + pe->pi_dvm;
		proc->percent_mem =
			(double)proc->m_resident / (double)(super->totalMem / CRT_page_size_kib) * 100;

		/* WARNING: The AIX C library is broken:
		 * Despite their name, 'ru_utime.tv_usec' and
		 * 'ru_stime.tv_usec' here are actually in nanoseconds!
		 */
		struct timeval tv = {
			.tv_sec = pe->pi_ru.ru_utime.tv_sec + pe->pi_ru.ru_stime.tv_sec,
			.tv_usec = (pe->pi_ru.ru_utime.tv_usec + pe->pi_ru.ru_stime.tv_usec) / 1000
		};
		proc->time = tv.tv_sec * 100 + tv.tv_usec / 10000;
		double last_hundredths_time =
			(double)ap->tv.tv_sec * 100 + (double)ap->tv.tv_usec / 10000;
		double hundredths_time = (double)tv.tv_sec * 100 + (double)tv.tv_usec / 10000;
		proc->percent_cpu = hundredths_time > last_hundredths_time ?
			(hundredths_time - last_hundredths_time) / interval : 0;
		ap->tv = tv;

		proc->priority = pe->pi_ppri;
		proc->nice = pe->pi_nice - NZERO;

		proc->nlwp = pe->pi_thcount;

		switch (pe->pi_state) {
			case SIDL:    proc->state = 'I'; break;
			case SSTOP:   proc->state = 'T'; break;
			case SZOMB:   proc->state = 'Z'; break;
			case SACTIVE: proc->state = 'R'; break;
			case SSWAP:   proc->state = 'W'; break;
			default:      proc->state = '?';
		}

		super->totalTasks++;
		super->thread_count += proc->nlwp;
		if (Process_isKernelProcess(proc)) {
			super->kernel_process_count++;
			super->kernel_thread_count += proc->nlwp;
		}

		if (proc->state == 'R') {
			super->running_process_count++;
			super->running_thread_count++;
		}

		proc->show = !(hide_kernel_processes && Process_isKernelProcess(proc));

		proc->updated = true;
	}

	free (pes);
}
