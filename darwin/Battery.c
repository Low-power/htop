/*
htop - darwin/Battery.c
(C) 2015 David C. Hunt
(C) 2015 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "BatteryMeter.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFString.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>

void Battery_getData(double* level, ACPresence* isOnAC) {
   *level = -1;
   *isOnAC = AC_ERROR;

   CFTypeRef power_sources_info = IOPSCopyPowerSourcesInfo();
   if(!power_sources_info) return;

   CFArrayRef list = IOPSCopyPowerSourcesList(power_sources_info);
   if(!list) {
      CFRelease(power_sources_info);
      return;
   }

   double total_capacity = 0;
   double total_max_capacity = 0;

   /* Iterate all batteries */
   int len = CFArrayGetCount(list);
   for(int i = 0; i < len; i++) {
      CFDictionaryRef power_source =
         IOPSGetPowerSourceDescription(power_sources_info, CFArrayGetValueAtIndex(list, i));
      if(!power_source) continue;
      CFStringRef type = CFDictionaryGetValue(power_source, CFSTR(kIOPSTransportTypeKey));
      if(!type) continue;
      if(CFStringCompare(type, CFSTR(kIOPSInternalType), 0) != kCFCompareEqualTo) continue;
      if(*isOnAC != AC_PRESENT) {
         CFStringRef power_state = CFDictionaryGetValue(power_source, CFSTR(kIOPSPowerSourceStateKey));
         if(power_state) {
            *isOnAC =
               CFStringCompare(power_state, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo ?
                  AC_PRESENT : AC_ABSENT;
         }
      }
      double capacity, max_capacity;
      CFNumberRef cfn = CFDictionaryGetValue(power_source, CFSTR(kIOPSCurrentCapacityKey));
      if(!cfn) continue;
      CFNumberGetValue(cfn, kCFNumberDoubleType, &capacity);
      cfn = CFDictionaryGetValue(power_source, CFSTR(kIOPSMaxCapacityKey));
      if(!cfn) continue;
      CFNumberGetValue(cfn, kCFNumberDoubleType, &max_capacity);
      total_capacity += capacity;
      total_max_capacity += max_capacity;
   }

   CFRelease(list);
   CFRelease(power_sources_info);

   if(total_max_capacity > 0) *level = total_capacity / total_max_capacity * 100;
}
