/*
htop - linux/Platform.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Platform.h"
#include "IOPriority.h"
#include "IOPriorityPanel.h"
#include "LinuxProcess.h"
#include "LinuxProcessList.h"
#include "Battery.h"

#include "Meter.h"
#include "CPUMeter.h"
#include "MemoryMeter.h"
#include "SwapMeter.h"
#include "TasksMeter.h"
#include "LoadAverageMeter.h"
#include "UptimeMeter.h"
#include "ClockMeter.h"
#include "HostnameMeter.h"
#include "LinuxProcess.h"
#include <signal.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

/*{
#include "Action.h"
#include "MainPanel.h"
#include "BatteryMeter.h"
#include "LinuxProcess.h"
#include "SignalsPanel.h"
}*/

#ifndef CLAMP
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))
#endif

ProcessField Platform_defaultFields[] = { PID, EFFECTIVE_USER, PRIORITY, NICE, M_SIZE, M_RESIDENT, M_SHARE, STATE, PERCENT_CPU, PERCENT_MEM, TIME, COMM, 0 };

//static ProcessField defaultIoFields[] = { PID, IO_PRIORITY, EFFECTIVE_USER, IO_READ_RATE, IO_WRITE_RATE, IO_RATE, COMM, 0 };

int Platform_numberOfFields = LAST_PROCESSFIELD;

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
   SIG(POLL),
   SIG(PWR),
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

static Htop_Reaction Platform_actionSetIOPriority(State* st) {
   Panel* panel = st->panel;

   LinuxProcess* p = (LinuxProcess*) Panel_getSelected(panel);
   if (!p) return HTOP_OK;
   IOPriority ioprio = p->ioPriority;
   Panel* ioprioPanel = IOPriorityPanel_new(ioprio);
   void* set = Action_pickFromVector(st, ioprioPanel, 21);
   if (set) {
      IOPriority ioprio = IOPriorityPanel_getIOPriority(ioprioPanel);
      bool ok = MainPanel_foreachProcess((MainPanel*)panel, (MainPanel_ForeachProcessFn) LinuxProcess_setIOPriority, (Arg){ .i = ioprio }, NULL);
      if (!ok)
         beep();
   }
   Panel_delete((Object*)ioprioPanel);
   return HTOP_REFRESH | HTOP_REDRAW_BAR | HTOP_UPDATE_PANELHDR;
}

void Platform_setBindings(Htop_Action* keys) {
   keys['i'] = Platform_actionSetIOPriority;
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
   double uptime;
   FILE *f = fopen(PROCDIR "/uptime", "r");
   if(!f) return -1;
   int n = fscanf(f, "%64lf", &uptime);
   fclose(f);
   if (n < 1) return -1;
   return (int) floor(uptime);
}

void Platform_getLoadAverage(double* one, double* five, double* fifteen) {
   *one = 0; *five = 0; *fifteen = 0;
   FILE *f = fopen(PROCDIR "/loadavg", "r");
   if(!f) return;
   fscanf(f, "%32lf %32lf %32lf", one, five, fifteen);
   fclose(f);
}

int Platform_getMaxPid() {
#ifdef __CYGWIN__
   /* Cygwin uses Windows PID directly, and Windows didn't have a hard
    * limit for processes; the maximum PID value based on data type
    * would be UINT32_MAX - 1, which is too big to use here. */
   return 4194304;
#else
   FILE* file = fopen(PROCDIR "/sys/kernel/pid_max", "r");
   if (!file) return -1;
   int maxPid = 4194303;
   int match = fscanf(file, "%32d", &maxPid);
   (void) match;
   fclose(file);
   return maxPid;
#endif
}

double Platform_setCPUValues(Meter* this, int cpu) {
   LinuxProcessList* pl = (LinuxProcessList*) this->pl;
   CPUData* cpuData = &(pl->cpus[cpu]);
   double total = (double) ( cpuData->totalPeriod == 0 ? 1 : cpuData->totalPeriod);
   double percent;
   double* v = this->values;
   v[CPU_METER_NICE] = cpuData->nicePeriod / total * 100.0;
   v[CPU_METER_NORMAL] = cpuData->userPeriod / total * 100.0;
   if (this->pl->settings->detailedCPUTime) {
      v[CPU_METER_KERNEL]  = cpuData->systemPeriod / total * 100.0;
      v[CPU_METER_IRQ]     = cpuData->irqPeriod / total * 100.0;
      v[CPU_METER_SOFTIRQ] = cpuData->softIrqPeriod / total * 100.0;
      v[CPU_METER_STEAL]   = cpuData->stealPeriod / total * 100.0;
      v[CPU_METER_GUEST]   = cpuData->guestPeriod / total * 100.0;
      v[CPU_METER_IOWAIT]  = cpuData->ioWaitPeriod / total * 100.0;
      Meter_setItems(this, 8);
      if (this->pl->settings->accountGuestInCPUMeter) {
         percent = v[0]+v[1]+v[2]+v[3]+v[4]+v[5]+v[6];
      } else {
         percent = v[0]+v[1]+v[2]+v[3]+v[4];
      }
   } else {
      v[2] = cpuData->systemAllPeriod / total * 100.0;
      v[3] = (cpuData->stealPeriod + cpuData->guestPeriod) / total * 100.0;
      Meter_setItems(this, 4);
      percent = v[0]+v[1]+v[2]+v[3];
   }
   percent = CLAMP(percent, 0.0, 100.0);
   if (isnan(percent)) percent = 0.0;
   return percent;
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

void Platform_setSwapValues(Meter* this) {
   ProcessList* pl = (ProcessList*) this->pl;
   this->total = pl->totalSwap;
   this->values[0] = pl->usedSwap;
}

static char **get_process_vector(const Process *proc, const char *v_type) {
   char path[sizeof PROCDIR + 20];
   xSnprintf(path, sizeof path, PROCDIR "/%d/%s", (int)proc->pid, v_type);
   FILE *f = fopen(path, "r");
   if(!f) return NULL;
   char **v = xMalloc(sizeof(char *));
   unsigned int i = 0;
   char buffer[4096];
   int c;
   do {
      size_t len = 0;
      while((c = fgetc(f)) != EOF && c) {
         if(len < sizeof buffer) buffer[len++] = c;
      }
      if(!len) continue;
      v[i] = xMalloc(len + 1);
      memcpy(v[i], buffer, len);
      v[i][len] = 0;
      v = xRealloc(v, (++i + 1) * sizeof(char *));
   } while(c != EOF);
   fclose(f);
   v[i] = NULL;
   return v;
}

char **Platform_getProcessArgv(const Process *proc) {
	return get_process_vector(proc, "cmdline");
}

char **Platform_getProcessEnvv(const Process *proc) {
	return get_process_vector(proc, "environ");
}
