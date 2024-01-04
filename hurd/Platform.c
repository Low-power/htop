/*
htop - hurd/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2015 David C. Hunt
Copyright 2015-2024 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "Action.h"
#include "BatteryMeter.h"
#include "SignalsPanel.h"
#include <mach/mach_traps.h>

static inline mach_port_t get_host_port() {
	static mach_port_t host = MACH_PORT_NULL;
	if(host == MACH_PORT_NULL) host = mach_host_self();
	return host;
}
}*/

#include "Platform.h"
#include "CPUMeter.h"
#include "MemoryMeter.h"
#include "SwapMeter.h"
#include "TasksMeter.h"
#include "LoadAverageMeter.h"
#include "ClockMeter.h"
#include "HostnameMeter.h"
#include "UptimeMeter.h"
#include "UsersMeter.h"
#include "HurdProcess.h"
#include "HurdProcessList.h"
#include <sys/mman.h>
#include <mach/host_info.h>
#include <mach/mach_host.h>
#include <mach/default_pager.h>
#include <hurd/process.h>
#include <hurd/paths.h>
#include <hurd.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

const SignalItem Platform_signals[] = {
   { .name = "Cancel", .number = 0 },
#define SIG(NAME) { .name = #NAME, .number = SIG##NAME }
   SIG(HUP),
   SIG(INT),
   SIG(QUIT),
   SIG(ILL),
   SIG(TRAP),
   SIG(ABRT),
   SIG(FPE),
   SIG(KILL),
   SIG(BUS),
   SIG(SEGV),
   SIG(SYS),
   SIG(PIPE),
   SIG(ALRM),
   SIG(TERM),
   SIG(URG),
   SIG(STOP),
   SIG(TSTP),
   SIG(CONT),
   SIG(CHLD),
   SIG(TTIN),
   SIG(TTOU),
   SIG(IO),
   SIG(POLL),
   SIG(XCPU),
   SIG(XFSZ),
   SIG(VTALRM),
   SIG(PROF),
   SIG(WINCH),
   SIG(INFO),
   SIG(USR1),
   SIG(USR2),
   SIG(LOST),
#undef SIG
};

const unsigned int Platform_numberOfSignals = sizeof(Platform_signals)/sizeof(SignalItem);

ProcessField Platform_defaultFields[] = { HTOP_PID_FIELD, HTOP_EFFECTIVE_USER_FIELD, HTOP_PRIORITY_FIELD, HTOP_NICE_FIELD, HTOP_M_SIZE_FIELD, HTOP_M_RESIDENT_FIELD, HTOP_STATE_FIELD, HTOP_PERCENT_CPU_FIELD, HTOP_PERCENT_MEM_FIELD, HTOP_TIME_FIELD, HTOP_COMM_FIELD, 0 };

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
	// TODO: get start time of process 1
	return -1;
}

void Platform_getLoadAverage(double* one, double* five, double* fifteen) {
	struct host_load_info load_info;
	size_t size = sizeof load_info;
	error_t e = host_info(get_host_port(), HOST_LOAD_INFO, (host_info_t)&load_info, &size);
	if(e) {
		*one = 0;
		*five = 0;
		*fifteen = 0;
	} else {
		*one = load_info.avenrun[0];
		*one /= LOAD_SCALE;
		*five = load_info.avenrun[1];
		*five /= LOAD_SCALE;
		*fifteen = load_info.avenrun[2];
		*fifteen /= LOAD_SCALE;
	}
}

int Platform_getMaxPid() {
	/* The Hurd process server only have a soft limit for PID values,
	 * which can be raised by creating a lot of processes that would run
	 * out of available PID value in current limit. Using the initial
	 * limit here and hoping that won't happen. */
	return 29999;
}

double Platform_updateCPUValues(Meter *meter, int cpu) {
	assert(cpu >= 0);
	assert(cpu < 2);
	const HurdProcessList *pl = (const HurdProcessList *)meter->pl;
	if(pl->idle_time.tv_sec == -1) return 0;
	meter->values[CPU_METER_NORMAL] = 100 -
		((double)pl->idle_time.tv_sec * 1000000 + pl->idle_time.tv_usec) /
		((double)pl->interval.tv_sec * 1000000 + pl->interval.tv_usec) * 100;
	return meter->values[CPU_METER_NORMAL];
}

void Platform_updateMemoryValues(Meter *meter) {
	meter->total = meter->pl->totalMem;
	meter->values[0] = meter->pl->usedMem;
	meter->values[1] = meter->pl->buffersMem;
	meter->values[2] = meter->pl->cachedMem;
}

void Platform_updateSwapValues(Meter *meter) {
	meter->total = meter->pl->totalSwap;
	meter->values[0] = meter->pl->usedSwap;
}

static char **get_process_vector(const Process *proc, error_t (*getv)(process_t, pid_t, char **, size_t *)) {
	char **v = xMalloc(2 * sizeof(char *));
	char *buffer;
	size_t len = 0;
	error_t e = getv(getproc(), proc->pid, &buffer, &len);
	if(e) {
		v[0] = strdup(strerror(e));
		if(v[0]) {
			v[1] = NULL;
		} else {
			free(v);
			v = NULL;
		}
		return v;
	}
	char *p = buffer;
	char *end_p = p + len;
	unsigned int i = 0;
	while(p < end_p && *p) {
		size_t len = strlen(p) + 1;
		v[i] = xMalloc(len);
		memcpy(v[i], p, len);
		v = xRealloc(v, (++i + 1) * sizeof(char *));
		p += len;
	}
	munmap(buffer, len);
	v[i] = NULL;
	return v;
}

char **Platform_getProcessArgv(const Process *proc) {
	return get_process_vector(proc, proc_getprocargs);
}

char **Platform_getProcessEnvv(const Process *proc) {
	return get_process_vector(proc, proc_getprocenv);
}

bool Platform_haveSwap() {
#ifdef _SERVERS_DEFPAGER
	mach_port_t defpager = file_name_lookup(_SERVERS_DEFPAGER, O_READ, 0);
	if(defpager != MACH_PORT_NULL) {
		struct default_pager_info info;
		error_t e = default_pager_info(defpager, &info);
		bool r = !e && info.dpi_total_space > 0;
		mach_port_deallocate(mach_task_self(), defpager);
		return r;
	}
#endif
	return false;
}
