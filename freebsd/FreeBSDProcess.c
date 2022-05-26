/*
htop - freebsd/FreeBSDProcess.c
(C) 2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "ProcessList.h"
#include "FreeBSDProcess.h"
#include "Platform.h"
#include "CRT.h"

#include <stdlib.h>
#include <string.h>

/*{

typedef enum {
   // Add platform-specific fields here, with ids >= 100
   JID_FIELD = 100,
   JAIL_FIELD,
   EMULATION_FIELD,
   LAST_PROCESSFIELD
} FreeBSDProcessField;


typedef struct FreeBSDProcess_ {
   Process super;
   bool kernel;
   int   jid;
   char* jname;
   char *emulation;
} FreeBSDProcess;


}*/

ProcessClass FreeBSDProcess_class = {
   .super = {
      .extends = Class(Process),
      .display = Process_display,
      .delete = Process_delete,
      .compare = FreeBSDProcess_compare
   },
   .writeField = FreeBSDProcess_writeField,
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
   [JID_FIELD] = { .name = "JID", .title = "    JID ", .description = "Jail prison ID", .flags = 0, },
   [JAIL_FIELD] = { .name = "JAIL", .title = "JAIL        ", .description = "Jail prison name", .flags = 0, },
   [EMULATION_FIELD] = { .name = "EMULATION", .title = "EMULATION        ", .description = "Binary format emulation type", .flags = 0 },
   [LAST_PROCESSFIELD] = { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0, },
};

ProcessPidColumn Process_pidColumns[] = {
   { .id = JID_FIELD, .label = "JID" },
   { .id = PID, .label = "PID" },
   { .id = PPID, .label = "PPID" },
   { .id = TPGID, .label = "TPGID" },
   { .id = TGID, .label = "TGID" },
   { .id = PGRP, .label = "PGRP" },
   { .id = SESSION, .label = "SID" },
   { .id = 0, .label = NULL },
};

FreeBSDProcess* FreeBSDProcess_new(Settings* settings) {
   FreeBSDProcess* this = xCalloc(1, sizeof(FreeBSDProcess));
   Object_setClass(this, Class(FreeBSDProcess));
   Process_init(&this->super, settings);
   return this;
}

void Process_delete(Object* cast) {
   FreeBSDProcess* this = (FreeBSDProcess*) cast;
   Process_done((Process*)cast);
   free(this->jname);
   free(this->emulation);
   free(this);
}

void FreeBSDProcess_writeField(Process *super, RichString* str, ProcessField field) {
   FreeBSDProcess *this = (FreeBSDProcess *)super;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[HTOP_DEFAULT_COLOR];
   int n = sizeof buffer;
   switch ((int) field) {
      // add FreeBSD-specific fields here
      case JID_FIELD:
         xSnprintf(buffer, n, Process_pidFormat, this->jid);
         break;
      case JAIL_FIELD:
         xSnprintf(buffer, n, "%-11s ", this->jname);
         if (buffer[12]) {
            buffer[11] = ' ';
            buffer[12] = '\0';
         }
         break;
      case EMULATION_FIELD:
         xSnprintf(buffer, n, "%-16s ", this->emulation);
         if(buffer[17]) {
            buffer[16] = ' ';
            buffer[17] = 0;
         }
         break;
      default:
         Process_writeField(super, str, field);
         return;
   }
   RichString_append(str, attr, buffer);
}

long FreeBSDProcess_compare(const void* v1, const void* v2) {
   const FreeBSDProcess *p1, *p2;
   const Settings *settings = ((const Process *)v1)->settings;
   if (settings->direction == 1) {
      p1 = v1;
      p2 = v2;
   } else {
      p2 = v1;
      p1 = v2;
   }
   switch ((int) settings->sortKey) {
      // add FreeBSD-specific fields here
      case JID_FIELD:
         return (p1->jid - p2->jid);
      case JAIL_FIELD:
         if(!p1->jname && !p2->jname) return p1->jid - p2->jid;
         return settings->sort_strcmp(p1->jname ? p1->jname : "", p2->jname ? p2->jname : "");
      case EMULATION_FIELD:
         return settings->sort_strcmp(p1->emulation, p2->emulation);
      default:
         return Process_compare(v1, v2);
   }
}

bool Process_isKernelProcess(const Process *this) {
	return ((const FreeBSDProcess *)this)->kernel;
}

bool Process_isExtraThreadProcess(const Process* this) {
	return false;
}
