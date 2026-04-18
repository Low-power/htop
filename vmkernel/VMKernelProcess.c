/*
htop - vmkernel/VMKernelProcess.c
(C) 2015 Hisham H. Muhammad
Copyright 2015-2026 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include <Process.h>
#include <Settings.h>
#include <stdint.h>
#include <stdbool.h>

#define Process_delete VMKernelProcess_delete

#define PROCESS_FLAG_VMKERNEL_PRIORITY 0x0100
#define PROCESS_FLAG_VMKERNEL_VCPU_COUNT 0x0200

typedef enum {
	HTOP_WORLD_GROUP_ID_FIELD = 100,
	HTOP_USERSPACE_ID_FIELD,
	HTOP_CARTEL_GROUP_ID_FIELD,
	HTOP_VCPU_COUNT_FIELD,
	HTOP_LAST_PROCESSFIELD
} VMKernelProcessField;

typedef struct {
	Process super;
	uint32_t group_id;
	uint32_t userspace_id;
	uint32_t cartel_group_id;
	uint32_t vcpu_count;
	uint64_t time_usec;
	bool is_kernel_process;
} VMKernelProcess;
}*/

#include <Process.h>
#include <VMKernelProcess.h>
#include <CRT.h>
#include <Platform.h>
#include <string.h>
#include <stdlib.h>

#define WORLD_SYSTEM 0x01
#define WORLD_IDLE 0x02
#define WORLD_USER 0x04
#define WORLD_VMM 0x08
#define WORLD_HELPER 0x10
#define WORLD_CLONE 0x20
#define WORLD_TEST 0x40
#define WORLD_UWVCPU 0x80
#define WORLD_ASSISTANT 0x100
#define WORLD_UTILITY_VM 0x200

ProcessClass VMKernelProcess_class = {
   .super = {
      .extends = Class(Process),
      .display = Process_display,
      .delete = Process_delete,
      .compare = VMKernelProcess_compare
   },
   .writeField = VMKernelProcess_writeField,
};

FieldData Process_fields[] = {
   [0] = { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_PID_FIELD] = { .name = "PID", .title = "    PID ", .description = "World ID", .flags = 0, },
   [HTOP_NAME_FIELD] = { .name = "NAME", .title = "NAME            ", .description = "World name", .flags = 0, },
   [HTOP_COMM_FIELD] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [HTOP_STATE_FIELD] = { .name = "STATE", .title = "S ", .description = "Process state (R running, T stopped, S sleeping, D uninterruptible sleeping)", .flags = 0 },
   [HTOP_PPID_FIELD] = { .name = "PPID", .title = "   PPID ", .description = "Parent process ID", .flags = 0, },
   [HTOP_PGRP_FIELD] = { .name = "PGRP", .title = "   PGRP ", .description = "Process group ID", .flags = 0, },
   [HTOP_SESSION_FIELD] = { .name = "SESSION", .title = "    SID ", .description = "Process's session ID", .flags = 0, },
   [HTOP_TTY_FIELD] = { .name = "TTY", .title = "    TTY ", .description = "Controlling terminal", .flags = 0, },
   [HTOP_TPGID_FIELD] = { .name = "TPGID", .title = "  TPGID ", .description = "Process ID of the fg process group of the controlling terminal", .flags = 0, },
   [HTOP_MINFLT_FIELD] = { .name = "MINFLT", .title = "     MINFLT ", .description = "Number of minor faults which have not required loading a memory page from disk", .flags = 0, },
   [HTOP_MAJFLT_FIELD] = { .name = "MAJFLT", .title = "     MAJFLT ", .description = "Number of major faults which have required loading a memory page from disk", .flags = 0, },
   //[HTOP_PRIORITY_FIELD] = { .name = "PRIORITY", .title = "PRI ", .description = "Kernel's internal priority for the process", .flags = PROCESS_FLAG_VMKERNEL_PRIORITY },
   [HTOP_NICE_FIELD] = { .name = "NICE", .title = " NI ", .description = "Nice value (the higher the value, the more it lets other processes take priority)", .flags = PROCESS_FLAG_VMKERNEL_PRIORITY },
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
   [HTOP_TGID_FIELD] = { .name = "TGID", .title = "   TGID ", .description = "Cartel ID", .flags = 0, },
   [HTOP_WORLD_GROUP_ID_FIELD] = { .name = "WGID", .title = "      WGID ", .description = "World group ID", .flags = 0 },
   [HTOP_USERSPACE_ID_FIELD] = { .name = "USID", .title = "USID ", .description = "Userspace ID", .flags = 0 },
   [HTOP_CARTEL_GROUP_ID_FIELD] = { .name = "CGID", .title = "      CGID ", .description = "Cartel group ID", .flags = 0 },
   [HTOP_VCPU_COUNT_FIELD] = { .name = "VCPU_COUNT", .title = "NVCPUS ", .description = "Number of virtual processors allocated to the world", .flags = PROCESS_FLAG_VMKERNEL_VCPU_COUNT },
   [HTOP_LAST_PROCESSFIELD] = { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0, },
};

ProcessPidColumn Process_pidColumns[] = {
   { .id = HTOP_PID_FIELD, .label = "PID" },
   { .id = HTOP_TGID_FIELD, .label = "TGID" },
   { .id = HTOP_PPID_FIELD, .label = "PPID" },
   { .id = HTOP_TPGID_FIELD, .label = "TPGID" },
   { .id = HTOP_PGRP_FIELD, .label = "PGRP" },
   { .id = HTOP_SESSION_FIELD, .label = "SID" },
   { .id = HTOP_WORLD_GROUP_ID_FIELD, .label = "WGID" },
   { .id = HTOP_CARTEL_GROUP_ID_FIELD, .label = "CGID" },
   { .id = 0, .label = NULL },
};

VMKernelProcess *VMKernelProcess_new(Settings* settings) {
   VMKernelProcess *this = xCalloc(1, sizeof(VMKernelProcess));
   Object_setClass(this, Class(VMKernelProcess));
   Process_init(&this->super, settings);
   return this;
}

void VMKernelProcess_delete(Object* cast) {
   VMKernelProcess *this = (VMKernelProcess *)cast;
   Object_setClass(this, Class(VMKernelProcess));
   Process_done((Process *)cast);
   free(this);
}

long int VMKernelProcess_compare(const void *o1, const void *o2) {
	const VMKernelProcess *p1, *p2;
	const Settings *settings = ((const Process *)o1)->settings;
	if(settings->direction == 1) {
		p1 = o1;
		p2 = o2;
	} else {
		p1 = o2;
		p2 = o1;
	}
	switch((int)settings->sortKey) {
		case HTOP_WORLD_GROUP_ID_FIELD:
			return uintcmp(p1->group_id, p2->group_id);
		case HTOP_USERSPACE_ID_FIELD:
			return uintcmp(p1->userspace_id, p2->userspace_id);
		case HTOP_CARTEL_GROUP_ID_FIELD:
			return uintcmp(p1->cartel_group_id, p2->cartel_group_id);
		case HTOP_VCPU_COUNT_FIELD:
			return uintcmp(p1->vcpu_count, p2->vcpu_count);
		default:
			return Process_compare(o1, o2);
	}
}

void VMKernelProcess_writeField(const Process *super, RichString *str, ProcessField field) {
	const VMKernelProcess *this = (const VMKernelProcess *)super;
	char buffer[256];
	switch((int)field) {
		case HTOP_WORLD_GROUP_ID_FIELD:
			xSnprintf(buffer, sizeof buffer, Process_pidFormat, this->group_id);
			break;
		case HTOP_USERSPACE_ID_FIELD:
			xSnprintf(buffer, sizeof buffer, "%4u ", (unsigned int)this->userspace_id);
			break;
		case HTOP_CARTEL_GROUP_ID_FIELD:
			xSnprintf(buffer, sizeof buffer, Process_pidFormat, this->cartel_group_id);
			break;
		case HTOP_VCPU_COUNT_FIELD:
			xSnprintf(buffer, sizeof buffer, "%6u ", (unsigned int)this->vcpu_count);
			break;
		default:
			Process_writeField(super, str, field);
			return;
	}
	RichString_append(str, CRT_colors[HTOP_DEFAULT_COLOR], buffer);
}

bool Process_isKernelProcess(const Process *this) {
	return ((const VMKernelProcess *)this)->is_kernel_process;
}

bool Process_isExtraThreadProcess(const Process *this) {
	return this->pid != this->tgid;
}

char **Process_getKernelStackTrace(const Process *this) {
	struct vsi_list *list = Platform_allocateVsiList(1, 0);
	Platform_vsiListAddInt(list, this->pid);
	struct world_backtrace_frame { uint64_t pc, bp; } frames[25];
	int e = Platform_vsiGet(platform.world_backtrace_id, platform.world_backtrace_cksum,
		list, frames, sizeof frames);
	if(e) {
		free(list);
		char **v = xMalloc(2 * sizeof(char *));
		v[0] = xMalloc(17);
		xSnprintf(v[0], 17, "Error 0x%08x", e);
		v[1] = NULL;
		return v;
	}
	size_t nframes = 0;
	while(nframes < sizeof frames / sizeof *frames && frames[nframes].bp) nframes++;
	char **v = xMalloc((nframes + 1) * sizeof(char *));
	size_t i = 0;
	while(i < nframes) {
		const struct world_backtrace_frame *f = frames + i;
		const char *pad = i <= 9 && nframes > 10 ? " " : "";
		// Reusing same 'list' to avoid repeated malloc(3) and free(3)
		Platform_vsiListSetValue(list, 0, f->pc);
		struct symbol { char name[128]; uint64_t addr; } symbol;
		e = Platform_vsiGet(platform.system_modloader_symaddrtoname_id,
			platform.system_modloader_symaddrtoname_cksum,
			list, &symbol, sizeof symbol);
		if(e) {
			v[i] = xMalloc(29);
			xSnprintf(v[i], 29, "%s#%zu 0x%016llx at ??", pad, i, (unsigned long long int)f->pc);
		} else {
			size_t name_len = strnlen(symbol.name, 128);
			char *end_p = memchr(symbol.name, 0, 128);
			if(!end_p) *(end_p = symbol.name + 127) = 0;
			size_t len = 22 + (name_len ? 4 + (end_p - symbol.name) + 20 : 1);
			v[i] = xMalloc(len);
			xSnprintf(v[i], len, "%s#%zu 0x%016llx at %s+0x%x", pad, i,
				(unsigned long long int)f->pc, symbol.name,
				(unsigned int)(f->pc - symbol.addr));
		}
		i++;
	}
	free(list);
	v[i] = NULL;
	return v;
}
