/*
htop - bsd/Platform.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <time.h>

int Platform_getUptime() {
   struct timeval bootTime, currTime;
   int mib[2] = { CTL_KERN, KERN_BOOTTIME };
   size_t size = sizeof(bootTime);
   if(sysctl(mib, 2, &bootTime, &size, NULL, 0) < 0) return -1;
   if(gettimeofday(&currTime, NULL) < 0) return -1;
   return (int) difftime(currTime.tv_sec, bootTime.tv_sec);
}
