/*
htop - linux/LinuxProcess.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "ProcessList.h"
#include "LinuxProcess.h"
#include "Platform.h"
#include "CRT.h"
#include "StringUtils.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif
#include <time.h>
#include <errno.h>

/*{

#define PROCESS_FLAG_LINUX_IOPRIO   0x0100
#define PROCESS_FLAG_LINUX_OPENVZ   0x0200
#define PROCESS_FLAG_LINUX_VSERVER  0x0400
#define PROCESS_FLAG_LINUX_CGROUP   0x0800
#define PROCESS_FLAG_LINUX_OOM      0x1000

typedef enum {
   HTOP_FLAGS_FIELD = 9,
   HTOP_ITREALVALUE_FIELD = 20,
   HTOP_VSIZE_FIELD = 22,
   HTOP_RSS_FIELD = 23,
   HTOP_RLIM_FIELD = 24,
   HTOP_STARTCODE_FIELD = 25,
   HTOP_ENDCODE_FIELD = 26,
   HTOP_STARTSTACK_FIELD = 27,
   HTOP_KSTKESP_FIELD = 28,
   HTOP_KSTKEIP_FIELD = 29,
   HTOP_SIGNAL_FIELD = 30,
   HTOP_BLOCKED_FIELD = 31,
   HTOP_SSIGIGNORE_FIELD = 32,
   HTOP_SIGCATCH_FIELD = 33,
   HTOP_WCHAN_FIELD = 34,
   HTOP_NSWAP_FIELD = 35,
   HTOP_CNSWAP_FIELD = 36,
   HTOP_EXIT_SIGNAL_FIELD = 37,
} UnsupportedProcessField;

typedef enum {
   HTOP_CMINFLT_FIELD = 11,
   HTOP_CMAJFLT_FIELD = 13,
   HTOP_UTIME_FIELD = 14,
   HTOP_STIME_FIELD = 15,
   HTOP_CUTIME_FIELD = 16,
   HTOP_CSTIME_FIELD = 17,
   HTOP_M_SHARE_FIELD = 41,
   HTOP_M_TRS_FIELD = 42,
   HTOP_M_DRS_FIELD = 43,
   HTOP_M_LRS_FIELD = 44,
   HTOP_M_DT_FIELD = 45,
   #ifdef HAVE_OPENVZ
   HTOP_CTID_FIELD = 100,
   HTOP_VPID_FIELD = 101,
   #endif
   #ifdef HAVE_VSERVER
   HTOP_VXID_FIELD = 102,
   #endif
   #ifdef HAVE_TASKSTATS
   HTOP_RCHAR_FIELD = 103,
   HTOP_WCHAR_FIELD = 104,
   HTOP_SYSCR_FIELD = 105,
   HTOP_SYSCW_FIELD = 106,
   HTOP_RBYTES_FIELD = 107,
   HTOP_WBYTES_FIELD = 108,
   HTOP_CNCLWB_FIELD = 109,
   HTOP_IO_READ_RATE_FIELD = 110,
   HTOP_IO_WRITE_RATE_FIELD = 111,
   HTOP_IO_RATE_FIELD = 112,
   #endif
   #ifdef HAVE_CGROUP
   HTOP_CGROUP_FIELD = 113,
   #endif
   HTOP_OOM_FIELD = 114,
   HTOP_IO_PRIORITY_FIELD = 115,
   #ifdef HAVE_DELAYACCT
   HTOP_PERCENT_CPU_DELAY_FIELD = 116,
   HTOP_PERCENT_IO_DELAY_FIELD = 117,
   HTOP_PERCENT_SWAP_DELAY_FIELD = 118,
   #endif
   HTOP_LAST_PROCESSFIELD = 119,
} LinuxProcessField;

#include "IOPriority.h"

typedef struct LinuxProcess_ {
   Process super;
   bool is_kernel_process;
   IOPriority ioPriority;
   unsigned long int cminflt;
   unsigned long int cmajflt;
   unsigned long long int utime;
   unsigned long long int stime;
   unsigned long long int cutime;
   unsigned long long int cstime;
   long m_share;
   long m_trs;
   long m_drs;
   long m_lrs;
   long m_dt;
   unsigned long long starttime;
   #ifdef HAVE_TASKSTATS
   unsigned long long io_rchar;
   unsigned long long io_wchar;
   unsigned long long io_syscr;
   unsigned long long io_syscw;
   unsigned long long io_read_bytes;
   unsigned long long io_write_bytes;
   unsigned long long io_cancelled_write_bytes;
   unsigned long long io_rate_read_time;
   unsigned long long io_rate_write_time;
   double io_rate_read_bps;
   double io_rate_write_bps;
   #endif
   #ifdef HAVE_OPENVZ
   unsigned int ctid;
   unsigned int vpid;
   #endif
   #ifdef HAVE_VSERVER
   unsigned int vxid;
   #endif
   #ifdef HAVE_CGROUP
   char* cgroup;
   #endif
   unsigned int oom;
   char* ttyDevice;
   #ifdef HAVE_DELAYACCT
   unsigned long long int delay_read_time;
   unsigned long long cpu_delay_total;
   unsigned long long blkio_delay_total;
   unsigned long long swapin_delay_total;
   float cpu_delay_percent;
   float blkio_delay_percent;
   float swapin_delay_percent;
   #endif
} LinuxProcess;

}*/

long long btime; /* semi-global */

ProcessFieldData Process_fields[] = {
   [0] = { .name = "", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_PID_FIELD] = { .name = "PID", .title = "    PID ", .description = "Process/thread ID", .flags = 0, },
   [HTOP_NAME_FIELD] = { .name = "NAME", .title = "NAME            ", .description = "Process (executable) name", .flags = 0, },
   [HTOP_COMM_FIELD] = { .name = "Command", .title = "Command ", .description = "Command line", .flags = 0, },
   [HTOP_STATE_FIELD] = { .name = "STATE", .title = "S ", .description = "Process state (R running, S sleeping, D uninterruptible sleeping, T stoppd, Z zombie, I idle kernel task, W paging)", .flags = 0 },
   [HTOP_PPID_FIELD] = { .name = "PPID", .title = "   PPID ", .description = "Parent process ID", .flags = 0, },
   [HTOP_PGRP_FIELD] = { .name = "PGRP", .title = "   PGRP ", .description = "Process group ID", .flags = 0, },
   [HTOP_SESSION_FIELD] = { .name = "SESSION", .title = "    SID ", .description = "Process's session ID", .flags = 0, },
   [HTOP_TTY_FIELD] = { .name = "TTY", .title = "TTY      ", .description = "Controlling terminal", .flags = 0, },
   [HTOP_TPGID_FIELD] = { .name = "TPGID", .title = "  TPGID ", .description = "Process ID of the fg process group of the controlling terminal", .flags = 0, },
   [HTOP_FLAGS_FIELD] = { .name = "FLAGS", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_MINFLT_FIELD] = { .name = "MINFLT", .title = "     MINFLT ", .description = "Number of minor faults which have not required loading a memory page from disk", .flags = 0, },
   [HTOP_CMINFLT_FIELD] = { .name = "CMINFLT", .title = "    CMINFLT ", .description = "Children processes' minor faults", .flags = 0, },
   [HTOP_MAJFLT_FIELD] = { .name = "MAJFLT", .title = "     MAJFLT ", .description = "Number of major faults which have required loading a memory page from disk", .flags = 0, },
   [HTOP_CMAJFLT_FIELD] = { .name = "CMAJFLT", .title = "    CMAJFLT ", .description = "Children processes' major faults", .flags = 0, },
   [HTOP_UTIME_FIELD] = { .name = "UTIME", .title = " UTIME+  ", .description = "User CPU time - time the process spent executing in user mode", .flags = 0, },
   [HTOP_STIME_FIELD] = { .name = "STIME", .title = " STIME+  ", .description = "System CPU time - time the kernel spent running system calls for this process", .flags = 0, },
   [HTOP_CUTIME_FIELD] = { .name = "CUTIME", .title = " CUTIME+ ", .description = "Children processes' user CPU time", .flags = 0, },
   [HTOP_CSTIME_FIELD] = { .name = "CSTIME", .title = " CSTIME+ ", .description = "Children processes' system CPU time", .flags = 0, },
   [HTOP_PRIORITY_FIELD] = { .name = "PRIORITY", .title = "PRI ", .description = "Kernel's internal priority for the process", .flags = 0, },
   [HTOP_NICE_FIELD] = { .name = "NICE", .title = " NI ", .description = "Nice value (the higher the value, the more it lets other processes take priority)", .flags = 0, },
   [HTOP_ITREALVALUE_FIELD] = { .name = "ITREALVALUE", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_STARTTIME_FIELD] = { .name = "STARTTIME", .title = "START ", .description = "Time the process was started", .flags = 0, },
   [HTOP_VSIZE_FIELD] = { .name = "VSIZE", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_RSS_FIELD] = { .name = "RSS", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_RLIM_FIELD] = { .name = "RLIM", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_STARTCODE_FIELD] = { .name = "STARTCODE", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_ENDCODE_FIELD] = { .name = "ENDCODE", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_STARTSTACK_FIELD] = { .name = "STARTSTACK", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_KSTKESP_FIELD] = { .name = "KSTKESP", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_KSTKEIP_FIELD] = { .name = "KSTKEIP", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_SIGNAL_FIELD] = { .name = "SIGNAL", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_BLOCKED_FIELD] = { .name = "BLOCKED", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_SSIGIGNORE_FIELD] = { .name = "SIGIGNORE", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_SIGCATCH_FIELD] = { .name = "SIGCATCH", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_WCHAN_FIELD] = { .name = "WCHAN", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_NSWAP_FIELD] = { .name = "NSWAP", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_CNSWAP_FIELD] = { .name = "CNSWAP", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_EXIT_SIGNAL_FIELD] = { .name = "EXIT_SIGNAL", .title = NULL, .description = NULL, .flags = 0, },
   [HTOP_PROCESSOR_FIELD] = { .name = "PROCESSOR", .title = "CPU ", .description = "Id of the CPU the process last executed on", .flags = 0, },
   [HTOP_M_SIZE_FIELD] = { .name = "M_SIZE", .title = " VIRT ", .description = "Total program size in virtual memory", .flags = 0, },
   [HTOP_M_RESIDENT_FIELD] = { .name = "M_RESIDENT", .title = "  RES ", .description = "Resident set size, size of the text and data sections, plus stack usage", .flags = 0, },
   [HTOP_M_SHARE_FIELD] = { .name = "M_SHARE", .title = "  SHR ", .description = "Size of the process's shared pages", .flags = 0, },
   [HTOP_M_TRS_FIELD] = { .name = "M_TRS", .title = " CODE ", .description = "Size of the text segment of the process", .flags = 0, },
   [HTOP_M_DRS_FIELD] = { .name = "M_DRS", .title = " DATA ", .description = "Size of the data segment plus stack usage of the process", .flags = 0, },
   [HTOP_M_LRS_FIELD] = { .name = "M_LRS", .title = " LIB ", .description = "The library size of the process", .flags = 0, },
   [HTOP_M_DT_FIELD] = { .name = "M_DT", .title = " DIRTY ", .description = "Size of the dirty pages of the process", .flags = 0, },
   [HTOP_REAL_UID_FIELD] = { .name = "REAL_UID", .title = "  RUID ", .description = "Real user ID", .flags = 0, },
   [HTOP_EFFECTIVE_UID_FIELD] = { .name = "EFFECTIVE_UID", .title = "  EUID ", .description = "Effective user ID", .flags = 0, },
   [HTOP_PERCENT_CPU_FIELD] = { .name = "PERCENT_CPU", .title = "CPU% ", .description = "Percentage of the CPU time the process used in the last sampling", .flags = 0, },
   [HTOP_PERCENT_MEM_FIELD] = { .name = "PERCENT_MEM", .title = "MEM% ", .description = "Percentage of the memory the process is using, based on resident memory size", .flags = 0, },
   [HTOP_REAL_USER_FIELD] = { .name = "REAL_USER", .title = "REAL_USER ", .description = "Real user (or numeric user ID if name cannot be determined)", .flags = 0, },
   [HTOP_EFFECTIVE_USER_FIELD] = { .name = "EFFECTIVE_USER", .title = "EFFE_USER ", .description = "Effective user (or numeric user ID if name cannot be determined)", .flags = 0, },
   [HTOP_TIME_FIELD] = { .name = "TIME", .title = "  TIME+  ", .description = "Total time the process has spent in user and system time", .flags = 0, },
   [HTOP_NLWP_FIELD] = { .name = "NLWP", .title = "NLWP ", .description = "Number of threads in the process", .flags = 0, },
   [HTOP_TGID_FIELD] = { .name = "TGID", .title = "   TGID ", .description = "Thread group ID (i.e. process ID)", .flags = 0, },
#ifdef HAVE_OPENVZ
   [HTOP_CTID_FIELD] = { .name = "CTID", .title = "   CTID ", .description = "OpenVZ container ID (a.k.a. virtual environment ID)", .flags = PROCESS_FLAG_LINUX_OPENVZ, },
   [HTOP_VPID_FIELD] = { .name = "VPID", .title = " VPID ", .description = "OpenVZ process ID", .flags = PROCESS_FLAG_LINUX_OPENVZ, },
#endif
#ifdef HAVE_VSERVER
   [HTOP_VXID_FIELD] = { .name = "VXID", .title = " VXID ", .description = "VServer process ID", .flags = PROCESS_FLAG_LINUX_VSERVER, },
#endif
#ifdef HAVE_TASKSTATS
   [HTOP_RCHAR_FIELD] = { .name = "RCHAR", .title = "    RD_CHAR ", .description = "Number of bytes the process has read", .flags = PROCESS_FLAG_IO, },
   [HTOP_WCHAR_FIELD] = { .name = "WCHAR", .title = "    WR_CHAR ", .description = "Number of bytes the process has written", .flags = PROCESS_FLAG_IO, },
   [HTOP_SYSCR_FIELD] = { .name = "SYSCR", .title = "    RD_SYSC ", .description = "Number of read(2) syscalls for the process", .flags = PROCESS_FLAG_IO, },
   [HTOP_SYSCW_FIELD] = { .name = "SYSCW", .title = "    WR_SYSC ", .description = "Number of write(2) syscalls for the process", .flags = PROCESS_FLAG_IO, },
   [HTOP_RBYTES_FIELD] = { .name = "RBYTES", .title = "  IO_RBYTES ", .description = "Bytes of read(2) I/O for the process", .flags = PROCESS_FLAG_IO, },
   [HTOP_WBYTES_FIELD] = { .name = "WBYTES", .title = "  IO_WBYTES ", .description = "Bytes of write(2) I/O for the process", .flags = PROCESS_FLAG_IO, },
   [HTOP_CNCLWB_FIELD] = { .name = "CNCLWB", .title = "  IO_CANCEL ", .description = "Bytes of cancelled write(2) I/O", .flags = PROCESS_FLAG_IO, },
   [HTOP_IO_READ_RATE_FIELD] = { .name = "IO_READ_RATE", .title = "  DISK READ ", .description = "The I/O rate of read(2) in bytes per second for the process", .flags = PROCESS_FLAG_IO, },
   [HTOP_IO_WRITE_RATE_FIELD] = { .name = "IO_WRITE_RATE", .title = " DISK WRITE ", .description = "The I/O rate of write(2) in bytes per second for the process", .flags = PROCESS_FLAG_IO, },
   [HTOP_IO_RATE_FIELD] = { .name = "IO_RATE", .title = "   DISK R/W ", .description = "Total I/O rate in bytes per second", .flags = PROCESS_FLAG_IO, },
#endif
#ifdef HAVE_CGROUP
   [HTOP_CGROUP_FIELD] = { .name = "CGROUP", .title = "    CGROUP ", .description = "Which cgroup the process is in", .flags = PROCESS_FLAG_LINUX_CGROUP, },
#endif
   [HTOP_OOM_FIELD] = { .name = "OOM", .title = "  OOM ", .description = "OOM (Out-of-Memory) killer score", .flags = PROCESS_FLAG_LINUX_OOM, },
   [HTOP_IO_PRIORITY_FIELD] = { .name = "IO_PRIORITY", .title = "IO ", .description = "I/O priority", .flags = PROCESS_FLAG_LINUX_IOPRIO, },
#ifdef HAVE_DELAYACCT
   [HTOP_PERCENT_CPU_DELAY_FIELD] = { .name = "PERCENT_CPU_DELAY", .title = "CPUD% ", .description = "CPU delay %", .flags = 0, },
   [HTOP_PERCENT_IO_DELAY_FIELD] = { .name = "PERCENT_IO_DELAY", .title = "IOD% ", .description = "Block I/O delay %", .flags = 0, },
   [HTOP_PERCENT_SWAP_DELAY_FIELD] = { .name = "PERCENT_SWAP_DELAY", .title = "SWAPD% ", .description = "Swapin delay %", .flags = 0, },
#endif
   [HTOP_LAST_PROCESSFIELD] = { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0, },
};

ProcessPidColumn Process_pidColumns[] = {
   { .id = HTOP_PID_FIELD, .label = "PID" },
   { .id = HTOP_PPID_FIELD, .label = "PPID" },
   #ifdef HAVE_OPENVZ
   { .id = HTOP_VPID_FIELD, .label = "VPID" },
   #endif
   { .id = HTOP_TPGID_FIELD, .label = "TPGID" },
   { .id = HTOP_TGID_FIELD, .label = "TGID" },
   { .id = HTOP_PGRP_FIELD, .label = "PGRP" },
   { .id = HTOP_SESSION_FIELD, .label = "SID" },
   { .id = 0, .label = NULL }
};

ProcessClass LinuxProcess_class = {
   .super = {
      .extends = Class(Process),
      .display = Process_display,
      .delete = Process_delete,
      .compare = LinuxProcess_compare
   },
   .writeField = LinuxProcess_writeField,
};

LinuxProcess* LinuxProcess_new(Settings* settings) {
   LinuxProcess* this = xCalloc(1, sizeof(LinuxProcess));
   Object_setClass(this, Class(LinuxProcess));
   Process_init(&this->super, settings);
   return this;
}

void Process_delete(Object* cast) {
   LinuxProcess* this = (LinuxProcess*) cast;
   Process_done((Process*)cast);
#ifdef HAVE_CGROUP
   free(this->cgroup);
#endif
   free(this->ttyDevice);
   free(this);
}

/*
[1] Note that before kernel 2.6.26 a process that has not asked for
an io priority formally uses "none" as scheduling class, but the
io scheduler will treat such processes as if it were in the best
effort class. The priority within the best effort class will  be
dynamically  derived  from  the  cpu  nice level of the process:
io_priority = (cpu_nice + 20) / 5. -- From ionice(1) man page
*/
#define LinuxProcess_effectiveIOPriority(p_) (IOPriority_class(p_->ioPriority) == IOPRIO_CLASS_NONE ? IOPriority_tuple(IOPRIO_CLASS_BE, (p_->super.nice + 20) / 5) : p_->ioPriority)

IOPriority LinuxProcess_updateIOPriority(LinuxProcess* this) {
   IOPriority ioprio = 0;
// Other OSes masquerading as Linux (NetBSD?) don't have this syscall
#ifdef SYS_ioprio_get
   ioprio = syscall(SYS_ioprio_get, IOPRIO_WHO_PROCESS, this->super.pid);
#endif
   this->ioPriority = ioprio;
   return ioprio;
}

bool LinuxProcess_setIOPriority(LinuxProcess* this, IOPriority ioprio) {
// Other OSes masquerading as Linux (NetBSD?) don't have this syscall
#ifdef SYS_ioprio_set
   syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, this->super.pid, ioprio);
#endif
   return (LinuxProcess_updateIOPriority(this) == ioprio);
}

#ifdef HAVE_DELAYACCT
void LinuxProcess_printDelay(float delay_percent, char* buffer, int n) {
  if (delay_percent < 0) {
    xSnprintf(buffer, n, " N/A  ");
  } else {
    xSnprintf(buffer, n, "%4.1f  ", delay_percent);
  }
}
#endif

void LinuxProcess_writeField(const Process *this, RichString* str, ProcessField field) {
   const LinuxProcess *lp = (const LinuxProcess *)this;
   bool coloring = this->settings->highlightMegabytes;
   char buffer[256]; buffer[255] = '\0';
   int attr = CRT_colors[HTOP_DEFAULT_COLOR];
   int n = sizeof(buffer);
   switch ((int)field) {
         struct tm tm;
         time_t start_wall_time;
         double total_rate;
      case HTOP_TTY_FIELD:
         if (lp->ttyDevice) {
            xSnprintf(buffer, n, "%-9s", lp->ttyDevice + 5 /* skip "/dev/" */);
         } else {
            attr = CRT_colors[HTOP_PROCESS_SHADOW_COLOR];
            xSnprintf(buffer, n, "?        ");
         }
         break;
      case HTOP_CMINFLT_FIELD: 
         Process_colorNumber(str, lp->cminflt, coloring);
         return;
      case HTOP_CMAJFLT_FIELD:
         Process_colorNumber(str, lp->cmajflt, coloring);
         return;
      case HTOP_M_DRS_FIELD:
         Process_humanNumber(str, lp->m_drs * CRT_page_size_kib, coloring);
         return;
      case HTOP_M_DT_FIELD:
         Process_humanNumber(str, lp->m_dt * CRT_page_size_kib, coloring);
         return;
      case HTOP_M_LRS_FIELD:
         Process_humanNumber(str, lp->m_lrs * CRT_page_size_kib, coloring);
         return;
      case HTOP_M_TRS_FIELD:
         Process_humanNumber(str, lp->m_trs * CRT_page_size_kib, coloring);
         return;
      case HTOP_M_SHARE_FIELD:
         Process_humanNumber(str, lp->m_share * CRT_page_size_kib, coloring);
         return;
      case HTOP_UTIME_FIELD:
         Process_printTime(str, lp->utime);
         return;
      case HTOP_STIME_FIELD:
         Process_printTime(str, lp->stime);
         return;
      case HTOP_CUTIME_FIELD:
         Process_printTime(str, lp->cutime);
         return;
      case HTOP_CSTIME_FIELD:
         Process_printTime(str, lp->cstime);
         return;
      case HTOP_STARTTIME_FIELD:
         start_wall_time = btime + (lp->starttime / sysconf(_SC_CLK_TCK));
         localtime_r(&start_wall_time, &tm);
         strftime(buffer, n, (start_wall_time > time(NULL) - 86400) ? "%R " : "%b%d ", &tm);
         break;
   #ifdef HAVE_TASKSTATS
      case HTOP_RCHAR_FIELD:
         Process_colorNumber(str, lp->io_rchar, coloring);
         return;
      case HTOP_WCHAR_FIELD:
         Process_colorNumber(str, lp->io_wchar, coloring);
         return;
      case HTOP_SYSCR_FIELD:
         Process_colorNumber(str, lp->io_syscr, coloring);
         return;
      case HTOP_SYSCW_FIELD:
         Process_colorNumber(str, lp->io_syscw, coloring);
         return;
      case HTOP_RBYTES_FIELD:
         Process_colorNumber(str, lp->io_read_bytes, coloring);
         return;
      case HTOP_WBYTES_FIELD:
         Process_colorNumber(str, lp->io_write_bytes, coloring);
         return;
      case HTOP_CNCLWB_FIELD:
         Process_colorNumber(str, lp->io_cancelled_write_bytes, coloring);
         return;
      case HTOP_IO_READ_RATE_FIELD:
         Process_outputRate(str, buffer, n, lp->io_rate_read_bps, coloring);
         return;
      case HTOP_IO_WRITE_RATE_FIELD:
         Process_outputRate(str, buffer, n, lp->io_rate_write_bps, coloring);
         return;
      case HTOP_IO_RATE_FIELD:
         total_rate = (lp->io_rate_read_bps >= 0) ? lp->io_rate_read_bps + lp->io_rate_write_bps : -1;
         Process_outputRate(str, buffer, n, total_rate, coloring);
         return;
   #endif
   #ifdef HAVE_OPENVZ
      case HTOP_CTID_FIELD:
         xSnprintf(buffer, n, "%7u ", lp->ctid);
         break;
      case HTOP_VPID_FIELD:
         xSnprintf(buffer, n, Process_pidFormat, lp->vpid);
         break;
   #endif
   #ifdef HAVE_VSERVER
      case HTOP_VXID_FIELD:
         xSnprintf(buffer, n, "%5u ", lp->vxid);
         break;
   #endif
   #ifdef HAVE_CGROUP
      case HTOP_CGROUP_FIELD:
         xSnprintf(buffer, n, "%-10s ", lp->cgroup);
         break;
   #endif
      case HTOP_OOM_FIELD:
         xSnprintf(buffer, n, "%5u ", lp->oom);
         break;
      case HTOP_IO_PRIORITY_FIELD:
         switch(IOPriority_class(lp->ioPriority)) {
            case IOPRIO_CLASS_NONE:
               // see note [1] above
               xSnprintf(buffer, n, "B%1d ", (int) (this->nice + 20) / 5);
               break;
            case IOPRIO_CLASS_BE:
               xSnprintf(buffer, n, "B%1d ", IOPriority_data(lp->ioPriority));
               break;
            case IOPRIO_CLASS_RT:
               attr = CRT_colors[HTOP_PROCESS_HIGH_PRIORITY_COLOR];
               xSnprintf(buffer, n, "R%1d ", IOPriority_data(lp->ioPriority));
               break;
            case IOPRIO_CLASS_IDLE:
               attr = CRT_colors[HTOP_PROCESS_LOW_PRIORITY_COLOR];
               xSnprintf(buffer, n, "id ");
               break;
            default:
               xSnprintf(buffer, n, "?? ");
               break;
         }
         break;
   #ifdef HAVE_DELAYACCT
      case HTOP_PERCENT_CPU_DELAY_FIELD:
         LinuxProcess_printDelay(lp->cpu_delay_percent, buffer, n);
         break;
      case HTOP_PERCENT_IO_DELAY_FIELD:
         LinuxProcess_printDelay(lp->blkio_delay_percent, buffer, n);
         break;
      case HTOP_PERCENT_SWAP_DELAY_FIELD:
         LinuxProcess_printDelay(lp->swapin_delay_percent, buffer, n);
         break;
   #endif
      default:
         Process_writeField(this, str, field);
         return;
   }
   RichString_append(str, attr, buffer);
}

long LinuxProcess_compare(const void* v1, const void* v2) {
   const LinuxProcess *p1, *p2;
   const Settings *settings = ((const Process *)v1)->settings;
   if (settings->direction == 1) {
      p1 = v1;
      p2 = v2;
   } else {
      p2 = v1;
      p1 = v2;
   }
   switch ((int)settings->sortKey) {
         long long int diff;
      case HTOP_M_DRS_FIELD:
         return (p2->m_drs - p1->m_drs);
      case HTOP_M_DT_FIELD:
         return (p2->m_dt - p1->m_dt);
      case HTOP_M_LRS_FIELD:
         return (p2->m_lrs - p1->m_lrs);
      case HTOP_M_TRS_FIELD:
         return (p2->m_trs - p1->m_trs);
      case HTOP_M_SHARE_FIELD:
         return (p2->m_share - p1->m_share);
      case HTOP_UTIME_FIELD:
         return uintcmp(p2->utime, p1->utime);
      case HTOP_CUTIME_FIELD:
         return uintcmp(p2->cutime, p1->cutime);
      case HTOP_STIME_FIELD:
         return uintcmp(p2->stime, p1->stime);
      case HTOP_CSTIME_FIELD:
         return uintcmp(p2->cstime, p1->cstime);
      case HTOP_STARTTIME_FIELD:
         return p1->starttime == p2->starttime ?
            p1->super.pid - p2->super.pid : uintcmp(p1->starttime, p2->starttime);
   #ifdef HAVE_TASKSTATS
      case HTOP_RCHAR_FIELD:
         return uintcmp(p2->io_rchar, p1->io_rchar);
      case HTOP_WCHAR_FIELD:
         return uintcmp(p2->io_wchar, p1->io_wchar);
      case HTOP_SYSCR_FIELD:
         return uintcmp(p2->io_syscr, p1->io_syscr);
      case HTOP_SYSCW_FIELD:
         return uintcmp(p2->io_syscw, p1->io_syscw);
      case HTOP_RBYTES_FIELD:
         return uintcmp(p2->io_read_bytes, p1->io_read_bytes);
      case HTOP_WBYTES_FIELD:
         return uintcmp(p2->io_write_bytes, p1->io_write_bytes);
      case HTOP_CNCLWB_FIELD:
         return uintcmp(p2->io_cancelled_write_bytes, p1->io_cancelled_write_bytes);
      case HTOP_IO_READ_RATE_FIELD:
         diff = p2->io_rate_read_bps - p1->io_rate_read_bps;
         goto test_diff;
      case HTOP_IO_WRITE_RATE_FIELD:
         diff = p2->io_rate_write_bps - p1->io_rate_write_bps;
         goto test_diff;
      case HTOP_IO_RATE_FIELD:
         diff = (p2->io_rate_read_bps + p2->io_rate_write_bps) - (p1->io_rate_read_bps + p1->io_rate_write_bps);
         goto test_diff;
   #endif
   #ifdef HAVE_OPENVZ
      case HTOP_CTID_FIELD:
         return uintcmp(p2->ctid, p1->ctid);
      case HTOP_VPID_FIELD:
         return uintcmp(p2->vpid, p1->vpid);
   #endif
   #ifdef HAVE_VSERVER
      case HTOP_VXID_FIELD:
         return uintcmp(p2->vxid, p1->vxid);
   #endif
   #ifdef HAVE_CGROUP
      case HTOP_CGROUP_FIELD:
         return settings->sort_strcmp(p1->cgroup ? p1->cgroup : "", p2->cgroup ? p2->cgroup : "");
   #endif
      case HTOP_OOM_FIELD:
         return uintcmp(p2->oom, p1->oom);
   #ifdef HAVE_DELAYACCT
      case HTOP_PERCENT_CPU_DELAY_FIELD:
         return (p2->cpu_delay_percent > p1->cpu_delay_percent ? 1 : -1);
      case HTOP_PERCENT_IO_DELAY_FIELD:
         return (p2->blkio_delay_percent > p1->blkio_delay_percent ? 1 : -1);
      case HTOP_PERCENT_SWAP_DELAY_FIELD:
         return (p2->swapin_delay_percent > p1->swapin_delay_percent ? 1 : -1);
   #endif
      case HTOP_IO_PRIORITY_FIELD:
         return LinuxProcess_effectiveIOPriority(p1) - LinuxProcess_effectiveIOPriority(p2);
      default:
         return Process_compare(v1, v2);
      test_diff:
         return (diff > 0) ? 1 : (diff < 0 ? -1 : 0);
   }
}

bool Process_isKernelProcess(const Process *this) {
	return ((const LinuxProcess *)this)->is_kernel_process;
}

bool Process_isExtraThreadProcess(const Process *this) {
	return this->pid != this->tgid;
}

char **Process_getKernelStackTrace(const Process *this) {
	char **v = xMalloc(2 * sizeof(char *));
	unsigned int i = 0;
	char path[sizeof PROCDIR + 17];
	xSnprintf(path, sizeof path, PROCDIR "/%d/stack", (int)this->pid);
	FILE *f = fopen(path, "r");
	if(!f) {
		v[0] = strdup(strerror(errno));
		if(v[0]) {
			v[1] = NULL;
		} else {
			free(v);
			v = NULL;
		}
		return v;
	}
	char *line;
	while((line = String_readLine(f))) {
		size_t len = strlen(line) + 1;
		v[i] = xMalloc(12 + len);
		int j = snprintf(v[i], 12 + len, "#%u ", i);
		assert(j > 0 && (size_t)j < 12 + len);
		if(j > 12) v[i] = xRealloc(v[i], j + len);
		memcpy(v[i] + j, line, len);
		free(line);
		v = xRealloc(v, (++i + 1) * sizeof(char *));
	}
	fclose(f);
	v[i] = NULL;
	return v;
}
