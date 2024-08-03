/*
htop - HostnameMeter.c
(C) 2004-2011 Hisham H. Muhammad
Copyright 2015-2024 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "HostnameMeter.h"

#include "CRT.h"
#include "XAlloc.h"
#include <unistd.h>
#include <errno.h>

/*{
#include "Meter.h"
}*/

int HostnameMeter_attributes[] = {
   HTOP_HOSTNAME_COLOR
};

static void HostnameMeter_updateValues(Meter* this, char* buffer, int size) {
   (void) this;
   if(gethostname(buffer, size - 1) < 0) {
      if(errno == ENAMETOOLONG) buffer[size - 1] = 0;
      else xSnprintf(buffer, size, "(Error %d)", errno);
   }
}

MeterClass HostnameMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = HostnameMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 0,
   .total = 100.0,
   .attributes = HostnameMeter_attributes,
   .name = "Hostname",
   .uiName = "Hostname",
   .caption = "Hostname",
};
