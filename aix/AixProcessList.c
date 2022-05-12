/*
htop - AixProcessList.c
(C) 2014 Hisham H. Muhammad
(C) 2018 Calvin Buckley
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"
#include "AixProcess.h"
#include "AixProcessList.h"
#include "CRT.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/types.h>
#include <sys/proc.h>
#include <procinfo.h>
#ifndef __PASE__
#include <libperfstat.h>
#else
#include <sys/vminfo.h>
#endif

/*{

#ifndef __PASE__
#include <libperfstat.h>
#endif

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
} AixProcessList;

}*/

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList, uid_t userId) {
   AixProcessList* apl = xCalloc(1, sizeof(AixProcessList));
   ProcessList* this = (ProcessList*) apl;
   ProcessList_init(this, Class(AixProcess), usersTable, pidWhiteList, userId);

#ifndef __PASE__
   perfstat_memory_total_t mt;
   perfstat_memory_total(NULL, &mt, sizeof(mt), 1);
   perfstat_cpu_total(NULL, &apl->ps_ct, sizeof(perfstat_cpu_total_t), 1);
   // most of these are in 4KB sized pages
   this->totalMem = mt.real_total * 4;
   this->totalSwap = mt.pgsp_total * 4;

   this->cpuCount = apl->ps_ct.ncpus;
   apl->ps_cpus = xCalloc (this->cpuCount, sizeof (perfstat_cpu_t));
#else
   this->cpuCount = sysconf (_SC_NPROCESSORS_CONF);
   this->totalMem = sysconf (_SC_AIX_REALMEM); /* XXX: returns bogus value on PASE */
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
    pl->sharedMem = 0;
    pl->usedMem = mt.real_inuse * 4;
    pl->totalSwap = mt.pgsp_total * 4;
    pl->freeSwap = mt.pgsp_free * 4;
    pl->usedSwap = pl->totalSwap - pl->freeSwap;
#else
    /* XXX: vmgetinfo won't work on PASE */
#endif
}

static void AixProcessList_readProcessName(struct procentry64 *pe, char **name, char **command) {
	*name = xStrdup(pe->pi_pid == 0 && !*pe->pi_comm ? "swapper" : pe->pi_comm);

	char argvbuf[256];
	if (getargs(pe, sizeof (struct procentry64), argvbuf, sizeof argvbuf) == 0 && *argvbuf) {
		// args are seperated by NUL, double NUL terminates
		unsigned int i = 0;
		while(argvbuf[i] || argvbuf[i + 1]) {
			if(!argvbuf[i]) argvbuf[i] = ' ';
			if(++i >= sizeof argvbuf - 2) {
				argvbuf[i] = 0;
			}
		}
		*command = xStrdup(argvbuf);
	} else {
		*command = xStrdup(*name);
	}
}

void ProcessList_goThroughEntries(ProcessList* super) {
	bool hide_kernel_processes = super->settings->hide_kernel_processes;
	/* getprocs stuff */
	struct procentry64 *pes;
	struct procentry64 *pe;
	pid_t pid = 0;
	int i;

	AixProcessList_scanMemoryInfo (super);
	AixProcessList_scanCpuInfo((AixProcessList *)super);

	// 1000000 is what IBM ps uses; instead of rerunning getprocs with
	// a PID cookie, get one big clump.
	int count = getprocs64 (NULL, 0, NULL, 0, &pid, 1000000);
	if (count < 1) {
		fprintf (stderr, "htop: ProcessList_goThroughEntries failed; during count: %s\n", strerror (errno));
		_exit (1);
	}
	count += 25; // it's not atomic, new processes could spawn in next call
	pes = xCalloc (count, sizeof (struct procentry64));
	pid = 0;
	count = getprocs64 (pes, sizeof (struct procentry64), NULL, 0, &pid, count);
	if (count < 1) {
		fprintf (stderr, "htop: ProcessList_goThroughEntries failed; during listing\n");
		_exit (1);
	}

	time_t t = time (NULL);
	for (i = 0; i < count; i++) {
		bool preExisting;
		pe = pes + i;
		Process *proc = ProcessList_getProcess(super, pe->pi_pid, &preExisting, (Process_New) AixProcess_new);
		AixProcess *ap = (AixProcess *)proc;

		proc->ppid = pe->pi_ppid;

		if (!preExisting) {
			ap->kernel = pe->pi_flags & SKPROC;
			proc->tgid = pe->pi_pid;
			proc->session = pe->pi_sid;
			proc->tty_nr = pe->pi_ttyd;
			proc->pgrp = pe->pi_pgrp;
			proc->ruid = pe->pi_uid;
			proc->euid = pe->pi_uid;	// XXX
			proc->real_user = UsersTable_getRef(super->usersTable, proc->ruid);
			proc->effective_user = UsersTable_getRef(super->usersTable, proc->euid);
			proc->starttime_ctime = pe->pi_start;
			ProcessList_add(super, proc);
			AixProcessList_readProcessName(pe, &proc->name, &proc->comm);
			// copy so localtime_r works properly
			time_t pt = pe->pi_start;
			struct tm date;
			localtime_r(&pt, &date);
			strftime(proc->starttime_show, 7, ((proc->starttime_ctime > t - 86400) || 0 ? "%R " : "%b%d "), &date);
		} else {
			if (super->settings->updateProcessNames) {
				free(proc->name);
				free(proc->comm);
				AixProcessList_readProcessName(pe, &proc->name, &proc->comm);
			}
		}

		ap->cid = pe->pi_cid;
		// XXX: are the numbers here right? I think these are based on pages or 1K?
		proc->m_resident = pe->pi_drss + pe->pi_trss;
		proc->m_size = pe->pi_ru.ru_maxrss;//pe->pi_drss + pe->pi_trss;
		proc->percent_mem = (pe->pi_drss + pe->pi_trss * CRT_page_size_kib) / (double)(super->totalMem) * 100.0;
		proc->nlwp = pe->pi_thcount;
		proc->nice = pe->pi_nice - NZERO;
		ap->utime = pe->pi_ru.ru_utime.tv_sec;
		ap->stime = pe->pi_ru.ru_stime.tv_sec;
		proc->time = ap->utime + ap->stime;
		proc->percent_cpu = (((double)proc->time / (t - proc->starttime_ctime)) * 100.0) / super->cpuCount;
		// sometimes this happens with freshly spawned in procs
		if (isnan(proc->percent_cpu))
			proc->percent_cpu = 0.0;
		proc->priority = pe->pi_ppri;

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
