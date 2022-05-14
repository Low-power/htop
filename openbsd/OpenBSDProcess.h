/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_openbsd_OpenBSDProcess
#define HEADER_openbsd_OpenBSDProcess
/*
htop - openbsd/OpenBSDProcess.h
(C) 2015 Hisham H. Muhammad
(C) 2015 Michael McConville
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/


typedef enum {
   // Add platform-specific fields here, with ids >= 100
   LAST_PROCESSFIELD = 100,
} OpenBSDProcessField;

typedef struct OpenBSDProcess_ {
   Process super;
   bool is_kernel_process;
   bool is_main_thread;
} OpenBSDProcess;


extern ProcessClass OpenBSDProcess_class;

extern ProcessFieldData Process_fields[];

extern ProcessPidColumn Process_pidColumns[];

OpenBSDProcess* OpenBSDProcess_new(Settings* settings);

void Process_delete(Object* cast);

void OpenBSDProcess_writeField(Process* this, RichString* str, ProcessField field);

long OpenBSDProcess_compare(const void* v1, const void* v2);

bool Process_isKernelProcess(const Process *this);

bool Process_isExtraThreadProcess(const Process* this);

#endif
