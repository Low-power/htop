/*
htop - Header.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Header.h"
#include "CRT.h"
#include "StringUtils.h"
#include "Platform.h"
#include "ScreenManager.h"
#include "CategoriesPanel.h"
#include <assert.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

/*{
#include "config.h"
#include "Vector.h"
#include "Settings.h"
#include "ProcessList.h"
#ifdef DISK_STATS
#include "DiskList.h"
#endif
#include "Meter.h"

#undef columns

typedef struct Header_ {
   Vector** columns;
   Settings* settings;
   ProcessList *pl;
#ifdef DISK_STATS
   DiskList *disk_list;
#endif
   int nrColumns;
   int pad;
   int height;
} Header;

}*/

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef Header_forEachColumn
#define Header_forEachColumn(this_, i_) for (int (i_)=0; (i_) < (this_)->nrColumns; ++(i_))
#endif

Header* Header_new(ProcessList *pl, Settings* settings, int nrColumns) {
   Header* this = xCalloc(1, sizeof(Header));
   this->columns = xCalloc(nrColumns, sizeof(Vector*));
   this->settings = settings;
   this->pl = pl;
   this->nrColumns = nrColumns;
   Header_forEachColumn(this, i) {
      this->columns[i] = Vector_new(Class(Meter), true, DEFAULT_SIZE);
   }
   return this;
}

void Header_delete(Header* this) {
   Header_forEachColumn(this, i) {
      Vector_delete(this->columns[i]);
   }
   free(this->columns);
   free(this);
}

void Header_populateFromSettings(Header* this) {
   Header_forEachColumn(this, col) {
      MeterColumnSettings* colSettings = &this->settings->columns[col];
      for (int i = 0; i < colSettings->len; i++) {
         Header_addMeterByName(this, colSettings->names[i], col);
         if (colSettings->modes[i] != 0) {
            Header_setMode(this, i, colSettings->modes[i], col);
         }
      }
   }
   Header_calculateHeight(this);
}

void Header_writeBackToSettings(const Header* this) {
   Header_forEachColumn(this, col) {
      MeterColumnSettings* colSettings = &this->settings->columns[col];
      String_freeArray(colSettings->names);
      free(colSettings->modes);
      Vector* vec = this->columns[col];
      int len = Vector_size(vec);
      colSettings->names = xCalloc(len+1, sizeof(char*));
      colSettings->modes = xCalloc(len, sizeof(int));
      colSettings->len = len;
      for (int i = 0; i < len; i++) {
         Meter* meter = (Meter*) Vector_get(vec, i);
         char* name = xCalloc(64, sizeof(char));
         if (meter->param) {
            xSnprintf(name, 64, "%s(%d)", As_Meter(meter)->name, meter->param);
         } else {
            xSnprintf(name, 64, "%s", As_Meter(meter)->name);
         }
         colSettings->names[i] = name;
         colSettings->modes[i] = meter->mode;
      }
   }
}

MeterModeId Header_addMeterByName(Header* this, const char *name, int column) {
   Vector* meters = this->columns[column];
   int param;
   const char *paren = name;
   while(*paren && *paren != '(') paren++;
   if(!*paren || sscanf(paren + 1, "%10d)", &param) < 1) param = 0;
   MeterModeId mode = TEXT_METERMODE;
   for (MeterClass** type = Platform_meterTypes; *type; type++) {
      if (strncmp(name, (*type)->name, paren - name) == 0) {
         Meter* meter = Meter_new(this->pl, param, *type);
         Vector_add(meters, meter);
         mode = meter->mode;
         break;
      }
   }
   return mode;
}

void Header_setMode(Header* this, int i, MeterModeId mode, int column) {
   Vector* meters = this->columns[column];

   if (i >= Vector_size(meters))
      return;
   Meter* meter = (Meter*) Vector_get(meters, i);
   Meter_setMode(meter, mode);
}

Meter* Header_addMeterByClass(Header* this, MeterClass* type, int param, int column) {
   Vector* meters = this->columns[column];

   Meter* meter = Meter_new(this->pl, param, type);
   Vector_add(meters, meter);
   return meter;
}

int Header_size(Header* this, int column) {
   Vector* meters = this->columns[column];
   return Vector_size(meters);
}

MeterModeId Header_readMeterMode(Header* this, int i, int column) {
   Vector* meters = this->columns[column];

   Meter* meter = (Meter*) Vector_get(meters, i);
   return meter->mode;
}

void Header_reinit(const Header *this) {
   Header_forEachColumn(this, col) {
      for (int i = 0; i < Vector_size(this->columns[col]); i++) {
         Meter* meter = (Meter*) Vector_get(this->columns[col], i);
         if (Meter_initFn(meter)) Meter_init(meter);
      }
   }
}

void Header_draw(const Header* this) {
   int height = this->height;
   int pad = this->pad;
   attrset(CRT_colors[HTOP_DEFAULT_COLOR]);
   for (int y = 0; y < height; y++) {
      mvhline(y, 0, ' ', COLS);
   }
   int width = (COLS + 1) / this->nrColumns - (pad * this->nrColumns - 1) - 1;
   int x = pad;
   Header_forEachColumn(this, col) {
      Vector* meters = this->columns[col];
      for (int y = (pad / 2), i = 0; i < Vector_size(meters); i++) {
         Meter* meter = (Meter*) Vector_get(meters, i);
         meter->draw(meter, x, y, width);
         y += meter->h;
      }
      x += width + pad;
   }
}

int Header_calculateHeight(Header* this) {
   int pad = this->settings->headerMargin ? 2 : 0;
   int maxHeight = pad;

   Header_forEachColumn(this, col) {
      Vector* meters = this->columns[col];
      int height = pad;
      for (int i = 0; i < Vector_size(meters); i++) {
         Meter* meter = (Meter*) Vector_get(meters, i);
         height += meter->h;
      }
      maxHeight = MAX(maxHeight, height);
   }
   this->height = maxHeight;
   this->pad = pad;
   return maxHeight;
}

void Header_runSetup(Header *this, Settings *settings, const ProcessList *pl) {
   ScreenManager *setup_scr = ScreenManager_new(0, this->height, 0, -1, HORIZONTAL, this, settings, true);
   CategoriesPanel *categories_panel = CategoriesPanel_new(setup_scr, settings, this, pl);
   ScreenManager_add(setup_scr, (Panel *)categories_panel, 16);
   CategoriesPanel_makeMetersPage(categories_panel);
   Panel *focused_panel;
   int ch;
   ScreenManager_run(setup_scr, &focused_panel, &ch);
   ScreenManager_delete(setup_scr);
   if (settings->changed) {
      CRT_setMouse(settings->use_mouse);
      CRT_setExplicitDelay(settings->explicit_delay);
      if(!settings->explicit_delay) CRT_enableDelay();
      Header_writeBackToSettings(this);
   }
}
