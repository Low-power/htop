/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_aix_AixProcess
#define HEADER_aix_AixProcess
/*
htop - AixProcess.h
(C) 2015 Hisham H. Muhammad
(C) 2018 Calvin Buckley
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Settings.h"
#include <sys/corralid.h> // cid_t 

#define Process_delete AixProcess_delete

typedef enum AixProcessFields {
   // Add platform-specific fields here, with ids >= 100
   WPAR_ID = 100,
   LAST_PROCESSFIELD = 101,
} AixProcessField;

typedef struct AixProcess_ {
   Process super;
   bool kernel;
   cid_t cid; // WPAR ID
   unsigned long long int stime, utime; // System/User time (stored sep for calculations)
} AixProcess;


extern ProcessClass AixProcess_class;

extern ProcessFieldData Process_fields[];

extern ProcessPidColumn Process_pidColumns[];

AixProcess* AixProcess_new(Settings* settings);

void AixProcess_delete(Object* cast);

void AixProcess_writeField(Process* this, RichString* str, ProcessField field);

long AixProcess_compare(const void* v1, const void* v2);

bool Process_isKernelProcess(Process *this);

bool Process_isExtraThreadProcess(Process *this);

#endif
