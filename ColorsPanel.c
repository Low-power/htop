/*
htop - ColorsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ColorsPanel.h"

#include "CRT.h"
#include "CheckItem.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/*{
#include "Panel.h"
#include "Settings.h"
#include "ScreenManager.h"

typedef struct ColorsPanel_ {
   Panel super;

   Settings* settings;
   ScreenManager* scr;
} ColorsPanel;

}*/

static const char* const ColorsFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static void ColorsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   ColorsPanel* this = (ColorsPanel*) object;
   Panel_done(super);
   free(this);
}

static HandlerResult ColorsPanel_eventHandler(Panel* super, int ch, int repeat) {
   ColorsPanel* this = (ColorsPanel*) super;
   HandlerResult result = IGNORED;
   int mark = Panel_getSelectedIndex(super);

   switch(ch) {
   case 0x0a:
   case 0x0d:
   case KEY_ENTER:
   case KEY_MOUSE:
   case KEY_RECLICK:
   case ' ':
      for (int i = 0; i < CRT_color_scheme_count; i++) {
         CheckItem_set((CheckItem*)Panel_get(super, i), false);
      }
      CheckItem_set((CheckItem*)Panel_get(super, mark), true);
      this->settings->colorScheme = mark;
      result = HANDLED;
   }

   if (result == HANDLED) {
      this->settings->changed = true;
      const Header* header = this->scr->header;
      CRT_setColors(mark);
      clear();
      Panel* menu = (Panel*) Vector_get(this->scr->panels, 0);
      Panel_setSelectionColor(menu, CRT_colors[HTOP_PANEL_SELECTION_FOCUS_COLOR]);
      Panel_setSelectionColor(super, CRT_colors[HTOP_PANEL_SELECTION_FOCUS_COLOR]);
      FunctionBar_draw(super->currentBar, NULL);
      Header_draw(header);
      RichString_setAttr(&(super->header), CRT_colors[HTOP_PANEL_HEADER_FOCUS_COLOR]);
      RichString_setAttr(&(menu->header), CRT_colors[HTOP_PANEL_HEADER_UNFOCUS_COLOR]);
      ScreenManager_resize(this->scr, this->scr->x1, header->height, this->scr->x2, this->scr->y2);
   }
   return result;
}

PanelClass ColorsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = ColorsPanel_delete
   },
   .eventHandler = ColorsPanel_eventHandler
};

ColorsPanel* ColorsPanel_new(Settings* settings, ScreenManager* scr) {
   ColorsPanel* this = AllocThis(ColorsPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(ColorsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(CheckItem), true, fuBar);

   this->settings = settings;
   this->scr = scr;

   Panel_setHeader(super, "Colors");
   for (int i = 0; i < CRT_color_scheme_count; i++) {
      Panel_add(super, (Object *)CheckItem_newByVal(xStrdup(CRT_color_scheme_names[i]), false));
   }
   CheckItem_set((CheckItem*)Panel_get(super, settings->colorScheme), true);
   return this;
}
