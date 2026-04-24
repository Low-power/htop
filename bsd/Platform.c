/*
htop - bsd/Platform.c
(C) 2014 Hisham H. Muhammad
Copyright 2022-2026 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include <sys/param.h>
#if defined __FreeBSD__ && !defined __FreeBSD_kernel__
#define __FreeBSD_kernel__
#endif
#include <sys/sysctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#if defined __FreeBSD_kernel__ || defined __DragonFly__
#include <vm/vm_param.h>
#endif
#include <assert.h>

int Platform_getUptime() {
   struct timeval bootTime, currTime;
   int mib[2] = { CTL_KERN, KERN_BOOTTIME };
   size_t size = sizeof(bootTime);
   if(sysctl(mib, 2, &bootTime, &size, NULL, 0) < 0) return -1;
   if(gettimeofday(&currTime, NULL) < 0) return -1;
   return (int) difftime(currTime.tv_sec, bootTime.tv_sec);
}

void Platform_getLoadAverage(double *values) {
   int mib[2] = { CTL_VM, VM_LOADAVG };
   struct loadavg loadavg;
   size_t size = sizeof loadavg;
   assert(sizeof loadavg.ldavg / sizeof *loadavg.ldavg >= 3);
   if(sysctl(mib, 2, &loadavg, &size, NULL, 0) < 0) {
      values[0] = 0;
      values[1] = 0;
      values[2] = 0;
   } else {
      values[0] = (double)loadavg.ldavg[0] / loadavg.fscale;
      values[1] = (double)loadavg.ldavg[1] / loadavg.fscale;
      values[2] = (double)loadavg.ldavg[2] / loadavg.fscale;
   }
}
