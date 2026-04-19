/*
htop - freebsd/FreeBSDProcess.c
(C) 2015 Hisham H. Muhammad
Copyright 2015-2026 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "FreeBSDProcess.h"
#include "bsd/BSDProcess.h"
#include "ProcessList.h"
#include "Platform.h"
#include "CRT.h"
#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

/*{
#include "config.h"
#include "Settings.h"
#include <stdbool.h>

#define PROCESS_FLAG_JAIL 0x100
#define PROCESS_FLAG_EMULATION 0x200
#define PROCESS_FLAG_IO_RATE 0x400

typedef enum {
   // Add platform-specific fields here, with ids >= 100
   HTOP_JID_FIELD = 100,
   HTOP_JAIL_FIELD,
   HTOP_EMULATION_FIELD,
   HTOP_FIB_FIELD,
   HTOP_CMINFLT_FIELD,
   HTOP_CMAJFLT_FIELD,
   HTOP_READ_BLOCKS_FIELD,
   HTOP_WRITE_BLOCKS_FIELD,
   HTOP_READ_BLOCK_RATE_FIELD,
   HTOP_WRITE_BLOCK_RATE_FIELD,
   HTOP_IO_RATE_FIELD,
   HTOP_LAST_PROCESSFIELD
} FreeBSDProcessField;

typedef struct FreeBSDProcess_ {
   Process super;
   bool kernel;
#ifdef HAVE_STRUCT_KINFO_PROC_KI_FIBNUM
   int fib;
#endif
#ifdef HAVE_STRUCT_KINFO_PROC_KI_JID
   int   jid;
   char* jname;
#endif
   char *emulation;
#ifdef HAVE_STRUCT_KINFO_PROC_KI_RUSAGE_CH
   unsigned long int cminflt;
   unsigned long int cmajflt;
#endif
   long int read_block_count;
   long int write_block_count;
   long int read_blocks_per_sec;
   long int write_blocks_per_sec;
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

FieldData Process_fields[] = {
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
#ifdef HAVE_STRUCT_KINFO_PROC_KI_RUSAGE_CH
   [HTOP_CMINFLT_FIELD] = { .name = "CMINFLT", .title = "    CMINFLT ", .description = "Children processes' minor faults", .flags = 0, },
   [HTOP_CMAJFLT_FIELD] = { .name = "CMAJFLT", .title = "    CMAJFLT ", .description = "Children processes' major faults", .flags = 0, },
#endif
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
#ifdef HAVE_STRUCT_KINFO_PROC_KI_JID
   [HTOP_JID_FIELD] = { .name = "JID", .title = "    JID ", .description = "Jail prison ID", .flags = 0, },
   [HTOP_JAIL_FIELD] = { .name = "JAIL", .title = "JAIL        ", .description = "Jail prison name", .flags = PROCESS_FLAG_JAIL },
#endif
   [HTOP_EMULATION_FIELD] = { .name = "EMULATION", .title = "EMULATION        ", .description = "Binary format emulation type", .flags = PROCESS_FLAG_EMULATION },
#ifdef HAVE_STRUCT_KINFO_PROC_KI_FIBNUM
   [HTOP_FIB_FIELD] = { .name = "FIB", .title = "  FIB ", .description = "Routing table ID", .flags = 0 },
#endif
   [HTOP_READ_BLOCKS_FIELD] = { .name = "READ_BLOCKS", .title = "   R_BLOCKS ", .description = "Number of blocks the process has read", .flags = PROCESS_FLAG_IO },
   [HTOP_WRITE_BLOCKS_FIELD] = { .name = "WRITE_BLOCKS", .title = "   W_BLOCKS ", .description = "Number of blocks the process has written", .flags = PROCESS_FLAG_IO },
   [HTOP_READ_BLOCK_RATE_FIELD] = { .name = "READ_BLOCK_RATE", .title = "  RBLK/S ", .description = "Read rate in blocks per second for the process", .flags = PROCESS_FLAG_IO_RATE },
   [HTOP_WRITE_BLOCK_RATE_FIELD] = { .name = "WRITE_BLOCK_RATE", .title = "  WBLK/S ", .description = "Write rate in blocks per second for the process", .flags = PROCESS_FLAG_IO_RATE },
   [HTOP_IO_RATE_FIELD] = { .name = "IO_RATE", .title = " RWBLK/S ", .description = "Total I/O rate in blocks per second for the process", .flags = PROCESS_FLAG_IO_RATE },
   [HTOP_LAST_PROCESSFIELD] = { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0, },
};

ProcessPidColumn Process_pidColumns[] = {
#ifdef HAVE_STRUCT_KINFO_PROC_KI_JID
   { .id = HTOP_JID_FIELD, .label = "JID" },
#endif
   { .id = HTOP_PID_FIELD, .label = "PID" },
   { .id = HTOP_PPID_FIELD, .label = "PPID" },
   { .id = HTOP_TPGID_FIELD, .label = "TPGID" },
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
#ifdef HAVE_STRUCT_KINFO_PROC_KI_JID
   free(this->jname);
#endif
   free(this->emulation);
   free(this);
}

void FreeBSDProcess_writeField(const Process *super, RichString* str, ProcessField field) {
   const FreeBSDProcess *this = (const FreeBSDProcess *)super;
   bool coloring = super->settings->highlightMegabytes;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[HTOP_DEFAULT_COLOR];
   int n = sizeof buffer;
   switch ((int) field) {
      // add FreeBSD-specific fields here
#ifdef HAVE_STRUCT_KINFO_PROC_KI_JID
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
#endif
      case HTOP_EMULATION_FIELD:
         xSnprintf(buffer, n, "%-16s ", this->emulation);
         if(buffer[17]) {
            buffer[16] = ' ';
            buffer[17] = 0;
         }
         break;
#ifdef HAVE_STRUCT_KINFO_PROC_KI_FIBNUM
      case HTOP_FIB_FIELD:
         xSnprintf(buffer, n, "%5d ", this->fib);
         break;
#endif
#ifdef HAVE_STRUCT_KINFO_PROC_KI_RUSAGE_CH
      case HTOP_CMINFLT_FIELD:
         Process_colorNumber(str, this->cminflt, coloring);
         return;
      case HTOP_CMAJFLT_FIELD:
         Process_colorNumber(str, this->cmajflt, coloring);
         return;
#endif
      case HTOP_READ_BLOCKS_FIELD:
         Process_colorNumber(str, this->read_block_count, coloring);
         return;
      case HTOP_WRITE_BLOCKS_FIELD:
         Process_colorNumber(str, this->write_block_count, coloring);
         return;
      case HTOP_READ_BLOCK_RATE_FIELD:
         xSnprintf(buffer, sizeof buffer, "%8ld ", this->read_blocks_per_sec);
         break;
      case HTOP_WRITE_BLOCK_RATE_FIELD:
         xSnprintf(buffer, sizeof buffer, "%8ld ", this->write_blocks_per_sec);
         break;
      case HTOP_IO_RATE_FIELD:
         xSnprintf(buffer, sizeof buffer, "%8ld ", this->read_blocks_per_sec + this->write_blocks_per_sec);
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
#ifdef HAVE_STRUCT_KINFO_PROC_KI_JID
      case HTOP_JID_FIELD:
         return (p1->jid - p2->jid);
      case HTOP_JAIL_FIELD:
         if(!p1->jname && !p2->jname) return p1->jid - p2->jid;
         return settings->sort_strcmp(p1->jname ? p1->jname : "", p2->jname ? p2->jname : "");
#endif
      case HTOP_EMULATION_FIELD:
         return settings->sort_strcmp(p1->emulation, p2->emulation);
#ifdef HAVE_STRUCT_KINFO_PROC_KI_FIBNUM
      case HTOP_FIB_FIELD:
         return p1->fib - p2->fib;
#endif
#ifdef HAVE_STRUCT_KINFO_PROC_KI_RUSAGE_CH
      case HTOP_CMINFLT_FIELD:
         return uintcmp(p2->cminflt, p1->cminflt);
      case HTOP_CMAJFLT_FIELD:
         return uintcmp(p2->cmajflt, p1->cmajflt);
#endif
      case HTOP_READ_BLOCKS_FIELD:
         return p2->read_block_count - p1->read_block_count;
      case HTOP_WRITE_BLOCKS_FIELD:
         return p2->write_block_count - p1->write_block_count;
      case HTOP_READ_BLOCK_RATE_FIELD:
         return p2->read_blocks_per_sec - p1->read_blocks_per_sec;
      case HTOP_WRITE_BLOCK_RATE_FIELD:
         return p2->write_blocks_per_sec - p1->write_blocks_per_sec;
      case HTOP_IO_RATE_FIELD:
         return (p2->read_blocks_per_sec + p2->write_blocks_per_sec) - (p1->read_blocks_per_sec + p1->write_blocks_per_sec);
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
retry_get_size:
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
		int e = errno;
		free(kiks_buffer);
		if(e == ENOMEM) goto retry_get_size;
		errno = e;
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

char **Process_getVirtualMemoryMappings(const Process *this) {
#ifdef KERN_PROC_VMMAP
#ifndef KVME_FLAG_SUPER
#define KVME_FLAG_SUPER 0x8
#endif
#ifndef KVME_TYPE_SG
#define KVME_TYPE_SG 7
#endif
#ifndef KVME_TYPE_MGTDEVICE
#define KVME_TYPE_MGTDEVICE 8
#endif
	static const char *const vm_type_name_map[] = {
		[KVME_TYPE_NONE] = "none",
		[KVME_TYPE_DEFAULT] = "default",
		[KVME_TYPE_VNODE] = "vnode",
		[KVME_TYPE_SWAP] = "swap",
		[KVME_TYPE_DEVICE] = "device",
		[KVME_TYPE_PHYS] = "physical",
		[KVME_TYPE_DEAD] = "dead",
		[KVME_TYPE_SG] = "sg",
		[KVME_TYPE_MGTDEVICE] = "mgtdevice",
	};

	char **v = xMalloc(2 * sizeof(char *));
	int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_VMMAP, this->pid };
	size_t buffer_size;
retry_get_size:
	if(sysctl(mib, 4, NULL, &buffer_size, NULL, 0) < 0) {
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
	if(buffer_size < offsetof(struct kinfo_vmentry, kve_path) + 1) {
		v[0] = xStrdup("No mapping entry");
		v[1] = NULL;
		return v;
	}
	void *buffer = malloc(buffer_size);
	if(!buffer) goto ret_err_msg;
	if(sysctl(mib, 4, buffer, &buffer_size, NULL, 0) < 0) {
		int e = errno;
		free(buffer);
		if(e == ENOMEM) goto retry_get_size;
		errno = e;
		goto ret_err_msg;
	}
	const struct kinfo_vmentry *kive = buffer;
	if(kive->kve_path >= (const char *)buffer + buffer_size || kive->kve_structsize <= 0) {
		free(buffer);
		v[0] = xStrdup("No mapping entry");
		v[1] = NULL;
		return v;
	}
	int i = 0;
	do {
		size_t line_len = 56;
#ifdef HAVE_STRUCT_KINFO_VMENTRY_KVE_OFFSET
		line_len += 19;
#endif
#ifdef HAVE_STRUCT_KINFO_VMENTRY_KVE_VN_FSID
		line_len += 11;
#endif
#ifdef HAVE_STRUCT_KINFO_VMENTRY_KVE_VN_FILEID
		line_len += 21;
#endif
		size_t path_len = strlen(kive->kve_path);
		line_len += path_len;
		v[i] = xMalloc(line_len);
		int len = snprintf(v[i], line_len, "0x%016llx 0x%016llx %c%c%c %c%c%c %-9s ",
			(unsigned long long int)kive->kve_start, (unsigned long long int)kive->kve_end,
			kive->kve_protection & KVME_PROT_READ ? 'r' : '-',
			kive->kve_protection & KVME_PROT_WRITE ? 'w' : '-',
			kive->kve_protection & KVME_PROT_EXEC ? 'x' : '-',
			kive->kve_flags & KVME_FLAG_COW ? 'C' : '-',
			kive->kve_flags & KVME_FLAG_NEEDS_COPY ? 'N' : '-',
			kive->kve_flags & KVME_FLAG_SUPER ? 'S' : '-',
			(size_t)kive->kve_type < sizeof vm_type_name_map / sizeof(char *) ?
				vm_type_name_map[kive->kve_type] : "unknown");
		assert(len == 56);
#ifdef HAVE_STRUCT_KINFO_VMENTRY_KVE_OFFSET
		int offset_len = snprintf(v[i] + len, line_len - len, "0x%016llx ",
			(unsigned long long int)kive->kve_offset);
		assert(offset_len == 19);
		len += offset_len;
#endif
#ifdef HAVE_STRUCT_KINFO_VMENTRY_KVE_VN_FSID
		if(kive->kve_type == KVME_TYPE_VNODE) {
			int fsid_len = snprintf(v[i] + len, line_len - len, "0x%08x ",
				(unsigned int)kive->kve_vn_fsid);
			assert(fsid_len == 11);
		} else {
			memset(v[i] + len, ' ', 11);
		}
		len += 11;
#endif
#ifdef HAVE_STRUCT_KINFO_VMENTRY_KVE_VN_FILEID
		if(kive->kve_type == KVME_TYPE_VNODE) {
			int fi_len = snprintf(v[i] + len, line_len - len, "%18llu ",
				(unsigned long long int)kive->kve_vn_fileid);
			assert(fi_len <= 21);
			len += fi_len;
		} else {
			memset(v[i] + len, ' ', 19);
			len += 19;
		}
#endif
		memcpy(v[i] + len, kive->kve_path, path_len + 1);
		if(i++) v = xRealloc(v, (i + 1) * sizeof(char *));
		kive = (const struct kinfo_vmentry *)((const char *)kive + kive->kve_structsize);
	} while(kive->kve_path < (const char *)buffer + buffer_size && kive->kve_structsize > 0);
	free(buffer);
	v[i] = NULL;
	return v;
#else
	return NULL;
#endif
}
