/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_Platform
#define HEADER_Platform
/*
htop - aix/Platform.h
(C) 2014 Hisham H. Muhammad
(C) 2015 David C. Hunt
(C) 2018 Calvin Buckley
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#define _LARGE_FILE_API

#include "Action.h"
#include "BatteryMeter.h"
#include "SignalsPanel.h"
#include "AixProcess.h"

// Used for the load average.
#include <sys/kinfo.h>
// AIX doesn't define this function for userland headers, but it's in libc
extern int getkerninfo(int, char*, int*, int32long64_t);

#ifndef CLAMP
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))
#endif

extern const SignalItem Platform_signals[];

extern const unsigned int Platform_numberOfSignals;

extern ProcessField Platform_defaultFields[];

extern MeterClass* Platform_meterTypes[];

void Platform_setBindings(Htop_Action* keys);

extern int Platform_numberOfFields;

// identical to Solaris, thanks System V
int Platform_getUptime();

void Platform_getLoadAverage(double* one, double* five, double* fifteen);

int Platform_getMaxPid();

double Platform_setCPUValues(Meter* this, int cpu);

void Platform_setMemoryValues(Meter* this);

void Platform_setSwapValues(Meter* this);

char **Platform_getProcessEnv(pid_t pid);

#endif
