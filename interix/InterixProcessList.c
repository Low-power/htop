/*
htop - interix/InterixProcessList.c
(C) 2014 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"
#include "InterixProcess.h"
#include "StringUtils.h"
#include "Settings.h"
#include <sys/procfs.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*{

}*/

static long int page_size;
static long int page_size_ki;

ProcessList* ProcessList_new(UsersTable* usersTable, const Hashtable *pidWhiteList, uid_t userId) {
   ProcessList* this = xCalloc(1, sizeof(ProcessList));
   ProcessList_init(this, Class(InterixProcess), usersTable, pidWhiteList, userId);
   return this;
}

void ProcessList_delete(ProcessList* this) {
   ProcessList_done(this);
   free(this);
}

static char state_name_map[] = {
	[PR_Free] = 'X',
	[PR_Initializing] = '1',
	[PR_Unconnected] = 'U',
	[PR_Active] = 'R',
	[PR_Stopped] = 'T',
	[PR_Waiting] = 'W',
	[PR_Tty] = 'S',
	[PR_Execing] = '7',
	[PR_Exiting] = '8',
	[PR_Exited] = 'Z',
};

void ProcessList_goThroughEntries(ProcessList *this) {
	unsigned long long int total_time_delta = 0;

	if(!page_size) {
		page_size = sysconf(_SC_PAGESIZE);
		if(page_size < 0) abort();
		page_size_ki = page_size / 1024;
	}

	DIR *dir = opendir("/proc");
	if(!dir) return;
	struct dirent *e;
	while((e = readdir(dir))) {
		bool is_existing;
		uid_t ruid, euid;
		unsigned long long int t;
		Process *proc;
		InterixProcess *i_proc;
		char *end_p;
		long int pid = strtol(e->d_name, &end_p, 10);
		if(*end_p) continue;
		size_t len = end_p - e->d_name;
		char path[6 + len + 9];
		memcpy(path, "/proc/", 6);
		memcpy(path + 6, e->d_name, len);
		strcpy(path + 6 + len, "/psinfo");
		FILE *f = fopen(path, "rb");
		if(f) {
			psinfo_t info;
			size_t s = fread(&info, sizeof info, 1, f);
			fclose(f);
			if(s < 1) goto try_stat;
			if(info.pr_pid != pid) continue;
			proc = ProcessList_getProcess(this, pid, &is_existing, (Process_New)InterixProcess_new);
			i_proc = (InterixProcess *)proc;
			i_proc->is_posix_process = !(info.pr_flag & PR_FOREIGN);
			if(info.pr_flag & PR_ISTOP) proc->state = 't';
			else if(info.pr_state < sizeof state_name_map) {
				proc->state = state_name_map[info.pr_state];
			} else proc->state = '?';
			proc->nlwp = info.pr_nlwp;
			if(!is_existing) {
				proc->tgid = info.pr_pid;
				i_proc->native_pid = info.pr_natpid;
				proc->starttime_ctime = info.pr_start;
			} else if(this->settings->updateProcessNames) {
				free(proc->name);
				free(proc->comm);
			}
			if(!is_existing || this->settings->updateProcessNames) {
				proc->name = xStrdup(info.pr_fname);
				proc->comm = xStrdup(*info.pr_psargs ? info.pr_psargs : info.pr_fname);
			}
			proc->ppid = info.pr_ppid;
			proc->pgrp = info.pr_pgid;
			proc->session = info.pr_sid;
			i_proc->native_sid = info.pr_natsid;
			ruid = info.pr_uid;
			euid = info.pr_euid;
			proc->m_size = info.pr_size / page_size_ki;
			proc->m_resident = info.pr_rssize / page_size_ki;
			proc->tty_nr = info.pr_ttydev;
			//t = info.pr_time * 100;
			t = info.pr_time;
#if 0
			proc->nice = info.pr_lwp.pr_nice - NZERO;
#else
			proc->nice = info.pr_lwp.pr_nice;
#endif
		} else {
try_stat:
			strcpy(path + 6 + len, "/stat");
			f = fopen(path, "r");
			if(!f) continue;
			char *line = String_readLine(f);
			if(!line) continue;
			if(strncmp(line, "argv0	", 6)) {
				free(line);
				continue;
			}
			size_t comm_len = strlen(line + 6) + 1;
			char *comm = memmove(line, line + 6, comm_len);
			line = String_readLine(f);
			if(!line || strncmp(line, "pid	", 4) || pid != atol(line + 4)) {
				free(comm);
				free(line);
				continue;
			}
			proc = ProcessList_getProcess(this, pid, &is_existing, (Process_New)InterixProcess_new);
			i_proc = (InterixProcess *)proc;
			ruid = -1;
			euid = -1;
			long int utime = 0, stime = 0;
			while((line = String_readLine(f))) {
				char *tab = strchr(line, '	');
				if(tab) {
					char *v = tab + 1;
					*tab = 0;
					if(strcmp(line, "ppid") == 0) proc->ppid = atol(v);
					else if(strcmp(line, "pgid") == 0) proc->pgrp = atol(v);
					else if(strcmp(line, "ruser") == 0) ruid = atol(v);
					else if(strcmp(line, "user") == 0) euid = atol(v);
					else if(strcmp(line, "utime") == 0) utime = atol(v);
					else if(strcmp(line, "stime") == 0) stime = atol(v);
					else if(strcmp(line, "state") == 0) {
						int state = atoi(v);
						proc->state =
							state < sizeof state_name_map && state >= 0 ?
								state_name_map[state] : '?';
					} else if(strcmp(line, "vsize") == 0) {
						proc->m_size = atol(v) / page_size;
					} else if(strcmp(line, "sid") == 0) proc->session = atol(v);
					else if(strcmp(line, "inpsx") == 0) {
						i_proc->is_posix_process = atoi(v);
					} else if(strcmp(line, "natpid") == 0) {
						i_proc->native_pid = atol(v);
					} else if(strcmp(line, "natsid") == 0) {
						i_proc->native_sid = atol(v);
					} else if(strcmp(line, "nice") == 0) proc->nice = atol(v);
					else if(strcmp(line, "sttime") == 0) {
						proc->starttime_ctime = atol(v);
					}
				}
				free(line);
			}
			fclose(f);
			t = utime + stime;	// XXX: Need verify
			if(!proc->name || this->settings->updateProcessNames) {
				free(proc->name);
				proc->name = comm;
			}
			if(!proc->comm || this->settings->updateProcessNames) {
				free(proc->comm);
				strcpy(path + 6 + len, "/cmdline");
				f = fopen(path, "rb");
				if(f) {
					char buffer[1024];
					size_t len = fread(buffer, 1, sizeof buffer, f);
					if(len && *buffer) {
						if(len >= sizeof buffer) len = sizeof buffer - 1;
						if(buffer[len - 1]) buffer[len] = 0;
						else len--;
						proc->argv0_length = len;
						while(len > 0) {
							if(!buffer[--len]) {
								buffer[len] = ' ';
								proc->argv0_length = len;
							}
						}
						proc->comm = xStrdup(buffer);
					} else {
						proc->comm = xStrdup(comm);
						proc->argv0_length = comm_len - 1;
					}
				} else {
					proc->comm = xStrdup(comm);
					proc->argv0_length = comm_len - 1;
				}
			}
		}
		if(!is_existing || ruid != proc->ruid) {
			proc->ruid = ruid;
			proc->real_user = UsersTable_getRef(this->usersTable, proc->ruid);
		}
		if(!is_existing || euid != proc->euid) {
			proc->euid = euid;
			proc->effective_user = UsersTable_getRef(this->usersTable, proc->euid);
		}
		if(t > proc->time) {
			i_proc->time_delta = t - proc->time;
			total_time_delta += i_proc->time_delta;
		}
		proc->time = t;
		proc->show = !this->settings->hide_kernel_processes || !Process_isKernelProcess(proc);
		proc->updated = true;
		if(!is_existing) ProcessList_add(this, proc);
		this->totalTasks++;
		this->thread_count += proc->nlwp;
		if(Process_isKernelProcess(proc)) {
			this->kernel_process_count++;
			this->kernel_thread_count += proc->nlwp;
		}
		if(proc->state == 'R') {
			this->running_process_count++;
			this->running_thread_count++;
		}
	}
	closedir(dir);

	int i = ProcessList_size(this);
	if(!i) return;
	do {
		Process *proc = ProcessList_get(this, --i);
		InterixProcess *i_proc = (InterixProcess *)proc;
		proc->percent_cpu = (float)(i_proc->time_delta * 100) / total_time_delta;
	} while(i > 0);
}
