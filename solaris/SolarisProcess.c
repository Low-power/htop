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

#include <stdlib.h>
#include <string.h>

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
   ZONEID   = 100,
   ZONE  = 101,
#endif
   PROJID = 102,
   TASKID = 103,
#ifdef HAVE_PSINFO_T_PR_POOLID
   POOLID = 104,
#endif
#ifdef HAVE_PSINFO_T_PR_CONTRACT
   CONTID = 105,
#endif
   LWPID = 106,
   LAST_PROCESSFIELD = 107,
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
};

ProcessFieldData Process_fields[] = {
   [0] = { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   [PID] = { .name = "PID", .title = "    PID    ", .description = "Process/thread ID", .flags = 0, },
   [NAME] = { .name = "NAME", .title = "NAME            ", .description = "Process (executable) name", .flags = 0, },
   [COMM] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [STATE] = { .name = "STATE", .title = "S ", .description = "Process state (S sleeping, R running, O onproc, Z zombie, T stopped, W waiting)", .flags = 0, },
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
#ifdef HAVE_ZONE_H
   [ZONEID] = { .name = "ZONEID", .title = " ZONEID ", .description = "Zone ID", .flags = 0, },
   [ZONE] = { .name = "ZONE", .title = "ZONE             ", .description = "Zone name", .flags = 0, },
#endif
   [PROJID] = { .name = "PROJID", .title = " PRJID ", .description = "Project ID", .flags = 0, },
   [TASKID] = { .name = "TASKID", .title = " TSKID ", .description = "Task ID", .flags = 0, },
#ifdef HAVE_PSINFO_T_PR_POOLID
   [POOLID] = { .name = "POOLID", .title = " POLID ", .description = "Pool ID", .flags = 0, },
#endif
#ifdef HAVE_PSINFO_T_PR_CONTRACT
   [CONTID] = { .name = "CONTID", .title = " CNTID ", .description = "Contract ID", .flags = 0, },
#endif
#ifdef HAVE_LIBPROC
   [LWPID] = { .name = "LWPID", .title = " LWPID ", .description = "LWP ID", .flags = 0, },
#endif
   [LAST_PROCESSFIELD] = { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0, }
};

ProcessPidColumn Process_pidColumns[] = {
#ifdef HAVE_ZONE_H
   { .id = ZONEID, .label = "ZONEID" },
#endif
   { .id = TASKID, .label = "TSKID" },
   { .id = PROJID, .label = "PRJID" },
#ifdef HAVE_PSINFO_T_PR_POOLID
   { .id = POOLID, .label = "POLID" },
#endif
#ifdef HAVE_PSINFO_T_PR_CONTRACT
   { .id = CONTID, .label = "CNTID" },
#endif
   { .id = PID, .label = "PID" },
   { .id = PPID, .label = "PPID" },
#ifdef HAVE_LIBPROC
   { .id = LWPID, .label = "LWPID" },
#endif
   { .id = TPGID, .label = "TPGID" },
   { .id = TGID, .label = "TGID" },
   { .id = PGRP, .label = "PGRP" },
   { .id = SESSION, .label = "SID" },
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

void SolarisProcess_writeField(Process* this, RichString* str, ProcessField field) {
   SolarisProcess* sp = (SolarisProcess*) this;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[HTOP_DEFAULT_COLOR];
   int n = sizeof(buffer);
   switch ((int) field) {
   // add Solaris-specific fields here
#ifdef HAVE_ZONE_H
   case ZONEID:
      xSnprintf(buffer, n, Process_pidFormat, sp->zoneid);
      break;
#endif
   case PROJID:
      xSnprintf(buffer, n, Process_pidFormat, sp->projid);
      break;
   case TASKID:
      xSnprintf(buffer, n, Process_pidFormat, sp->taskid);
      break;
#ifdef HAVE_PSINFO_T_PR_POOLID
   case POOLID:
      xSnprintf(buffer, n, Process_pidFormat, sp->poolid);
      break;
#endif
#ifdef HAVE_PSINFO_T_PR_CONTRACT
   case CONTID:
      xSnprintf(buffer, n, Process_pidFormat, sp->contid);
      break;
#endif
#ifdef HAVE_ZONE_H
   case ZONE:
      xSnprintf(buffer, n, "%-*s ", ZONENAME_MAX/4, sp->zname);
      if (buffer[ZONENAME_MAX/4 + 1]) {
         buffer[ZONENAME_MAX/4] = ' ';
         buffer[ZONENAME_MAX/4 + 1] = 0;
      }
      break;
#endif
   case PID:
      xSnprintf(buffer, n, Process_pidFormat, sp->realpid);
      break;
   case PPID:
      xSnprintf(buffer, n, Process_pidFormat, sp->realppid);
      break;
#ifdef HAVE_LIBPROC
   case LWPID:
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
   case ZONEID:
      return (p1->zoneid - p2->zoneid);
#endif
   case PROJID:
      return (p1->projid - p2->projid);
   case TASKID:
      return (p1->taskid - p2->taskid);
#ifdef HAVE_PSINFO_T_PR_POOLID
   case POOLID:
      return (p1->poolid - p2->poolid);
#endif
#ifdef HAVE_PSINFO_T_PR_CONTRACT
   case CONTID:
      return (p1->contid - p2->contid);
#endif
#ifdef HAVE_ZONE_H
   case ZONE:
      if(!p1->zname && !p2->zname) return p1->zoneid - p2->zoneid;
      return settings->sort_strcmp(p1->zname ? p1->zname : "global", p2->zname ? p2->zname : "global");
#endif
   case PID:
      return (p1->realpid - p2->realpid);
   case PPID:
      return (p1->realppid - p2->realppid);
#ifdef HAVE_LIBPROC
   case LWPID:
      return (p1->lwpid - p2->lwpid);
#endif
   default:
      return Process_compare(v1, v2);
   }
}

bool Process_isKernelProcess(const Process *this) {
	return ((const SolarisProcess *)this)->kernel;
}

bool Process_isExtraThreadProcess(const Process* this) {
   const SolarisProcess *fp = (const SolarisProcess *)this;
   return fp->is_lwp;
}
