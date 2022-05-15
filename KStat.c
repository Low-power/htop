/*
htop - KStat.c
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#include <sys/param.h>
#include <sys/types.h>
#include <stdint.h>
#include <assert.h>

#define KSTAT_UNIX_SYSTEM_PAGES 0
#define KSTAT_UNIX_VAR 1
#define KSTAT_ZFS_ARCSTATS 2
#define KSTAT_COUNT 3
#define KSTAT_UNIX_SYSTEM_PAGES_PHYSMEM 0
#define KSTAT_ZFS_ARCSTATS_SIZE 1
#define KSTAT_DATA_COUNT 2

#if defined __FreeBSD__ && !defined __FreeBSD_kernel__
#define __FreeBSD_kernel__
#endif
#if (defined __DragonFly__ || (defined __APPLE__ && defined __MACH__) || defined __NetBSD__ || defined __OpenBSD__) && !defined BSD
#define BSD 199506
#endif

#if defined __sun && defined __SVR4
#define USE_LIBKSTAT
#elif defined __linux__
#define USE_SPL_KSTAT
#elif (defined BSD || defined __FreeBSD_kernel__) && !defined __OpenBSD__
#define USE_SYSCTL_KSTAT
#endif

#ifndef KSTAT_DATA_INT32
#define KSTAT_DATA_INT32 1
#endif
#ifndef KSTAT_DATA_UINT32
#define KSTAT_DATA_UINT32 2
#endif
#ifndef KSTAT_DATA_INT64
#define KSTAT_DATA_INT64 3
#endif
#ifndef KSTAT_DATA_UINT64
#define KSTAT_DATA_UINT64 4
#endif

#ifdef USE_LIBKSTAT
#include <kstat.h>
#include <string.h>

static const struct kstat_name {
	char *module;
	const char *class;
	char *name;
} kstat_name_map[KSTAT_COUNT] = {
	[KSTAT_UNIX_SYSTEM_PAGES] = { "unix", "pages", "system_pages" },
	[KSTAT_UNIX_VAR] = { "unix", "misc", "var" },
	[KSTAT_ZFS_ARCSTATS] = { "zfs", "misc", "arcstats" }
};
static char *kstat_data_map[KSTAT_COUNT] = {
	[KSTAT_UNIX_SYSTEM_PAGES_PHYSMEM] = "physmem",
	[KSTAT_ZFS_ARCSTATS_SIZE] = "size"
};
static kstat_ctl_t *ksc;
static kstat_t *kstat_table[KSTAT_COUNT];

static kstat_t *get_kstat(unsigned int kstat_key) {
	assert(kstat_key < KSTAT_COUNT);
	if(!ksc) ksc = kstat_open();
	kstat_t *ksp = kstat_table[kstat_key];
	if(!ksp) {
		const struct kstat_name *name = kstat_name_map + kstat_key;
		if(!name->module) return NULL;
		ksp = kstat_lookup(ksc, name->module, -1, name->name);
		if(!ksp) return NULL;
		if(strcmp(ksp->ks_class, name->class)) return NULL;
		kstat_table[kstat_key] = ksp;
	}
	if(kstat_read(ksc, ksp, NULL) < 0) return NULL;
	return ksp;
}

void *read_unnamed_kstat(unsigned int kstat_key) {
	kstat_t *ksp = get_kstat(kstat_key);
	return ksp ? ksp->ks_data : NULL;
}
#elif defined USE_SPL_KSTAT
#include "StringUtils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *kstat_path_map[KSTAT_COUNT] = {
	[KSTAT_ZFS_ARCSTATS] = "/spl/kstat/zfs/arcstats"
};
static const char *kstat_data_map[KSTAT_COUNT] = {
	[KSTAT_ZFS_ARCSTATS_SIZE] = "size ",
};
static FILE *kstat_file_map[KSTAT_COUNT];
#elif defined USE_SYSCTL_KSTAT
#include "XAlloc.h"
#include <sys/sysctl.h>
#include <stdlib.h>

static const char *sysctl_name_map[KSTAT_COUNT | (KSTAT_DATA_COUNT << 8)] = {
	[KSTAT_ZFS_ARCSTATS | (KSTAT_ZFS_ARCSTATS_SIZE << 8)] = "kstat.zfs.misc.arcstats.size"
};
static int *sysctl_mib_map[KSTAT_COUNT | (KSTAT_DATA_COUNT << 8)];
#endif

int read_kstat(unsigned int kstat_key, unsigned int data_key, unsigned char data_type, void *value) {
	assert(data_key < KSTAT_DATA_COUNT);
#ifdef USE_LIBKSTAT
	kstat_t *ksp = get_kstat(kstat_key);
	if(!ksp) return -1;
	char *data_name = kstat_data_map[data_key];
	if(!data_name) return -1;
	kstat_named_t *ksn = kstat_data_lookup(ksp, data_name);
	if(!ksn) return -1;
	if(ksn->data_type != data_type) return -1;
	switch(data_type) {
		case KSTAT_DATA_INT32:
			*(int32_t *)value = ksn->value.i32;
			return 0;
		case KSTAT_DATA_UINT32:
			*(uint32_t *)value = ksn->value.ui32;
			return 0;
		case KSTAT_DATA_INT64:
			*(int64_t *)value = ksn->value.i64;
			return 0;
		case KSTAT_DATA_UINT64:
			*(uint64_t *)value = ksn->value.ui64;
			return 0;
		default:
			return -1;
	}
#elif defined USE_SPL_KSTAT
	FILE *f = kstat_file_map[kstat_key];
	if(!f) {
		const char *path = kstat_path_map[kstat_key];
		if(!path) return -1;
		size_t len = strlen(path);
		char full_path[sizeof PROCDIR + len];
		memcpy(full_path, PROCDIR, sizeof PROCDIR - 1);
		memcpy(full_path + sizeof PROCDIR - 1, path, len);
		full_path[sizeof PROCDIR - 1 + len] = 0;
		f = fopen(full_path, "r");
		if(!f) return -1;
		kstat_file_map[kstat_key] = f;
	}
	const char *data_name = kstat_data_map[data_key];
	if(!data_name) return -1;
	fseek(f, SEEK_SET, 0);
	char buffer[128];
	unsigned int i = 0;
	while(fgets(buffer, sizeof buffer, f)) {
		if(++i < 3) continue;
		if(!String_startsWith(buffer, data_name)) continue;
		const char *p = buffer + strlen(data_name);
		char *endp;
		long int t = strtol(p, &endp, 10);
		if(*endp != ' ') return -1;
		if(t != data_type) return -1;
		p = endp;
		switch(data_type) {
			case KSTAT_DATA_INT32:
				*(int32_t *)value = strtol(p, &endp, 10);
				break;
			case KSTAT_DATA_UINT32:
				*(uint32_t *)value = strtoul(p, &endp, 10);
				break;
			case KSTAT_DATA_INT64:
				*(int64_t *)value = strtoll(p, &endp, 10);
				break;
			case KSTAT_DATA_UINT64:
				*(uint64_t *)value = strtoull(p, &endp, 10);
				break;
			default:
				return -1;
		}
		return !*endp || *endp == '\n' ? 0 : -1;
	}
	return -1;
#elif defined USE_SYSCTL_KSTAT
	unsigned int key = kstat_key | (data_key << 8);
	int *mib = sysctl_mib_map[key];
	if(!mib) {
		const char *name = sysctl_name_map[key];
		if(!name) return -1;
		size_t len = 5;
		mib = xMalloc(len * sizeof(int));
		if(sysctlnametomib(name, mib, &len) < 0 || len != 5) {
			free(mib);
			return -1;
		}
		sysctl_mib_map[key] = mib;
	}
	size_t value_size;
	switch(data_type) {
		case KSTAT_DATA_INT32:
		case KSTAT_DATA_UINT32:
			value_size = 4;
			break;
		case KSTAT_DATA_INT64:
		case KSTAT_DATA_UINT64:
			value_size = 8;
			break;
		default:
			return -1;
	}
	size_t actual_value_size = value_size;
	int r = sysctl(mib, 5, value, &value_size, NULL, 0);
	if(r < 0) return -1;
	return actual_value_size == value_size ? r : -1;
#else
	return -1;
#endif
}
