/*
htop - aix/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2015 David C. Hunt
(C) 2018 Calvin Buckley
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
#include "AixProcessList.h"
#include <signal.h>
#include <sys/proc.h>
#include <string.h>
#include <math.h>
#include <procinfo.h>

/*{
#define _LARGE_FILE_API

#include "Action.h"
#include "BatteryMeter.h"
#include "SignalsPanel.h"
#include "AixProcess.h"

// Used for the load average.
#include <sys/kinfo.h>
// AIX doesn't define this function for userland headers, but it's in libc
extern int getkerninfo(int, char*, int*, int32long64_t);
}*/

#ifndef CLAMP
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))
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
   SIG(STOP),
   SIG(TSTP),
   SIG(CONT),
   SIG(CHLD),
   SIG(TTIN),
   SIG(TTOU),
   SIG(IO),
   SIG(XCPU),
   SIG(XFSZ),
   SIG(MSG),
   SIG(WINCH),
   SIG(USR1),
   SIG(USR2),
   SIG(PROF),
   SIG(DANGER),
   SIG(VTALRM),
   SIG(MIGRATE),
   SIG(PRE),
   SIG(VIRT),
   SIG(TALRM),
   // AIX requests not use SIGWAITING/39
   // don't know about the hole here
   SIG(SYSERROR),
   SIG(RECOVERY),
   // Real-time signals 50-57
   //SIG(RECONFIG),
   // AIX also requests us to not use SIGCPUFAIL/59
   //SIG(KAP),
   //SIG(RETRACT),
   //SIG(SOUND),
   //SIG(SAK),
#undef SIG
};

const unsigned int Platform_numberOfSignals = sizeof(Platform_signals)/sizeof(SignalItem);

ProcessField Platform_defaultFields[] = { PID, EFFECTIVE_USER, PRIORITY, NICE, M_SIZE, M_RESIDENT, STATE, PERCENT_CPU, PERCENT_MEM, TIME, COMM, 0 };

extern ProcessFieldData Process_fields[];

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

int Platform_numberOfFields = 101;

extern char Process_pidFormat[20];

int Platform_getUptime() {
	// Fallback to utmpx
	return -1;
}

void Platform_getLoadAverage(double* one, double* five, double* fifteen) {
#ifndef __PASE__
   unsigned long long int avenrun[3];
   int size = sizeof (avenrun);
   if (getkerninfo(KINFO_GET_AVENRUN, (char*)avenrun, &size, 0) != -1) {
      // apply float scaling factor
      *one = (double)avenrun [0] / 65536;
      *five = (double)avenrun [1] / 65536;
      *fifteen = (double)avenrun [2] / 65536;
   }
#else
   // IBM i doesn't generate load averages
   *one = 0;
   *five = 0;
   *fifteen = 0;
#endif
}

int Platform_getMaxPid() {
   return PIDMAX;
}

double Platform_setCPUValues(Meter* this, int cpu) {
   AixProcessList* apl = (AixProcessList*) this->pl;
   CPUData* cpuData;
   cpuData = &(apl->cpus [cpu]);

   double  percent;
   double* v = this->values;

   v[CPU_METER_NICE]   = 0.0;
   v[CPU_METER_NORMAL] = cpuData->utime_p;
   if (this->pl->settings->detailedCPUTime) {
      v[CPU_METER_KERNEL]  = cpuData->stime_p;
      v[CPU_METER_IRQ]     = cpuData->wtime_p;
      Meter_setItems(this, 4);
      percent = v[0]+v[1]+v[2]+v[3];
   } else {
      v[2] = cpuData->stime_p + cpuData->wtime_p;
      Meter_setItems(this, 3);
      percent = v[0]+v[1]+v[2];
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
   // the mem scan code in AixProcessList does the work
   ProcessList* pl = (ProcessList*) this->pl;
   this->total = pl->totalSwap;
   this->values[0] = pl->usedSwap;
}

char **Platform_getProcessEnv(Process *proc) {
   char* buf;
   struct procentry64 pe;
   /* we only need to fill in the pid, it seems */
   pe.pi_pid = proc->pid;
   buf = (char*)xMalloc(0x10000);
   if (getevars (&pe, sizeof (pe), buf, 0x10000) == -1) {
      free (buf);
      return NULL;
   }
   char **env = xMalloc(sizeof(char *));
   unsigned int i = 0;
   char *p = buf;
   while(*p) {
      size_t len = strlen(p) + 1;
      env[i] = xMalloc(len);
      memcpy(env[i], p, len);
      env = xRealloc(env, (++i + 1) * sizeof(char *));
      p += len;
   }
   env[i] = NULL;
   return env;
}
