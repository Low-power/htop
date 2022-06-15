/*
htop - linux/LinuxDisk.c
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "Disk.h"
#include "Settings.h"

#if 0
typedef enum {
	// Add platform-specific fields here, with ids >= 100
	HTOP_LINUX_DISK_FIELD_COUNT = 100
} LinuxDiskField;
#endif

typedef struct {
	Disk super;
	int major;
	int minor;
	unsigned int oper_time_msec;
} LinuxDisk;
}*/

#include "LinuxDisk.h"

const FieldData Disk_fields[] = {
	[0] = { .name = "", .title = NULL, .description = NULL, .flags = 0 },
	[HTOP_DISK_NAME_FIELD] = { .name = "NAME", .title = "NAME         ", .description = "Canonical device name", .flags = 0 },
	[HTOP_DISK_PHYS_PATH_FIELD] = { .name = "PHYS_PATH", .title = "PHYS_PATH                                                ", .description = "Predictable device name based on phyiscal location", .flags = HTOP_DISK_PHYS_PATH_FLAG },
	[HTOP_DISK_DEVID_FIELD] = { .name = "DEVID", .title = "DEVID                                                   ", .description = "Predictable device name based on model and serial number", .flags = HTOP_DISK_DEVID_FLAG },
	[HTOP_DISK_BLOCK_SIZE_FIELD] = { .name = "BLOCK_SIZE", .title = " BLKSZ ", .description = "I/O block size", .flags = 0 },
	[HTOP_DISK_QUEUE_LENGTH_FIELD] = { .name = "QUEUE_LENGTH", .title = "QUEUE ", .description = "Request queue length", .flags = 0 },
	[HTOP_DISK_OPERATIONS_FIELD] = { .name = "OPERATIONS", .title = "   OPS ", .description = "Total operations performed", .flags = 0 },
	[HTOP_DISK_READ_OPS_FIELD] = { .name = "READ_OPERATIONS", .title = " R_OPS ", .description = "Total read operations performed", .flags = 0 },
	[HTOP_DISK_WRITE_OPS_FIELD] = { .name = "WRITE_OPERATIONS", .title = " W_OPS ", .description = "Total write operations performed", .flags = 0 },
	[HTOP_DISK_READ_BLOCKS_FIELD] = { .name = "READ_BLOCKS", .title = "R_BLKS ", .description = "Total blocks readen", .flags = 0 },
	[HTOP_DISK_WRITE_BLOCKS_FIELD] = { .name = "WRITE_BLOCKS", .title = "W_BLKS ", .description = "Total blocks written", .flags = 0 },
	[HTOP_DISK_READ_BYTES_FIELD] = { .name = "READ_BYTES", .title = " RBYTES ", .description = "Total bytes readon", .flags = 0 },
	[HTOP_DISK_WRITE_BYTES_FIELD] = { .name = "WRITE_BYTES", .title = " WBYTES ", .description = "Total bytes written", .flags = 0 },
	[HTOP_DISK_OPER_TIME_FIELD] = { .name = "OPER_TIME", .title = "OPERTIME ", .description = "Total time of transactions", .flags = 0 },
	[HTOP_DISK_CREATION_TIME_FIELD] = { .name = "CREATION_TIME", .title = "BTIME ", .description = "Time when the disk appeared", .flags = 0 },
	[HTOP_DISK_OPERATION_RATE_FIELD] = { .name = "OPERATION_RATE", .title = " OPS/S ", .description = "Operations per second", .flags = 0 },
	[HTOP_DISK_READ_OPERATION_RATE_FIELD] = { .name = "READ_OPERATION_RATE", .title = "ROPS/S ", .description = "Read operations per second", .flags = 0 },
	[HTOP_DISK_WRITE_OPERATION_RATE_FIELD] = { .name = "WRITE_OPERATION_RATE", .title = "WOPS/S ", .description = "Write operations per second", .flags = 0 },
	[HTOP_DISK_READ_BLOCK_RATE_FIELD] = { .name = "READ_BLOCK_RATE", .title = "  RBLK/S ", .description = "Read blocks per second", .flags = 0 },
	[HTOP_DISK_WRITE_BLOCK_RATE_FIELD] = { .name = "WRITE_BLOCK_RATE", .title = "  WBLK/S ", .description = "Write blocks per second", .flags = 0 },
	[HTOP_DISK_READ_BYTE_RATE_FIELD] = { .name = "READ_BYTE_RATE", .title = "RBYTE/S ", .description = "Read bytes per second", .flags = 0 },
	[HTOP_DISK_WRITE_BYTE_RATE_FIELD] = { .name = "WRITE_BYTE_RATE", .title = "WBYTE/S ", .description = "Write bytes per second", .flags = 0 },
	[HTOP_DISK_PERCENT_UTIL_FIELD] = { .name = "PERCENT_UTIL", .title = "UTIL% ", .description = "Percentage of time during transactions", .flags = HTOP_DISK_PERCENT_UTIL_FLAG },
};

const unsigned int Disk_field_count = HTOP_BASE_DISK_FIELD_COUNT;

const DiskField Disk_default_fields[] = {
	HTOP_DISK_QUEUE_LENGTH_FIELD,
	HTOP_DISK_OPERATION_RATE_FIELD,
	HTOP_DISK_READ_OPERATION_RATE_FIELD,
	HTOP_DISK_READ_BYTE_RATE_FIELD,
	HTOP_DISK_WRITE_OPERATION_RATE_FIELD,
	HTOP_DISK_WRITE_BYTE_RATE_FIELD,
	HTOP_DISK_PERCENT_UTIL_FIELD,
	HTOP_DISK_NAME_FIELD,
	HTOP_DISK_PHYS_PATH_FIELD,
	0
};

DiskClass LinuxDisk_class = {
	.super = {
		.extends = Class(Disk),
		.delete = LinuxDisk_delete
	}
};

LinuxDisk *LinuxDisk_new(const Settings *settings) {
	LinuxDisk *this = xCalloc(1, sizeof(LinuxDisk));
	Object_setClass(this, Class(LinuxDisk));
	Disk_init(&this->super, settings);
	return this;
}

void LinuxDisk_delete(Object *this) {
	Disk_done((Disk *)this);
	free(this);
}
