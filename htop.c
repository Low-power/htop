/*
htop - htop.c
(C) 2004-2011 Hisham H. Muhammad
Copyright 2015-2023 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"

#if defined DISK_STATS && !defined HAVE_GETOPT_LONG
#error "Disk statistics mode requires getopt_long(3)"
#endif

#include "FunctionBar.h"
#include "Hashtable.h"
#include "ColumnsPanel.h"
#include "CRT.h"
#include "MainPanel.h"
#include "ProcessList.h"
#include "ScreenManager.h"
#include "Settings.h"
#include "UsersTable.h"
#include "Platform.h"
#ifdef DISK_STATS
#include "DiskPanel.h"
#include "DiskList.h"
#include "Disk.h"
#endif
#if defined HAVE_GETOPT_H && defined HAVE_GETOPT_LONG
#include <getopt.h>
#endif
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void printVersionFlag() {
   fputs("htop " VERSION "\n" COPYRIGHT "\n"
         "Released under the GNU GPL any version.\n",
         stdout);
}

static void print_usage(FILE *f, const char *name) {
   fprintf(f,
         "Usage: %s [OPTION]...\n\n"
         "Options:\n"
#ifdef HAVE_GETOPT_LONG
         "   -C, --no-color              Use a monochrome color scheme\n"
         "   -d, --delay=DELAY           Set the delay between updates, in tenths of\n"
         "                               seconds\n"
         "   -h, --help                  Print this help screen\n"
         "   -s, --sort-key=COLUMN       Sort by COLUMN (try --sort-key=help for a list)\n"
         "   -t, --tree                  Show the tree view by default\n"
         "   -u, --user=USERNAME         Show only processes of a given user\n"
         "   -p, --pid=PID[,PID,PID...]  Show only the given PIDs\n"
#ifdef DISK_STATS
         "       --disk                  Show disk instead of process statistics\n"
#endif
         "       --explicit-delay        Explicitly delay between updates\n"
         "   -v, --version               Print version info\n"
         "\nArguments to long options are required for short options too.\n\n"
#else
         "   -C                   Use a monochrome color scheme\n"
         "   -d DELAY             Set the delay between updates, in tenths of seconds\n"
         "   -h                   Print this help screen\n"
         "   -s COLUMN            Sort by COLUMN (try --sort-key=help for a list)\n"
         "   -t                   Show the tree view by default\n"
         "   -u USERNAME          Show only processes of a given user\n"
         "   -p PID[,PID,PID...]  Show only the given PIDs\n"
         "   -v                   Print version info\n"
         "\n"
#endif
         "Press F1 inside htop for online help.\n"
         "See 'man htop' for more information.\n",
         name);
}

// ----------------------------------------

typedef struct CommandLineSettings_ {
   Hashtable* pidWhiteList;
   uid_t userId;
   int sortKey;
   int delay;
   bool useColors;
   bool treeView;
   bool explicit_delay;
#ifdef DISK_STATS
   bool disk;
#endif
} CommandLineSettings;

static CommandLineSettings parseArguments(int argc, char** argv) {
   const char *sort_key = NULL;
   CommandLineSettings flags = {
      .pidWhiteList = NULL,
      .userId = -1, // -1 is guaranteed to be an invalid uid_t (see setreuid(2))
      .sortKey = 0,
      .delay = -1,
      .useColors = true,
      .treeView = false,
      .explicit_delay = false,
   };

#define HTOP_LONG_OPTION_EXPLICIT_DELAY (1 << 8)
#define HTOP_LONG_OPTION_DISK (2 << 8)
#ifdef HAVE_GETOPT_LONG
   static struct option long_opts[] = {
      { "help",           no_argument,       NULL, 'h' },
      { "version",        no_argument,       NULL, 'v' },
      { "delay",          required_argument, NULL, 'd' },
      { "sort-key",       required_argument, NULL, 's' },
      { "user",           required_argument, NULL, 'u' },
      { "no-color",       no_argument,       NULL, 'C' },
      { "no-colour",      no_argument,       NULL, 'C' },
      { "tree",           no_argument,       NULL, 't' },
      { "pid",            required_argument, NULL, 'p' },
      { "explicit-delay", no_argument,       NULL, HTOP_LONG_OPTION_EXPLICIT_DELAY },
#ifdef DISK_STATS
      { "disk",           no_argument,       NULL, HTOP_LONG_OPTION_DISK },
#endif
      { NULL, 0, NULL, 0 }
   };
#endif
   /* Parse options */
   while(true) {
#ifdef HAVE_GETOPT_LONG
      int opt = getopt_long(argc, argv, "hvCs:td:u:p:", long_opts, NULL);
#else
      int opt = getopt(argc, argv, "hvCs:td:u:p:");
#endif
      if (opt == -1) break;
      switch (opt) {
            char *p;
         case HTOP_LONG_OPTION_EXPLICIT_DELAY:
            flags.explicit_delay = true;
            break;
#ifdef DISK_STATS
         case HTOP_LONG_OPTION_DISK:
            flags.disk = true;
            break;
#endif
         case 'h':
            print_usage(stdout, argv[0]);
            exit(0);
         case 'v':
            printVersionFlag();
            exit(0);
         case 's':
            sort_key = optarg;
            break;
         case 'd':
            if (sscanf(optarg, "%16d", &(flags.delay)) == 1) {
               if (flags.delay < 1) flags.delay = 1;
               if (flags.delay > 100) flags.delay = 100;
            } else {
               fprintf(stderr, "Error: invalid delay value \"%s\".\n", optarg);
               exit(-1);
            }
            break;
         case 'u':
            if (!Action_setUserOnly(optarg, &(flags.userId))) {
               fprintf(stderr, "Error: invalid user \"%s\".\n", optarg);
               exit(-1);
            }
            break;
         case 'C':
            flags.useColors = false;
            break;
         case 't':
            flags.treeView = true;
            break;
         case 'p':
            if(!flags.pidWhiteList) {
               flags.pidWhiteList = Hashtable_new(8, false);
            }
            p = optarg;
            do {
               long int pid = strtol(p, &p, 0);
               if(*p && *p != ',') {
                  fprintf(stderr, "Error: invalid PID list \"%s\".\n", optarg);
                  exit(-1);
               }
               if(pid < 0) {
                  fprintf(stderr, "Error: invalid PID %ld.\n", pid);
                  exit(-1);
               }
               Hashtable_put(flags.pidWhiteList, pid, (void *) 1);
            } while(*p++);
            break;
         default:
#ifdef __ANDROID__
            fputc('\n', stderr);
#endif
            exit(-1);
      }
   }
   if(sort_key) {
#ifdef DISK_STATS
      const FieldData *field_data = flags.disk ? Disk_fields : Process_fields;
      unsigned int nfields = flags.disk ? Disk_field_count : Platform_numberOfFields;
#else
      const FieldData *field_data = Process_fields;
      unsigned int nfields = Platform_numberOfFields;
#endif
      if (strcmp(sort_key, "help") == 0) {
         for (unsigned int j = 1; j < nfields; j++) {
            const char* name = field_data[j].name;
            if (name) puts(name);
         }
         exit(0);
      }
      flags.sortKey = ColumnsPanel_fieldNameToIndex(field_data, nfields, sort_key);
      if (flags.sortKey == -1) {
         fprintf(stderr, "Error: invalid column \"%s\".\n", sort_key);
         exit(-1);
      }
   }
   return flags;
}

static void millisleep(unsigned long millisec) {
#ifdef HAVE_NANOSLEEP
   struct timespec req = {
      .tv_sec = 0,
      .tv_nsec = millisec * 1000000L
   };
   while(nanosleep(&req,&req)==-1) {
      continue;
   }
#else
	unsigned int sec = millisec / 1000;
	if(sec) sleep(sec);
	millisec %= 1000;
	usleep(millisec);
#endif
}

int main(int argc, char** argv) {
   const char *lc_ctype = getenv("LC_CTYPE");
   if(!lc_ctype) {
      lc_ctype = getenv("LC_ALL");
      if(!lc_ctype) lc_ctype = "";
   }
   setlocale(LC_CTYPE, lc_ctype);

   CommandLineSettings flags = parseArguments(argc, argv); // may exit(3)

#ifdef HAVE_PROC
   if (access(PROCDIR, R_OK) < 0) {
      fprintf(stderr, "Error: could not read procfs (compiled to look in %s).\n", PROCDIR);
      return 1;
   }
#endif
   CRT_initColorSchemes();
   Process_setupColumnWidths();
   UsersTable* ut = UsersTable_new();
   ProcessList* pl = ProcessList_new(ut, flags.pidWhiteList, flags.userId);
   Settings* settings = Settings_new(pl->cpuCount, Platform_haveSwap());
   pl->settings = settings;

   Header* header = Header_new(pl, settings, 2);

   Header_populateFromSettings(header);

   if (flags.delay != -1) settings->delay = flags.delay;
   // Use built-in MONOCHROME_COLOR_SCHEME for '--no-color'
   if (!flags.useColors) settings->colorScheme = MONOCHROME_COLOR_SCHEME;
   if (flags.treeView) settings->treeView = true;
   if(flags.explicit_delay) settings->explicit_delay = true;

   CRT_init(settings);
   CRT_setMouse(settings->use_mouse);
   CRT_setExplicitDelay(settings->explicit_delay);

   Panel *panel;
#ifdef DISK_STATS
   DiskList *disk_list = NULL;
   settings->disk_mode = flags.disk;
   if(flags.disk) {
      disk_list = DiskList_new(settings);
      panel = (Panel *)DiskPanel_new(settings, disk_list, header);
      DiskList_setPanel(disk_list, panel);
      header->disk_list = disk_list;
   } else
#endif
   {
      MainPanel *process_panel = MainPanel_new();
      panel = (Panel *)process_panel;
      ProcessList_setPanel(pl, panel);
      MainPanel_updateTreeFunctions(process_panel, settings->treeView);
   }

   if (flags.sortKey > 0) {
#ifdef DISK_STATS
      if(flags.disk) settings->disk_sort_key = flags.sortKey;
      else
#endif
      {
         settings->sortKey = flags.sortKey;
         settings->treeView = false;
      }
      settings->direction = 1;
   }
#ifdef DISK_STATS
   if(flags.disk) DiskList_printHeader(disk_list, Panel_getHeader(panel));
   else
#endif
   ProcessList_printHeader(pl, Panel_getHeader(panel));

#ifdef DISK_STATS
   if(!flags.disk)
#endif
   {
      State state = {
         .settings = settings,
         .ut = ut,
         .pl = pl,
         .panel = panel,
         .header = header,
         .repeat = 1
      };
      MainPanel_setState((MainPanel *)panel, &state);
   }

   ScreenManager* scr = ScreenManager_new(0, header->height, 0, -1, HORIZONTAL, header, settings, true);
   ScreenManager_add(scr, panel, -1);

#ifdef DISK_STATS
   if(flags.disk) {
      DiskList_scan(disk_list, 0);
      ProcessList_scan(pl, true);
   } else
#endif
   ProcessList_scan(pl, false);
   millisleep(75);
#ifdef DISK_STATS
   ProcessList_scan(pl, flags.disk);
#else
   ProcessList_scan(pl, false);
#endif

   ScreenManager_run(scr, NULL, NULL);
   attron(CRT_colors[HTOP_DEFAULT_COLOR]);
   mvhline(LINES-1, 0, ' ', COLS);
   attroff(CRT_colors[HTOP_DEFAULT_COLOR]);
   refresh();
   CRT_done();
   if (settings->changed) Settings_write(settings);
   Header_delete(header);
   ProcessList_delete(pl);
#ifdef DISK_STATS
   if(flags.disk) DiskList_delete(disk_list);
#endif

   ScreenManager_delete(scr);
   UsersTable_delete(ut);
   Settings_delete(settings);
   if(flags.pidWhiteList) {
      Hashtable_delete(flags.pidWhiteList);
   }

   return 0;
}
