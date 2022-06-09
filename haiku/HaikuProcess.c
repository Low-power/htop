/*
htop - haiku/HaikuProcess.c
(C) 2015 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "HaikuProcess.h"
#include "CRT.h"
#include <string.h>
#include <stdlib.h>

/*{
#include "Settings.h"
#include <SupportDefs.h>

#define Process_delete HaikuProcess_delete

typedef enum {
	HTOP_LAST_PROCESSFIELD = 100
} HaikuProcessField;

typedef struct {
	Process super;
	bigtime_t time_usec;
} HaikuProcess;
}*/

ProcessClass HaikuProcess_class = {
   .super = {
      .extends = Class(Process),
      .display = Process_display,
      .delete = Process_delete,
      .compare = HaikuProcess_compare
   },
   .writeField = HaikuProcess_writeField,
};

ProcessFieldData Process_fields[] = {
   [0] = { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_PID_FIELD] = { .name = "PID", .title = "    PID ", .description = "Process/thread ID", .flags = 0, },
   [HTOP_NAME_FIELD] = { .name = "NAME", .title = "NAME            ", .description = "Process (executable) name", .flags = 0, },
   [HTOP_COMM_FIELD] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [HTOP_STATE_FIELD] = { .name = "STATE", .title = "S ", .description = "Process state (O running, R ready, T stopped, S sleeping, D uninterruptible sleeping)", .flags = 0 },
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
   [HTOP_LAST_PROCESSFIELD] = { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0, },
};

ProcessPidColumn Process_pidColumns[] = {
   { .id = HTOP_PID_FIELD, .label = "PID" },
   { .id = HTOP_PPID_FIELD, .label = "PPID" },
   { .id = HTOP_TPGID_FIELD, .label = "TPGID" },
   { .id = HTOP_TGID_FIELD, .label = "TGID" },
   { .id = HTOP_PGRP_FIELD, .label = "PGRP" },
   { .id = HTOP_SESSION_FIELD, .label = "SID" },
   { .id = 0, .label = NULL },
};

HaikuProcess *HaikuProcess_new(Settings* settings) {
   HaikuProcess *this = xCalloc(1, sizeof(HaikuProcess));
   Object_setClass(this, Class(HaikuProcess));
   Process_init(&this->super, settings);
   return this;
}

void HaikuProcess_delete(Object* cast) {
   HaikuProcess *this = (HaikuProcess *)cast;
   Object_setClass(this, Class(HaikuProcess));
   Process_done((Process *)cast);
   free(this);
}

long int HaikuProcess_compare(const void *o1, const void *o2) {
	const HaikuProcess *p1, *p2;
	const Settings *settings = ((const Process *)o1)->settings;
	if(settings->direction == 1) {
		p1 = o1;
		p2 = o2;
	} else {
		p1 = o2;
		p2 = o1;
	}
	switch((int)settings->sortKey) {
		default:
			return Process_compare(o1, o2);
	}
}

void HaikuProcess_writeField(const Process *super, RichString *str, ProcessField field) {
	const HaikuProcess *this = (const HaikuProcess *)super;
	char buffer[256];
	int len;
	switch((int)field) {
		default:
			Process_writeField(super, str, field);
			return;
	}
	RichString_append(str, CRT_colors[HTOP_DEFAULT_COLOR], buffer);
}

bool Process_isKernelProcess(const Process *this) {
	return this->tgid == 1;
}

bool Process_isExtraThreadProcess(const Process *this) {
	return this->pid != this->tgid;
}

char **Process_getKernelStackTrace(const Process *this) {
	return NULL;
}
