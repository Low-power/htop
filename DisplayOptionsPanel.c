/*
htop - DisplayOptionsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "Panel.h"
#include "Settings.h"
#include "ScreenManager.h"
#include "CheckItem.h"

typedef struct DisplayOptionsPanel_ {
   Panel super;

   Settings* settings;
   ScreenManager* scr;
   CheckItem *case_insensitive_sort_check_item;
} DisplayOptionsPanel;

}*/

#include "config.h"
#include "DisplayOptionsPanel.h"
#include "CRT.h"
#include "Platform.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>

static const char* const DisplayOptionsFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static void DisplayOptionsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   DisplayOptionsPanel* this = (DisplayOptionsPanel*) object;
   Panel_done(super);
   free(this);
}

static HandlerResult DisplayOptionsPanel_eventHandler(Panel* super, int ch, int repeat) {
   DisplayOptionsPanel* this = (DisplayOptionsPanel*) super;
   HandlerResult result = IGNORED;
   CheckItem* selected = (CheckItem*) Panel_getSelected(super);

   switch(ch) {
         bool new_value;
      case 0x0a:
      case 0x0d:
      case KEY_ENTER:
      case KEY_MOUSE:
      case KEY_RECLICK:
      case ' ':
         new_value = !CheckItem_get(selected);
         CheckItem_set(selected, new_value);
         result = HANDLED;
         if(selected == this->case_insensitive_sort_check_item) {
            this->settings->sort_strcmp = new_value ? strcasecmp : strcmp;
         }
         break;
   }

   if (result == HANDLED) {
      this->settings->changed = true;
      Header *header = this->scr->header;
      Header_calculateHeight(header);
      Header_reinit(header);
      Header_draw(header);
      ScreenManager_resize(this->scr, this->scr->x1, header->height, this->scr->x2, this->scr->y2);
   }
   return result;
}

PanelClass DisplayOptionsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = DisplayOptionsPanel_delete
   },
   .eventHandler = DisplayOptionsPanel_eventHandler
};

DisplayOptionsPanel* DisplayOptionsPanel_new(Settings* settings, ScreenManager* scr) {
   DisplayOptionsPanel* this = AllocThis(DisplayOptionsPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(DisplayOptionsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(CheckItem), true, fuBar);

   this->settings = settings;
   this->scr = scr;

   Panel_setHeader(super, "Display options");
#ifdef DISK_STATS
   if(!settings->disk_mode) {
#endif
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Tree view"), &(settings->treeView)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Shadow other users' processes"), &(settings->shadowOtherUsers)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Hide kernel processes"), &(settings->hide_kernel_processes)));
#ifdef PLATFORM_PRESENT_THREADS_AS_PROCESSES
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Hide additional thread processes"), &(settings->hide_thread_processes)));
#if defined __OpenBSD__ && defined PID_AND_MAIN_THREAD_ID_DIFFER
   Panel_add(super, (Object *)CheckItem_newByRef(xStrdup("Hide high-level processes"), &settings->hide_high_level_processes));
#endif
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Display additional threads in a different color"), &(settings->highlightThreads)));
#endif
   Panel_add(super, (Object*)CheckItem_newByRef(xStrdup("Display kernel processes in a different color"), &(settings->highlight_kernel_processes)));
#ifdef PLATFORM_PRESENT_THREADS_AS_PROCESSES
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Show custom thread names"), &(settings->showThreadNames)));
#endif
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Show program path"), &(settings->showProgramPath)));
   Panel_add(super, (Object *)CheckItem_newByRef(xStrdup("Highlight newly created processes"), &settings->highlight_new_processes));
   Panel_add(super, (Object *)CheckItem_newByRef(xStrdup("Show kernel process/thread count in task counter"), &settings->tasks_meter_show_kernel_process_count));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Highlight program \"basename\""), &(settings->highlightBaseName)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Highlight large numbers in memory counters"), &(settings->highlightMegabytes)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Update process names on every refresh"), &(settings->updateProcessNames)));
#ifdef DISK_STATS
   }
#endif
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Leave a margin around header"), &(settings->headerMargin)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Detailed CPU time (System/IO-Wait/Hard-IRQ/Soft-IRQ/Steal/Guest)"), &(settings->detailedCPUTime)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Count CPUs from 0 instead of 1"), &(settings->countCPUsFromZero)));
   Panel_add(super, (Object*) CheckItem_newByRef(xStrdup("Add guest time in CPU meter percentage"), &(settings->accountGuestInCPUMeter)));
   this->case_insensitive_sort_check_item = CheckItem_newByVal(xStrdup("Case-insensitive sort"), settings->sort_strcmp == strcasecmp);
   Panel_add(super, (Object *)this->case_insensitive_sort_check_item);
   Panel_add(super, (Object *)CheckItem_newByRef(xStrdup("Explicitly delay between updates to workaround a ncurses issue"), &settings->explicit_delay));
   return this;
}
