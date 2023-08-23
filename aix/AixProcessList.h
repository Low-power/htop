/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_aix_AixProcessList
#define HEADER_aix_AixProcessList
/*
htop - aix/AixProcessList.h
(C) 2014 Hisham H. Muhammad
(C) 2018 Calvin Buckley
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#ifdef __PASE__
#endif

#include "ProcessList.h"
#ifndef __PASE__
#define Hyp_Name __Hyp_Name_dummy(struct __Hyp_Name_dummy); static const char *const __Hyp_Name__
#include <libperfstat.h>
#endif
#include <sys/time.h>

// publically consumed
typedef struct CPUData_ {
   // per libperfstat.h, clock ticks
   unsigned long long utime, stime, itime, wtime;
   // warning: doubles are 4 bytes in structs on AIX
   // ...not like we need precision here anyways
   double utime_p, stime_p, itime_p, wtime_p;
} CPUData;

typedef struct AixProcessList_ {
   ProcessList super;
   CPUData* cpus;
#ifndef __PASE__
   perfstat_cpu_t* ps_cpus;
   perfstat_cpu_total_t ps_ct;
#endif
   struct timeval last_updated;
} AixProcessList;

ProcessList* ProcessList_new(UsersTable* usersTable, const Hashtable *pidWhiteList, uid_t userId);

void ProcessList_delete(ProcessList* this);

void ProcessList_goThroughEntries(ProcessList* super, bool skip_processes);

#endif
