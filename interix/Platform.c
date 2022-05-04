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
#include <utmpx.h>
#include <stdio.h>

/*{
#include "Action.h"
#include "BatteryMeter.h"
#include "SignalsPanel.h"
#include "InterixProcess.h"
}*/

const SignalItem Platform_signals[] = {
   { .name = " 0 Cancel",    .number =  0 },
   { .name = " 1 SIGHUP",    .number = SIGHUP },
   { .name = " 2 SIGINT",    .number = SIGINT },
   { .name = " 3 SIGQUIT",   .number = SIGQUIT },
   { .name = " 4 SIGILL",    .number = SIGILL },
   { .name = " 5 SIGTRAP",   .number = SIGTRAP },
   { .name = " 6 SIGABRT",   .number = SIGABRT },
   { .name = " 7 SIGEXCEPT", .number = SIGEXCEPT },
   { .name = " 8 SIGFPE",    .number = SIGFPE },
   { .name = " 9 SIGKILL",   .number = SIGKILL },
   { .name = "10 SIGBUS",    .number = SIGBUS },
   { .name = "11 SIGSEGV",   .number = SIGSEGV },
   { .name = "12 SIGSYS",    .number = SIGSYS },
   { .name = "13 SIGPIPE",   .number = SIGPIPE },
   { .name = "14 SIGALRM",   .number = SIGALRM },
   { .name = "15 SIGTERM",   .number = SIGTERM },
   { .name = "16 SIGUSR1",   .number = SIGUSR1 },
   { .name = "17 SIGUSR2",   .number = SIGUSR2 },
   { .name = "18 SIGCHLD",   .number = SIGCHLD },
   { .name = "19 SIGIO",     .number = SIGIO },
   { .name = "20 SIGWINCH",  .number = SIGWINCH },
   { .name = "21 SIGURG",    .number = SIGURG },
   { .name = "22 SIGPOLL",   .number = SIGPOLL },
   { .name = "23 SIGSTOP",   .number = SIGSTOP },
   { .name = "24 SIGTSTP",   .number = SIGTSTP },
   { .name = "25 SIGCONT",   .number = SIGCONT },
   { .name = "26 SIGTTIN",   .number = SIGTTIN },
   { .name = "27 SIGTTOU",   .number = SIGTTOU },
   { .name = "28 SIGVTALRM", .number = SIGVTALRM },
   { .name = "29 SIGPROF",   .number = SIGPROF },
   { .name = "30 SIGXCPU",   .number = SIGXCPU },
   { .name = "31 SIGXFSZ",   .number = SIGXFSZ },
   { .name = "32 SIGCANCEL", .number = SIGCANCEL }
};

const unsigned int Platform_numberOfSignals = sizeof(Platform_signals)/sizeof(SignalItem);

ProcessField Platform_defaultFields[] = { PID, EFFECTIVE_USER, PRIORITY, NICE, M_SIZE, M_RESIDENT, STATE, PERCENT_CPU, PERCENT_MEM, TIME, COMM, 0 };

int Platform_numberOfFields = LAST_PROCESSFIELD;

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
   time_t boot_time = -1;
   time_t curr_time = time(NULL);   
   struct utmpx *utx;
   while ((utx = getutxent())) {
      if(utx->ut_type == BOOT_TIME) {
         boot_time = utx->ut_tv.tv_sec;
         break;
      }
   }
   endutxent();
   return boot_time == -1 ? 0 : curr_time - boot_time;
}

void Platform_getLoadAverage(double* one, double* five, double* fifteen) {
   *one = 0;
   *five = 0;
   *fifteen = 0;
}

int Platform_getMaxPid() {
   return 4096;	// XXX: Just guessing
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

char **Platform_getProcessEnv(Process *proc) {
   char path[32];
   xSnprintf(path, sizeof path, "/proc/%d/environ", (int)proc->pid);
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
