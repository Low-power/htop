/*
htop - ArgScreen.c
(C) 2004-2012 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "InfoScreen.h"

typedef struct ArgScreen_ {
   InfoScreen super;
} ArgScreen;
}*/

#include "ArgScreen.h"
#include "CRT.h"
#include "ListItem.h"
#include "Platform.h"
#include <stdlib.h>
#include <unistd.h>

InfoScreenClass ArgScreen_class = {
   .super = {
      .extends = Class(Object),
      .delete = ArgScreen_delete
   },
   .scan = ArgScreen_scan,
   .draw = ArgScreen_draw
};

ArgScreen *ArgScreen_new(Process* process) {
   ArgScreen* this = xMalloc(sizeof(ArgScreen));
   Object_setClass(this, Class(ArgScreen));
   return (ArgScreen *)InfoScreen_init(&this->super, process, NULL, LINES-3, " ");
}

void ArgScreen_delete(Object* this) {
   free(InfoScreen_done((InfoScreen*)this));
}

void ArgScreen_draw(InfoScreen* this) {
   InfoScreen_drawTitled(this, "Command line arguments of process %d - %s",
      (int)this->process->pid, this->process->name);
}

void ArgScreen_scan(InfoScreen* this) {
   Panel_prune(this->display);
   CRT_dropPrivileges();
   char **argv = Platform_getProcessArgv(this->process);
   CRT_restorePrivileges();
   if (argv) {
      char **p = argv;
      while(*p) {
         InfoScreen_addLine(this, *p, HTOP_DEFAULT_COLOR);
         free(*p++);
      }
      free(argv);
   } else {
      InfoScreen_addLine(this, "Could not read process command line arguments.",
         HTOP_LARGE_NUMBER_COLOR);
   }
}
