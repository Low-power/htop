/*
htop - openbsd/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
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
#include "SignalsPanel.h"
#include "OpenBSDProcess.h"
#include "OpenBSDProcessList.h"

#ifdef SAFE_TO_DEFINE_KERNEL
#define _KERNEL
struct proc;
#endif
#include <sys/param.h>
#include <sys/proc.h>
#ifdef SAFE_TO_DEFINE_KERNEL
#undef _KERNEL
#endif
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/swap.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <kvm.h>
#include <limits.h>
#include <math.h>

/*{
#include "bsd/Platform.h"
#include "Action.h"
#include "BatteryMeter.h"
#include "SignalsPanel.h"

#define PLATFORM_SUPPORT_PROCESS_O_STATE

extern ProcessFieldData Process_fields[];

}*/

ProcessField Platform_defaultFields[] = { HTOP_PID_FIELD, HTOP_EFFECTIVE_USER_FIELD, HTOP_PRIORITY_FIELD, HTOP_NICE_FIELD, HTOP_M_SIZE_FIELD, HTOP_M_RESIDENT_FIELD, HTOP_STATE_FIELD, HTOP_PERCENT_CPU_FIELD, HTOP_PERCENT_MEM_FIELD, HTOP_TIME_FIELD, HTOP_COMM_FIELD, 0 };

int Platform_numberOfFields = HTOP_LAST_PROCESSFIELD;

/*
 * See /usr/include/sys/signal.h
 */
const SignalItem Platform_signals[] = {
   { .name = "Cancel", .number = 0 },
#define SIG(NAME) { .name = #NAME, .number = SIG##NAME }
   SIG(HUP),
   SIG(INT),
   SIG(QUIT),
   SIG(ILL),
   SIG(TRAP),
   SIG(ABRT),
   SIG(IOT),
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
   // this is hard-coded in sys/sys/proc.h - no sysctl exists
#ifdef TID_MASK
   return TID_MASK + 1;
#elif defined PID_MAX
   return PID_MAX;
#else
   return 32766;
#endif
}

double Platform_updateCPUValues(Meter *meter, int cpu) {
   const OpenBSDProcessList *pl = (const OpenBSDProcessList *)meter->pl;
   const CPUData *cpuData = pl->cpus + cpu;
   double total = cpuData->totalPeriod == 0 ? 1 : cpuData->totalPeriod;
   double totalPercent;
   double *v = meter->values;
   v[CPU_METER_NICE] = cpuData->nicePeriod / total * 100.0;
   v[CPU_METER_NORMAL] = cpuData->userPeriod / total * 100.0;
   if (meter->pl->settings->detailedCPUTime) {
      v[CPU_METER_KERNEL]  = cpuData->sysPeriod / total * 100.0;
      v[CPU_METER_IRQ]     = cpuData->intrPeriod / total * 100.0;
      v[CPU_METER_SOFTIRQ] = 0.0;
      v[CPU_METER_STEAL]   = 0.0;
      v[CPU_METER_GUEST]   = 0.0;
      v[CPU_METER_IOWAIT]  = 0.0;
      Meter_setItems(meter, 8);
      totalPercent = v[0]+v[1]+v[2]+v[3];
   } else {
      v[2] = cpuData->sysAllPeriod / total * 100.0;
      v[3] = 0.0; // No steal nor guest on OpenBSD
      totalPercent = v[0]+v[1]+v[2];
      Meter_setItems(meter, 4);
   }

   totalPercent = CLAMP(totalPercent, 0.0, 100.0);
   if (isnan(totalPercent)) totalPercent = 0.0;
   return totalPercent;
}

void Platform_updateMemoryValues(Meter *meter) {
   const ProcessList *pl = meter->pl;
   long int usedMem = pl->usedMem;
   long int buffersMem = pl->buffersMem;
   long int cachedMem = pl->cachedMem;
   usedMem -= buffersMem + cachedMem;
   meter->total = pl->totalMem;
   meter->values[0] = usedMem;
   meter->values[1] = buffersMem;
   meter->values[2] = cachedMem;
}

/*
 * Copyright (c) 1994 Thorsten Lockert <tholo@sigmasoft.com>
 * All rights reserved.
 *
 * Based on OpenBSD's top(1)
 */
void Platform_updateSwapValues(Meter *meter) {
   unsigned long long int total = 0, used = 0;
   int nswap = swapctl(SWAP_NSWAP, 0, 0);
   if (nswap > 0) {
      struct swapent *swdev = xCalloc(nswap, sizeof(struct swapent));
      nswap = swapctl(SWAP_STATS, swdev, nswap);
      while(nswap > 0) {
         const struct swapent *e = swdev + --nswap;
         if(e->se_flags & SWF_ENABLE) {
            used += e->se_inuse / (1024 / DEV_BSIZE);
            total += e->se_nblks / (1024 / DEV_BSIZE);
         }
      }
      free(swdev);
   }
   meter->total = total;
   meter->values[0] = used;
}

static char **get_process_vector(const Process *proc, char **(*getv)(kvm_t *, const struct kinfo_proc *, int)) {
   char errbuf[_POSIX2_LINE_MAX];
   kvm_t *kvm = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
   if(!kvm) return NULL;

   int count;
   struct kinfo_proc *kproc = kvm_getprocs(kvm,
#ifdef KERN_PROC_SHOW_THREADS
      (
#ifdef PID_AND_MAIN_THREAD_ID_DIFFER
         proc->pid == proc->tgid ? 0 : KERN_PROC_SHOW_THREADS
#else
         KERN_PROC_SHOW_THREADS
#endif
      ) |
#endif
      KERN_PROC_PID, proc->tgid, sizeof(struct kinfo_proc), &count);
   if(!kproc) {
      kvm_close(kvm);
      return NULL;
   }
#ifdef HAVE_STRUCT_KINFO_PROC_P_TID
#ifdef PID_AND_MAIN_THREAD_ID_DIFFER
   if(proc->pid != proc->tgid)
#endif
   while(count > 0 && kproc->p_tid - THREAD_PID_OFFSET != proc->pid) {
      count--;
      kproc++;
   }
#endif
   if(count < 1) {
      kvm_close(kvm);
      return NULL;
   }

   char **ptr = getv(kvm, kproc, 0);
   if(!ptr) {
      kvm_close(kvm);
      return NULL;
   }

   count = 0;
   while(ptr[count]) count++;
   char **v = xMalloc((count + 1) * sizeof(char *));
   v[count] = NULL;
   while(count-- > 0) v[count] = xStrdup(ptr[count]);
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
	return swapctl(SWAP_NSWAP, NULL, 0) > 0;
}
