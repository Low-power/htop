/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_aix_AixProcess
#define HEADER_aix_AixProcess
/*
htop - aix/AixProcess.h
(C) 2015 Hisham H. Muhammad
(C) 2018 Calvin Buckley
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Settings.h"
#include <sys/corralid.h> // cid_t 

#define Process_delete AixProcess_delete

typedef enum {
   // Add platform-specific fields here, with ids >= 100
   HTOP_WPAR_ID_FIELD = 100,
   HTOP_LAST_PROCESSFIELD = 101,
} AixProcessField;

typedef struct AixProcess_ {
   Process super;
   bool kernel;
   cid_t cid; // WPAR ID
   struct timeval tv; // More precise time for calculating percent_cpu
} AixProcess;


extern ProcessClass AixProcess_class;

extern ProcessFieldData Process_fields[];

extern ProcessPidColumn Process_pidColumns[];

AixProcess* AixProcess_new(Settings* settings);

void AixProcess_delete(Object* cast);

void AixProcess_writeField(Process *super, RichString* str, ProcessField field);

long AixProcess_compare(const void* v1, const void* v2);

bool AixProcess_sendSignal(const Process *this, int sig);

bool Process_isKernelProcess(const Process *this);

bool Process_isExtraThreadProcess(const Process *this);

#endif
