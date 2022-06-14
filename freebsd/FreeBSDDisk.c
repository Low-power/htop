/*
htop - freebsd/FreeBSDDisk.c
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "Disk.h"
#include "Settings.h"
#include <sys/time.h>
#include <stdint.h>

typedef enum {
	// Add platform-specific fields here, with ids >= 100
	HTOP_DISK_BUSY_TIME_FIELD = 100,
	HTOP_DISK_PERCENT_BUSY_FIELD,
	HTOP_FREEBSD_DISK_FIELD_COUNT
} FreeBSDDiskField;

typedef struct {
	Disk super;
	struct timeval duration;
	uint64_t busy_time; // In 10-millisecond
	float percent_busy;
} FreeBSDDisk;
}*/

#include "FreeBSDDisk.h"
#include "CRT.h"

const FieldData Disk_fields[] = {
	[0] = { .name = "", .title = NULL, .description = NULL, .flags = 0 },
	[HTOP_DISK_NAME_FIELD] = { .name = "NAME", .title = "NAME     ", .description = "Canonical device name", .flags = 0 },
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
	[HTOP_DISK_BUSY_TIME_FIELD] = { .name = "BUSY_TIME", .title = "BUSYTIME ", .description = "Total time of when the disk had one or more outstanding transactions", .flags = 0 },
	[HTOP_DISK_CREATION_TIME_FIELD] = { .name = "CREATION_TIME", .title = "BTIME ", .description = "Time when the disk appeared", .flags = 0 },
	[HTOP_DISK_OPERATION_RATE_FIELD] = { .name = "OPERATION_RATE", .title = " OPS/S ", .description = "Operations per second", .flags = 0 },
	[HTOP_DISK_READ_OPERATION_RATE_FIELD] = { .name = "READ_OPERATION_RATE", .title = "ROPS/S ", .description = "Read operations per second", .flags = 0 },
	[HTOP_DISK_WRITE_OPERATION_RATE_FIELD] = { .name = "WRITE_OPERATION_RATE", .title = "WOPS/S ", .description = "Write operations per second", .flags = 0 },
	[HTOP_DISK_READ_BLOCK_RATE_FIELD] = { .name = "READ_BLOCK_RATE", .title = "  RBLK/S ", .description = "Read blocks per second", .flags = 0 },
	[HTOP_DISK_WRITE_BLOCK_RATE_FIELD] = { .name = "WRITE_BLOCK_RATE", .title = "  WBLK/S ", .description = "Write blocks per second", .flags = 0 },
	[HTOP_DISK_READ_BYTE_RATE_FIELD] = { .name = "READ_BYTE_RATE", .title = "RBYTE/S ", .description = "Read bytes per second", .flags = 0 },
	[HTOP_DISK_WRITE_BYTE_RATE_FIELD] = { .name = "WRITE_BYTE_RATE", .title = "WBYTE/S ", .description = "Write bytes per second", .flags = 0 },
	[HTOP_DISK_PERCENT_UTIL_FIELD] = { .name = "PERCENT_UTIL", .title = "UTIL% ", .description = "Percentage of time during transactions", .flags = HTOP_DISK_PERCENT_UTIL_FLAG },
	[HTOP_DISK_PERCENT_BUSY_FIELD] = { .name = "PERCENT_BUSY", .title = "BUSY% ", .description = "Percentage of time the disk had one or more transactions outstanding", .flags = 0 },
	[HTOP_FREEBSD_DISK_FIELD_COUNT] = { .name = "*** report bug! ***", .title = NULL, .description = NULL, .flags = 0 }
};

const unsigned int Disk_field_count = HTOP_FREEBSD_DISK_FIELD_COUNT;

const DiskField Disk_default_fields[] = {
	HTOP_DISK_QUEUE_LENGTH_FIELD,
	HTOP_DISK_OPERATION_RATE_FIELD,
	HTOP_DISK_READ_OPERATION_RATE_FIELD,
	HTOP_DISK_READ_BYTE_RATE_FIELD,
	HTOP_DISK_WRITE_OPERATION_RATE_FIELD,
	HTOP_DISK_WRITE_BYTE_RATE_FIELD,
	HTOP_DISK_PERCENT_UTIL_FIELD,
	(DiskField)HTOP_DISK_PERCENT_BUSY_FIELD,
	HTOP_DISK_NAME_FIELD,
	HTOP_DISK_PHYS_PATH_FIELD,
	0
};

static void FreeBSDDisk_writeField(const Disk *super, RichString *s, DiskField field) {
	const FreeBSDDisk *this = (const FreeBSDDisk *)super;
	switch((unsigned int)field) {
		case HTOP_DISK_BUSY_TIME_FIELD:
			CRT_printTime(s, this->busy_time);
			return;
		case HTOP_DISK_PERCENT_BUSY_FIELD:
			Disk_printPercent(s, this->percent_busy);
			return;
		default:
			base_Disk_writeField(super, s, field);
			return;
	}
}

static long int FreeBSDDisk_compare(const void *o1, const void *o2) {
	const FreeBSDDisk *d1;
	const FreeBSDDisk *d2;
	const Settings *settings = ((const Disk *)o1)->settings;
	if(settings->direction == 1) {
		d1 = o1;
		d2 = o2;
	} else {
		d2 = o1;
		d1 = o2;
	}
	switch((unsigned int)settings->disk_sort_key) {
		case HTOP_DISK_BUSY_TIME_FIELD:
			return uintcmp(d2->busy_time, d1->busy_time);
		case HTOP_DISK_PERCENT_BUSY_FIELD:
			return d2->percent_busy - d1->percent_busy;
		default:
			return Disk_compare(o1, o2);
	}
}

DiskClass FreeBSDDisk_class = {
	.super = {
		.extends = Class(Disk),
		.delete = FreeBSDDisk_delete,
		.compare = FreeBSDDisk_compare
	},
	.writeField = FreeBSDDisk_writeField
};

FreeBSDDisk *FreeBSDDisk_new(const Settings *settings) {
	FreeBSDDisk *this = xCalloc(1, sizeof(FreeBSDDisk));
	Object_setClass(this, Class(FreeBSDDisk));
	Disk_init(&this->super, settings);
	return this;
}

void FreeBSDDisk_delete(Object *this) {
	Disk_done((Disk *)this);
	free(this);
}
