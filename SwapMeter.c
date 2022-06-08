/*
htop - SwapMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "SwapMeter.h"

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

int SwapMeter_attributes[] = {
   HTOP_SWAP_COLOR
};

static void SwapMeter_updateValues(Meter* this, char* buffer, int size) {
   int written;
   Platform_updateSwapValues(this);

   written = Meter_humanUnit(buffer, this->values[0], size);
   buffer += written;
   if ((size -= written) > 0) {
      *buffer++ = '/';
      size--;
      Meter_humanUnit(buffer, this->total, size);
   }
}

static void SwapMeter_display(Object* cast, RichString* out) {
   char buffer[50];
   Meter* this = (Meter*)cast;
   RichString_write(out, CRT_colors[HTOP_METER_TEXT_COLOR], ":");
   Meter_humanUnit(buffer, this->total, 50);
   RichString_append(out, CRT_colors[HTOP_METER_VALUE_COLOR], buffer);
   Meter_humanUnit(buffer, this->values[0], 50);
   RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], " used:");
   RichString_append(out, CRT_colors[HTOP_METER_VALUE_COLOR], buffer);
}

MeterClass SwapMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = SwapMeter_display,
   },
   .updateValues = SwapMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .maxItems = 1,
   .total = 100.0,
   .attributes = SwapMeter_attributes,
   .name = "Swap",
   .uiName = "Swap",
   .caption = "Swp"
};
