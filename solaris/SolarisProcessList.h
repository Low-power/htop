/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_SolarisProcessList
#define HEADER_SolarisProcessList
/*
htop - SolarisProcessList.h
(C) 2014 Hisham H. Muhammad
(C) 2017,2018 Guy M. Broome
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#ifndef HAVE_LIBPROC
#endif

#define MAXCMDLINE 255


#include <stdint.h>
#include <kstat.h>
#ifdef HAVE_LIBPROC
#include <libproc.h>
#else
#include <dirent.h>
#endif

typedef struct CPUData_ {
   double userPercent;
   double nicePercent;
   double systemPercent;
   double irqPercent;
   double idlePercent;
   double systemAllPercent;
   uint64_t luser;
   uint64_t lkrnl;
   uint64_t lintr;
   uint64_t lidle;
} CPUData;

typedef struct SolarisProcessList_ {
   ProcessList super;
   kstat_ctl_t* kd;
   CPUData* cpus;
#ifndef HAVE_LIBPROC
   DIR *proc_dir;
#endif
   time_t last_updated;
} SolarisProcessList;


char* SolarisProcessList_readZoneName(kstat_ctl_t* kd, SolarisProcess* sproc);

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList, uid_t userId);

void ProcessList_delete(ProcessList* pl);

#ifdef HAVE_LIBPROC

/* NOTE: the following is a callback function of type proc_walk_f
 *       and MUST conform to the appropriate definition in order
 *       to work.  See libproc(3LIB) on a Solaris or Illumos
 *       system for more info.
 */ 

int SolarisProcessList_walkproc(psinfo_t *_psinfo, lwpsinfo_t *_lwpsinfo, void *listptr);

void ProcessList_goThroughEntries(ProcessList* this);

#else

void ProcessList_goThroughEntries(ProcessList *super);

#endif

#endif
