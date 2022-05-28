/*
htop - CategoriesPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "CategoriesPanel.h"
#include "AvailableMetersPanel.h"
#include "MetersPanel.h"
#include "DisplayOptionsPanel.h"
#include "ColumnsPanel.h"
#include "ColorsPanel.h"
#include "AvailableColumnsPanel.h"
#include "ControlOptionsPanel.h"
#include <assert.h>
#include <stdlib.h>

/*{
#include "Panel.h"
#include "Settings.h"
#include "ScreenManager.h"
#include "ProcessList.h"

typedef struct CategoriesPanel_ {
   Panel super;
   ScreenManager* scr;

   Settings* settings;
   Header* header;
   ProcessList* pl;
} CategoriesPanel;

}*/

static const char* const CategoriesFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static void CategoriesPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   CategoriesPanel* this = (CategoriesPanel*) object;
   Panel_done(super);
   free(this);
}

void CategoriesPanel_makeMetersPage(CategoriesPanel* this) {
   MetersPanel* leftMeters = MetersPanel_new(this->settings, "Left column", this->header->columns[0], this->scr);
   MetersPanel* rightMeters = MetersPanel_new(this->settings, "Right column", this->header->columns[1], this->scr);
   leftMeters->rightNeighbor = rightMeters;
   rightMeters->leftNeighbor = leftMeters;
   Panel* availableMeters = (Panel*) AvailableMetersPanel_new(this->settings, this->header, (Panel*) leftMeters, (Panel*) rightMeters, this->scr, this->pl);
   ScreenManager_add(this->scr, (Panel*) leftMeters, 20);
   ScreenManager_add(this->scr, (Panel*) rightMeters, 20);
   ScreenManager_add(this->scr, availableMeters, -1);
}

static void CategoriesPanel_makeDisplayOptionsPage(CategoriesPanel* this) {
   Panel* displayOptions = (Panel*) DisplayOptionsPanel_new(this->settings, this->scr);
   ScreenManager_add(this->scr, displayOptions, -1);
}

static void CategoriesPanel_makeControlOptionsPage(CategoriesPanel *this) {
   Panel *panel = (Panel *)ControlOptionsPanel_new(this->settings, this->scr);
   ScreenManager_add(this->scr, panel, -1);
}

static void CategoriesPanel_makeColorsPage(CategoriesPanel* this) {
   Panel* colors = (Panel*) ColorsPanel_new(this->settings, this->scr);
   ScreenManager_add(this->scr, colors, -1);
}

static void CategoriesPanel_makeColumnsPage(CategoriesPanel* this) {
   Panel* columns = (Panel*) ColumnsPanel_new(this->settings);
   Panel* availableColumns = (Panel*) AvailableColumnsPanel_new(columns);
   ScreenManager_add(this->scr, columns, 20);
   ScreenManager_add(this->scr, availableColumns, -1);
}

static void (*const make_panel_functions[])(CategoriesPanel *) = {
	CategoriesPanel_makeMetersPage,
	CategoriesPanel_makeDisplayOptionsPage,
	CategoriesPanel_makeColorsPage,
	CategoriesPanel_makeColumnsPage,
	CategoriesPanel_makeControlOptionsPage,
};

static HandlerResult CategoriesPanel_eventHandler(Panel* super, int ch, int repeat) {
   CategoriesPanel* this = (CategoriesPanel*) super;

   HandlerResult result = IGNORED;

   int selected = Panel_getSelectedIndex(super);
   switch (ch) {
         int previous;
      case EVENT_SET_SELECTED:
         result = HANDLED;
         break;
      case KEY_UP:
      case KEY_CTRL('P'):
      case KEY_DOWN:
      case KEY_CTRL('N'):
      case KEY_NPAGE:
      case KEY_PPAGE:
      case KEY_HOME:
      case KEY_END:
         previous = selected;
         Panel_onKey(super, ch, repeat);
         selected = Panel_getSelectedIndex(super);
         if (previous != selected) result = HANDLED;
         break;
      default:
         if (ch < 255 && isalpha(ch)) result = Panel_selectByTyping(super, ch);
         if (result == BREAK_LOOP) result = IGNORED;
         break;
   }
   if (result == HANDLED) {
      int size = ScreenManager_size(this->scr);
      for (int i = 1; i < size; i++) {
         ScreenManager_remove(this->scr, 1);
      }
      assert(selected >= 0);
      assert((unsigned int)selected < sizeof make_panel_functions / sizeof *make_panel_functions);
      make_panel_functions[selected](this);
   }
   return result;
}

PanelClass CategoriesPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = CategoriesPanel_delete
   },
   .eventHandler = CategoriesPanel_eventHandler
};

CategoriesPanel* CategoriesPanel_new(ScreenManager* scr, Settings* settings, Header* header, ProcessList* pl) {
   CategoriesPanel* this = AllocThis(CategoriesPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(CategoriesFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, fuBar);

   this->scr = scr;
   this->settings = settings;
   this->header = header;
   this->pl = pl;
   if(settings->terminal_type) {
      char title[17];
      snprintf(title, sizeof title, "Setup (%s)", settings->terminal_type);
      Panel_setHeader(super, title);
   } else {
      Panel_setHeader(super, "Setup");
   }
   Panel_add(super, (Object *)ListItem_new("Meters", 0, settings));
   Panel_add(super, (Object *)ListItem_new("Display options", 0, settings));
   Panel_add(super, (Object *)ListItem_new("Colors", 0, settings));
   Panel_add(super, (Object *)ListItem_new("Columns", 0, settings));
   Panel_add(super, (Object *)ListItem_new("Control options", 0, settings));
   return this;
}
