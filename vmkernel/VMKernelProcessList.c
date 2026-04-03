/*
htop - vmkernel/VMKernelProcessList.c
(C) 2014 Hisham H. Muhammad
Copyright 2015-2026 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "ProcessList.h"

typedef struct {
	ProcessList super;

	uint32_t vsi_world_id;
	uint32_t vsi_world_info_id;
	uint32_t vsi_world_name_id;
	uint32_t vsi_sched_id;
	uint32_t vsi_sched_memclients_id;
	uint32_t vsi_sched_memclients_memstats_id;
	uint32_t vsi_sched_memclients_memstats_common_id;
	uint32_t vsi_sched_memclients_memstats_uw_id;
	uint32_t vsi_sched_cpuclients_id;
	uint32_t vsi_sched_cpuclients_numvcpus_id;
	uint32_t vsi_userworld_id;
	uint32_t vsi_userworld_cartel_id;
	uint32_t vsi_userworld_cartel_cmdline_id;
	uint32_t vsi_memory_id;
	uint32_t vsi_memory_comprehensive_id;
	uint64_t vsi_world_cksum;
	uint64_t vsi_world_info_cksum;
	uint64_t vsi_world_name_cksum;
	uint64_t vsi_sched_cksum;
	uint64_t vsi_sched_memclients_cksum;
	uint64_t vsi_sched_memclients_memstats_cksum;
	uint64_t vsi_sched_memclients_memstats_common_cksum;
	uint64_t vsi_sched_memclients_memstats_uw_cksum;
	uint64_t vsi_sched_cpuclients_cksum;
	uint64_t vsi_sched_cpuclients_numvcpus_cksum;
	uint64_t vsi_userworld_cksum;
	uint64_t vsi_userworld_cartel_cksum;
	uint64_t vsi_userworld_cartel_cmdline_cksum;
	uint64_t vsi_memory_cksum;
	uint64_t vsi_memory_comprehensive_cksum;

	size_t world_info_size;
} VMKernelProcessList;
}*/

#include "VMKernelProcessList.h"
#include "VMKernelProcess.h"
#include "StringUtils.h"
#include "Settings.h"
#include "Platform.h"
#include "CRT.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

struct world_info {
	uint32_t world_id;
	uint32_t group_id;
	uint32_t userspace_id;
	uint32_t cartel_id;
	uint32_t parent_cartel_id;
	uint32_t cartel_group_id;
	uint32_t session_id;
	uint32_t flags;
	uint32_t ref_count;
	uint32_t reap_calls;
	uint8_t scsi_active;
	uint8_t security_domain;
#if 0
	uint8_t _unknown[26];
#else
	uint8_t _unknown[30];
#endif
};

// Size values are in kibibyte
struct memstats_common_32 {
	uint32_t reservation;
	uint32_t limit;
	uint32_t reservation_limit;
	int32_t shares;
	uint32_t max_mem_size;
	uint32_t size;
	uint32_t touched;
	uint32_t written;
	uint32_t current_size;
	uint32_t target_size;
	uint32_t overhead;
	uint32_t overhead_touched;
	uint32_t overhead_max;
	uint32_t zero;
	uint32_t poison;
	uint32_t swapped;
	uint32_t zipped;
	uint32_t llswapped;
	uint32_t llswap_used;
	uint32_t mapped;
	uint32_t copy_on_write_mapped;
	uint32_t shared_saved;
	uint32_t zip_saved;
	uint32_t commit_target;
	uint32_t swap_target;
	uint64_t pages_per_share;
	uint32_t swap_scope;
	uint32_t _reserved_1;
	uint8_t use_reliable_mem;
	uint32_t min_commit_target;
	uint32_t commit_charged;
	union {
		struct {
			uint8_t client_responsive;
		} __attribute__((__packed__)) v5;
		struct {
			uint32_t _reserved_2;
			uint8_t client_responsive;
			uint8_t _reserved_3[2];
		} __attribute__((__packed__)) v6;
	};
} __attribute__((__packed__));

// Size values are in kibibyte
struct memstats_common_64 {
	uint64_t reservation;
	uint64_t limit;
	uint64_t reservation_limit;
	int32_t shares;
	uint64_t max_mem_size;
	uint64_t size;
	uint64_t touched;
	uint64_t written;
	uint64_t current_size;
	uint64_t target_size;
	uint64_t overhead;
	uint64_t overhead_touched;
	uint64_t overhead_max;
	uint64_t zero;
	uint64_t poison;
	uint64_t swapped;
	uint64_t zipped;
	uint64_t llswapped;
	uint64_t ro_vpmem;
	uint64_t rw_vpmem;
	uint64_t uncached_vpmem;
	uint64_t llswap_used;
	uint64_t mapped;
	uint64_t copy_on_write_mapped;
	uint64_t shared_saved;
	uint64_t zip_saved;
	uint64_t commit_target;
	uint64_t swap_target;
	uint64_t pages_per_share;
	uint32_t swap_scope;
	uint32_t _reserved_1;
	uint8_t use_reliable_mem;
	uint64_t consumed;
	uint64_t min_commit_target;
	uint64_t commit_charged;
	uint8_t client_responsive;
	uint8_t _reserved_2[2];
} __attribute__((__packed__));

#if 0
struct memstats_uw_64 {
	struct memstats_common64 common;
	uint64_t total_reserve;
	uint64_t pinned_reserve;
	uint64_t mem_bstore;
	uint64_t swap_bstore;
	uint64_t swappable;
	uint64_t shm;
	uint64_t cow;
	uint64_t pageable;
	uint64_t pageable_unacct;
	uint64_t swapped;
	uint64_t pinned;
	uint64_t prealloced;
} __attribute__((__packed__));
#endif

// All values are in kibibyte
struct comprehensive_memory_statistics {
	uint64_t physical_memory_estimate;
	uint64_t given_to_vmkernel;
	uint64_t reliable_memory;
	uint64_t discarded_by_vmkernel;
	uint64_t mmap_critical_space;
	uint64_t mmap_buddy_overhead;
	uint64_t kernel_code;
	uint64_t kernel_data_and_heap;
	uint64_t kernel_other;
	uint64_t non_kernel;
	uint64_t reserved_low;
	uint64_t free;
};

// All values are in kibibyte
struct comprehensive_memory_statistics_6_7 {
	uint64_t physical_memory_estimate;
	uint64_t given_to_vmkernel;
	uint64_t reliable_memory;
	uint64_t discarded_by_vmkernel;
	uint64_t mmap_buddy_overhead;
	uint64_t kernel_code;
	uint64_t kernel_data_and_heap;
	uint64_t kernel_other;
	uint64_t non_kernel;
	uint64_t reserved_low;
	uint64_t free;
};

ProcessList* ProcessList_new(UsersTable* usersTable, const Hashtable *pidWhiteList, uid_t userId) {
   VMKernelProcessList *this = xCalloc(1, sizeof(VMKernelProcessList));
   ProcessList_init(&this->super, Class(VMKernelProcess), usersTable, pidWhiteList, userId);

   int e = Platform_vsiGetNodeIdAndChecksum(0, "world", &this->vsi_world_id, &this->vsi_world_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo world", e);
   e = Platform_vsiGetNodeIdAndChecksum(this->vsi_world_id, "info",
      &this->vsi_world_info_id, &this->vsi_world_info_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo world.info", e);
   e = Platform_vsiGetNodeIdAndChecksum(this->vsi_world_id, "name",
      &this->vsi_world_name_id, &this->vsi_world_name_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo world.name", e);
   e = Platform_vsiGetNodeIdAndChecksum(0, "sched", &this->vsi_sched_id, &this->vsi_sched_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched", e);
   e = Platform_vsiGetNodeIdAndChecksum(this->vsi_sched_id, "memClients",
      &this->vsi_sched_memclients_id, &this->vsi_sched_memclients_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.memClients", e);
   e = Platform_vsiGetNodeIdAndChecksum(this->vsi_sched_memclients_id, "memStats",
      &this->vsi_sched_memclients_memstats_id, &this->vsi_sched_memclients_memstats_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.memClients.memStats", e);
   e = Platform_vsiGetNodeIdAndChecksum(this->vsi_sched_memclients_memstats_id, "totalCommon",
      &this->vsi_sched_memclients_memstats_common_id, &this->vsi_sched_memclients_memstats_common_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.memClients.memStats.totalCommon", e);
   e = Platform_vsiGetNodeIdAndChecksum(this->vsi_sched_memclients_memstats_id, "uw",
      &this->vsi_sched_memclients_memstats_uw_id, &this->vsi_sched_memclients_memstats_uw_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.memClients.memStats.uw", e);
   e = Platform_vsiGetNodeIdAndChecksum(this->vsi_sched_id, "cpuClients",
      &this->vsi_sched_cpuclients_id, &this->vsi_sched_cpuclients_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.cpuClients", e);
   e = Platform_vsiGetNodeIdAndChecksum(this->vsi_sched_cpuclients_id, "numVcpus",
      &this->vsi_sched_cpuclients_numvcpus_id, &this->vsi_sched_cpuclients_numvcpus_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.cpuClients.numVcpus", e);
   e = Platform_vsiGetNodeIdAndChecksum(0, "userworld",
      &this->vsi_userworld_id, &this->vsi_userworld_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo userworld", e);
   e = Platform_vsiGetNodeIdAndChecksum(this->vsi_userworld_id, "cartel",
      &this->vsi_userworld_cartel_id, &this->vsi_userworld_cartel_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo userworld.cartel", e);
   e = Platform_vsiGetNodeIdAndChecksum(this->vsi_userworld_cartel_id, "cmdline",
      &this->vsi_userworld_cartel_cmdline_id, &this->vsi_userworld_cartel_cmdline_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo userworld.cartel.cmdline", e);
   e = Platform_vsiGetNodeIdAndChecksum(0, "memory", &this->vsi_memory_id, &this->vsi_memory_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo memory", e);
   e = Platform_vsiGetNodeIdAndChecksum(this->vsi_memory_id, "comprehensive",
      &this->vsi_memory_comprehensive_id, &this->vsi_memory_comprehensive_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo memory.comprehensive", e);

   Platform_checkVMkernelVersion();
   switch(Platform_running_vmkernel_version) {
      case VMKERNEL_VERSION_5_5:
      case VMKERNEL_VERSION_6_7:
         this->world_info_size = 68;
         break;
      case VMKERNEL_VERSION_6_0:
      case VMKERNEL_VERSION_6_5:
         this->world_info_size = 72;
         break;
   }

   return &this->super;
}

void ProcessList_delete(ProcessList* this) {
   ProcessList_done(this);
   free(this);
}

static struct vsi_list *allocate_vsi_list(uint32_t count, uint32_t string_size) {
	size_t list_size = sizeof(struct vsi_list) + sizeof(struct vsi_param) * count + string_size;
	struct vsi_list *list = xMalloc(list_size);
	memset(list, 0, list_size);
	list->type_or_version = 1;
	list->allocated_count = count;
	list->param_size = sizeof(struct vsi_list) + sizeof(struct vsi_param) * count;
	list->string_size = string_size;
	if(string_size) list->string_offset = list->param_size;
	list->self_ptr = (uintptr_t)list;
	return list;
}

static void get_global_memory_stats(VMKernelProcessList *this) {
	struct vsi_list list = {
		.type_or_version = 1, .param_size = sizeof(struct vsi_list), .self_ptr = (uintptr_t)&list
	};
	switch(Platform_running_vmkernel_version) {
			struct comprehensive_memory_statistics stats;
			struct comprehensive_memory_statistics_6_7 stats_6_7;
			int e;
		case VMKERNEL_VERSION_5_5:
		case VMKERNEL_VERSION_6_0:
		case VMKERNEL_VERSION_6_5:
			e = Platform_vsiGet(this->vsi_memory_comprehensive_id, this->vsi_memory_comprehensive_cksum,
				&list, &stats, sizeof stats);
			if(e) goto failure;
			this->super.totalMem = stats.physical_memory_estimate;
			this->super.freeMem = stats.free;
			this->super.usedMem = stats.physical_memory_estimate - stats.free;
			break;
		case VMKERNEL_VERSION_6_7:
			e = Platform_vsiGet(this->vsi_memory_comprehensive_id, this->vsi_memory_comprehensive_cksum,
				&list, &stats_6_7, sizeof stats_6_7);
			if(e) goto failure;
			this->super.totalMem = stats_6_7.physical_memory_estimate;
			this->super.freeMem = stats_6_7.free;
			this->super.usedMem = stats_6_7.physical_memory_estimate - stats_6_7.free;
			break;
		failure:
			this->super.totalMem = 0;
			this->super.freeMem = 0;
			this->super.usedMem = 0;
			break;
		default:
			abort();
	}
}

static bool get_world_info(const VMKernelProcessList *this, Process *proc) {
	struct vsi_list *list = allocate_vsi_list(1, 0);
	Platform_vsiListAddInt(list, proc->pid);
	struct world_info info;
	assert(this->world_info_size <= sizeof info);
	int e = Platform_vsiGet(this->vsi_world_info_id, this->vsi_world_info_cksum,
		list, &info, this->world_info_size);
	free(list);
	if(e) return false;
	VMKernelProcess *vmk_proc = (VMKernelProcess *)proc;
	vmk_proc->group_id = info.group_id;
	vmk_proc->userspace_id = info.userspace_id;
	proc->tgid = info.cartel_id;
	proc->ppid = info.parent_cartel_id;
	vmk_proc->cartel_group_id = info.cartel_group_id;
	proc->session = info.session_id;
	vmk_proc->is_kernel_process = info.flags & WORLD_SYSTEM;
	return true;
}

static void get_world_name(const VMKernelProcessList *this, Process *proc) {
	struct vsi_list *list = allocate_vsi_list(1, 0);
	Platform_vsiListAddInt(list, proc->pid);
	char buffer[128];
	int e = Platform_vsiGet(this->vsi_world_name_id, this->vsi_world_name_cksum,
		list, buffer, sizeof buffer);
	free(list);
	proc->name = xStrdup(e ? "" : buffer);
}

static bool get_cartel_command(const VMKernelProcessList *this, Process *proc) {
	if(proc->tgid <= 0) return false;
	struct vsi_list *list = allocate_vsi_list(1, 0);
	Platform_vsiListAddInt(list, proc->tgid);
	char buffer[1024];
	int e = Platform_vsiGet(this->vsi_userworld_cartel_cmdline_id,
		this->vsi_userworld_cartel_cmdline_cksum, list, buffer, sizeof buffer);
	free(list);
	if(e) return false;
	proc->comm = xStrdup(buffer);
	return true;
}

static void get_memory_stats(const VMKernelProcessList *this, Process *proc) {
	struct vsi_list *list = allocate_vsi_list(1, 0);
	Platform_vsiListAddInt(list, proc->pid);
	switch(Platform_running_vmkernel_version) {
			struct memstats_common_32 common32;
			struct memstats_common_64 common64;
			int e;
		case VMKERNEL_VERSION_5_5:
			assert(128 < sizeof common32);
			e = Platform_vsiGet(this->vsi_sched_memclients_memstats_common_id,
				this->vsi_sched_memclients_memstats_common_cksum, list,
				&common32, 128);
			goto copy_common32_values;
		case VMKERNEL_VERSION_6_0:
			assert(132 == sizeof common32);
			e = Platform_vsiGet(this->vsi_sched_memclients_memstats_common_id,
				this->vsi_sched_memclients_memstats_common_cksum, list,
				&common32, 132);
		copy_common32_values:
			if(e) goto failure;
			proc->m_size = common32.size / CRT_page_size_kibibyte;
			proc->m_resident = common32.mapped / CRT_page_size_kibibyte;
			break;
		case VMKERNEL_VERSION_6_5:
		case VMKERNEL_VERSION_6_7:
			e = Platform_vsiGet(this->vsi_sched_memclients_memstats_common_id,
				this->vsi_sched_memclients_memstats_common_cksum, list,
				&common64, sizeof common64);
			if(e) goto failure;
			proc->m_size = common64.size / CRT_page_size_kibibyte;
			proc->m_resident = common64.mapped / CRT_page_size_kibibyte;
			break;
		failure:
			proc->m_size = 0;
			proc->m_resident = 0;
			break;
		default:
			abort();
	}
	free(list);
}

static void get_vcpu_stats(const VMKernelProcessList *this, VMKernelProcess *proc) {
	struct vsi_list *list = allocate_vsi_list(1, 0);
	Platform_vsiListAddInt(list, proc->super.pid);
	int e = Platform_vsiGet(this->vsi_sched_cpuclients_numvcpus_id,
		this->vsi_sched_cpuclients_numvcpus_cksum, list,
		&proc->vcpu_count, sizeof proc->vcpu_count);
	free(list);
	if(e) proc->vcpu_count = 0;
}

void ProcessList_goThroughEntries(ProcessList *super, bool skip_processes) {
	VMKernelProcessList *this = (VMKernelProcessList *)super;

	get_global_memory_stats(this);

	struct vsi_list *worlds_list = xMalloc(sizeof(struct vsi_list));
	memset(worlds_list, 0, sizeof(struct vsi_list));
	worlds_list->type_or_version = 1;
	worlds_list->param_size = sizeof(struct vsi_list);
	worlds_list->self_ptr = (uintptr_t)worlds_list;
	int e = Platform_vsiGetList(this->vsi_world_id, this->vsi_world_cksum,
		worlds_list, sizeof(struct vsi_list));
	if(e) CRT_fatalError("VSI_GetList", e);
	size_t count = worlds_list->instance_count;
	worlds_list->instance_count = 0;
	size_t param_size = sizeof(struct vsi_list) + sizeof(struct vsi_param) * count;
	size_t string_size = 128 * count;
	size_t list_size = param_size + string_size;
	worlds_list = xRealloc(worlds_list, list_size);
	worlds_list->allocated_count = count;
	worlds_list->param_size = param_size;
	worlds_list->string_size = string_size;
	worlds_list->string_offset = param_size;
	worlds_list->self_ptr = (uintptr_t)worlds_list;
	e = Platform_vsiGetList(this->vsi_world_id, this->vsi_world_cksum, worlds_list, list_size);
	if(e) CRT_fatalError("VSI_GetList", e);
	for(size_t i = 0; i < worlds_list->instance_count; i++) {
		pid_t pid = Platform_vsiListGetValue(worlds_list, i);
		bool is_existing;
		Process *proc = ProcessList_getProcess(super, pid, &is_existing, (Process_New)VMKernelProcess_new);
		if(!get_world_info(this, proc)) {
			if(!is_existing) Process_delete((Object *)proc);
			continue;
		}
		if(!is_existing) {
			proc->state = '?';
			ProcessList_add(super, proc);
		}
		get_memory_stats(this, proc);
		get_vcpu_stats(this, (VMKernelProcess *)proc);
		if(!is_existing || ProcessList_shouldUpdateProcessNames(super)) {
			free(proc->name);
			free(proc->comm);
			get_world_name(this, proc);
			if(!get_cartel_command(this, proc)) proc->comm = xStrdup(proc->name);
		}
		if(Process_isKernelProcess(proc)) {
			super->kernel_process_count++;
			super->kernel_thread_count++;
			if(proc->tgid == 0) proc->tgid = pid;
		}
		super->totalTasks++;
		proc->updated = true;
	}
	free(worlds_list);
}
