/*
htop - KernelStackTraceScreen.c
(C) 2004-2012 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "InfoScreen.h"

typedef struct KernelStackTraceScreen_ {
   InfoScreen super;
} KernelStackTraceScreen;
}*/

#include "KernelStackTraceScreen.h"
#include "CRT.h"
#include "ListItem.h"
#include "Process.h"
#include <stdlib.h>
#include <unistd.h>

InfoScreenClass KernelStackTraceScreen_class = {
   .super = {
      .extends = Class(Object),
      .delete = KernelStackTraceScreen_delete
   },
   .scan = KernelStackTraceScreen_scan,
   .draw = KernelStackTraceScreen_draw
};

KernelStackTraceScreen *KernelStackTraceScreen_new(const Process *process) {
   KernelStackTraceScreen* this = xMalloc(sizeof(KernelStackTraceScreen));
   Object_setClass(this, Class(KernelStackTraceScreen));
   return (KernelStackTraceScreen *)InfoScreen_init(&this->super, process, NULL, LINES-3, " ");
}

void KernelStackTraceScreen_delete(Object* this) {
   free(InfoScreen_done((InfoScreen*)this));
}

void KernelStackTraceScreen_draw(InfoScreen* this) {
   InfoScreen_drawTitled(this, "Kernel stack trace of process %d - %s",
      (int)this->process->pid, this->process->comm);
}

void KernelStackTraceScreen_scan(InfoScreen* this) {
   Panel_prune(this->display);
   CRT_dropPrivileges();
   char **frames = Process_getKernelStackTrace(this->process);
   CRT_restorePrivileges();
   if(frames) {
      char **p = frames;
      while(*p) {
         InfoScreen_addLine(this, *p, **p == '#' ? HTOP_DEFAULT_COLOR : HTOP_PROCESS_TAG_COLOR);
         free(*p++);
      }
      free(frames);
   } else {
      InfoScreen_addLine(this, "Could not read process kernel stack trace.", HTOP_LARGE_NUMBER_COLOR);
   }
}
