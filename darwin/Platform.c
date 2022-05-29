/*
htop - darwin/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2015 David C. Hunt
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
#include "DarwinProcessList.h"
#include <signal.h>
#include <stdlib.h>

/*{
#include "bsd/Platform.h"
#include "Action.h"
#include "SignalsPanel.h"
#include "CPUMeter.h"
#include "BatteryMeter.h"
#include "DarwinProcess.h"
}*/

#ifndef CLAMP
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))
#endif

ProcessField Platform_defaultFields[] = { HTOP_PID_FIELD, HTOP_EFFECTIVE_USER_FIELD, HTOP_PRIORITY_FIELD, HTOP_NICE_FIELD, HTOP_M_SIZE_FIELD, HTOP_M_RESIDENT_FIELD, HTOP_STATE_FIELD, HTOP_PERCENT_CPU_FIELD, HTOP_PERCENT_MEM_FIELD, HTOP_TIME_FIELD, HTOP_COMM_FIELD, 0 };

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
#undef SIG
};

const unsigned int Platform_numberOfSignals = sizeof(Platform_signals)/sizeof(SignalItem);

ProcessFieldData Process_fields[] = {
   [0] = { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_PID_FIELD] = { .name = "PID", .title = "    PID ", .description = "Process/thread ID", .flags = 0, },
   [HTOP_NAME_FIELD] = { .name = "NAME", .title = "NAME            ", .description = "Process (executable) name", .flags = 0, },
   [HTOP_COMM_FIELD] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [HTOP_STATE_FIELD] = { .name = "STATE", .title = "S ", .description = "Process state (S sleeping, R running, D disk, Z zombie, T stoppd, W paging)", .flags = 0, },
   [HTOP_PPID_FIELD] = { .name = "PPID", .title = "   PPID ", .description = "Parent process ID", .flags = 0, },
   [HTOP_PGRP_FIELD] = { .name = "PGRP", .title = "   PGRP ", .description = "Process group ID", .flags = 0, },
   [HTOP_SESSION_FIELD] = { .name = "SESSION", .title = "    SID ", .description = "Process's session ID", .flags = 0, },
   [HTOP_TTY_FIELD] = { .name = "TTY", .title = "TTY     ", .description = "Controlling terminal", .flags = 0, },
   [HTOP_TPGID_FIELD] = { .name = "TPGID", .title = "  TPGID ", .description = "Process ID of the fg process group of the controlling terminal", .flags = 0, },
   [HTOP_MINFLT_FIELD] = { .name = "MINFLT", .title = "     MINFLT ", .description = "Number of minor faults which have not required loading a memory page from disk", .flags = 0, },
   [HTOP_MAJFLT_FIELD] = { .name = "MAJFLT", .title = "     MAJFLT ", .description = "Number of major faults which have required loading a memory page from disk", .flags = 0, },
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
   [HTOP_TGID_FIELD] = { .name = "TGID", .title = "   TGID ", .description = "Thread group ID (i.e. process ID)", .flags = 0, },
   [100] = { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0, },
};

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

void Platform_setBindings(Htop_Action* keys) {
   (void) keys;
}

int Platform_numberOfFields = 100;

void Platform_getLoadAverage(double* one, double* five, double* fifteen) {
   double results[3];

   if(3 == getloadavg(results, 3)) {
      *one = results[0];
      *five = results[1];
      *fifteen = results[2];
   } else {
      *one = 0;
      *five = 0;
      *fifteen = 0;
   }
}

int Platform_getMaxPid() {
   /* http://opensource.apple.com/source/xnu/xnu-2782.1.97/bsd/sys/proc_internal.hh */
   return 99999;
}

ProcessPidColumn Process_pidColumns[] = {
   { .id = HTOP_PID_FIELD, .label = "PID" },
   { .id = HTOP_PPID_FIELD, .label = "PPID" },
   { .id = HTOP_TPGID_FIELD, .label = "TPGID" },
   { .id = HTOP_TGID_FIELD, .label = "TGID" },
   { .id = HTOP_PGRP_FIELD, .label = "PGRP" },
   { .id = HTOP_SESSION_FIELD, .label = "SID" },
   { .id = 0, .label = NULL },
};

static double Platform_setCPUAverageValues(Meter* mtr) {
   DarwinProcessList *dpl = (DarwinProcessList *)mtr->pl;
   int cpus = dpl->super.cpuCount;
   double sumNice = 0.0;
   double sumNormal = 0.0;
   double sumKernel = 0.0;
   double sumPercent = 0.0;
   for (int i = 1; i <= cpus; i++) {
      sumPercent += Platform_setCPUValues(mtr, i);
      sumNice    += mtr->values[CPU_METER_NICE];
      sumNormal  += mtr->values[CPU_METER_NORMAL];
      sumKernel  += mtr->values[CPU_METER_KERNEL];
   }
   mtr->values[CPU_METER_NICE]   = sumNice   / cpus;
   mtr->values[CPU_METER_NORMAL] = sumNormal / cpus;
   mtr->values[CPU_METER_KERNEL] = sumKernel / cpus;
   return sumPercent / cpus;
}

double Platform_setCPUValues(Meter* mtr, int cpu) {

   if (cpu == 0) {
      return Platform_setCPUAverageValues(mtr);
   }

   DarwinProcessList *dpl = (DarwinProcessList *)mtr->pl;
   processor_cpu_load_info_t prev = &dpl->prev_load[cpu-1];
   processor_cpu_load_info_t curr = &dpl->curr_load[cpu-1];
   double total = 0;

   /* Take the sums */
   for(size_t i = 0; i < CPU_STATE_MAX; ++i) {
      total += (double)curr->cpu_ticks[i] - (double)prev->cpu_ticks[i];
   }

   mtr->values[CPU_METER_NICE]
           = ((double)curr->cpu_ticks[CPU_STATE_NICE] - (double)prev->cpu_ticks[CPU_STATE_NICE])* 100.0 / total;
   mtr->values[CPU_METER_NORMAL]
           = ((double)curr->cpu_ticks[CPU_STATE_USER] - (double)prev->cpu_ticks[CPU_STATE_USER])* 100.0 / total;
   mtr->values[CPU_METER_KERNEL]
           = ((double)curr->cpu_ticks[CPU_STATE_SYSTEM] - (double)prev->cpu_ticks[CPU_STATE_SYSTEM])* 100.0 / total;

   Meter_setItems(mtr, 3);

   /* Convert to percent and return */
   total = mtr->values[CPU_METER_NICE] + mtr->values[CPU_METER_NORMAL] + mtr->values[CPU_METER_KERNEL];

   return CLAMP(total, 0.0, 100.0);
}

void Platform_setMemoryValues(Meter* mtr) {
   DarwinProcessList *dpl = (DarwinProcessList *)mtr->pl;
   vm_statistics_t vm = &dpl->vm_stats;
   double page_K = (double)vm_page_size / (double)1024;

   mtr->total = dpl->host_info.max_mem / 1024;
   mtr->values[0] = (double)(vm->active_count + vm->wire_count) * page_K;
   mtr->values[1] = (double)vm->purgeable_count * page_K;
   mtr->values[2] = (double)vm->inactive_count * page_K;
}

void Platform_setSwapValues(Meter* mtr) {
	int mib[2] = {CTL_VM, VM_SWAPUSAGE};
	struct xsw_usage swapused;
	size_t swlen = sizeof(swapused);
	if(sysctl(mib, 2, &swapused, &swlen, NULL, 0) < 0) {
		mtr->total = 0;
		mtr->values[0] = 0;
	} else {
		mtr->total = swapused.xsu_total / 1024;
		mtr->values[0] = swapused.xsu_used / 1024;
	}
}

static char **get_process_vector(const Process *proc, bool is_env) {
   int mib[3] = { CTL_KERN, KERN_ARGMAX };
   int argmax;
   size_t bufsz = sizeof(argmax);
   if (sysctl(mib, 2, &argmax, &bufsz, 0, 0) < 0) return NULL;
   mib[1] = KERN_PROCARGS2;
   mib[2] = proc->pid;
   char *buf = xMalloc(argmax);
   bufsz = argmax;
   if (sysctl(mib, 3, buf, &bufsz, 0, 0) < 0 || bufsz <= sizeof(int)) {
      free(buf);
      return NULL;
   }

   char **v = xMalloc(sizeof(char *));
   unsigned int i = 0;

   char *p = buf, *endp = buf + bufsz;
   int argc = *(int *)p;
   p += sizeof(int);

   // skip exe
   p = strchr(p, 0)+1;

   // skip padding
   while(!*p && p < endp) ++p;

   if(is_env) {
      // skip argv
      while(argc-- && p < endp) p = strrchr(p, 0) + 1;

      // skip padding
      while(!*p && p < endp) ++p;
   }

   while(p < endp && *p && (is_env || argc-- > 0)) {
      size_t len = strlen(p) + 1;
      v[i] = xMalloc(len);
      memcpy(v[i], p, len);
      v = xRealloc(v, (++i + 1) * sizeof(char *));
      p += len;
   }

   free(buf);

   v[i] = NULL;
   return v;
}

char **Platform_getProcessArgv(const Process *proc) {
	return get_process_vector(proc, false);
}

char **Platform_getProcessEnvv(const Process *proc) {
	return get_process_vector(proc, true);
}

bool Platform_haveSwap() {
	int mib[] = { CTL_VM, VM_SWAPUSAGE };
	struct xsw_usage swap_usage;
	size_t len = sizeof swap_usage;
	if(sysctl(mib, 2, &swap_usage, &len, NULL, 0) < 0) return false;
	return swap_usage.xsu_total > 0;
}
