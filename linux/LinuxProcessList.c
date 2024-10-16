/*
htop - linux/LinuxProcessList.c
(C) 2014 Hisham H. Muhammad
Copyright 2015-2024 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "LinuxProcessList.h"
#include "LinuxProcess.h"
#include "CRT.h"
#include "StringUtils.h"
#include "IOUtils.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <fcntl.h>
#ifdef MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#elif defined MAJOR_IN_SYSMACROS
#include <sys/sysmacros.h>
#endif

#ifdef HAVE_DELAYACCT
#include <netlink/attr.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/socket.h>
#include <netlink/msg.h>
#include <linux/taskstats.h>
#endif

/*{

#include "ProcessList.h"

extern long long btime;

typedef struct CPUData_ {
   unsigned long long int totalTime;
   unsigned long long int userTime;
   unsigned long long int systemTime;
   unsigned long long int systemAllTime;
   unsigned long long int idleAllTime;
   unsigned long long int idleTime;
   unsigned long long int niceTime;
   unsigned long long int ioWaitTime;
   unsigned long long int irqTime;
   unsigned long long int softIrqTime;
   unsigned long long int stealTime;
   unsigned long long int guestTime;
   unsigned long long int totalPeriod;
   unsigned long long int userPeriod;
   unsigned long long int systemPeriod;
   unsigned long long int systemAllPeriod;
   unsigned long long int idleAllPeriod;
   unsigned long long int idlePeriod;
   unsigned long long int nicePeriod;
   unsigned long long int ioWaitPeriod;
   unsigned long long int irqPeriod;
   unsigned long long int softIrqPeriod;
   unsigned long long int stealPeriod;
   unsigned long long int guestPeriod;
} CPUData;

typedef struct TtyDriver_ {
   char* path;
   unsigned int major;
   unsigned int minorFrom;
   unsigned int minorTo;
} TtyDriver;

typedef struct LinuxProcessList_ {
   ProcessList super;
   CPUData* cpus;
   TtyDriver* ttyDrivers;
   #ifdef HAVE_DELAYACCT
   struct nl_sock *netlink_socket;
   int netlink_family;
   #endif
   bool support_kthread_flag;
} LinuxProcessList;

#ifndef PROCDIR
#define PROCDIR "/proc"
#endif

#ifndef PROCSTATFILE
#define PROCSTATFILE PROCDIR "/stat"
#endif

#ifndef PROCMEMINFOFILE
#define PROCMEMINFOFILE PROCDIR "/meminfo"
#endif

#ifndef PROCTTYDRIVERSFILE
#define PROCTTYDRIVERSFILE PROCDIR "/tty/drivers"
#endif

#ifndef PROC_LINE_LENGTH
#define PROC_LINE_LENGTH 4096
#endif

#ifndef SYS_SYSTEM_CPU_DIR
#define SYS_SYSTEM_CPU_DIR "/sys/devices/system/cpu/"
#endif

}*/

#ifndef PF_KTHREAD
#define PF_KTHREAD 0x00200000
#endif

#ifndef CLAMP
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))
#endif

static int sortTtyDrivers(const void* va, const void* vb) {
   const TtyDriver *a = (const TtyDriver *)va;
   const TtyDriver *b = (const TtyDriver *)vb;
   return (a->major == b->major) ? uintcmp(a->minorFrom, b->minorFrom) : uintcmp(a->major, b->major);
}

static void LinuxProcessList_initTtyDrivers(LinuxProcessList* this) {
   TtyDriver* ttyDrivers;
   int fd = open(PROCTTYDRIVERSFILE, O_RDONLY);
   if (fd == -1)
      return;
   char* buf = NULL;
   int bufSize = MAX_READ;
   int bufLen = 0;
   for(;;) {
      buf = realloc(buf, bufSize);
      int size = xread(fd, buf + bufLen, MAX_READ);
      if (size <= 0) {
         buf[bufLen] = '\0';
         close(fd);
         break;
      }
      bufLen += size;
      bufSize += MAX_READ;
   }
   if (bufLen == 0) {
      free(buf);
      return;
   }
   int numDrivers = 0;
   int allocd = 10;
   ttyDrivers = malloc(sizeof(TtyDriver) * allocd);
   char* at = buf;
   while (*at != '\0') {
      at = strchr(at, ' ');    // skip first token
      while (*at == ' ') at++; // skip spaces
      char* token = at;        // mark beginning of path
      at = strchr(at, ' ');    // find end of path
      *at = '\0'; at++;        // clear and skip
      ttyDrivers[numDrivers].path = strdup(token); // save
      while (*at == ' ') at++; // skip spaces
      token = at;              // mark beginning of major
      at = strchr(at, ' ');    // find end of major
      *at = '\0'; at++;        // clear and skip
      ttyDrivers[numDrivers].major = atoi(token); // save
      while (*at == ' ') at++; // skip spaces
      token = at;              // mark beginning of minorFrom
      while (*at >= '0' && *at <= '9') at++; //find end of minorFrom
      if (*at == '-') {        // if has range
         *at = '\0'; at++;        // clear and skip
         ttyDrivers[numDrivers].minorFrom = atoi(token); // save
         token = at;              // mark beginning of minorTo
         at = strchr(at, ' ');    // find end of minorTo
         *at = '\0'; at++;        // clear and skip
         ttyDrivers[numDrivers].minorTo = atoi(token); // save
      } else {                 // no range
         *at = '\0'; at++;        // clear and skip
         ttyDrivers[numDrivers].minorFrom = atoi(token); // save
         ttyDrivers[numDrivers].minorTo = atoi(token); // save
      }
      at = strchr(at, '\n');   // go to end of line
      at++;                    // skip
      numDrivers++;
      if (numDrivers == allocd) {
         allocd += 10;
         ttyDrivers = realloc(ttyDrivers, sizeof(TtyDriver) * allocd);
      }
   }
   free(buf);
   numDrivers++;
   ttyDrivers = realloc(ttyDrivers, sizeof(TtyDriver) * numDrivers);
   ttyDrivers[numDrivers - 1].path = NULL;
   qsort(ttyDrivers, numDrivers - 1, sizeof(TtyDriver), sortTtyDrivers);
   this->ttyDrivers = ttyDrivers;
}

#ifdef HAVE_DELAYACCT

static void LinuxProcessList_initNetlinkSocket(LinuxProcessList* this) {
   this->netlink_socket = nl_socket_alloc();
   if (this->netlink_socket == NULL) {
      return;
   }
   if (nl_connect(this->netlink_socket, NETLINK_GENERIC) < 0) {
      return;
   }
   this->netlink_family = genl_ctrl_resolve(this->netlink_socket, TASKSTATS_GENL_NAME);
}

#endif

ProcessList* ProcessList_new(UsersTable* usersTable, const Hashtable *pidWhiteList, uid_t userId) {
   LinuxProcessList* this = xCalloc(1, sizeof(LinuxProcessList));
   ProcessList* pl = &(this->super);

   ProcessList_init(pl, Class(LinuxProcess), usersTable, pidWhiteList, userId);
   LinuxProcessList_initTtyDrivers(this);

   #ifdef HAVE_DELAYACCT
   LinuxProcessList_initNetlinkSocket(this);
   #endif

   // Update CPU count:
   unsigned int cpu_count = 0;
   unsigned int max_cpu_i = 0;

   DIR *dir = opendir(SYS_SYSTEM_CPU_DIR);
   if(dir) {
      struct dirent *e;
      while((e = readdir(dir))) {
         char *end_p;
         if(!String_startsWith(e->d_name, "cpu")) continue;
         unsigned int cpu_i = strtoul(e->d_name + 3, &end_p, 10);
         if(*end_p) continue;
         cpu_count++;
         if(cpu_i > max_cpu_i) max_cpu_i = cpu_i;
      }
      closedir(dir);
      if(!cpu_count) dir = NULL;
   }

   FILE* file = fopen(PROCSTATFILE, "r");
   if (file == NULL) {
      CRT_fatalError("Cannot open " PROCSTATFILE, 0);
   }
   do {
      char buffer[PROC_LINE_LENGTH + 1];
      if (fgets(buffer, PROC_LINE_LENGTH + 1, file) == NULL) {
         CRT_fatalError("No btime in " PROCSTATFILE, 0);
      } else if (!dir && String_startsWith(buffer, "cpu") && buffer[3] != ' ') {
         char *end_p;
         unsigned int cpu_i = strtoul(buffer + 3, &end_p, 10);
         if(*end_p != ' ') continue;
         cpu_count++;
         if(cpu_i > max_cpu_i) max_cpu_i = cpu_i;
      } else if (String_startsWith(buffer, "btime ")) {
         sscanf(buffer, "btime %lld\n", &btime);
         break;
      }
   } while(true);
   fclose(file);

   if(cpu_count <= max_cpu_i) cpu_count = max_cpu_i + 1;
   pl->cpuCount = MAX(cpu_count, 1);
   this->cpus = xCalloc(cpu_count + 1, sizeof(CPUData));
   for (unsigned int i = 0; i < cpu_count + 1; i++) {
      this->cpus[i].totalTime = 1;
      this->cpus[i].totalPeriod = 1;
   }

   this->support_kthread_flag = true;
   struct utsname utsname;
   if(uname(&utsname) == 0 && strcmp(utsname.sysname, "Linux") == 0) {
      char *end_p;
      long int n = strtol(utsname.release, &end_p, 10);
      if(*end_p == '.') {
         if(n == 2) {
            n = strtol(end_p + 1, &end_p, 10);
            if(*end_p == '.') {
               if(n == 6) {
                  n = strtol(end_p + 1, NULL, 10);
                  if(n < 27) this->support_kthread_flag = false;
               } else if(n < 6) {
                  this->support_kthread_flag = false;
               }
            } else this->support_kthread_flag = false;
         } else if(n < 2) {
            this->support_kthread_flag = false;
         }
      }
   }

   return pl;
}

void ProcessList_delete(ProcessList* pl) {
   LinuxProcessList* this = (LinuxProcessList*) pl;
   ProcessList_done(pl);
   free(this->cpus);
   if (this->ttyDrivers) {
      for(int i = 0; this->ttyDrivers[i].path; i++) {
         free(this->ttyDrivers[i].path);
      }
      free(this->ttyDrivers);
   }
   #ifdef HAVE_DELAYACCT
   if (this->netlink_socket) {
      nl_close(this->netlink_socket);
      nl_socket_free(this->netlink_socket);
   }
   #endif
   free(this);
}

static inline unsigned long long LinuxProcess_adjustTime(unsigned long long t) {
   static double jiffy = -1;
   if(jiffy < 0) jiffy = sysconf(_SC_CLK_TCK);
   double jiffytime = 1.0 / jiffy;
   return (unsigned long long) t * jiffytime * 100;
}

static bool LinuxProcessList_readStatFile(Process *process, const char* dirname, const char* name, char* command, int* commLen) {
   LinuxProcess* lp = (LinuxProcess*) process;
   char filename[MAX_NAME];
   xSnprintf(filename, MAX_NAME, "%s/%s/stat", dirname, name);
   int fd = open(filename, O_RDONLY);
   if (fd == -1)
      return false;

   static char buf[MAX_READ+1];

   int size = xread(fd, buf, MAX_READ);
   close(fd);
   if (size <= 0) return false;
   buf[size] = '\0';

   assert(process->pid == atoi(buf));
   char *location = strchr(buf, ' ');
   if (!location) return false;

   location += 2;
   char *end = strrchr(location, ')');
   if (!end) return false;
   int commsize = end - location;
   memcpy(command, location, commsize);
   command[commsize] = '\0';
   *commLen = commsize;
   location = end + 2;

   process->state = location[0];
   if(*++location != ' ' || !*++location) return false;
   process->ppid = strtol(location, &location, 10);
   if(*location != ' ' || !*++location) return false;
   process->pgrp = strtoul(location, &location, 10);
   if(*location != ' ' || !*++location) return false;
   process->session = strtoul(location, &location, 10);
   if(*location != ' ' || !*++location) return false;
   process->tty_nr = strtol(location, &location, 10);
   if(*location != ' ' || !*++location) return false;
   process->tpgid = strtol(location, &location, 10);
   if(*location != ' ' || !*++location) return false;
   unsigned int flags = strtoul(location, &location, 10);
   if(*location != ' ' || !*++location) return true;
   lp->is_kernel_process = flags & PF_KTHREAD;
   process->minflt = strtoull(location, &location, 10);
   if(*location != ' ' || !*++location) return true;
   lp->cminflt = strtoull(location, &location, 10);
   if(*location != ' ' || !*++location) return true;
   process->majflt = strtoull(location, &location, 10);
   if(*location != ' ' || !*++location) return true;
   lp->cmajflt = strtoull(location, &location, 10);
   if(*location != ' ' || !*++location) return true;
   lp->utime = LinuxProcess_adjustTime(strtoull(location, &location, 10));
   if(*location != ' ' || !*++location) return true;
   lp->stime = LinuxProcess_adjustTime(strtoull(location, &location, 10));
   process->time = lp->utime + lp->stime;
   if(*location != ' ' || !*++location) return true;
   lp->cutime = LinuxProcess_adjustTime(strtoull(location, &location, 10));
   if(*location != ' ' || !*++location) return true;
   lp->cstime = LinuxProcess_adjustTime(strtoull(location, &location, 10));
   if(*location != ' ' || !*++location) return true;
   process->priority = strtol(location, &location, 10);
   if(*location != ' ' || !*++location) return true;
   process->nice = strtol(location, &location, 10);
   if(*location != ' ' || !*++location) return true;
   process->nlwp = strtol(location, &location, 10);
   if(*location != ' ' || !*++location) return true;
   location = strchr(location, ' ');
   if(!location) return true;
   location++;
   lp->starttime = strtoll(location, &location, 10);
   if(*location != ' ' || !*++location) return true;
   for (int i=0; i<15; i++) {
      location = strchr(location, ' ');
      if(!location) return true;
      location++;
   }
   process->exit_signal = strtol(location, &location, 10);
   if(*location != ' ' || !*++location) return true;
   process->processor = strtol(location, NULL, 10);
   return true;
}

// Only used for Linux without PF_KTHREAD flag support (version < 2.6.27).
static void check_legacy_kernel_process(LinuxProcess *proc) {
	if(proc->super.m_size > 0) return;
	if(proc->super.state == 'Z') return;

	/* Not checking 'cwd' symbolic link because Linux versions before
	 * 2.1.96 didn't pass CLONE_FS when creating kernel processes. */
	char path[MAX_NAME];
	int len = snprintf(path, sizeof path, PROCDIR "/%d/environ", proc->super.pid);
	assert(len < (int)sizeof path);
	char *name_p = path + (len - 7);
	char buffer;
	unsigned int deny_count = 0;
	while(true) {
		int fd = open(path, O_RDONLY);
		if(fd == -1) {
			if(errno != EACCES) return;
			deny_count++;
		} else {
			int s;
			do {
				s = read(fd, &buffer, 1);
			} while(s < 0 && errno == EINTR);
			close(fd);
			if(s) return;
		}
		switch(*name_p) {
			case 'e':
				memcpy(name_p, "cmdline", 7);
				break;
			case 'c':
				strcpy(name_p, "maps");
				break;
			default:
				goto check_exe;
		}
	}
check_exe:
	strcpy(name_p, "exe");
	if(readlink(path, &buffer, 1) >= 0) return;
	if(errno != ENOENT && (errno != EACCES || deny_count > 2)) return;
	proc->is_kernel_process = true;
}

static bool LinuxProcessList_getOwner(Process* process, int pid) {
	char path[MAX_NAME];
	int len = snprintf(path, sizeof path, PROCDIR "/%d", pid);
	assert(len < (int)sizeof path - 5);	// Assuming MAX_NAME is large enough to hold the full path
	struct stat st;
	if(stat(path, &st) < 0) return false;
	process->ruid = st.st_uid;
	strcpy(path + len, "/stat");
	if(stat(path, &st) < 0) return false;
	process->euid = st.st_uid;
	return true;
}

#ifdef HAVE_TASKSTATS

static void LinuxProcessList_readIoFile(LinuxProcess* process, const char* dirname, char* name, unsigned long long now) {
   char filename[MAX_NAME];
   xSnprintf(filename, MAX_NAME, "%s/%s/io", dirname, name);
   int fd = open(filename, O_RDONLY);
   if (fd == -1) {
      process->io_rate_read_bps = -1;
      process->io_rate_write_bps = -1;
      process->io_rchar = -1LL;
      process->io_wchar = -1LL;
      process->io_syscr = -1LL;
      process->io_syscw = -1LL;
      process->io_read_bytes = -1LL;
      process->io_write_bytes = -1LL;
      process->io_cancelled_write_bytes = -1LL;
      process->io_rate_read_time = -1LL;
      process->io_rate_write_time = -1LL;
      return;
   }
   char buffer[1024];
   ssize_t buflen = xread(fd, buffer, 1023);
   close(fd);
   if (buflen < 1) return;
   buffer[buflen] = '\0';
   unsigned long long last_read = process->io_read_bytes;
   unsigned long long last_write = process->io_write_bytes;
   char *buf = buffer;
   char *line = NULL;
   while ((line = strsep(&buf, "\n")) != NULL) {
      switch (line[0]) {
      case 'r':
         if (line[1] == 'c' && strncmp(line+2, "har: ", 5) == 0)
            process->io_rchar = strtoull(line+7, NULL, 10);
         else if (strncmp(line+1, "ead_bytes: ", 11) == 0) {
            process->io_read_bytes = strtoull(line+12, NULL, 10);
            process->io_rate_read_bps =
               ((double)(process->io_read_bytes - last_read))/(((double)(now - process->io_rate_read_time))/1000);
            process->io_rate_read_time = now;
         }
         break;
      case 'w':
         if (line[1] == 'c' && strncmp(line+2, "har: ", 5) == 0)
            process->io_wchar = strtoull(line+7, NULL, 10);
         else if (strncmp(line+1, "rite_bytes: ", 12) == 0) {
            process->io_write_bytes = strtoull(line+13, NULL, 10);
            process->io_rate_write_bps =
               ((double)(process->io_write_bytes - last_write))/(((double)(now - process->io_rate_write_time))/1000);
            process->io_rate_write_time = now;
         }
         break;
      case 's':
         if (line[4] == 'r' && strncmp(line+1, "yscr: ", 6) == 0) {
            process->io_syscr = strtoull(line+7, NULL, 10);
         } else if (strncmp(line+1, "yscw: ", 6) == 0) {
            process->io_syscw = strtoull(line+7, NULL, 10);
         }
         break;
      case 'c':
         if (strncmp(line+1, "ancelled_write_bytes: ", 22) == 0) {
           process->io_cancelled_write_bytes = strtoull(line+23, NULL, 10);
        }
      }
   }
}

#endif



static bool LinuxProcessList_readStatmFile(LinuxProcess* process, const char* dirname, const char* name) {
   char filename[MAX_NAME];
   xSnprintf(filename, MAX_NAME, "%s/%s/statm", dirname, name);
   int fd = open(filename, O_RDONLY);
   if (fd == -1)
      return false;
   char buf[PROC_LINE_LENGTH + 1];
   ssize_t rres = xread(fd, buf, PROC_LINE_LENGTH);
   close(fd);
   if (rres < 1) return false;

   char *p = buf;
   errno = 0;
   process->super.m_size = strtol(p, &p, 10); if (*p == ' ') p++;
   process->super.m_resident = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_share = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_trs = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_lrs = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_drs = strtol(p, &p, 10); if (*p == ' ') p++;
   process->m_dt = strtol(p, &p, 10);
   return (errno == 0);
}

#ifdef HAVE_OPENVZ

static void LinuxProcessList_readOpenVZData(LinuxProcess* process, const char* dirname, const char* name) {
   if ( (access(PROCDIR "/vz", R_OK) != 0)) {
      process->vpid = process->super.pid;
      process->ctid = 0;
      return;
   }
   char filename[MAX_NAME];
   xSnprintf(filename, MAX_NAME, "%s/%s/stat", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file)
      return;
   (void) fscanf(file,
      "%*32u %*32s %*1c %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %*32u %*32u %*32u %*32u %*32u "
      "%*32u %*32u %32u %32u",
      &process->vpid, &process->ctid);
   fclose(file);
   return;
}

#endif

#ifdef HAVE_CGROUP

static void LinuxProcessList_readCGroupFile(LinuxProcess* process, const char* dirname, const char* name) {
   char filename[MAX_NAME];
   xSnprintf(filename, MAX_NAME, "%s/%s/cgroup", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file) {
      process->cgroup = xStrdup("");
      return;
   }
   char output[PROC_LINE_LENGTH + 1];
   output[0] = '\0';
   char* at = output;
   int left = PROC_LINE_LENGTH;
   while (!feof(file) && left > 0) {
      char buffer[PROC_LINE_LENGTH + 1];
      char *ok = fgets(buffer, PROC_LINE_LENGTH, file);
      if (!ok) break;
      char* group = strchr(buffer, ':');
      if (!group) break;
      if (at != output) {
         *at = ';';
         at++;
         left--;
      }
      int wrote = snprintf(at, left, "%s", group);
      left -= wrote;
   }
   fclose(file);
   free(process->cgroup);
   process->cgroup = xStrdup(output);
}

#endif

#ifdef HAVE_VSERVER

static void LinuxProcessList_readVServerData(LinuxProcess* process, const char* dirname, const char* name) {
   char filename[MAX_NAME];
   xSnprintf(filename, MAX_NAME, "%s/%s/status", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file)
      return;
   char buffer[PROC_LINE_LENGTH + 1];
   process->vxid = 0;
   while (fgets(buffer, PROC_LINE_LENGTH, file)) {
      if (String_startsWith(buffer, "VxID:")) {
         int vxid;
         int ok = sscanf(buffer, "VxID:\t%32d", &vxid);
         if (ok >= 1) {
            process->vxid = vxid;
         }
      }
      #if defined HAVE_ANCIENT_VSERVER
      else if (String_startsWith(buffer, "s_context:")) {
         int vxid;
         int ok = sscanf(buffer, "s_context:\t%32d", &vxid);
         if (ok >= 1) {
            process->vxid = vxid;
         }
      }
      #endif
   }
   fclose(file);
}

#endif

static void LinuxProcessList_readOomData(LinuxProcess* process, const char* dirname, const char* name) {
   char filename[MAX_NAME];
   xSnprintf(filename, MAX_NAME, "%s/%s/oom_score", dirname, name);
   FILE* file = fopen(filename, "r");
   if (!file) {
      return;
   }
   char buffer[PROC_LINE_LENGTH + 1];
   if (fgets(buffer, PROC_LINE_LENGTH, file)) {
      unsigned int oom;
      int ok = sscanf(buffer, "%32u", &oom);
      if (ok >= 1) {
         process->oom = oom;
      }
   }
   fclose(file);
}

#ifdef HAVE_DELAYACCT

static int handleNetlinkMsg(struct nl_msg *nlmsg, void *linuxProcess) {
   struct nlmsghdr *nlhdr;
   struct nlattr *nlattrs[TASKSTATS_TYPE_MAX + 1];
   struct nlattr *nlattr;
   struct taskstats *stats;
   int rem;
   unsigned long long int timeDelta;
   LinuxProcess* lp = (LinuxProcess*) linuxProcess;

   nlhdr = nlmsg_hdr(nlmsg);

   if (genlmsg_parse(nlhdr, 0, nlattrs, TASKSTATS_TYPE_MAX, NULL) < 0) {
      return NL_SKIP;
   }

   if ((nlattr = nlattrs[TASKSTATS_TYPE_AGGR_PID]) || (nlattr = nlattrs[TASKSTATS_TYPE_NULL])) {
      stats = nla_data(nla_next(nla_data(nlattr), &rem));
      assert(lp->super.pid == stats->ac_pid);
      timeDelta = (stats->ac_etime*1000 - lp->delay_read_time);
      #define BOUNDS(x) isnan(x) ? 0.0 : (x > 100) ? 100.0 : x;
      #define DELTAPERC(x,y) BOUNDS((float) (x - y) / timeDelta * 100);
      lp->cpu_delay_percent = DELTAPERC(stats->cpu_delay_total, lp->cpu_delay_total);
      lp->blkio_delay_percent = DELTAPERC(stats->blkio_delay_total, lp->blkio_delay_total);
      lp->swapin_delay_percent = DELTAPERC(stats->swapin_delay_total, lp->swapin_delay_total);
      #undef DELTAPERC
      #undef BOUNDS
      lp->swapin_delay_total = stats->swapin_delay_total;
      lp->blkio_delay_total = stats->blkio_delay_total;
      lp->cpu_delay_total = stats->cpu_delay_total;
      lp->delay_read_time = stats->ac_etime*1000;
   }
   return NL_OK;
}

static void LinuxProcessList_readDelayAcctData(LinuxProcessList* this, LinuxProcess* process) {
   struct nl_msg *msg;

   if (nl_socket_modify_cb(this->netlink_socket, NL_CB_VALID, NL_CB_CUSTOM, handleNetlinkMsg, process) < 0) {
      return;
   }

   if (! (msg = nlmsg_alloc())) {
      return;
   }

   if (! genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, this->netlink_family, 0, NLM_F_REQUEST, TASKSTATS_CMD_GET, TASKSTATS_VERSION)) {
      nlmsg_free(msg);
   }

   if (nla_put_u32(msg, TASKSTATS_CMD_ATTR_PID, process->super.pid) < 0) {
      nlmsg_free(msg);
   }

   if (nl_send_sync(this->netlink_socket, msg) < 0) {
      process->swapin_delay_percent = -1LL;
      process->blkio_delay_percent = -1LL;
      process->cpu_delay_percent = -1LL;
      return;
   }
   if (nl_recvmsgs_default(this->netlink_socket) < 0) {
      return;
   }
}

#endif

static void setCommand(Process* process, const char* command, int len) {
   if (process->comm && process->commLen >= len) {
      memcpy(process->comm, command, len + 1);
   } else {
      free(process->comm);
      process->comm = xStrdup(command);
   }
   process->commLen = len;
}

static bool LinuxProcessList_readCmdlineFile(Process* process, const char* dirname, const char* name) {
   char filename[MAX_NAME];
   xSnprintf(filename, MAX_NAME, "%s/%s/cmdline", dirname, name);
   int fd = open(filename, O_RDONLY);
   if (fd == -1) return false;
   char command[4096+1]; // max cmdline length on Linux
   int amtRead = xread(fd, command, sizeof(command) - 1);
   close(fd);
   int tokenEnd = 0;
   int lastChar = 0;
   if (amtRead == 0) {
      return true;
   } else if (amtRead < 0) {
      return false;
   }
   for (int i = 0; i < amtRead; i++) {
      if (command[i] == '\0' || command[i] == '\n') {
         if (tokenEnd == 0) {
            tokenEnd = i;
         }
         command[i] = ' ';
      } else {
         lastChar = i;
      }
   }
   if (tokenEnd == 0) {
      tokenEnd = amtRead;
   }
   command[lastChar + 1] = '\0';
   process->argv0_length = tokenEnd;
   setCommand(process, command, lastChar + 1);

   return true;
}

static char* LinuxProcessList_updateTtyDevice(TtyDriver* ttyDrivers, dev_t tty_nr) {
   unsigned int maj = major(tty_nr);
   unsigned int min = minor(tty_nr);
   int i = -1;
   while(ttyDrivers[++i].path && maj >= ttyDrivers[i].major) {
      if (maj > ttyDrivers[i].major) {
         continue;
      }
      if (min < ttyDrivers[i].minorFrom) {
         break;
      }
      if (min > ttyDrivers[i].minorTo) {
         continue;
      }
      unsigned int unit = min - ttyDrivers[i].minorFrom;
      struct stat st;
      char* fullPath;
      do {
         if(asprintf(&fullPath, "%s/%u", ttyDrivers[i].path, unit) < 0) return NULL;
         if(stat(fullPath, &st) == 0 &&
           (unsigned int)major(st.st_rdev) == maj && (unsigned int)minor(st.st_rdev) == min) {
            return fullPath;
         }
         free(fullPath);
         if(asprintf(&fullPath, "%s%u", ttyDrivers[i].path, unit) < 0) return NULL;
         if(stat(fullPath, &st) == 0 &&
           (unsigned int)major(st.st_rdev) == maj && (unsigned int)minor(st.st_rdev) == min) {
            return fullPath;
         }
         free(fullPath);
      } while(unit != min && (int)(unit = min) >= 0);
      if(stat(ttyDrivers[i].path, &st) == 0 && tty_nr == st.st_rdev) {
         return strdup(ttyDrivers[i].path);
      }
   }
   char* out;
   if(asprintf(&out, "%u:%u", maj, min) < 0) return NULL;
   return out;
}

static bool LinuxProcessList_recurseProcTree(LinuxProcessList* this, const char* dirname, Process* parent, double period, struct timeval tv) {
   ProcessList* pl = (ProcessList*) this;
   DIR* dir;
   struct dirent* entry;
   Settings* settings = pl->settings;

   #ifdef HAVE_TASKSTATS
   unsigned long long now = tv.tv_sec*1000LL+tv.tv_usec/1000LL;
   #endif

   dir = opendir(dirname);
   if (!dir) return false;
   int cpus = pl->cpuCount;
   bool hide_kernel_processes = settings->hide_kernel_processes;
   bool hide_thread_processes = settings->hide_thread_processes;
   while ((entry = readdir(dir)) != NULL) {
      char* name = entry->d_name;

      // The RedHat kernel hides threads with a dot.
      // I believe this is non-standard.
      if (name[0] == '.') {
         name++;
      }

      // Just skip all non-number directories.
      if (name[0] < '0' || name[0] > '9') {
         continue;
      }

      // filename is a number: process directory
      int pid = atoi(name);
      if (parent && pid == parent->pid) continue;
      if (pid <= 0) continue;

      bool preExisting = false;
      Process* proc = ProcessList_getProcess(pl, pid, &preExisting, (Process_New) LinuxProcess_new);
      proc->tgid = parent ? parent->pid : pid;
      LinuxProcess* lp = (LinuxProcess*) proc;

      char subdirname[MAX_NAME];
      xSnprintf(subdirname, MAX_NAME, "%s/%s/task", dirname, name);
      LinuxProcessList_recurseProcTree(this, subdirname, proc, period, tv);

      #ifdef HAVE_TASKSTATS
      if (settings->flags & PROCESS_FLAG_IO)
         LinuxProcessList_readIoFile(lp, dirname, name, now);
      #endif

      if (! LinuxProcessList_readStatmFile(lp, dirname, name))
         goto errorReadingProcess;

      char command[MAX_NAME+1];
      unsigned long long int lasttimes = (lp->utime + lp->stime);
      int commLen = 0;
      dev_t tty_nr = proc->tty_nr;
      if (! LinuxProcessList_readStatFile(proc, dirname, name, command, &commLen)) {
         goto errorReadingProcess;
      }
      free(proc->name);
      proc->name = xStrdup(command);
      if(!this->support_kthread_flag && !lp->is_kernel_process) check_legacy_kernel_process(lp);
      if (tty_nr != proc->tty_nr && this->ttyDrivers) {
         free(lp->ttyDevice);
         lp->ttyDevice = LinuxProcessList_updateTtyDevice(this->ttyDrivers, proc->tty_nr);
      }
      if (settings->flags & PROCESS_FLAG_LINUX_IOPRIO)
         LinuxProcess_updateIOPriority(lp);
      float percent_cpu = (lp->utime + lp->stime - lasttimes) / period * 100.0;
      proc->percent_cpu = CLAMP(percent_cpu, 0.0, cpus * 100.0);
      if (isnan(proc->percent_cpu)) proc->percent_cpu = 0.0;
      proc->percent_mem = (proc->m_resident * CRT_page_size_kib) / (double)(pl->totalMem) * 100.0;

      if(!preExisting) {

         if (! LinuxProcessList_getOwner(proc, pid))
            goto errorReadingProcess;

         proc->real_user = UsersTable_getRef(pl->usersTable, proc->ruid);
         proc->effective_user = UsersTable_getRef(pl->usersTable, proc->euid);

         #ifdef HAVE_OPENVZ
         if (settings->flags & PROCESS_FLAG_LINUX_OPENVZ) {
            LinuxProcessList_readOpenVZData(lp, dirname, name);
         }
         #endif
         #ifdef HAVE_VSERVER
         if (settings->flags & PROCESS_FLAG_LINUX_VSERVER) {
            LinuxProcessList_readVServerData(lp, dirname, name);
         }
         #endif

         if (! LinuxProcessList_readCmdlineFile(proc, dirname, name)) {
            goto errorReadingProcess;
         }

         ProcessList_add(pl, proc);
      } else {
         if (ProcessList_shouldUpdateProcessNames(pl) && proc->state != 'Z') {
            if (! LinuxProcessList_readCmdlineFile(proc, dirname, name)) {
               goto errorReadingProcess;
            }
         }
      }

      #ifdef HAVE_DELAYACCT
      LinuxProcessList_readDelayAcctData(this, lp);
      #endif

      #ifdef HAVE_CGROUP
      if (settings->flags & PROCESS_FLAG_LINUX_CGROUP)
         LinuxProcessList_readCGroupFile(lp, dirname, name);
      #endif
      if (settings->flags & PROCESS_FLAG_LINUX_OOM)
         LinuxProcessList_readOomData(lp, dirname, name);

      if (!proc->comm || (proc->state == 'Z' && proc->argv0_length == 0)) {
         proc->argv0_length = -1;
         setCommand(proc, command, commLen);
      } else if (Process_isExtraThreadProcess(proc)) {
         if (settings->showThreadNames || (proc->state == 'Z' && proc->argv0_length == 0)) {
            proc->argv0_length = -1;
            setCommand(proc, command, commLen);
         } else if (settings->showThreadNames && !LinuxProcessList_readCmdlineFile(proc, dirname, name)) {
            goto errorReadingProcess;
         }
      }
      pl->totalTasks++;
      pl->thread_count++;
      if (Process_isKernelProcess(proc)) {
         pl->kernel_process_count++;
         pl->kernel_thread_count++;
      }
      if (proc->state == 'R') {
         pl->running_process_count++;
         pl->running_thread_count++;
      }

      proc->show = !((hide_kernel_processes && Process_isKernelProcess(proc)) || (hide_thread_processes && Process_isExtraThreadProcess(proc)));

      proc->updated = true;
      continue;

      // Exception handler.
      errorReadingProcess: {
         if (preExisting) {
            ProcessList_remove(pl, proc);
         } else {
            Process_delete((Object*)proc);
         }
      }
   }
   closedir(dir);
   return true;
}

static inline void LinuxProcessList_scanMemoryInfo(ProcessList* this) {
   unsigned long long int swapFree = 0;
   unsigned long long int shmem = 0;
   unsigned long long int sreclaimable = 0;

   FILE* file = fopen(PROCMEMINFOFILE, "r");
   if (file == NULL) {
      CRT_fatalError("Cannot open " PROCMEMINFOFILE, 0);
   }
   char buffer[128];
   while (fgets(buffer, 128, file)) {
      #define tryRead(label, variable) do { if (String_startsWith(buffer, label) && sscanf(buffer + strlen(label), " %32llu ", variable)) continue; } while(0)
      switch (buffer[0]) {
      case 'M':
         tryRead("MemTotal:", &this->totalMem);
         tryRead("MemFree:", &this->freeMem);
         break;
      case 'B':
         tryRead("Buffers:", &this->buffersMem);
         break;
      case 'C':
         tryRead("Cached:", &this->cachedMem);
         break;
      case 'S':
         switch (buffer[1]) {
            case 'w':
               tryRead("SwapTotal:", &this->totalSwap);
               tryRead("SwapFree:", &swapFree);
               break;
            case 'h':
               tryRead("Shmem:", &shmem);
               break;
            case 'R':
               tryRead("SReclaimable:", &sreclaimable);
               break;
         }
         break;
      }
      #undef tryRead
   }
   fclose(file);
   this->usedMem = this->totalMem - this->freeMem;
   if(this->cachedMem > shmem) this->cachedMem -= shmem;
   this->cachedMem += sreclaimable;
   this->usedSwap = this->totalSwap - swapFree;
}

static inline double LinuxProcessList_scanCPUTime(LinuxProcessList* this) {

   FILE* file = fopen(PROCSTATFILE, "r");
   if (file == NULL) {
      CRT_fatalError("Cannot open " PROCSTATFILE, 0);
   }
   char offline_cpu_map[this->super.cpuCount];
   memset(offline_cpu_map, 1, this->super.cpuCount);
   unsigned int i = 0;
   char buffer[PROC_LINE_LENGTH + 1];
   while(fgets(buffer, PROC_LINE_LENGTH + 1, file)) {
      int cpuid = -1;
      unsigned long long int usertime, nicetime, systemtime, idletime;
      unsigned long long int ioWait, irq, softIrq, steal, guest, guestnice;
      ioWait = irq = softIrq = steal = guest = guestnice = 0;
      // Depending on your kernel version,
      // 5, 7, 8 or 9 of these fields will be set.
      // The rest will remain at zero.
      if(!String_startsWith(buffer, "cpu")) continue;
      if(i++ == 0) {
         sscanf(buffer, "cpu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu", &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
      } else {
         sscanf(buffer, "cpu%4d %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu", &cpuid, &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
         offline_cpu_map[cpuid] = 0;
      }
      // Guest time is already accounted in usertime
      usertime = usertime - guest;
      nicetime = nicetime - guestnice;
      // Fields existing on kernels >= 2.6
      // (and RHEL's patched kernel 2.4...)
      unsigned long long int idlealltime = idletime + ioWait;
      unsigned long long int systemalltime = systemtime + irq + softIrq;
      unsigned long long int virtalltime = guest + guestnice;
      unsigned long long int totaltime = usertime + nicetime + systemalltime + idlealltime + steal + virtalltime;
      if(this->super.cpuCount <= cpuid) {
         this->cpus = xRealloc(this->cpus, (cpuid + 2) * sizeof(CPUData));
/*
         if(this->super.cpuCount < cpuid) {
            memset(this->cpus + this->super.cpuCount + 1, 0, (cpuid - this->super.cpuCount) * sizeof(CPUData));
         }
*/
         memset(this->cpus + this->super.cpuCount + 1, 0, (cpuid - this->super.cpuCount + 1) * sizeof(CPUData));
         this->super.cpuCount = cpuid + 1;
      }
      CPUData *cpuData = this->cpus + cpuid + 1;
      // Since we do a subtraction (usertime - guest) and cputime64_to_clock_t()
      // used in /proc/stat rounds down numbers, it can lead to a case where the
      // integer overflow.
      #define WRAP_SUBTRACT(a,b) (a > b) ? a - b : 0
      cpuData->userPeriod = WRAP_SUBTRACT(usertime, cpuData->userTime);
      cpuData->nicePeriod = WRAP_SUBTRACT(nicetime, cpuData->niceTime);
      cpuData->systemPeriod = WRAP_SUBTRACT(systemtime, cpuData->systemTime);
      cpuData->systemAllPeriod = WRAP_SUBTRACT(systemalltime, cpuData->systemAllTime);
      cpuData->idleAllPeriod = WRAP_SUBTRACT(idlealltime, cpuData->idleAllTime);
      cpuData->idlePeriod = WRAP_SUBTRACT(idletime, cpuData->idleTime);
      cpuData->ioWaitPeriod = WRAP_SUBTRACT(ioWait, cpuData->ioWaitTime);
      cpuData->irqPeriod = WRAP_SUBTRACT(irq, cpuData->irqTime);
      cpuData->softIrqPeriod = WRAP_SUBTRACT(softIrq, cpuData->softIrqTime);
      cpuData->stealPeriod = WRAP_SUBTRACT(steal, cpuData->stealTime);
      cpuData->guestPeriod = WRAP_SUBTRACT(virtalltime, cpuData->guestTime);
      cpuData->totalPeriod = WRAP_SUBTRACT(totaltime, cpuData->totalTime);
      #undef WRAP_SUBTRACT
      cpuData->userTime = usertime;
      cpuData->niceTime = nicetime;
      cpuData->systemTime = systemtime;
      cpuData->systemAllTime = systemalltime;
      cpuData->idleAllTime = idlealltime;
      cpuData->idleTime = idletime;
      cpuData->ioWaitTime = ioWait;
      cpuData->irqTime = irq;
      cpuData->softIrqTime = softIrq;
      cpuData->stealTime = steal;
      cpuData->guestTime = virtalltime;
      cpuData->totalTime = totaltime;
   }
   int online_cpu_count = 0;
   for(i = 0; i < (unsigned int)this->super.cpuCount; i++) {
      if(offline_cpu_map[i]) memset(this->cpus + i + 1, 0, sizeof(CPUData));
      else online_cpu_count++;
   }
   double period = (double)this->cpus[0].totalPeriod / online_cpu_count;
   fclose(file);
   return period;
}

void ProcessList_goThroughEntries(ProcessList* super, bool skip_processes) {
   LinuxProcessList* this = (LinuxProcessList*) super;

   LinuxProcessList_scanMemoryInfo(super);
   double period = LinuxProcessList_scanCPUTime(this);

   if(skip_processes) return;

   struct timeval tv;
   gettimeofday(&tv, NULL);
   LinuxProcessList_recurseProcTree(this, PROCDIR, NULL, period, tv);
}
