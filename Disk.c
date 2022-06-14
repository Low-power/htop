/*
htop - Disk.c
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "RichString.h"
#include "Object.h"
#include "FieldData.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define uintcmp(n1,n2) ((n1)>(n2)?1:((n1)<(n2)?-1:0))

#define HTOP_DISK_PHYS_PATH_FLAG (1 << 0)
#define HTOP_DISK_DEVID_FLAG (1 << 1)

typedef enum {
	HTOP_DISK_NULL_FIELD = 0,
	HTOP_DISK_NAME_FIELD,
	HTOP_DISK_PHYS_PATH_FIELD,
	HTOP_DISK_DEVID_FIELD,
	HTOP_DISK_BLOCK_SIZE_FIELD,
	HTOP_DISK_QUEUE_LENGTH_FIELD,
	HTOP_DISK_OPERATIONS_FIELD,
	HTOP_DISK_READ_OPS_FIELD,
	HTOP_DISK_WRITE_OPS_FIELD,
	HTOP_DISK_READ_BLOCKS_FIELD,
	HTOP_DISK_WRITE_BLOCKS_FIELD,
	HTOP_DISK_READ_BYTES_FIELD,
	HTOP_DISK_WRITE_BYTES_FIELD,
	HTOP_DISK_OPER_TIME_FIELD,
	HTOP_DISK_BUSY_TIME_FIELD,
	HTOP_DISK_CREATION_TIME_FIELD,
	HTOP_DISK_OPERATION_RATE_FIELD,
	HTOP_DISK_READ_OPERATION_RATE_FIELD,
	HTOP_DISK_WRITE_OPERATION_RATE_FIELD,
	HTOP_DISK_READ_BLOCK_RATE_FIELD,
	HTOP_DISK_WRITE_BLOCK_RATE_FIELD,
	HTOP_DISK_READ_BYTE_RATE_FIELD,
	HTOP_DISK_WRITE_BYTE_RATE_FIELD,
	HTOP_DISK_PERCENT_BUSY_FIELD,
	HTOP_BASE_DISK_FIELD_COUNT
} DiskField;

typedef struct {
	Object super;
	const struct Settings_ *settings;
	bool updated;
	char *name;
	char *phys_path;
	char *devid;
	uint32_t block_size;
	uint64_t queue_length;
	uint64_t operation_count;
	uint64_t read_operation_count;
	uint64_t write_operation_count;
	uint64_t read_block_count;
	uint64_t write_block_count;
	uint64_t oper_time; // In 10-millisecond
	uint64_t busy_time; // In 10-millisecond
	time_t creation_time;
	uint32_t operation_rate;
	uint32_t read_operation_rate;
	uint32_t write_operation_rate;
	uint32_t read_block_rate;
	uint32_t write_block_rate;
	float percent_busy;
} Disk;

typedef Disk *(*DiskConstructor)(const struct Settings_ *);
typedef void (*DiskWriteFieldFunction)(const Disk *, RichString *, DiskField);

typedef struct {
	ObjectClass super;
	DiskWriteFieldFunction writeField;
} DiskClass;

#define As_Disk(this_) ((DiskClass *)((this_)->super.klass))

#define Disk_writeField(this_,str_,field_) (As_Disk(this_)->writeField((this_), (str_), (field_)))

// Implemented in platform-specific code:
extern const FieldData Disk_fields[];
extern const unsigned int Disk_field_count;
extern const DiskField Disk_default_fields[];
}*/

#include "Disk.h"
#include "Settings.h"
#include "CRT.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>

static void human_readable_decimal(RichString *s, uint64_t n, int unit_prefix, unsigned int scale, int width, bool coloring) {
	assert(width > 1);
	int scale_count = 0;
	switch(unit_prefix) {
		case 0:
			if(n < 1000 * scale) break;
			n /= 1000;
			unit_prefix = 'k';
			scale_count++;
		case 'k':
			if(n < 1000 * scale) break;
			n /= 1000;
			unit_prefix = 'M';
			scale_count++;
		case 'M':
			if(n < 1000 * scale) break;
			n /= 1000;
			unit_prefix = 'G';
			scale_count++;
		case 'G':
			if(n < 1000 * scale) break;
			n /= 1000;
			unit_prefix = 'T';
			scale_count++;
		case 'T':
			if(n < 1000 * scale) break;
			n /= 1000;
			unit_prefix = 'P';
			scale_count++;
		case 'P':
			if(n < 1000 * scale) break;
			n /= 1000;
			unit_prefix = 'E';
			scale_count++;
			break;
		default:
			abort();
	}
	char buffer[16];
	if(unit_prefix) {
		xSnprintf(buffer, sizeof buffer, "%*llu%c ", width - 1, (unsigned long long int)n, unit_prefix);
	} else {
		xSnprintf(buffer, sizeof buffer, "%*llu ", width, (unsigned long long int)n);
	}
	int attr = CRT_colors[coloring && scale_count > 2 ? HTOP_LARGE_NUMBER_COLOR : HTOP_PROCESS_COLOR];
	RichString_append(s, attr, buffer);
}

static void human_readable_binary(RichString *s, uint64_t n, int unit_prefix, unsigned int scale, int width, bool coloring) {
	assert(width > 2);
	int scale_count = 0;
	switch(unit_prefix) {
		case 0:
			if(n < 1024 * scale) break;
			n /= 1024;
			unit_prefix = 'K';
			scale_count++;
		case 'K':
			if(n < 1024 * scale) break;
			n /= 1024;
			unit_prefix = 'M';
			scale_count++;
		case 'M':
			if(n < 1024 * scale) break;
			n /= 1024;
			unit_prefix = 'G';
			scale_count++;
		case 'G':
			if(n < 1024 * scale) break;
			n /= 1024;
			unit_prefix = 'T';
			scale_count++;
		case 'T':
			if(n < 1024 * scale) break;
			n /= 1024;
			unit_prefix = 'P';
			scale_count++;
		case 'P':
			if(n < 1024 * scale) break;
			n /= 1024;
			unit_prefix = 'E';
			scale_count++;
			break;
		default:
			abort();
	}
	char buffer[16];
	if(unit_prefix) {
		xSnprintf(buffer, sizeof buffer, "%*llu%ci ", width - 2, (unsigned long long int)n, unit_prefix);
	} else {
		xSnprintf(buffer, sizeof buffer, "%*llu ", width, (unsigned long long int)n);
	}
	int attr = CRT_colors[coloring && scale_count > 2 ? HTOP_LARGE_NUMBER_COLOR : HTOP_PROCESS_COLOR];
	RichString_append(s, attr, buffer);
}

void base_Disk_writeField(const Disk *this, RichString *s, DiskField field) {
	char buffer[256];
	int attr = CRT_colors[HTOP_DEFAULT_COLOR];
	switch(field) {
			struct tm tm;
			size_t len;
		case HTOP_DISK_NAME_FIELD:
			xSnprintf(buffer, sizeof buffer, "%-8s ", this->name);
			break;
		case HTOP_DISK_PHYS_PATH_FIELD:
			xSnprintf(buffer, sizeof buffer, "%-56s ",
				this->phys_path ? this->phys_path : "-");
			break;
		case HTOP_DISK_DEVID_FIELD:
			xSnprintf(buffer, sizeof buffer, "%-56s ", this->devid ? this->devid : "-");
			break;
		case HTOP_DISK_BLOCK_SIZE_FIELD:
			xSnprintf(buffer, sizeof buffer, "%6u ", (unsigned int)this->block_size);
			break;
		case HTOP_DISK_QUEUE_LENGTH_FIELD:
			xSnprintf(buffer, sizeof buffer, "%5lu ", (unsigned long int)this->queue_length);
			break;
		case HTOP_DISK_OPERATIONS_FIELD:
			human_readable_decimal(s, this->operation_count, 0, 10, 6, true);
			return;
		case HTOP_DISK_READ_OPS_FIELD:
			human_readable_decimal(s, this->read_operation_count, 0, 10, 6, true);
			return;
		case HTOP_DISK_WRITE_OPS_FIELD:
			human_readable_decimal(s, this->write_operation_count, 0, 10, 6, true);
			return;
		case HTOP_DISK_READ_BLOCKS_FIELD:
			human_readable_decimal(s, this->read_block_count, 0, 10, 6, true);
			return;
		case HTOP_DISK_WRITE_BLOCKS_FIELD:
			human_readable_decimal(s, this->write_block_count, 0, 10, 6, true);
			return;
		case HTOP_DISK_READ_BYTES_FIELD:
			human_readable_binary(s, this->read_block_count * this->block_size, 0, 10, 7, true);
			return;
		case HTOP_DISK_WRITE_BYTES_FIELD:
			human_readable_binary(s, this->write_block_count *this->block_size, 0, 10, 7, true);
			return;
		case HTOP_DISK_OPER_TIME_FIELD:
			CRT_printTime(s, this->oper_time);
			return;
		case HTOP_DISK_BUSY_TIME_FIELD:
			CRT_printTime(s, this->busy_time);
			return;
		case HTOP_DISK_CREATION_TIME_FIELD:
			localtime_r(&this->creation_time, &tm);
			len = strftime(buffer, sizeof buffer,
				(this->creation_time > time(NULL) - 86400) ? "%R " : "%b%d ", &tm);
			if(len != 6) {
				if(len < 6) memset(buffer + len, ' ', 6 - len);
				buffer[6] = 0;
			}
			break;
		case HTOP_DISK_OPERATION_RATE_FIELD:
#if 0
			xSnprintf(buffer, sizeof buffer, "%6u ",
				(unsigned int)(this->read_operation_rate + this->write_operation_rate));
#else
			xSnprintf(buffer, sizeof buffer, "%6u ", (unsigned int)this->operation_rate);
#endif
			break;
		case HTOP_DISK_READ_OPERATION_RATE_FIELD:
			xSnprintf(buffer, sizeof buffer, "%6u ", (unsigned int)this->read_operation_rate);
			break;
		case HTOP_DISK_WRITE_OPERATION_RATE_FIELD:
			xSnprintf(buffer, sizeof buffer, "%6u ", (unsigned int)this->write_operation_rate);
			break;
		case HTOP_DISK_READ_BLOCK_RATE_FIELD:
			xSnprintf(buffer, sizeof buffer, "%8u ", (unsigned int)this->read_block_rate);
			break;
		case HTOP_DISK_WRITE_BLOCK_RATE_FIELD:
			xSnprintf(buffer, sizeof buffer, "%8u ", (unsigned int)this->write_block_rate);
			break;
		case HTOP_DISK_READ_BYTE_RATE_FIELD:
			human_readable_binary(s, this->read_block_rate * this->block_size, 0, 10, 7, true);
			return;
		case HTOP_DISK_WRITE_BYTE_RATE_FIELD:
			human_readable_binary(s, this->write_block_rate * this->block_size, 0, 10, 7, true);
			return;
		case HTOP_DISK_PERCENT_BUSY_FIELD:
			if(this->percent_busy > 80) attr = CRT_colors[HTOP_LARGE_NUMBER_COLOR];
			xSnprintf(buffer, sizeof buffer, "%5.1f ", this->percent_busy);
			break;
		default:
			strcpy(buffer, "- ");
			break;
	}
	RichString_append(s, attr, buffer);
}

void Disk_display(Object *super, RichString *s) {
	const Disk *this = (const Disk *)super;
	const unsigned int *field = this->settings->disk_fields;
	RichString_prune(s);
	while(*field) {
		Disk_writeField(this, s, *field);
		field++;
	}
	assert(s->chlen > 0);
}

void Disk_done(Disk *this) {
	free(this->name);
	free(this->phys_path);
	free(this->devid);
}

static void Disk_inherit(ObjectClass *super_class) {
	if(!super_class->display) super_class->display = Disk_display;
	if(!super_class->compare) super_class->compare = Disk_compare;
	DiskClass *class = (DiskClass *)super_class;
	if(!class->writeField) class->writeField = base_Disk_writeField;
}

DiskClass Disk_class = {
	.super = {
		.extends = Class(Object),
		.inherit = Disk_inherit,
		.display = Disk_display,
		.compare = Disk_compare
	},
	.writeField = base_Disk_writeField
};

void Disk_init(Disk *this, const struct Settings_ *settings) {
	this->settings = settings;
	this->creation_time = -1;
}

long int Disk_nameCompare(const void *o1, const void *o2) {
	const Disk *d1 = o1;
	const Disk *d2 = o2;
	const Settings *settings = d1->settings;
	int (*cmp)(const char *, const char *) = settings ? settings->sort_strcmp : strcmp;
	return cmp(d1->name, d2->name);
}

long int Disk_compare(const void *o1, const void *o2) {
	const Disk *d1;
	const Disk *d2;
	const Settings *settings = ((const Disk *)o1)->settings;
	if(settings->direction == 1) {
		d1 = o1;
		d2 = o2;
	} else {
		d2 = o1;
		d1 = o2;
	}
	switch(settings->disk_sort_key) {
		case HTOP_DISK_NAME_FIELD:
		default:
			return settings->sort_strcmp(d1->name, d2->name);
		case HTOP_DISK_PHYS_PATH_FIELD:
			return settings->sort_strcmp(d1->phys_path ? d1->phys_path : "",
				d2->phys_path ? d2->phys_path : "");
		case HTOP_DISK_DEVID_FIELD:
			return settings->sort_strcmp(d1->devid ? d1->devid : "", d2->devid ? d2->devid : "");
		case HTOP_DISK_BLOCK_SIZE_FIELD:
			return uintcmp(d1->block_size, d2->block_size);
		case HTOP_DISK_QUEUE_LENGTH_FIELD:
			return uintcmp(d2->queue_length, d1->queue_length);
		case HTOP_DISK_OPERATIONS_FIELD:
			return uintcmp(d2->operation_count, d1->operation_count);
		case HTOP_DISK_READ_OPS_FIELD:
			return uintcmp(d2->read_operation_count, d1->read_operation_count);
		case HTOP_DISK_WRITE_OPS_FIELD:
			return uintcmp(d2->write_operation_count, d1->write_operation_count);
		case HTOP_DISK_READ_BLOCKS_FIELD:
		case HTOP_DISK_READ_BYTES_FIELD:
			return uintcmp(d2->read_block_count, d1->read_block_count);
		case HTOP_DISK_WRITE_BLOCKS_FIELD:
		case HTOP_DISK_WRITE_BYTES_FIELD:
			return uintcmp(d2->write_block_count, d1->write_block_count);
		case HTOP_DISK_OPER_TIME_FIELD:
			return uintcmp(d2->oper_time, d1->oper_time);
		case HTOP_DISK_BUSY_TIME_FIELD:
			return uintcmp(d2->busy_time, d1->busy_time);
		case HTOP_DISK_CREATION_TIME_FIELD:
			return d1->creation_time - d2->creation_time;
		case HTOP_DISK_OPERATION_RATE_FIELD:
#if 0
			return uintcmp(d2->read_operation_rate + d2->write_operation_rate,
				d1->read_operation_rate + d1->write_operation_rate);
#else
			return uintcmp(d2->operation_rate, d1->operation_rate);
#endif
		case HTOP_DISK_READ_OPERATION_RATE_FIELD:
			return uintcmp(d2->read_operation_rate, d1->read_operation_rate);
		case HTOP_DISK_WRITE_OPERATION_RATE_FIELD:
			return uintcmp(d2->write_operation_rate, d1->write_operation_rate);
		case HTOP_DISK_READ_BLOCK_RATE_FIELD:
		case HTOP_DISK_READ_BYTE_RATE_FIELD:
			return uintcmp(d2->read_block_rate, d1->read_block_rate);
		case HTOP_DISK_WRITE_BLOCK_RATE_FIELD:
		case HTOP_DISK_WRITE_BYTE_RATE_FIELD:
			return uintcmp(d2->write_block_rate, d1->write_block_rate);
		case HTOP_DISK_PERCENT_BUSY_FIELD:
			return d2->percent_busy - d1->percent_busy;
	}
}
