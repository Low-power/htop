/*
htop - linux/Battery.c
(C) 2004-2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.

Linux battery readings written by Ian P. Hands (iphands@gmail.com, ihands@redhat.com).
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include "BatteryMeter.h"
#include "StringUtils.h"
#include "IOUtils.h"

#define SYS_POWERSUPPLY_DIR "/sys/class/power_supply"

// ----------------------------------------
// READ FROM /proc
// ----------------------------------------

// This implementation reading from from /proc/acpi is really inefficient,
// but I think this is on the way out so I did not rewrite it.
// The /sys implementation below does things the right way.

static long int parseBatInfo(const char *fileName, int lineNum, int wordNum) {
   const char batteryPath[] = PROCDIR "/acpi/battery/";
   DIR* batteryDir = opendir(batteryPath);
   if (!batteryDir) return -1;

   #define MAX_BATTERIES 64
   char* batteries[MAX_BATTERIES];
   unsigned int nBatteries = 0;
   memset(batteries, 0, MAX_BATTERIES * sizeof(char*));

   while (nBatteries < MAX_BATTERIES) {
      struct dirent* dirEntry = readdir(batteryDir);
      if (!dirEntry)
         break;
      char* entryName = dirEntry->d_name;
      if (strncmp(entryName, "BAT", 3))
         continue;
      batteries[nBatteries] = xStrdup(entryName);
      nBatteries++;
   }
   closedir(batteryDir);

   long int total = -1;
   for (unsigned int i = 0; i < nBatteries; i++) {
      char infoPath[30];
      xSnprintf(infoPath, sizeof infoPath, "%s%s/%s", batteryPath, batteries[i], fileName);

      FILE* file = fopen(infoPath, "r");
      if (!file) {
         break;
      }

      char* line = NULL;
      for (int i = 0; i < lineNum; i++) {
         free(line);
         line = String_readLine(file);
         if (!line) break;
      }

      fclose(file);

      if (!line) break;

      char *foundNumStr = String_getToken(line, wordNum);
      const unsigned long int foundNum = atoi(foundNumStr);
      free(foundNumStr);
      free(line);
      if(total < 0) total = foundNum;
      else total += foundNum;
   }

   for (unsigned int i = 0; i < nBatteries; i++) {
      free(batteries[i]);
   }

   return total;
}

static ACPresence procAcpiCheck() {
   ACPresence isOn = AC_ERROR;
   const char *power_supplyPath = PROCDIR "/acpi/ac_adapter";
   DIR *dir = opendir(power_supplyPath);
   if (!dir) {
      return AC_ERROR;
   }

   for (;;) {
      struct dirent* dirEntry = readdir((DIR *) dir);
      if (!dirEntry)
         break;

      char* entryName = (char *) dirEntry->d_name;

      if (entryName[0] != 'A')
         continue;

      char statePath[50];
      xSnprintf((char *) statePath, sizeof statePath, "%s/%s/state", power_supplyPath, entryName);
      FILE* file = fopen(statePath, "r");
      if (!file) {
         isOn = AC_ERROR;
         continue;
      }
      char* line = String_readLine(file);
      fclose(file);
      if (!line) continue;

      char *isOnline = String_getToken(line, 2);
      free(line);
      isOn = strcmp(isOnline, "on-line") == 0 ? AC_PRESENT : AC_ABSENT;
      free(isOnline);
      if (isOn == AC_PRESENT) {
         break;
      }
   }

   if (dir)
      closedir(dir);
   return isOn;
}

static double Battery_getProcBatData() {
   long int totalFull = parseBatInfo("info", 3, 4);
   if (totalFull < 0) return -1;

   long int totalRemain = parseBatInfo("state", 5, 3);
   if (totalRemain < 0) return -1;

   return totalRemain * 100.0 / (double)totalFull;
}

static void Battery_getProcData(double* level, ACPresence* isOnAC) {
   *level = Battery_getProcBatData();
   *isOnAC = procAcpiCheck();
}

// ----------------------------------------
// READ FROM /sys
// ----------------------------------------

static void Battery_getSysData(double* level, ACPresence* isOnAC) {
   *level = -1;
   *isOnAC = AC_ERROR;

   DIR *dir = opendir(SYS_POWERSUPPLY_DIR);
   if (!dir)
      return;

   unsigned long int totalFull = 0;
   unsigned long int totalRemain = 0;
   unsigned int total_capacity = 0;
   unsigned int battery_count = 0;

   for (;;) {
      struct dirent* dirEntry = readdir(dir);
      if (!dirEntry) break;
      const char *entryName = dirEntry->d_name;
      if(*entryName == '.') continue;
      const char filePath[50];
      xSnprintf((char *)filePath, sizeof filePath, SYS_POWERSUPPLY_DIR "/%s/type", entryName);
      FILE *f = fopen(filePath, "r");
      if(!f) continue;
      char *line = String_readLine(f);
      fclose(f);
      if(!line) continue;
      if(strcmp(line, "Battery") == 0) {
         free(line);
         xSnprintf((char *) filePath, sizeof filePath, SYS_POWERSUPPLY_DIR "/%s/uevent", entryName);
         int fd = open(filePath, O_RDONLY);
         if (fd == -1) continue;
         char buffer[1024];
         ssize_t buflen = xread(fd, buffer, 1023);
         close(fd);
         if (buflen < 1) continue;
         buffer[buflen] = '\0';
         char *buf = buffer;
         line = NULL;
         bool full = false;
         bool now = false;
         while ((line = strsep(&buf, "\n")) != NULL) {
   #define match(str,prefix) \
           (String_startsWith(str,prefix) ? (str) + strlen(prefix) : NULL)
            const char* ps = match(line, "POWER_SUPPLY_");
            if (!ps) {
               continue;
            }
            const char* energy = match(ps, "ENERGY_");
            if (!energy) {
               energy = match(ps, "CHARGE_");
            }
            if (!energy) {
               const char *capacity = match(ps, "CAPACITY=");
               if(capacity) total_capacity += atoi(capacity);
               continue;
            }
            const char* value = (!full) ? match(energy, "FULL=") : NULL;
            if (value) {
               totalFull += atoi(value);
               full = true;
               if (now) break;
               continue;
            }
            value = (!now) ? match(energy, "NOW=") : NULL;
            if (value) {
               totalRemain += atoi(value);
               now = true;
               if (full) break;
               continue;
            }
         }
         line = NULL;
         battery_count++;
   #undef match
      } else if((strcmp(line, "Mains") == 0 || strcmp(line, "USB") == 0) && *isOnAC != AC_PRESENT) {
         free(line);
         line = NULL;
         xSnprintf((char *) filePath, sizeof filePath, SYS_POWERSUPPLY_DIR "/%s/online", entryName);
         int fd = open(filePath, O_RDONLY);
         if (fd == -1) continue;
         char b = 0;
         while(read(fd, &b, 1) < 0 && errno == EINTR);
         close(fd);
         switch(b) {
            case '0': *isOnAC = AC_ABSENT; break;
            case '1': *isOnAC = AC_PRESENT; break;
         }
      }
      free(line);
   }
   closedir(dir);
   if(totalFull > 0) {
      *level = ((double) totalRemain * 100) / (double) totalFull;
   } else if(battery_count > 0) {
      *level = (double)total_capacity / (double)battery_count;
   } else {
      *level = -1;
   }
}

static enum { BAT_PROC, BAT_SYS, BAT_ERR } Battery_method = BAT_PROC;

static time_t Battery_cacheTime = 0;
static double Battery_cacheLevel = -1;
static ACPresence Battery_cacheIsOnAC = 0;

void Battery_getData(double* level, ACPresence* isOnAC) {
   time_t now = time(NULL);
   // update battery reading is slow. Update it each 10 seconds only.
   if (now < Battery_cacheTime + 10) {
      *level = Battery_cacheLevel;
      *isOnAC = Battery_cacheIsOnAC;
      return;
   }

   if (Battery_method == BAT_PROC) {
      Battery_getProcData(level, isOnAC);
      if (*level < 0) {
         Battery_method = BAT_SYS;
      }
   }
   if (Battery_method == BAT_SYS) {
      Battery_getSysData(level, isOnAC);
      if (*level < 0) {
         Battery_method = BAT_ERR;
      }
   }
   if (Battery_method == BAT_ERR) {
      *level = -1;
      *isOnAC = AC_ERROR;
   }
   if (*level > 100.0) {
      *level = 100.0;
   }
   Battery_cacheLevel = *level;
   Battery_cacheIsOnAC = *isOnAC;
   Battery_cacheTime = now;
}
