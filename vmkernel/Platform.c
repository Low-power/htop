/*
htop - vmkernel/Platform.c
(C) 2014 Hisham H. Muhammad
(C) 2015 David C. Hunt
Copyright 2015-2026 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "Action.h"
#include "BatteryMeter.h"
#include "SignalsPanel.h"
#include "VMKernelProcess.h"
#include <stdint.h>
#include <stdlib.h>

#define PLATFORM_SUPPORT_PROCESS_O_STATE
#define PLATFORM_PRESENT_THREADS_AS_PROCESSES

#define VMKERNEL_VERSION_5_5 55
#define VMKERNEL_VERSION_6_0 60
#define VMKERNEL_VERSION_6_5 65
#define VMKERNEL_VERSION_6_7 67

#define SYS_VSI_Get 1194
#define SYS_VSI_GetList 1197
#define SYS_VSI_GetNodeInfo 1202
#define SYS_GetMemSize 1215
#define SYS_GetUptimeUS 1216

typedef struct {
	uint32_t world_id;
	uint32_t world_info_id;
	uint32_t world_name_id;
	uint32_t world_backtrace_id;
	uint32_t sched_id;
	uint32_t sched_memclients_id;
	uint32_t sched_memclients_memstats_id;
	uint32_t sched_memclients_memstats_common_id;
	uint32_t sched_memclients_memstats_uw_id;
	uint32_t sched_cpuclients_id;
	uint32_t sched_cpuclients_numvcpus_id;
	uint32_t sched_vcpus_id;
	uint32_t sched_vcpus_stats_id;
	uint32_t sched_vcpus_stats_summarystats_id;
	uint32_t sched_vcpus_stats_statetimes_id;
	uint32_t sched_pcpus_id;
	uint32_t sched_pcpus_stats_id;
	uint32_t sched_globalstats_id;
	uint32_t sched_globalstats_numpcpus_id;
	uint32_t sched_groups_id;
	uint32_t sched_groups_stats_id;
	uint32_t sched_groups_stats_cpustatsdir_id;
	uint32_t sched_groups_stats_cpustatsdir_cpuloadhistory_id;
	uint32_t sched_groups_stats_cpustatsdir_cpuloadhistory_cpuloadhistory1mininpct_id;
	uint32_t sched_groups_stats_cpustatsdir_cpuloadhistory_cpuloadhistory5mininpct_id;
	uint32_t sched_groups_stats_cpustatsdir_cpuloadhistory_cpuloadhistory15mininpct_id;
	uint32_t sched_systemswap_id;
	uint32_t userworld_id;
	uint32_t userworld_cartel_id;
	uint32_t userworld_cartel_cmdline_id;
	uint32_t userworld_cartel_mem_id;
	uint32_t userworld_cartel_mem_mmaps_id;
	uint32_t memory_id;
	uint32_t memory_comprehensive_id;
	uint32_t system_id;
	uint32_t system_modloader_id;
	uint32_t system_modloader_symaddrtoname_id;
	uint64_t world_cksum;
	uint64_t world_info_cksum;
	uint64_t world_name_cksum;
	uint64_t world_backtrace_cksum;
	uint64_t sched_cksum;
	uint64_t sched_memclients_cksum;
	uint64_t sched_memclients_memstats_cksum;
	uint64_t sched_memclients_memstats_common_cksum;
	uint64_t sched_memclients_memstats_uw_cksum;
	uint64_t sched_cpuclients_cksum;
	uint64_t sched_cpuclients_numvcpus_cksum;
	uint64_t sched_vcpus_cksum;
	uint64_t sched_vcpus_stats_cksum;
	uint64_t sched_vcpus_stats_summarystats_cksum;
	uint64_t sched_vcpus_stats_statetimes_cksum;
	uint64_t sched_pcpus_cksum;
	uint64_t sched_pcpus_stats_cksum;
	uint64_t sched_globalstats_cksum;
	uint64_t sched_globalstats_numpcpus_cksum;
	uint64_t sched_groups_cksum;
	uint64_t sched_groups_stats_cksum;
	uint64_t sched_groups_stats_cpustatsdir_cksum;
	uint64_t sched_groups_stats_cpustatsdir_cpuloadhistory_cksum;
	uint64_t sched_groups_stats_cpustatsdir_cpuloadhistory_cpuloadhistory1mininpct_cksum;
	uint64_t sched_groups_stats_cpustatsdir_cpuloadhistory_cpuloadhistory5mininpct_cksum;
	uint64_t sched_groups_stats_cpustatsdir_cpuloadhistory_cpuloadhistory15mininpct_cksum;
	uint64_t sched_systemswap_cksum;
	uint64_t userworld_cksum;
	uint64_t userworld_cartel_cksum;
	uint64_t userworld_cartel_cmdline_cksum;
	uint64_t userworld_cartel_mem_cksum;
	uint64_t userworld_cartel_mem_mmaps_cksum;
	uint64_t memory_cksum;
	uint64_t memory_comprehensive_cksum;
	uint64_t system_cksum;
	uint64_t system_modloader_cksum;
	uint64_t system_modloader_symaddrtoname_cksum;

	int ncores;
} GlobalPlatformData;

struct vsi_node_info {
	uint32_t id;
	uint8_t _unknown_1[9];
	uint32_t parent_id;
	uint32_t sibling_id;
	uint32_t first_child_id;
	uint8_t _unknown_2[28];
	uint64_t checksum;
	uint8_t _unknown_3[67];
} __attribute__((__packed__));

struct vsi_param {
	uint32_t type;
	uint64_t value;
	uint8_t _reserved[28];
} __attribute__((__packed__));

struct vsi_param_6_7 {
	uint64_t value;
	uint8_t _reserved[20];
	uint32_t type;
} __attribute__((__packed__));

struct vsi_list {
	uint32_t type_or_version;
	uint32_t flags;
	uint32_t used_count;
	uint32_t instance_count;
	uint32_t allocated_count;
	uint32_t param_size;
	uint32_t unknown_1;
	uint32_t string_size;
	uint64_t string_offset;
	uint64_t self_ptr;
	uint64_t unknown_2[2];
	union {
		struct vsi_param v5[0];
		struct vsi_param_6_7 v6_7[0];
	} param;
};

static inline uint64_t Platform_vsiListGetValue(const struct vsi_list *list, int i) {
	extern int Platform_running_vmkernel_version;
	switch(Platform_running_vmkernel_version) {
		case VMKERNEL_VERSION_5_5:
		case VMKERNEL_VERSION_6_0:
		case VMKERNEL_VERSION_6_5:
			return list->param.v5[i].value;
		case VMKERNEL_VERSION_6_7:
			return list->param.v6_7[i].value;
		default:
			abort();
	}
}

static inline void Platform_vsiListSetValue(struct vsi_list *list, int i, uint64_t value) {
	extern int Platform_running_vmkernel_version;
	switch(Platform_running_vmkernel_version) {
		case VMKERNEL_VERSION_5_5:
		case VMKERNEL_VERSION_6_0:
		case VMKERNEL_VERSION_6_5:
			list->param.v5[i].value = value;
			break;
		case VMKERNEL_VERSION_6_7:
			list->param.v6_7[i].value = value;
			break;
		default:
			abort();
	}
}

}*/

#include "config.h"
#include "vmk_error_codes.h"
#include <Platform.h>
#include <CPUMeter.h>
#include <MemoryMeter.h>
#include <SwapMeter.h>
#include <TasksMeter.h>
#include <LoadAverageMeter.h>
#include <ClockMeter.h>
#include <HostnameMeter.h>
#include <UptimeMeter.h>
#include <UsersMeter.h>
#include <CRT.h>
#include <VMKernelProcessList.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

struct cpu_load_history_info_in_pct {
	uint32_t avg_run_time;
	uint32_t avg_active;
	uint32_t avg_max_limited;
	uint32_t max_run_time;
	uint32_t max_active;
	uint32_t max_max_limited;
	uint32_t run_quantiles[10];
	uint32_t active_quantiles[10];
	uint32_t max_limited_quantiles[10];
};

// Size values are in kibibyte
struct system_swap_status_32 {
	uint8_t enabled;
	uint8_t writeable;
	uint32_t total_size;
	uint32_t available_size;
	uint32_t reserved_size;
	uint32_t consumed_size;
	char path[2050];
} __attribute__((__packed__));

// Size values are in kibibyte
struct system_swap_status_64 {
	uint8_t enabled;
	uint8_t writeable;
	uint64_t total_size;
	uint64_t available_size;
	uint64_t reserved_size;
	uint64_t consumed_size;
	char path[2050];
} __attribute__((__packed__));

#ifdef __i386__
#define VMKSC_0(N) ({ int e; __asm__ __volatile__("int $0x90" : "=a"(e) : "a"(N) : "memory"); e; })
#define VMKSC_1(N,A1) \
	({ int e; __asm__ __volatile__("int $0x90" : "=a"(e) : "a"(N), "c"(A1) : "memory"); e; })
#define VMKSC_2(N,A1,A2) \
	({ int e; __asm__ __volatile__("int $0x90" : "=a"(e) : "a"(N), "c"(A1), "d"(A2) : "memory"); e; })
#define VMKSC_3(N,A1,A2,A3) \
	({ int e; __asm__ __volatile__("int $0x90" : "=a"(e) : "a"(N), "c"(A1), "d"(A2), "S"(A3) : "memory"); e; })
#define VMKSC_4(N,A1,A2,A3,A4) \
	({ int e; __asm__ __volatile__("int $0x90" : "=a"(e) : "a"(N), "c"(A1), "d"(A2), "S"(A3), "D"(A4) : "memory"); e; })
#define VMKSC_VA_6(A1,A2,A3,A4,A5,A6,...) A6
#define VMKSC_VA(...) VMKSC_VA_6(__VA_ARGS__,VMKSC_4,VMKSC_3,VMKSC_2,VMKSC_1,VMKSC_0,)
#define VMKSC(...) VMKSC_VA(__VA_ARGS__)(__VA_ARGS__)
#else
#define VMKSC syscall
#endif

GlobalPlatformData platform;

static void check_vmkernel_version(void);

static int vsi_get_node_id_and_checksum(uint32_t parent_node_id, const char *node_name, uint32_t *node_id, uint64_t *node_cksum) {
	struct vsi_node_info info_buffer;
	char name_buffer[128];
#ifdef __i386__
	uint32_t extra_args[2] = { (uint32_t)name_buffer, sizeof info_buffer };
	int e = VMKSC(SYS_VSI_GetNodeInfo, parent_node_id, 0, &info_buffer, extra_args);
#else
	int e = syscall(SYS_VSI_GetNodeInfo, parent_node_id, 0,
		&info_buffer, name_buffer, sizeof info_buffer);
#endif
	if(e) return e;
	uint32_t child_node_id = info_buffer.first_child_id;
	do {
#ifdef __i386__
		e = VMKSC(SYS_VSI_GetNodeInfo, child_node_id, 0, &info_buffer, extra_args);
#else
		e = syscall(SYS_VSI_GetNodeInfo, child_node_id, 0,
			&info_buffer, name_buffer, sizeof info_buffer);
#endif
		if(e) return e;
		if(strcmp(name_buffer, node_name) == 0) {
			if(node_id) *node_id = info_buffer.id;
			if(node_cksum) memcpy(node_cksum, &info_buffer.checksum, 8);
			return VMK_OK;
		}
		child_node_id = info_buffer.sibling_id;
	} while(child_node_id && child_node_id != UINT32_MAX);
	//return VMK_NOT_FOUND;
	return ENOENT;
}

void Platform_init() {
   check_vmkernel_version();

   int e = vsi_get_node_id_and_checksum(0, "world", &platform.world_id, &platform.world_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo world", e);
   e = vsi_get_node_id_and_checksum(platform.world_id, "info",
      &platform.world_info_id, &platform.world_info_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo world.info", e);
   e = vsi_get_node_id_and_checksum(platform.world_id, "name",
      &platform.world_name_id, &platform.world_name_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo world.name", e);
   e = vsi_get_node_id_and_checksum(platform.world_id, "backtrace",
      &platform.world_backtrace_id, &platform.world_backtrace_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo world.backtrace", e);
   e = vsi_get_node_id_and_checksum(0, "sched", &platform.sched_id, &platform.sched_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched", e);
   e = vsi_get_node_id_and_checksum(platform.sched_id, "memClients",
      &platform.sched_memclients_id, &platform.sched_memclients_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.memClients", e);
   e = vsi_get_node_id_and_checksum(platform.sched_memclients_id, "memStats",
      &platform.sched_memclients_memstats_id, &platform.sched_memclients_memstats_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.memClients.memStats", e);
   e = vsi_get_node_id_and_checksum(platform.sched_memclients_memstats_id, "totalCommon",
      &platform.sched_memclients_memstats_common_id, &platform.sched_memclients_memstats_common_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.memClients.memStats.totalCommon", e);
   e = vsi_get_node_id_and_checksum(platform.sched_memclients_memstats_id, "uw",
      &platform.sched_memclients_memstats_uw_id, &platform.sched_memclients_memstats_uw_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.memClients.memStats.uw", e);
   e = vsi_get_node_id_and_checksum(platform.sched_id, "cpuClients",
      &platform.sched_cpuclients_id, &platform.sched_cpuclients_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.cpuClients", e);
   e = vsi_get_node_id_and_checksum(platform.sched_cpuclients_id, "numVcpus",
      &platform.sched_cpuclients_numvcpus_id, &platform.sched_cpuclients_numvcpus_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.cpuClients.numVcpus", e);
   e = vsi_get_node_id_and_checksum(platform.sched_id, "Vcpus",
      &platform.sched_vcpus_id, &platform.sched_vcpus_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.Vcpus", e);
   e = vsi_get_node_id_and_checksum(platform.sched_vcpus_id, "stats",
      &platform.sched_vcpus_stats_id, &platform.sched_vcpus_stats_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.Vcpus.stats", e);
   e = vsi_get_node_id_and_checksum(platform.sched_vcpus_stats_id, "summaryStats",
      &platform.sched_vcpus_stats_summarystats_id, &platform.sched_vcpus_stats_summarystats_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.Vcpus.stats.summaryStats", e);
   e = vsi_get_node_id_and_checksum(platform.sched_vcpus_stats_id, "stateTimes",
      &platform.sched_vcpus_stats_statetimes_id, &platform.sched_vcpus_stats_statetimes_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.Vcpus.stats.stateTimes", e);
   e = vsi_get_node_id_and_checksum(platform.sched_id, "pcpus",
      &platform.sched_pcpus_id, &platform.sched_pcpus_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.pcpus", e);
   e = vsi_get_node_id_and_checksum(platform.sched_pcpus_id, "stats",
      &platform.sched_pcpus_stats_id, &platform.sched_pcpus_stats_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.pcpus.stats", e);
   e = vsi_get_node_id_and_checksum(platform.sched_id, "globalStats",
      &platform.sched_globalstats_id, &platform.sched_globalstats_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.globalStats", e);
   e = vsi_get_node_id_and_checksum(platform.sched_globalstats_id, "numPcpus",
      &platform.sched_globalstats_numpcpus_id, &platform.sched_globalstats_numpcpus_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.globalStats.numPcpus", e);
   e = vsi_get_node_id_and_checksum(platform.sched_id, "groups",
      &platform.sched_groups_id, &platform.sched_groups_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.groups", e);
   e = vsi_get_node_id_and_checksum(platform.sched_groups_id, "stats",
      &platform.sched_groups_stats_id, &platform.sched_groups_stats_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.groups.stats", e);
   e = vsi_get_node_id_and_checksum(platform.sched_groups_stats_id, "cpuStatsDir",
      &platform.sched_groups_stats_cpustatsdir_id, &platform.sched_groups_stats_cpustatsdir_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.groups.stats.cpuStatsDir", e);
   e = vsi_get_node_id_and_checksum(platform.sched_groups_stats_cpustatsdir_id, "cpuLoadHistory",
      &platform.sched_groups_stats_cpustatsdir_cpuloadhistory_id, 
      &platform.sched_groups_stats_cpustatsdir_cpuloadhistory_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.groups.stats.cpuStatsDir.cpuLoadHistory", e);
   e = vsi_get_node_id_and_checksum(platform.sched_groups_stats_cpustatsdir_cpuloadhistory_id,
      "cpuLoadHistory1MinInPct",
      &platform.sched_groups_stats_cpustatsdir_cpuloadhistory_cpuloadhistory1mininpct_id,
      &platform.sched_groups_stats_cpustatsdir_cpuloadhistory_cpuloadhistory1mininpct_cksum);
   if(e) {
      CRT_fatalError(
         "VSI_GetNodeInfo sched.groups.stats.cpuStatsDir.cpuLoadHistory.cpuLoadHistory1MinInPct", e
      );
   }
   e = vsi_get_node_id_and_checksum(platform.sched_groups_stats_cpustatsdir_cpuloadhistory_id,
      "cpuLoadHistory5MinInPct",
      &platform.sched_groups_stats_cpustatsdir_cpuloadhistory_cpuloadhistory5mininpct_id,
      &platform.sched_groups_stats_cpustatsdir_cpuloadhistory_cpuloadhistory5mininpct_cksum);
   if(e) {
      CRT_fatalError(
         "VSI_GetNodeInfo sched.groups.stats.cpuStatsDir.cpuLoadHistory.cpuLoadHistory5MinInPct", e
      );
   }
   e = vsi_get_node_id_and_checksum(platform.sched_groups_stats_cpustatsdir_cpuloadhistory_id,
      "cpuLoadHistory15MinInPct",
      &platform.sched_groups_stats_cpustatsdir_cpuloadhistory_cpuloadhistory15mininpct_id,
      &platform.sched_groups_stats_cpustatsdir_cpuloadhistory_cpuloadhistory15mininpct_cksum);
   if(e) {
      CRT_fatalError(
         "VSI_GetNodeInfo sched.groups.stats.cpuStatsDir.cpuLoadHistory.cpuLoadHistory15MinInPct", e
      );
   }
   e = vsi_get_node_id_and_checksum(platform.sched_id, "systemSwap",
      &platform.sched_systemswap_id, &platform.sched_systemswap_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo sched.systemSwap", e);
   e = vsi_get_node_id_and_checksum(0, "userworld",
      &platform.userworld_id, &platform.userworld_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo userworld", e);
   e = vsi_get_node_id_and_checksum(platform.userworld_id, "cartel",
      &platform.userworld_cartel_id, &platform.userworld_cartel_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo userworld.cartel", e);
   e = vsi_get_node_id_and_checksum(platform.userworld_cartel_id, "cmdline",
      &platform.userworld_cartel_cmdline_id, &platform.userworld_cartel_cmdline_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo userworld.cartel.cmdline", e);
   e = vsi_get_node_id_and_checksum(platform.userworld_cartel_id, "mem",
      &platform.userworld_cartel_mem_id, &platform.userworld_cartel_mem_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo userworld.cartel.mem", e);
   e = vsi_get_node_id_and_checksum(platform.userworld_cartel_mem_id, "mmaps",
      &platform.userworld_cartel_mem_mmaps_id, &platform.userworld_cartel_mem_mmaps_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo userworld.cartel.mem.mmaps", e);
   e = vsi_get_node_id_and_checksum(0, "memory", &platform.memory_id, &platform.memory_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo memory", e);
   e = vsi_get_node_id_and_checksum(platform.memory_id, "comprehensive",
      &platform.memory_comprehensive_id, &platform.memory_comprehensive_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo memory.comprehensive", e);
   e = vsi_get_node_id_and_checksum(0, "system", &platform.system_id, &platform.system_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo system", e);
   e = vsi_get_node_id_and_checksum(platform.system_id, "modloader",
      &platform.system_modloader_id, &platform.system_modloader_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo system.modloader", e);
   e = vsi_get_node_id_and_checksum(platform.system_modloader_id, "symAddrToName",
      &platform.system_modloader_symaddrtoname_id, &platform.system_modloader_symaddrtoname_cksum);
   if(e) CRT_fatalError("VSI_GetNodeInfo system.modloader.symAddrToName", e);
}

const SignalItem Platform_signals[] = {
   { .name = "Cancel", .number = 0 },
#define SIG(NAME) { .name = #NAME, .number = SIG##NAME }
   SIG(HUP),
   SIG(INT),
   SIG(QUIT),
   SIG(ILL),
   SIG(TRAP),
   SIG(ABRT),
#ifdef SIGIOT
   SIG(IOT),
#endif
#ifdef SIGEMT
   SIG(EMT),
#endif
   SIG(BUS),
   SIG(FPE),
   SIG(KILL),
   SIG(USR1),
   SIG(SEGV),
   SIG(USR2),
   SIG(PIPE),
   SIG(ALRM),
   SIG(TERM),
#ifdef SIGSTKFLT
   SIG(STKFLT),
#endif
   SIG(CHLD),
   SIG(CONT),
   SIG(STOP),
   SIG(TSTP),
   SIG(TTIN),
   SIG(TTOU),
   SIG(URG),
   SIG(XCPU),
   SIG(XFSZ),
   SIG(VTALRM),
   SIG(PROF),
   SIG(WINCH),
   SIG(IO),
#ifdef SIGPOLL
   SIG(POLL),
#endif
#ifdef SIGPWR
   SIG(PWR),
#endif
#ifdef SIGINFO
   SIG(INFO),
#endif
#ifdef SIGLOST
   SIG(LOST),
#endif
#ifdef SIGSYS
   SIG(SYS),
#endif
#undef SIG
};

const unsigned int Platform_numberOfSignals = sizeof(Platform_signals)/sizeof(SignalItem);

ProcessField Platform_defaultFields[] = {
	HTOP_PID_FIELD, HTOP_EFFECTIVE_USER_FIELD, HTOP_NICE_FIELD, HTOP_M_SIZE_FIELD, HTOP_M_RESIDENT_FIELD, HTOP_STATE_FIELD, HTOP_PERCENT_CPU_FIELD, HTOP_PERCENT_MEM_FIELD, HTOP_TIME_FIELD, HTOP_COMM_FIELD, 0
};

const unsigned int Platform_numberOfFields = HTOP_LAST_PROCESSFIELD;

MeterClass* Platform_meterTypes[] = {
   &CPUMeter_class,
   &ClockMeter_class,
   &LoadAverageMeter_class,
   &LoadMeter_class,
   &MemoryMeter_class,
   &SwapMeter_class,
   &TasksMeter_class,
#ifdef HAVE_UTMPX
   &UsersMeter_class,
#endif
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

int Platform_getUptime() {
	uint64_t uptime;
	int e = VMKSC(SYS_GetUptimeUS, &uptime);
	return e == VMK_OK ? (int)(uptime / 1000000) : -1;
}

void Platform_getLoadAverage(double *values) {
	uint32_t vsi_node_ids[3] = {
		platform.sched_groups_stats_cpustatsdir_cpuloadhistory_cpuloadhistory1mininpct_id,
		platform.sched_groups_stats_cpustatsdir_cpuloadhistory_cpuloadhistory5mininpct_id,
		platform.sched_groups_stats_cpustatsdir_cpuloadhistory_cpuloadhistory15mininpct_id
	};
	uint64_t vsi_node_cksums[3] = {
		platform.sched_groups_stats_cpustatsdir_cpuloadhistory_cpuloadhistory1mininpct_cksum,
		platform.sched_groups_stats_cpustatsdir_cpuloadhistory_cpuloadhistory5mininpct_cksum,
		platform.sched_groups_stats_cpustatsdir_cpuloadhistory_cpuloadhistory15mininpct_cksum
	};

	assert(platform.ncores > 0);
	double divisor = platform.ncores * 100;

	struct vsi_list *list = Platform_allocateVsiList(1, 0);
	Platform_vsiListAddInt(list, 0);
	for(int i = 0; i < 3; i++) {
		struct cpu_load_history_info_in_pct load_info;
		int e = Platform_vsiGet(vsi_node_ids[i], vsi_node_cksums[i], list,
			&load_info, sizeof load_info);
		values[i] = e ? 0 : load_info.avg_active / divisor;
	}
	free(list);
}

int Platform_getMaxPid() {
   return (1 << 30) - 1;
}

double Platform_updateCPUValues(Meter *meter, int cpu) {
	double total_percent;
	bool use_detailed_cpu_time = meter->pl->settings->detailedCPUTime;
	const VMKernelProcessList *pl = (const VMKernelProcessList *)meter->pl;
	if(!pl->interval_usec) {
		total_percent = 0;
		Meter_setItemCount(meter, 0);
	} else if(cpu || pl->super.cpuCount == 1) {
		const VMKernelCPUStatistics *stats = pl->cpu_stats + (cpu ? cpu - 1 : 0);
		total_percent = stats->total_used_period / (double)pl->interval_usec * 100;
		meter->values[CPU_METER_NORMAL] =
			(stats->total_used_period - stats->kernel_nw_period - stats->wdt_period) /
				(double)pl->interval_usec * 100;
		if(use_detailed_cpu_time) {
			meter->values[CPU_METER_KERNEL] = stats->kernel_nw_period / (double)pl->interval_usec * 100;
			meter->values[CPU_METER_SOFTIRQ] = stats->wdt_period / (double)pl->interval_usec * 100;
			Meter_setItemCount(meter, 5);
		} else {
			meter->values[CPU_METER_KERNEL] =
				(stats->kernel_nw_period + stats->wdt_period) / (double)pl->interval_usec * 100;
			Meter_setItemCount(meter, 3);
		}
	} else {
		double total_time = 0;
		double total_user_time = 0;
		double total_kernel_nw_time = 0;
		double total_wdt_time = 0;
		for(int i = 0; i < pl->super.cpuCount; i++) {
			const VMKernelCPUStatistics *stats = pl->cpu_stats + i;
			total_time += stats->total_used_period;
			total_user_time += stats->total_used_period - stats->kernel_nw_period;
			total_kernel_nw_time += stats->kernel_nw_period;
			total_wdt_time += stats->wdt_period;
		}
		total_percent = total_time / (double)pl->super.cpuCount / (double)pl->interval_usec * 100;
		meter->values[CPU_METER_NORMAL] =
			total_user_time / (double)pl->super.cpuCount / (double)pl->interval_usec * 100;
		if(use_detailed_cpu_time) {
			meter->values[CPU_METER_KERNEL] =
				total_kernel_nw_time / (double)pl->super.cpuCount /
					(double)pl->interval_usec * 100;
			meter->values[CPU_METER_SOFTIRQ] =
				total_wdt_time / (double)pl->super.cpuCount / (double)pl->interval_usec * 100;
			Meter_setItemCount(meter, 5);
		} else {
			meter->values[CPU_METER_KERNEL] =
				(total_kernel_nw_time + total_wdt_time) /
					(double)pl->super.cpuCount / (double)pl->interval_usec * 100;
			Meter_setItemCount(meter, 3);
		}
	}
	return total_percent;
}

void Platform_updateMemoryValues(Meter *meter) {
#if 0
	uint64_t total, free;
	unsigned int e = VMKSC(SYS_GetMemSize, &total, &free);
	if(e != VMK_OK) return;
	meter->total = total / 1024;
	meter->values[0] = (total - free) / 1024;
#else
	const ProcessList *pl = meter->pl;
	meter->total = pl->totalMem;
	meter->values[0] = pl->usedMem;
#endif
}

void Platform_updateSwapValues(Meter *meter) {
	struct vsi_list list = {
		.type_or_version = 1, .param_size = sizeof(struct vsi_list), .self_ptr = (uintptr_t)&list
	};
	struct system_swap_status_32 stat32;
	struct system_swap_status_64 stat64;
	int e;
	switch(Platform_running_vmkernel_version) {
		case VMKERNEL_VERSION_5_5:
		case VMKERNEL_VERSION_6_0:
			e = Platform_vsiGet(platform.sched_systemswap_id,
				platform.sched_systemswap_cksum, &list,
				&stat32, sizeof stat32);
			if(e) goto failure;
			meter->total = stat32.total_size;
			meter->values[0] = stat32.consumed_size;
			break;
		case VMKERNEL_VERSION_6_5:
		case VMKERNEL_VERSION_6_7:
			e = Platform_vsiGet(platform.sched_systemswap_id,
				platform.sched_systemswap_cksum, &list,
				&stat64, sizeof stat64);
			if(e) goto failure;
			meter->total = stat64.total_size;
			meter->values[0] = stat64.consumed_size;
			break;
		failure:
			meter->total = 0;
			meter->values[0] = 0;
			break;
		default:
			abort();
	}
}

char **Platform_getProcessArgv(const Process *proc) {
	return NULL;
}

char **Platform_getProcessEnvv(const Process *proc) {
	return NULL;
}

bool Platform_haveSwap() {
	struct vsi_list list = {
		.type_or_version = 1, .param_size = sizeof(struct vsi_list), .self_ptr = (uintptr_t)&list
	};
	struct system_swap_status_32 stat32;
	struct system_swap_status_64 stat64;
	int e;
	switch(Platform_running_vmkernel_version) {
		case VMKERNEL_VERSION_5_5:
		case VMKERNEL_VERSION_6_0:
			e = Platform_vsiGet(platform.sched_systemswap_id,
				platform.sched_systemswap_cksum, &list,
				&stat32, sizeof stat32);
			if(e) return false;
			return stat32.enabled;
		case VMKERNEL_VERSION_6_5:
		case VMKERNEL_VERSION_6_7:
			e = Platform_vsiGet(platform.sched_systemswap_id,
				platform.sched_systemswap_cksum, &list,
				&stat64, sizeof stat64);
			if(e) return false;
			return stat64.enabled;
		default:
			abort();
	}
}

int Platform_running_vmkernel_version;

#ifndef __i386__

static int vsi_get_5_0(uint32_t node_id, uint64_t node_cksum, const struct vsi_list *list, void *buffer, size_t size) {
	struct {
		uint64_t checksum;
		void *buffer;
		size_t buffer_size;
		uint64_t _unknown_1;
	} extra_args[2] = {
		{ node_cksum, buffer, size },
		{ node_cksum, buffer, size }
	};
	return syscall(SYS_VSI_Get, node_id, 0, list, list->param_size, 0, extra_args);
}

static int vsi_get_6_5(uint32_t node_id, uint64_t node_cksum, const struct vsi_list *list, void *buffer, size_t size) {
	struct {
		uint32_t _unknown_1;
		void *buffer;
		size_t buffer_size;
	} __attribute__((__packed__)) extra_args = {
		0, buffer, size
	};
	return syscall(SYS_VSI_Get, node_id, 0, node_cksum, list, list->param_size, &extra_args);
}

static int (*vsi_get)(uint32_t, uint64_t, const struct vsi_list *, void *, size_t);

static int vsi_get_list_5_0(uint32_t node_id, uint64_t node_cksum, const struct vsi_list *input_list, struct vsi_list *output_list, size_t size) {
	struct {
		uint64_t checksum;
		size_t output_size;
	} extra_args[2] = {
		{ node_cksum, size }, { node_cksum, size }
	};
	return syscall(SYS_VSI_GetList, node_id, 0, input_list, input_list->param_size,
		output_list, extra_args);
}

static int vsi_get_list_6_5(uint32_t node_id, uint64_t node_cksum, const struct vsi_list *input_list, struct vsi_list *output_list, size_t size) {
	struct {
		struct vsi_list *output_list;
		size_t output_size;
	} extra_args[2] = {
		{ output_list, size }, { output_list, size }
	};
	return syscall(SYS_VSI_GetList, node_id, 0, node_cksum, input_list, input_list->param_size,
		extra_args);
}

static int (*vsi_get_list)(uint32_t, uint64_t, const struct vsi_list *, struct vsi_list *, size_t);

#endif

static void check_vmkernel_version() {
	struct utsname utsname;
	if(uname(&utsname) < 0) CRT_fatalError("uname", 0);
	if(strcmp(utsname.sysname, "VMkernel")) CRT_fatalError("Kernel type not supported", EPERM);
	char *end_p;
	long int major_version = strtol(utsname.release, &end_p, 10);
	if(end_p == utsname.release || *end_p != '.') {
		CRT_fatalError("Kernel version not supported", EPERM);
	}
	const char *begin_p = end_p + 1;
	long int minor_version = strtol(begin_p, &end_p, 10);
	if(end_p == begin_p || *end_p != '.') CRT_fatalError("Kernel version not supported", EPERM);
	Platform_running_vmkernel_version = major_version * 10 + minor_version;
	switch(Platform_running_vmkernel_version) {
		case VMKERNEL_VERSION_5_5:
#ifndef __i386__
			vsi_get = vsi_get_5_0;
			vsi_get_list = vsi_get_list_5_0;
#endif
			break;
		case VMKERNEL_VERSION_6_0:
#ifndef __i386__
			vsi_get = vsi_get_5_0;
			vsi_get_list = vsi_get_list_5_0;
#endif
			break;
		case VMKERNEL_VERSION_6_5:
#ifndef __i386__
			vsi_get = vsi_get_6_5;
			vsi_get_list = vsi_get_list_6_5;
#endif
			break;
		case VMKERNEL_VERSION_6_7:
#ifndef __i386__
			vsi_get = vsi_get_6_5;
			vsi_get_list = vsi_get_list_6_5;
#endif
			break;
		default:
			CRT_fatalError("Kernel version not supported", EPERM);
	}
}

struct vsi_list *Platform_allocateVsiList(uint32_t count, uint32_t string_size) {
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

void Platform_vsiListAddInt(struct vsi_list *list, uint64_t value) {
	uint32_t i = list->used_count;
	assert(i < list->allocated_count);
	switch(Platform_running_vmkernel_version) {
		case VMKERNEL_VERSION_5_5:
		case VMKERNEL_VERSION_6_0:
		case VMKERNEL_VERSION_6_5:
			list->param.v5[i].type = 0;
			list->param.v5[i].value = value;
			break;
		case VMKERNEL_VERSION_6_7:
			list->param.v6_7[i].type = 1;
			list->param.v6_7[i].value = value;
			break;
		default:
			abort();
	}
	list->used_count++;
	list->instance_count++;
}

int Platform_vsiGet(uint32_t node_id, uint64_t node_cksum, const struct vsi_list *list, void *buffer, size_t size) {
#ifdef __i386__
	struct {
		uint64_t checksum;
		size_t input_size;
		uint32_t _unknown_1;
		void *buffer;
		size_t buffer_size;
	} extra_args[2] = {
		{ node_cksum, list->param_size, 0, buffer, size },
		{ node_cksum, list->param_size, 0, buffer, size }
	};
	return VMKSC(SYS_VSI_Get, node_id, 0, list, extra_args);
#else
	if(!vsi_get) check_vmkernel_version();
	return vsi_get(node_id, node_cksum, list, buffer, size);
#endif
}

int Platform_vsiGetList(uint32_t node_id, uint64_t node_cksum, const struct vsi_list *input_list, struct vsi_list *output_list, size_t size) {
#ifdef __i386__
	struct {
		uint64_t checksum;
		size_t input_size;
		struct vsi_list *output_list;
		size_t output_size;
	} extra_args[2] = {
		{ node_cksum, input_list->param_size, output_list, size },
		{ node_cksum, input_list->param_size, output_list, size }
	};
	return VMKSC(SYS_VSI_GetList, node_id, 0, input_list, extra_args);
#else
	if(!vsi_get_list) check_vmkernel_version();
	return vsi_get_list(node_id, node_cksum, input_list, output_list, size);
#endif
}
