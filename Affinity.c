/*
htop - Affinity.c
(C) 2004-2011 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Affinity.h"

#include <stdlib.h>

#ifdef HAVE_LIBHWLOC
#include <hwloc.h>
#ifdef __linux__
#define HTOP_HWLOC_CPUBIND_FLAG HWLOC_CPUBIND_THREAD
#else
#define HTOP_HWLOC_CPUBIND_FLAG HWLOC_CPUBIND_PROCESS
#endif
#elif defined HAVE_LINUX_AFFINITY
#include <sched.h>
#elif defined HAVE_KFREEBSD_CPUSET
#include <sys/param.h>
#include <sys/cpuset.h>
#endif

/*{
#include "Process.h"
#include "ProcessList.h"

typedef struct Affinity_ {
   const ProcessList *pl;
   int size;
   int used;
   int* cpus;
} Affinity;

}*/

Affinity* Affinity_new(const ProcessList *pl) {
   Affinity* this = xCalloc(1, sizeof(Affinity));
   this->size = 8;
   this->cpus = xCalloc(this->size, sizeof(int));
   this->pl = pl;
   return this;
}

void Affinity_delete(Affinity* this) {
   free(this->cpus);
   free(this);
}

void Affinity_add(Affinity* this, int id) {
   if (this->used == this->size) {
      this->size *= 2;
      this->cpus = xRealloc(this->cpus, sizeof(int) * this->size);
   }
   this->cpus[this->used] = id;
   this->used++;
}


#ifdef HAVE_LIBHWLOC

Affinity* Affinity_get(const Process *proc, const ProcessList *pl) {
   hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
   bool ok = (hwloc_get_proc_cpubind(pl->topology, proc->pid, cpuset, HTOP_HWLOC_CPUBIND_FLAG) == 0);
   Affinity* affinity = NULL;
   if (ok) {
      affinity = Affinity_new(pl);
      if (hwloc_bitmap_last(cpuset) == -1) {
         for (int i = 0; i < pl->cpuCount; i++) {
            Affinity_add(affinity, i);
         }
      } else {
         unsigned int id;
         hwloc_bitmap_foreach_begin(id, cpuset);
         Affinity_add(affinity, id);
         hwloc_bitmap_foreach_end();
      }
   }
   hwloc_bitmap_free(cpuset);
   return affinity;
}

bool Affinity_set(const Process *proc, Affinity *affinity) {
   hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
   for (int i = 0; i < affinity->used; i++) {
      hwloc_bitmap_set(cpuset, affinity->cpus[i]);
   }
   bool ok = hwloc_set_proc_cpubind(affinity->pl->topology, proc->pid, cpuset, HTOP_HWLOC_CPUBIND_FLAG) == 0;
   hwloc_bitmap_free(cpuset);
   return ok;
}

#elif defined HAVE_LINUX_AFFINITY

Affinity* Affinity_get(const Process *proc, const ProcessList *pl) {
   cpu_set_t cpuset;
   bool ok = (sched_getaffinity(proc->pid, sizeof(cpu_set_t), &cpuset) == 0);
   if (!ok) return NULL;
   Affinity* affinity = Affinity_new(pl);
   for (int i = 0; i < pl->cpuCount; i++) {
      if (CPU_ISSET(i, &cpuset)) Affinity_add(affinity, i);
   }
   return affinity;
}

bool Affinity_set(const Process *proc, Affinity *affinity) {
   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   for (int i = 0; i < affinity->used; i++) {
      CPU_SET(affinity->cpus[i], &cpuset);
   }
   bool ok = (sched_setaffinity(proc->pid, sizeof(unsigned long), &cpuset) == 0);
   return ok;
}

#elif defined HAVE_KFREEBSD_CPUSET

Affinity *Affinity_get(const Process *proc, const ProcessList *pl) {
	cpuset_t cpuset;
	if(cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID, proc->pid, sizeof cpuset, &cpuset) < 0) {
		return NULL;
	}
	Affinity *affinity = Affinity_new(pl);
	for(int i = 0; i < pl->cpuCount; i++) {
		if(CPU_ISSET(i, &cpuset)) Affinity_add(affinity, i);
	}
	return affinity;
}

bool Affinity_set(const Process *proc, Affinity *affinity) {
	cpuset_t cpuset;
	CPU_ZERO(&cpuset);
	for(int i = 0; i < affinity->used; i++) {
		CPU_SET(affinity->cpus[i], &cpuset);
	}
	return cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID, proc->pid, sizeof cpuset, &cpuset) == 0;
}

#endif
