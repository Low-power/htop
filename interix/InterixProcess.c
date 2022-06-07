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
	HTOP_NATIVE_PID_FIELD = 100,
	HTOP_NATIVE_SID_FIELD = 101,
	HTOP_LAST_PROCESSFIELD = 102
} InterixProcessField;

typedef struct {
	Process super;
	pid_t native_pid;
	pid_t native_sid;
	bool is_posix_process;
	unsigned long long int time_msec;
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
   [HTOP_PID_FIELD] = { .name = "PID", .title = "    PID ", .description = "Process/thread ID", .flags = 0, },
   [HTOP_NAME_FIELD] = { .name = "NAME", .title = "NAME            ", .description = "Process (executable) name", .flags = 0, },
   [HTOP_COMM_FIELD] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [HTOP_STATE_FIELD] = { .name = "STATE", .title = "S ", .description = "Process state (U unconnected, R running, T stopped, W waiting, S waiting tty, Z zombie)", .flags = 0 },
   [HTOP_PPID_FIELD] = { .name = "PPID", .title = "   PPID ", .description = "Parent process ID", .flags = 0, },
   [HTOP_PGRP_FIELD] = { .name = "PGRP", .title = "   PGRP ", .description = "Process group ID", .flags = 0, },
   [HTOP_SESSION_FIELD] = { .name = "SESSION", .title = "    SID ", .description = "Process's session ID", .flags = 0, },
   [HTOP_TTY_FIELD] = { .name = "TTY", .title = "    TTY ", .description = "Controlling terminal", .flags = 0, },
   [HTOP_TPGID_FIELD] = { .name = "TPGID", .title = "  TPGID ", .description = "Process ID of the fg process group of the controlling terminal", .flags = 0, },
   [HTOP_MINFLT_FIELD] = { .name = "MINFLT", .title = "     MINFLT ", .description = "Number of minor faults which have not required loading a memory page from disk", .flags = 0, },
   [HTOP_MAJFLT_FIELD] = { .name = "MAJFLT", .title = "     MAJFLT ", .description = "Number of major faults which have required loading a memory page from disk", .flags = 0, },
   [HTOP_PRIORITY_FIELD] = { .name = "PRIORITY", .title = "PRI ", .description = "Kernel's internal priority for the process", .flags = 0, },
   [HTOP_NICE_FIELD] = { .name = "NICE", .title = " NI ", .description = "Nice value (the higher the value, the more it lets other processes take priority)", .flags = 0, },
   [HTOP_STARTTIME_FIELD] = { .name = "STARTTIME", .title = "START ", .description = "Time the process was started", .flags = 0, },
   [HTOP_PROCESSOR_FIELD] = { .name = "PROCESSOR", .title = "CPU ", .description = "Id of the CPU the process last executed on", .flags = 0, },
   [HTOP_M_SIZE_FIELD] = { .name = "M_SIZE", .title = " VIRT ", .description = "Total program size in virtual memory", .flags = 0, },
   [HTOP_M_RESIDENT_FIELD] = { .name = "M_RESIDENT", .title = "  RES ", .description = "Resident set size, size of the text and data sections, plus stack usage", .flags = 0, },
   [HTOP_REAL_UID_FIELD] = { .name = "REAL_UID", .title = "  RUID ", .description = "Real user ID", .flags = 0, },
   [HTOP_EFFECTIVE_UID_FIELD] = { .name = "EFFECTIVE_UID", .title = "  EUID ", .description = "Effective user ID", .flags = 0, },
   [HTOP_PERCENT_CPU_FIELD] = { .name = "PERCENT_CPU", .title = "CPU% ", .description = "Percentage of the CPU time the process used in the last sampling", .flags = 0, },
   [HTOP_PERCENT_MEM_FIELD] = { .name = "PERCENT_MEM", .title = "MEM% ", .description = "Percentage of the memory the process is using, based on resident memory size", .flags = 0, },
   [HTOP_REAL_USER_FIELD] = { .name = "REAL_USER", .title = "REAL_USER ", .description = "Real user (or numeric user ID if name cannot be determined)", .flags = 0, },
   [HTOP_EFFECTIVE_USER_FIELD] = { .name = "EFFECTIVE_USER", .title = "EFFE_USER ", .description = "Effective user (or numeric user ID if name cannot be determined)", .flags = 0, },
   [HTOP_TIME_FIELD] = { .name = "TIME", .title = "  TIME+  ", .description = "Total time the process has spent in user and system time", .flags = 0, },
   [HTOP_NLWP_FIELD] = { .name = "NLWP", .title = "NLWP ", .description = "Number of threads in the process", .flags = 0, },
   [HTOP_TGID_FIELD] = { .name = "TGID", .title = "   TGID ", .description = "Thread group ID (i.e. process ID)", .flags = 0, },
   [HTOP_NATIVE_PID_FIELD] = { .name = "NATIVE_PID", .title = " NATPID ", .description = "Windows NT process ID", .flags = 0, },
   [HTOP_NATIVE_SID_FIELD] = { .name = "NATIVE_SID", .title = " NATSID ", .description = "Windows NT session ID", .flags = 0, },
   [HTOP_LAST_PROCESSFIELD] = { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0, },
};

ProcessPidColumn Process_pidColumns[] = {
   { .id = HTOP_PID_FIELD, .label = "PID" },
   { .id = HTOP_PPID_FIELD, .label = "PPID" },
   { .id = HTOP_TPGID_FIELD, .label = "TPGID" },
   { .id = HTOP_TGID_FIELD, .label = "TGID" },
   { .id = HTOP_PGRP_FIELD, .label = "PGRP" },
   { .id = HTOP_SESSION_FIELD, .label = "SID" },
   { .id = HTOP_NATIVE_PID_FIELD, .label = "NATPID" },
   { .id = HTOP_NATIVE_SID_FIELD, .label = "NATSID" },
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
		case HTOP_NATIVE_PID_FIELD:
			return p1->native_pid - p2->native_pid;
		case HTOP_NATIVE_SID_FIELD:
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
		case HTOP_NATIVE_PID_FIELD:
			len = snprintf(buffer, sizeof buffer, Process_pidFormat, this->native_pid);
			break;
		case HTOP_NATIVE_SID_FIELD:
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
	RichString_append(str, CRT_colors[HTOP_DEFAULT_COLOR], buffer);
}

bool Process_isKernelProcess(const Process *this) {
	const InterixProcess *p = (const InterixProcess *)this;
	return p->native_pid == 0 || p->native_pid == 4 || p->native_pid == 8;
}

bool Process_isExtraThreadProcess(const Process *this) {
	return false;
}

char **Process_getKernelStackTrace(const Process *this) {
	return NULL;
}
