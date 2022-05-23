/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_freebsd_FreeBSDProcessList
#define HEADER_freebsd_FreeBSDProcessList
/*
htop - freebsd/FreeBSDProcessList.h
(C) 2014 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#ifdef HAVE_LIBKVM
#include <kvm.h>
#endif
#include <sys/param.h>
#include <sys/jail.h>
#include <sys/uio.h>
#include <sys/resource.h>

typedef struct CPUData_ {
   double userPercent;
   double nicePercent;
   double systemPercent;
   double irqPercent;
   double idlePercent;
   double systemAllPercent;
} CPUData;

typedef struct FreeBSDProcessList_ {
   ProcessList super;
#ifdef HAVE_LIBKVM
   kvm_t* kd;
#else
   struct kinfo_proc *kip_buffer;
   size_t kip_buffer_size;
#endif
   unsigned long long int memWire;
   unsigned long long int memActive;
   unsigned long long int memInactive;
   unsigned long long int memFree;
   unsigned long long int laundry_size;

   CPUData* cpus;

   long int *cp_time_o;
   long int *cp_time_n;

   long int *cp_times_o;
   long int *cp_times_n;

   int arg_max;
} FreeBSDProcessList;


#ifdef __GLIBC__
// GNU C Library defines NZERO to 20, which is incorrect for kFreeBSD
#undef NZERO
#define NZERO 0
#endif

ProcessList* ProcessList_new(UsersTable* usersTable, const Hashtable *pidWhiteList, uid_t userId);

void ProcessList_delete(ProcessList* this);

#define JAIL_ERRMSGLEN 1024

void ProcessList_goThroughEntries(ProcessList* this);

#endif
