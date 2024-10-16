/*
htop - ScreenManager.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "FunctionBar.h"
#include "Vector.h"
#include "Header.h"
#include "Settings.h"
#include "Panel.h"

typedef enum Orientation_ {
   VERTICAL,
   HORIZONTAL
} Orientation;

typedef struct ScreenManager_ {
   int x1;
   int y1;
   int x2;
   int y2;
   Orientation orientation;
   Vector* panels;
   int panelCount;
   Header *header;
   const Settings* settings;
   bool owner;
   bool allowFocusChange;
} ScreenManager;

}*/

#include "config.h"
#include "ScreenManager.h"
#include "ProcessList.h"
#include "Object.h"
#include "CRT.h"
#include "local-curses.h"
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>

#if defined ERR && ERR > 0
#undef ERR
#define ERR (-1)
#endif

ScreenManager* ScreenManager_new(int x1, int y1, int x2, int y2, Orientation orientation, Header *header, const Settings* settings, bool owner) {
   ScreenManager* this;
   this = xMalloc(sizeof(ScreenManager));
   this->x1 = x1;
   this->y1 = y1;
   this->x2 = x2;
   this->y2 = y2;
   this->orientation = orientation;
   this->panels = Vector_new(Class(Panel), owner, DEFAULT_SIZE);
   this->panelCount = 0;
   this->header = header;
   this->settings = settings;
   this->owner = owner;
   this->allowFocusChange = true;
   return this;
}

void ScreenManager_delete(ScreenManager* this) {
   Vector_delete(this->panels);
   free(this);
}

inline int ScreenManager_size(ScreenManager* this) {
   return this->panelCount;
}

void ScreenManager_add(ScreenManager* this, Panel* item, int size) {
   if (this->orientation == HORIZONTAL) {
      int lastX = 0;
      if (this->panelCount > 0) {
         Panel* last = (Panel*) Vector_get(this->panels, this->panelCount - 1);
         lastX = last->x + last->w + 1;
      }
      int height = LINES - this->y1 + this->y2;
      if (size > 0) {
         Panel_resize(item, size, height);
      } else {
         Panel_resize(item, COLS-this->x1+this->x2-lastX, height);
      }
      Panel_move(item, lastX, this->y1);
   }
   // TODO: VERTICAL
   Vector_add(this->panels, item);
   item->needsRedraw = true;
   this->panelCount++;
}

Panel* ScreenManager_remove(ScreenManager* this, int idx) {
   assert(this->panelCount > idx);
   Panel* panel = (Panel*) Vector_remove(this->panels, idx);
   this->panelCount--;
   return panel;
}

void ScreenManager_resize(ScreenManager* this, int x1, int y1, int x2, int y2) {
   this->x1 = x1;
   this->y1 = y1;
   this->x2 = x2;
   this->y2 = y2;
   int panels = this->panelCount;
   if (this->orientation == HORIZONTAL) {
      int lastX = 0;
      for (int i = 0; i < panels - 1; i++) {
         Panel* panel = (Panel*) Vector_get(this->panels, i);
         Panel_resize(panel, panel->w, LINES-y1+y2);
         Panel_move(panel, lastX, y1);
         lastX = panel->x + panel->w + 1;
      }
      Panel* panel = (Panel*) Vector_get(this->panels, panels-1);
      Panel_resize(panel, COLS-x1+x2-lastX, LINES-y1+y2);
      Panel_move(panel, lastX, y1);
   }
   // TODO: VERTICAL
}

static void checkRecalculation(ScreenManager* this, double* oldTime, int* sortTimeout, bool* redraw, bool *rescan, bool *timedOut) {
   ProcessList* pl = this->header->pl;

   struct timeval tv;
   gettimeofday(&tv, NULL);
   double newTime = ((double)tv.tv_sec * 10) + ((double)tv.tv_usec / 100000);
   *timedOut = (newTime - *oldTime > this->settings->delay);
   if (*timedOut || newTime < *oldTime || newTime - *oldTime > 255) {
      // timed out or clock was obviously adjusted
      *rescan = true;
   }
   if (*rescan) {
#ifdef DISK_STATS
      double interval = (newTime - *oldTime) / 10;
#endif
      *oldTime = newTime;
#ifdef DISK_STATS
      if(this->header->disk_list) {
         DiskList_scan(this->header->disk_list, interval);
         ProcessList_scan(pl, true);
         if(*sortTimeout <= 0) {
            DiskList_sort(this->header->disk_list);
            *sortTimeout = 1;
         }
      } else
#endif
      {
         ProcessList_scan(pl, false);
         if (*sortTimeout <= 0 || this->settings->treeView) {
            ProcessList_sort(pl);
            *sortTimeout = 1;
         }
      }
      *redraw = true;
   }
   if (*redraw) {
#ifdef DISK_STATS
      if(this->header->disk_list) DiskList_rebuildPanel(this->header->disk_list);
      else
#endif
      ProcessList_rebuildPanel(pl);
      Header_draw(this->header);
   }
   *rescan = false;
}

static void ScreenManager_drawPanels(ScreenManager* this, int focus) {
   const int nPanels = this->panelCount;
   for (int i = 0; i < nPanels; i++) {
      Panel* panel = (Panel*) Vector_get(this->panels, i);
      Panel_draw(panel, i == focus);
      if (this->orientation == HORIZONTAL) {
         mvvline(panel->y, panel->x+panel->w, ' ', panel->h+1);
      }
   }
}

static Panel* setCurrentPanel(Panel* panel) {
   FunctionBar_draw(panel->currentBar, NULL);
   return panel;
}

void ScreenManager_run(ScreenManager* this, Panel** lastFocus, int* lastKey) {
   bool quit = false;
   int focus = 0;
   Panel *focused_panel = setCurrentPanel((Panel*) Vector_get(this->panels, focus));

   double oldTime = 0.0;

   int ch = ERR;
   int closeTimeout = 0;

   bool timedOut = true;
   bool redraw = true;
   bool rescan = false;
   int sortTimeout = 0;
   int resetSortTimeout = 5;

   while (!quit) {
      if (this->header) {
         checkRecalculation(this, &oldTime, &sortTimeout, &redraw, &rescan, &timedOut);
      }
      if (redraw) {
         ScreenManager_drawPanels(this, focus);
         Panel_placeCursor(focused_panel);
         refresh();
      }

      int prevCh = ch;
      CRT_explicitDelay();
      ch = getch();

      HandlerResult result = IGNORED;
      if (ch == KEY_MOUSE) {
         ch = ERR;
#ifdef HAVE_NCURSES_GETMOUSE
         MEVENT mevent;
         if (getmouse(&mevent) == OK) {
            if (mevent.bstate & BUTTON1_RELEASED) {
               if (mevent.y == LINES - 1) {
                  ch = FunctionBar_synthesizeEvent(focused_panel->currentBar, mevent.x);
               } else {
                  for (int i = 0; i < this->panelCount; i++) {
                     Panel* panel = (Panel*) Vector_get(this->panels, i);
                     if (mevent.x >= panel->x && mevent.x <= panel->x+panel->w) {
                        if (mevent.y == panel->y) {
                           ch = EVENT_HEADER_CLICK(mevent.x - panel->x);
                           break;
                        }
                        if (mevent.y > panel->y && mevent.y <= panel->y+panel->h) {
                           ch = KEY_MOUSE;
                           if (panel == focused_panel || this->allowFocusChange) {
                              focus = i;
                              focused_panel = setCurrentPanel(panel);
                              Object* oldSelection = Panel_getSelected(panel);
                              Panel_onMouseSelect(panel, mevent.y);
                              if (Panel_getSelected(panel) == oldSelection) {
                                 ch = KEY_RECLICK;
                              }
                           }
                           break;
                        }
                     }
                  }
               }
            #if NCURSES_MOUSE_VERSION > 1
            } else if (mevent.bstate & BUTTON4_PRESSED) {
               ch = KEY_WHEELUP;
            } else if (mevent.bstate & BUTTON5_PRESSED) {
               ch = KEY_WHEELDOWN;
            #endif
            }
         }
#endif
      }
      if (ch == ERR) {
         if(sortTimeout > 0) sortTimeout--;
         if (prevCh == ch && !timedOut) {
            closeTimeout++;
            if (closeTimeout == 100) {
               break;
            }
         } else {
            closeTimeout = 0;
         }
         redraw = false;
         continue;
      }
      unsigned int repeat = 1;
      if(!Panel_isInsertMode(focused_panel)) {
         if(this->settings->vi_mode) {
            repeat = Panel_getViModeRepeatForKey(focused_panel, &ch);
            if(!repeat) continue;
         } else switch (ch) {
            case KEY_ALT('H'): ch = KEY_LEFT; break;
            case KEY_ALT('J'): ch = KEY_DOWN; break;
            case KEY_ALT('K'): ch = KEY_UP; break;
            case KEY_ALT('L'): ch = KEY_RIGHT; break;
         }
      }
      redraw = true;
      if (Panel_eventHandlerFn(focused_panel)) {
         result = Panel_eventHandler(focused_panel, ch, repeat);
      }
      if (result & SYNTH_KEY) {
         ch = result >> 16;
      }
      if (result & REDRAW) {
         sortTimeout = 0;
      }
      if (result & RESCAN) {
         rescan = true;
         sortTimeout = 0;
      }
      if (result & HANDLED) {
         continue;
      } else if (result & BREAK_LOOP) {
         quit = true;
         continue;
      }
      switch (ch) {
#ifdef KEY_RESIZE
         case KEY_RESIZE:
            ScreenManager_resize(this, this->x1, this->y1, this->x2, this->y2);
            continue;
#endif
         case KEY_LEFT:
         case KEY_CTRL('B'):
            if (this->panelCount < 2) goto defaultHandler;
            if (!this->allowFocusChange) break;
         tryLeft:
            if (focus > 0) focus--;
            focused_panel = setCurrentPanel((Panel*) Vector_get(this->panels, focus));
            if (Panel_size(focused_panel) == 0 && focus > 0) goto tryLeft;
            break;
         case KEY_RIGHT:
         case KEY_CTRL('F'):
         case 9:
            if (this->panelCount < 2) goto defaultHandler;
            if (!this->allowFocusChange) break;
         tryRight:
            if (focus < this->panelCount - 1) focus++;
            focused_panel = setCurrentPanel((Panel*) Vector_get(this->panels, focus));
            if (Panel_size(focused_panel) == 0 && focus < this->panelCount - 1) goto tryRight;
            break;
         case KEY_F(10):
         case 'q':
         case 27:
            quit = true;
            continue;
         default:
         defaultHandler:
            sortTimeout = resetSortTimeout;
            Panel_onKey(focused_panel, ch, repeat);
            break;
      }
   }

   if (lastFocus) *lastFocus = focused_panel;
   if (lastKey) *lastKey = ch;
}
