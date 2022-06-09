/*
htop - dragonflybsd/DragonFlyBSDProcess.c
(C) 2015 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "DragonFlyBSDProcess.h"
#include "bsd/BSDProcess.h"
#include "ProcessList.h"
#include "Platform.h"
#include "CRT.h"
#include <stdlib.h>
#include <string.h>

/*{
#include "Settings.h"
#include <stdbool.h>

typedef enum {
   // Add platform-specific fields here, with ids >= 100
   HTOP_JID_FIELD = 100,
   HTOP_JAIL_FIELD = 101,
   HTOP_LAST_PROCESSFIELD = 102,
} DragonFlyBSDProcessField;

typedef struct DragonFlyBSDProcess_ {
   Process super;
   bool kernel;
   int   jid;
   char* jname;
} DragonFlyBSDProcess;
}*/

ProcessClass DragonFlyBSDProcess_class = {
   .super = {
      .extends = Class(Process),
      .display = Process_display,
      .delete = Process_delete,
      .compare = DragonFlyBSDProcess_compare
   },
   .writeField = DragonFlyBSDProcess_writeField,
};

ProcessFieldData Process_fields[] = {
   [0] = { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_PID_FIELD] = { .name = "PID", .title = "    PID ", .description = "Process/thread ID", .flags = 0, },
   [HTOP_NAME_FIELD] = { .name = "NAME", .title = "NAME            ", .description = "Process (executable) name", .flags = 0, },
   [HTOP_COMM_FIELD] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [HTOP_STATE_FIELD] = { .name = "STATE", .title = "S ", .description = "Process state (R running, I idle, S sleeping, D uninterruptible sleeping, B uninterruptible kernel process, T stoppd, Z zombie, A asked page)", .flags = 0 },
   [HTOP_PPID_FIELD] = { .name = "PPID", .title = "   PPID ", .description = "Parent process ID", .flags = 0, },
   [HTOP_PGRP_FIELD] = { .name = "PGRP", .title = "   PGRP ", .description = "Process group ID", .flags = 0, },
   [HTOP_SESSION_FIELD] = { .name = "SESSION", .title = "    SID ", .description = "Process's session ID", .flags = 0, },
   [HTOP_TTY_FIELD] = { .name = "TTY", .title = "TTY     ", .description = "Controlling terminal", .flags = 0, },
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
   [HTOP_JID_FIELD] = { .name = "JID", .title = "    JID ", .description = "Jail prison ID", .flags = 0, },
   [HTOP_JAIL_FIELD] = { .name = "JAIL", .title = "JAIL        ", .description = "Jail prison name", .flags = 0, },
   [HTOP_LAST_PROCESSFIELD] = { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0, },
};

ProcessPidColumn Process_pidColumns[] = {
   { .id = HTOP_JID_FIELD, .label = "JID" },
   { .id = HTOP_PID_FIELD, .label = "PID" },
   { .id = HTOP_PPID_FIELD, .label = "PPID" },
   { .id = HTOP_TPGID_FIELD, .label = "TPGID" },
   { .id = HTOP_TGID_FIELD, .label = "TGID" },
   { .id = HTOP_PGRP_FIELD, .label = "PGRP" },
   { .id = HTOP_SESSION_FIELD, .label = "SID" },
   { .id = 0, .label = NULL },
};

DragonFlyBSDProcess* DragonFlyBSDProcess_new(Settings* settings) {
   DragonFlyBSDProcess* this = xCalloc(1, sizeof(DragonFlyBSDProcess));
   Object_setClass(this, Class(DragonFlyBSDProcess));
   Process_init(&this->super, settings);
   return this;
}

void Process_delete(Object* cast) {
   DragonFlyBSDProcess* this = (DragonFlyBSDProcess*) cast;
   Process_done((Process*)cast);
   free(this->jname);
   free(this);
}

void DragonFlyBSDProcess_writeField(const Process *super, RichString* str, ProcessField field) {
   const DragonFlyBSDProcess *this = (const DragonFlyBSDProcess *)super;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[HTOP_DEFAULT_COLOR];
   int n = sizeof(buffer);
   switch ((int) field) {
      // add Platform-specific fields here
      case HTOP_PID_FIELD:
         if(Process_isSelf(super)) attr = CRT_colors[HTOP_PROCESS_BASENAME_COLOR];
         xSnprintf(buffer, n, Process_pidFormat, this->kernel ? super->tgid : super->pid);
         break;
      case HTOP_JID_FIELD:
         xSnprintf(buffer, n, Process_pidFormat, this->jid);
         break;
      case HTOP_JAIL_FIELD:
         xSnprintf(buffer, n, "%-11s ", this->jname);
         if (buffer[12]) {
            buffer[11] = ' ';
            buffer[12] = '\0';
         }
         break;
      default:
         BSDProcess_writeField(super, str, field);
         return;
   }
   RichString_append(str, attr, buffer);
}

long DragonFlyBSDProcess_compare(const void* v1, const void* v2) {
   const DragonFlyBSDProcess *p1, *p2;
   const Settings *settings = ((const Process *)v1)->settings;
   if (settings->direction == 1) {
      p1 = v1;
      p2 = v2;
   } else {
      p2 = v1;
      p1 = v2;
   }
   switch ((int) settings->sortKey) {
      // add Platform-specific fields here
      case HTOP_JID_FIELD:
         return (p1->jid - p2->jid);
      case HTOP_JAIL_FIELD:
         if(!p1->jname && !p2->jname) return p1->jid - p2->jid;
         return settings->sort_strcmp(p1->jname ? p1->jname : "", p2->jname ? p2->jname : "");
      default:
         return Process_compare(v1, v2);
   }
}

bool Process_isKernelProcess(const Process *this) {
	return ((const DragonFlyBSDProcess *)this)->kernel;
}

bool Process_isExtraThreadProcess(const Process* this) {
	return false;
}

char **Process_getKernelStackTrace(const Process *this) {
	return NULL;
}
