/*
htop - aix/AixProcess.c
(C) 2015 Hisham H. Muhammad
(C) 2018 Calvin Buckley
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "AixProcess.h"
#include "CRT.h"
#include "XAlloc.h"
#include <sys/types.h>
#include <sys/procfs.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

/*{
#include "Settings.h"
#include <sys/corralid.h> // cid_t 

#define Process_delete AixProcess_delete

typedef enum {
   // Add platform-specific fields here, with ids >= 100
   HTOP_WPAR_ID_FIELD = 100,
   HTOP_LAST_PROCESSFIELD = 101,
} AixProcessField;

typedef struct AixProcess_ {
   Process super;
   bool kernel;
   cid_t cid; // WPAR ID
   struct timeval tv; // More precise time for calculating percent_cpu
} AixProcess;

}*/

ProcessClass AixProcess_class = {
   .super = {
      .extends = Class(Process),
      .display = Process_display,
      .delete = Process_delete,
      .compare = AixProcess_compare
   },
   .writeField = AixProcess_writeField,
   .sendSignal = AixProcess_sendSignal
};

FieldData Process_fields[] = {
   [0] = {
      .name = "",
      .title = NULL,
      .description = NULL,
      .flags = 0, },
   [HTOP_PID_FIELD] = {
      .name = "PID",
      .title = "    PID ",
      .description = "Process/thread ID",
      .flags = 0, },
   [HTOP_NAME_FIELD] = { .name = "NAME", .title = "NAME            ", .description = "Process name", .flags = 0, },
   [HTOP_COMM_FIELD] = {
      .name = "Command",
      .title = "Command ",
      .description = "Command line",
      .flags = 0, },
   [HTOP_STATE_FIELD] = { .name = "STATE", .title = "S ", .description = "Process state (I idle, T stoppd, Z zombie, R running, W paging)", .flags = 0 },
   [HTOP_PPID_FIELD] = {
      .name = "PPID",
      .title = "   PPID ",
      .description = "Parent process ID",
      .flags = 0, },
   [HTOP_PGRP_FIELD] = {
      .name = "PGRP",
      .title = "   PGRP ",
      .description = "Process group ID",
      .flags = 0, },
   [HTOP_SESSION_FIELD] = {
      .name = "SESSION",
      .title = "   SESN ",
      .description = "Process's session ID",
      .flags = 0, },
   [HTOP_TTY_FIELD] = { .name = "TTY", .title = "    TTY ", .description = "Controlling terminal", .flags = 0, },
   [HTOP_TPGID_FIELD] = {
      .name = "TPGID",
      .title = "  TPGID ",
      .description = "Process ID of the fg process group of the controlling terminal",
      .flags = 0, },
   [HTOP_MINFLT_FIELD] = {
      .name = "MINFLT",
      .title = "     MINFLT ",
      .description = "Number of minor faults which have not required loading a memory page from disk",
      .flags = 0, },
   [HTOP_MAJFLT_FIELD] = {
      .name = "MAJFLT",
      .title = "     MAJFLT ",
      .description = "Number of major faults which have required loading a memory page from disk",
      .flags = 0, },
   [HTOP_PRIORITY_FIELD] = {
      .name = "PRIORITY",
      .title = "PRI ",
      .description = "Kernel's internal priority for the process",
      .flags = 0, },
   [HTOP_NICE_FIELD] = {
      .name = "NICE",
      .title = " NI ",
      .description = "Nice value (the higher the value, the more it lets other processes take priority)",
      .flags = 0, },
   [HTOP_STARTTIME_FIELD] = {
      .name = "STARTTIME",
      .title = "START ",
      .description = "Time the process was started",
      .flags = 0, },
   [HTOP_PROCESSOR_FIELD] = {
      .name = "PROCESSOR",
      .title = "CPU ",
      .description = "Id of the CPU the process last executed on",
      .flags = 0, },
   [HTOP_M_SIZE_FIELD] = {
      .name = "M_SIZE",
      .title = " VIRT ",
      .description = "Total program size in virtual memory",
      .flags = 0, },
   [HTOP_M_RESIDENT_FIELD] = {
      .name = "M_RESIDENT",
      .title = "  RES ",
      .description = "Resident set size, size of the text and data sections, plus stack usage",
      .flags = 0, },
   [HTOP_REAL_UID_FIELD] = { .name = "REAL_UID", .title = "  RUID ", .description = "Real user ID", .flags = 0, },
   [HTOP_EFFECTIVE_UID_FIELD] = { .name = "EFFECTIVE_UID", .title = "  EUID ", .description = "Effective user ID", .flags = 0, },
   [HTOP_PERCENT_CPU_FIELD] = {
      .name = "PERCENT_CPU",
      .title = "CPU% ",
      .description = "Percentage of the CPU time the process used in the last sampling",
      .flags = 0, },
   [HTOP_PERCENT_MEM_FIELD] = {
      .name = "PERCENT_MEM",
      .title = "MEM% ",
      .description = "Percentage of the memory the process is using, based on resident memory size",
      .flags = 0, },
   [HTOP_REAL_USER_FIELD] = { .name = "REAL_USER", .title = "REAL_USER ", .description = "Real user (or numeric user ID if name cannot be determined)", .flags = 0, },
   [HTOP_EFFECTIVE_USER_FIELD] = { .name = "EFFECTIVE_USER", .title = "EFFE_USER ", .description = "Effective user (or numeric user ID if name cannot be determined)", .flags = 0, },
   [HTOP_TIME_FIELD] = {
      .name = "TIME",
      .title = "  TIME+  ",
      .description = "Total time the process has spent in user and system time",
      .flags = 0, },
   [HTOP_NLWP_FIELD] = {
      .name = "NLWP",
      .title = "NLWP ",
      .description = "Number of threads in the process",
      .flags = 0, },
   [HTOP_TGID_FIELD] = {
      .name = "TGID",
      .title = "   TGID ",
      .description = "Thread group ID (i.e. process ID)",
      .flags = 0, },
   [HTOP_WPAR_ID_FIELD] = {
      .name = "WPAR",
      .title = "   WPAR ",
      .description = "Workload Partition ID",
      .flags = 0, },
   [HTOP_LAST_PROCESSFIELD] = {
      .name = "*** report bug! ***",
      .title = NULL,
      .description = NULL,
      .flags = 0, },
};

ProcessPidColumn Process_pidColumns[] = {
   { .id = HTOP_PID_FIELD, .label = "PID" },
   { .id = HTOP_PPID_FIELD, .label = "PPID" },
   { .id = HTOP_TPGID_FIELD, .label = "TPGID" },
   { .id = HTOP_TGID_FIELD, .label = "TGID" },
   { .id = HTOP_PGRP_FIELD, .label = "PGRP" },
   { .id = HTOP_SESSION_FIELD, .label = "SESN" },
   { .id = 0, .label = NULL },
};

AixProcess* AixProcess_new(Settings* settings) {
   AixProcess* this = xCalloc(1, sizeof(AixProcess));
   Object_setClass(this, Class(AixProcess));
   Process_init(&this->super, settings);
   return this;
}

void AixProcess_delete(Object* cast) {
   AixProcess* this = (AixProcess*) cast;
   Process_done((Process*)cast);
   // free platform-specific fields here
   free(this);
}

void AixProcess_writeField(const Process *super, RichString* str, ProcessField field) {
   const AixProcess *this = (const AixProcess *)super;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[HTOP_DEFAULT_COLOR];
   int n = sizeof buffer;
   switch ((int) field) {
      // add AIX-specific fields here
      case HTOP_WPAR_ID_FIELD:
         xSnprintf(buffer, n, Process_pidFormat, this->cid);
         break;
      default:
         Process_writeField(super, str, field);
         return;
   }
   RichString_append(str, attr, buffer);
}

long AixProcess_compare(const void* v1, const void* v2) {
   const AixProcess *p1, *p2;
   const Settings *settings = ((const Process *)v1)->settings;
   if (settings->direction == 1) {
      p1 = v1;
      p2 = v2;
   } else {
      p2 = v1;
      p1 = v2;
   }
   switch ((int) settings->sortKey) {
      // add Aix-specific fields here
      case HTOP_WPAR_ID_FIELD:
         return uintcmp(p1->cid, p2->cid);
      default:
         return Process_compare(v1, v2);
   }
}

bool AixProcess_sendSignal(const Process *this, int sig) {
	if(kill(this->pid, sig) < 0) {
		if(errno != EINVAL) return false;
		char path[22];
		xSnprintf(path, sizeof path, "/proc/%d/ctl", (int)this->pid);
		int fd = open(path, O_WRONLY);
		if(fd == -1) return false;
		int msg[] = { PCKILL, sig };
		bool r = write(fd, msg, sizeof msg) == sizeof msg;
		close(fd);
		return r;
	}
	return true;
}

bool Process_isKernelProcess(const Process *this) {
	return ((const AixProcess *)this)->kernel;
}

bool Process_isExtraThreadProcess(const Process *this) {
	return false;
}

char **Process_getKernelStackTrace(const Process *this) {
	return NULL;
}
