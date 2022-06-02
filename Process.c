/*
htop - Process.c
(C) 2004-2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "Settings.h"

#include "CRT.h"
#include "StringUtils.h"
#include "RichString.h"
#include "Platform.h"
#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <pwd.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#ifdef MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#elif defined(MAJOR_IN_SYSMACROS) || \
   (defined(HAVE_SYS_SYSMACROS_H) && HAVE_SYS_SYSMACROS_H)
#include <sys/sysmacros.h>
#endif
#ifdef HAVE_LIBNCURSESW
#include <limits.h>
#include <wchar.h>
#endif

#ifdef __ANDROID__
#define SYS_ioprio_get __NR_ioprio_get
#define SYS_ioprio_set __NR_ioprio_set
#endif

/*{
#include "config.h"
#include "Object.h"
#include <sys/types.h>
#include <stdbool.h>

#define uintcmp(n1,n2) ((n1)>(n2)?1:((n1)<(n2)?-1:0))

#define PROCESS_FLAG_IO 0x0001

typedef enum {
   HTOP_NULL_PROCESSFIELD = 0,
   HTOP_PID_FIELD = 1,
   HTOP_NAME_FIELD = 2,
   HTOP_STATE_FIELD = 3,
   HTOP_PPID_FIELD = 4,
   HTOP_PGRP_FIELD = 5,
   HTOP_SESSION_FIELD = 6,
   HTOP_TTY_FIELD = 7,
   HTOP_TPGID_FIELD = 8,
   HTOP_MINFLT_FIELD = 10,
   HTOP_MAJFLT_FIELD = 12,
   HTOP_PRIORITY_FIELD = 18,
   HTOP_NICE_FIELD = 19,
   HTOP_STARTTIME_FIELD = 21,
   HTOP_PROCESSOR_FIELD = 38,
   HTOP_M_SIZE_FIELD = 39,
   HTOP_M_RESIDENT_FIELD = 40,
   HTOP_EFFECTIVE_UID_FIELD = 46,
   HTOP_PERCENT_CPU_FIELD = 47,
   HTOP_PERCENT_MEM_FIELD = 48,
   HTOP_EFFECTIVE_USER_FIELD = 49,
   HTOP_TIME_FIELD = 50,
   HTOP_NLWP_FIELD = 51,
   HTOP_TGID_FIELD = 52,
   HTOP_REAL_UID_FIELD = 53,
   HTOP_REAL_USER_FIELD = 54,
   HTOP_COMM_FIELD = 99
} ProcessField;

typedef struct ProcessPidColumn_ {
   int id;
   char* label;
} ProcessPidColumn;

typedef struct Process_ {
   Object super;

   struct Settings_* settings;

   bool created;
   bool updated;

   bool tag;
   bool showChildren;
   bool show;
   bool seen_in_tree_loop;
   int indent;

   int state;
   pid_t pid;
   pid_t ppid;
   pid_t tgid;
   char *name;
   char* comm;
   int commLen;
   int argv0_length;

   unsigned int pgrp;
   unsigned int session;
   dev_t tty_nr;
   int tpgid;
   uid_t ruid;
   uid_t euid;

   char *real_user;
   char *effective_user;

   unsigned long long int time;
   int processor;

   float percent_cpu;
   float percent_mem;

   long int priority;
   long int nice;
   long int nlwp;
   time_t starttime_ctime;

   // In pages
   long int m_size;
   long int m_resident;

   int exit_signal;

   unsigned long int minflt;
   unsigned long int majflt;
} Process;

typedef struct ProcessFieldData_ {
   const char* name;
   const char* title;
   const char* description;
   int flags;
} ProcessFieldData;

// Implemented in platform-specific code:
void Process_writeField(Process* this, RichString* str, ProcessField field);
long Process_compare(const void* v1, const void* v2);
bool Process_isKernelProcess(const Process *);
bool Process_isExtraThreadProcess(const Process *);
char **Process_getKernelStackTrace(const Process *);
extern ProcessFieldData Process_fields[];
extern ProcessPidColumn Process_pidColumns[];
extern char Process_pidFormat[20];

typedef Process*(*Process_New)(struct Settings_*);
typedef void (*Process_WriteField)(Process*, RichString*, ProcessField);
typedef bool (*ProcessSendSignalFunction)(const Process *, int);

typedef struct ProcessClass_ {
   ObjectClass super;
   Process_WriteField writeField;
   ProcessSendSignalFunction sendSignal;
} ProcessClass;

#define As_Process(this_)              ((ProcessClass*)((this_)->super.klass))

#if defined __OpenBSD__ && defined PID_AND_MAIN_THREAD_ID_DIFFER
#define Process_getParentPid(process_) ((process_)->tgid == (process_)->pid || (process_)->settings->hide_high_level_processes ? (process_)->ppid : (process_)->tgid)
#define Process_isChildOf(process_, pid_) ((process_)->tgid == (pid_) || (((process_)->tgid == (process_)->pid || (process_)->settings->hide_high_level_processes) && (process_)->ppid == (pid_)))
#else
#define Process_getParentPid(process_) ((process_)->tgid == (process_)->pid ? (process_)->ppid : (process_)->tgid)
#define Process_isChildOf(process_, pid_) ((process_)->tgid == (pid_) || ((process_)->tgid == (process_)->pid && (process_)->ppid == (pid_)))
#endif

#define Process_sortState(state) ((state) == 'I' ? 0x100 : (state))

}*/

#ifndef NODEV
#ifdef __INTERIX
#define NODEV ((dev_t)0)
#else
#define NODEV ((dev_t)-1)
#endif
#endif

static int Process_getuid = -1;

char Process_pidFormat[20] = "%7d ";

static char Process_titleBuffer[20][20];

void Process_setupColumnWidths() {
   int maxPid = Platform_getMaxPid();
   if (maxPid == -1) return;
   int digits = ceil(log10(maxPid));
   assert(digits < 20);
   for (int i = 0; Process_pidColumns[i].label; i++) {
      assert(i < 20);
      xSnprintf(Process_titleBuffer[i], 20, "%*s ", digits, Process_pidColumns[i].label);
      Process_fields[Process_pidColumns[i].id].title = Process_titleBuffer[i];
   }
   xSnprintf(Process_pidFormat, sizeof(Process_pidFormat), "%%%dd ", digits);
}

void Process_humanNumber(RichString* str, unsigned long number, bool coloring) {
   char buffer[11];
   int len;
   int largeNumberColor = CRT_colors[HTOP_LARGE_NUMBER_COLOR];
   int processMegabytesColor = CRT_colors[HTOP_PROCESS_MEGABYTES_COLOR];
   int processColor = CRT_colors[HTOP_PROCESS_COLOR];
   if (!coloring) {
      largeNumberColor = CRT_colors[HTOP_PROCESS_COLOR];
      processMegabytesColor = CRT_colors[HTOP_PROCESS_COLOR];
   }
   if(number >= (10 * ONE_DECIMAL_M)) {
      #ifdef __LP64__
      if(number >= (100 * ONE_DECIMAL_G)) {
         len = snprintf(buffer, sizeof buffer, "%4luT ", number / ONE_BINARY_G);
         RichString_appendn(str, largeNumberColor, buffer, len);
         return;
      } else if (number >= (1000 * ONE_DECIMAL_M)) {
         len = snprintf(buffer, sizeof buffer, "%4.1lfT ", (double)number / ONE_BINARY_G);
         if((size_t)len > sizeof buffer - 1) {
            strcpy(buffer, "***** ");
            len = 6;
         }
         RichString_appendn(str, largeNumberColor, buffer, len);
         return;
      }
      #endif
      if(number >= (100 * ONE_DECIMAL_M)) {
         len = snprintf(buffer, sizeof buffer, "%4luG ", number / ONE_BINARY_M);
         if((size_t)len > sizeof buffer - 1) {
            strcpy(buffer, "***** ");
            len = 6;
         }
         RichString_appendn(str, largeNumberColor, buffer, len);
         return;
      }
      len = snprintf(buffer, sizeof buffer, "%4.1lfG ", (double)number / ONE_BINARY_M);
      RichString_appendn(str, largeNumberColor, buffer, len);
      return;
   } else if (number >= 10000) {
      len = snprintf(buffer, sizeof buffer, "%4luM ", number / ONE_BINARY_K);
      RichString_appendn(str, processMegabytesColor, buffer, len);
      return;
   } else if (number >= 1000) {
      len = snprintf(buffer, sizeof buffer, "%1lu", number / 1000);
      RichString_appendn(str, processMegabytesColor, buffer, len);
      number %= 1000;
      len = snprintf(buffer, sizeof buffer, "%03luK ", number);
      RichString_appendn(str, processColor, buffer, len);
      return;
   }
   len = snprintf(buffer, sizeof buffer, "%4luK ", number);
   RichString_appendn(str, processColor, buffer, len);
}

void Process_colorNumber(RichString* str, unsigned long long number, bool coloring) {
   char buffer[13];

   int largeNumberColor = CRT_colors[HTOP_LARGE_NUMBER_COLOR];
   int processMegabytesColor = CRT_colors[HTOP_PROCESS_MEGABYTES_COLOR];
   int processColor = CRT_colors[HTOP_PROCESS_COLOR];
   int processShadowColor = CRT_colors[HTOP_PROCESS_SHADOW_COLOR];
   if (!coloring) {
      largeNumberColor = CRT_colors[HTOP_PROCESS_COLOR];
      processMegabytesColor = CRT_colors[HTOP_PROCESS_COLOR];
      processShadowColor = CRT_colors[HTOP_PROCESS_COLOR];
   }

   if ((long long) number == -1LL) {
      int len = snprintf(buffer, sizeof buffer, "    no perm ");
      RichString_appendn(str, CRT_colors[HTOP_PROCESS_SHADOW_COLOR], buffer, len);
   } else if (number > 10000000000ULL) {
      // Allow the trailing space be truncated by snprintf(3)
      int len = snprintf(buffer, sizeof buffer, "%10lluk ", number / 1000);
      if(len > 13) {
         RichString_appendn(str, largeNumberColor, ">9999999999k", 12);
      } else {
         int large_part_len = len > 12 ? 5 : 4;
         RichString_appendn(str, largeNumberColor, buffer, large_part_len);
         RichString_appendn(str, processMegabytesColor, buffer + large_part_len, 3);
         RichString_appendn(str, processColor, buffer + large_part_len + 3, len > 12 ? 4 : 5);
      }
   } else {
      xSnprintf(buffer, sizeof buffer, "%11llu ", number);
      RichString_appendn(str, largeNumberColor, buffer, 2);
      RichString_appendn(str, processMegabytesColor, buffer+2, 3);
      RichString_appendn(str, processColor, buffer+5, 3);
      RichString_appendn(str, processShadowColor, buffer+8, 4);
   }
}

void Process_printTime(RichString* str, unsigned long long totalHundredths) {
   unsigned long long totalSeconds = totalHundredths / 100;

   unsigned long long hours = totalSeconds / 3600;
   int minutes = (totalSeconds / 60) % 60;
   int seconds = totalSeconds % 60;
   int hundredths = totalHundredths - (totalSeconds * 100);
   char buffer[23];
   if (hours >= 100) {
      xSnprintf(buffer, sizeof buffer, "%7lluh ", hours);
      RichString_append(str, CRT_colors[HTOP_LARGE_NUMBER_COLOR], buffer);
   } else {
      if (hours) {
         xSnprintf(buffer, sizeof buffer, "%2lluh", hours);
         RichString_append(str, CRT_colors[HTOP_LARGE_NUMBER_COLOR], buffer);
         xSnprintf(buffer, sizeof buffer, "%02d:%02d ", minutes, seconds);
      } else {
         xSnprintf(buffer, sizeof buffer, "%2d:%02d.%02d ", minutes, seconds, hundredths);
      }
      RichString_append(str, CRT_colors[HTOP_DEFAULT_COLOR], buffer);
   }
}

static inline void Process_writeCommand(Process* this, int attr, int baseattr, RichString* str) {
   int start = RichString_size(str), finish = 0;
   char* comm = this->comm;

   if (this->settings->highlightBaseName || !this->settings->showProgramPath) {
      int i, basename = 0;
      for (i = 0; i < this->argv0_length; i++) {
         if (comm[i] == '/') {
            basename = i + 1;
         } else if (comm[i] == ':') {
            finish = i + 1;
            break;
         }
      }
      if (!finish) {
         if (this->settings->showProgramPath)
            start += basename;
         else
            comm += basename;
         finish = this->argv0_length - basename;
      }
      finish += start - 1;
   }

   RichString_append(str, attr, comm);

   if (this->settings->highlightBaseName)
      RichString_setAttrn(str, baseattr, start, finish);
}

void Process_outputRate(RichString* str, char* buffer, int n, double rate, int coloring) {
   int largeNumberColor = CRT_colors[HTOP_LARGE_NUMBER_COLOR];
   int processMegabytesColor = CRT_colors[HTOP_PROCESS_MEGABYTES_COLOR];
   int processColor = CRT_colors[HTOP_PROCESS_COLOR];
   if (!coloring) {
      largeNumberColor = CRT_colors[HTOP_PROCESS_COLOR];
      processMegabytesColor = CRT_colors[HTOP_PROCESS_COLOR];
   }
   if (rate < 0) {
      int len = snprintf(buffer, n, "    no perm ");
      RichString_appendn(str, CRT_colors[HTOP_PROCESS_SHADOW_COLOR], buffer, len);
   } else if (rate < ONE_BINARY_K) {
      int len = snprintf(buffer, n, "%8.2fB/s ", rate);
      RichString_appendn(str, processColor, buffer, len);
   } else if (rate < ONE_BINARY_K * ONE_BINARY_K) {
      int len = snprintf(buffer, n, "%6.1fKiB/s ", rate / ONE_BINARY_K);
      RichString_appendn(str, processColor, buffer, len);
   } else if (rate < ONE_BINARY_K * ONE_BINARY_K * ONE_BINARY_K) {
      int len = snprintf(buffer, n, "%6.1fMiB/s ", rate / ONE_BINARY_K / ONE_BINARY_K);
      RichString_appendn(str, processMegabytesColor, buffer, len);
   } else {
      int len = snprintf(buffer, n, "%6.1fGiB/s ", rate / ONE_BINARY_K / ONE_BINARY_K / ONE_BINARY_K);
      RichString_appendn(str, largeNumberColor, buffer, len);
   }
}

static void copy_fixed_width_field(char *buffer, size_t buffer_size, const char *s, int width) {
   assert(buffer_size > (size_t)width);
#ifdef HAVE_LIBNCURSESW
   wchar_t wcs[width + 1];
   int truncate_count = 0;
   do {
      size_t wc_count = mbstowcs(wcs, s, width + 1 - truncate_count);
      if(wc_count != (size_t)-1) {
         char mb_buffer[MB_LEN_MAX];
         size_t out_i = 0;
         for(size_t wcs_i = 0; wcs_i < wc_count && width > 1; wcs_i++) {
            int mb_len = wctomb(mb_buffer, wcs[wcs_i]);
            if(mb_len < 0 || out_i + mb_len + 1 >= buffer_size) break;
            int cw = wcwidth(wcs[wcs_i]);
            if(cw < 0) {
               width--;
               buffer[out_i++] = '?';
            } else {
               width -= cw;
               if(width < 1) break;
               memcpy(buffer + out_i, mb_buffer, mb_len);
               out_i += mb_len;
            }
         }
         if(out_i > 0) {
            if(out_i + width >= buffer_size) out_i = buffer_size - width - 1;
            memset(buffer + out_i, ' ', width);
            buffer[out_i + width] = 0;
            return;
         }
         break;
      }
   } while(++truncate_count < width + 1);
#endif
   xSnprintf(buffer, buffer_size, "%-*s ", width, s);
   if(buffer[width]) {
      buffer[width - 1] = ' ';
      buffer[width] = 0;
   }
}

void Process_writeField(Process* this, RichString* str, ProcessField field) {
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[HTOP_DEFAULT_COLOR];
   int baseattr = CRT_colors[HTOP_PROCESS_BASENAME_COLOR];
   int n = sizeof(buffer);
   bool coloring = this->settings->highlightMegabytes;

   switch (field) {
         struct tm tm;
      case HTOP_PERCENT_CPU_FIELD:
         if (this->percent_cpu > 999.9) {
            xSnprintf(buffer, n, "%4u ", (unsigned int)this->percent_cpu);
         } else if (this->percent_cpu > 99.9) {
            xSnprintf(buffer, n, "%3u. ", (unsigned int)this->percent_cpu);
         } else {
            xSnprintf(buffer, n, "%4.1f ", this->percent_cpu);
         }
         break;
      case HTOP_PERCENT_MEM_FIELD:
         if (this->percent_mem > 99.9) {
            xSnprintf(buffer, n, "100. ");
         } else {
            xSnprintf(buffer, n, "%4.1f ", this->percent_mem);
         }
         break;
      case HTOP_NAME_FIELD:
         copy_fixed_width_field(buffer, n, this->name, 16);
         break;
      case HTOP_COMM_FIELD:
         if (this->settings->highlightThreads && Process_isExtraThreadProcess(this)) {
            attr = CRT_colors[HTOP_PROCESS_THREAD_COLOR];
            baseattr = CRT_colors[HTOP_PROCESS_THREAD_BASENAME_COLOR];
         } else if(this->settings->highlight_kernel_processes && Process_isKernelProcess(this)) {
            attr = CRT_colors[HTOP_PROCESS_KERNEL_PROCESS_COLOR];
            baseattr = CRT_colors[HTOP_PROCESS_KERNEL_PROCESS_COLOR];
         }
         if (!this->settings->treeView || this->indent == 0) {
            Process_writeCommand(this, attr, baseattr, str);
            return;
         } else {
            char* buf = buffer;
            int maxIndent = 0;
            bool lastItem = (this->indent < 0);
            int indent = (this->indent < 0 ? -this->indent : this->indent);
            for (int i = 0; i < 32; i++) if (indent & (1U << i)) maxIndent = i+1;
            for (int i = 0; i < maxIndent - 1; i++) {
               int written, ret;
               ret = (indent & (1 << i)) ?
                  snprintf(buf, n, "%s  ", CRT_treeStr[TREE_STR_VERT]) : snprintf(buf, n, "   ");
               written = (ret < 0 || ret >= n) ? n : ret;
               buf += written;
               n -= written;
            }
            const char* draw = CRT_treeStr[lastItem ? (this->settings->direction == 1 ? TREE_STR_BEND : TREE_STR_TEND) : TREE_STR_RTEE];
            xSnprintf(buf, n, "%s%s ", draw, this->showChildren ? CRT_treeStr[TREE_STR_SHUT] : CRT_treeStr[TREE_STR_OPEN] );
            RichString_append(str, CRT_colors[HTOP_PROCESS_TREE_COLOR], buffer);
            Process_writeCommand(this, attr, baseattr, str);
            return;
         }
      case HTOP_MAJFLT_FIELD:
         Process_colorNumber(str, this->majflt, coloring);
         return;
      case HTOP_MINFLT_FIELD:
         Process_colorNumber(str, this->minflt, coloring);
         return;
      case HTOP_M_RESIDENT_FIELD:
         Process_humanNumber(str, this->m_resident * CRT_page_size_kib, coloring);
         return;
      case HTOP_M_SIZE_FIELD:
         Process_humanNumber(str, this->m_size * CRT_page_size_kib, coloring);
         return;
      case HTOP_NICE_FIELD:
         n = snprintf(buffer, n, "%3ld", this->nice);
         assert(n >= 3);
         if(n == 3) {
            buffer[3] = ' ';
            buffer[4] = 0;
         }
         if(this->nice < 0) attr = CRT_colors[HTOP_PROCESS_HIGH_PRIORITY_COLOR];
         else if(this->nice > 0) attr = CRT_colors[HTOP_PROCESS_LOW_PRIORITY_COLOR];
         break;
      case HTOP_NLWP_FIELD:
         xSnprintf(buffer, n, "%4ld ", this->nlwp);
         break;
      case HTOP_PGRP_FIELD:
         xSnprintf(buffer, n, Process_pidFormat, this->pgrp);
         break;
      case HTOP_PID_FIELD:
         xSnprintf(buffer, n, Process_pidFormat, this->pid);
         break;
      case HTOP_PPID_FIELD:
         xSnprintf(buffer, n, Process_pidFormat, this->ppid);
         break;
      case HTOP_PRIORITY_FIELD:
         if(this->priority <= -100) xSnprintf(buffer, n, " RT ");
         else xSnprintf(buffer, n, "%3ld ", this->priority);
         break;
      case HTOP_PROCESSOR_FIELD:
         if(this->processor < 0) strcpy(buffer, "  - ");
         else xSnprintf(buffer, n, "%3d ", Settings_cpuId(this->settings, this->processor));
         break;
      case HTOP_SESSION_FIELD:
         xSnprintf(buffer, n, Process_pidFormat, this->session);
         break;
      case HTOP_STARTTIME_FIELD:
         localtime_r(&this->starttime_ctime, &tm);
         n = strftime(buffer, n, (this->starttime_ctime > time(NULL) - 86400) ? "%R " : "%b%d ", &tm);
         if(n != 6) {
            if(n < 6) memset(buffer + n, ' ', 6 - n);
            buffer[6] = 0;
         }
         break;
      case HTOP_STATE_FIELD:
         xSnprintf(buffer, n, "%c ", this->state);
         switch(this->state) {
            case 'R':
            case 'O':
               attr = CRT_colors[HTOP_PROCESS_R_STATE_COLOR];
               break;
            case 'D':
               attr = CRT_colors[HTOP_PROCESS_D_STATE_COLOR];
               break;
            case 'Z':
               attr = CRT_colors[HTOP_PROCESS_Z_STATE_COLOR];
               break;
         }
         break;
      case HTOP_REAL_UID_FIELD:
         xSnprintf(buffer, n, "%6d ", (int)this->ruid);
         break;
      case HTOP_EFFECTIVE_UID_FIELD:
         xSnprintf(buffer, n, "%6d ", (int)this->euid);
         break;
      case HTOP_TIME_FIELD:
         Process_printTime(str, this->time);
         return;
      case HTOP_TGID_FIELD:
         xSnprintf(buffer, n, Process_pidFormat, (int)this->tgid);
         break;
      case HTOP_TPGID_FIELD:
         xSnprintf(buffer, n, Process_pidFormat, (int)this->tpgid);
         break;
      case HTOP_TTY_FIELD:
         if(this->tty_nr == NODEV) {
            xSnprintf(buffer, n, "      ? ");
            attr = CRT_colors[HTOP_PROCESS_SHADOW_COLOR];
         } else {
            xSnprintf(buffer, n, "%3u:%3u ", (unsigned int)major(this->tty_nr), (unsigned int)minor(this->tty_nr));
         }
         break;
      case HTOP_REAL_USER_FIELD:
         if (Process_getuid != (int)this->ruid) attr = CRT_colors[HTOP_PROCESS_SHADOW_COLOR];
         if (this->real_user) {
            xSnprintf(buffer, n, "%-9s ", this->real_user);
         } else {
            xSnprintf(buffer, n, "%-9d ", (int)this->ruid);
         }
         goto user_end;
      case HTOP_EFFECTIVE_USER_FIELD:
         if (Process_getuid != (int)this->euid) attr = CRT_colors[HTOP_PROCESS_SHADOW_COLOR];
         if (this->effective_user) {
            xSnprintf(buffer, n, "%-9s ", this->effective_user);
         } else {
            xSnprintf(buffer, n, "%-9d ", (int)this->euid);
         }
      user_end:
         if (buffer[10]) {
            buffer[9] = ' ';
            buffer[10] = '\0';
         }
         break;
      default:
         xSnprintf(buffer, n, "- ");
         break;
   }
   RichString_append(str, attr, buffer);
}

void Process_display(Object* cast, RichString* out) {
   Process* this = (Process*) cast;
   ProcessField* fields = this->settings->fields;
   RichString_prune(out);
   for (int i = 0; fields[i]; i++) {
      As_Process(this)->writeField(this, out, fields[i]);
   }
   if (this->settings->shadowOtherUsers && (int)this->ruid != Process_getuid && (int)this->euid != Process_getuid) {
      RichString_setAttr(out, CRT_colors[HTOP_PROCESS_SHADOW_COLOR]);
   }
   if (this->tag) {
      RichString_setAttr(out, CRT_colors[HTOP_PROCESS_TAG_COLOR]);
   } else if(this->created && this->settings->highlight_new_processes) {
      RichString_setAttr(out, CRT_colors[HTOP_PROCESS_CREATED_COLOR]);
   }
   assert(out->chlen > 0);
}

void Process_done(Process* this) {
   assert (this != NULL);
   free(this->name);
   free(this->comm);
}

static bool base_Process_sendSignal(const Process *this, int sig) {
	return kill(this->pid, sig) == 0;
}

static void Process_inherit(ObjectClass *super_class) {
	if(!super_class->display) super_class->display = Process_display;
	if(!super_class->compare) super_class->compare = Process_compare;
	ProcessClass *class = (ProcessClass *)super_class;
	if(!class->writeField) class->writeField = Process_writeField;
	if(!class->sendSignal) class->sendSignal = base_Process_sendSignal;
}

ProcessClass Process_class = {
   .super = {
      .extends = Class(Object),
      .inherit = Process_inherit,
      .display = Process_display,
      .compare = Process_compare
   },
   .writeField = Process_writeField,
   .sendSignal = base_Process_sendSignal
};

void Process_init(Process* this, struct Settings_* settings) {
   this->settings = settings;
   this->tag = false;
   this->showChildren = true;
   this->show = true;
   this->created = true;
   this->updated = false;
   this->argv0_length = -1;
   this->processor = -1;
   if (Process_getuid == -1) Process_getuid = getuid();
}

void Process_toggleTag(Process* this) {
   this->tag = !this->tag;
}

bool Process_setPriority(Process* this, int priority) {
   CRT_dropPrivileges();
   int old_prio = getpriority(PRIO_PROCESS, this->pid);
   int err = setpriority(PRIO_PROCESS, this->pid, priority);
   CRT_restorePrivileges();
   if (err == 0 && old_prio != getpriority(PRIO_PROCESS, this->pid)) {
      this->nice = priority;
   }
   return (err == 0);
}

bool Process_changePriorityBy(Process* this, int delta) {
   return Process_setPriority(this, this->nice + delta);
}

bool Process_sendSignal(const Process *this, int sgn) {
   CRT_dropPrivileges();
   int r = As_Process(this)->sendSignal(this, sgn);
   CRT_restorePrivileges();
   return r;
}

long Process_pidCompare(const void* v1, const void* v2) {
   const Process *p1 = v1;
   const Process *p2 = v2;
   return (p1->pid - p2->pid);
}

long Process_compare(const void* v1, const void* v2) {
   const Process *p1, *p2;
   const Settings *settings = ((const Process *)v1)->settings;
   if (settings->direction == 1) {
      p1 = v1;
      p2 = v2;
   } else {
      p2 = v1;
      p1 = v2;
   }
   switch (settings->sortKey) {
      case HTOP_PERCENT_CPU_FIELD:
         return (p2->percent_cpu > p1->percent_cpu ? 1 : -1);
      case HTOP_PERCENT_MEM_FIELD:
         return (p2->m_resident - p1->m_resident);
      case HTOP_NAME_FIELD:
         return settings->sort_strcmp(p1->name, p2->name);
      case HTOP_COMM_FIELD:
         return settings->sort_strcmp(p1->comm, p2->comm);
      case HTOP_MAJFLT_FIELD:
         return uintcmp(p2->majflt, p1->majflt);
      case HTOP_MINFLT_FIELD:
         return uintcmp(p2->minflt, p1->minflt);
      case HTOP_M_RESIDENT_FIELD:
         return (p2->m_resident - p1->m_resident);
      case HTOP_M_SIZE_FIELD:
         return (p2->m_size - p1->m_size);
      case HTOP_NICE_FIELD:
         return (p1->nice - p2->nice);
      case HTOP_NLWP_FIELD:
         return (p1->nlwp - p2->nlwp);
      case HTOP_PGRP_FIELD:
         return uintcmp(p1->pgrp, p2->pgrp);
      case HTOP_PID_FIELD:
         return (p1->pid - p2->pid);
      case HTOP_PPID_FIELD:
         return (p1->ppid - p2->ppid);
      case HTOP_PRIORITY_FIELD:
         return (p1->priority - p2->priority);
      case HTOP_PROCESSOR_FIELD:
         return (p1->processor - p2->processor);
      case HTOP_SESSION_FIELD:
         return uintcmp(p1->session, p2->session);
      case HTOP_STARTTIME_FIELD:
         return p1->starttime_ctime == p2->starttime_ctime ?
            p1->pid - p2->pid : p1->starttime_ctime - p2->starttime_ctime;
      case HTOP_STATE_FIELD:
         return (Process_sortState(p1->state) - Process_sortState(p2->state));
      case HTOP_REAL_UID_FIELD:
      compare_ruid:
         return uintcmp(p1->ruid, p2->ruid);
      case HTOP_EFFECTIVE_UID_FIELD:
      compare_euid:
         return uintcmp(p1->euid, p2->euid);
      case HTOP_TIME_FIELD:
         return uintcmp(p2->time, p1->time);
      case HTOP_TGID_FIELD:
         return (p1->tgid - p2->tgid);
      case HTOP_TPGID_FIELD:
         return (p1->tpgid - p2->tpgid);
      case HTOP_TTY_FIELD:
         return uintcmp(p1->tty_nr, p2->tty_nr);
      case HTOP_REAL_USER_FIELD:
         if(!p1->real_user && !p2->real_user) goto compare_ruid;
         return settings->sort_strcmp(p1->real_user ? p1->real_user : "", p2->real_user ? p2->real_user : "");
      case HTOP_EFFECTIVE_USER_FIELD:
         if(!p1->effective_user && !p2->effective_user) goto compare_euid;
         return settings->sort_strcmp(p1->effective_user ? p1->effective_user : "", p2->effective_user ? p2->effective_user : "");
      default:
         return (p1->pid - p2->pid);
   }
}
