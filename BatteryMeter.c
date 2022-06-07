/*
htop - BatteryMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.

This meter written by Ian P. Hands (iphands@gmail.com, ihands@redhat.com).
*/

#include "BatteryMeter.h"

#include "Battery.h"
#include "ProcessList.h"
#include "CRT.h"
#include "StringUtils.h"
#include "Platform.h"

#include <string.h>
#include <stdlib.h>

/*{
#include "Meter.h"

typedef enum ACPresence_ {
   AC_ABSENT,
   AC_PRESENT,
   AC_ERROR
} ACPresence;
}*/

int BatteryMeter_attributes[] = {
   HTOP_BATTERY_COLOR
};

static void BatteryMeter_updateValues(Meter *this, char *buffer, int buffer_size) {
   double percent;
   ACPresence is_on_ac;
   Battery_getData(&percent, &is_on_ac);
   if (percent < 0) {
      this->values[0] = 0;
      xSnprintf(buffer, buffer_size, "n/a");
      return;
   }
   this->values[0] = percent;
   int len = snprintf(buffer, buffer_size, "%.1f%%", percent);
   assert(len > 0);
   assert(len < buffer_size - 21);
   switch(is_on_ac) {
      case AC_PRESENT:
         strcpy(buffer + len, this->mode == TEXT_METERMODE ? " (Running on A/C)" : "(A/C)");
         break;
      case AC_ABSENT:
         strcpy(buffer + len, this->mode == TEXT_METERMODE ? " (Running on battery)" : "(bat)");
         break;
   }
}

MeterClass BatteryMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = BatteryMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 1,
   .total = 100.0,
   .attributes = BatteryMeter_attributes,
   .name = "Battery",
   .uiName = "Battery",
   .caption = "Battery: "
};
