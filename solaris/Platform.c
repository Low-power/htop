/*
htop - solaris/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2015 David C. Hunt
(C) 2017,2018 Guy M. Broome
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Platform.h"
#include "Meter.h"
#include "CPUMeter.h"
#include "MemoryMeter.h"
#include "SwapMeter.h"
#include "TasksMeter.h"
#include "LoadAverageMeter.h"
#include "ClockMeter.h"
#include "HostnameMeter.h"
#include "UptimeMeter.h"
#include "SolarisProcess.h"
#include "SolarisProcessList.h"
#include "IOUtils.h"
#include "KStat.h"
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/loadavg.h>
#include <sys/stat.h>
#include <sys/swap.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <sys/var.h>
#ifdef HAVE_LIBPROC
#include <libproc.h>
#else
#include <procfs.h>
#include <unistd.h>
#include <fcntl.h>
#endif

/*{
#include "config.h"
#include "Action.h"
#include "BatteryMeter.h"
#include "SignalsPanel.h"
#include <signal.h>
#include <sys/mkdev.h>
#include <sys/proc.h>

#define PLATFORM_SUPPORT_PROCESS_O_STATE

#define MAX_VALUE_OF(T) (((size_t)1 << (sizeof(T) * 8 - ((T)-1 == -1))) - 1)
}*/

const SignalItem Platform_signals[] = {
   { .name = "Cancel", .number = 0 },
#define SIG(NAME) { .name = #NAME, .number = SIG##NAME }
   SIG(HUP),
   SIG(INT),
   SIG(QUIT),
   SIG(ILL),
   SIG(TRAP),
   SIG(IOT),
   SIG(ABRT),
   SIG(EMT),
   SIG(FPE),
   SIG(KILL),
   SIG(BUS),
   SIG(SEGV),
   SIG(SYS),
   SIG(PIPE),
   SIG(ALRM),
   SIG(TERM),
   SIG(USR1),
   SIG(USR2),
   SIG(CHLD),
   SIG(PWR),
   SIG(WINCH),
   SIG(URG),
   SIG(POLL),
   SIG(IO),
   SIG(STOP),
   SIG(TSTP),
   SIG(CONT),
   SIG(TTIN),
   SIG(TTOU),
   SIG(VTALRM),
   SIG(PROF),
   SIG(XCPU),
   SIG(XFSZ),
   SIG(WAITING),
   SIG(LWP),
   SIG(FREEZE),
   SIG(THAW),
   SIG(CANCEL),
   SIG(LOST),
   SIG(XRES),
#ifdef SIGJVM1
   SIG(JVM1),
#endif
#ifdef SIGJVM2
   SIG(JVM2),
#endif
#undef SIG
};

const unsigned int Platform_numberOfSignals = sizeof(Platform_signals)/sizeof(SignalItem);

#ifdef HAVE_LIBPROC
ProcessField Platform_defaultFields[] = { HTOP_PID_FIELD, HTOP_LWPID_FIELD, HTOP_EFFECTIVE_USER_FIELD, HTOP_PRIORITY_FIELD, HTOP_NICE_FIELD, HTOP_M_SIZE_FIELD, HTOP_M_RESIDENT_FIELD, HTOP_STATE_FIELD, HTOP_PERCENT_CPU_FIELD, HTOP_PERCENT_MEM_FIELD, HTOP_TIME_FIELD, HTOP_COMM_FIELD, 0 };
#else
ProcessField Platform_defaultFields[] = { HTOP_PID_FIELD, HTOP_EFFECTIVE_USER_FIELD, HTOP_PRIORITY_FIELD, HTOP_NICE_FIELD, HTOP_M_SIZE_FIELD, HTOP_M_RESIDENT_FIELD, HTOP_STATE_FIELD, HTOP_PERCENT_CPU_FIELD, HTOP_PERCENT_MEM_FIELD, HTOP_TIME_FIELD, HTOP_COMM_FIELD, 0 };
#endif

MeterClass* Platform_meterTypes[] = {
   &CPUMeter_class,
   &ClockMeter_class,
   &LoadAverageMeter_class,
   &LoadMeter_class,
   &MemoryMeter_class,
   &SwapMeter_class,
   &TasksMeter_class,
   &BatteryMeter_class,
   &HostnameMeter_class,
   &UptimeMeter_class,
   &AllCPUsMeter_class,
   &AllCPUs2Meter_class,
   &LeftCPUsMeter_class,
   &RightCPUsMeter_class,
   &LeftCPUs2Meter_class,
   &RightCPUs2Meter_class,
   &BlankMeter_class,
   NULL
};

void Platform_setBindings(Htop_Action* keys) {
   (void) keys;
}

const unsigned int Platform_numberOfFields = HTOP_LAST_PROCESSFIELD;

int Platform_getUptime() {
	// Fallback to utmpx
	return -1;
}

void Platform_getLoadAverage(double* one, double* five, double* fifteen) {
   double plat_loadavg[LOADAVG_NSTATS];
   if(getloadavg(plat_loadavg, LOADAVG_NSTATS) < 0) return;
   *one = plat_loadavg[LOADAVG_1MIN];
   *five = plat_loadavg[LOADAVG_5MIN];
   *fifteen = plat_loadavg[LOADAVG_15MIN];
}

int Platform_getMaxPid() {
#if 0
	/* Disabled because the maximum number of processes doesn't
	 * necessarily mean the maximum possible PID value. */
	struct var *kvar = read_unnamed_kstat(KSTAT_UNIX_VAR);
	if(kvar && kvar->v_proc > 0) return kvar->v_proc - 1;
#endif
#ifdef PID_MAX
	return PID_MAX;
#else
	return = 32778; // Reasonable Solaris default
#endif
}

double Platform_updateCPUValues(Meter *meter, int cpu) {
   const SolarisProcessList *spl = (const SolarisProcessList *)meter->pl;
   const CPUData *cpuData = spl->cpus + (meter->pl->cpuCount > 1 ? cpu : 0);
   double *v = meter->values;
   double percent;
   v[CPU_METER_NICE]   = cpuData->nicePercent;
   v[CPU_METER_NORMAL] = cpuData->userPercent;
   if (meter->pl->settings->detailedCPUTime) {
      v[CPU_METER_KERNEL] = cpuData->systemPercent;
      v[CPU_METER_IRQ]    = cpuData->irqPercent;
      Meter_setItems(meter, 4);
      percent = v[0]+v[1]+v[2]+v[3];
   } else {
      v[2] = cpuData->systemAllPercent;
      Meter_setItems(meter, 3);
      percent = v[0]+v[1]+v[2];
   }

   percent = CLAMP(percent, 0.0, 100.0);
   if (isnan(percent)) percent = 0.0;
   return percent;
}

void Platform_updateMemoryValues(Meter *meter) {
   const ProcessList *pl = meter->pl;
   meter->total = pl->totalMem;
   meter->values[0] = pl->usedMem;
   meter->values[1] = pl->buffersMem;
   meter->values[2] = pl->cachedMem;
}

void Platform_updateSwapValues(Meter *meter) {
   const ProcessList *pl = meter->pl;
   meter->total = pl->totalSwap;
   meter->values[0] = pl->usedSwap;
}

static char **get_process_vector(const Process *proc, bool is_env) {
	bool is_64bit;
	SolarisProcess *sol_proc = (SolarisProcess *)proc;
	switch(sol_proc->data_model) {
		case PR_MODEL_ILP32:
			is_64bit = false;
			break;
		case PR_MODEL_LP64:
			is_64bit = true;
			break;
		default:
			return NULL;
	}
	if(Process_isKernelProcess(proc)) return NULL;
	if((is_env ? sol_proc->envv_offset : sol_proc->argv_offset) < 0) return NULL;
	char path[20];
#ifdef HAVE_LIBPROC
	xSnprintf(path, sizeof path, "/proc/%d/as", (int)sol_proc->realpid);
#else
	xSnprintf(path, sizeof path, "/proc/%d/as", (int)proc->pid);
#endif
	int fd = open(path, O_RDONLY);
	if(fd == -1) return NULL;
	if(lseek(fd, is_env ? sol_proc->envv_offset : sol_proc->argv_offset, SEEK_SET) < 0) {
		close(fd);
		return NULL;
	}
	char **v = xMalloc(sizeof(char *));
	unsigned int i = 0;
	while(true) {
		off_t offset;
		if(is_64bit) {
			uint64_t buffer;
			if(xread(fd, &buffer, 8) < 8) break;
			if(buffer > MAX_VALUE_OF(off_t)) continue;
			offset = buffer;
		} else {
			uint32_t buffer;
			if(xread(fd, &buffer, 4) < 4) break;
			offset = buffer;
		}
		if(!offset) break;
		v[i] = xMalloc(256);
		int len = pread(fd, v[i], 256, offset);
		if(len < 1) {
			free(v[i]);
			continue;
		}
		len = strnlen(v[i], len);
		if(len < 255) v[i] = realloc(v[i], len + 1);
		else if(len == 256) v[i][255] = 0;
		v = xRealloc(v, (++i + 1) * sizeof(char *));
	}
	close(fd);
	v[i] = NULL;
	return v;
}

char **Platform_getProcessArgv(const Process *proc) {
	return get_process_vector(proc, false);
}

#ifdef HAVE_LIBPROC

struct env_accum {
   char **env;
   unsigned int count;
};

static int append_env(void *arg, struct ps_prochandle *handle, uintptr_t addr, const char *s) {
	struct env_accum *accum = arg;
	accum->env[accum->count] = xStrdup(s);
	accum->env = xRealloc(accum->env, (++accum->count + 1) * sizeof(char *));
	return 0; 
}

char **Platform_getProcessEnvv(const Process *proc) {
   pid_t pid = ((SolarisProcess *)proc)->realpid;
   int graberr;
   struct ps_prochandle *handle = Pgrab(pid, PGRAB_RDONLY, &graberr);
   if(!handle) return NULL;
   struct env_accum accum = { .env = xMalloc(sizeof(char *)) };
   Penv_iter(handle, append_env, &accum); 
   Prelease(handle, 0);
   accum.env[accum.count] = NULL;
   return accum.env;
}

#else

char **Platform_getProcessEnvv(const Process *proc) {
	return get_process_vector(proc, true);
}

#endif

#ifndef HAVE_STRNLEN
size_t strnlen(const char *s, size_t max_len) {
	size_t len = 0;
	while(len < max_len && s[len]) len++;
	return len;
}
#endif

bool Platform_haveSwap() {
	return swapctl(SC_GETNSWP, NULL) > 0;
}
