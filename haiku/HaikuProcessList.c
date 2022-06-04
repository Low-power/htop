/*
htop - haiku/HaikuProcessList.c
(C) 2014 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "HaikuProcessList.h"
#include "HaikuProcess.h"
#include "StringUtils.h"
#include "Settings.h"
#include "Platform.h"
#include "CRT.h"
#include <OS.h>
#include <dlfcn.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/*{
#include "ProcessList.h"
#include <SupportDefs.h>

struct cpu_data {
	bigtime_t time;
	bigtime_t period;
};

typedef struct {
	ProcessList super;

	int team_count;
	struct cpu_data *cpu_data;
	bigtime_t last_updated;
	bigtime_t interval;
} HaikuProcessList;
}*/


ProcessList* ProcessList_new(UsersTable* usersTable, const Hashtable *pidWhiteList, uid_t userId) {
   HaikuProcessList *this = xCalloc(1, sizeof(HaikuProcessList));
   ProcessList_init(&this->super, Class(HaikuProcess), usersTable, pidWhiteList, userId);
   system_info si;
   get_system_info(&si);
   this->cpu_data = xCalloc(si.cpu_count, sizeof(struct cpu_data));
   this->super.cpuCount = si.cpu_count;
   this->super.totalMem = si.max_pages * (B_PAGE_SIZE / 1024);
   return &this->super;
}

void ProcessList_delete(ProcessList* this) {
   ProcessList_done(this);
   free(((HaikuProcessList *)this)->cpu_data);
   free(this);
}

static const char state_name_map[] = {
	[B_THREAD_RUNNING] = 'O',
	[B_THREAD_READY] = 'R',
	[B_THREAD_RECEIVING] = 'D',
	[B_THREAD_ASLEEP] = 'S',
	[B_THREAD_SUSPENDED] = 'T',
	[B_THREAD_WAITING] = 'S'
};

static void get_extended_team_info(pid_t team_id, uid_t *ruid, uid_t *euid, pid_t *pgrp, pid_t *session) {
	// TODO
	*ruid = -1;
	*euid = -1;
	*pgrp = -1;
	*session = -1;
}

static pid_t get_parent_team_id(pid_t team_id) {
	// XXX: This system call works only for the calling team
	static bool is_available = true;
	if(!is_available) return -1;
	static pid_t (*get_id)(pid_t, int32_t);
	if(!get_id) {
		void *libroot = get_libroot();
		if(!libroot) {
			is_available = false;
			return -1;
		}
		*(void **)&get_id = dlsym(libroot, "_kern_process_info");
		if(!get_id) {
			is_available = false;
			return -1;
		}
	}
	return get_id(team_id, 3);
}

void ProcessList_goThroughEntries(ProcessList *super) {
	HaikuProcessList *this = (HaikuProcessList *)super;

	bigtime_t now = system_time();
	this->interval = now - this->last_updated;
	this->last_updated = now;

	system_info si;
	get_system_info(&si);

	for(int i = 0; i < super->cpuCount; i++) {
		struct cpu_data *data = this->cpu_data + i;
		if(i < si.cpu_count) {
			bigtime_t current_time = si.cpu_infos[i].active_time;
			data->period = current_time > data->time ? current_time - data->time : 0;
			data->time = current_time;
		} else {
			data->period = 0;
		}
	}

	super->usedMem = si.used_pages * CRT_page_size_kib;

	struct vm_stat vm_stat;
	if(Platform_getVMStat(&vm_stat, sizeof vm_stat)) {
		// Is this correct?
		long int cache_page_count =
			si.max_pages - (si.used_pages + vm_stat.free_memory / CRT_page_size);
		super->cachedMem = cache_page_count > 0 ? cache_page_count * CRT_page_size_kib : 0;
		super->totalSwap = vm_stat.max_swap_space / 1024;
		super->usedSwap = (vm_stat.max_swap_space - vm_stat.free_swap_space) / 1024;
	} else {
		super->cachedMem = 0;
	}

	this->team_count = 0;

	int32 team_cookie = 0;
	team_info team_info;
	while(get_next_team_info(&team_cookie, &team_info) == B_OK) {
		long int *nthr_p = NULL;
		uid_t ruid, euid;
		pid_t pgrp, session;
		get_extended_team_info(team_info.team, &ruid, &euid, &pgrp, &session);
		if(ruid == (uid_t)-1) ruid = team_info.uid;
		if(euid == (uid_t)-1) euid = team_info.uid;
		int32 thread_cookie = 0;
		thread_info thread_info;
		while(get_next_thread_info(team_info.team, &thread_cookie, &thread_info) == B_OK) {
			bool is_existing;
			Process *proc =
				ProcessList_getProcess(super, thread_info.thread, &is_existing,
					(Process_New)HaikuProcess_new);
			HaikuProcess *haiku_proc = (HaikuProcess *)proc;
			if(is_existing) {
				if(proc->ruid != ruid) proc->real_user = NULL;
				if(proc->euid != euid) proc->effective_user = NULL;
			}
			proc->state = thread_info.state < sizeof state_name_map ?
				state_name_map[thread_info.state] : '?';
			proc->ruid = ruid;
			proc->euid = euid;
			proc->ppid = get_parent_team_id(team_info.team);
			proc->pgrp = pgrp;
			proc->session = session;
			proc->priority = thread_info.priority;
			proc->nice = B_NORMAL_PRIORITY - thread_info.priority;
			bigtime_t time = thread_info.user_time + thread_info.kernel_time;
			proc->time = time / 10000;
			double delta = time > haiku_proc->time_usec ?
				time - haiku_proc->time_usec : 0;
			proc->percent_cpu = delta / (double)this->interval * 100;
			haiku_proc->time_usec = time;
			if(!proc->real_user) {
				proc->real_user = UsersTable_getRef(super->usersTable, proc->ruid);
			}
			if(!proc->effective_user) {
				proc->effective_user = UsersTable_getRef(super->usersTable, proc->euid);
			}
			if(!is_existing) {
				proc->tgid = team_info.team;
				ProcessList_add(super, proc);
			}
			if(!is_existing || super->settings->updateProcessNames) {
				free(proc->name);
				free(proc->comm);
				proc->name = xStrdup(thread_info.name);
				proc->comm = xStrdup(team_info.args);
			}
			super->totalTasks++;
			super->thread_count++;
			if(Process_isKernelProcess(proc)) {
				super->kernel_process_count++;
				super->kernel_thread_count++;
			}
			if(proc->state == 'O') {
				super->running_process_count++;
				super->running_thread_count++;
			}
			proc->show = (!((super->settings->hide_kernel_processes && Process_isKernelProcess(proc)) ||
				(super->settings->hide_thread_processes && Process_isExtraThreadProcess(proc))));
			proc->updated = true;
			if(!Process_isExtraThreadProcess(proc)) nthr_p = &proc->nlwp;
		}
		if(nthr_p) *nthr_p = team_info.thread_count;
		this->team_count++;
	}
}
