/*
htop - linux/LinuxDiskList.c
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "Object.h"
#include "DiskList.h"
#include "Vector.h"
#include <stdio.h>

typedef struct {
	Object super;
	char *orig_name;
	char *alt_name;
} AltDiskName;

typedef struct {
	DiskList super;
	FILE *diskstats_file;
	Vector *disk_id_cache;
	Vector *disk_path_cache;
} LinuxDiskList;
}*/

#include "config.h"
#include "LinuxDiskList.h"
#include "LinuxDisk.h"
#include "CRT.h"
#include "XAlloc.h"
#include "StringUtils.h"
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

DiskList *DiskList_new(const Settings *settings) {
	LinuxDiskList *disk_list = xMalloc(sizeof(LinuxDiskList));
	DiskList_init(&disk_list->super, Class(LinuxDisk), settings);
	disk_list->diskstats_file = fopen(PROCDIR "/diskstats", "r");
	if(!disk_list->diskstats_file) CRT_fatalError("fopen: " PROCDIR "/diskstats", 0);
	disk_list->disk_id_cache = NULL;
	disk_list->disk_path_cache = NULL;
	return (DiskList *)disk_list;
}

void DiskList_delete(DiskList *super) {
	DiskList_done(super);
	LinuxDiskList *this = (LinuxDiskList *)super;
	fclose(this->diskstats_file);
	if(this->disk_id_cache) Vector_delete(this->disk_id_cache);
	if(this->disk_path_cache) Vector_delete(this->disk_path_cache);
	free(this);
}

static uint32_t get_block_size(const char *name, size_t len) {
	char path[11 + len + 26];
	memcpy(path, "/sys/block/", 11);
	memcpy(path + 11, name, len);
	strcpy(path + 11 + len, "/queue/logical_block_size");
	FILE *f = fopen(path, "r");
	if(!f) {
		strcpy(path + 11 + len + 7, "hw_sector_size");
		f = fopen(path, "r");
		if(!f) return 512;	// TODO: try ioctl(2) as a fallback
	}
	char buffer[8];
	if(!fgets(buffer, sizeof buffer, f)) {
		fclose(f);
		return 512;
	}
	fclose(f);
	char *end_p;
	unsigned long int size = strtoul(buffer, &end_p, 10);
	if((*end_p && *end_p != '\n') || !size) return 512;
	return size;
}

static time_t get_creation_time(const char *name, size_t len) {
	char path[5 + len + 1];
	memcpy(path, "/dev/", 5);
	memcpy(path + 5, name, len + 1);
	struct stat st;
	if(stat(path, &st) < 0) return -1;
	return st.st_mtime;
}

static long int AltDiskName_compare(const void *o1, const void *o2) {
	const AltDiskName *n1 = o1;
	const AltDiskName *n2 = o2;
	return strcmp(n1->orig_name, n2->orig_name);
}

static void AltDiskName_delete(Object *super) {
	AltDiskName *this = (AltDiskName *)super;
	free(this->orig_name);
	free(this->alt_name);
	free(this);
}

ObjectClass AltDiskName_class = {
	.compare = AltDiskName_compare,
	.delete = AltDiskName_delete
};

static char *get_alternative_name_from_cache(const char *name, const Vector *cache) {
	int i = Vector_size(cache);
	while(i > 0) {
		const AltDiskName *n = (const AltDiskName *)Vector_get(cache, --i);
		if(strcmp(name, n->orig_name) == 0) return xStrdup(n->alt_name);
	}
	return NULL;
}

static char *get_alternative_name(const char *name, size_t len, const char *disk_link_dir_path, Vector **cache) {
	if(*cache) {
		char *alt_name = get_alternative_name_from_cache(name, *cache);
		if(alt_name) return alt_name;
		Vector_prune(*cache);
	} else {
		*cache = Vector_new(Class(AltDiskName), true, DEFAULT_SIZE);
	}

	char *r = NULL;
	DIR *dir = opendir(disk_link_dir_path);
	if(!dir) return NULL;
	struct dirent *e;
	while((e = readdir(dir))) {
		if(e->d_name[0] == '.') continue;
		char *path = String_cat(disk_link_dir_path, e->d_name);
		char buffer[32];
		int target_len = readlink(path, buffer, sizeof buffer);
		free(path);
		if(target_len < 7 || memcmp(buffer, "../../", 6)) continue;
		target_len -= 6;
		char *orig_name = xMalloc(target_len + 1);
		memcpy(orig_name, buffer + 6, target_len);
		orig_name[target_len] = 0;
		if(!r && (size_t)target_len == len && memcmp(orig_name, name, len) == 0) {
			r = xStrdup(e->d_name);
		}
		char *alt_name = xStrdup(e->d_name);
		AltDiskName *n = xMalloc(sizeof(AltDiskName));
		Object_setClass(n, Class(AltDiskName));
		n->orig_name = orig_name;
		n->alt_name = alt_name;
		Vector_add(*cache, n);
	}
	closedir(dir);
	return r;
}

static char *get_phys_path(LinuxDiskList *disk_list, const char *name, size_t len) {
	return get_alternative_name(name, len, "/dev/disk/by-path/", &disk_list->disk_path_cache);
}

static char *get_devid(LinuxDiskList *disk_list, const char *name, size_t len) {
	return get_alternative_name(name, len, "/dev/disk/by-id/", &disk_list->disk_id_cache);
}

void DiskList_internalScan(DiskList *super, double interval) {
	LinuxDiskList *this = (LinuxDiskList *)super;
	if(interval < 0) interval = 0.000001;
	char last_name[32];
	*last_name = 0;
	size_t last_name_len = 0;
	char *line;
	rewind(this->diskstats_file);
	while((line = String_readLine(this->diskstats_file))) {
		int major, minor;
		char name[32];
		unsigned long int read_op_count, read_merge_count, read_block_count;
		unsigned int read_time;
		unsigned long int write_op_count, write_merge_count, write_block_count;
		unsigned int write_time;
		unsigned int pending_op_count, operation_time, queue_time;
		int n = sscanf(line, "%d %d %31s %lu %lu %lu %u %lu %lu %lu %u %u %u %u",
			&major, &minor, name,
			&read_op_count, &read_merge_count, &read_block_count, &read_time,
			&write_op_count, &write_merge_count, &write_block_count, &write_time,
			&pending_op_count, &operation_time, &queue_time);
		free(line);
		if(n < 11) continue;
		size_t name_len = strlen(name);
		if(*last_name && name_len > 2) {
			if(last_name_len > 2 && name_len > 3 && name_len > last_name_len && islower(name[0]) && name[1] == 'd' && islower(name[2]) && strncmp(name, last_name, last_name_len) == 0) {
				unsigned int i = 3;
				while(islower(name[i])) i++;
				if(i == last_name_len && i < name_len) {
					while(isdigit(name[i])) i++;
					if(i >= name_len) continue;
				}
			} else if(isdigit(name[name_len - 1])) {
				unsigned int i = name_len - 2;
				while(isdigit(name[i]) && i) i--;
				if(i && name[i] == 'p' && i == last_name_len && strncmp(name, last_name, last_name_len) == 0) {
					continue;
				}
			}
		}
		memcpy(last_name, name, name_len + 1);
		last_name_len = name_len;

		bool is_existing;
		Disk *disk = DiskList_getOrCreate(super, name, &is_existing, (DiskConstructor)LinuxDisk_new);
		LinuxDisk *linux_disk = (LinuxDisk *)disk;
		if(!is_existing) {
			disk->block_size = get_block_size(name, name_len);
			disk->creation_time = get_creation_time(name, name_len);
			if(super->settings->disk_flags & HTOP_DISK_PHYS_PATH_FLAG) {
				disk->phys_path = get_phys_path(this, name, name_len);
			}
			if(super->settings->disk_flags & HTOP_DISK_DEVID_FLAG) {
				disk->devid = get_devid(this, name, name_len);
			}
			DiskList_add(super, disk);
		}
		disk->queue_length = pending_op_count;
		uint64_t operation_count = read_op_count + write_op_count;
		disk->operation_rate = (operation_count - disk->operation_count) / interval;
		disk->read_operation_rate = (read_op_count - disk->read_operation_count) / interval;
		disk->write_operation_rate = (write_op_count - disk->write_operation_count) / interval;
		disk->read_block_rate = (read_block_count - disk->read_block_count) / interval;
		disk->write_block_rate = (write_block_count - disk->write_block_count) / interval;
		disk->operation_count = operation_count;
		disk->read_operation_count = read_op_count;
		disk->write_operation_count = write_op_count;
		disk->read_block_count = read_block_count;
		disk->write_block_count = write_block_count;
		if(super->settings->disk_flags & HTOP_DISK_PERCENT_UTIL_FLAG) {
			disk->percent_util =
				(operation_time - linux_disk->oper_time_msec) / interval / 10;
		}
		linux_disk->oper_time_msec = operation_time;
		disk->oper_time = operation_time / 10;
		disk->updated = true;
	}
}
