/*
htop - DiskList.c
(C) 2004,2005 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "Vector.h"
#include "Settings.h"
#include "Object.h"
#include "Panel.h"
#include "Disk.h"
#include "RichString.h"
#include <stdbool.h>

typedef struct {
	Vector *disks;
	const Settings *settings;
	Panel *panel;
} DiskList;

// Abstract functions
DiskList *DiskList_new(const Settings *);
void DiskList_delete(DiskList *);
void DiskList_internalScan(DiskList *, double);
}*/

#include "DiskList.h"
#include "CRT.h"
#include <string.h>

DiskList *DiskList_init(DiskList *this, ObjectClass *class, const Settings *settings) {
	this->disks = Vector_new(class, true, 4);
	this->settings = settings;
	return this;
}

void DiskList_done(DiskList *this) {
	Vector_delete(this->disks);
}

void DiskList_setPanel(DiskList *this, Panel *panel) {
	this->panel = panel;
}

void DiskList_printHeader(DiskList *this, RichString *header) {
	RichString_prune(header);
	const unsigned int *field = this->settings->disk_fields;
	while(*field) {
		const char *title = Disk_fields[*field].title;
		if(!title) title = "- ";
		RichString_append(header,
			CRT_colors[this->settings->disk_sort_key == *field ?
				HTOP_PANEL_SELECTION_FOCUS_COLOR : HTOP_PANEL_HEADER_FOCUS_COLOR],
			title);
		field++;
	}
}

void DiskList_add(DiskList *this, Disk *disk) {
	assert(Vector_indexOf(this->disks, disk, Disk_nameCompare) == -1);
	Vector_add(this->disks, disk);
}

void DiskList_remove(DiskList *this, Disk *disk) {
	int i = Vector_indexOf(this->disks, disk, Disk_nameCompare);
	assert(i != -1);
	if(i >= 0) Vector_remove(this->disks, i);
}

Disk *DiskList_get(DiskList *this, int i) {
	return (Disk *)Vector_get(this->disks, i);
}

int DiskList_size(DiskList *this) {
	return Vector_size(this->disks);
}

void DiskList_sort(DiskList *this) {
	Vector_insertionSort(this->disks);
}

DiskField DiskList_keyAt(DiskList *this, int at) {
	int x = 0;
	const unsigned int *field = this->settings->disk_fields;
	while(*field) {
		const char *title = Disk_fields[*field].title;
		int len = title ? strlen(title) : 2;
		if(at >= x && at <= x + len) return *field;
		x += len;
		field++;
	}
	return HTOP_DISK_NAME_FIELD;
}

void DiskList_rebuildPanel(DiskList *this) {
	int currPos = Panel_getSelectedIndex(this->panel);
	int currScrollV = this->panel->scrollV;

	Panel_prune(this->panel);
	int size = DiskList_size(this);
	for (int i = 0; i < size; i++) {
		Disk *disk = DiskList_get(this, i);
		Panel_set(this->panel, i, (Object*)disk);
		if(i == currPos) {
			Panel_setSelected(this->panel, i);
			this->panel->scrollV = currScrollV;
		}
	}
}

Disk *DiskList_getOrCreate(DiskList *this, const char *name, bool *is_existing, DiskConstructor constructor) {
	int i = DiskList_size(this);
	while(i > 0) {
		Disk *disk = DiskList_get(this, --i);
		if(strcmp(disk->name, name) == 0) {
			*is_existing = true;
			return disk;
		}
	}
	*is_existing = false;
	Disk *disk = constructor(this->settings);
	disk->name = xStrdup(name);
	return disk;
}

void DiskList_scan(DiskList *this, double interval) {
	int i = DiskList_size(this);
	while(i > 0) {
		Disk *disk = DiskList_get(this, --i);
		disk->updated = false;
	}
	DiskList_internalScan(this, interval);
	i = DiskList_size(this);
	while(i > 0) {
		Disk *disk = DiskList_get(this, --i);
		if(!disk->updated) DiskList_remove(this, disk);
	}
}
