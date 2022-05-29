/*
htop - interix/Platform.c
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
#include <signal.h>
#include <stdio.h>

/*{
#include "Action.h"
#include "BatteryMeter.h"
#include "SignalsPanel.h"
#include "InterixProcess.h"
}*/

const SignalItem Platform_signals[] = {
   { .name = "Cancel", .number = 0 },
#define SIG(NAME) { .name = #NAME, .number = SIG##NAME }
   SIG(HUP),
   SIG(INT),
   SIG(QUIT),
   SIG(ILL),
   SIG(TRAP),
   SIG(ABRT),
   SIG(EXCEPT),
   SIG(FPE),
   SIG(KILL),
   SIG(BUS),
   SIG(SEGV),
   SIG(SYS),
   SIG(PIPE),
   SIG(ALRM),
   SIG(TERM),
   SIG(USR1),
   SIG(USR2),
   SIG(CHLD),
   SIG(IO),
   SIG(WINCH),
   SIG(URG),
   SIG(POLL),
   SIG(STOP),
   SIG(TSTP),
   SIG(CONT),
   SIG(TTIN),
   SIG(TTOU),
   SIG(VTALRM),
   SIG(PROF),
   SIG(XCPU),
   SIG(XFSZ),
   SIG(CANCEL),
#undef SIG
};

const unsigned int Platform_numberOfSignals = sizeof(Platform_signals)/sizeof(SignalItem);

ProcessField Platform_defaultFields[] = { HTOP_PID_FIELD, HTOP_EFFECTIVE_USER_FIELD, HTOP_PRIORITY_FIELD, HTOP_NICE_FIELD, HTOP_M_SIZE_FIELD, HTOP_M_RESIDENT_FIELD, HTOP_STATE_FIELD, HTOP_PERCENT_CPU_FIELD, HTOP_PERCENT_MEM_FIELD, HTOP_TIME_FIELD, HTOP_COMM_FIELD, 0 };

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

void Platform_setBindings(Htop_Action* keys) {
   (void) keys;
}

extern char Process_pidFormat[20];

int Platform_getUptime() {
	// Fallback to utmpx
	return -1;
}

void Platform_getLoadAverage(double* one, double* five, double* fifteen) {
   *one = 0;
   *five = 0;
   *fifteen = 0;
}

int Platform_getMaxPid() {
   return 65535;	// XXX: Just guessing
}

double Platform_setCPUValues(Meter* this, int cpu) {
   (void) cpu;
   return 0.0;
}

void Platform_setMemoryValues(Meter* this) {
   (void) this;
}

void Platform_setSwapValues(Meter* this) {
   (void) this;
}

static char **get_process_vector(const Process *proc, const char *v_type) {
   char path[32];
   xSnprintf(path, sizeof path, "/proc/%d/%s", (int)proc->pid, v_type);
   FILE *f = fopen(path, "r");
   if(!f) return NULL;
   char **env = xMalloc(sizeof(char *));
   unsigned int i = 0;
   char buffer[4096];
   int c;
   do {
      size_t len = 0;
      while((c = fgetc(f)) != EOF && c) {
         if(len < sizeof buffer) buffer[len++] = c;
      }
      if(!len) continue;
      env[i] = xMalloc(len + 1);
      memcpy(env[i], buffer, len);
      env[i][len] = 0;
      env = xRealloc(env, (++i + 1) * sizeof(char *));
   } while(c != EOF);
   fclose(f);
   env[i] = NULL;
   return env;
}

char **Platform_getProcessArgv(const Process *proc) {
	return get_process_vector(proc, "cmdline");
}

char **Platform_getProcessEnvv(const Process *proc) {
	return get_process_vector(proc, "environ");
}

bool Platform_haveSwap() {
	return false;
}
