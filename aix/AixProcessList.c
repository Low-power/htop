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

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
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
   // per libperfstat.h, ticks
   unsigned long long utime;
   unsigned long long stime;
   unsigned long long itime;
   unsigned long long wtime;
} CPUData;

typedef struct AixProcessList_ {
   ProcessList super;
   CPUData* cpus;
#ifndef __PASE__a
   perfstat_cpu_t* ps_cpus;
#endif
} AixProcessList;

#ifndef Process_isKernelThread
#define Process_isKernelThread(_process) (_process->kernel == 1)
#endif

#ifndef Process_isUserlandThread
// XXX
#define Process_isUserlandThread(_process) (false)
#endif

}*/

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList, uid_t userId) {
   AixProcessList* apl = xCalloc(1, sizeof(AixProcessList));
   ProcessList* this = (ProcessList*) apl;
   ProcessList_init(this, Class(AixProcess), usersTable, pidWhiteList, userId);

#ifndef __PASE__
   perfstat_memory_total_t mt;
   perfstat_cpu_total_t ct;
   perfstat_memory_total(NULL, &mt, sizeof(mt), 1);
   perfstat_cpu_total(NULL, &ct, sizeof(ct), 1);
   // most of these are in 4KB sized pages
   this->totalMem = mt.real_total * 4;
   this->totalSwap = mt.pgsp_total * 4;

   this->cpuCount = ct.ncpus;
   apl->ps_cpus = xCalloc (this->cpuCount, sizeof (perfstat_cpu_t));
#else
   this->cpuCount = sysconf (_SC_NPROCESSORS_CONF);
   this->totalMem = sysconf (_SC_AIX_REALMEM);
#endif

   apl->cpus = xCalloc (this->cpuCount, sizeof (CPUData));

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
    perfstat_id_t id;
    int i;

    super = (ProcessList*) apl;
    strcpy (id.name, FIRST_CPU);
    
    perfstat_cpu (&id, apl->ps_cpus, sizeof (perfstat_cpu_t), super->cpuCount);
    for (i = 0; i < super->cpuCount; i++) {
        apl->cpus [i].utime = apl->ps_cpus [i].user;
        apl->cpus [i].stime = apl->ps_cpus [i].sys;
        apl->cpus [i].itime = apl->ps_cpus [i].idle;
        apl->cpus [i].wtime = apl->ps_cpus [i].wait;
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
    struct vminfo64 vi;
    if (vmgetinfo (&vi, VMINFO64, sizeof (struct vminfo64)) == -1) {
        fprintf (stderr, "htop: AixProcessList_scanMemoryInfo failed: vmgetinfo error\n");
        _exit (1);
    }

    pl->freeMem = vi.memavailable / 0x1000;
    // XXX: cached memory? nmon can do it...
    pl->cachedMem = 0;
    pl->buffersMem = 0;
    pl->sharedMem = 0;
    pl->usedMem = pl->totalMem - pl->freeMem;
#endif
}

static char *AixProcessList_readProcessName (struct procentry64 *pe) {
	char argvbuf [0x100000]; // completely arbitrary
	int i;
	// if empty fall back
	if (getargs (pe, sizeof (struct procentry64), argvbuf, 0x100000) == 0 && *argvbuf) {
		// args are seperated by NUL, double NUL terminates
		for (i = 0; i < 0x100000; i++) {
			if (argvbuf [i] == '\0' && argvbuf [i + 1] != '\0')
				argvbuf [i] = ' ';
		}
		return xStrdup (argvbuf);
	}
	return xStrdup (pe->pi_comm);
}

void ProcessList_goThroughEntries(ProcessList* super) {
    AixProcessList* apl = (AixProcessList*)super;
    Settings* settings = super->settings;
    bool hideKernelThreads = settings->hideKernelThreads;
    bool hideUserlandThreads = settings->hideUserlandThreads;
    bool preExisting;
    Process *proc;
    AixProcess *ap;
    /* getprocs stuff */
    struct procentry64 *pes;
    struct procentry64 *pe;
    pid_t pid;
    int count, i;
    struct tm date;
    time_t t, pt;

    AixProcessList_scanMemoryInfo (super);
    AixProcessList_scanCpuInfo (apl);

    // 1000000 is what IBM ps uses; instead of rerunning getprocs with
    // a PID cookie, get one big clump. also, pid 0 is a strange proc,
    // seems to maybe represent the kernel? it has no name/argv, and
    // marked with SKPROC, so if you have the tree view and kernel
    // threads hidden, everything is hidden. oops? kernel threads seem
    // to be children of it as well, but having it in the list is odd
    pid = 1;
    count = getprocs64 (NULL, 0, NULL, 0, &pid, 1000000);
    if (count < 1) {
        fprintf (stderr, "htop: ProcessList_goThroughEntries failed; during count: %s\n", strerror (errno));
	_exit (1);
    }
    count += 25; // it's not atomic, new processes could spawn in next call
    pes = xCalloc (count, sizeof (struct procentry64));
    pid = 1;
    count = getprocs64 (pes, sizeof (struct procentry64), NULL, 0, &pid, count);
    if (count < 1) {
        fprintf (stderr, "htop: ProcessList_goThroughEntries failed; during listing\n");
	_exit (1);
    }

    t = time (NULL);
    for (i = 0; i < count; i++) {
        pe = pes + i;
        proc = ProcessList_getProcess(super, pe->pi_pid, &preExisting, (Process_New) AixProcess_new);
        ap = (AixProcess*) proc;

	proc->show = ! ((hideKernelThreads && Process_isKernelThread(ap))
                    || (hideUserlandThreads && Process_isUserlandThread(ap)));

        if (!preExisting) {
            ap->kernel = pe->pi_flags & SKPROC ? 1 : 0;
            proc->pid = pe->pi_pid;
            proc->ppid = pe->pi_ppid;
            /* XXX: tpgid? */
            proc->tgid = pe->pi_pid;
            proc->session = pe->pi_sid;
            proc->tty_nr = pe->pi_ttyd;
            proc->pgrp = pe->pi_pgrp;
            proc->st_uid = pe->pi_uid;
            proc->starttime_ctime = pe->pi_start;
            proc->user = UsersTable_getRef(super->usersTable, proc->st_uid);
            ProcessList_add((ProcessList*)super, proc);
            proc->comm = AixProcessList_readProcessName (pe);
            // copy so localtime_r works properly
            pt = pe->pi_start;
            (void) localtime_r((time_t*) &pt, &date);
            strftime(proc->starttime_show, 7, ((proc->starttime_ctime > t - 86400) || 0 ? "%R " : "%b%d "), &date);
	} else {
            if (settings->updateProcessNames) {
                free(proc->comm);
                proc->comm = AixProcessList_readProcessName (pe);
            }
        }

       ap->cid = pe->pi_cid;
       // XXX: are the numbers here right? I think these are based on pages or 1K?
       proc->m_size = pe->pi_drss;
       proc->m_resident = pe->pi_ru.ru_maxrss;
       proc->percent_mem = (pe->pi_drss * PAGE_SIZE_KB) / (double)(super->totalMem) * 100.0;
       //proc->percent_mem = pe->pi_prm; // don't use prm, it's not fractional
       //proc->percent_cpu = CLAMP(getpcpu(kproc), 0.0, this->cpuCount*100.0);
       proc->nlwp = pe->pi_thcount;
       proc->nice = pe->pi_nice;
       ap->utime = pe->pi_ru.ru_utime.tv_sec;
       ap->stime = pe->pi_ru.ru_stime.tv_sec;
       proc->time = ap->utime + ap->stime;
       proc->priority = pe->pi_ppri;

       switch (pe->pi_state) {
           case SIDL:    proc->state = 'I'; break;
           case SRUN:    proc->state = 'R'; break;
           case SSLEEP:  proc->state = 'S'; break;
           case SSTOP:   proc->state = 'T'; break;
           case SZOMB:   proc->state = 'Z'; break;
           case SACTIVE: proc->state = 'P'; break;
           case SSWAP:   proc->state = 'W'; break;
           default:      proc->state = '?';
       }

       if (Process_isKernelThread(ap)) {
          super->kernelThreads++;
       }

       super->totalTasks++;
       // SRUN ('R') means runnable, not running
       if (proc->state == 'P') {
          super->runningTasks++;
       }

	proc->updated = true;
    }

    free (pes);
}
