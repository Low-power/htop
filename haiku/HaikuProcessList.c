/*
htop - haiku/HaikuProcessList.c
(C) 2014 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

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

#define _XOPEN_SOURCE 600
#include "config.h"
#include "HaikuProcessList.h"
#include "HaikuProcess.h"
#include "StringUtils.h"
#include "Settings.h"
#include "Platform.h"
#include "CRT.h"
#include <OS.h>
#include <dlfcn.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define KMESSAGE_BUFFER_ALIGNMENT 4
#define KMESSAGE_HEADER_MAGIC 0x6b4d7347

#define KMESSAGE_ALIGN(P,O) ((void *)(((uintptr_t)(P)+(O)+KMESSAGE_BUFFER_ALIGNMENT-1) & ~(KMESSAGE_BUFFER_ALIGNMENT-1)))

struct kmessage_field_header {
	type_code type;
	int32_t element_size;
	int32_t element_count;
	int32_t field_size;
	int16_t header_size;
	char name[0];
};

struct kmessage_field_value_header {
	int32_t size;
};

struct kmessage_header {
	uint32_t magic;
	int32_t size;
	uint32_t what;
	team_id sender;
	int32_t target_token;
	port_id reply_port;
	int32_t reply_token;
};

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
	*ruid = -1;
	*euid = -1;
	*pgrp = -1;
	*session = -1;

	static bool is_available = true;
	if(!is_available) return;
	static status_t (*get_extended_team_info_p)(int32_t, uint32_t, void *, size_t, size_t *);
	if(!get_extended_team_info_p) {
		void *libroot = get_libroot();
		if(!libroot) {
			is_available = false;
			return;
		}
		*(void **)&get_extended_team_info_p = dlsym(libroot, "_kern_get_extended_team_info");
		if(!get_extended_team_info_p) {
			is_available = false;
			return;
		}
	}
	size_t size;
	status_t status = get_extended_team_info_p(team_id, 1, NULL, 0, &size);
	if(status != B_OK && status != B_BUFFER_OVERFLOW) return;
	if(size < sizeof(struct kmessage_header)) return;
	char buffer[size];
	status = get_extended_team_info_p(team_id, 1, buffer, size, &size);
	if(status != B_OK) return;
	if(size < sizeof(struct kmessage_header)) return;
	const struct kmessage_header *header = (const struct kmessage_header *)buffer;
	if(header->magic != KMESSAGE_HEADER_MAGIC) return;
	if((size_t)header->size != size) return;
	struct kmessage_field_header *field_header =
		KMESSAGE_ALIGN(buffer, sizeof(struct kmessage_header));
	while((char *)field_header < buffer + size) {
		if(field_header->element_size == 4) {
			char *value_p = (char *)field_header + field_header->header_size;
			int32_t value;
			if(strcmp(field_header->name, "process group") == 0) {
				memcpy(&value, value_p, 4);
				*pgrp = value;
			} else if(strcmp(field_header->name, "session") == 0) {
				memcpy(&value, value_p, 4);
				*session = value;
			} else if(strcmp(field_header->name, "uid") == 0) {
				memcpy(&value, value_p, 4);
				*ruid = value;
			} else if(strcmp(field_header->name, "euid") == 0) {
				memcpy(&value, value_p, 4);
				*euid = value;
			}
		}
		field_header = KMESSAGE_ALIGN(field_header, field_header->field_size);
	}
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

void ProcessList_goThroughEntries(ProcessList *super, bool skip_processes) {
	HaikuProcessList *this = (HaikuProcessList *)super;

	bigtime_t now = system_time();
	this->interval = now - this->last_updated;
	this->last_updated = now;

	system_info si;
	get_system_info(&si);

#ifdef HAVE_SYSTEM_INFO_CPU_INFOS
#define CPU_INFOS si.cpu_infos
#elif defined HAVE_GET_CPU_INFO
	cpu_info cpu_infos[si.cpu_count];
	get_cpu_info(0, si.cpu_count, cpu_infos);
#define CPU_INFOS cpu_infos
#endif

	for(int i = 0; i < super->cpuCount; i++) {
		struct cpu_data *data = this->cpu_data + i;
		if(
#ifdef HAVE_CPU_INFO_ENABLED
		  CPU_INFOS[i].enabled &&
#endif
		  i < (int)si.cpu_count) {
			bigtime_t current_time = CPU_INFOS[i].active_time;
			data->period = current_time > data->time ? current_time - data->time : 0;
			data->time = current_time;
		} else {
			data->period = 0;
		}
	}

	super->usedMem = si.used_pages * CRT_page_size_kib;

#if !defined HAVE_SYSTEM_INFO_CACHED_PAGES || !defined HAVE_SYSTEM_INFO_MAX_SWAP_PAGES
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
#endif
#ifdef HAVE_SYSTEM_INFO_CACHED_PAGES
	super->cachedMem = si.cached_pages * CRT_page_size_kib;
#endif
#ifdef HAVE_SYSTEM_INFO_MAX_SWAP_PAGES
	super->totalSwap = si.max_swap_pages * CRT_page_size_kib;
	super->usedSwap = (si.max_swap_pages - si.free_swap_pages) * CRT_page_size_kib;
#endif

	if(skip_processes) return;

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
		unsigned long int vsize = 0, rssize = 0;
		ssize_t area_cookie = 0;
		area_info area_info;
		while(get_next_area_info(team_info.team, &area_cookie, &area_info) == B_OK) {
			vsize += area_info.size / CRT_page_size;
			rssize += area_info.ram_size / CRT_page_size;
		}
		float pmem = (float)rssize / (float)si.max_pages * 100;
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
#if defined NZERO && defined HAVE_SETPRIORITY
			// From Haiku src/system/libroot/posix/sys/priority.c
#define BZERO B_NORMAL_PRIORITY
#define BMAX (B_REAL_TIME_DISPLAY_PRIORITY - 1)
#define BMIN 1
			proc->nice = thread_info.priority < BZERO ?
				((BZERO - thread_info.priority) * (NZERO - 1)) / (BZERO - BMIN) + 1 :
				-(((thread_info.priority - BZERO) * NZERO + (BMAX - BZERO) / 2) / (BMAX - BZERO));
#else
			proc->nice = B_NORMAL_PRIORITY - thread_info.priority;
#endif
			bigtime_t time = thread_info.user_time + thread_info.kernel_time;
			proc->time = time / 10000;
			double delta = time > haiku_proc->time_usec ?
				time - haiku_proc->time_usec : 0;
			proc->percent_cpu = delta / (double)this->interval * 100;
			haiku_proc->time_usec = time;
			proc->m_size = vsize;
			proc->m_resident = rssize;
			proc->percent_mem = pmem;
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
			if(!is_existing || ProcessList_shouldUpdateProcessNames(super)) {
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
