/*
htop - InfoScreen.c
(C) 2004-2018 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "Process.h"
#include "Panel.h"
#include "FunctionBar.h"
#include "IncSet.h"
#include "Vector.h"
#include "Settings.h"

typedef struct InfoScreen_ InfoScreen;

typedef void(*InfoScreen_Scan)(InfoScreen*);
typedef void(*InfoScreen_Draw)(InfoScreen*);
typedef void(*InfoScreen_OnErr)(InfoScreen*);
typedef bool(*InfoScreen_OnKey)(InfoScreen*, int);

typedef struct InfoScreenClass_ {
   ObjectClass super;
   InfoScreen_Scan scan;
   InfoScreen_Draw draw;
   InfoScreen_OnErr onErr;
   InfoScreen_OnKey onKey;
} InfoScreenClass;

#define As_InfoScreen(this_)          ((InfoScreenClass*)(((InfoScreen*)(this_))->super.klass))
#define InfoScreen_scan(this_)        As_InfoScreen(this_)->scan((InfoScreen*)(this_))
#define InfoScreen_draw(this_)        As_InfoScreen(this_)->draw((InfoScreen*)(this_))
#define InfoScreen_onErr(this_)       As_InfoScreen(this_)->onErr((InfoScreen*)(this_))
#define InfoScreen_onKey(this_, ch_)  As_InfoScreen(this_)->onKey((InfoScreen*)(this_), (ch_))

struct InfoScreen_ {
   Object super;
   const Process *process;
   Panel* display;
   FunctionBar* bar;
   IncSet* inc;
   Vector* lines;
   const Settings *settings;
};
}*/

#include "InfoScreen.h"
#include "Object.h"
#include "CRT.h"
#include "ListItem.h"
#include "Platform.h"
#include "StringUtils.h"
#include "local-curses.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>

#if defined ERR && ERR > 0
#undef ERR
#define ERR (-1)
#endif

static const char* const InfoScreenFunctions[] = {"Search ", "Filter ", "Refresh", "Done   ", NULL};

static const char* const InfoScreenKeys[] = {"F3", "F4", "F5", "Esc"};

static int InfoScreenEvents[] = {KEY_F(3), KEY_F(4), KEY_F(5), 27};

InfoScreen* InfoScreen_init(InfoScreen* this, const Process *process, FunctionBar* bar, int height, char* panelHeader) {
   this->process = process;
   if (!bar) {
      bar = FunctionBar_new(InfoScreenFunctions, InfoScreenKeys, InfoScreenEvents);
   }
   this->display = Panel_new(0, 1, COLS, height, false, Class(ListItem), bar);
   this->inc = IncSet_new(bar);
   this->lines = Vector_new(this->display->items->type, true, DEFAULT_SIZE);
   Panel_setHeader(this->display, panelHeader);
   this->settings = process->settings;
   return this;
}

InfoScreen* InfoScreen_done(InfoScreen* this) {
   Panel_delete((Object*)this->display);
   IncSet_delete(this->inc);
   Vector_delete(this->lines);
   return this;
}

void InfoScreen_drawTitled(InfoScreen* this, char* fmt, ...) {
   va_list ap;
   va_start(ap, fmt);
   attrset(CRT_colors[HTOP_METER_TEXT_COLOR]);
   mvhline(0, 0, ' ', COLS);
   wmove(stdscr, 0, 0);
   vw_printw(stdscr, fmt, ap);
   attrset(CRT_colors[HTOP_DEFAULT_COLOR]);
   this->display->needsRedraw = true;
   Panel_draw(this->display, true);
   IncSet_drawBar(this->inc);
   va_end(ap);
}

void InfoScreen_addLine(InfoScreen* this, const char* line, unsigned int color_index) {
   Vector_add(this->lines, (Object *)ListItem_new(line, color_index, 0, this->settings));
   const char* incFilter = IncSet_filter(this->inc);
   if (!incFilter || String_contains_i(line, incFilter)) {
      Panel_add(this->display, (Object*)Vector_get(this->lines, Vector_size(this->lines)-1));
   }
}

void InfoScreen_appendLine(InfoScreen* this, const char* line) {
   ListItem* last = (ListItem*)Vector_get(this->lines, Vector_size(this->lines)-1);
   ListItem_append(last, line);
   const char* incFilter = IncSet_filter(this->inc);
   if (incFilter && Panel_get(this->display, Panel_size(this->display)-1) != (Object*)last && String_contains_i(line, incFilter))
      Panel_add(this->display, (Object*)last);
}

void InfoScreen_run(InfoScreen* this) {
   Panel* panel = this->display;

   if (As_InfoScreen(this)->scan) InfoScreen_scan(this);
   InfoScreen_draw(this);

   bool looping = true;
   while (looping) {

      Panel_draw(panel, true);
      refresh();

      if (this->inc->active) {
         (void) move(LINES-1, CRT_cursorX);
      }
      CRT_explicitDelay();
      int ch = getch();
      if (ch == ERR) {
         if (As_InfoScreen(this)->onErr) {
            InfoScreen_onErr(this);
            continue;
         }
      }

      if (ch == KEY_MOUSE) {
         MEVENT mevent;
         if(getmouse(&mevent) == OK) {
            if (mevent.y >= panel->y && mevent.y < LINES - 1) {
               Panel_setSelected(panel, mevent.y - panel->y + panel->scrollV - 1);
               continue;
            }
            if (mevent.y == LINES - 1) ch = IncSet_synthesizeEvent(this->inc, mevent.x);
         }
      }

      if (this->inc->active) {
         IncSet_handleKey(this->inc, ch, panel, IncSet_getListItemValue, this->lines);
         continue;
      }

      unsigned int repeat = 1;
      if(this->settings->vi_mode) {
         repeat = Panel_getViModeRepeatForKey(panel, &ch);
         if(!repeat) continue;
      }

      switch(ch) {
      case ERR:
         continue;
      case KEY_F(3):
      case '/':
         IncSet_activate(this->inc, INC_SEARCH, panel);
         break;
      case KEY_F(4):
      case '\\':
         IncSet_activate(this->inc, INC_FILTER, panel);
         break;
      case KEY_F(5):
         clear();
         if (As_InfoScreen(this)->scan) InfoScreen_scan(this);
         InfoScreen_draw(this);
         break;
      case '\014': // Ctrl+L
         clear();
         InfoScreen_draw(this);
         break;
      case 'q':
      case 27:
      case KEY_F(10):
         looping = false;
         break;
      case KEY_RESIZE:
         Panel_resize(panel, COLS, LINES-2);
         InfoScreen_draw(this);
         break;
      default:
         if (As_InfoScreen(this)->onKey && InfoScreen_onKey(this, ch)) {
            continue;
         }
         Panel_onKey(panel, ch, repeat);
         break;
      }
   }
}
