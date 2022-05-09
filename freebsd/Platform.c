/*
htop - freebsd/Platform.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#include "Platform.h"
#include "Meter.h"
#include "CPUMeter.h"
#include "MemoryMeter.h"
#include "SwapMeter.h"
#include "TasksMeter.h"
#include "LoadAverageMeter.h"
#include "UptimeMeter.h"
#include "ClockMeter.h"
#include "HostnameMeter.h"
#include "FreeBSDProcess.h"
#include "FreeBSDProcessList.h"
#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <vm/vm_param.h>
#if !defined KERN_PROC_ENV && defined HAVE_LIBKVM
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#endif
#include <string.h>
#include <time.h>
#include <math.h>

/*{
#include "Action.h"
#include "BatteryMeter.h"
#include "SignalsPanel.h"

extern ProcessFieldData Process_fields[];

}*/

#ifndef CLAMP
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))
#endif

ProcessField Platform_defaultFields[] = { PID, EFFECTIVE_USER, PRIORITY, NICE, M_SIZE, M_RESIDENT, STATE, PERCENT_CPU, PERCENT_MEM, TIME, COMM, 0 };

int Platform_numberOfFields = LAST_PROCESSFIELD;

const SignalItem Platform_signals[] = {
   { .name = " 0 Cancel",    .number =  0 },
   { .name = " 1 SIGHUP",    .number =  1 },
   { .name = " 2 SIGINT",    .number =  2 },
   { .name = " 3 SIGQUIT",   .number =  3 },
   { .name = " 4 SIGILL",    .number =  4 },
   { .name = " 5 SIGTRAP",   .number =  5 },
   { .name = " 6 SIGABRT",   .number =  6 },
   { .name = " 7 SIGEMT",    .number =  7 },
   { .name = " 8 SIGFPE",    .number =  8 },
   { .name = " 9 SIGKILL",   .number =  9 },
   { .name = "10 SIGBUS",    .number = 10 },
   { .name = "11 SIGSEGV",   .number = 11 },
   { .name = "12 SIGSYS",    .number = 12 },
   { .name = "13 SIGPIPE",   .number = 13 },
   { .name = "14 SIGALRM",   .number = 14 },
   { .name = "15 SIGTERM",   .number = 15 },
   { .name = "16 SIGURG",    .number = 16 },
   { .name = "17 SIGSTOP",   .number = 17 },
   { .name = "18 SIGTSTP",   .number = 18 },
   { .name = "19 SIGCONT",   .number = 19 },
   { .name = "20 SIGCHLD",   .number = 20 },
   { .name = "21 SIGTTIN",   .number = 21 },
   { .name = "22 SIGTTOU",   .number = 22 },
   { .name = "23 SIGIO",     .number = 23 },
   { .name = "24 SIGXCPU",   .number = 24 },
   { .name = "25 SIGXFSZ",   .number = 25 },
   { .name = "26 SIGVTALRM", .number = 26 },
   { .name = "27 SIGPROF",   .number = 27 },
   { .name = "28 SIGWINCH",  .number = 28 },
   { .name = "29 SIGINFO",   .number = 29 },
   { .name = "30 SIGUSR1",   .number = 30 },
   { .name = "31 SIGUSR2",   .number = 31 },
   { .name = "32 SIGTHR",    .number = 32 },
   { .name = "33 SIGLIBRT",  .number = 33 },
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

int Platform_getUptime() {
   struct timeval bootTime, currTime;
   int mib[2] = { CTL_KERN, KERN_BOOTTIME };
   size_t size = sizeof(bootTime);

   int err = sysctl(mib, 2, &bootTime, &size, NULL, 0);
   if (err) {
      return -1;
   }
   gettimeofday(&currTime, NULL);

   return (int) difftime(currTime.tv_sec, bootTime.tv_sec);
}

void Platform_getLoadAverage(double* one, double* five, double* fifteen) {
   struct loadavg loadAverage;
   int mib[2] = { CTL_VM, VM_LOADAVG };
   size_t size = sizeof(loadAverage);

   int err = sysctl(mib, 2, &loadAverage, &size, NULL, 0);
   if (err) {
      *one = 0;
      *five = 0;
      *fifteen = 0;
      return;
   }
   *one     = (double) loadAverage.ldavg[0] / loadAverage.fscale;
   *five    = (double) loadAverage.ldavg[1] / loadAverage.fscale;
   *fifteen = (double) loadAverage.ldavg[2] / loadAverage.fscale;
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

double Platform_setCPUValues(Meter* this, int cpu) {
   FreeBSDProcessList* fpl = (FreeBSDProcessList*) this->pl;
   int cpus = this->pl->cpuCount;
   CPUData* cpuData;

   if (cpus == 1) {
     // single CPU box has everything in fpl->cpus[0]
     cpuData = &(fpl->cpus[0]);
   } else {
     cpuData = &(fpl->cpus[cpu]);
   }

   double  percent;
   double* v = this->values;

   v[CPU_METER_NICE]   = cpuData->nicePercent;
   v[CPU_METER_NORMAL] = cpuData->userPercent;
   if (this->pl->settings->detailedCPUTime) {
      v[CPU_METER_KERNEL]  = cpuData->systemPercent;
      v[CPU_METER_IRQ]     = cpuData->irqPercent;
      Meter_setItems(this, 4);
      percent = v[0]+v[1]+v[2]+v[3];
   } else {
      v[2] = cpuData->systemAllPercent;
      Meter_setItems(this, 3);
      percent = v[0]+v[1]+v[2];
   }

   percent = CLAMP(percent, 0.0, 100.0);
   if (isnan(percent)) percent = 0.0;
   return percent;
}

void Platform_setMemoryValues(Meter* this) {
   // TODO
   ProcessList* pl = (ProcessList*) this->pl;

   this->total = pl->totalMem;
   this->values[0] = pl->usedMem;
   this->values[1] = pl->buffersMem;
   this->values[2] = pl->cachedMem;
}

void Platform_setSwapValues(Meter* this) {
   ProcessList* pl = (ProcessList*) this->pl;
   this->total = pl->totalSwap;
   this->values[0] = pl->usedSwap;
}

char **Platform_getProcessEnv(Process *proc) {
#ifdef KERN_PROC_ENV
	static int arg_max;
	int mib[4] = { CTL_KERN };
	size_t len;
	if(!arg_max) {
		mib[1] = KERN_ARGMAX;
		len = sizeof arg_max;
		if(sysctl(mib, 2, &arg_max, &len, NULL, 0) < 0) arg_max = ARG_MAX;
	}

	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_ENV;
	mib[3] = proc->pid;
	char *buffer = xMalloc(arg_max);
	len = arg_max;
	if(sysctl(mib, 4, buffer, &len, NULL, 0) < 0) {
		free(buffer);
		return NULL;
	}
	char *p = buffer;
	char *end_p = p + len;
	char **env = xMalloc(sizeof(char *));
	unsigned int i = 0;
	while(p < end_p && *p) {
		len = strlen(p) + 1;
		env[i] = xMalloc(len);
		memcpy(env[i], p, len);
		env = xRealloc(env, (++i + 1) * sizeof(char *));
		p += len;
	}
	free(buffer);
	env[i] = NULL;
	return env;
#elif defined HAVE_LIBKVM
	char **env = xMalloc(2 * sizeof(char *));
	env[1] = NULL;
	char errmsg[_POSIX2_LINE_MAX];
	kvm_t *kvm = kvm_openfiles(NULL, "/dev/null", NULL, 0, errmsg);
	if(!kvm) {
		env[0] = xStrdup(errmsg);
		return env;
	}
	int count;
	struct kinfo_proc *kip = kvm_getprocs(kvm, KERN_PROC_PID, proc->pid, &count);
	if(!kip || count < 1) {
		const char *e = kip ? NULL : kvm_geterr(kvm);
		if(e && *e) {
			env[0] = xStrdup(e);
		} else {
			free(env);
			env = NULL;
		}
		kvm_close(kvm);
		return env;
	}
	char **v = kvm_getenvv(kvm, kip, 0);
	if(!v) {
		const char *e = kvm_geterr(kvm);
		if(*e) {
			env[0] = xStrdup(e);
		} else {
			free(env);
			env = NULL;
		}
		kvm_close(kvm);
		return env;
	}
	free(env);
	count = 0;
	while(v[count]) count++;
	env = xMalloc((count + 1) * sizeof(char *));
	env[count] = NULL;
	while(count-- > 0) env[count] = xStrdup(v[count]);
	kvm_close(kvm);
	return env;
#else
	return NULL;
#endif
}
