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
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/swap.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <kvm.h>
#include <limits.h>

/*{
#include "Action.h"
#include "BatteryMeter.h"
#include "SignalsPanel.h"

extern ProcessFieldData Process_fields[];

}*/

#define MAXCPU 256
// XXX: probably should be a struct member
static uint64_t old_v[1+MAXCPU][5];

/*
 * Copyright (c) 1984, 1989, William LeFebvre, Rice University
 * Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 *
 * Taken directly from OpenBSD's top(1).
 *
 * percentages(cnt, out, new, old, diffs) - calculate percentage change
 * between array "old" and "new", putting the percentages in "out".
 * "cnt" is size of each array and "diffs" is used for scratch space.
 * The array "old" is updated on each call.
 * The routine assumes modulo arithmetic.  This function is especially
 * useful on BSD machines for calculating cpu state percentages.
 */
static void percentages(int cnt, uint64_t *out, uint64_t *new, uint64_t *old, uint64_t *diffs) {
   uint64_t change, total_change, *dp, half_total;
   int i;

   /* initialization */
   total_change = 0;
   dp = diffs;

   /* calculate changes for each state and the overall change */
   for (i = 0; i < cnt; i++) {
      if(*new < *old) {
         /* this only happens when the counter wraps */
         change = UINT64_MAX - *old + *new;
      } else {
         change = (int64_t)(*new - *old);
      }
      total_change += (*dp++ = change);
      *old++ = *new++;
   }

   /* avoid divide by zero potential */
   if (total_change == 0)
      total_change = 1;

   /* calculate percentages based on overall change, rounding up */
   half_total = total_change / 2l;
   for (i = 0; i < cnt; i++)
      *out++ = ((*diffs++ * 1000 + half_total) / total_change);

   /* return the total in case the caller wants to use it */
   //return (total_change);
}

ProcessField Platform_defaultFields[] = { PID, EFFECTIVE_USER, PRIORITY, NICE, M_SIZE, M_RESIDENT, STATE, PERCENT_CPU, PERCENT_MEM, TIME, COMM, 0 };

int Platform_numberOfFields = LAST_PROCESSFIELD;

/*
 * See /usr/include/sys/signal.h
 */
const SignalItem Platform_signals[] = {
   { .name = " 0 Cancel",    .number =  0 },
   { .name = " 1 SIGHUP",    .number =  1 },
   { .name = " 2 SIGINT",    .number =  2 },
   { .name = " 3 SIGQUIT",   .number =  3 },
   { .name = " 4 SIGILL",    .number =  4 },
   { .name = " 5 SIGTRAP",   .number =  5 },
   { .name = " 6 SIGABRT",   .number =  6 },
   { .name = " 6 SIGIOT",    .number =  6 },
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

// preserved from FreeBSD port
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
   // this is hard-coded in sys/sys/proc.h - no sysctl exists
#ifdef TID_MASK
   return TID_MASK + 1;
#elif defined PID_MAX
   return PID_MAX;
#else
   return 32766;
#endif
}

double Platform_setCPUValues(Meter *meter, int cpu) {
   int i;
   OpenBSDProcessList* pl = (OpenBSDProcessList*)meter->pl;
   CPUData* cpuData = pl->cpus + cpu;
   uint64_t new_v[CPUSTATES], diff_v[CPUSTATES], scratch_v[CPUSTATES];
   double *v = meter->values;
   if(cpu) {
      int mib[] = { CTL_KERN, KERN_CPTIME2, cpu-1 };
      size_t size = sizeof new_v;
      if (sysctl(mib, 3, new_v, &size, NULL, 0) == -1) return 0;
   } else {
      int mib[] = { CTL_KERN, KERN_CPTIME };
      long int new_avg_v[CPUSTATES];
      size_t size = sizeof new_avg_v;
      if(sysctl(mib, 2, new_avg_v, &size, NULL, 0) < 0) return 0;
      for(i = 0; i < CPUSTATES; i++) new_v[i] = new_avg_v[i];
   }

   // XXX: why?
   cpuData->totalPeriod = 1;

   percentages(CPUSTATES, diff_v, new_v,
         old_v[cpu], scratch_v);

   for (i = 0; i < CPUSTATES; i++) {
      old_v[cpu][i] = new_v[i];
      v[i] = diff_v[i] / 10.;
   }

   Meter_setItems(meter, 4);

   double perc = v[0] + v[1] + v[2] + v[3];

   if (perc <= 100. && perc >= 0.) {
      return perc;
   } else {
      return 0.;
   }
}

void Platform_setMemoryValues(Meter* this) {
   ProcessList* pl = (ProcessList*) this->pl;
   long int usedMem = pl->usedMem;
   long int buffersMem = pl->buffersMem;
   long int cachedMem = pl->cachedMem;
   usedMem -= buffersMem + cachedMem;
   this->total = pl->totalMem;
   this->values[0] = usedMem;
   this->values[1] = buffersMem;
   this->values[2] = cachedMem;
}

/*
 * Copyright (c) 1994 Thorsten Lockert <tholo@sigmasoft.com>
 * All rights reserved.
 *
 * Based on OpenBSD's top(1)
 */
void Platform_setSwapValues(Meter *meter) {
   ProcessList* pl = (ProcessList *)meter->pl;
   unsigned long long int total = 0, used = 0;
   int nswap = swapctl(SWAP_NSWAP, 0, 0);
   if (nswap > 0) {
      struct swapent *swdev = xCalloc(nswap, sizeof(*swdev));
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
   meter->total = pl->totalSwap = total;
   meter->values[0] = pl->usedSwap = used;
}

char **Platform_getProcessEnv(Process *proc) {
   char errbuf[_POSIX2_LINE_MAX];
   kvm_t *kvm = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
   if(!kvm) return NULL;

   int count;
   struct kinfo_proc *kproc = kvm_getprocs(kvm,
#ifdef KERN_PROC_SHOW_THREADS
      KERN_PROC_SHOW_THREADS |
#endif
      KERN_PROC_PID, proc->tgid, sizeof(struct kinfo_proc), &count);
   if(!kproc) {
      kvm_close(kvm);
      return NULL;
   }
#ifdef HAVE_STRUCT_KINFO_PROC_P_TID
   while(count > 0 && kproc->p_tid - THREAD_PID_OFFSET != proc->pid) {
      count--;
      kproc++;
   }
#endif
   if(count < 1) return NULL;

   char **ptr = kvm_getenvv(kvm, kproc, 0);
   if(!ptr) {
      kvm_close(kvm);
      return NULL;
   }

   count = 0;
   while(ptr[count]) count++;
   char **env = xMalloc((count + 1) * sizeof(char *));
   env[count] = NULL;
   while(count-- > 0) env[count] = xStrdup(ptr[count]);
   kvm_close(kvm);
   return env;
}
