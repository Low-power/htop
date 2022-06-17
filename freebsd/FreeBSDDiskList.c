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

#include "config.h"
#include "FreeBSDDiskList.h"
#include "FreeBSDDisk.h"
#include "CRT.h"
#include "XAlloc.h"
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/disk.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/devicestat.h>
#include <devstat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/scsi/scsi_pass.h>
#include <assert.h>

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
	DiskList_done(super);
	FreeBSDDiskList *this = (FreeBSDDiskList *)super;
	geom_stats_snapshot_free(this->previous_stats);
	free(this);
}

static void fill_from_device_node(Disk *disk, int flags, bool need_block_size) {
	size_t len = strlen(disk->name) + 1;
	char path[5 + len];
	memcpy(path, "/dev/", 5);
	memcpy(path + 5, disk->name, len);
	int fd = open(path, O_RDONLY);
	if(fd == -1) return;
	if(need_block_size) {
		unsigned int size;
		if(ioctl(fd, DIOCGSECTORSIZE, &size) == 0 && size) disk->block_size = size;
	}
#ifdef DIOCGPHYSPATH
	if(flags & HTOP_DISK_PHYS_PATH_FLAG) {
		char phys_path[MAXPATHLEN];
		if(ioctl(fd, DIOCGPHYSPATH, phys_path) == 0) disk->phys_path = xStrdup(phys_path);
	}
#endif
	if(flags & HTOP_DISK_CAPACITY_FLAG) {
		off_t size;
		if(ioctl(fd, DIOCGMEDIASIZE, &size) == 0) {
			disk->block_count = size / disk->block_size;
		} else {
			size = lseek(fd, SEEK_END, 0);
			if(size >= 0) disk->block_count = size / disk->block_size;
		}
	}
	close(fd);
}

static char *get_cam_path(const char *name) {
	size_t name_len = strlen(name);
	size_t unit_i = name_len;
	while(isdigit(name[unit_i - 1])) if(!--unit_i) return NULL;
	if(unit_i == name_len) return NULL;
	int fd = open("/dev/xpt0", O_RDWR);
	if(fd == -1) return NULL;
	struct dev_match_pattern match_pattern = {
		.type = DEV_MATCH_PERIPH,
		.pattern.periph_pattern.flags = PERIPH_MATCH_NAME | PERIPH_MATCH_UNIT,
		.pattern.periph_pattern.unit_number = atoi(name + unit_i)
	};
	assert(sizeof match_pattern.pattern.periph_pattern.periph_name > unit_i);
	memcpy(match_pattern.pattern.periph_pattern.periph_name, name, unit_i);
	struct dev_match_result match_result;
	union ccb ccb = {
		.cdm = {
			.ccb_h.path_id = CAM_XPT_PATH_ID,
			.ccb_h.target_id = CAM_TARGET_WILDCARD,
			.ccb_h.target_lun = CAM_LUN_WILDCARD,
			.ccb_h.func_code = XPT_DEV_MATCH,
			.num_patterns = 1,
			.pattern_buf_len = sizeof match_pattern,
			.patterns = &match_pattern,
			.num_matches = 0,
			.match_buf_len = sizeof match_result,
			.matches = &match_result
		}
	};
	if(ioctl(fd, CAMIOCOMMAND, &ccb) < 0) {
		close(fd);
		return NULL;
	}
	close(fd);
	if(ccb.ccb_h.status != CAM_REQ_CMP) return NULL;
	if(ccb.cdm.status != CAM_DEV_MATCH_LAST && ccb.cdm.status != CAM_DEV_MATCH_MORE) {
		return NULL;
	}
/*
	assert(ccb.cdm.num_matches <= sizeof match_results / sizeof *match_results);
	for(int i = 0; i < ccb.cdm.num_matches; i++) {
		const struct dev_match_result *match = ccb.cdm.matches + i;
		assert(match->type == DEV_MATCH_PERIPH);
*/
	assert(ccb.cdm.num_matches <= 1);
	if(!ccb.cdm.num_matches) return NULL;
	assert(ccb.cdm.matches->type == DEV_MATCH_PERIPH);
	char *phys_path = xMalloc(41);
	int len = snprintf(phys_path, 41, "scbus%u-target%u-lun%u",
		(unsigned int)ccb.cdm.matches->result.periph_result.path_id,
		(unsigned int)ccb.cdm.matches->result.periph_result.target_id,
		(unsigned int)ccb.cdm.matches->result.periph_result.target_lun);
	if(len < 40) phys_path = xRealloc(phys_path, len + 1);
	return phys_path;
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
			if(!devstat->block_size ||
			  (super->settings->disk_flags & (HTOP_DISK_PHYS_PATH_FLAG | HTOP_DISK_CAPACITY_FLAG))) {
				fill_from_device_node(disk, super->settings->disk_flags, !devstat->block_size);
			}
			if(super->settings->disk_flags & HTOP_DISK_DEVID_FLAG) {
				struct gconfig *kvp;
				LIST_FOREACH(kvp, &p->lg_config, lg_config) {
					if(strcmp(kvp->lg_name, "ident") == 0) {
						disk->devid = xStrdup(kvp->lg_val);
						break;
					}
				}
			}
			if(super->settings->disk_flags & HTOP_DISK_CAM_PATH_FLAG) {
				fbsd_disk->cam_path = get_cam_path(p->lg_name);
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
