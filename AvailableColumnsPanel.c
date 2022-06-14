/*
htop - AvailableColumnsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "Panel.h"
#include "FieldData.h"

typedef struct AvailableColumnsPanel_ {
   Panel super;
   Panel* columns;
   const FieldData *field_data;
   unsigned int nfields;
} AvailableColumnsPanel;
}*/

#include "config.h"
#include "AvailableColumnsPanel.h"
#include "Platform.h"
#include "Header.h"
#include "ColumnsPanel.h"
#include "CRT.h"
#ifdef DISK_STATS
#include "Disk.h"
#endif
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

static const char* const AvailableColumnsFunctions[] = {"      ", "      ", "      ", "      ", "Add   ", "      ", "      ", "      ", "      ", "Done  ", NULL};

static void AvailableColumnsPanel_delete(Object* object) {
   Panel* super = (Panel*) object;
   AvailableColumnsPanel* this = (AvailableColumnsPanel*) object;
   Panel_done(super);
   free(this);
}

static HandlerResult AvailableColumnsPanel_eventHandler(Panel* super, int ch, int repeat) {
   AvailableColumnsPanel* this = (AvailableColumnsPanel*) super;
   int key = ((ListItem*) Panel_getSelected(super))->key;
   HandlerResult result = IGNORED;

   switch(ch) {
         int at;
      case 13:
      case KEY_ENTER:
      case KEY_F(5):
         at = Panel_getSelectedIndex(this->columns);
         Panel_insert(this->columns, at,
            (Object *)ListItem_new(this->field_data[key].name, HTOP_DEFAULT_COLOR, key, NULL));
         Panel_setSelected(this->columns, at+1);
         ColumnsPanel_update(this->columns);
         result = HANDLED;
         break;
      default:
         if (ch < 255 && ch > 0 && isalpha(ch)) result = Panel_selectByTyping(super, ch);
         break;
   }
   return result;
}

PanelClass AvailableColumnsPanel_class = {
   .super = {
      .extends = Class(Panel),
      .delete = AvailableColumnsPanel_delete
   },
   .eventHandler = AvailableColumnsPanel_eventHandler
};

AvailableColumnsPanel* AvailableColumnsPanel_new(Panel* columns, bool disk_mode) {
   AvailableColumnsPanel* this = AllocThis(AvailableColumnsPanel);
   Panel* super = (Panel*) this;
   FunctionBar* fuBar = FunctionBar_new(AvailableColumnsFunctions, NULL, NULL);
   Panel_init(super, 1, 1, 1, 1, Class(ListItem), true, fuBar);
   Panel_setHeader(super, "Available Columns");
#ifdef DISK_STATS
   this->field_data = disk_mode ? Disk_fields : Process_fields;
   this->nfields = disk_mode ? Disk_field_count : Platform_numberOfFields;
#else
   assert(!disk_mode);
   this->field_data = Process_fields;
   this->nfields = Platform_numberOfFields;
#endif
   for (unsigned int i = 1; i < this->nfields; i++) {
      if(!disk_mode && i == HTOP_COMM_FIELD) continue;
      const FieldData *field = this->field_data + i;
      if (field->description) {
         char description[256];
         xSnprintf(description, sizeof(description), "%s - %s", field->name, field->description);
         Panel_add(super, (Object *)ListItem_new(description, HTOP_DEFAULT_COLOR, i, NULL));
      }
   }
   this->columns = columns;
   return this;
}
