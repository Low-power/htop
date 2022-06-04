/*
htop - haiku/Battery.c
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#include "BatteryMeter.h"
#include <Drivers.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>

#define PM_GET_BATTERY_INFO (B_DEVICE_OP_CODES_END + 20010)
#define PM_GET_EXTENDED_BATTERY_INFO (B_DEVICE_OP_CODES_END + 20011)

#define ACPI_BATTERY_PATH "/dev/power/acpi_battery/"

#define ACPI_BATTERY_DISCHARGING 0x01
#define ACPI_BATTERY_CHARGING 0x02
#define ACPI_BATTERY_CRITICAL_STATE 0x04

struct acpi_battery_info {
	int state;
	int current_rate;
	int capacity;
	int voltage;
};

struct acpi_extended_battery_info {
	int power_unit;
	int design_capacity;
	int last_full_charge;
	int technology;
	int design_voltage;
	int design_capacity_warning;
	int design_capacity_low;
	int capacity_granularity_1;
	int capacity_granularity_2;
	char model_number[32];
	char serial_number[32];
	char type[32];
	char oem_info[32];
};

static DIR *acpi_battery_dir;

void Battery_getData(double *level, ACPresence *is_on_ac) {
	*level = -1;
	*is_on_ac = AC_ERROR;

	if(!acpi_battery_dir) {
		acpi_battery_dir = opendir(ACPI_BATTERY_PATH);
		if(!acpi_battery_dir) return;
	}

	double max_cap = 0;
	double cap = 0;

	rewinddir(acpi_battery_dir);
	struct dirent *e;
	while((e = readdir(acpi_battery_dir))) {
		if(e->d_name[0] == '.') continue;
		size_t name_len = strlen(e->d_name) + 1;
		char path[sizeof ACPI_BATTERY_PATH - 1 + name_len];
		memcpy(path, ACPI_BATTERY_PATH, sizeof ACPI_BATTERY_PATH - 1);
		memcpy(path + sizeof ACPI_BATTERY_PATH - 1, e->d_name, name_len);
		int fd = open(path, O_RDONLY);
		if(fd == -1) continue;
		struct acpi_battery_info info;
		if(ioctl(fd, PM_GET_BATTERY_INFO, &info, sizeof info) == 0 &&
		  !(info.state & ACPI_BATTERY_CRITICAL_STATE)) {
			if(*is_on_ac != AC_PRESENT) {
				*is_on_ac = (info.state & ACPI_BATTERY_DISCHARGING) ?
					AC_ABSENT : AC_PRESENT;
			}
			struct acpi_extended_battery_info ext_info;
			if(ioctl(fd, PM_GET_EXTENDED_BATTERY_INFO, &ext_info, sizeof ext_info) == 0) {
				max_cap += ext_info.last_full_charge;
				cap += info.capacity;
			}
		}
		close(fd);
	}

	if(max_cap > 0) {
		*level = cap / max_cap * 100;
		if(*level > 100) *level = 100;
	}
}
