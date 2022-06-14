/*
htop - ColumnsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#include "ColumnsPanel.h"
#include "Platform.h"

#include "StringUtils.h"
#include "ListItem.h"
#include "CRT.h"

#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

/*{
#include "Panel.h"
#include "Settings.h"

typedef struct ColumnsPanel_ {
   Panel super;

   Settings* settings;
   bool moving;
} ColumnsPanel;

}*/

static const char* const ColumnsFunctions[] = {"      ", "      ", "      ", "      ", "      ", "      ", "MoveUp", "MoveDn", "Remove", "Done  ", NULL};

static void ColumnsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   ColumnsPanel* this = (ColumnsPanel*) object;
   Panel_done(super);
   free(this);
}

static HandlerResult ColumnsPanel_eventHandler(Panel* super, int ch, int repeat) {
   ColumnsPanel *this = (ColumnsPanel *)super;
   HandlerResult result = IGNORED;
   int size = Panel_size(super);
#ifdef DISK_STATS
#define DISK_MODE (this->settings->disk_mode)
#else
#define DISK_MODE false
#endif
   switch(ch) {
         int selected;

      case 0x0a:
      case 0x0d:
      case KEY_ENTER:
      case KEY_MOUSE:
      case KEY_RECLICK:
         if (Panel_getSelectedIndex(super) < size - (DISK_MODE ? 0 : 1)) {
            this->moving = !(this->moving);
            Panel_setSelectionColor(super, CRT_colors[this->moving ? HTOP_PANEL_SELECTION_FOLLOW_COLOR : HTOP_PANEL_SELECTION_FOCUS_COLOR]);
            ((ListItem*)Panel_getSelected(super))->moving = this->moving;
            result = HANDLED;
         }
         break;
      case KEY_UP:
      case KEY_PPAGE:	// XXX
         if (!this->moving) {
            break;
         }
         /* else fallthrough */
      case KEY_F(7):
      case '[':
      case '-':
         while(repeat-- > 0 && Panel_getSelectedIndex(super) < size - (DISK_MODE ? 0 : 1)) {
            Panel_moveSelectedUp(super);
         }
         result = HANDLED;
         break;
      case KEY_DOWN:
      case KEY_NPAGE:	// XXX
         if (!this->moving) {
            break;
         }
         /* else fallthrough */
      case KEY_F(8):
      case ']':
      case '+':
         while(repeat-- > 0 && Panel_getSelectedIndex(super) < size - (DISK_MODE ? 1 : 2)) {
            Panel_moveSelectedDown(super);
         }
         result = HANDLED;
         break;
      case KEY_F(9):
      case KEY_DC:
#if 0
         while(repeat-- > 0 && (selected = Panel_getSelectedIndex(super)) < size - (DISK_MODE ? 0 : 1)) {
            Panel_remove(super, selected);
            size--;
         }
#else
         // Don't repeat delete operation
         selected = Panel_getSelectedIndex(super);
         if(selected < size - (DISK_MODE ? 0 : 1)) {
            Panel_remove(super, selected);
         }
#endif
         result = HANDLED;
         break;
      default:
         if (ch < 255 && ch > 0 && isalpha(ch)) result = Panel_selectByTyping(super, ch);
         if (result == BREAK_LOOP) result = IGNORED;
         break;
   }
#undef DISK_MODE
   if (result == HANDLED) ColumnsPanel_update(super);
   return result;
}

PanelClass ColumnsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = ColumnsPanel_delete
   },
   .eventHandler = ColumnsPanel_eventHandler
};

ColumnsPanel* ColumnsPanel_new(Settings* settings) {
   ColumnsPanel* this = AllocThis(ColumnsPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(ColumnsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, fuBar);

   this->settings = settings;
   this->moving = false;
   Panel_setHeader(super, "Active Columns");
#ifdef DISK_STATS
   const unsigned int *field = settings->disk_mode ? settings->disk_fields : settings->fields;
   const FieldData *field_data = settings->disk_mode ? Disk_fields : Process_fields;
#else
   const unsigned int *field = settings->fields;
   const FieldData *field_data = Process_fields;
#endif
   while(*field) {
      if (field_data[*field].name) {
         Panel_add(super,
            (Object *)ListItem_new(field_data[*field].name, HTOP_DEFAULT_COLOR, *field, settings));
      }
      field++;
   }
   return this;
}

int ColumnsPanel_fieldNameToIndex(const FieldData *field_data, unsigned int nfields, const char* name) {
   for (unsigned int j = 1; j <= nfields; j++) {
      if (String_eq(name, field_data[j].name)) {
         return j;
      }
   }
   return -1;
}

void ColumnsPanel_update(Panel* super) {
   ColumnsPanel* this = (ColumnsPanel*) super;
   int size = Panel_size(super);
   unsigned int **fields;
   int *flags;
#ifdef DISK_STATS
   if(this->settings->disk_mode) {
      fields = &this->settings->disk_fields;
      flags = &this->settings->disk_flags;
   } else
#endif
   {
      fields = &this->settings->fields;
      flags = &this->settings->flags;
   }
   free(*fields);
   *fields = xMalloc((size + 1) * sizeof(unsigned int));
   *flags = 0;
   for (int i = 0; i < size; i++) {
      int key = ((ListItem*) Panel_get(super, i))->key;
      (*fields)[i] = key;
      *flags |= Process_fields[key].flags;
   }
   (*fields)[size] = 0;
   this->settings->changed = true;
}
