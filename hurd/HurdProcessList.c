/*
htop - hurd/HurdProcessList.c
(C) 2014 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "HurdProcessList.h"
#include "HurdProcess.h"
#include "StringUtils.h"
#include "Settings.h"
#include "Platform.h"
#include "CRT.h"
#include <sys/time.h>
#include <sys/mman.h>
#include <mach/host_info.h>
#include <mach/vm_statistics.h>
#include <mach/vm_cache_statistics.h>
#include <mach/default_pager.h>
#include <mach/gnumach.h>
#include <hurd/paths.h>
#include <hurd.h>
#include <error.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

/*{
#include "ProcessList.h"
#include <hurd/process.h>

typedef struct {
	ProcessList super;

	process_t proc;
	struct timeval last_updated;
	struct timeval interval;
	struct timeval total_idle_time;
	struct timeval idle_time;
} HurdProcessList;
}*/


ProcessList* ProcessList_new(UsersTable* usersTable, const Hashtable *pidWhiteList, uid_t userId) {
   HurdProcessList *this = xCalloc(1, sizeof(HurdProcessList));
   ProcessList_init(&this->super, Class(HurdProcess), usersTable, pidWhiteList, userId);
   this->proc = getproc();
   this->idle_time.tv_sec = -1;
   this->super.cpuCount = 1;
   return &this->super;
}

void ProcessList_delete(ProcessList* this) {
   ProcessList_done(this);
   free(this);
}

static void scan_memory_info(ProcessList *this) {
	this->buffersMem = 0;
	this->totalSwap = 0;
	this->usedSwap = 0;

	struct host_basic_info basic_info;
	size_t size = sizeof basic_info;
	error_t e = host_info(get_host_port(), HOST_BASIC_INFO, (host_info_t)&basic_info, &size);
	if(e) goto fail;
	this->totalMem = basic_info.memory_size / 1024;
	struct vm_statistics vm_stat;
	e = vm_statistics(mach_task_self(), &vm_stat);
	if(e) goto fail;
	this->usedMem = (vm_stat.active_count + vm_stat.wire_count) * CRT_page_size_kib;
	struct vm_cache_statistics vm_cache_stat;
	e = vm_cache_statistics(mach_task_self(), &vm_cache_stat);
	if(e) goto fail;
	this->cachedMem = vm_cache_stat.cache_count * CRT_page_size_kib;

#ifdef _SERVERS_DEFPAGER
	mach_port_t defpager = file_name_lookup(_SERVERS_DEFPAGER, O_READ, 0);
	if(defpager != MACH_PORT_NULL) {
		struct default_pager_info info;
		e = default_pager_info(defpager, &info);
		if(!e) {
			this->totalSwap = info.dpi_total_space / 1024;
			this->usedSwap = (info.dpi_total_space - info.dpi_free_space) / 1024;
		}
		mach_port_deallocate(mach_task_self(), defpager);
	}
#endif

	return;

fail:
	this->totalMem = 0;
	this->usedMem = 0;
	this->buffersMem = 0;
	this->cachedMem = 0;
}

static void scan_thread_info(HurdProcessList *this, const struct procinfo *info, Process *proc) {
	int proc_state = INT_MAX;
	proc->nlwp = 0;
	for(int i = 0; i < info->nthreads; i++) {
		if(info->threadinfos[i].died) continue;
		int state = info->threadinfos[i].pis_bi.run_state;
		if(state > 0 && state < proc_state) proc_state = state;
		this->super.thread_count++;
		if(state == TH_STATE_RUNNING) this->super.running_thread_count++;
		proc->nlwp++;
		if(this->idle_time.tv_sec == -1 && (info->threadinfos[i].pis_bi.flags & TH_FLAGS_IDLE)) {
			const struct time_value *tv = &info->threadinfos[i].pis_bi.system_time;
			struct timeval idle_tv = { .tv_sec = tv->seconds, .tv_usec = tv->microseconds };
			if(timercmp(&idle_tv, &this->total_idle_time, >)) {
				timersub(&idle_tv, &this->total_idle_time, &this->idle_time);
			} else {
				this->idle_time.tv_sec = 0;
				this->idle_time.tv_usec = 0;
			}
			this->total_idle_time = idle_tv;
		}
	}
	switch(proc_state) {
		case TH_STATE_RUNNING:
			proc->state = 'R';
			break;
		case TH_STATE_STOPPED:
			proc->state = 'T';
			break;
		case TH_STATE_WAITING:
			proc->state = 'S';
			break;
		case TH_STATE_UNINTERRUPTIBLE:
			proc->state = 'D';
			break;
		case TH_STATE_HALTED:
			proc->state = 'T';
			break;
		default:
			proc->state = '?';
			break;
	}
}

static void scan_process_command_line(const HurdProcessList *this, pid_t pid, Process *proc) {
	size_t len = 256;
	proc->comm = xMalloc(len);
	error_t e = proc_getprocargs(this->proc, pid, &proc->comm, &len);
	if(e || !len) {
		proc->name = xMalloc(1);
		*proc->name = 0;
		free(proc->comm);
		if(e) {
			proc->comm = xStrdup(strerror(e));
		} else {
			proc->comm = xMalloc(1);
			*proc->comm = 0;
		}
		return;
	}
	assert(len <= 256);
	proc->comm = xRealloc(proc->comm, len);
	proc->argv0_length = --len;
	assert(!proc->comm[len]);
	proc->name = xStrdup(proc->comm);
	while(len > 0) {
		if(!proc->comm[--len]) {
			proc->comm[len] = ' ';
			proc->argv0_length = len;
		}
	}
}

void ProcessList_goThroughEntries(ProcessList *super, bool skip_processes) {
	HurdProcessList *this = (HurdProcessList *)super;

	struct timeval now;
	gettimeofday(&now, NULL);
	timersub(&now, &this->last_updated, &this->interval);
	this->last_updated = now;
	this->idle_time.tv_sec = -1;

	scan_memory_info(super);

	pid_t *pids;
	size_t count = 0;
	error_t e = proc_getallpids(this->proc, &pids, &count);
	if(e) error(1, e, "proc_getallpids");
	for(size_t i = 0; i < count; i++) {
		pid_t pid = pids[i];
		int flags = PI_FETCH_TASKINFO | PI_FETCH_THREADS | PI_FETCH_THREAD_BASIC;
		struct procinfo *info = NULL;
		size_t entry_count = 0;
		char *waits;
		size_t wait_count = 0;
		e = proc_getprocinfo(this->proc, pid, &flags, (procinfo_t *)&info, &entry_count, &waits, &wait_count);
		if(e) continue;
		bool is_existing;
		Process *proc = ProcessList_getProcess(super, pid, &is_existing, (Process_New)HurdProcess_new);
		HurdProcess *hurd_proc = (HurdProcess *)proc;
		if(is_existing) {
			if(info->state & PI_NOTOWNED) {
				proc->real_user = NULL;
				proc->effective_user = NULL;
			} else {
				if(proc->ruid != info->owner) proc->real_user = NULL;
				if(proc->euid != info->owner) proc->effective_user = NULL;
			}
		}
		scan_thread_info(this, info, proc);
		if(proc->state == '?') {
			if(info->state & PI_ZOMBIE) proc->state = 'Z';
			else if(info->state & PI_STOPPED) proc->state = 'T';
			else if(info->state & PI_WAITING) proc->state = 'W';	// wait(3)ing for children
		}
		proc->ruid = (info->state & PI_NOTOWNED) ? (uid_t)-1 : info->owner;
		proc->euid = proc->ruid;
		proc->ppid = info->ppid;
		proc->pgrp = info->pgrp;
		proc->session = info->session;
		proc->priority = info->taskinfo.base_priority;
		proc->m_size = info->taskinfo.virtual_size / CRT_page_size;
		proc->m_resident = info->taskinfo.resident_size / CRT_page_size;
		proc->percent_mem =
			(double)proc->m_resident / (super->totalMem / CRT_page_size_kib) * 100;
		struct timeval tv = {
			.tv_sec = info->taskinfo.user_time.seconds + info->taskinfo.system_time.seconds,
			.tv_usec = info->taskinfo.user_time.microseconds + info->taskinfo.system_time.microseconds
		};
		struct timeval delta;
		if(timercmp(&tv, &hurd_proc->tv, >)) {
			timersub(&tv, &hurd_proc->tv, &delta);
		} else {
			delta.tv_sec = 0;
			delta.tv_usec = 0;
		}
		proc->time = tv.tv_sec * 100 + tv.tv_usec / 10000;
		hurd_proc->tv = tv;
		proc->percent_cpu = ((double)delta.tv_sec * 1000000 + delta.tv_usec) /
			((double)this->interval.tv_sec * 1000000 + this->interval.tv_usec) * 100;
		if(!(info->state & PI_NOTOWNED) && !proc->real_user) {
			proc->real_user = UsersTable_getRef(super->usersTable, proc->ruid);
		}
		if(!(info->state & PI_NOTOWNED) && !proc->effective_user) {
			proc->effective_user = UsersTable_getRef(super->usersTable, proc->euid);
		}
		if(!is_existing) {
			proc->tgid = pid;
			proc->starttime_ctime = info->taskinfo.creation_time.seconds;
			ProcessList_add(super, proc);
		}
		if(!is_existing || super->settings->updateProcessNames) {
			free(proc->name);
			free(proc->comm);
			scan_process_command_line(this, pid, proc);
		}
		super->totalTasks++;
		if(proc->state == 'R') super->running_process_count++;
		proc->updated = true;
		munmap(info, entry_count * sizeof(int));
		munmap(waits, wait_count);
	}
	munmap(pids, count * sizeof(pid_t));
}
