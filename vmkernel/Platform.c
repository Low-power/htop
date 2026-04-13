/*
htop - vmkernel/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2015 David C. Hunt
Copyright 2015-2026 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "Action.h"
#include "BatteryMeter.h"
#include "SignalsPanel.h"
#include "VMKernelProcess.h"
#include <stdint.h>
#include <stdlib.h>

#define PLATFORM_SUPPORT_PROCESS_O_STATE
#define PLATFORM_PRESENT_THREADS_AS_PROCESSES

#define VMKERNEL_VERSION_5_5 55
#define VMKERNEL_VERSION_6_0 60
#define VMKERNEL_VERSION_6_5 65
#define VMKERNEL_VERSION_6_7 67

#define SYS_VSI_Get 1194
#define SYS_VSI_GetList 1197
#define SYS_VSI_GetNodeInfo 1202
#define SYS_GetMemSize 1215
#define SYS_GetUptimeUS 1216

struct vsi_node_info {
	uint32_t id;
	uint8_t _unknown_1[9];
	uint32_t parent_id;
	uint32_t sibling_id;
	uint32_t first_child_id;
	uint8_t _unknown_2[28];
	uint64_t checksum;
	uint8_t _unknown_3[67];
} __attribute__((__packed__));

struct vsi_param {
	uint32_t type;
	uint64_t value;
	uint8_t _reserved[28];
} __attribute__((__packed__));

struct vsi_param_6_7 {
	uint64_t value;
	uint8_t _reserved[20];
	uint32_t type;
} __attribute__((__packed__));

struct vsi_list {
	uint32_t type_or_version;
	uint32_t flags;
	uint32_t used_count;
	uint32_t instance_count;
	uint32_t allocated_count;
	uint32_t param_size;
	uint32_t unknown_1;
	uint32_t string_size;
	uint64_t string_offset;
	uint64_t self_ptr;
	uint64_t unknown_2[2];
	union {
		struct vsi_param v5[0];
		struct vsi_param_6_7 v6_7[0];
	} param;
};

static inline uint64_t Platform_vsiListGetValue(const struct vsi_list *list, int i) {
	extern int Platform_running_vmkernel_version;
	switch(Platform_running_vmkernel_version) {
		case VMKERNEL_VERSION_5_5:
		case VMKERNEL_VERSION_6_0:
		case VMKERNEL_VERSION_6_5:
			return list->param.v5[i].value;
		case VMKERNEL_VERSION_6_7:
			return list->param.v6_7[i].value;
		default:
			abort();
	}
}

static inline void Platform_vsiListSetValue(struct vsi_list *list, int i, uint64_t value) {
	extern int Platform_running_vmkernel_version;
	switch(Platform_running_vmkernel_version) {
		case VMKERNEL_VERSION_5_5:
		case VMKERNEL_VERSION_6_0:
		case VMKERNEL_VERSION_6_5:
			list->param.v5[i].value = value;
			break;
		case VMKERNEL_VERSION_6_7:
			list->param.v6_7[i].value = value;
			break;
		default:
			abort();
	}
}

}*/

#include "vmk_error_codes.h"
#include <Platform.h>
#include <CPUMeter.h>
#include <MemoryMeter.h>
#include <SwapMeter.h>
#include <TasksMeter.h>
#include <LoadAverageMeter.h>
#include <ClockMeter.h>
#include <HostnameMeter.h>
#include <UptimeMeter.h>
#include <UsersMeter.h>
#include <CRT.h>
#include <VMKernelProcessList.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifdef __i386__
#define VMKSC_0(N) ({ int e; __asm__ __volatile__("int $0x90" : "=a"(e) : "a"(N) : "memory"); e; })
#define VMKSC_1(N,A1) \
	({ int e; __asm__ __volatile__("int $0x90" : "=a"(e) : "a"(N), "c"(A1) : "memory"); e; })
#define VMKSC_2(N,A1,A2) \
	({ int e; __asm__ __volatile__("int $0x90" : "=a"(e) : "a"(N), "c"(A1), "d"(A2) : "memory"); e; })
#define VMKSC_3(N,A1,A2,A3) \
	({ int e; __asm__ __volatile__("int $0x90" : "=a"(e) : "a"(N), "c"(A1), "d"(A2), "S"(A3) : "memory"); e; })
#define VMKSC_4(N,A1,A2,A3,A4) \
	({ int e; __asm__ __volatile__("int $0x90" : "=a"(e) : "a"(N), "c"(A1), "d"(A2), "S"(A3), "D"(A4) : "memory"); e; })
#define VMKSC_VA_6(A1,A2,A3,A4,A5,A6,...) A6
#define VMKSC_VA(...) VMKSC_VA_6(__VA_ARGS__,VMKSC_4,VMKSC_3,VMKSC_2,VMKSC_1,VMKSC_0,)
#define VMKSC(...) VMKSC_VA(__VA_ARGS__)(__VA_ARGS__)
#else
#define VMKSC syscall
#endif

const SignalItem Platform_signals[] = {
   { .name = "Cancel", .number = 0 },
#define SIG(NAME) { .name = #NAME, .number = SIG##NAME }
   SIG(HUP),
   SIG(INT),
   SIG(QUIT),
   SIG(ILL),
   SIG(TRAP),
   SIG(ABRT),
#ifdef SIGIOT
   SIG(IOT),
#endif
#ifdef SIGEMT
   SIG(EMT),
#endif
   SIG(BUS),
   SIG(FPE),
   SIG(KILL),
   SIG(USR1),
   SIG(SEGV),
   SIG(USR2),
   SIG(PIPE),
   SIG(ALRM),
   SIG(TERM),
#ifdef SIGSTKFLT
   SIG(STKFLT),
#endif
   SIG(CHLD),
   SIG(CONT),
   SIG(STOP),
   SIG(TSTP),
   SIG(TTIN),
   SIG(TTOU),
   SIG(URG),
   SIG(XCPU),
   SIG(XFSZ),
   SIG(VTALRM),
   SIG(PROF),
   SIG(WINCH),
   SIG(IO),
#ifdef SIGPOLL
   SIG(POLL),
#endif
#ifdef SIGPWR
   SIG(PWR),
#endif
#ifdef SIGINFO
   SIG(INFO),
#endif
#ifdef SIGLOST
   SIG(LOST),
#endif
#ifdef SIGSYS
   SIG(SYS),
#endif
#undef SIG
};

const unsigned int Platform_numberOfSignals = sizeof(Platform_signals)/sizeof(SignalItem);

ProcessField Platform_defaultFields[] = {
	HTOP_PID_FIELD, HTOP_EFFECTIVE_USER_FIELD, HTOP_NICE_FIELD, HTOP_M_SIZE_FIELD, HTOP_M_RESIDENT_FIELD, HTOP_STATE_FIELD, HTOP_PERCENT_CPU_FIELD, HTOP_PERCENT_MEM_FIELD, HTOP_TIME_FIELD, HTOP_COMM_FIELD, 0
};

const unsigned int Platform_numberOfFields = HTOP_LAST_PROCESSFIELD;

MeterClass* Platform_meterTypes[] = {
   &CPUMeter_class,
   &ClockMeter_class,
   &LoadAverageMeter_class,
   &LoadMeter_class,
   &MemoryMeter_class,
   &SwapMeter_class,
   &TasksMeter_class,
#ifdef HAVE_UTMPX
   &UsersMeter_class,
#endif
   &BatteryMeter_class,
   &HostnameMeter_class,
   &UptimeMeter_class,
   &AllCPUsMeter_class,
   &AllCPUs2Meter_class,
   &LeftCPUsMeter_class,
   &RightCPUsMeter_class,
   &LeftCPUs2Meter_class,
   &RightCPUs2Meter_class,
   &BlankMeter_class,
   NULL
};

void Platform_setBindings(Htop_Action* keys) {
   (void) keys;
}

int Platform_getUptime() {
	uint64_t uptime;
	int e = VMKSC(SYS_GetUptimeUS, &uptime);
	return e == VMK_OK ? (int)(uptime / 1000000) : -1;
}

void Platform_getLoadAverage(double* one, double* five, double* fifteen) {
	double values[3] = { 0 };
	getloadavg(values, 3);
	*one = values[0];
	*five = values[1];
	*fifteen = values[2];
}

int Platform_getMaxPid() {
   return (1 << 30) - 1;
}

double Platform_updateCPUValues(Meter *meter, int cpu) {
	double total_percent;
	const VMKernelProcessList *pl = (const VMKernelProcessList *)meter->pl;
	if(!pl->interval_usec) {
		total_percent = 0;
		meter->values[CPU_METER_NORMAL] = 0;
		meter->values[CPU_METER_KERNEL] = 0;
	} else if(cpu || pl->super.cpuCount == 1) {
		const VMKernelCPUStatistics *stats = pl->cpu_stats + (cpu ? cpu - 1 : 0);
		total_percent = stats->total_used_period / (double)pl->interval_usec * 100;
		meter->values[CPU_METER_NORMAL] =
			(stats->total_used_period - stats->kernel_used_period) / (double)pl->interval_usec * 100;
		meter->values[CPU_METER_KERNEL] = stats->kernel_used_period / (double)pl->interval_usec * 100;
	} else {
		double total_time = 0;
		double total_user_time = 0;
		double total_kernel_time = 0;
		for(int i = 0; i < pl->super.cpuCount; i++) {
			const VMKernelCPUStatistics *stats = pl->cpu_stats + i;
			total_time += stats->total_used_period;
			total_user_time += stats->total_used_period - stats->kernel_used_period;
			total_kernel_time += stats->kernel_used_period;
		}
		total_percent = total_time / (double)pl->super.cpuCount / (double)pl->interval_usec * 100;
		meter->values[CPU_METER_NORMAL] =
			total_user_time / (double)pl->super.cpuCount / (double)pl->interval_usec * 100;
		meter->values[CPU_METER_KERNEL] =
			total_kernel_time / (double)pl->super.cpuCount / (double)pl->interval_usec * 100;
	}
	return total_percent;
}

void Platform_updateMemoryValues(Meter *meter) {
#if 0
	uint64_t total, free;
	unsigned int e = VMKSC(SYS_GetMemSize, &total, &free);
	if(e != VMK_OK) return;
	meter->total = total / 1024;
	meter->values[0] = (total - free) / 1024;
#else
	const ProcessList *pl = meter->pl;
	meter->total = pl->totalMem;
	meter->values[0] = pl->usedMem;
#endif
}

void Platform_updateSwapValues(Meter *meter) {
	// TODO
}

char **Platform_getProcessArgv(const Process *proc) {
	return NULL;
}

char **Platform_getProcessEnvv(const Process *proc) {
	return NULL;
}

bool Platform_haveSwap() {
	// TODO
	return false;
}

int Platform_vsiGetNodeIdAndChecksum(uint32_t parent_node_id, const char *node_name, uint32_t *node_id, uint64_t *node_cksum) {
	struct vsi_node_info info_buffer;
	char name_buffer[128];
#ifdef __i386__
	uint32_t extra_args[2] = { (uint32_t)name_buffer, sizeof info_buffer };
	int e = VMKSC(SYS_VSI_GetNodeInfo, parent_node_id, 0, &info_buffer, extra_args);
#else
	int e = syscall(SYS_VSI_GetNodeInfo, parent_node_id, 0,
		&info_buffer, name_buffer, sizeof info_buffer);
#endif
	if(e) return e;
	uint32_t child_node_id = info_buffer.first_child_id;
	do {
#ifdef __i386__
		e = VMKSC(SYS_VSI_GetNodeInfo, child_node_id, 0, &info_buffer, extra_args);
#else
		e = syscall(SYS_VSI_GetNodeInfo, child_node_id, 0,
			&info_buffer, name_buffer, sizeof info_buffer);
#endif
		if(e) return e;
		if(strcmp(name_buffer, node_name) == 0) {
			if(node_id) *node_id = info_buffer.id;
			if(node_cksum) memcpy(node_cksum, &info_buffer.checksum, 8);
			return VMK_OK;
		}
		child_node_id = info_buffer.sibling_id;
	} while(child_node_id && child_node_id != UINT32_MAX);
	//return VMK_NOT_FOUND;
	return ENOENT;
}

int Platform_running_vmkernel_version;

int Platform_vsi_int;
int Platform_vsi_string;

#ifndef __i386__

static int vsi_get_5_0(uint32_t node_id, uint64_t node_cksum, const struct vsi_list *list, void *buffer, size_t size) {
	struct {
		uint64_t checksum;
		void *buffer;
		size_t buffer_size;
		uint64_t _unknown_1;
	} extra_args[2] = {
		{ node_cksum, buffer, size },
		{ node_cksum, buffer, size }
	};
	return syscall(SYS_VSI_Get, node_id, 0, list, list->param_size, 0, extra_args);
}

static int vsi_get_6_5(uint32_t node_id, uint64_t node_cksum, const struct vsi_list *list, void *buffer, size_t size) {
	struct {
		uint32_t _unknown_1;
		void *buffer;
		size_t buffer_size;
	} __attribute__((__packed__)) extra_args = {
		0, buffer, size
	};
	return syscall(SYS_VSI_Get, node_id, 0, node_cksum, list, list->param_size, &extra_args);
}

static int (*vsi_get)(uint32_t, uint64_t, const struct vsi_list *, void *, size_t);

static int vsi_get_list_5_0(uint32_t node_id, uint64_t node_cksum, struct vsi_list *list, size_t size) {
	struct vsi_list input_list = {
		.type_or_version = 1, .param_size = sizeof(struct vsi_list), .self_ptr = (uintptr_t)&input_list
	};
	struct {
		uint64_t checksum;
		size_t output_size;
	} extra_args[2] = {
		{ node_cksum, size }, { node_cksum, size }
	};
	return syscall(SYS_VSI_GetList, node_id, 0, &input_list, input_list.param_size, list,
		extra_args);
}

static int vsi_get_list_6_5(uint32_t node_id, uint64_t node_cksum, struct vsi_list *list, size_t size) {
	struct vsi_list input_list = {
		.type_or_version = 1, .param_size = sizeof(struct vsi_list), .self_ptr = (uintptr_t)&input_list
	};
	struct {
		struct vsi_list *output_list;
		size_t output_size;
	} extra_args[2] = {
		{ list, size }, { list, size }
	};
	return syscall(SYS_VSI_GetList, node_id, 0, node_cksum, &input_list, input_list.param_size,
		extra_args);
}

static int (*vsi_get_list)(uint32_t, uint64_t, struct vsi_list *, size_t);

#endif

void Platform_checkVMkernelVersion() {
	struct utsname utsname;
	if(uname(&utsname) < 0) CRT_fatalError("uname", 0);
	if(strcmp(utsname.sysname, "VMkernel")) CRT_fatalError("Kernel type not supported", EPERM);
	char *end_p;
	long int major_version = strtol(utsname.release, &end_p, 10);
	if(end_p == utsname.release || *end_p != '.') {
		CRT_fatalError("Kernel version not supported", EPERM);
	}
	const char *begin_p = end_p + 1;
	long int minor_version = strtol(begin_p, &end_p, 10);
	if(end_p == begin_p || *end_p != '.') CRT_fatalError("Kernel version not supported", EPERM);
	Platform_running_vmkernel_version = major_version * 10 + minor_version;
	switch(Platform_running_vmkernel_version) {
		case VMKERNEL_VERSION_5_5:
			Platform_vsi_int = 0;
			Platform_vsi_string = 1;
#ifndef __i386__
			vsi_get = vsi_get_5_0;
			vsi_get_list = vsi_get_list_5_0;
#endif
			break;
		case VMKERNEL_VERSION_6_0:
			Platform_vsi_int = 0;
			Platform_vsi_string = 1;
#ifndef __i386__
			vsi_get = vsi_get_5_0;
			vsi_get_list = vsi_get_list_5_0;
#endif
			break;
		case VMKERNEL_VERSION_6_5:
			Platform_vsi_int = 0;
			Platform_vsi_string = 1;
#ifndef __i386__
			vsi_get = vsi_get_6_5;
			vsi_get_list = vsi_get_list_6_5;
#endif
			break;
		case VMKERNEL_VERSION_6_7:
			Platform_vsi_int = 1;
			Platform_vsi_string = 2;
#ifndef __i386__
			vsi_get = vsi_get_6_5;
			vsi_get_list = vsi_get_list_6_5;
#endif
			break;
		default:
			CRT_fatalError("Kernel version not supported", EPERM);
	}
}

void Platform_vsiListAddInt(struct vsi_list *list, uint64_t value) {
	uint32_t i = list->used_count;
	assert(i < list->allocated_count);
	switch(Platform_running_vmkernel_version) {
		case VMKERNEL_VERSION_5_5:
		case VMKERNEL_VERSION_6_0:
		case VMKERNEL_VERSION_6_5:
			list->param.v5[i].type = 0;
			list->param.v5[i].value = value;
			break;
		case VMKERNEL_VERSION_6_7:
			list->param.v6_7[i].type = 1;
			list->param.v6_7[i].value = value;
			break;
		default:
			abort();
	}
	list->used_count++;
	list->instance_count++;
}

int Platform_vsiGet(uint32_t node_id, uint64_t node_cksum, const struct vsi_list *list, void *buffer, size_t size) {
#ifdef __i386__
	struct {
		uint64_t checksum;
		size_t input_size;
		uint32_t _unknown_1;
		void *buffer;
		size_t buffer_size;
	} extra_args[2] = {
		{ node_cksum, list->param_size, 0, buffer, size },
		{ node_cksum, list->param_size, 0, buffer, size }
	};
	return VMKSC(SYS_VSI_Get, node_id, 0, list, extra_args);
#else
	if(!vsi_get) Platform_checkVMkernelVersion();
	return vsi_get(node_id, node_cksum, list, buffer, size);
#endif
}

int Platform_vsiGetList(uint32_t node_id, uint64_t node_cksum, struct vsi_list *list, size_t size) {
#ifdef __i386__
	struct vsi_list input_list = {
		.type_or_version = 1, .param_size = sizeof(struct vsi_list), .self_ptr = (uintptr_t)&input_list
	};
	struct {
		uint64_t checksum;
		size_t input_size;
		struct vsi_list *output_list;
		size_t output_size;
	} extra_args[2] = {
		{ node_cksum, input_list.param_size, list, size },
		{ node_cksum, input_list.param_size, list, size }
	};
	return VMKSC(SYS_VSI_GetList, node_id, 0, &input_list, extra_args);
#else
	if(!vsi_get_list) Platform_checkVMkernelVersion();
	return vsi_get_list(node_id, node_cksum, list, size);
#endif
}
