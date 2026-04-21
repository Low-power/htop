/*
htop - freebsd/Platform.c
(C) 2014 Hisham H. Muhammad
Copyright 2022-2026 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include <bsd/Platform.h>
#include <Action.h>
#include <BatteryMeter.h>
#include <SignalsPanel.h>

typedef struct {
	int hw_physmem_mib[2];
	int vm_stats_vm_v_page_count_mib[4];
	int vm_stats_vm_v_wire_count_mib[4];
	int vm_stats_vm_v_active_count_mib[4];
	int *vm_stats_vm_v_cache_count_mib;
	int vm_stats_vm_v_inactive_count_mib[4];
	int *vm_stats_vm_v_laundry_count_mib;
	int vfs_bufspace_mib[2];
	int kern_cp_time_mib[2];
	int kern_cp_times_mib[2];
#ifndef HAVE_LIBKVM
	int *vm_swap_info_mib;
#endif
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
#include <FreeBSDProcess.h>
#include <FreeBSDProcessList.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <vm/vm_param.h>
#include <signal.h>
#if !defined KERN_PROC_ENV && defined HAVE_LIBKVM
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#endif
#include <string.h>
#include <time.h>
#include <math.h>
#include <assert.h>

#ifndef CLAMP
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))
#endif

GlobalPlatformData platform;

void Platform_init() {
   int mib[4];
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
   if(sysctlnametomib("vm.stats.vm.v_cache_count", mib, &len) == 0) {
      assert(len == 4);
      platform.vm_stats_vm_v_cache_count_mib = xMalloc(4 * sizeof(int));
      memcpy(platform.vm_stats_vm_v_cache_count_mib, mib, 4 * sizeof(int));
   }

   len = 4;
   sysctlnametomib("vm.stats.vm.v_inactive_count", platform.vm_stats_vm_v_inactive_count_mib, &len);
   //len = 4;
   //sysctlnametomib("vm.stats.vm.v_free_count", platform.vm_stats_vm_v_free_count_mib, &len);

   len = 2;
   sysctlnametomib("vfs.bufspace", platform.vfs_bufspace_mib, &len);

   len = 4;
   if(sysctlnametomib("vm.stats.vm.v_laundry_count", mib, &len) == 0) {
      assert(len == 4);
      platform.vm_stats_vm_v_laundry_count_mib = xMalloc(4 * sizeof(int));
      memcpy(platform.vm_stats_vm_v_laundry_count_mib, mib, 4 * sizeof(int));
   }

#ifndef HAVE_LIBKVM
   len = 2;
   if(sysctlnametomib("vm.swap_info", mib, &len) == 0) {
      assert(len == 2);
      //platform.vm_swap_info_mib = xMalloc(sizeof(int) * 3);
      platform.vm_swap_info_mib = xMalloc(sizeof(int) * 2);
      memcpy(platform.vm_swap_info_mib, mib, sizeof(int) * 2);
   }
#endif

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
#ifdef SIGTHR
   SIG(THR),
#endif
#ifdef SIGLIBRT
   SIG(LIBRT),
#endif
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

void Platform_getLoadAverage(double *values) {
   int mib[2] = { CTL_VM, VM_LOADAVG };
   struct loadavg loadavg;
   size_t size = sizeof loadavg;
   assert(sizeof loadavg.ldavg / sizeof *loadavg.ldavg >= 3);
   if(sysctl(mib, 2, &loadavg, &size, NULL, 0) < 0) {
      values[0] = 0;
      values[1] = 0;
      values[2] = 0;
   } else {
      values[0] = (double)loadavg.ldavg[0] / loadavg.fscale;
      values[1] = (double)loadavg.ldavg[1] / loadavg.fscale;
      values[2] = (double)loadavg.ldavg[2] / loadavg.fscale;
   }
}

int Platform_getMaxPid() {
   int maxPid;
   size_t size = sizeof(maxPid);
   if(sysctlbyname("kern.pid_max", &maxPid, &size, NULL, 0) < 0) {
#ifdef PID_MAX
      return PID_MAX;
#else
      return 99999;
#endif
   }
   return maxPid;
}

double Platform_updateCPUValues(Meter *meter, int cpu) {
   const CPUData *cpuData =
      ((FreeBSDProcessList *)meter->pl)->cpus + (meter->pl->cpuCount > 1 ? cpu : 0);
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
   const FreeBSDProcessList *fbsd_pl = (const FreeBSDProcessList *)pl;
   meter->total = pl->totalMem;
   meter->values[0] = pl->usedMem;
   meter->values[1] = 0;
   meter->values[2] = fbsd_pl->vfs_buffer_size + pl->cachedMem;
}

void Platform_updateSwapValues(Meter *meter) {
   const ProcessList *pl = meter->pl;
   meter->total = pl->totalSwap;
   meter->values[0] = pl->usedSwap;
}

#if defined KERN_PROC_ARGS || defined KERN_PROC_ENV
static char **get_process_vector_from_sysctl(const Process *proc, int v_type) {
	static int arg_max;
	int mib[4] = { CTL_KERN };
	size_t len;
	if(!arg_max) {
		mib[1] = KERN_ARGMAX;
		len = sizeof arg_max;
		if(sysctl(mib, 2, &arg_max, &len, NULL, 0) < 0) arg_max = ARG_MAX;
	}

	mib[1] = KERN_PROC;
	mib[2] = v_type;
	mib[3] = proc->pid;
	char *buffer = xMalloc(arg_max);
	len = arg_max;
	if(sysctl(mib, 4, buffer, &len, NULL, 0) < 0) {
		free(buffer);
		return NULL;
	}
	char *p = buffer;
	char *end_p = p + len;
	char **v = xMalloc(sizeof(char *));
	unsigned int i = 0;
	while(p < end_p) {
		len = strlen(p) + 1;
		v[i] = xMalloc(len);
		memcpy(v[i], p, len);
		v = xRealloc(v, (++i + 1) * sizeof(char *));
		p += len;
	}
	free(buffer);
	v[i] = NULL;
	return v;
}
#endif

#if (!defined KERN_PROC_ARGS || !defined KERN_PROC_ENV) && defined HAVE_LIBKVM
static char **get_process_vector_from_kvm(const Process *proc, char **(*getv)(kvm_t *, const struct kinfo_proc *, int)) {
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
#endif

char **Platform_getProcessArgv(const Process *proc) {
#ifdef KERN_PROC_ARGS
	return get_process_vector_from_sysctl(proc, KERN_PROC_ARGS);
#elif defined HAVE_LIBKVM
	return get_process_vector_from_kvm(proc, kvm_getargv);
#else
	return NULL;
#endif
}

char **Platform_getProcessEnvv(const Process *proc) {
#ifdef KERN_PROC_ENV
	return get_process_vector_from_sysctl(proc, KERN_PROC_ENV);
#elif defined HAVE_LIBKVM
	return get_process_vector_from_kvm(proc, kvm_getenvv);
#else
	return NULL;
#endif
}

bool Platform_haveSwap() {
	int nswapdev;
	size_t len = sizeof nswapdev;
	if(sysctlbyname("vm.nswapdev", &nswapdev, &len, NULL, 0) < 0) return false;
	return nswapdev > 0;
}
