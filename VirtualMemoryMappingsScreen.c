/*
htop - VirtualMemoryMappingsScreen.c
(C) 2004-2012 Hisham H. Muhammad
Copyright 2015-2026 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include <InfoScreen.h>

typedef struct VirtualMemoryMappingsScreen_ {
   InfoScreen super;
} VirtualMemoryMappingsScreen;
}*/

#include <VirtualMemoryMappingsScreen.h>
#include <CRT.h>
#include <ListItem.h>
#include <Process.h>
#include <stdlib.h>
#include <unistd.h>

InfoScreenClass VirtualMemoryMappingsScreen_class = {
   .super = {
      .extends = Class(Object),
      .delete = VirtualMemoryMappingsScreen_delete
   },
   .scan = VirtualMemoryMappingsScreen_scan,
   .draw = VirtualMemoryMappingsScreen_draw
};

VirtualMemoryMappingsScreen *VirtualMemoryMappingsScreen_new(const Process *process) {
   VirtualMemoryMappingsScreen* this = xMalloc(sizeof(VirtualMemoryMappingsScreen));
   Object_setClass(this, Class(VirtualMemoryMappingsScreen));
   return (VirtualMemoryMappingsScreen *)InfoScreen_init(&this->super, process, NULL, LINES-3, " ");
}

void VirtualMemoryMappingsScreen_delete(Object* this) {
   free(InfoScreen_done((InfoScreen*)this));
}

void VirtualMemoryMappingsScreen_draw(InfoScreen *this) {
   InfoScreen_drawTitled(this, "Virtual memory mappings of process %d - %s",
      (int)this->process->pid, this->process->comm);
}

void VirtualMemoryMappingsScreen_scan(InfoScreen *this) {
   Panel_prune(this->display);
   char **entries = Process_getVirtualMemoryMappings(this->process);
   if(entries) {
      char **p = entries;
      while(*p) {
         InfoScreen_addLine(this, *p, HTOP_DEFAULT_COLOR);
         free(*p++);
      }
      free(entries);
   } else {
      InfoScreen_addLine(this, "Could not read process virtual memory mappings.",
         HTOP_LARGE_NUMBER_COLOR);
   }
}
