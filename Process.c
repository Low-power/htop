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

#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <pwd.h>
#include <time.h>
#include <assert.h>
#include <math.h>
#ifdef MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#elif defined(MAJOR_IN_SYSMACROS) || \
   (defined(HAVE_SYS_SYSMACROS_H) && HAVE_SYS_SYSMACROS_H)
#include <sys/sysmacros.h>
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

#define PROCESS_FLAG_IO 0x0001

typedef enum ProcessFields {
   NULL_PROCESSFIELD = 0,
   PID = 1,
   NAME = 2,
   STATE = 3,
   PPID = 4,
   PGRP = 5,
   SESSION = 6,
   TTY_NR = 7,
   TPGID = 8,
   MINFLT = 10,
   MAJFLT = 12,
   PRIORITY = 18,
   NICE = 19,
   STARTTIME = 21,
   PROCESSOR = 38,
   M_SIZE = 39,
   M_RESIDENT = 40,
   EFFECTIVE_UID = 46,
   PERCENT_CPU = 47,
   PERCENT_MEM = 48,
   EFFECTIVE_USER = 49,
   TIME = 50,
   NLWP = 51,
   TGID = 52,
   REAL_UID = 53,
   REAL_USER = 54,
   COMM = 99
} ProcessField;

typedef struct ProcessPidColumn_ {
   int id;
   char* label;
} ProcessPidColumn;

typedef struct Process_ {
   Object super;

   struct Settings_* settings;

   unsigned long long int time;
   pid_t pid;
   pid_t ppid;
   pid_t tgid;
   char *name;
   char* comm;
   int commLen;
   int indent;

   int basenameOffset;
   bool updated;

   char state;
   bool tag;
   bool showChildren;
   bool show;
   unsigned int pgrp;
   unsigned int session;
   unsigned int tty_nr;
   int tpgid;
   uid_t ruid;
   uid_t euid;
   int processor;

   float percent_cpu;
   float percent_mem;

   char *real_user;
   char *effective_user;

   long int priority;
   long int nice;
   long int nlwp;
   char starttime_show[8];
   time_t starttime_ctime;

   long m_size;
   long m_resident;

   int exit_signal;

   unsigned long int minflt;
   unsigned long int majflt;
   #ifdef DEBUG
   long int itrealvalue;
   unsigned long int vsize;
   long int rss;
   unsigned long int rlim;
   unsigned long int startcode;
   unsigned long int endcode;
   unsigned long int startstack;
   unsigned long int kstkesp;
   unsigned long int kstkeip;
   unsigned long int signal;
   unsigned long int blocked;
   unsigned long int sigignore;
   unsigned long int sigcatch;
   unsigned long int wchan;
   unsigned long int nswap;
   unsigned long int cnswap;
   #endif

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
void Process_delete(Object* cast);
bool Process_isKernelProcess(const Process *);
bool Process_isExtraThreadProcess(const Process *);
extern ProcessFieldData Process_fields[];
extern ProcessPidColumn Process_pidColumns[];
extern char Process_pidFormat[20];

typedef Process*(*Process_New)(struct Settings_*);
typedef void (*Process_WriteField)(Process*, RichString*, ProcessField);

typedef struct ProcessClass_ {
   const ObjectClass super;
   const Process_WriteField writeField;
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
   char buffer[22];

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
   } else if (number > 10000000000) {
      xSnprintf(buffer, sizeof buffer, "%11llu ", number / 1000);
      RichString_appendn(str, largeNumberColor, buffer, 5);
      RichString_appendn(str, processMegabytesColor, buffer+5, 3);
      RichString_appendn(str, processColor, buffer+8, 4);
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
      for (i = 0; i < this->basenameOffset; i++) {
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
         finish = this->basenameOffset - basename;
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
   if (rate == -1) {
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

void Process_writeField(Process* this, RichString* str, ProcessField field) {
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[HTOP_DEFAULT_COLOR];
   int baseattr = CRT_colors[HTOP_PROCESS_BASENAME_COLOR];
   int n = sizeof(buffer);
   bool coloring = this->settings->highlightMegabytes;

   switch (field) {
   case PERCENT_CPU:
      if (this->percent_cpu > 999.9) {
         xSnprintf(buffer, n, "%4u ", (unsigned int)this->percent_cpu);
      } else if (this->percent_cpu > 99.9) {
         xSnprintf(buffer, n, "%3u. ", (unsigned int)this->percent_cpu);
      } else {
         xSnprintf(buffer, n, "%4.1f ", this->percent_cpu);
      }
      break;
   case PERCENT_MEM:
      if (this->percent_mem > 99.9) {
         xSnprintf(buffer, n, "100. ");
      } else {
         xSnprintf(buffer, n, "%4.1f ", this->percent_mem);
      }
      break;
   case NAME:
      xSnprintf(buffer, n, "%-15s ", this->name);
      if(buffer[16]) {
         buffer[15] = ' ';
         buffer[16] = 0;
      }
      break;
   case COMM:
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

         for (int i = 0; i < 32; i++)
            if (indent & (1U << i))
               maxIndent = i+1;
          for (int i = 0; i < maxIndent - 1; i++) {
            int written, ret;
            if (indent & (1 << i))
               ret = snprintf(buf, n, "%s  ", CRT_treeStr[TREE_STR_VERT]);
            else
               ret = snprintf(buf, n, "   ");
            if (ret < 0 || ret >= n) {
               written = n;
            } else {
               written = ret;
            }
            buf += written;
            n -= written;
         }
         const char* draw = CRT_treeStr[lastItem ? (this->settings->direction == 1 ? TREE_STR_BEND : TREE_STR_TEND) : TREE_STR_RTEE];
         xSnprintf(buf, n, "%s%s ", draw, this->showChildren ? CRT_treeStr[TREE_STR_SHUT] : CRT_treeStr[TREE_STR_OPEN] );
         RichString_append(str, CRT_colors[HTOP_PROCESS_TREE_COLOR], buffer);
         Process_writeCommand(this, attr, baseattr, str);
         return;
      }
   case MAJFLT:
      Process_colorNumber(str, this->majflt, coloring);
      return;
   case MINFLT:
      Process_colorNumber(str, this->minflt, coloring);
      return;
   case M_RESIDENT:
      Process_humanNumber(str, this->m_resident * CRT_page_size_kib, coloring);
      return;
   case M_SIZE:
      Process_humanNumber(str, this->m_size * CRT_page_size_kib, coloring);
      return;
   case NICE:
      n = snprintf(buffer, n, "%3ld", this->nice);
      assert(n >= 3);
      if(n == 3) {
         buffer[3] = ' ';
         buffer[4] = 0;
      }
      if(this->nice < 0) attr = CRT_colors[HTOP_PROCESS_HIGH_PRIORITY_COLOR];
      else if(this->nice > 0) attr = CRT_colors[HTOP_PROCESS_LOW_PRIORITY_COLOR];
      break;
   case NLWP:
      xSnprintf(buffer, n, "%4ld ", this->nlwp);
      break;
   case PGRP:
      xSnprintf(buffer, n, Process_pidFormat, this->pgrp);
      break;
   case PID:
      xSnprintf(buffer, n, Process_pidFormat, this->pid);
      break;
   case PPID:
      xSnprintf(buffer, n, Process_pidFormat, this->ppid);
      break;
   case PRIORITY:
      if(this->priority <= -100) xSnprintf(buffer, n, " RT ");
      else xSnprintf(buffer, n, "%3ld ", this->priority);
      break;
   case PROCESSOR:
      xSnprintf(buffer, n, "%3d ", Settings_cpuId(this->settings, this->processor));
      break;
   case SESSION:
      xSnprintf(buffer, n, Process_pidFormat, this->session);
      break;
   case STARTTIME:
      xSnprintf(buffer, n, "%s", this->starttime_show);
      break;
   case STATE:
      xSnprintf(buffer, n, "%c ", this->state);
      switch(this->state) {
          case 'R':
              attr = CRT_colors[HTOP_PROCESS_R_STATE_COLOR];
              break;
          case 'D':
              attr = CRT_colors[HTOP_PROCESS_D_STATE_COLOR];
              break;
      }
      break;
   case REAL_UID:
      xSnprintf(buffer, n, "%6d ", (int)this->ruid);
      break;
   case EFFECTIVE_UID:
      xSnprintf(buffer, n, "%6d ", (int)this->euid);
      break;
   case TIME:
      Process_printTime(str, this->time);
      return;
   case TGID:
      xSnprintf(buffer, n, Process_pidFormat, (int)this->tgid);
      break;
   case TPGID:
      xSnprintf(buffer, n, Process_pidFormat, (int)this->tpgid);
      break;
   case TTY_NR:
      xSnprintf(buffer, n, "%3u:%3u ", (unsigned int)major(this->tty_nr), (unsigned int)minor(this->tty_nr));
      break;
   case REAL_USER:
      if (Process_getuid != (int)this->ruid) attr = CRT_colors[HTOP_PROCESS_SHADOW_COLOR];
      if (this->real_user) {
         xSnprintf(buffer, n, "%-9s ", this->real_user);
      } else {
         xSnprintf(buffer, n, "%-9d ", (int)this->ruid);
      }
      goto user_end;
   case EFFECTIVE_USER:
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
   }
   assert(out->chlen > 0);
}

void Process_done(Process* this) {
   assert (this != NULL);
   free(this->name);
   free(this->comm);
}

ProcessClass Process_class = {
   .super = {
      .extends = Class(Object),
      .display = Process_display,
      .delete = Process_delete,
      .compare = Process_compare
   },
   .writeField = Process_writeField,
};

void Process_init(Process* this, struct Settings_* settings) {
   this->settings = settings;
   this->tag = false;
   this->showChildren = true;
   this->show = true;
   this->updated = false;
   this->basenameOffset = -1;
   if (Process_getuid == -1) Process_getuid = getuid();
}

void Process_toggleTag(Process* this) {
   this->tag = this->tag == true ? false : true;
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

void Process_sendSignal(Process* this, int sgn) {
   CRT_dropPrivileges();
   kill(this->pid, sgn);
   CRT_restorePrivileges();
}

long Process_pidCompare(const void* v1, const void* v2) {
   const Process *p1 = v1;
   const Process *p2 = v2;
   return (p1->pid - p2->pid);
}

long Process_compare(const void* v1, const void* v2) {
   const Process *p1, *p2;
   Settings *settings = ((const Process *)v1)->settings;
   if (settings->direction == 1) {
      p1 = v1;
      p2 = v2;
   } else {
      p2 = v1;
      p1 = v2;
   }
   switch (settings->sortKey) {
   case PERCENT_CPU:
      return (p2->percent_cpu > p1->percent_cpu ? 1 : -1);
   case PERCENT_MEM:
      return (p2->m_resident - p1->m_resident);
   case NAME:
      return strcmp(p1->name, p2->name);
   case COMM:
      return strcmp(p1->comm, p2->comm);
   case MAJFLT:
      return (p2->majflt - p1->majflt);
   case MINFLT:
      return (p2->minflt - p1->minflt);
   case M_RESIDENT:
      return (p2->m_resident - p1->m_resident);
   case M_SIZE:
      return (p2->m_size - p1->m_size);
   case NICE:
      return (p1->nice - p2->nice);
   case NLWP:
      return (p1->nlwp - p2->nlwp);
   case PGRP:
      return (p1->pgrp - p2->pgrp);
   case PID:
      return (p1->pid - p2->pid);
   case PPID:
      return (p1->ppid - p2->ppid);
   case PRIORITY:
      return (p1->priority - p2->priority);
   case PROCESSOR:
      return (p1->processor - p2->processor);
   case SESSION:
      return (p1->session - p2->session);
   case STARTTIME:
      return p1->starttime_ctime == p2->starttime_ctime ?
         p1->pid - p2->pid : p1->starttime_ctime - p2->starttime_ctime;
   case STATE:
      return (Process_sortState(p1->state) - Process_sortState(p2->state));
   case REAL_UID:
      return (p1->ruid - p2->ruid);
   case EFFECTIVE_UID:
      return (p1->euid - p2->euid);
   case TIME:
      return p2->time - p1->time;
   case TGID:
      return (p1->tgid - p2->tgid);
   case TPGID:
      return (p1->tpgid - p2->tpgid);
   case TTY_NR:
      return (p1->tty_nr - p2->tty_nr);
   case REAL_USER:
      if(!p1->real_user && !p2->real_user) return p1->ruid - p2->ruid;
      return strcmp(p1->real_user ? p1->real_user : "", p2->real_user ? p2->real_user : "");
   case EFFECTIVE_USER:
      if(!p1->effective_user && !p2->effective_user) return p1->euid - p2->euid;
      return strcmp(p1->effective_user ? p1->effective_user : "", p2->effective_user ? p2->effective_user : "");
   default:
      return (p1->pid - p2->pid);
   }
}
