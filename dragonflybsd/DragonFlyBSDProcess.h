/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_dragonflybsd_DragonFlyBSDProcess
#define HEADER_dragonflybsd_DragonFlyBSDProcess
/*
htop - dragonflybsd/DragonFlyBSDProcess.h
(C) 2015 Hisham H. Muhammad
(C) 2017 Diederik de Groot
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/


typedef enum {
   // Add platform-specific fields here, with ids >= 100
   HTOP_JID_FIELD = 100,
   HTOP_JAIL_FIELD = 101,
   HTOP_LAST_PROCESSFIELD = 102,
} DragonFlyBSDProcessField;


typedef struct DragonFlyBSDProcess_ {
   Process super;
   bool kernel;
   int   jid;
   char* jname;
} DragonFlyBSDProcess;



extern ProcessClass DragonFlyBSDProcess_class;

extern ProcessFieldData Process_fields[];

extern ProcessPidColumn Process_pidColumns[];

DragonFlyBSDProcess* DragonFlyBSDProcess_new(Settings* settings);

void Process_delete(Object* cast);

void DragonFlyBSDProcess_writeField(Process* this, RichString* str, ProcessField field);

long DragonFlyBSDProcess_compare(const void* v1, const void* v2);

bool Process_isKernelProcess(const Process *this);

bool Process_isExtraThreadProcess(const Process* this);

#endif
