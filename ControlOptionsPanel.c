/*
htop - ControlOptionsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#include "ControlOptionsPanel.h"
#include "CheckItem.h"
#include "CRT.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/*{
#include "Panel.h"
#include "Settings.h"
#include "ScreenManager.h"

typedef struct ControlOptionsPanel_ {
   Panel super;

   Settings* settings;
   ScreenManager* scr;
} ControlOptionsPanel;

}*/

static const char* const ControlOptionsFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static void ControlOptionsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   ControlOptionsPanel* this = (ControlOptionsPanel*) object;
   Panel_done(super);
   free(this);
}

static HandlerResult ControlOptionsPanel_eventHandler(Panel* super, int ch, int repeat) {
   ControlOptionsPanel* this = (ControlOptionsPanel*) super;
   HandlerResult result = IGNORED;
   CheckItem* selected = (CheckItem*) Panel_getSelected(super);

   switch(ch) {
      case 0x0a:
      case 0x0d:
      case KEY_ENTER:
      case KEY_MOUSE:
      case KEY_RECLICK:
      case ' ':
         CheckItem_set(selected, ! (CheckItem_get(selected)) );
         result = HANDLED;
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

PanelClass ControlOptionsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = ControlOptionsPanel_delete
   },
   .eventHandler = ControlOptionsPanel_eventHandler
};

ControlOptionsPanel* ControlOptionsPanel_new(Settings* settings, ScreenManager* scr) {
   ControlOptionsPanel* this = AllocThis(ControlOptionsPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(ControlOptionsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(CheckItem), true, fuBar);

   this->settings = settings;
   this->scr = scr;

   Panel_setHeader(super, "Control options");
   Panel_add(super, (Object *)CheckItem_newByRef(xStrdup("Use vi(1)-style key-bindings"), &settings->vi_mode));
#ifdef HAVE_MOUSEMASK
   Panel_add(super, (Object *)CheckItem_newByRef(xStrdup("Use mouse control"), &settings->use_mouse));
#endif
#ifdef DISK_STATS
   if(!settings->disk_mode) {
#endif
      Panel_add(super, (Object *)CheckItem_newByRef(xStrdup("Update process names on CTRL-L"), &settings->update_process_names_on_ctrl_l));
#ifdef DISK_STATS
   }
#endif
   return this;
}
