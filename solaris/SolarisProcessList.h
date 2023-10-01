/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_solaris_SolarisProcessList
#define HEADER_solaris_SolarisProcessList
/*
htop - solaris/SolarisProcessList.h
(C) 2014 Hisham H. Muhammad
(C) 2017,2018 Guy M. Broome
Copyright 2015-2023 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#ifndef HAVE_LIBPROC
#endif

#define MAXCMDLINE 255


#include "SolarisProcess.h"
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
   int online_cpu_count;
   CPUData* cpus;
#ifndef HAVE_LIBPROC
   DIR *proc_dir;
#endif
   time_t last_updated;
} SolarisProcessList;


#if !defined HAVE_DIRFD && !defined dirfd
#define dirfd(D) ((D)->dd_fd)
#endif

#ifdef HAVE_ZONE_H
#endif

ProcessList* ProcessList_new(UsersTable* usersTable, const Hashtable *pidWhiteList, uid_t userId);

void ProcessList_delete(ProcessList* pl);

#ifdef HAVE_LIBPROC

/* NOTE: the following is a callback function of type proc_walk_f
 *       and MUST conform to the appropriate definition in order
 *       to work.  See libproc(3LIB) on a Solaris or Illumos
 *       system for more info.
 */
void ProcessList_goThroughEntries(ProcessList* this, bool skip_processes);

#else

void ProcessList_goThroughEntries(ProcessList *super, bool skip_processes);

#endif

#endif
