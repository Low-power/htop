/*
htop - unsupported/UnsupportedProcess.c
(C) 2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "UnsupportedProcess.h"
#include <stdlib.h>

/*{
#include "Settings.h"

#define Process_delete UnsupportedProcess_delete

}*/

Process* UnsupportedProcess_new(Settings* settings) {
   Process* this = xCalloc(1, sizeof(Process));
   Object_setClass(this, Class(Process));
   Object_getClass(this)->delete = UnsupportedProcess_delete;
   Process_init(this, settings);
   return this;
}

void UnsupportedProcess_delete(Object* cast) {
   Process* this = (Process*) cast;
   Object_setClass(this, Class(Process));
   Process_done((Process*)cast);
   // free platform-specific fields here
   free(this);
}

bool Process_isKernelProcess(const Process *this) {
	return false;
}

bool Process_isExtraThreadProcess(const Process *this) {
	return false;
}

char **Process_getKernelStackTrace(const Process *this) {
	return NULL;
}
