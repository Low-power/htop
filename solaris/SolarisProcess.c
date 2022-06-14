/*
htop - solaris/SolarisProcess.c
(C) 2015 Hisham H. Muhammad
(C) 2017,2018 Guy M. Broome
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "ProcessList.h"
#include "SolarisProcess.h"
#include "Platform.h"
#include "CRT.h"
#include "XAlloc.h"
#include <sys/types.h>
#include <signal.h>
#include <procfs.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*{
#include "config.h"
#include "Settings.h"
#include <sys/proc.h>
#ifdef HAVE_ZONE_H
#include <zone.h>
#endif

typedef enum {
   // Add platform-specific fields here, with ids >= 100
#ifdef HAVE_ZONE_H
   HTOP_ZONEID_FIELD = 100,
   HTOP_ZONE_FIELD = 101,
#endif
   HTOP_PROJID_FIELD = 102,
   HTOP_TASKID_FIELD = 103,
#ifdef HAVE_PSINFO_T_PR_POOLID
   HTOP_POOLID_FIELD = 104,
#endif
#ifdef HAVE_PSINFO_T_PR_CONTRACT
   HTOP_CONTID_FIELD = 105,
#endif
#ifdef HAVE_LIBPROC
   HTOP_LWPID_FIELD = 106,
#endif
   HTOP_LAST_PROCESSFIELD = 107,
} SolarisProcessField;

typedef struct SolarisProcess_ {
   Process    super;
   bool       kernel;
#ifdef HAVE_ZONE_H
   zoneid_t   zoneid;
#endif
   char*      zname;
   taskid_t   taskid;
   projid_t   projid;
#ifdef HAVE_PSINFO_T_PR_POOLID
   poolid_t   poolid;
#endif
#ifdef HAVE_PSINFO_T_PR_CONTRACT
   ctid_t     contid;
#endif
   bool       is_lwp;
   pid_t      realpid;
   pid_t      realppid;
   pid_t      lwpid;
   off_t      argv_offset;
   off_t      envv_offset;
   char       data_model;
} SolarisProcess;

}*/

ProcessClass SolarisProcess_class = {
   .super = {
      .extends = Class(Process),
      .display = Process_display,
      .delete = Process_delete,
      .compare = SolarisProcess_compare
   },
   .writeField = SolarisProcess_writeField,
   .sendSignal = SolarisProcess_sendSignal,
#ifdef HAVE_LIBPROC
   .isSelf = SolarisProcess_isSelf,
#endif
};

FieldData Process_fields[] = {
   [0] = { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_PID_FIELD] = { .name = "PID", .title = "    PID    ", .description = "Process/thread ID", .flags = 0, },
   [HTOP_NAME_FIELD] = { .name = "NAME", .title = "NAME            ", .description = "Process (executable) name", .flags = 0, },
   [HTOP_COMM_FIELD] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [HTOP_STATE_FIELD] = { .name = "STATE", .title = "S ", .description = "Process state (O running, R runnable, S sleeping, T stopped, Z zombie, W waiting CPU-caps)", .flags = 0 },
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
#ifdef HAVE_ZONE_H
   [HTOP_ZONEID_FIELD] = { .name = "ZONEID", .title = " ZONEID ", .description = "Zone ID", .flags = 0, },
   [HTOP_ZONE_FIELD] = { .name = "ZONE", .title = "ZONE             ", .description = "Zone name", .flags = 0, },
#endif
   [HTOP_PROJID_FIELD] = { .name = "PROJID", .title = " PRJID ", .description = "Project ID", .flags = 0, },
   [HTOP_TASKID_FIELD] = { .name = "TASKID", .title = " TSKID ", .description = "Task ID", .flags = 0, },
#ifdef HAVE_PSINFO_T_PR_POOLID
   [HTOP_POOLID_FIELD] = { .name = "POOLID", .title = " POLID ", .description = "Pool ID", .flags = 0, },
#endif
#ifdef HAVE_PSINFO_T_PR_CONTRACT
   [HTOP_CONTID_FIELD] = { .name = "CONTID", .title = " CNTID ", .description = "Contract ID", .flags = 0, },
#endif
#ifdef HAVE_LIBPROC
   [HTOP_LWPID_FIELD] = { .name = "LWPID", .title = " LWPID ", .description = "LWP ID", .flags = 0, },
#endif
   [HTOP_LAST_PROCESSFIELD] = { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0, }
};

ProcessPidColumn Process_pidColumns[] = {
#ifdef HAVE_ZONE_H
   { .id = HTOP_ZONEID_FIELD, .label = "ZONEID" },
#endif
   { .id = HTOP_TASKID_FIELD, .label = "TSKID" },
   { .id = HTOP_PROJID_FIELD, .label = "PRJID" },
#ifdef HAVE_PSINFO_T_PR_POOLID
   { .id = HTOP_POOLID_FIELD, .label = "POLID" },
#endif
#ifdef HAVE_PSINFO_T_PR_CONTRACT
   { .id = HTOP_CONTID_FIELD, .label = "CNTID" },
#endif
   { .id = HTOP_PID_FIELD, .label = "PID" },
   { .id = HTOP_PPID_FIELD, .label = "PPID" },
#ifdef HAVE_LIBPROC
   { .id = HTOP_LWPID_FIELD, .label = "LWPID" },
#endif
   { .id = HTOP_TPGID_FIELD, .label = "TPGID" },
   { .id = HTOP_TGID_FIELD, .label = "TGID" },
   { .id = HTOP_PGRP_FIELD, .label = "PGRP" },
   { .id = HTOP_SESSION_FIELD, .label = "SID" },
   { .id = 0, .label = NULL }
};

SolarisProcess* SolarisProcess_new(Settings* settings) {
   SolarisProcess* this = xCalloc(1, sizeof(SolarisProcess));
   Object_setClass(this, Class(SolarisProcess));
   Process_init(&this->super, settings);
   return this;
}

void Process_delete(Object* cast) {
   SolarisProcess* sp = (SolarisProcess*) cast;
   Process_done((Process*)cast);
   free(sp->zname);
   free(sp);
}

void SolarisProcess_writeField(const Process *this, RichString* str, ProcessField field) {
   const SolarisProcess *sp = (const SolarisProcess *)this;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[HTOP_DEFAULT_COLOR];
   int n = sizeof(buffer);
   switch ((int) field) {
      // add Solaris-specific fields here
#ifdef HAVE_ZONE_H
      case HTOP_ZONEID_FIELD:
         xSnprintf(buffer, n, Process_pidFormat, sp->zoneid);
         break;
#endif
      case HTOP_PROJID_FIELD:
         xSnprintf(buffer, n, Process_pidFormat, sp->projid);
         break;
      case HTOP_TASKID_FIELD:
         xSnprintf(buffer, n, Process_pidFormat, sp->taskid);
         break;
#ifdef HAVE_PSINFO_T_PR_POOLID
      case HTOP_POOLID_FIELD:
         xSnprintf(buffer, n, Process_pidFormat, sp->poolid);
         break;
#endif
#ifdef HAVE_PSINFO_T_PR_CONTRACT
      case HTOP_CONTID_FIELD:
         xSnprintf(buffer, n, Process_pidFormat, sp->contid);
         break;
#endif
#ifdef HAVE_ZONE_H
      case HTOP_ZONE_FIELD:
         xSnprintf(buffer, n, "%-*s ", ZONENAME_MAX/4, sp->zname);
         if (buffer[ZONENAME_MAX/4 + 1]) {
            buffer[ZONENAME_MAX/4] = ' ';
            buffer[ZONENAME_MAX/4 + 1] = 0;
         }
         break;
#endif
      case HTOP_PID_FIELD:
         if(Process_isSelf(this)) attr = CRT_colors[HTOP_PROCESS_BASENAME_COLOR];
         xSnprintf(buffer, n, Process_pidFormat, sp->realpid);
         break;
      case HTOP_PPID_FIELD:
         xSnprintf(buffer, n, Process_pidFormat, sp->realppid);
         break;
#ifdef HAVE_LIBPROC
      case HTOP_LWPID_FIELD:
         xSnprintf(buffer, n, Process_pidFormat, sp->lwpid);
         break;
#endif
      default:
         Process_writeField(this, str, field);
         return;
   }
   RichString_append(str, attr, buffer);
}

long SolarisProcess_compare(const void* v1, const void* v2) {
   const SolarisProcess *p1, *p2;
   const Settings* settings = ((const Process *)v1)->settings;
   if (settings->direction == 1) {
      p1 = v1;
      p2 = v2;
   } else {
      p2 = v1;
      p1 = v2;
   }
   switch ((int) settings->sortKey) {
#ifdef HAVE_ZONE_H
      case HTOP_ZONEID_FIELD:
         return (p1->zoneid - p2->zoneid);
#endif
      case HTOP_PROJID_FIELD:
         return (p1->projid - p2->projid);
      case HTOP_TASKID_FIELD:
         return (p1->taskid - p2->taskid);
#ifdef HAVE_PSINFO_T_PR_POOLID
      case HTOP_POOLID_FIELD:
         return (p1->poolid - p2->poolid);
#endif
#ifdef HAVE_PSINFO_T_PR_CONTRACT
      case HTOP_CONTID_FIELD:
         return (p1->contid - p2->contid);
#endif
#ifdef HAVE_ZONE_H
      case HTOP_ZONE_FIELD:
         if(!p1->zname && !p2->zname) return p1->zoneid - p2->zoneid;
         return settings->sort_strcmp(p1->zname ? p1->zname : "global", p2->zname ? p2->zname : "global");
#endif
      case HTOP_PID_FIELD:
         return (p1->realpid - p2->realpid);
      case HTOP_PPID_FIELD:
         return (p1->realppid - p2->realppid);
#ifdef HAVE_LIBPROC
      case HTOP_LWPID_FIELD:
         return (p1->lwpid - p2->lwpid);
#endif
      default:
         return Process_compare(v1, v2);
   }
}

bool SolarisProcess_sendSignal(const Process *this, int sig) {
#ifdef HAVE_LIBPROC
	pid_t pid = ((const SolarisProcess *)this)->realpid;
#else
	pid_t pid = this->pid;
#endif
	if(kill(pid, sig) < 0) {
		if(errno != EPERM) return false;
		char path[22];
		xSnprintf(path, sizeof path, "/proc/%d/ctl", (int)pid);
		int fd = open(path, O_WRONLY);
		if(fd == -1) return false;
		long int msg[] = { PCKILL, sig };
		bool r = write(fd, msg, sizeof msg) == sizeof msg;
		close(fd);
		return r;
	}
	return true;
}

#ifdef HAVE_LIBPROC
bool SolarisProcess_isSelf(const Process *this) {
	static pid_t mypid = -1;
	if(mypid == -1) mypid = getpid();
	return ((const SolarisProcess *)this)->realpid == mypid;
}
#endif

bool Process_isKernelProcess(const Process *this) {
	return ((const SolarisProcess *)this)->kernel;
}

bool Process_isExtraThreadProcess(const Process* this) {
   const SolarisProcess *fp = (const SolarisProcess *)this;
   return fp->is_lwp;
}

char **Process_getKernelStackTrace(const Process *this) {
	return NULL;
}
