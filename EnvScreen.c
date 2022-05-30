/*{
#include "InfoScreen.h"

typedef struct EnvScreen_ {
   InfoScreen super;
} EnvScreen;
}*/

#include "EnvScreen.h"
#include "CRT.h"
#include "ListItem.h"
#include "Platform.h"
#include <stdlib.h>
#include <unistd.h>

InfoScreenClass EnvScreen_class = {
   .super = {
      .extends = Class(Object),
      .delete = EnvScreen_delete
   },
   .scan = EnvScreen_scan,
   .draw = EnvScreen_draw
};

EnvScreen* EnvScreen_new(Process* process) {
   EnvScreen* this = xMalloc(sizeof(EnvScreen));
   Object_setClass(this, Class(EnvScreen));
   return (EnvScreen*) InfoScreen_init(&this->super, process, NULL, LINES-3, " ");
}

void EnvScreen_delete(Object* this) {
   free(InfoScreen_done((InfoScreen*)this));
}

void EnvScreen_draw(InfoScreen* this) {
   InfoScreen_drawTitled(this, "Environment of process %d - %s", this->process->pid, this->process->comm);
}

void EnvScreen_scan(InfoScreen* this) {
   Panel* panel = this->display;
   int idx = MAX(Panel_getSelectedIndex(panel), 0);

   Panel_prune(panel);

   CRT_dropPrivileges();
   char **envv = Platform_getProcessEnvv(this->process);
   CRT_restorePrivileges();
   if (envv) {
      char **p = envv;
      while(*p) {
         InfoScreen_addLine(this, *p, HTOP_DEFAULT_COLOR);
         free(*p++);
      }
      free(envv);
   }
   else {
      InfoScreen_addLine(this, "Could not read process environment.", HTOP_LARGE_NUMBER_COLOR);
   }

   Vector_insertionSort(this->lines);
   Vector_insertionSort(panel->items);
   Panel_setSelected(panel, idx);
}
