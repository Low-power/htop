/*
htop - Panel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

//#link curses

/*{
#include "config.h"
#include "Object.h"
#include "Vector.h"
#include "FunctionBar.h"
#include "local-curses.h"

typedef struct Panel_ Panel;

typedef enum HandlerResult_ {
   HANDLED     = 0x01,
   IGNORED     = 0x02,
   BREAK_LOOP  = 0x04,
   REDRAW      = 0x08,
   RESCAN      = 0x10,
   SYNTH_KEY   = 0x20,
} HandlerResult;

#define EVENT_SET_SELECTED -1

#define EVENT_HEADER_CLICK(x_) (-10000 + (x_))
#define EVENT_IS_HEADER_CLICK(ev_) ((ev_) >= -10000 && (ev_) <= -9000)
#define EVENT_HEADER_CLICK_GET_X(ev_) ((ev_) + 10000)

typedef HandlerResult (*Panel_EventHandler)(Panel *, int, int);
typedef bool (*Panel_BoolFunction)(const Panel *);
typedef void (*Panel_VoidFunction)(const Panel *);
typedef void (*Panel_VoidIntFunction)(Panel *, int);

typedef struct PanelClass_ {
   ObjectClass super;
   Panel_EventHandler eventHandler;
   Panel_BoolFunction isInsertMode;
   Panel_VoidFunction placeCursor;
   Panel_VoidIntFunction onMouseSelect;
} PanelClass;

#define As_Panel(this_)                ((PanelClass*)((this_)->super.klass))
#define Panel_eventHandlerFn(this_)    As_Panel(this_)->eventHandler
#define Panel_eventHandler(this_,ev_,rep_) As_Panel(this_)->eventHandler((Panel*)(this_),(ev_),(rep_))
#define Panel_isInsertMode(this_) (As_Panel(this_)->isInsertMode && As_Panel(this_)->isInsertMode((Panel*)(this_)))
#define Panel_placeCursor(this_) (As_Panel(this_)->placeCursor && (As_Panel(this_)->placeCursor(this_), true))
#define Panel_onMouseSelect(this_,y_) As_Panel(this_)->onMouseSelect((this_), (y_))

struct Panel_ {
   Object super;
   int x, y, w, h;
   WINDOW* window;
   Vector* items;
   int selected;
   int oldSelected;
   int selectedLen;
   void* eventHandlerState;
   int scrollV;
   short scrollH;
   bool needsRedraw;
   FunctionBar* currentBar;
   FunctionBar* defaultBar;
   RichString header;
   int selectionColor;
   char repeat_number_buffer[11];
   unsigned int repeat_number_i;
};

#define Panel_setDefaultBar(this_) do{ (this_)->currentBar = (this_)->defaultBar; }while(0)

}*/

#include "Panel.h"
#include "CRT.h"
#include "RichString.h"
#include "ListItem.h"
#include "StringUtils.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <assert.h>

#if defined ERR && ERR > 0
#undef ERR
#define ERR (-1)
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef ROUND_UP
#define ROUND_UP(A,B) ((((A)+((B)-1))/(B))*(B))
#endif

#define KEY_CTRL(l) ((l)-'A'+1)

static void base_Panel_onMouseSelect(Panel *this, int y) {
   Panel_setSelected(this, y - this->y + this->scrollV - 1);
}

static void Panel_inherit(ObjectClass *super_class) {
   PanelClass *class = (PanelClass *)super_class;
   if(!class->onMouseSelect) class->onMouseSelect = base_Panel_onMouseSelect;
}

PanelClass Panel_class = {
   .super = {
      .extends = Class(Object),
      .inherit = Panel_inherit,
      .delete = Panel_delete
   },
   .eventHandler = (Panel_EventHandler)Panel_selectByTyping,
   .onMouseSelect = base_Panel_onMouseSelect
};

Panel* Panel_new(int x, int y, int w, int h, bool owner, ObjectClass* type, FunctionBar* fuBar) {
   Panel* this;
   this = xMalloc(sizeof(Panel));
   Object_setClass(this, Class(Panel));
   Panel_init(this, x, y, w, h, type, owner, fuBar);
   return this;
}

void Panel_delete(Object* cast) {
   Panel* this = (Panel*)cast;
   Panel_done(this);
   free(this);
}

void Panel_init(Panel* this, int x, int y, int w, int h, ObjectClass* type, bool owner, FunctionBar* fuBar) {
   this->x = x;
   this->y = y;
   this->w = w;
   this->h = h;
   this->eventHandlerState = NULL;
   this->items = Vector_new(type, owner, DEFAULT_SIZE);
   this->scrollV = 0;
   this->scrollH = 0;
   this->selected = 0;
   this->oldSelected = 0;
   this->needsRedraw = true;
   RichString_beginAllocated(this->header);
   this->defaultBar = fuBar;
   this->currentBar = fuBar;
   this->selectionColor = CRT_colors[HTOP_PANEL_SELECTION_FOCUS_COLOR];
   this->repeat_number_i = 0;
}

void Panel_done(Panel* this) {
   assert (this != NULL);
   free(this->eventHandlerState);
   Vector_delete(this->items);
   FunctionBar_delete(this->defaultBar);
   RichString_end(this->header);
}

void Panel_setSelectionColor(Panel* this, int color) {
   this->selectionColor = color;
}

RichString* Panel_getHeader(Panel* this) {
   assert (this != NULL);

   this->needsRedraw = true;
   return &(this->header);
}

inline void Panel_setHeader(Panel* this, const char* header) {
   RichString_write(&(this->header), CRT_colors[HTOP_PANEL_HEADER_FOCUS_COLOR], header);
   this->needsRedraw = true;
}

void Panel_move(Panel* this, int x, int y) {
   assert (this != NULL);

   this->x = x;
   this->y = y;
   this->needsRedraw = true;
}

void Panel_resize(Panel* this, int w, int h) {
   assert (this != NULL);

   if (RichString_sizeVal(this->header) > 0) h--;
   this->w = w;
   this->h = h;
   this->needsRedraw = true;
}

void Panel_setReservedHeight(Panel *this, int reserved_height) {
   Panel_move(this, 0, reserved_height);
   this->h = LINES - reserved_height - (RichString_sizeVal(this->header) > 0 ? 2 : 1);
   this->needsRedraw = true;
}

void Panel_prune(Panel* this) {
   assert (this != NULL);

   Vector_prune(this->items);
   this->scrollV = 0;
   this->selected = 0;
   this->oldSelected = 0;
   this->needsRedraw = true;
}

void Panel_add(Panel* this, Object* o) {
   assert (this != NULL);

   Vector_add(this->items, o);
   this->needsRedraw = true;
}

void Panel_insert(Panel* this, int i, Object* o) {
   assert (this != NULL);

   Vector_insert(this->items, i, o);
   this->needsRedraw = true;
}

void Panel_set(Panel* this, int i, Object* o) {
   assert (this != NULL);

   Vector_set(this->items, i, o);
}

Object* Panel_get(Panel* this, int i) {
   assert (this != NULL);

   return Vector_get(this->items, i);
}

Object* Panel_remove(Panel* this, int i) {
   assert (this != NULL);

   this->needsRedraw = true;
   Object* removed = Vector_remove(this->items, i);
   if (this->selected > 0 && this->selected >= Vector_size(this->items))
      this->selected--;
   return removed;
}

Object* Panel_getSelected(Panel* this) {
   assert (this != NULL);
   if (Vector_size(this->items) > 0)
      return Vector_get(this->items, this->selected);
   else
      return NULL;
}

void Panel_moveSelectedUp(Panel* this) {
   assert (this != NULL);

   Vector_moveUp(this->items, this->selected);
   if (this->selected > 0)
      this->selected--;
}

void Panel_moveSelectedDown(Panel* this) {
   assert (this != NULL);

   Vector_moveDown(this->items, this->selected);
   if (this->selected + 1 < Vector_size(this->items))
      this->selected++;
}

void Panel_moveSelectedToTop(Panel *this) {
	assert(this != NULL);
	Vector_moveToTop(this->items, this->selected);
	this->selected = 0;
}

void Panel_moveSelectedToBottom(Panel *this) {
	assert(this != NULL);
	Vector_moveToBottom(this->items, this->selected);
	this->selected = Vector_size(this->items) - 1;
}

int Panel_getSelectedIndex(Panel* this) {
   assert (this != NULL);

   return this->selected;
}

int Panel_size(Panel* this) {
   assert (this != NULL);

   return Vector_size(this->items);
}

void Panel_setSelected(Panel* this, int selected) {
   assert (this != NULL);

   int size = Vector_size(this->items);
   if (selected >= size) {
      selected = size - 1;
   }
   if (selected < 0)
      selected = 0;
   this->selected = selected;
   if (Panel_eventHandlerFn(this)) {
      Panel_eventHandler(this, EVENT_SET_SELECTED, 1);
   }
}

void Panel_draw(Panel* this, bool focus) {
   assert (this != NULL);

   int size = Vector_size(this->items);
   int y = this->y;
   int x = this->x;
   int h = this->h;

   assert(this->scrollH >= 0);

   int headerLen = RichString_sizeVal(this->header);
   if (headerLen > 0) {
      attrset(CRT_colors[
            focus ? HTOP_PANEL_HEADER_FOCUS_COLOR : HTOP_PANEL_HEADER_UNFOCUS_COLOR
         ]);
      mvhline(y, x, ' ', this->w);
      if (this->scrollH < headerLen) {
         RichString_printoffnVal(this->header, y, x, this->scrollH,
            MIN(headerLen - this->scrollH, this->w));
      }
      attrset(CRT_colors[HTOP_DEFAULT_COLOR]);
      y++;
   }

   // ensure scroll area is on screen
   if (this->scrollV < 0) {
      this->scrollV = 0;
      this->needsRedraw = true;
   } else if (this->scrollV >= size) {
      this->scrollV = MAX(size - 1, 0);
      this->needsRedraw = true;
   }
   // ensure selection is on screen
   if (this->selected < this->scrollV) {
      this->scrollV = this->selected;
      this->needsRedraw = true;
   } else if (this->selected >= this->scrollV + h) {
      this->scrollV = this->selected - h + 1;
      this->needsRedraw = true;
   }

   int first = this->scrollV;
   int upTo = MIN(first + h, size);

   int selectionColor = focus ?
      this->selectionColor : CRT_colors[HTOP_PANEL_SELECTION_UNFOCUS_COLOR];

   if (this->needsRedraw) {
      int line = 0;
      for(int i = first; line < h && i < upTo; i++) {
         Object* itemObj = Vector_get(this->items, i);
         assert(itemObj); if(!itemObj) continue;
         RichString_begin(item);
         Object_display(itemObj, &item);
         int itemLen = RichString_sizeVal(item);
         int amt = MIN(itemLen - this->scrollH, this->w);
         bool selected = (i == this->selected);
         if (selected) {
            attrset(selectionColor);
            RichString_setAttr(&item, selectionColor);
            this->selectedLen = itemLen;
         }
         mvhline(y + line, x, ' ', this->w);
         if (amt > 0) RichString_printoffnVal(item, y + line, x, this->scrollH, amt);
         if (selected) attrset(CRT_colors[HTOP_DEFAULT_COLOR]);
         RichString_end(item);
         line++;
      }
      while (line < h) {
         mvhline(y + line, x, ' ', this->w);
         line++;
      }
      this->needsRedraw = false;

   } else {
      Object* oldObj = Vector_get(this->items, this->oldSelected);
      assert(oldObj);
      RichString_begin(old);
      Object_display(oldObj, &old);
      int oldLen = RichString_sizeVal(old);
      Object* newObj = Vector_get(this->items, this->selected);
      RichString_begin(new);
      Object_display(newObj, &new);
      int newLen = RichString_sizeVal(new);
      this->selectedLen = newLen;
      mvhline(y+ this->oldSelected - first, x+0, ' ', this->w);
      if (this->scrollH < oldLen) {
         RichString_printoffnVal(old, y+this->oldSelected - first, x,
            this->scrollH, MIN(oldLen - this->scrollH, this->w));
      }
      attrset(selectionColor);
      mvhline(y+this->selected - first, x+0, ' ', this->w);
      RichString_setAttr(&new, selectionColor);
      if (this->scrollH < newLen) {
         RichString_printoffnVal(new, y+this->selected - first, x,
            this->scrollH, MIN(newLen - this->scrollH, this->w));
      }
      attrset(CRT_colors[HTOP_DEFAULT_COLOR]);
      RichString_end(new);
      RichString_end(old);
   }
   this->oldSelected = this->selected;
   move(0, 0);
}

bool Panel_onKey(Panel* this, int key, int repeat) {
   assert (this != NULL);
   int size = Vector_size(this->items);
   switch (key) {
   case KEY_DOWN:
   case KEY_CTRL('N'):
#ifdef KEY_C_DOWN
   case KEY_C_DOWN:
#endif
      this->selected += repeat;
      break;
   case KEY_UP:
   case KEY_CTRL('P'):
#ifdef KEY_C_UP
   case KEY_C_UP:
#endif
      this->selected -= repeat;
      break;
   case KEY_LEFT:
      if (this->scrollH > 0) {
         this->scrollH -= MAX(CRT_scrollHAmount, 0);
         if(this->scrollH < 0) this->scrollH = 0;
         this->needsRedraw = true;
      }
      break;
   case KEY_RIGHT:
      this->scrollH += CRT_scrollHAmount;
      this->needsRedraw = true;
      break;
   case KEY_PPAGE:
   case KEY_CTRL('B'):
      this->selected -= (this->h - 1);
      this->scrollV = MAX(0, this->scrollV - this->h + 1);
      this->needsRedraw = true;
      break;
   case KEY_NPAGE:
   case KEY_CTRL('F'):
      this->selected += (this->h - 1);
      this->scrollV = MAX(0, MIN(Vector_size(this->items) - this->h,
                                 this->scrollV + this->h - 1));
      this->needsRedraw = true;
      break;
   case KEY_WHEELUP:
      this->selected -= CRT_scrollWheelVAmount;
      this->scrollV -= CRT_scrollWheelVAmount;
      this->needsRedraw = true;
      break;
   case KEY_WHEELDOWN:
      this->selected += CRT_scrollWheelVAmount;
      this->scrollV += CRT_scrollWheelVAmount;
      if (this->scrollV > Vector_size(this->items) - this->h) {
         this->scrollV = Vector_size(this->items) - this->h;
      }
      this->needsRedraw = true;
      break;
   case KEY_HOME:
      this->selected = 0;
      break;
   case KEY_END:
      this->selected = size - 1;
      break;
   case '^':
      this->scrollH = 0;
      this->needsRedraw = true;
      break;
   case '$':
      this->scrollH = MAX(ROUND_UP(this->selectedLen - this->w, CRT_scrollHAmount), 0);
      this->needsRedraw = true;
      break;
   default:
      return false;
   }

   // ensure selection within bounds
   if (this->selected < 0 || size == 0) {
      this->selected = 0;
      this->needsRedraw = true;
   } else if (this->selected >= size) {
      this->selected = size - 1;
      this->needsRedraw = true;
   }
   return true;
}

HandlerResult Panel_selectByTyping(Panel* this, int ch) {
   int size = Panel_size(this);
   if (!this->eventHandlerState)
      this->eventHandlerState = xCalloc(100, sizeof(char));
   char* buffer = this->eventHandlerState;

   if (ch > 0 && ch < 255 && isalnum(ch)) {
      int len = strlen(buffer);
      if (len < 99) {
         buffer[len] = ch;
         buffer[len+1] = '\0';
      }
      for (int try = 0; try < 2; try++) {
         len = strlen(buffer);
         for (int i = 0; i < size; i++) {
            char* cur = ((ListItem*) Panel_get(this, i))->value;
            while (*cur == ' ') cur++;
            if (strncasecmp(cur, buffer, len) == 0) {
               Panel_setSelected(this, i);
               return HANDLED;
            }
         }
         // if current word did not match,
         // retry considering the character the start of a new word.
         buffer[0] = ch;
         buffer[1] = '\0';
      }
      return HANDLED;
   } else if (ch != ERR) {
      buffer[0] = '\0';
   }
   if (ch == KEY_ENTER || ch == '\r') {
      return BREAK_LOOP;
   }
   return IGNORED;
}

unsigned int Panel_getViModeRepeatForKey(Panel *this, int *ch) {
   if(*ch == ERR) return 0;
   if(*ch < 256 && *ch > 0 && isdigit(*ch)) {
      this->repeat_number_buffer[this->repeat_number_i] = *ch;
      if(++this->repeat_number_i >= sizeof this->repeat_number_buffer) this->repeat_number_i = 0;
      return 0;
   }
   unsigned int repeat = 1;
   if(this->repeat_number_i > 0) {
      this->repeat_number_buffer[this->repeat_number_i] = 0;
      repeat = atoi(this->repeat_number_buffer);
      if(repeat < 1) repeat = 1;
      this->repeat_number_i = 0;
   }
   switch(*ch) {
      case 'j':
         *ch = KEY_DOWN;
         break;
      case 'k':
         *ch = KEY_UP;
         break;
      case 'h':
         *ch = KEY_LEFT;
         break;
      case 'l':
         *ch = KEY_RIGHT;
         break;
   }
   return repeat;
}
