/*
htop - linux/IOPriorityPanel.c
(C) 2004-2012 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "IOPriorityPanel.h"
#include "CRT.h"

/*{
#include "Panel.h"
#include "IOPriority.h"
#include "ListItem.h"
}*/

Panel* IOPriorityPanel_new(IOPriority currPrio) {
   Panel* this = Panel_new(1, 1, 1, 1, true, Class(ListItem), FunctionBar_newEnterEsc("Set    ", "Cancel "));

   Panel_setHeader(this, "IO Priority:");
   Panel_add(this, (Object *)ListItem_new("None (based on nice)", HTOP_DEFAULT_COLOR, IOPriority_None, NULL));
   if (currPrio == IOPriority_None) Panel_setSelected(this, 0);
   static const struct { int klass; const char* name; } classes[] = {
      { .klass = IOPRIO_CLASS_RT, .name = "Realtime" },
      { .klass = IOPRIO_CLASS_BE, .name = "Best-effort" },
      { .klass = 0, .name = NULL }
   };
   for (int c = 0; classes[c].name; c++) {
      for (int i = 0; i < 8; i++) {
         char name[50];
         xSnprintf(name, sizeof name, "%s %d %s", classes[c].name, i, i == 0 ? "(High)" : (i == 7 ? "(Low)" : ""));
         IOPriority ioprio = IOPriority_tuple(classes[c].klass, i);
         Panel_add(this, (Object *)ListItem_new(name, HTOP_DEFAULT_COLOR, ioprio, NULL));
         if (currPrio == ioprio) Panel_setSelected(this, Panel_size(this) - 1);
      }
   }
   Panel_add(this, (Object *)ListItem_new("Idle", HTOP_DEFAULT_COLOR, IOPriority_Idle, NULL));
   if (currPrio == IOPriority_Idle) Panel_setSelected(this, Panel_size(this) - 1);
   return this;
}

IOPriority IOPriorityPanel_getIOPriority(Panel* this) {
   return (IOPriority) ( ((ListItem*) Panel_getSelected(this))->key );
}

