/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_freebsd_FreeBSDProcess
#define HEADER_freebsd_FreeBSDProcess
/*
htop - freebsd/FreeBSDProcess.h
(C) 2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/


typedef enum {
   // Add platform-specific fields here, with ids >= 100
   JID   = 100,
   JAIL  = 101,
   LAST_PROCESSFIELD = 102,
} FreeBSDProcessField;


typedef struct FreeBSDProcess_ {
   Process super;
   bool kernel;
   int   jid;
   char* jname;
} FreeBSDProcess;



extern ProcessClass FreeBSDProcess_class;

extern ProcessFieldData Process_fields[];

extern ProcessPidColumn Process_pidColumns[];

FreeBSDProcess* FreeBSDProcess_new(Settings* settings);

void Process_delete(Object* cast);

void FreeBSDProcess_writeField(Process *super, RichString* str, ProcessField field);

long FreeBSDProcess_compare(const void* v1, const void* v2);

bool Process_isKernelProcess(const Process *this);

bool Process_isExtraThreadProcess(const Process* this);

#endif
