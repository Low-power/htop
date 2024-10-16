/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_freebsd_FreeBSDProcess
#define HEADER_freebsd_FreeBSDProcess
/*
htop - freebsd/FreeBSDProcess.h
(C) 2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Settings.h"
#include <stdbool.h>

#define PROCESS_FLAG_JAIL 0x100
#define PROCESS_FLAG_EMULATION 0x200

typedef enum {
   // Add platform-specific fields here, with ids >= 100
   HTOP_JID_FIELD = 100,
   HTOP_JAIL_FIELD,
   HTOP_EMULATION_FIELD,
   HTOP_LAST_PROCESSFIELD
} FreeBSDProcessField;

typedef struct FreeBSDProcess_ {
   Process super;
   bool kernel;
   int   jid;
   char* jname;
   char *emulation;
} FreeBSDProcess;

extern ProcessClass FreeBSDProcess_class;

extern FieldData Process_fields[];

extern ProcessPidColumn Process_pidColumns[];

FreeBSDProcess* FreeBSDProcess_new(Settings* settings);

void Process_delete(Object* cast);

void FreeBSDProcess_writeField(const Process *super, RichString* str, ProcessField field);

long FreeBSDProcess_compare(const void* v1, const void* v2);

bool Process_isKernelProcess(const Process *this);

bool Process_isExtraThreadProcess(const Process* this);

char **Process_getKernelStackTrace(const Process *this);

#endif
