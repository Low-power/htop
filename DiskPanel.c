/*
htop - DiskPanel.c
(C) 2004-2015 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "Panel.h"
#include "Settings.h"
#include "DiskList.h"
#include "Header.h"
#include "IncSet.h"
#include "Object.h"

typedef struct {
	Panel super;
	Settings *settings;
	DiskList *disk_list;
	Header *header;
	IncSet *inc;
} DiskPanel;

#define DiskPanel_getFunctionBar(this_) (((Panel *)(this_))->defaultBar)
}*/

#include "DiskPanel.h"
#include "Disk.h"
#include "CRT.h"
#include "Settings.h"
#include "Action.h"
#include "local-curses.h"
#include <stdbool.h>
#include <stdlib.h>

#if defined ERR && ERR > 0
#undef ERR
#define ERR (-1)
#endif

static const char *const DiskPanel_functions[] = {"      ", "Setup ", "Search", "      ", "      ", "SortBy", "      ", "      ", "      ", "Quit  ", NULL};

static bool DiskPanel_selectSortKey(DiskPanel *this) {
	Panel *panel = Panel_new(0, 0, 0, 0, true, Class(ListItem),
		FunctionBar_newEnterEsc("Sort   ", "Cancel "));
	Panel_setHeader(panel, "Sort by");
	const unsigned int *fields = this->settings->disk_fields;
	for(int i = 0; fields[i]; i++) {
		const FieldData *field_data = Disk_fields + fields[i];
		const char *name = field_data->name;
		Panel_add(panel, (Object *)ListItem_new(name, HTOP_DEFAULT_COLOR, fields[i], this->settings));
		if(fields[i] == this->settings->disk_sort_key) Panel_setSelected(panel, i);
	}
	bool r = false;
	State state = { .settings = this->settings, .panel = (Panel *)this, .header = this->header, .repeat = 1 };
	ListItem *item = (ListItem *)Action_pickFromVector(&state, panel, 15, false);
	if(item) {
		this->settings->disk_sort_key = item->key;
		this->settings->direction = 1;
		r = true;
	}
	Object_delete(panel);
	return r;
}

static HandlerResult DiskPanel_eventHandler(Panel *super, int ch, int repeat) {
   DiskPanel *this = (DiskPanel *)super;
   if (EVENT_IS_HEADER_CLICK(ch)) {
      int x = EVENT_HEADER_CLICK_GET_X(ch);
      int hx = super->scrollH + x + 1;
      DiskField field = DiskList_keyAt(this->disk_list, hx);
      if (field == this->settings->disk_sort_key) {
         Settings_invertSortOrder(this->settings);
      } else {
         this->settings->disk_sort_key = field;
         this->settings->direction = 1;
      }
      this->settings->changed = true;
      DiskList_printHeader(this->disk_list, Panel_getHeader(super));
      return HANDLED | REDRAW | RESCAN;
   }
   if (ch != ERR && this->inc->active) {
      if(IncSet_handleKey(this->inc, ch, super, (IncMode_GetPanelValue)DiskPanel_getValue, NULL)) {
         //incFilter = IncSet_filter(this->inc);
         //reaction = HTOP_REFRESH | HTOP_REDRAW_BAR;
         IncSet_drawBar(this->inc);
      }
      if (this->inc->found) {
         //reaction |= Action_follow(this->state);
         //reaction |= HTOP_KEEP_FOLLOWING;
      }
      return HANDLED | REDRAW;
   }
   switch(ch) {
/*
      case ERR:
      case KEY_LEFT:
      case KEY_RIGHT:
         //reaction |= HTOP_KEEP_FOLLOWING;
*/
      case 0x1b:
         return HANDLED;
      case KEY_F(3):
      case '/':
         IncSet_reset(this->inc, INC_SEARCH);
         IncSet_activate(this->inc, INC_SEARCH, super);
         return HANDLED;
      case 'n':
         IncSet_next(this->inc, INC_SEARCH, super, (IncMode_GetPanelValue)DiskPanel_getValue, repeat);
         return HANDLED;
      case 'N':
         IncSet_prev(this->inc, INC_SEARCH, super, (IncMode_GetPanelValue)DiskPanel_getValue, repeat);
         return HANDLED;
      case KEY_F(2):
      case 'C':
         Header_runSetup(this->header, this->settings, NULL);
         DiskList_printHeader(this->disk_list, Panel_getHeader(super));
         IncSet_drawBar(this->inc);
         return HANDLED | RESCAN;
      case KEY_F(6):
         if(DiskPanel_selectSortKey(this)) {
            this->settings->changed = true;
            DiskList_printHeader(this->disk_list, Panel_getHeader(super));
         }
         IncSet_drawBar(this->inc);
         return HANDLED | REDRAW;
      case KEY_F(10):
      case 'q':
         return BREAK_LOOP;
      default:
         return IGNORED;
   }
}

static bool DiskPanel_isInsertMode(const Panel *super) {
	const DiskPanel *this = (const DiskPanel *)super;
	return this->inc->active != NULL;
}

static void DiskPanel_placeCursor(const Panel *super) {
	const DiskPanel *this = (const DiskPanel *)super;
	if(this->inc->active) move(LINES - 1, CRT_cursorX);
}

const char *DiskPanel_getValue(DiskPanel *this, int i) {
	Disk *disk = (Disk *)Panel_get((Panel *)this, i);
	return disk ? disk->name : "";
}

PanelClass DiskPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = DiskPanel_delete
   },
   .eventHandler = DiskPanel_eventHandler,
   .isInsertMode = DiskPanel_isInsertMode,
   .placeCursor = DiskPanel_placeCursor
};

DiskPanel *DiskPanel_new(Settings *settings, DiskList *disk_list, Header *header) {
	DiskPanel *this = AllocThis(DiskPanel);
	Panel_init((Panel *)this, 1, 1, 1, 1, Class(Disk), false, FunctionBar_new(DiskPanel_functions, NULL, NULL));
	this->settings = settings;
	this->disk_list = disk_list;
	this->header = header;
	this->inc = IncSet_new(DiskPanel_getFunctionBar(this));
	return this;
}

void DiskPanel_delete(Object *this) {
	Panel_done((Panel *)this);
	IncSet_delete(((DiskPanel *)this)->inc);
	free(this);
}
