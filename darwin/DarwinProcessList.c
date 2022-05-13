/*
htop - DarwinProcessList.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"
#include "DarwinProcess.h"
#include "DarwinProcessList.h"
#include "CRT.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <libproc.h>
#include <sys/mman.h>
#include <utmpx.h>
#include <err.h>
#include <sys/sysctl.h>
#include <stdbool.h>

struct kern {
    short int version[3];
};

static void GetKernelVersion(struct kern *k) {
   static short int version_[3] = {0};
   if (!version_[0]) {
      // just in case it fails someday
      version_[0] = version_[1] = version_[2] = -1;
      char str[256] = {0};
      size_t size = sizeof(str);
      int ret = sysctlbyname("kern.osrelease", str, &size, NULL, 0);
      if (ret == 0) sscanf(str, "%hd.%hd.%hd", &version_[0], &version_[1], &version_[2]);
    }
    memcpy(k->version, version_, sizeof(version_));
}

/* compare the given os version with the one installed returns:
0 if equals the installed version
positive value if less than the installed version
negative value if more than the installed version
*/
static int CompareKernelVersion(short int major, short int minor, short int component) {
    struct kern k;
    GetKernelVersion(&k);
    if ( k.version[0] !=  major) return k.version[0] - major;
    if ( k.version[1] !=  minor) return k.version[1] - minor;
    if ( k.version[2] !=  component) return k.version[2] - component;
    return 0;
}

/*{
#include "ProcessList.h"
#include <mach/mach_host.h>
#include <sys/sysctl.h>

typedef struct DarwinProcessList_ {
   ProcessList super;

   host_basic_info_data_t host_info;
   vm_statistics_data_t vm_stats;
   processor_cpu_load_info_t prev_load;
   processor_cpu_load_info_t curr_load;
   uint64_t kernel_threads;
   uint64_t user_threads;
   uint64_t global_diff;
} DarwinProcessList;

}*/

static void ProcessList_getHostInfo(host_basic_info_data_t *p) {
   mach_msg_type_number_t info_size = HOST_BASIC_INFO_COUNT;

   if(0 != host_info(mach_host_self(), HOST_BASIC_INFO, (host_info_t)p, &info_size)) {
       CRT_fatalError("Unable to retrieve host info\n");
   }
}

static void ProcessList_freeCPULoadInfo(processor_cpu_load_info_t *p) {
   if(NULL != p && NULL != *p) {
       if(0 != munmap(*p, vm_page_size)) {
           CRT_fatalError("Unable to free old CPU load information\n");
       }
       *p = NULL;
   }
}

static unsigned int ProcessList_allocateCPULoadInfo(processor_cpu_load_info_t *p) {
   mach_msg_type_number_t info_size = sizeof(processor_cpu_load_info_t);
   unsigned cpu_count;

   // TODO Improving the accuracy of the load counts woule help a lot.
   if(0 != host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &cpu_count, (processor_info_array_t *)p, &info_size)) {
       CRT_fatalError("Unable to retrieve CPU info\n");
   }

   return cpu_count;
}

static void ProcessList_getVMStats(vm_statistics_t p) {
    mach_msg_type_number_t info_size = HOST_VM_INFO_COUNT;

    if (host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)p, &info_size) != 0)
       CRT_fatalError("Unable to retrieve VM statistics\n");
}

static struct kinfo_proc *ProcessList_getKInfoProcs(size_t *count) {
   int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
   struct kinfo_proc *processes;

   /* Note the two calls to sysctl(). One to get length and one to get the
    * data. This -does- mean that the second call could end up with a missing
    * process entry or two.
    */
   *count = 0;
   if (sysctl(mib, 3, NULL, count, NULL, 0) < 0) {
      CRT_fatalError("Unable to get size of kproc_infos");
   }

   processes = xMalloc(*count);

   if (sysctl(mib, 3, processes, count, NULL, 0) < 0) {
      CRT_fatalError("Unable to get kinfo_procs");
   }

   *count = *count / sizeof(struct kinfo_proc);

   // Check for process 0 (kernel_task)
   for(unsigned int i = 0; i < *count; i++) {
      if(processes[i].kp_proc.p_pid == 0) return processes;
   }

   mib[2] = KERN_PROC_PID;
   struct kinfo_proc proc0_info;
   size_t info_size = sizeof proc0_info;
   if(sysctl(mib, 4, &proc0_info, &info_size, NULL, 0) == 0 && info_size == sizeof proc0_info) {
      processes = xRealloc(processes, (*count + 1) * sizeof(struct kinfo_proc));
      memcpy(processes + *count, &proc0_info, sizeof proc0_info);
      (*count)++;
   }
   return processes;
}


ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList, uid_t userId) {
   DarwinProcessList* this = xCalloc(1, sizeof(DarwinProcessList));

   ProcessList_init(&this->super, Class(Process), usersTable, pidWhiteList, userId);

   /* Initialize the CPU information */
   this->super.cpuCount = ProcessList_allocateCPULoadInfo(&this->prev_load);
   ProcessList_getHostInfo(&this->host_info);
   ProcessList_allocateCPULoadInfo(&this->curr_load);

   /* Initialize the VM statistics */
   ProcessList_getVMStats(&this->vm_stats);

   this->super.totalTasks = 0;
   this->super.thread_count = 0;
   this->super.kernel_process_count = 0;
   this->super.kernel_thread_count = 0;
   this->super.running_thread_count = 0;

   return &this->super;
}

void ProcessList_delete(ProcessList* this) {
   ProcessList_done(this);
   free(this);
}

void ProcessList_goThroughEntries(ProcessList* super) {
	DarwinProcessList *dpl = (DarwinProcessList *)super;
	struct kinfo_proc *ps;
	size_t count;
	struct timeval now;

	gettimeofday(&now, NULL); /* Start processing time */

	/* Update the global data (CPU times and VM stats) */
	ProcessList_freeCPULoadInfo(&dpl->prev_load);
	dpl->prev_load = dpl->curr_load;
	ProcessList_allocateCPULoadInfo(&dpl->curr_load);
	ProcessList_getVMStats(&dpl->vm_stats);

	/* Get the time difference */
	dpl->global_diff = 0;
	for(int i = 0; i < dpl->super.cpuCount; ++i) {
		for(size_t j = 0; j < CPU_STATE_MAX; ++j) {
			dpl->global_diff += dpl->curr_load[i].cpu_ticks[j] - dpl->prev_load[i].cpu_ticks[j];
		}
	}

	/* We use kinfo_procs for initial data since :
	 *
	 * 1) They always succeed.
	 * 2) The contain the basic information.
	 *
	 * We attempt to fill-in additional information with libproc.
	 */
	ps = ProcessList_getKInfoProcs(&count);

	for(size_t i = 0; i < count; ++i) {
		bool preExisting;
		const struct kinfo_proc *info = ps + i;
		DarwinProcess *proc = (DarwinProcess *)ProcessList_getProcess(super, info->kp_proc.p_pid, &preExisting, (Process_New)DarwinProcess_new);

		DarwinProcess_setFromKInfoProc(&proc->super, info, now.tv_sec, preExisting);
		DarwinProcess_setFromLibprocPidinfo(proc, dpl);

		super->totalTasks++;
		if(Process_isKernelProcess(&proc->super)) {
			super->kernel_process_count++;
			if(super->settings->hide_kernel_processes) proc->super.show = false;
		}
		super->running_process_count++;

		// Disabled for High Sierra due to bug in macOS High Sierra
		bool isScanThreadSupported  = ! ( CompareKernelVersion(17, 0, 0) >= 0 && CompareKernelVersion(17, 5, 0) < 0);

		if (isScanThreadSupported){
			DarwinProcess_scanThreads(proc);
		}
		if(!proc->super.real_user) {
			proc->super.real_user = UsersTable_getRef(super->usersTable, proc->super.ruid);
		}
		if(!proc->super.effective_user) {
			proc->super.effective_user = UsersTable_getRef(super->usersTable, proc->super.euid);
		}
		if(!preExisting) {
			ProcessList_add(super, &proc->super);
		}
	}

	free(ps);
}
