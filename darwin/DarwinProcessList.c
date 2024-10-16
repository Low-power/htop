/*
htop - darwin/DarwinProcessList.c
(C) 2014 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "DarwinProcess.h"
#include "DarwinProcessList.h"
#include "CRT.h"
#include <mach/mach_init.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

struct kern {
	short int version[3];
};

static void GetKernelVersion(struct kern *k) {
	static short int version_[3] = { -1, -1, -1 };
	if (version_[0] < 0) {
		char buffer[256];
		size_t size = sizeof buffer;
		*buffer = 0;
		if(sysctlbyname("kern.osrelease", buffer, &size, NULL, 0) == 0) {
			sscanf(buffer, "%hd.%hd.%hd", version_, version_ + 1, version_ + 2);
		}
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
#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct DarwinProcessList_ {
   ProcessList super;

   struct kinfo_proc *kip_buffer;
   size_t kip_buffer_size;
   struct host_basic_info host_info;
   struct vm_statistics vm_stats;
   struct processor_cpu_load_info *prev_load;
   struct processor_cpu_load_info *curr_load;
   uint64_t global_diff;
   bool is_mach_task_info_supported;
} DarwinProcessList;

}*/

static void ProcessList_getHostInfo(struct host_basic_info *p) {
   mach_msg_type_number_t info_size = HOST_BASIC_INFO_COUNT;
   int e = host_info(mach_host_self(), HOST_BASIC_INFO, (host_info_t)p, &info_size);
   if(e) CRT_fatalError("Unable to retrieve host info", e << 8);
}

static void ProcessList_freeCPULoadInfo(struct processor_cpu_load_info **p) {
	if(p && *p) {
		if(munmap(*p, vm_page_size) < 0) {
			CRT_fatalError("Unable to free old CPU load information", 0);
		}
		*p = NULL;
	}
}

static unsigned int ProcessList_allocateCPULoadInfo(struct processor_cpu_load_info **p) {
   mach_msg_type_number_t info_size = sizeof(struct processor_cpu_load_info *);
   unsigned int cpu_count;
   int e = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &cpu_count, (processor_info_array_t *)p, &info_size);
   if(e) CRT_fatalError("Unable to retrieve CPU info", e << 8);
   return cpu_count;
}

static void ProcessList_getVMStats(struct vm_statistics *p) {
	mach_msg_type_number_t info_size = HOST_VM_INFO_COUNT;
	int e = host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)p, &info_size);
	if(e) CRT_fatalError("Unable to retrieve VM statistics", e << 8);
}

static size_t ProcessList_updateProcessList(DarwinProcessList *this) {
   /* Note the two calls to sysctl(3). One to get length and one to get
    * the data. This -does- mean that the second call could end up with
    * a missing process entry or two; if this happens sysctl(3) will
    * apparently fail with ENOMEM, therefore we must ignore it.
    */
   int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
   size_t buffer_size;
   if (sysctl(mib, 3, NULL, &buffer_size, NULL, 0) < 0) {
      CRT_fatalError("Unable to get size of process list", 0);
   }
   if(this->kip_buffer_size < buffer_size) {
      free(this->kip_buffer);
      this->kip_buffer = xMalloc(buffer_size);
      this->kip_buffer_size = buffer_size;
   }
   if (sysctl(mib, 3, this->kip_buffer, &buffer_size, NULL, 0) < 0 && errno != ENOMEM) {
      CRT_fatalError("Unable to get process list", 0);
   }
   size_t count = buffer_size / sizeof(struct kinfo_proc);

   // Check for process 0 (kernel_task)
   for(size_t i = 0; i < count; i++) {
      if(this->kip_buffer[i].kp_proc.p_pid == 0) return count;
   }

   mib[2] = KERN_PROC_PID;
   struct kinfo_proc proc0_info;
   size_t info_size = sizeof proc0_info;
   if(sysctl(mib, 4, &proc0_info, &info_size, NULL, 0) == 0 && info_size == sizeof proc0_info) {
      buffer_size += sizeof(struct kinfo_proc);
      if(this->kip_buffer_size < buffer_size) {
         this->kip_buffer = xRealloc(this->kip_buffer, buffer_size);
         this->kip_buffer_size = buffer_size;
      }
      memcpy(this->kip_buffer + count, &proc0_info, sizeof proc0_info);
      count++;
   }
   return count;
}


ProcessList* ProcessList_new(UsersTable* usersTable, const Hashtable *pidWhiteList, uid_t userId) {
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

   // Disabled for High Sierra due to bug in macOS High Sierra
   this->is_mach_task_info_supported = !(CompareKernelVersion(17, 0, 0) >= 0 && CompareKernelVersion(17, 5, 0) < 0);

   return &this->super;
}

void ProcessList_delete(ProcessList* this) {
   ProcessList_done(this);
   free(((DarwinProcessList *)this)->kip_buffer);
   free(this);
}

void ProcessList_goThroughEntries(ProcessList* super, bool skip_processes) {
	DarwinProcessList *dpl = (DarwinProcessList *)super;

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

	if(skip_processes) return;

	/* We use kinfo_procs for initial data since :
	 *
	 * 1) They always succeed.
	 * 2) The contain the basic information.
	 *
	 * We attempt to fill-in additional information with libproc.
	 */
	size_t count = ProcessList_updateProcessList(dpl);

	for(size_t i = 0; i < count; i++) {
		bool preExisting;
		const struct kinfo_proc *info = dpl->kip_buffer + i;
		DarwinProcess *proc = (DarwinProcess *)ProcessList_getProcess(super, info->kp_proc.p_pid, &preExisting, (Process_New)DarwinProcess_new);

		DarwinProcess_setFromKInfoProc(&proc->super, info, super, now.tv_sec, preExisting);
		DarwinProcess_setFromLibprocPidinfo(proc, dpl);

		super->totalTasks++;
		if(Process_isKernelProcess(&proc->super)) {
			super->kernel_process_count++;
			if(super->settings->hide_kernel_processes) proc->super.show = false;
		}
		super->running_process_count++;

		if(dpl->is_mach_task_info_supported) {
			DarwinProcess_setFromMachTaskInfo(proc, dpl);
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
}
