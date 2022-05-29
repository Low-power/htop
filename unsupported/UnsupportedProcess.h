/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_unsupported_UnsupportedProcess
#define HEADER_unsupported_UnsupportedProcess
/*
htop - unsupported/UnsupportedProcess.h
(C) 2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Settings.h"

#define Process_delete UnsupportedProcess_delete


Process* UnsupportedProcess_new(Settings* settings);

void UnsupportedProcess_delete(Object* cast);

bool Process_isKernelProcess(const Process *this);

bool Process_isExtraThreadProcess(const Process *this);

char **Process_getKernelStackTrace(const Process *this);

#endif
