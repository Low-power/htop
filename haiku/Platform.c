/*
htop - haiku/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2015 David C. Hunt
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Platform.h"
#include "CPUMeter.h"
#include "MemoryMeter.h"
#include "SwapMeter.h"
#include "TasksMeter.h"
#include "LoadAverageMeter.h"
#include "ClockMeter.h"
#include "HostnameMeter.h"
#include "UptimeMeter.h"
#include "HaikuProcess.h"
#include "HaikuProcessList.h"
#include <OS.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

/*{
#include "config.h"
#include "Action.h"
#include "BatteryMeter.h"
#include "SignalsPanel.h"
#include <stdint.h>
#include <dlfcn.h>

#define PLATFORM_SUPPORT_PROCESS_O_STATE

#ifndef PRIO_PROCESS
#define PRIO_PROCESS 0
#endif

#if !defined HAVE_SYSTEM_INFO_CACHED_PAGES || !defined HAVE_SYSTEM_INFO_MAX_SWAP_PAGES
#ifndef B_MEMORY_INFO
#define B_MEMORY_INFO 0x6d656d6f
#endif

struct vm_stat {
	uint64_t max_memory;
	uint64_t free_memory;
	uint64_t needed_memory;
	uint64_t max_swap_space;
	uint64_t free_swap_space;
	uint64_t block_cache_memory;
	uint32_t page_faults;
};
#endif

static inline void *get_libroot() {
	static void *libroot;
	if(!libroot) libroot = dlopen("libroot.so", RTLD_LAZY);
	return libroot;
}
}*/

const SignalItem Platform_signals[] = {
   { .name = "Cancel", .number = 0 },
#define SIG(NAME) { .name = #NAME, .number = SIG##NAME }
   SIG(HUP),
   SIG(INT),
   SIG(QUIT),
   SIG(ILL),
   SIG(CHLD),
   SIG(PIPE),
   SIG(FPE),
   SIG(KILL),
   SIG(STOP),
   SIG(SEGV),
   SIG(CONT),
   SIG(TSTP),
   SIG(ALRM),
   SIG(TERM),
   SIG(TTIN),
   SIG(TTOU),
   SIG(USR1),
   SIG(USR2),
   SIG(WINCH),
   SIG(KILLTHR),
   SIG(TRAP),
   SIG(POLL),
   SIG(PROF),
   SIG(SYS),
   SIG(URG),
   SIG(VTALRM),
   SIG(XCPU),
   SIG(XFSZ),
   SIG(BUS),
#undef SIG
};

const unsigned int Platform_numberOfSignals = sizeof(Platform_signals)/sizeof(SignalItem);

ProcessField Platform_defaultFields[] = { HTOP_PID_FIELD, HTOP_EFFECTIVE_USER_FIELD, HTOP_PRIORITY_FIELD, HTOP_NICE_FIELD, HTOP_M_SIZE_FIELD, HTOP_M_RESIDENT_FIELD, HTOP_STATE_FIELD, HTOP_PERCENT_CPU_FIELD, HTOP_PERCENT_MEM_FIELD, HTOP_TIME_FIELD, HTOP_NAME_FIELD, HTOP_COMM_FIELD, 0 };

int Platform_numberOfFields = HTOP_LAST_PROCESSFIELD;

MeterClass* Platform_meterTypes[] = {
   &CPUMeter_class,
   &ClockMeter_class,
   &LoadAverageMeter_class,
   &LoadMeter_class,
   &MemoryMeter_class,
   &SwapMeter_class,
   &TasksMeter_class,
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

#if !defined HAVE_SYSTEM_INFO_CACHED_PAGES || !defined HAVE_SYSTEM_INFO_MAX_SWAP_PAGES
bool Platform_getVMStat(struct vm_stat *buffer, size_t size) {
	static bool is_available = true;
	if(!is_available) return false;
	static status_t (*get_system_info_etc)(uint32_t, void *, size_t);
	if(!get_system_info_etc) {
		void *libroot = get_libroot();
		if(!libroot) {
			is_available = false;
			return false;
		}
		*(void **)&get_system_info_etc = dlsym(libroot, "_kern_get_system_info_etc");
		if(!get_system_info_etc) {
			is_available = false;
			return false;
		}
	}
	return get_system_info_etc(B_MEMORY_INFO, buffer, size) == B_OK;
}
#endif

void Platform_setBindings(Htop_Action* keys) {
   (void) keys;
}

int Platform_getUptime() {
	return system_time() / 1000000;
}

void Platform_getLoadAverage(double* one, double* five, double* fifteen) {
	*one = 0;
	*five = 0;
	*fifteen = 0;
}

int Platform_getMaxPid() {
	return INT32_MAX;
}

double Platform_setCPUValues(Meter *meter, int cpu) {
	const HaikuProcessList *pl = (const HaikuProcessList *)meter->pl;
	if(cpu || pl->super.cpuCount == 1) {
		struct cpu_data *data = pl->cpu_data + (cpu ? cpu - 1 : 0);
		meter->values[CPU_METER_NORMAL] = data->period / (double)pl->interval * 100;
	} else {
		double total_cpu_time = 0;
		for(int i = 0; i < pl->super.cpuCount; i++) {
			total_cpu_time += pl->cpu_data[i].period;
		}
		meter->values[CPU_METER_NORMAL] =
			total_cpu_time / (double)pl->super.cpuCount / (double)pl->interval * 100;
	}
	return meter->values[CPU_METER_NORMAL];
}

void Platform_setMemoryValues(Meter *meter) {
	meter->total = meter->pl->totalMem;
	meter->values[0] = meter->pl->usedMem;
	meter->values[1] = meter->pl->buffersMem;
	meter->values[2] = meter->pl->cachedMem;
}

void Platform_setSwapValues(Meter *meter) {
	meter->total = meter->pl->totalSwap;
	meter->values[0] = meter->pl->usedSwap;
}

char **Platform_getProcessArgv(const Process *proc) {
	return NULL;
}

char **Platform_getProcessEnvv(const Process *proc) {
	return NULL;
}

bool Platform_haveSwap() {
#ifdef HAVE_SYSTEM_INFO_MAX_SWAP_PAGES
	system_info si;
	get_system_info(&si);
	return si.max_swap_pages > 0;
#else
	struct vm_stat st;
	if(!Platform_getVMStat(&st, sizeof st)) return false;
	return st.max_swap_space > 0;
#endif
}

#ifndef HAVE_GETPRIORITY
int getpriority(int which, int who) {
	if(which != PRIO_PROCESS) {
		errno = EINVAL;
		return -1;
	}
	thread_info info;
	status_t status = get_thread_info(who, &info);
	if(status != B_OK) {
		errno = status;
		return -1;
	}
	return B_NORMAL_PRIORITY - info.priority;
}
#endif

#ifndef HAVE_SETPRIORITY
int setpriority(int which, int who, int value) {
	if(which != PRIO_PROCESS) {
		errno = EINVAL;
		return -1;
	}
	status_t status = set_thread_priority(who, B_NORMAL_PRIORITY - value);
	if(status != B_OK) {
		errno = status;
		return -1;
	}
	return 0;
}
#endif
