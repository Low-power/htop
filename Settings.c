/*
htop - Settings.c
(C) 2004-2011 Hisham H. Muhammad
Copyright 2015-2023 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
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

}*/

#include "Settings.h"
#include "Platform.h"
#include "StringUtils.h"
#include "Vector.h"
#include "CRT.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define DEFAULT_DELAY 15

void Settings_delete(Settings* this) {
   free(this->filename);
   free(this->fields);
#ifdef DISK_STATS
   free(this->disk_fields);
#endif
   for (unsigned int i = 0; i < (sizeof(this->columns)/sizeof(MeterColumnSettings)); i++) {
      String_freeArray(this->columns[i].names);
      free(this->columns[i].modes);
   }
   String_freeArray(this->unsupported_options);
   free(this);
}

static void Settings_readMeters(Settings* this, char* line, int column) {
   char* trim = String_trim(line);
   int nIds;
   char** ids = String_split(trim, ' ', &nIds);
   free(trim);
   this->columns[column].names = ids;
}

static void Settings_readMeterModes(Settings* this, char* line, int column) {
   char* trim = String_trim(line);
   int nIds;
   char** ids = String_split(trim, ' ', &nIds);
   free(trim);
   int len = 0;
   for (int i = 0; ids[i]; i++) {
      len++;
   }
   this->columns[column].len = len;
   int* modes = xCalloc(len, sizeof(int));
   for (int i = 0; i < len; i++) {
      modes[i] = atoi(ids[i]);
   }
   String_freeArray(ids);
   this->columns[column].modes = modes;
}

static void Settings_defaultMeters(Settings* this, bool have_swap) {
   int sizes[] = { have_swap ? 3 : 2, this->cpuCount > 4 ? 4 : 3 };
   for (int i = 0; i < 2; i++) {
      this->columns[i].names = xCalloc(sizes[i] + 1, sizeof(char*));
      this->columns[i].modes = xCalloc(sizes[i], sizeof(int));
      this->columns[i].len = sizes[i];
   }
   int r = 0;
   if (this->cpuCount > 8) {
      this->columns[0].names[0] = xStrdup("LeftCPUs2");
      this->columns[0].modes[0] = BAR_METERMODE;
      this->columns[1].names[r] = xStrdup("RightCPUs2");
      this->columns[1].modes[r++] = BAR_METERMODE;
   } else if (this->cpuCount > 4) {
      this->columns[0].names[0] = xStrdup("LeftCPUs");
      this->columns[0].modes[0] = BAR_METERMODE;
      this->columns[1].names[r] = xStrdup("RightCPUs");
      this->columns[1].modes[r++] = BAR_METERMODE;
   } else {
      this->columns[0].names[0] = xStrdup("AllCPUs");
      this->columns[0].modes[0] = BAR_METERMODE;
   }
   this->columns[0].names[1] = xStrdup("Memory");
   this->columns[0].modes[1] = BAR_METERMODE;
   if(have_swap) {
      this->columns[0].names[2] = xStrdup("Swap");
      this->columns[0].modes[2] = BAR_METERMODE;
   }
   this->columns[1].names[r] = xStrdup("Tasks");
   this->columns[1].modes[r++] = TEXT_METERMODE;
   this->columns[1].names[r] = xStrdup("LoadAverage");
   this->columns[1].modes[r++] = TEXT_METERMODE;
   this->columns[1].names[r] = xStrdup("Uptime");
   this->columns[1].modes[r++] = TEXT_METERMODE;
}

static void readFields(unsigned int *fields, int* flags, const FieldData *field_data, unsigned int nfields, const char* line) {
   char* trim = String_trim(line);
   int nIds;
   char** ids = String_split(trim, ' ', &nIds);
   free(trim);
   unsigned int i = 0;
   int j = 0;
   *flags = 0;
   while(i < nfields && ids[i]) {
      // This "+1" is for compatibility with the older enum format.
      unsigned int id = atoi(ids[i]) + 1;
      if (id > 0 && field_data[id].name && id < nfields) {
         fields[j] = id;
         *flags |= field_data[id].flags;
         j++;
      }
      i++;
   }
   fields[j] = HTOP_NULL_PROCESSFIELD;
   String_freeArray(ids);
}

static bool Settings_read(Settings* this, const char* fileName, bool should_preserve_unsupported) {
   CRT_dropPrivileges();
   FILE *f = fopen(fileName, "r");
   CRT_restorePrivileges();
   if (!f) return false;
   int unsupported_count = 0;
   bool didReadFields = false;
   char *line;
   while((line = String_readLine(f))) {
      int nOptions;
      char** option = String_split(line, '=', &nOptions);
      if (nOptions < 2) {
         free(line);
         String_freeArray(option);
         continue;
      }
      if (String_eq(option[0], "fields")) {
         readFields(this->fields, &this->flags, Process_fields, Platform_numberOfFields, option[1]);
         didReadFields = true;
#ifdef DISK_STATS
      } else if(String_eq(option[0], "disk_fields")) {
         readFields(this->disk_fields, &this->disk_flags, Disk_fields, Disk_field_count, option[1]);
#endif
      } else if (String_eq(option[0], "sort_key")) {
         // This "+1" is for compatibility with the older enum format.
         this->sortKey = atoi(option[1]) + 1;
#ifdef DISK_STATS
      } else if(String_eq(option[0], "disk_sort_key")) {
         this->disk_sort_key = atoi(option[1]) + 1;
#endif
      } else if (String_eq(option[0], "sort_direction")) {
         this->direction = atoi(option[1]);
      } else if (String_eq(option[0], "tree_view")) {
         this->treeView = atoi(option[1]);
      } else if (String_eq(option[0], "hide_kernel_processes") || String_eq(option[0], "hide_kernel_threads")) {
         this->hide_kernel_processes = atoi(option[1]);
      } else if (String_eq(option[0], "hide_thread_processes") || String_eq(option[0], "hide_userland_threads")) {
         this->hide_thread_processes = atoi(option[1]);
#if defined __OpenBSD__ && defined PID_AND_MAIN_THREAD_ID_DIFFER
      } else if (String_eq(option[0], "hide_high_level_processes")) {
         this->hide_high_level_processes = atoi(option[1]);
#endif
      } else if (String_eq(option[0], "shadow_other_users")) {
         this->shadowOtherUsers = atoi(option[1]);
      } else if (String_eq(option[0], "show_thread_names")) {
         this->showThreadNames = atoi(option[1]);
      } else if (String_eq(option[0], "show_program_path")) {
         this->showProgramPath = atoi(option[1]);
      } else if (String_eq(option[0], "highlight_base_name")) {
         this->highlightBaseName = atoi(option[1]);
      } else if (String_eq(option[0], "highlight_megabytes")) {
         this->highlightMegabytes = atoi(option[1]);
      } else if (String_eq(option[0], "highlight_threads")) {
         this->highlightThreads = atoi(option[1]);
      } else if(String_eq(option[0], "highlight_kernel_processes")) {
         this->highlight_kernel_processes = atoi(option[1]);
      } else if (String_eq(option[0], "header_margin")) {
         this->headerMargin = atoi(option[1]);
      } else if (String_eq(option[0], "expand_system_time")) {
         // Compatibility option.
         this->detailedCPUTime = atoi(option[1]);
      } else if (String_eq(option[0], "detailed_cpu_time")) {
         this->detailedCPUTime = atoi(option[1]);
      } else if (String_eq(option[0], "cpu_count_from_zero")) {
         this->countCPUsFromZero = atoi(option[1]);
      } else if (String_eq(option[0], "update_process_names")) {
         this->updateProcessNames = atoi(option[1]);
      } else if(String_eq(option[0], "case_insensitive_sort")) {
         this->sort_strcmp = atoi(option[1]) ? strcasecmp : strcmp;
      } else if(String_eq(option[0], "explicit_delay")) {
         this->explicit_delay = atoi(option[1]);
      } else if(String_eq(option[0], "highlight_new_processes")) {
         this->highlight_new_processes = atoi(option[1]);
      } else if(String_eq(option[0], "tasks_meter_show_kernel_process_count")) {
         this->tasks_meter_show_kernel_process_count = atoi(option[1]);
      } else if(String_eq(option[0], "vi_mode")) {
         this->vi_mode = atoi(option[1]);
      } else if(String_eq(option[0], "use_mouse")) {
         this->use_mouse = atoi(option[1]);
      } else if(String_eq(option[0], "update_process_names_on_ctrl_l")) {
         this->update_process_names_on_ctrl_l = atoi(option[1]);
      } else if (String_eq(option[0], "account_guest_in_cpu_meter")) {
         this->accountGuestInCPUMeter = atoi(option[1]);
      } else if (String_eq(option[0], "delay")) {
         this->delay = atoi(option[1]);
      } else if (String_eq(option[0], "color_scheme")) {
         this->colorScheme = CRT_getColorSchemeIndexForName(option[1]);
         if(this->colorScheme < 0) {
            this->colorScheme = atoi(option[1]);
            if (this->colorScheme < 0 || this->colorScheme >= CRT_color_scheme_count) {
               //this->colorScheme = DEFAULT_COLOR_SCHEME;
               this->colorScheme = CRT_getDefaultColorScheme();
            }
         }
      } else if (String_eq(option[0], "left_meters")) {
         Settings_readMeters(this, option[1], 0);
      } else if (String_eq(option[0], "right_meters")) {
         Settings_readMeters(this, option[1], 1);
      } else if (String_eq(option[0], "left_meter_modes")) {
         Settings_readMeterModes(this, option[1], 0);
      } else if (String_eq(option[0], "right_meter_modes")) {
         Settings_readMeterModes(this, option[1], 1);
      } else if(should_preserve_unsupported) {
         this->unsupported_options = xRealloc(this->unsupported_options, (unsupported_count + 1) * sizeof(char *));
         this->unsupported_options[unsupported_count++] = line;
         this->unsupported_options[unsupported_count] = NULL;
         line = NULL;
      }
      free(line);
      String_freeArray(option);
   }
   fclose(f);
   return didReadFields;
}

static void writeFields(FILE *f, const unsigned int *fields, const char* name) {
   fprintf(f, "%s=", name);
   const char* sep = "";
   for (int i = 0; fields[i]; i++) {
      // This "-1" is for compatibility with the older enum format.
      fprintf(f, "%s%d", sep, (int) fields[i]-1);
      sep = " ";
   }
   fputc('\n', f);
}

static void writeMeters(Settings* this, FILE *f, int column) {
   const char* sep = "";
   for (int i = 0; i < this->columns[column].len; i++) {
      fprintf(f, "%s%s", sep, this->columns[column].names[i]);
      sep = " ";
   }
   fputc('\n', f);
}

static void writeMeterModes(Settings* this, FILE *f, int column) {
   const char* sep = "";
   for (int i = 0; i < this->columns[column].len; i++) {
      fprintf(f, "%s%d", sep, this->columns[column].modes[i]);
      sep = " ";
   }
   fputc('\n', f);
}

bool Settings_write(Settings* this) {
   CRT_dropPrivileges();
   FILE *f = fopen(this->filename, "w");
   CRT_restorePrivileges();

   if (f == NULL) {
      return false;
   }
   fprintf(f, "# Beware! This file is rewritten by htop when settings are changed in the interface.\n");
   fprintf(f, "# The parser is also very primitive, and not human-friendly.\n");
   writeFields(f, this->fields, "fields");
#ifdef DISK_STATS
   writeFields(f, this->disk_fields, "disk_fields");
#endif
   // This "-1" is for compatibility with the older enum format.
   fprintf(f, "sort_key=%d\n", (int) this->sortKey-1);
#ifdef DISK_STATS
   fprintf(f, "disk_sort_key=%d\n", (int)this->disk_sort_key - 1);
#endif
   fprintf(f, "sort_direction=%d\n", this->direction);
   fprintf(f, "hide_kernel_processes=%d\n", (int) this->hide_kernel_processes);
   fprintf(f, "hide_thread_processes=%d\n", (int) this->hide_thread_processes);
#if defined __OpenBSD__ && defined PID_AND_MAIN_THREAD_ID_DIFFER
   fprintf(f, "hide_high_level_processes=%d\n", (int)this->hide_high_level_processes);
#endif
   fprintf(f, "shadow_other_users=%d\n", (int) this->shadowOtherUsers);
   fprintf(f, "show_thread_names=%d\n", (int) this->showThreadNames);
   fprintf(f, "show_program_path=%d\n", (int) this->showProgramPath);
   fprintf(f, "highlight_base_name=%d\n", (int) this->highlightBaseName);
   fprintf(f, "highlight_megabytes=%d\n", (int) this->highlightMegabytes);
   fprintf(f, "highlight_threads=%d\n", (int) this->highlightThreads);
   fprintf(f, "highlight_kernel_processes=%d\n", (int)this->highlight_kernel_processes);
   fprintf(f, "tree_view=%d\n", (int) this->treeView);
   fprintf(f, "header_margin=%d\n", (int) this->headerMargin);
   fprintf(f, "detailed_cpu_time=%d\n", (int) this->detailedCPUTime);
   fprintf(f, "cpu_count_from_zero=%d\n", (int) this->countCPUsFromZero);
   fprintf(f, "update_process_names=%d\n", (int) this->updateProcessNames);
   fprintf(f, "case_insensitive_sort=%d\n", (int)(this->sort_strcmp == strcasecmp));
   fprintf(f, "explicit_delay=%d\n", (int)this->explicit_delay);
   fprintf(f, "highlight_new_processes=%d\n", (int)this->highlight_new_processes);
   fprintf(f, "tasks_meter_show_kernel_process_count=%d\n", (int)this->tasks_meter_show_kernel_process_count);
   fprintf(f, "vi_mode=%d\n", (int)this->vi_mode);
   fprintf(f, "use_mouse=%d\n", (int)this->use_mouse);
   fprintf(f, "update_process_names_on_ctrl_l=%d", (int)this->update_process_names_on_ctrl_l);
   fprintf(f, "account_guest_in_cpu_meter=%d\n", (int) this->accountGuestInCPUMeter);
   fprintf(f, "color_scheme=%s\n", CRT_color_scheme_names[this->colorScheme]);
   fprintf(f, "delay=%d\n", this->delay);
   fprintf(f, "left_meters="); writeMeters(this, f, 0);
   fprintf(f, "left_meter_modes="); writeMeterModes(this, f, 0);
   fprintf(f, "right_meters="); writeMeters(this, f, 1);
   fprintf(f, "right_meter_modes="); writeMeterModes(this, f, 1);
   if(this->unsupported_options) {
      char **p = this->unsupported_options;
      while(*p) {
         fputs(*p, f);
         fputc('\n', f);
         p++;
      }
   }
   fclose(f);
   return true;
}

static const char *get_terminal_type() {
	const char *term = getenv("TERM");
	if(!term || !*term) return NULL;
	int i = 0;
	do {
		switch(term[i]) {
			case 0x4:
			case '\b':
			case '	':
			case '\n':
			case '\r':
			case 0x1b:
			case ' ':
			case '"':
			case '#':
			case '\'':
			case '*':
			case '/':
			case ':':
			case '\\':
			case 0x7f:
				return NULL;
		}
		if(++i > 31) return NULL;
	} while(term[i]);
	return term;
}

// Make sure the last process field is HTOP_COMM_FIELD
static void check_fields(Settings *this) {
	unsigned int i = 0;
	while(this->fields[i]) {
		if(this->fields[i] == HTOP_COMM_FIELD) return;
		if(++i >= Platform_numberOfFields) return;
	}
	this->fields[i++] = HTOP_COMM_FIELD;
	this->fields[i] = HTOP_NULL_PROCESSFIELD;
}

Settings* Settings_new(int cpuCount, bool have_swap) {
   Settings* this = xCalloc(1, sizeof(Settings));

   this->sortKey = HTOP_PERCENT_CPU_FIELD;
#ifdef DISK_STATS
   this->disk_sort_key = HTOP_DISK_PERCENT_UTIL_FIELD;
#endif
   this->direction = 1;
   this->shadowOtherUsers = false;
   this->showThreadNames = false;
   this->hide_kernel_processes = false;
   this->hide_thread_processes = false;
#if defined __OpenBSD__ && defined PID_AND_MAIN_THREAD_ID_DIFFER
   this->hide_high_level_processes = false;
#endif
   this->treeView = false;
   this->highlightBaseName = false;
   this->highlightMegabytes = false;
   this->detailedCPUTime = false;
   this->countCPUsFromZero = false;
   this->updateProcessNames = false;
   this->cpuCount = cpuCount;
   this->showProgramPath = true;
   this->highlightThreads = true;
   this->highlight_kernel_processes = true;
   this->fields = xCalloc(Platform_numberOfFields, sizeof(unsigned int));
   // TODO: turn 'fields' into a Vector
   this->flags = 0;
   ProcessField* defaults = Platform_defaultFields;
   for (int i = 0; defaults[i]; i++) {
      this->fields[i] = defaults[i];
      this->flags |= Process_fields[defaults[i]].flags;
   }
#ifdef DISK_STATS
   this->disk_fields = xCalloc(Disk_field_count, sizeof(unsigned int));
   this->disk_flags = 0;
   const DiskField *disk_field = Disk_default_fields;
   unsigned int i = 0;
   while(*disk_field && i < Disk_field_count) {
      this->disk_fields[i++] = *disk_field;
      this->disk_flags |= Disk_fields[*disk_field].flags;
      disk_field++;
   }
#endif
   this->sort_strcmp = strcmp;
   this->explicit_delay = false;
   this->highlight_new_processes = false;
   this->tasks_meter_show_kernel_process_count = true;
   this->vi_mode = false;
   this->use_mouse = true;
   this->update_process_names_on_ctrl_l = false;

   char* legacyDotfile = NULL;
   char *global_file_path = NULL;
   const char *override = getenv("HTOPRC");
   if (override) {
      this->filename = xStrdup(override);
   } else {
      const char *home;
      char* htopDir = CRT_getConfigDirPath(&home);
      const char *term = get_terminal_type();
      global_file_path = String_cat(htopDir, "htoprc");
      if(term) {
         size_t global_len = strlen(global_file_path);
         size_t term_len = strlen(term);
         this->filename = xMalloc(global_len + 1 + term_len + 1);
         memcpy(this->filename, global_file_path, global_len);
         this->filename[global_len] = '.';
         memcpy(this->filename + global_len + 1, term, term_len);
         this->filename[global_len + 1 + term_len] = 0;
         this->terminal_type = this->filename + global_len + 1;
      } else {
         this->filename = global_file_path;
         global_file_path = NULL;
      }
      legacyDotfile = String_cat(home, "/.htoprc");
      free(htopDir);
      struct stat st;
      CRT_dropPrivileges();
      if (lstat(legacyDotfile, &st) < 0 || S_ISLNK(st.st_mode)) {
         free(legacyDotfile);
         legacyDotfile = NULL;
      }
      CRT_restorePrivileges();
   }
   this->colorScheme = CRT_getDefaultColorScheme();
   this->changed = false;
   this->delay = DEFAULT_DELAY;
   bool ok = false;
   if (legacyDotfile) {
      ok = Settings_read(this, legacyDotfile, false);
      if (ok) {
         // Transition to new location and delete old configuration file
         if (Settings_write(this)) unlink(legacyDotfile);
         check_fields(this);
      }
      free(legacyDotfile);
   }
   if (!ok) {
      ok = Settings_read(this, this->filename, true);
   }
   if(!ok && !override && global_file_path) {
      // Try read 'htoprc' without terminal name suffix
      ok = Settings_read(this, global_file_path, false);
      if(ok) check_fields(this);
   }
   free(global_file_path);
   if (!ok) {
      this->changed = true;
      // TODO: how to get SYSCONFDIR correctly through Autoconf?
      char* systemSettings = String_cat(SYSCONFDIR, "/htoprc");
      ok = Settings_read(this, systemSettings, false);
      free(systemSettings);
   }
   if (!ok) {
      Settings_defaultMeters(this, have_swap);
      this->hide_kernel_processes = true;
      this->highlightMegabytes = true;
      this->highlightThreads = true;
      this->headerMargin = true;
   }
   return this;
}

void Settings_invertSortOrder(Settings* this) {
   this->direction = (this->direction == 1) ? -1 : 1;
}
