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

#ifndef __PASE__
#else
#endif


#ifndef __PASE__
#include <libperfstat.h>
#endif

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
} AixProcessList;


ProcessList* ProcessList_new(UsersTable* usersTable, const Hashtable *pidWhiteList, uid_t userId);

void ProcessList_delete(ProcessList* this);

void ProcessList_goThroughEntries(ProcessList* super);

#endif
