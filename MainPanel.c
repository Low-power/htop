/*
htop - MainPanel.c
(C) 2004-2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "Panel.h"
#include "Action.h"
#include "IncSet.h"

typedef struct MainPanel_ {
   Panel super;
   State* state;
   IncSet* inc;
   Htop_Action *keys;
   pid_t pidSearch;
} MainPanel;

typedef union {
   int i;
   void* v;
} Arg;

typedef bool(*MainPanel_ForeachProcessFn)(Process*, Arg);

#define MainPanel_getFunctionBar(this_) (((Panel*)(this_))->defaultBar)

}*/

#include "MainPanel.h"
#include "Process.h"
#include "Platform.h"
#include "CRT.h"
#include "Settings.h"
#include "local-curses.h"
#include <string.h>
#include <stdlib.h>

#if defined ERR && ERR > 0
#undef ERR
#define ERR (-1)
#endif

static const char* const MainFunctions[]  = {"Help  ", "Setup ", "Search", "Filter", "Tree  ", "SortBy", "Nice -", "Nice +", "Kill  ", "Quit  ", NULL};

void MainPanel_updateTreeFunctions(MainPanel* this, bool mode) {
   FunctionBar* bar = MainPanel_getFunctionBar(this);
   if (mode) {
      FunctionBar_setLabel(bar, KEY_F(5), "Sorted");
      FunctionBar_setLabel(bar, KEY_F(6), "Collap");
   } else {
      FunctionBar_setLabel(bar, KEY_F(5), "Tree  ");
      FunctionBar_setLabel(bar, KEY_F(6), "SortBy");
   }
}

void MainPanel_pidSearch(MainPanel* this, int ch) {
   Panel* super = (Panel*) this;
   pid_t pid = ch-48 + this->pidSearch;
   for (int i = 0; i < Panel_size(super); i++) {
      Process* p = (Process*) Panel_get(super, i);
      if (p && p->pid == pid) {
         Panel_setSelected(super, i);
         break;
      }
   }
   this->pidSearch = pid * 10;
   if (this->pidSearch > 10000000) {
      this->pidSearch = 0;
   }
}

static HandlerResult MainPanel_eventHandler(Panel* super, int ch, int repeat) {
   MainPanel* this = (MainPanel*) super;

   HandlerResult result = IGNORED;
   Htop_Reaction reaction = HTOP_OK;

   if (EVENT_IS_HEADER_CLICK(ch)) {
      int x = EVENT_HEADER_CLICK_GET_X(ch);
      ProcessList* pl = this->state->pl;
      Settings* settings = this->state->settings;
      int hx = super->scrollH + x + 1;
      ProcessField field = ProcessList_keyAt(pl, hx);
      if (field == settings->sortKey) {
         Settings_invertSortOrder(settings);
         settings->treeView = false;
      } else {
         reaction |= Action_setSortKey(settings, field);
      }
      reaction |= HTOP_RECALCULATE | HTOP_REDRAW_BAR | HTOP_SAVE_SETTINGS;
      result = HANDLED;
   } else if (ch != ERR && this->inc->active) {
      bool filterChanged = IncSet_handleKey(this->inc, ch, super, (IncMode_GetPanelValue) MainPanel_getValue, NULL);
      if (filterChanged) {
         this->state->pl->incFilter = IncSet_filter(this->inc);
         reaction = HTOP_REFRESH | HTOP_REDRAW_BAR;
      }
      if (this->inc->found) {
         reaction |= Action_follow(this->state);
         reaction |= HTOP_KEEP_FOLLOWING;
      }
      result = HANDLED;
   } else if (!this->state->settings->vi_mode && ch < 256 && isdigit(ch)) {
      MainPanel_pidSearch(this, ch);
   } else switch(ch) {
      case ERR:
      case KEY_LEFT:
      case KEY_RIGHT:
         reaction |= HTOP_KEEP_FOLLOWING;
         break;
      case 0x1b:
         return HANDLED;
      default:
         if(ch > 0 && ch < KEY_MAX && this->keys[ch]) {
            this->state->repeat = repeat;
            reaction |= this->keys[ch](this->state);
            result = HANDLED;
         } else {
            this->pidSearch = 0;
         }
         break;
   }

   if (reaction & HTOP_REDRAW_BAR) {
      MainPanel_updateTreeFunctions(this, this->state->settings->treeView);
      IncSet_drawBar(this->inc);
   }
   if (reaction & HTOP_UPDATE_PANELHDR) {
      ProcessList_printHeader(this->state->pl, Panel_getHeader(super));
   }
   if (reaction & HTOP_REFRESH) {
      result |= REDRAW;
   }
   if (reaction & HTOP_RECALCULATE) {
      result |= RESCAN;
   }
   if (reaction & HTOP_SAVE_SETTINGS) {
      this->state->settings->changed = true;
   }
   if (reaction & HTOP_QUIT) {
      return BREAK_LOOP;
   }
   if (!(reaction & HTOP_KEEP_FOLLOWING)) {
      this->state->pl->following = -1;
      Panel_setSelectionColor(super, CRT_colors[HTOP_PANEL_SELECTION_FOCUS_COLOR]);
   }
   return result;
}

static bool MainPanel_isInsertMode(const Panel *super) {
	const MainPanel *this = (const MainPanel *)super;
	return this->inc->active != NULL;
}

static void MainPanel_placeCursor(const Panel *super) {
	const MainPanel *this = (const MainPanel *)super;
	if(this->inc->active) move(LINES - 1, CRT_cursorX);
}

int MainPanel_selectedPid(MainPanel* this) {
   Process* p = (Process*) Panel_getSelected((Panel*)this);
   if (p) {
      return p->pid;
   }
   return -1;
}

const char* MainPanel_getValue(MainPanel* this, int i) {
   Process* p = (Process*) Panel_get((Panel*)this, i);
   if (p)
      return p->comm;
   return "";
}

bool MainPanel_foreachProcess(MainPanel* this, MainPanel_ForeachProcessFn fn, Arg arg, bool* wasAnyTagged) {
   Panel* super = (Panel*) this;
   bool ok = true;
   bool anyTagged = false;
   for (int i = 0; i < Panel_size(super); i++) {
      Process* p = (Process*) Panel_get(super, i);
      if (p->tag) {
         ok = fn(p, arg) && ok;
         anyTagged = true;
      }
   }
   if (!anyTagged) {
      Process* p = (Process*) Panel_getSelected(super);
      if (p) ok = fn(p, arg) && ok;
   }
   if (wasAnyTagged)
      *wasAnyTagged = anyTagged;
   return ok;
}

PanelClass MainPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = MainPanel_delete
   },
   .eventHandler = MainPanel_eventHandler,
   .isInsertMode = MainPanel_isInsertMode,
   .placeCursor = MainPanel_placeCursor
};

MainPanel* MainPanel_new() {
   MainPanel* this = AllocThis(MainPanel);
   Panel_init((Panel*) this, 1, 1, 1, 1, Class(Process), false, FunctionBar_new(MainFunctions, NULL, NULL));
   this->keys = xCalloc(KEY_MAX, sizeof(Htop_Action));
   this->inc = IncSet_new(MainPanel_getFunctionBar(this));

   Action_setBindings(this->keys);
   Platform_setBindings(this->keys);

   return this;
}

void MainPanel_setState(MainPanel* this, const State *state) {
   this->state = xMalloc(sizeof(State));
   memcpy(this->state, state, sizeof(State));
}

void MainPanel_delete(Object *super) {
   MainPanel *this = (MainPanel *)super;
   Panel_done((Panel *)super);
   free(this->state);
   IncSet_delete(this->inc);
   free(this->keys);
   free(this);
}
