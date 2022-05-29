/*
htop - freebsd/FreeBSDProcess.c
(C) 2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "FreeBSDProcess.h"
#include "bsd/BSDProcess.h"
#include "ProcessList.h"
#include "Platform.h"
#include "CRT.h"
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

/*{
#include "Settings.h"
#include <stdbool.h>

typedef enum {
   // Add platform-specific fields here, with ids >= 100
   HTOP_JID_FIELD = 100,
   HTOP_JAIL_FIELD,
   HTOP_EMULATION_FIELD,
   HTOP_LAST_PROCESSFIELD
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
   [HTOP_PID_FIELD] = { .name = "PID", .title = "    PID ", .description = "Process/thread ID", .flags = 0, },
   [HTOP_NAME_FIELD] = { .name = "NAME", .title = "NAME            ", .description = "Process (executable) name", .flags = 0, },
   [HTOP_COMM_FIELD] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [HTOP_STATE_FIELD] = { .name = "STATE", .title = "S ", .description = "Process state (R running, I idle, S sleeping, D uninterruptible sleeping, T stoppd, Z zombie, W idle kernel process, L waiting lock)", .flags = 0 },
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
   [HTOP_EMULATION_FIELD] = { .name = "EMULATION", .title = "EMULATION        ", .description = "Binary format emulation type", .flags = 0 },
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
      case HTOP_EMULATION_FIELD:
         xSnprintf(buffer, n, "%-16s ", this->emulation);
         if(buffer[17]) {
            buffer[16] = ' ';
            buffer[17] = 0;
         }
         break;
      default:
         BSDProcess_writeField(super, str, field);
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
      case HTOP_JID_FIELD:
         return (p1->jid - p2->jid);
      case HTOP_JAIL_FIELD:
         if(!p1->jname && !p2->jname) return p1->jid - p2->jid;
         return settings->sort_strcmp(p1->jname ? p1->jname : "", p2->jname ? p2->jname : "");
      case HTOP_EMULATION_FIELD:
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

char **Process_getKernelStackTrace(const Process *this) {
#ifdef KERN_PROC_KSTACK
	char **v = xMalloc(2 * sizeof(char *));
	unsigned int i = 0;
	int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_KSTACK, this->pid };
	size_t len;
	if(sysctl(mib, 4, NULL, &len, NULL, 0) < 0) {
ret_err_msg:
		v[0] = strdup(strerror(errno));
		if(v[0]) {
			v[1] = NULL;
		} else {
			free(v);
			v = NULL;
		}
		return v;
	}
	if(len < sizeof(struct kinfo_kstack)) {
		v[0] = xStrdup("No stack available");
		v[1] = NULL;
		return v;
	}
	struct kinfo_kstack *kiks_buffer = malloc(len);
	if(!kiks_buffer) goto ret_err_msg;
	if(sysctl(mib, 4, kiks_buffer, &len, NULL, 0) < 0) {
		free(kiks_buffer);
		goto ret_err_msg;
	}
	len /= sizeof(struct kinfo_kstack);
	for(size_t j = 0; j < len; j++) {
		struct kinfo_kstack *kiks = kiks_buffer + j;
		v = xRealloc(v, (i + 3) * sizeof(char *));
		v[i] = xMalloc(32);
		snprintf(v[i++], 32, "Thread %d:", kiks->kkst_tid);
		switch(kiks->kkst_state) {
				char *p, *end_p;
			case KKST_STATE_STACKOK:
				p = kiks->kkst_trace;
				do {
					end_p = strchr(p, '\n');
					int prefix_len = *p == '#' ? 0 : 3;
					size_t frame_len = prefix_len +
						(end_p ? (size_t)(end_p - p) : strlen(p));
					v[i] = xMalloc(frame_len + 1);
					if(*p != '#') memcpy(v[i], "#? ", 3);
					memcpy(v[i] + prefix_len, p, frame_len - prefix_len);
					v[i][frame_len] = 0;
					v = xRealloc(v, (++i + 1) * sizeof(char *));
				} while(end_p && *(p = end_p + 1));
				break;
			case KKST_STATE_SWAPPED:
				v[i++] = xStrdup("swapped");
				break;
			case KKST_STATE_RUNNING:
				v[i++] = xStrdup("running");
				break;
			default:
				v[i++] = xStrdup("unknown state");
				break;
		}
	}
	free(kiks_buffer);
	v[i] = NULL;
	return v;
#else
	return NULL;
#endif
}
