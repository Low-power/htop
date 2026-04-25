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
	uint64_t total_used_time;
	uint64_t wdt_time;
	uint64_t kernel_nw_time;
	uint32_t total_used_period;
	uint32_t wdt_period;
	uint32_t kernel_nw_period;
} VMKernelCPUStatistics;

typedef struct {
	ProcessList super;

	size_t world_info_size;
	size_t pcpu_info_size;
	struct timeval last_updated;
	uint64_t interval_usec;

	VMKernelCPUStatistics *cpu_stats;
} VMKernelProcessList;
}*/

#include "VMKernelProcessList.h"
#include "VMKernelProcess.h"
#include "StringUtils.h"
#include "Settings.h"
#include "Platform.h"
#include "CRT.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#define WORLD_SYSTEM 0x01
#define WORLD_IDLE 0x02
#define WORLD_USER 0x04
#define WORLD_VMM 0x08
#define WORLD_HELPER 0x10
#define WORLD_CLONE 0x20
#define WORLD_TEST 0x40
#define WORLD_UWVCPU 0x80
#define WORLD_ASSISTANT 0x100
#define WORLD_UTILITY_VM 0x200

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

struct sched_allocation {
	uint32_t min;
	uint32_t max;
	uint32_t shares;
	uint32_t min_limits;
	uint32_t units;
};

struct vcpu_load_metrics {
	uint64_t inter_wait_run_avg;
	uint64_t sleep_avg;
	uint32_t cache_miss_rate;
} __attribute__((__packed__));

struct vcpu_stats_5_5 {
	uint32_t world_flags;
	uint32_t run_state;
	uint32_t wait_state;
	uint8_t paused;
	uint32_t vmm_world_id;
	uint32_t active_world_id;
	uint32_t ht_sharing;
	struct sched_allocation normalized_allocated;
	struct sched_allocation internal_allocated;
	uint32_t pcpu;
	uint32_t effective_min;
	uint8_t ht_quarantine;
	uint8_t group_max_enforced;
	uint64_t ht_stolen_time;
	uint64_t vtime_main;
	uint64_t vtime_extra;
	uint64_t vtime_limit;
	uint64_t vtime_ahead;
	struct vcpu_load_metrics load_information;
	uint64_t cpu_latency;
	uint64_t mem_swap_fault_time;
	uint32_t mem_swap_fault_count;
	uint64_t mem_compress_fault_time;
	uint32_t mem_compress_fault_count;
	uint8_t _reserved[53];
} __attribute__((__packed__));

struct vcpu_stats_6_5 {
	uint32_t world_flags;
	uint32_t run_state;
	uint32_t wait_state;
	uint32_t vmm_world_id;
	uint32_t active_world_id;
	struct sched_allocation normalized_allocated;
	struct sched_allocation internal_allocated;
	uint32_t pcpu;
	uint64_t _reserved[24];
} __attribute__((__packed__));

struct vcpu_state_times_5_0 {
	uint64_t up_time;
	uint64_t used_time;
	uint64_t system_time;
	uint64_t system_overlap_time;
	uint64_t run_time;
	uint64_t wait_time;
	uint64_t co_stop_time;
	uint64_t idle_time;
	uint64_t ready_time;
	uint64_t max_limited_time;
	uint64_t paused_time;
	uint64_t vmkernel_call_time;
	uint64_t guest_progress_time;
	uint64_t sticky_time;
	uint64_t active_sticky_time;
};

struct vcpu_state_times_6_0 {
	uint64_t up_time;
	uint64_t used_time;
	uint64_t system_time;
	uint64_t system_overlap_time;
	uint64_t system_distributed_time;
	uint64_t run_time;
	uint64_t wait_time;
	uint64_t co_stop_time;
	uint64_t idle_time;
	uint64_t ready_time;
	uint64_t max_limited_time;
	uint64_t paused_time;
	uint64_t vmkernel_call_time;
	uint64_t guest_progress_time;
	uint64_t sticky_time;
	uint64_t active_sticky_time;
};

struct vcpu_state_times_6_5 {
	uint64_t up_time;
	uint64_t used_time;
	uint64_t system_time;
	uint64_t system_overlap_time;
	uint64_t system_distributed_time;
	uint64_t run_time;
	uint64_t wait_time;
	uint64_t co_stop_time;
	uint64_t idle_time;
	uint64_t ready_time;
	uint64_t max_limited_time;
	uint64_t vmkernel_call_time;
	uint64_t guest_progress_time;
	uint64_t sticky_time;
	uint64_t active_sticky_time;
};

struct pcpu_info {
	uint64_t used_time;
	uint64_t idle_time;
	uint64_t wdt_time;
	uint64_t busy_wait_time;
	uint64_t halt_time;
	uint64_t core_halt_time;
	uint64_t elapsed_time;
	uint64_t system_time;
	uint8_t is_halted;
	uint32_t exclusive_affinity;
	uint32_t numa_node;
	uint32_t dispatch_count;
	uint32_t use_whole_core;
	uint32_t preempts;
	uint32_t timer_intrs;
	uint32_t inter_processor_interrupts;
	uint32_t handoff;
	uint32_t handoff_failed;
	uint32_t handoff_switch_failed;
	uint32_t remote_req_add_failed;
	uint32_t idle_halt_end;
	uint32_t idle_halt_end_interrupts;
	uint32_t retry_dispatch;
	uint8_t _reserved[51];
} __attribute__((__packed__));

struct number_of_pcpus {
	uint32_t ncpus;
	uint32_t ncores;
	uint32_t npackages;
};

static const char vcpu_run_state_map_5_0[] = { 'N', 'Z', 'O', 'R', 'R', 'W' };
static const char vcpu_run_state_map_6_5[] = { 'O', 'R', 'W', 'R', 'N', 'Z' };

ProcessList* ProcessList_new(UsersTable* usersTable, const Hashtable *pidWhiteList, uid_t userId) {
   VMKernelProcessList *this = xCalloc(1, sizeof(VMKernelProcessList));
   ProcessList_init(&this->super, Class(VMKernelProcess), usersTable, pidWhiteList, userId);

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
   switch(Platform_running_vmkernel_version) {
      case VMKERNEL_VERSION_5_5:
      case VMKERNEL_VERSION_6_5:
      case VMKERNEL_VERSION_6_7:
         this->pcpu_info_size = 172;
         break;
      case VMKERNEL_VERSION_6_0:
         this->pcpu_info_size = 156;
         break;
   }

   struct vsi_list list = {
      .type_or_version = 1, .param_size = sizeof(struct vsi_list), .self_ptr = (uintptr_t)&list
   };
   struct number_of_pcpus npcpus;
   int e = Platform_vsiGet(platform.sched_globalstats_numpcpus_id, platform.sched_globalstats_numpcpus_cksum,
      &list, &npcpus, sizeof npcpus);
   if(e) CRT_fatalError("VSI_Get sched.globalStats.numPcpus", e);
   this->super.cpuCount = npcpus.ncpus;
   this->cpu_stats = xCalloc(npcpus.ncpus, sizeof(VMKernelCPUStatistics));
   platform.ncores = npcpus.ncores;

   return &this->super;
}

void ProcessList_delete(ProcessList* this) {
   ProcessList_done(this);
   free(this);
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
			e = Platform_vsiGet(platform.memory_comprehensive_id, platform.memory_comprehensive_cksum,
				&list, &stats, sizeof stats);
			if(e) goto failure;
			this->super.totalMem = stats.physical_memory_estimate;
			this->super.freeMem = stats.free;
			this->super.usedMem = stats.physical_memory_estimate - stats.free;
			break;
		case VMKERNEL_VERSION_6_7:
			e = Platform_vsiGet(platform.memory_comprehensive_id, platform.memory_comprehensive_cksum,
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

static void get_global_cpu_stats(VMKernelProcessList *this) {
	struct vsi_list *list = Platform_allocateVsiList(1, 0);
	Platform_vsiListAddInt(list, 0);
	for(int i = 0; i < this->super.cpuCount; i++) {
		VMKernelCPUStatistics *htop_cpu_stats = this->cpu_stats + i;
		Platform_vsiListSetValue(list, 0, i);
		struct pcpu_info info;
		assert(this->pcpu_info_size <= sizeof info);
		int e = Platform_vsiGet(platform.sched_pcpus_stats_id, platform.sched_pcpus_stats_cksum, list, &info, this->pcpu_info_size);
		if(e) {
			htop_cpu_stats->total_used_period = 0;
			htop_cpu_stats->wdt_period = 0;
			htop_cpu_stats->kernel_nw_period = 0;
		} else {
#define DIFF(A,B) ((A)>(B)?((A)-(B)):0)
			htop_cpu_stats->total_used_period =
				DIFF(info.used_time, htop_cpu_stats->total_used_time);
			htop_cpu_stats->wdt_period = DIFF(info.wdt_time, htop_cpu_stats->wdt_time);
			htop_cpu_stats->kernel_nw_period =
				DIFF(info.system_time, htop_cpu_stats->kernel_nw_time);
			if(htop_cpu_stats->total_used_period < htop_cpu_stats->wdt_period + htop_cpu_stats->kernel_nw_period) {
				htop_cpu_stats->total_used_period =
					htop_cpu_stats->wdt_period + htop_cpu_stats->kernel_nw_period;
			}
#undef DIFF
			htop_cpu_stats->total_used_time = info.used_time;
			htop_cpu_stats->wdt_time = info.wdt_time;
			htop_cpu_stats->kernel_nw_time = info.system_time;
		}
	}
	free(list);
}

static bool get_world_info(const VMKernelProcessList *this, Process *proc) {
	struct vsi_list *list = Platform_allocateVsiList(1, 0);
	Platform_vsiListAddInt(list, proc->pid);
	struct world_info info;
	assert(this->world_info_size <= sizeof info);
	int e = Platform_vsiGet(platform.world_info_id, platform.world_info_cksum,
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
	struct vsi_list *list = Platform_allocateVsiList(1, 0);
	Platform_vsiListAddInt(list, proc->pid);
	char buffer[128];
	int e = Platform_vsiGet(platform.world_name_id, platform.world_name_cksum,
		list, buffer, sizeof buffer);
	free(list);
	proc->name = xStrdup(e ? "" : buffer);
}

static bool get_cartel_command(const VMKernelProcessList *this, Process *proc) {
	if(proc->tgid <= 0) return false;
	struct vsi_list *list = Platform_allocateVsiList(1, 0);
	Platform_vsiListAddInt(list, proc->tgid);
	char buffer[1024];
	int e = Platform_vsiGet(platform.userworld_cartel_cmdline_id,
		platform.userworld_cartel_cmdline_cksum, list, buffer, sizeof buffer);
	free(list);
	if(e) return false;
	proc->comm = xStrdup(buffer);
	return true;
}

static void get_memory_stats(const VMKernelProcessList *this, Process *proc) {
	struct vsi_list *list = Platform_allocateVsiList(1, 0);
	Platform_vsiListAddInt(list, proc->pid);
	switch(Platform_running_vmkernel_version) {
			struct memstats_common_32 common32;
			struct memstats_common_64 common64;
			int e;
		case VMKERNEL_VERSION_5_5:
			assert(128 < sizeof common32);
			e = Platform_vsiGet(platform.sched_memclients_memstats_common_id,
				platform.sched_memclients_memstats_common_cksum, list,
				&common32, 128);
			goto copy_common32_values;
		case VMKERNEL_VERSION_6_0:
			assert(132 == sizeof common32);
			e = Platform_vsiGet(platform.sched_memclients_memstats_common_id,
				platform.sched_memclients_memstats_common_cksum, list,
				&common32, 132);
		copy_common32_values:
			if(e) goto failure;
			proc->m_size = common32.size / CRT_page_size_kibibyte;
			proc->m_resident = common32.mapped / CRT_page_size_kibibyte;
			break;
		case VMKERNEL_VERSION_6_5:
		case VMKERNEL_VERSION_6_7:
			e = Platform_vsiGet(platform.sched_memclients_memstats_common_id,
				platform.sched_memclients_memstats_common_cksum, list,
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
	struct vsi_list *list = Platform_allocateVsiList(1, 0);
	Platform_vsiListAddInt(list, proc->super.pid);
	switch(Platform_running_vmkernel_version) {
			struct vcpu_stats_5_5 stats_5_5;
			struct vcpu_stats_6_5 stats_6_5;
			int e;
		case VMKERNEL_VERSION_5_5:
			e = Platform_vsiGet(platform.sched_vcpus_stats_summarystats_id,
				platform.sched_vcpus_stats_summarystats_cksum, list,
				&stats_5_5, 204);
			if(e) goto stats_failure;
			goto read_5_5_stats;
		case VMKERNEL_VERSION_6_0:
			e = Platform_vsiGet(platform.sched_vcpus_stats_summarystats_id,
				platform.sched_vcpus_stats_summarystats_cksum, list,
				&stats_5_5, 220);
			if(e) goto stats_failure;
		read_5_5_stats:
			proc->super.state = stats_5_5.run_state < sizeof vcpu_run_state_map_5_0 ?
				vcpu_run_state_map_5_0[stats_5_5.run_state] : '?';
			if(proc->super.state == 'W') {
				proc->super.state = stats_5_5.wait_state ? stats_5_5.wait_state << 8 : '?';
			}
			proc->super.processor = stats_5_5.pcpu;
			break;
		case VMKERNEL_VERSION_6_5:
			e = Platform_vsiGet(platform.sched_vcpus_stats_summarystats_id,
				platform.sched_vcpus_stats_summarystats_cksum, list,
				&stats_6_5, 248);
			if(e) goto stats_failure;
			goto read_6_5_stats;
		case VMKERNEL_VERSION_6_7:
			e = Platform_vsiGet(platform.sched_vcpus_stats_summarystats_id,
				platform.sched_vcpus_stats_summarystats_cksum, list,
				&stats_6_5, 256);
			if(e) goto stats_failure;
		read_6_5_stats:
			proc->super.state = stats_6_5.run_state < sizeof vcpu_run_state_map_6_5 ?
				vcpu_run_state_map_6_5[stats_6_5.run_state] : '?';
			if(proc->super.state == 'W') {
				proc->super.state = stats_6_5.wait_state ? stats_6_5.wait_state << 8 : '?';
			}
			proc->super.processor = stats_6_5.pcpu;
			break;
		stats_failure:
			proc->super.state = '?';
			proc->super.processor = -1;
			break;
		default:
			abort();
	}
	free(list);
	if(proc->super.state > 255) switch(Platform_running_vmkernel_version) {
		case VMKERNEL_VERSION_5_5:
			switch(proc->super.state >> 8) {
				case 1:
				case 3 ... 5:
				case 53:
					proc->super.state = 'L';
					break;
				case 2:
				case 10 ... 24:
				case 26:
				case 28:
				case 50:
				case 54 ... 71:
				case 73 ... 75:
				case 77:
				case 84:
				case 88 ... 91:
				case 93 ... 102:
				case 106:
				case 113 ... 116:
				case 119:
				case 125:
				case 126:
					proc->super.state = 'D';
					break;
				case 6:
				case 7:
				case 32 ... 47:
				case 52:
					proc->super.state = 'S';
					break;
				case 8:
				case 9:
				case 72:
				case 76:
				case 92:
				case 112:
				case 121:
					proc->super.state = 'I';
					break;
				case 25:
				case 27:
					proc->super.state = 'V';
					break;
				case 29:
				case 30:
				case 104:
				case 105:
				case 110:
				case 117:
				case 118:
					proc->super.state = 'C';
					break;
				case 31:
					proc->super.state = 'T';
					break;
				case 49:
					proc->super.state = 't';
					break;
				case 78 ... 83:
					proc->super.state = 'H';
					break;
				default:
					proc->super.state = 'W';
					break;
			}
			break;
		case VMKERNEL_VERSION_6_0:
			switch(proc->super.state >> 8) {
				case 1:
				case 3 ... 5:
				case 58:
					proc->super.state = 'L';
					break;
				case 2:
				case 11:
				case 12:
				case 15 ... 22:
				case 24 ... 26:
				case 28:
				case 30:
				case 35:
				case 55:
				case 59 ... 67:
				case 69:
				case 70:
				case 72:
				case 74 ... 82:
				case 84 ... 86:
				case 88 ... 90:
				case 97:
				case 101 ... 104:
				case 106:
				case 108 ... 116:
				case 120:
				case 143:
				case 144:
				case 146:
					proc->super.state = 'D';
					break;
				case 7:
				case 39:
				case 45:
				case 50:
				case 51:
				case 52:
				case 57:
					proc->super.state = 'S';
					break;
				case 8:
				case 10:
				case 13:
				case 14:
				case 23:
				case 27:
				case 32:
				case 68:
				case 71:
				case 73:
				case 83:
				case 87:
				case 105:
				case 107:
				case 126:
				case 131 ... 133:
				case 136:
					proc->super.state = 'I';
					break;
				case 29:
					proc->super.state = 'V';
					break;
				case 31:
				case 33:
				case 34:
				case 118:
				case 119:
				case 124:
					proc->super.state = 'C';
					break;
				case 36:
					proc->super.state = 'T';
					break;
				case 54:
				case 142:
					proc->super.state = 't';
					break;
				case 91 ... 96:
				case 137:
					proc->super.state = 'H';
					break;
				default:
					proc->super.state = 'W';
					break;
			}
			break;
		case VMKERNEL_VERSION_6_5:
			switch(proc->super.state >> 8) {
				case 1 ... 5:
				case 57:
					proc->super.state = 'L';
					break;
				case 6:
				case 37 ... 52:
				case 56:
					proc->super.state = 'S';
					break;
				case 7:
				case 8:
				case 11:
				case 13:
				case 33:
				case 65:
				case 68:
				case 70:
				case 71:
				case 90:
				case 107 ... 109:
				case 113:
				case 130:
				case 135 ... 137:
				case 140:
					proc->super.state = 'I';
					break;
				case 9:
				case 10:
				case 12:
				case 14:
				case 19 ... 32:
				case 34:
				case 35:
				case 55:
				case 58 ... 64:
				case 66:
				case 67:
				case 69:
				case 72 ... 89:
				case 91 ... 95:
				case 101:
				case 104 ... 106:
				case 110 ... 112:
				case 114 ... 122:
				case 149:
				case 151 ... 153:
					proc->super.state = 'D';
					break;
				case 15:
					proc->super.state = 'V';
					break;
				case 16 ... 18:
				case 102:
				case 124:
				case 128:
					proc->super.state = 'C';
					break;
				case 36:
					proc->super.state = 'T';
					break;
				case 54:
				case 139:
				case 145:
					proc->super.state = 't';
					break;
				case 96 ... 100:
				case 141:
					proc->super.state = 'H';
					break;
				default:
					proc->super.state = 'W';
					break;
			}
			break;
		case VMKERNEL_VERSION_6_7:
			switch(proc->super.state >> 8) {
				case 1 ... 5:
				case 59:
				case 82:
					proc->super.state = 'L';
					break;
				case 6:
				case 39 ... 54:
				case 58:
				case 62:
					proc->super.state = 'S';
					break;
				case 7:
				case 8:
				case 12:
				case 14:
				case 35:
				case 69:
				case 72:
				case 74:
				case 75:
				case 98:
				case 115 ... 117:
				case 121:
				case 139:
				case 144 ... 146:
				case 149:
					proc->super.state = 'I';
					break;
				case 9 ... 11:
				case 13:
				case 15:
				case 20 ... 34:
				case 36:
				case 37:
				case 57:
				case 60:
				case 61:
				case 64 ... 68:
				case 70:
				case 71:
				case 73:
				case 76 ... 81:
				case 83 ... 97:
				case 99 ... 103:
				case 109:
				case 112 ... 114:
				case 118 ... 120:
				case 122 ... 131:
				case 159:
				case 161:
				case 162:
				case 165:
					proc->super.state = 'D';
					break;
				case 16:
					proc->super.state = 'V';
					break;
				case 17 ... 19:
				case 110:
				case 133:
				case 137:
					proc->super.state = 'C';
					break;
				case 38:
					proc->super.state = 'T';
					break;
				case 56:
				case 63:
				case 155:
					proc->super.state = 't';
					break;
				case 104 ... 108:
				case 150:
					proc->super.state = 'H';
					break;
				default:
					proc->super.state = 'W';
					break;
			}
			break;
	}
	uint64_t diff = proc->time_usec;
	list = Platform_allocateVsiList(1, 0);
	Platform_vsiListAddInt(list, proc->super.pid);
	switch(Platform_running_vmkernel_version) {
			struct vcpu_state_times_5_0 state_times_5_0;
			struct vcpu_state_times_6_0 state_times_6_0;
			struct vcpu_state_times_6_5 state_times_6_5;
			int e;
		case VMKERNEL_VERSION_5_5:
			e = Platform_vsiGet(platform.sched_vcpus_stats_statetimes_id,
				platform.sched_vcpus_stats_statetimes_cksum, list,
				&state_times_5_0, sizeof state_times_5_0);
			if(e) break;
			if(proc->super.starttime_ctime == (time_t)-1) {
				proc->super.starttime_ctime = this->last_updated.tv_sec - state_times_5_0.up_time / 1000000;
			}
			proc->time_usec = state_times_5_0.run_time;
			break;
		case VMKERNEL_VERSION_6_0:
			e = Platform_vsiGet(platform.sched_vcpus_stats_statetimes_id,
				platform.sched_vcpus_stats_statetimes_cksum, list,
				&state_times_6_0, sizeof state_times_6_0);
			if(e) break;
			if(proc->super.starttime_ctime == (time_t)-1) {
				proc->super.starttime_ctime = this->last_updated.tv_sec - state_times_6_0.up_time / 1000000;
			}
			proc->time_usec = state_times_6_0.run_time;
			break;
		case VMKERNEL_VERSION_6_5:
		case VMKERNEL_VERSION_6_7:
			e = Platform_vsiGet(platform.sched_vcpus_stats_statetimes_id,
				platform.sched_vcpus_stats_statetimes_cksum, list,
				&state_times_6_5, sizeof state_times_6_5);
			if(e) break;
			if(proc->super.starttime_ctime == (time_t)-1) {
				proc->super.starttime_ctime = this->last_updated.tv_sec - state_times_6_5.up_time / 1000000;
			}
			proc->time_usec = state_times_6_5.run_time;
			break;
		default:
			abort();
	}
	free(list);
	// Just use last value if VSI_Get failed
	diff = proc->time_usec < diff ? 0 : proc->time_usec - diff;
	proc->super.time = proc->time_usec / 10000;
	if(this->interval_usec) {
		proc->super.percent_cpu = (double)diff / (double)this->interval_usec * 100;
	}
	if(this->super.settings->flags & PROCESS_FLAG_VMKERNEL_VCPU_COUNT) {
		list = Platform_allocateVsiList(1, 0);
		Platform_vsiListAddInt(list, proc->super.pid);
		int e = Platform_vsiGet(platform.sched_cpuclients_numvcpus_id,
			platform.sched_cpuclients_numvcpus_cksum, list,
			&proc->vcpu_count, sizeof proc->vcpu_count);
		free(list);
		if(e) proc->vcpu_count = 0;
	}
}

void ProcessList_goThroughEntries(ProcessList *super, bool skip_processes) {
	VMKernelProcessList *this = (VMKernelProcessList *)super;

	struct timeval now;
	gettimeofday(&now, NULL);
	this->interval_usec = now.tv_sec < this->last_updated.tv_sec ?
		0 : (uint64_t)(now.tv_sec - this->last_updated.tv_sec) * 1000000 +
			(now.tv_usec - this->last_updated.tv_usec);
	this->last_updated = now;

	get_global_memory_stats(this);
	get_global_cpu_stats(this);

	struct vsi_list empty_list = {
		.type_or_version = 1, .param_size = sizeof(struct vsi_list), .self_ptr = (uintptr_t)&empty_list
	};
	struct vsi_list *worlds_list = xMalloc(sizeof(struct vsi_list));
	memset(worlds_list, 0, sizeof(struct vsi_list));
	worlds_list->type_or_version = 1;
	worlds_list->param_size = sizeof(struct vsi_list);
	worlds_list->self_ptr = (uintptr_t)worlds_list;
	int e = Platform_vsiGetList(platform.world_id, platform.world_cksum,
		&empty_list, worlds_list, sizeof(struct vsi_list));
	if(e) CRT_fatalError("VSI_GetList world", e);
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
	e = Platform_vsiGetList(platform.world_id, platform.world_cksum,
		&empty_list, worlds_list, list_size);
	if(e) CRT_fatalError("VSI_GetList world", e);
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
			proc->ruid = -1;
			proc->euid = -1;
			proc->real_user = "?";
			proc->effective_user = "?";
			ProcessList_add(super, proc);
		}
		get_memory_stats(this, proc);
		get_vcpu_stats(this, (VMKernelProcess *)proc);
		if(super->settings->flags & PROCESS_FLAG_VMKERNEL_PRIORITY) {
			errno = 0;
			proc->nice = getpriority(PRIO_PROCESS, pid);
			if(proc->nice == -1 && errno) proc->nice = INT_MAX;
		}
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
		if(proc->state == 'O') {
			super->running_thread_count++;
			super->running_process_count++;
		}
		proc->updated = true;
	}
	free(worlds_list);
}
