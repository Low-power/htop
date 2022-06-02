/*
htop - solaris/Battery.c
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#include "BatteryMeter.h"

#ifdef HAVE_SYS_ACPI_DRV_H
#include <sys/acpi_drv.h>
#include <kstat.h>
#include <string.h>

void Battery_getData(double *level, ACPresence *is_on_ac) {
	*level = -1;
	*is_on_ac = AC_ERROR;

	kstat_ctl_t *ksc = kstat_open();
	if(!ksc) return;

	kstat_t *acpi_drv_stats = kstat_lookup(ksc, "acpi_drv", -1, NULL);
	if(!acpi_drv_stats) {
		kstat_close(ksc);
		return;
	}

	double max_cap = 0;
	double cap = 0;
	do {
		if(acpi_drv_stats->ks_type != KSTAT_TYPE_NAMED) continue;
		if(kstat_read(ksc, acpi_drv_stats, NULL) == -1) continue;
		if(strcmp(acpi_drv_stats->ks_name, ACPI_DRV_POWER_KSTAT_NAME) == 0) {
			kstat_named_t *ps_status = kstat_data_lookup(acpi_drv_stats, SYSTEM_POWER);
			if(ps_status && ps_status->data_type == KSTAT_DATA_STRING) {
				if(strcmp(ps_status->value.str.addr.ptr, AC) == 0) *is_on_ac = AC_PRESENT;
				else if(strcmp(ps_status->value.str.addr.ptr, BATTERY) == 0) *is_on_ac = AC_ABSENT;
			}
		} else if(strncmp(acpi_drv_stats->ks_name, ACPI_DRV_BIF_KSTAT_NAME, sizeof ACPI_DRV_BIF_KSTAT_NAME - 1) == 0) {
			kstat_named_t *bif_last_cap = kstat_data_lookup(acpi_drv_stats, BIF_LAST_CAP);
			if(bif_last_cap) max_cap += bif_last_cap->value.ui32;
		} else if(strncmp(acpi_drv_stats->ks_name, ACPI_DRV_BST_KSTAT_NAME, sizeof ACPI_DRV_BST_KSTAT_NAME - 1) == 0) {
			kstat_named_t *bst_rem_cap = kstat_data_lookup(acpi_drv_stats, BST_REM_CAP);
			if(bst_rem_cap) cap += bst_rem_cap->value.ui32;
		}
	} while((acpi_drv_stats = acpi_drv_stats->ks_next) && strcmp(acpi_drv_stats->ks_module, "acpi_drv") == 0);

	kstat_close(ksc);

	if(max_cap > 0) {
		*level = cap / max_cap * 100;
		if(*level > 100) *level = 100;
	}
}
#else
void Battery_getData(double *level, ACPresence *is_on_ac) {
	*level = -1;
	*is_on_ac = AC_ERROR;
}
#endif
