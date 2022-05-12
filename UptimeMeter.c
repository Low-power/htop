/*
htop - UptimeMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#include "UptimeMeter.h"
#include "Platform.h"
#include "CRT.h"
#ifdef HAVE_UTMPX
#include <utmpx.h>
#endif
#if defined HAVE_UTMPX || defined HAVE_CLOCK_GETTIME
#include <time.h>
#endif

/*{
#include "Meter.h"
}*/

int UptimeMeter_attributes[] = {
   UPTIME
};

static int UptimeMeter_getUptimeFromUtmpx() {
#ifdef HAVE_UTMPX
	time_t boot_time = -1;
	time_t curr_time = time(NULL);   
	struct utmpx *utx;
	setutxent();
	while ((utx = getutxent())) {
		if(utx->ut_type == BOOT_TIME) {
			boot_time = utx->ut_tv.tv_sec;
			break;
		}
	}
	endutxent();
	return boot_time == -1 ? -1 : curr_time - boot_time;
#else
	return -1;
#endif
}

#ifdef HAVE_CLOCK_GETTIME
// CLOCK_UPTIME excludes suspend time, available since kFreeBSD 7.0
// CLOCK_BOOTTIME includes suspend time, available since Linux 2.6.39
// CLOCK_MONOTONIC_RAW excludes suspend time, available since Linux 2.6.28
#ifndef CLOCK_UPTIME
#ifdef CLOCK_BOOTTIME
#define CLOCK_UPTIME CLOCK_BOOTTIME
#elif defined CLOCK_MONOTONIC_RAW && defined __linux__
#define CLOCK_UPTIME CLOCK_MONOTONIC_RAW
#endif
#endif
#endif

static int UptimeMeter_getUptime() {
	int totalseconds = Platform_getUptime();
	if(totalseconds != -1) return totalseconds;
#if defined HAVE_CLOCK_GETTIME && defined CLOCK_UPTIME
	struct timespec t;
	if(clock_gettime(CLOCK_UPTIME, &t) == 0) return t.tv_sec;
#endif
	return UptimeMeter_getUptimeFromUtmpx();
}

static void UptimeMeter_updateValues(Meter* this, char* buffer, int len) {
   int totalseconds = UptimeMeter_getUptime();
   if (totalseconds == -1) {
      xSnprintf(buffer, len, "(unknown)");
      return;
   }
   int seconds = totalseconds % 60;
   int minutes = (totalseconds/60) % 60;
   int hours = (totalseconds/3600) % 24;
   int days = (totalseconds/86400);
   this->values[0] = days;
   if (days > this->total) {
      this->total = days;
   }
   char daysbuf[32];
   if (days > 100) {
      xSnprintf(daysbuf, sizeof(daysbuf), "%d days(!), ", days);
   } else if (days > 1) {
      xSnprintf(daysbuf, sizeof(daysbuf), "%d days, ", days);
   } else if (days == 1) {
      xSnprintf(daysbuf, sizeof(daysbuf), "1 day, ");
   } else {
      daysbuf[0] = '\0';
   }
   xSnprintf(buffer, len, "%s%02d:%02d:%02d", daysbuf, hours, minutes, seconds);
}

MeterClass UptimeMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = UptimeMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 1,
   .total = 100.0,
   .attributes = UptimeMeter_attributes,
   .name = "Uptime",
   .uiName = "Uptime",
   .caption = "Uptime: "
};
