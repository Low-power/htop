/**
htop - dragonflybsd/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Copyright 2022-2026 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include <bsd/Platform.h>
#include <Action.h>
#include <BatteryMeter.h>
#include <SignalsPanel.h>

#define PLATFORM_HAVE_BUFFERS_MEMORY_CLASS

typedef struct {
	int hw_physmem_mib[2];
	int vm_stats_vm_v_page_count_mib[4];
	int vm_stats_vm_v_wire_count_mib[4];
	int vm_stats_vm_v_active_count_mib[4];
	int vm_stats_vm_v_cache_count_mib[4];
	int vm_stats_vm_v_inactive_count_mib[4];
	int vfs_bufspace_mib[2];
	int kern_cp_time_mib[2];
	int kern_cp_times_mib[2];
} GlobalPlatformData;
}*/

#include "config.h"
#include <Platform.h>
#include <Meter.h>
#include <CPUMeter.h>
#include <MemoryMeter.h>
#include <SwapMeter.h>
#include <TasksMeter.h>
#include <LoadAverageMeter.h>
#include <UptimeMeter.h>
#include <ClockMeter.h>
#include <HostnameMeter.h>
#include <UsersMeter.h>
#include <DragonFlyBSDProcess.h>
#include <DragonFlyBSDProcessList.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/user.h>
#include <sys/blist.h>
#include <signal.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <time.h>
#include <math.h>

#ifndef CLAMP
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))
#endif

GlobalPlatformData platform;

void Platform_init() {
   size_t len;

   // physical memory in system: hw.physmem
   // physical page size: hw.pagesize
   // usable pagesize : vm.stats.vm.v_page_size
   len = 2;
   sysctlnametomib("hw.physmem", platform.hw_physmem_mib, &len);

   // usable page count vm.stats.vm.v_page_count
   // actually usable memory : vm.stats.vm.v_page_count * vm.stats.vm.v_page_size
   len = 4;
   sysctlnametomib("vm.stats.vm.v_page_count", platform.vm_stats_vm_v_page_count_mib, &len);

   len = 4;
   sysctlnametomib("vm.stats.vm.v_wire_count", platform.vm_stats_vm_v_wire_count_mib, &len);
   len = 4;
   sysctlnametomib("vm.stats.vm.v_active_count", platform.vm_stats_vm_v_active_count_mib, &len);
   len = 4;
   sysctlnametomib("vm.stats.vm.v_cache_count", platform.vm_stats_vm_v_cache_count_mib, &len);
   len = 4;
   sysctlnametomib("vm.stats.vm.v_inactive_count", platform.vm_stats_vm_v_inactive_count_mib, &len);
   //len = 4;
   //sysctlnametomib("vm.stats.vm.v_free_count", platform.vm_stats_vm_v_free_count_mib, &len);

   len = 2;
   sysctlnametomib("vfs.bufspace", platform.vfs_bufspace_mib, &len);

   len = 2;
   sysctlnametomib("kern.cp_time", platform.kern_cp_time_mib, &len);
   len = 2;
   sysctlnametomib("kern.cp_times", platform.kern_cp_times_mib, &len);
}

ProcessField Platform_defaultFields[] = { HTOP_PID_FIELD, HTOP_EFFECTIVE_USER_FIELD, HTOP_PRIORITY_FIELD, HTOP_NICE_FIELD, HTOP_M_SIZE_FIELD, HTOP_M_RESIDENT_FIELD, HTOP_STATE_FIELD, HTOP_PERCENT_CPU_FIELD, HTOP_PERCENT_MEM_FIELD, HTOP_TIME_FIELD, HTOP_COMM_FIELD, 0 };

const unsigned int Platform_numberOfFields = HTOP_LAST_PROCESSFIELD;

const SignalItem Platform_signals[] = {
   { .name = "Cancel", .number = 0 },
#define SIG(NAME) { .name = #NAME, .number = SIG##NAME }
   SIG(HUP),
   SIG(INT),
   SIG(QUIT),
   SIG(ILL),
   SIG(TRAP),
   SIG(ABRT),
   SIG(EMT),
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
   SIG(XCPU),
   SIG(XFSZ),
   SIG(VTALRM),
   SIG(PROF),
   SIG(WINCH),
   SIG(INFO),
   SIG(USR1),
   SIG(USR2),
   SIG(THR),
#undef SIG
};

const unsigned int Platform_numberOfSignals = sizeof(Platform_signals)/sizeof(SignalItem);

void Platform_setBindings(Htop_Action* keys) {
   (void) keys;
}

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
   &UptimeMeter_class,
   &BatteryMeter_class,
   &HostnameMeter_class,
   &AllCPUsMeter_class,
   &AllCPUs2Meter_class,
   &LeftCPUsMeter_class,
   &RightCPUsMeter_class,
   &LeftCPUs2Meter_class,
   &RightCPUs2Meter_class,
   &BlankMeter_class,
   NULL
};

int Platform_getMaxPid() {
   return PID_MAX;
}

double Platform_updateCPUValues(Meter *meter, int cpu) {
   const CPUData *cpuData =
      ((DragonFlyBSDProcessList *)meter->pl)->cpus + (meter->pl->cpuCount > 1 ? cpu : 0);
   double percent;
   double *v = meter->values;
   v[CPU_METER_NICE]   = cpuData->nicePercent;
   v[CPU_METER_NORMAL] = cpuData->userPercent;
   if (meter->pl->settings->detailedCPUTime) {
      v[CPU_METER_KERNEL]  = cpuData->systemPercent;
      v[CPU_METER_IRQ]     = cpuData->irqPercent;
      Meter_setItemCount(meter, 4);
      percent = v[0]+v[1]+v[2]+v[3];
   } else {
      v[2] = cpuData->systemAllPercent;
      Meter_setItemCount(meter, 3);
      percent = v[0]+v[1]+v[2];
   }

   percent = CLAMP(percent, 0.0, 100.0);
   if (isnan(percent)) percent = 0.0;
   return percent;
}

void Platform_updateMemoryValues(Meter *meter) {
   const ProcessList *pl = meter->pl;
   meter->total = pl->totalMem;
   meter->values[0] = pl->usedMem;
   meter->values[1] = ((const DragonFlyBSDProcessList *)pl)->buffers_size;
   meter->values[2] = pl->cachedMem;
}

void Platform_updateSwapValues(Meter *meter) {
   const ProcessList *pl = meter->pl;
   meter->total = pl->totalSwap;
   meter->values[0] = pl->usedSwap;
}

static char **get_process_vector(const Process *proc, char **(*getv)(kvm_t *, const struct kinfo_proc *, int)) {
	if(Process_isKernelProcess(proc)) return NULL;

	char **v = xMalloc(2 * sizeof(char *));
	v[1] = NULL;
	char errmsg[_POSIX2_LINE_MAX];
	kvm_t *kvm = kvm_openfiles(NULL, "/dev/null", NULL, 0, errmsg);
	if(!kvm) {
		v[0] = xStrdup(errmsg);
		return v;
	}
	int count;
	struct kinfo_proc *kip = kvm_getprocs(kvm, KERN_PROC_PID, proc->pid, &count);
	if(!kip || count < 1) {
		const char *e = kip ? NULL : kvm_geterr(kvm);
		if(e && *e) {
			v[0] = xStrdup(e);
		} else {
			free(v);
			v = NULL;
		}
		kvm_close(kvm);
		return v;
	}
	char **tmp_v = getv(kvm, kip, 0);
	if(!tmp_v) {
		const char *e = kvm_geterr(kvm);
		if(*e) {
			v[0] = xStrdup(e);
		} else {
			free(v);
			v = NULL;
		}
		kvm_close(kvm);
		return v;
	}
	free(v);
	count = 0;
	while(tmp_v[count]) count++;
	v = xMalloc((count + 1) * sizeof(char *));
	v[count] = NULL;
	while(count-- > 0) v[count] = xStrdup(tmp_v[count]);
	kvm_close(kvm);
	return v;
}

char **Platform_getProcessArgv(const Process *proc) {
	return get_process_vector(proc, kvm_getargv);
}

char **Platform_getProcessEnvv(const Process *proc) {
	return get_process_vector(proc, kvm_getenvv);
}

bool Platform_haveSwap() {
#if defined SWBLK_BITS && SWBLK_BITS == 64
	long int swap_size;
#else
	int swap_size;
#endif
	size_t len = sizeof swap_size;
	if(sysctlbyname("vm.swap_size", &swap_size, &len, NULL, 0) < 0) return false;
	return swap_size > 0;
}
