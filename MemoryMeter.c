/*
htop - MemoryMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "MemoryMeter.h"

#include "CRT.h"
#include "Platform.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/param.h>
#include <assert.h>

/*{
#include "Meter.h"
}*/

static const int MemoryMeter_attributes[] = {
   HTOP_MEMORY_USED_COLOR, HTOP_MEMORY_BUFFERS_COLOR, HTOP_MEMORY_CACHE_COLOR, HTOP_MEMORY_ZFS_ARC_COLOR
};

static void MemoryMeter_updateValues(Meter* this, char* buffer, int size) {
   int written;
   Platform_updateMemoryValues(this);
   this->values[3] = this->pl->zfs_arc_size;

   written = Meter_humanUnit(buffer, this->values[0], size);
   buffer += written;
   if ((size -= written) > 0) {
      *buffer++ = '/';
      size--;
      Meter_humanUnit(buffer, this->total, size);
   }
}

static void MemoryMeter_display(Object* cast, RichString* out) {
   char buffer[50];
   Meter* this = (Meter*)cast;
   RichString_write(out, CRT_colors[HTOP_METER_TEXT_COLOR], ": ");
   Meter_humanUnit(buffer, this->total, 50);
   RichString_append(out, CRT_colors[HTOP_METER_VALUE_COLOR], buffer);
   Meter_humanUnit(buffer, this->values[0], 50);
   RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], " used");
   if(this->mode == TEXT_METERMODE) RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], "=");
   RichString_append(out, CRT_colors[HTOP_MEMORY_USED_COLOR], buffer);
   Meter_humanUnit(buffer, this->values[1], 50);
   RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], " buffers");
   if(this->mode == TEXT_METERMODE) RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], "=");
   RichString_append(out, CRT_colors[HTOP_MEMORY_BUFFERS_TEXT_COLOR], buffer);
   Meter_humanUnit(buffer, this->values[2], 50);
   RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], " cache");
   if(this->mode == TEXT_METERMODE) RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], "=");
   RichString_append(out, CRT_colors[HTOP_MEMORY_CACHE_COLOR], buffer);
   if(this->values[3] > 0) {
      Meter_humanUnit(buffer, this->values[3], 50);
      RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], " zfs-arc");
      if(this->mode == TEXT_METERMODE) RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], "=");
      RichString_append(out, CRT_colors[HTOP_MEMORY_ZFS_ARC_COLOR], buffer);
   }
}

MeterClass MemoryMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = MemoryMeter_display,
   },
   .updateValues = MemoryMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .maxItems = 4,
   .total = 100.0,
   .attributes = MemoryMeter_attributes,
   .name = "Memory",
   .uiName = "Memory",
   .caption = "Mem"
};
