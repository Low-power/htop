/*
htop - darwin/Battery.c
(C) 2015 David C. Hunt
(C) 2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "BatteryMeter.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFString.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>

void Battery_getData(double* level, ACPresence* isOnAC) {
   CFTypeRef power_sources = IOPSCopyPowerSourcesInfo();

   *level = -1;
   *isOnAC = AC_ERROR;

   if(NULL == power_sources) {
      return;
   }

   if(power_sources != NULL) {
      CFArrayRef list = IOPSCopyPowerSourcesList(power_sources);
      CFDictionaryRef battery = NULL;
      int len;

      if(NULL == list) {
         CFRelease(power_sources);

         return;
      }

      len = CFArrayGetCount(list);

      /* Get the battery */
      for(int i = 0; i < len && battery == NULL; ++i) {
         CFDictionaryRef candidate = IOPSGetPowerSourceDescription(power_sources,
                                     CFArrayGetValueAtIndex(list, i)); /* GET rule */
         if(candidate) {
            CFStringRef type = (CFStringRef)CFDictionaryGetValue(candidate,
               CFSTR(kIOPSTransportTypeKey)); /* GET rule */
            if(type && CFStringCompare(type, CFSTR(kIOPSInternalType), 0) == kCFCompareEqualTo) {
               CFRetain(candidate);
               battery = candidate;
               break;
            }
         }
      }

      if(battery) {
         /* Determine the AC state */
         CFStringRef power_state = CFDictionaryGetValue(battery, CFSTR(kIOPSPowerSourceStateKey));
         if(power_state) {
            *isOnAC = CFStringCompare(power_state, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo ?
               AC_PRESENT : AC_ABSENT;

            /* Get the percentage remaining */
            double current;
            double max;
            CFNumberGetValue(CFDictionaryGetValue(battery, CFSTR(kIOPSCurrentCapacityKey)),
                 kCFNumberDoubleType, &current);
            CFNumberGetValue(CFDictionaryGetValue(battery, CFSTR(kIOPSMaxCapacityKey)),
                 kCFNumberDoubleType, &max);
            *level = (current * 100) / max;
         }
         CFRelease(battery);
      }

      CFRelease(list);
      CFRelease(power_sources);
   }
}
