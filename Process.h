/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_Process
#define HEADER_Process
/*
htop - Process.h
(C) 2004-2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#ifdef MAJOR_IN_MKDEV
#elif defined MAJOR_IN_SYSMACROS
#endif
#ifdef HAVE_LIBNCURSESW
#endif

#ifdef __ANDROID__
#define SYS_ioprio_get __NR_ioprio_get
#define SYS_ioprio_set __NR_ioprio_set
#endif

#include "config.h"
#include "Object.h"
#include "FieldData.h"
#include <sys/types.h>
#include <stdbool.h>

#define uintcmp(n1,n2) ((n1)>(n2)?1:((n1)<(n2)?-1:0))

#define PROCESS_FLAG_IO 0x0001

typedef enum {
   HTOP_NULL_PROCESSFIELD = 0,
   HTOP_PID_FIELD = 1,
   HTOP_NAME_FIELD = 2,
   HTOP_STATE_FIELD = 3,
   HTOP_PPID_FIELD = 4,
   HTOP_PGRP_FIELD = 5,
   HTOP_SESSION_FIELD = 6,
   HTOP_TTY_FIELD = 7,
   HTOP_TPGID_FIELD = 8,
   HTOP_MINFLT_FIELD = 10,
   HTOP_MAJFLT_FIELD = 12,
   HTOP_PRIORITY_FIELD = 18,
   HTOP_NICE_FIELD = 19,
   HTOP_STARTTIME_FIELD = 21,
   HTOP_PROCESSOR_FIELD = 38,
   HTOP_M_SIZE_FIELD = 39,
   HTOP_M_RESIDENT_FIELD = 40,
   HTOP_EFFECTIVE_UID_FIELD = 46,
   HTOP_PERCENT_CPU_FIELD = 47,
   HTOP_PERCENT_MEM_FIELD = 48,
   HTOP_EFFECTIVE_USER_FIELD = 49,
   HTOP_TIME_FIELD = 50,
   HTOP_NLWP_FIELD = 51,
   HTOP_TGID_FIELD = 52,
   HTOP_REAL_UID_FIELD = 53,
   HTOP_REAL_USER_FIELD = 54,
   HTOP_COMM_FIELD = 99
} ProcessField;

typedef struct ProcessPidColumn_ {
   int id;
   char* label;
} ProcessPidColumn;

typedef struct Process_ {
   Object super;

   struct Settings_* settings;

   bool created;
   bool updated;

   bool tag;
   bool showChildren;
   bool show;
   bool seen_in_tree_loop;
   int indent;

   int state;
   pid_t pid;
   pid_t ppid;
   pid_t tgid;
   char *name;
   char* comm;
   int commLen;
   int argv0_length;

   unsigned int pgrp;
   unsigned int session;
   dev_t tty_nr;
   int tpgid;
   uid_t ruid;
   uid_t euid;

   char *real_user;
   char *effective_user;

   unsigned long long int time;
   int processor;

   float percent_cpu;
   float percent_mem;

   long int priority;
   long int nice;
   long int nlwp;
   time_t starttime_ctime;

   // In pages
   long int m_size;
   long int m_resident;

   int exit_signal;

   unsigned long int minflt;
   unsigned long int majflt;
} Process;

// Implemented in platform-specific code:
bool Process_isKernelProcess(const Process *);
bool Process_isExtraThreadProcess(const Process *);
char **Process_getKernelStackTrace(const Process *);
extern FieldData Process_fields[];
extern ProcessPidColumn Process_pidColumns[];

typedef Process*(*Process_New)(struct Settings_*);
typedef void (*ProcessWriteFieldFunction)(const Process *, RichString *, ProcessField);
typedef bool (*ProcessSendSignalFunction)(const Process *, int);
typedef bool (*ProcessGetBooleanFunction)(const Process *);

typedef struct ProcessClass_ {
   ObjectClass super;
   ProcessWriteFieldFunction writeField;
   ProcessSendSignalFunction sendSignal;
   ProcessGetBooleanFunction isSelf;
} ProcessClass;

#define As_Process(this_)              ((ProcessClass*)((this_)->super.klass))

#if defined __OpenBSD__ && defined PID_AND_MAIN_THREAD_ID_DIFFER
#define Process_getParentPid(process_) ((process_)->tgid == (process_)->pid || (process_)->settings->hide_high_level_processes ? (process_)->ppid : (process_)->tgid)
#define Process_isChildOf(process_, pid_) ((process_)->tgid == (pid_) || (((process_)->tgid == (process_)->pid || (process_)->settings->hide_high_level_processes) && (process_)->ppid == (pid_)))
#else
#define Process_getParentPid(process_) ((process_)->tgid == (process_)->pid ? (process_)->ppid : (process_)->tgid)
#define Process_isChildOf(process_, pid_) ((process_)->tgid == (pid_) || ((process_)->tgid == (process_)->pid && (process_)->ppid == (pid_)))
#endif
#define Process_isSelf(this_) (As_Process(this_)->isSelf(this_))
#define Process_sortState(state) ((state) == 'I' ? 0x100 : (state))


#ifndef NODEV
#ifdef __INTERIX
#define NODEV ((dev_t)0)
#else
#define NODEV ((dev_t)-1)
#endif
#endif

extern char Process_pidFormat[20];

void Process_setupColumnWidths();

void Process_humanNumber(RichString* str, unsigned long number, bool coloring);

void Process_colorNumber(RichString* str, unsigned long long number, bool coloring);

void Process_outputRate(RichString* str, char* buffer, int n, double rate, int coloring);

void Process_writeField(const Process *this, RichString* str, ProcessField field);

void Process_display(Object* cast, RichString* out);

void Process_done(Process* this);

extern ProcessClass Process_class;

void Process_init(Process* this, struct Settings_* settings);

void Process_toggleTag(Process* this);

bool Process_setPriority(Process* this, int priority);

bool Process_changePriorityBy(Process* this, int delta);

bool Process_sendSignal(const Process *this, int sgn);

long Process_pidCompare(const void* v1, const void* v2);

long Process_compare(const void* v1, const void* v2);

#endif
