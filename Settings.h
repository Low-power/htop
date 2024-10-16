/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_Settings
#define HEADER_Settings
/*
htop - Settings.h
(C) 2004-2011 Hisham H. Muhammad
Copyright 2015-2023 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#include "Process.h"
#ifdef DISK_STATS
#include "Disk.h"
#endif
#include <stdbool.h>

#undef columns

typedef struct {
   int len;
   char** names;
   int* modes;
} MeterColumnSettings;

typedef struct Settings_ {
   char* filename;
   const char *terminal_type;

   MeterColumnSettings columns[2];

   unsigned int *fields;
#ifdef DISK_STATS
   unsigned int *disk_fields;
#endif
   int flags;
#ifdef DISK_STATS
   int disk_flags;
#endif
   int colorScheme;
   int delay;

   int cpuCount;
   int direction;
   ProcessField sortKey;
#ifdef DISK_STATS
   DiskField disk_sort_key;
#endif

   bool countCPUsFromZero;
   bool detailedCPUTime;
   bool treeView;
   bool showProgramPath;
   bool shadowOtherUsers;
   bool showThreadNames;
   bool hide_kernel_processes;
   bool hide_thread_processes;
#if defined __OpenBSD__ && defined PID_AND_MAIN_THREAD_ID_DIFFER
   bool hide_high_level_processes;
#endif
   bool highlightBaseName;
   bool highlightMegabytes;
   bool highlightThreads;
   bool highlight_kernel_processes;
   bool updateProcessNames;
   bool accountGuestInCPUMeter;
   bool headerMargin;
   bool explicit_delay;
   bool highlight_new_processes;
   bool tasks_meter_show_kernel_process_count;
   bool vi_mode;
   bool use_mouse;
   bool update_process_names_on_ctrl_l;
   int (*sort_strcmp)(const char *, const char *);

   char **unsupported_options;

   bool changed;

#ifdef DISK_STATS
   bool disk_mode;
#endif
} Settings;

#ifndef Settings_cpuId
#define Settings_cpuId(settings, cpu) ((settings)->countCPUsFromZero ? (cpu) : (cpu)+1)
#endif


#define DEFAULT_DELAY 15

void Settings_delete(Settings* this);

bool Settings_write(Settings* this);

// Make sure the last process field is HTOP_COMM_FIELD
Settings* Settings_new(int cpuCount, bool have_swap);

void Settings_invertSortOrder(Settings* this);

#endif
