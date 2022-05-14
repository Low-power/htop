/*
htop - interix/InterixProcess.c
(C) 2015 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "InterixProcess.h"
#include "CRT.h"
#include <string.h>
#include <stdlib.h>

/*{
#include "Settings.h"

#define Process_delete InterixProcess_delete

typedef enum {
	NATIVE_PID = 100,
	NATIVE_SID = 101,
	LAST_PROCESSFIELD = 102
} InterixProcessField;

typedef struct {
	Process super;
	pid_t native_pid;
	pid_t native_sid;
	bool is_posix_process;
	unsigned int time_delta;
} InterixProcess;
}*/

ProcessClass InterixProcess_class = {
   .super = {
      .extends = Class(Process),
      .display = Process_display,
      .delete = Process_delete,
      .compare = InterixProcess_compare
   },
   .writeField = InterixProcess_writeField,
};

ProcessFieldData Process_fields[] = {
   [0] = { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   [PID] = { .name = "PID", .title = "    PID ", .description = "Process/thread ID", .flags = 0, },
   [NAME] = { .name = "NAME", .title = "NAME            ", .description = "Process (executable) name", .flags = 0, },
   [COMM] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [STATE] = { .name = "STATE", .title = "S ", .description = "Process state (S sleeping, R running, D disk, Z zombie, T stoppd, W paging)", .flags = 0, },
   [PPID] = { .name = "PPID", .title = "   PPID ", .description = "Parent process ID", .flags = 0, },
   [PGRP] = { .name = "PGRP", .title = "   PGRP ", .description = "Process group ID", .flags = 0, },
   [SESSION] = { .name = "SESSION", .title = "    SID ", .description = "Process's session ID", .flags = 0, },
   [TTY_NR] = { .name = "TTY_NR", .title = "    TTY ", .description = "Controlling terminal", .flags = 0, },
   [TPGID] = { .name = "TPGID", .title = "  TPGID ", .description = "Process ID of the fg process group of the controlling terminal", .flags = 0, },
   [MINFLT] = { .name = "MINFLT", .title = "     MINFLT ", .description = "Number of minor faults which have not required loading a memory page from disk", .flags = 0, },
   [MAJFLT] = { .name = "MAJFLT", .title = "     MAJFLT ", .description = "Number of major faults which have required loading a memory page from disk", .flags = 0, },
   [PRIORITY] = { .name = "PRIORITY", .title = "PRI ", .description = "Kernel's internal priority for the process", .flags = 0, },
   [NICE] = { .name = "NICE", .title = " NI ", .description = "Nice value (the higher the value, the more it lets other processes take priority)", .flags = 0, },
   [STARTTIME] = { .name = "STARTTIME", .title = "START ", .description = "Time the process was started", .flags = 0, },
   [PROCESSOR] = { .name = "PROCESSOR", .title = "CPU ", .description = "Id of the CPU the process last executed on", .flags = 0, },
   [M_SIZE] = { .name = "M_SIZE", .title = " VIRT ", .description = "Total program size in virtual memory", .flags = 0, },
   [M_RESIDENT] = { .name = "M_RESIDENT", .title = "  RES ", .description = "Resident set size, size of the text and data sections, plus stack usage", .flags = 0, },
   [REAL_UID] = { .name = "REAL_UID", .title = "  RUID ", .description = "Real user ID", .flags = 0, },
   [EFFECTIVE_UID] = { .name = "EFFECTIVE_UID", .title = "  EUID ", .description = "Effective user ID", .flags = 0, },
   [PERCENT_CPU] = { .name = "PERCENT_CPU", .title = "CPU% ", .description = "Percentage of the CPU time the process used in the last sampling", .flags = 0, },
   [PERCENT_MEM] = { .name = "PERCENT_MEM", .title = "MEM% ", .description = "Percentage of the memory the process is using, based on resident memory size", .flags = 0, },
   [REAL_USER] = { .name = "REAL_USER", .title = "REAL_USER ", .description = "Real user (or numeric user ID if name cannot be determined)", .flags = 0, },
   [EFFECTIVE_USER] = { .name = "EFFECTIVE_USER", .title = "EFFE_USER ", .description = "Effective user (or numeric user ID if name cannot be determined)", .flags = 0, },
   [TIME] = { .name = "TIME", .title = "  TIME+  ", .description = "Total time the process has spent in user and system time", .flags = 0, },
   [NLWP] = { .name = "NLWP", .title = "NLWP ", .description = "Number of threads in the process", .flags = 0, },
   [TGID] = { .name = "TGID", .title = "   TGID ", .description = "Thread group ID (i.e. process ID)", .flags = 0, },
   [NATIVE_PID] = { .name = "NATIVE_PID", .title = " NATPID ", .description = "Windows NT process ID", .flags = 0, },
   [NATIVE_SID] = { .name = "NATIVE_SID", .title = " NATSID ", .description = "Windows NT session ID", .flags = 0, },
   [LAST_PROCESSFIELD] = { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0, },
};

ProcessPidColumn Process_pidColumns[] = {
   { .id = PID, .label = "PID" },
   { .id = PPID, .label = "PPID" },
   { .id = TPGID, .label = "TPGID" },
   { .id = TGID, .label = "TGID" },
   { .id = PGRP, .label = "PGRP" },
   { .id = SESSION, .label = "SID" },
   { .id = NATIVE_PID, .label = "NATPID" },
   { .id = NATIVE_SID, .label = "NATSID" },
   { .id = 0, .label = NULL },
};

InterixProcess *InterixProcess_new(Settings* settings) {
   InterixProcess *this = xCalloc(1, sizeof(InterixProcess));
   Object_setClass(this, Class(InterixProcess));
   Process_init(&this->super, settings);
   return this;
}

void InterixProcess_delete(Object* cast) {
   InterixProcess *this = (InterixProcess *)cast;
   Object_setClass(this, Class(InterixProcess));
   Process_done((Process*)cast);
   free(this);
}

long int InterixProcess_compare(const void *o1, const void *o2) {
	const InterixProcess *p1, *p2;
	const Settings *settings = ((const Process *)o1)->settings;
	if(settings->direction == 1) {
		p1 = o1;
		p2 = o2;
	} else {
		p1 = o2;
		p2 = o1;
	}
	switch((int)settings->sortKey) {
		case NATIVE_PID:
			return p1->native_pid - p2->native_pid;
		case NATIVE_SID:
			return p1->native_sid - p2->native_sid;
		default:
			return Process_compare(o1, o2);
	}
}

void InterixProcess_writeField(Process *super, RichString *str, ProcessField field) {
	InterixProcess *this = (InterixProcess *)super;
	char buffer[256];
	int len;
	switch((int)field) {
		case NATIVE_PID:
			len = snprintf(buffer, sizeof buffer, Process_pidFormat, this->native_pid);
			break;
		case NATIVE_SID:
			len = snprintf(buffer, sizeof buffer, Process_pidFormat, this->native_sid);
			break;
		default:
			Process_writeField(super, str, field);
			return;
	}
	if(len < 7) {
#if 1
		int space_len = 7 - len;
		memmove(buffer + space_len, buffer, len + 1);
		memset(buffer, ' ', space_len);
#else
		memset(buffer + len, ' ', 7 - len);
		buffer[7] = 0;
#endif
	}
	RichString_append(str, CRT_colors[DEFAULT_COLOR], buffer);
}

bool Process_isKernelProcess(const Process *this) {
	const InterixProcess *p = (const InterixProcess *)this;
	return p->native_pid == 0 || p->native_pid == 4 || p->native_pid == 8;
}

bool Process_isExtraThreadProcess(const Process *this) {
	return false;
}
