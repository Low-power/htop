/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_openbsd_OpenBSDProcessList
#define HEADER_openbsd_OpenBSDProcessList
/*
htop - OpenBSDProcessList.h
(C) 2014 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/


#include <kvm.h>

typedef struct CPUData_ {
   unsigned long long int totalTime;
   unsigned long long int totalPeriod;
} CPUData;

typedef struct OpenBSDProcessList_ {
   ProcessList super;
   kvm_t* kd;

   CPUData* cpus;

} OpenBSDProcessList;


/*
 * avoid relying on or conflicting with MIN() and MAX() in sys/param.h
 */
#ifndef MINIMUM
#define MINIMUM(x, y)		((x) > (y) ? (y) : (x))
#endif

#ifndef MAXIMUM
#define MAXIMUM(x, y)		((x) > (y) ? (x) : (y))
#endif

#ifndef CLAMP
#define CLAMP(x, low, high)	(((x) > (high)) ? (high) : MAXIMUM(x, low))
#endif

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList, uid_t userId);

void ProcessList_delete(ProcessList* this);

/*
 * Taken from OpenBSD's ps(1).
 */
#ifdef PID_AND_MAIN_THREAD_ID_DIFFER
#endif

void ProcessList_goThroughEntries(ProcessList* this);

#endif
