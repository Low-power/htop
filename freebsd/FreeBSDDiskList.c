/*
htop - freebsd/FreeBSDDiskList.c
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "DiskList.h"
#include <libgeom.h>

typedef struct {
	DiskList super;
	void *previous_stats;
	struct gmesh tree;
} FreeBSDDiskList;
}*/

#include "FreeBSDDiskList.h"
#include "FreeBSDDisk.h"
#include "CRT.h"
#include "XAlloc.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/devicestat.h>
#include <devstat.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

DiskList *DiskList_new(const Settings *settings) {
	int e = geom_stats_open();
	if(e && e != EBUSY) CRT_fatalError("geom_stats_open", e);
	FreeBSDDiskList *disk_list = xMalloc(sizeof(FreeBSDDiskList));
	DiskList_init(&disk_list->super, Class(FreeBSDDisk), settings);
	disk_list->previous_stats = geom_stats_snapshot_get();
	if(!disk_list->previous_stats) CRT_fatalError("geom_stats_snapshot_get", 0);
	e = geom_gettree(&disk_list->tree);
	if(e) CRT_fatalError("geom_gettree", e);
	return (DiskList *)disk_list;
}

void DiskList_delete(DiskList *super) {
	FreeBSDDiskList *this = (FreeBSDDiskList *)super;
	geom_stats_snapshot_free(this->previous_stats);
	free(this);
}

#define BINTIME_TO_HUNDREDTHSEC(BINTIME) \
	((BINTIME)->sec * 100 + (((uint8_t)((BINTIME)->frac>>48) * (uint8_t)100) >> 8))

void DiskList_internalScan(DiskList *super, double unused_interval) {
	FreeBSDDiskList *this = (FreeBSDDiskList *)super;
	void *stats = geom_stats_snapshot_get();
	if(!stats) return;
	struct timespec ts, prev_ts;
	geom_stats_snapshot_timestamp(stats, &ts);
	geom_stats_snapshot_timestamp(this->previous_stats, &prev_ts);
	long double interval = (ts.tv_sec - prev_ts.tv_sec) +
		(long double)(ts.tv_nsec - prev_ts.tv_nsec) / 1000000000;
	struct devstat *devstat;
	while((devstat = geom_stats_snapshot_next(stats))) {
		struct devstat *prev_devstat = geom_stats_snapshot_next(this->previous_stats);
		const struct gident *gid = geom_lookupid(&this->tree, devstat->id);
		if(!gid) continue;
		if(gid->lg_what != ISPROVIDER) continue;
		const struct gprovider *p = gid->lg_ptr;
		if(p->lg_geom->lg_rank != 1) continue;

		bool is_existing;
		Disk *disk = DiskList_getOrCreate(super, p->lg_name, &is_existing, (DiskConstructor)FreeBSDDisk_new);
		FreeBSDDisk *fbsd_disk = (FreeBSDDisk *)disk;
		if(!is_existing) {
			//disk->block_size = lg_sectorsize;
			disk->block_size = devstat->block_size ? : 512;
			disk->creation_time = devstat->creation_time.sec;
			if(super->settings->disk_flags & HTOP_DISK_DEVID_FLAG) {
				struct gconfig *kvp;
				LIST_FOREACH(kvp, &p->lg_config, lg_config) {
					if(strcmp(kvp->lg_name, "ident") == 0) {
						disk->devid = xStrdup(kvp->lg_val);
						break;
					}
				}
			}
			DiskList_add(super, disk);
		}
		struct timeval duration_tv;
#ifdef DSM_TOTAL_DURATION
		long double total_duration;
#endif
#ifdef DSM_TOTAL_BUSY_TIME
		long double total_busy_time;
#endif
		devstat_compute_statistics(devstat, NULL, 0,
			DSM_QUEUE_LENGTH, &disk->queue_length,
			DSM_TOTAL_TRANSFERS, &disk->operation_count,
			DSM_TOTAL_TRANSFERS_READ, &disk->read_operation_count,
			DSM_TOTAL_TRANSFERS_WRITE, &disk->write_operation_count,
			DSM_TOTAL_BLOCKS_READ, &disk->read_block_count,
			DSM_TOTAL_BLOCKS_WRITE, &disk->write_block_count,
#ifdef DSM_TOTAL_DURATION
			DSM_TOTAL_DURATION, &total_duration,
#endif
#ifdef DSM_TOTAL_BUSY_TIME
			DSM_TOTAL_BUSY_TIME, &total_busy_time,
#endif
			DSM_NONE);
#ifdef DSM_TOTAL_DURATION
		duration_tv.tv_sec = total_duration;
		duration_tv.tv_usec = (int)(total_duration * 1000000) % 1000000;
		disk->oper_time = total_duration * 100;
#else
		struct bintime duration = devstat->duration[DEVSTAT_READ];
		bintime_add(&duration, devstat->duration + DEVSTAT_WRITE);
		bintime_add(&duration, devstat->duration + DEVSTAT_FREE);
		bintime_add(&duration, devstat->duration + DEVSTAT_NO_DATA);
		bintime2timeval(&duration, &duration_tv);
		disk->oper_time = BINTIME_TO_HUNDREDTHSEC(&duration);
#endif
#ifdef DSM_TOTAL_BUSY_TIME
		fbsd_disk->busy_time = total_busy_time * 100;
#else
		fbsd_disk->busy_time = BINTIME_TO_HUNDREDTHSEC(&devstat->busy_time);
#endif
		if(super->settings->disk_flags & HTOP_DISK_PERCENT_UTIL_FLAG) {
			struct timeval delta;
			if(timercmp(&duration_tv, &fbsd_disk->duration, >)) {
				timersub(&duration_tv, &fbsd_disk->duration, &delta);
			} else {
				delta.tv_sec = 0;
				delta.tv_usec = 0;
			}
			disk->percent_util =
				(delta.tv_sec * 100 + (long double)delta.tv_usec / 10000) /
					interval;
			if(disk->percent_util > 100) disk->percent_util = 100;
		}
		fbsd_disk->duration = duration_tv;
		if(prev_devstat) {
			long double transfers_per_second;
			long double transfers_per_second_read;
			long double transfers_per_second_write;
			long double blocks_per_seconds_read;
			long double blocks_per_seconds_write;
			long double busy_pct;
			assert(devstat->id == prev_devstat->id);
			devstat_compute_statistics(devstat, prev_devstat, interval,
				DSM_TRANSFERS_PER_SECOND, &transfers_per_second,
				DSM_TRANSFERS_PER_SECOND_READ, &transfers_per_second_read,
				DSM_TRANSFERS_PER_SECOND_WRITE, &transfers_per_second_write,
				DSM_BLOCKS_PER_SECOND_READ, &blocks_per_seconds_read,
				DSM_BLOCKS_PER_SECOND_WRITE, &blocks_per_seconds_write,
				DSM_BUSY_PCT, &busy_pct,
				DSM_NONE);
			disk->operation_rate = transfers_per_second;
			disk->read_operation_rate = transfers_per_second_read;
			disk->write_operation_rate = transfers_per_second_write;
			disk->read_block_rate = blocks_per_seconds_read;
			disk->write_block_rate = blocks_per_seconds_write;
			fbsd_disk->percent_busy = busy_pct;
		}
		disk->updated = true;
	}
	geom_stats_snapshot_reset(stats);
	geom_stats_snapshot_free(this->previous_stats);
	this->previous_stats = stats;
}
