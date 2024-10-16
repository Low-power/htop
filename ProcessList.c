/*
htop - ProcessList.c
(C) 2004,2005 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"
#include "Platform.h"

#include "CRT.h"
#include "StringUtils.h"
#include "KStat.h"
#include <stdlib.h>
#include <string.h>

/*{
#include "Vector.h"
#include "Hashtable.h"
#include "UsersTable.h"
#include "Panel.h"
#include "Process.h"
#include "Settings.h"
#include <stdint.h>

#ifdef HAVE_LIBHWLOC
#include <hwloc.h>
#endif

#ifndef MAX_NAME
#define MAX_NAME 128
#endif

#ifndef MAX_READ
#define MAX_READ 2048
#endif

typedef struct ProcessList_ {
   Settings* settings;

   Vector* processes;
   Vector* processes2;
   Hashtable* processTable;
   UsersTable* usersTable;
   const Hashtable *pidWhiteList;

   Panel* panel;
   int following;
   uid_t userId;
   const char* incFilter;

   #ifdef HAVE_LIBHWLOC
   hwloc_topology_t topology;
   bool topologyOk;
   #endif

   bool should_update_names;

   int totalTasks;
   int thread_count;
   int kernel_process_count;
   int kernel_thread_count;
   int running_process_count;
   int running_thread_count;

   unsigned long long int totalMem;
   unsigned long long int usedMem;
   unsigned long long int freeMem;
   unsigned long long int buffersMem;
   unsigned long long int cachedMem;
   uint64_t zfs_arc_size;
   unsigned long long int totalSwap;
   unsigned long long int usedSwap;
   unsigned long long int freeSwap;

   int cpuCount;

} ProcessList;

ProcessList* ProcessList_new(UsersTable* ut, const Hashtable *pidWhiteList, uid_t userId);
void ProcessList_delete(ProcessList* pl);
void ProcessList_goThroughEntries(ProcessList *, bool);

#define ProcessList_shouldUpdateProcessNames(THIS) ((THIS)->should_update_names || (THIS)->settings->updateProcessNames)
}*/

ProcessList* ProcessList_init(ProcessList* this, ObjectClass* klass, UsersTable* usersTable, const Hashtable *pidWhiteList, uid_t userId) {
   this->processes = Vector_new(klass, true, DEFAULT_SIZE);
   this->processTable = Hashtable_new(140, false);
   this->usersTable = usersTable;
   this->pidWhiteList = pidWhiteList;
   this->userId = userId;
   // tree-view auxiliary buffer
   this->processes2 = Vector_new(klass, true, DEFAULT_SIZE);
   // set later by platform-specific code
   this->cpuCount = 0;

#ifdef HAVE_LIBHWLOC
   this->topologyOk = false;
   int topoErr = hwloc_topology_init(&this->topology);
   if (topoErr == 0) {
      topoErr = hwloc_topology_load(this->topology);
   }
   if (topoErr == 0) {
      this->topologyOk = true;
   }
#endif

   this->following = -1;

   return this;
}

void ProcessList_done(ProcessList* this) {
#ifdef HAVE_LIBHWLOC
   if (this->topologyOk) {
      hwloc_topology_destroy(this->topology);
   }
#endif
   Hashtable_delete(this->processTable);
   Vector_delete(this->processes);
   Vector_delete(this->processes2);
}

void ProcessList_setPanel(ProcessList* this, Panel* panel) {
   this->panel = panel;
}

void ProcessList_printHeader(ProcessList* this, RichString* header) {
   RichString_prune(header);
   const unsigned int *fields = this->settings->fields;
   for (int i = 0; fields[i]; i++) {
      const char *title = Process_fields[fields[i]].title;
      if (!title) title = "- ";
      int attr = CRT_colors[!this->settings->treeView && this->settings->sortKey == fields[i] ?
         HTOP_PANEL_SELECTION_FOCUS_COLOR : HTOP_PANEL_HEADER_FOCUS_COLOR];
      RichString_append(header, attr, title);
   }
}

void ProcessList_add(ProcessList* this, Process* p) {
   assert(Vector_indexOf(this->processes, p, Process_pidCompare) == -1);
   assert(Hashtable_get(this->processTable, p->pid) == NULL);
   Vector_add(this->processes, p);
   Hashtable_put(this->processTable, p->pid, p);
   assert(Vector_indexOf(this->processes, p, Process_pidCompare) != -1);
   assert(Hashtable_get(this->processTable, p->pid) != NULL);
   assert(Hashtable_count(this->processTable) == Vector_count(this->processes));
}

void ProcessList_remove(ProcessList* this, Process* p) {
   assert(Vector_indexOf(this->processes, p, Process_pidCompare) != -1);
   assert(Hashtable_get(this->processTable, p->pid) != NULL);
   Process* pp = Hashtable_remove(this->processTable, p->pid);
   assert(pp == p); (void)pp;
   unsigned int pid = p->pid;
   int idx = Vector_indexOf(this->processes, p, Process_pidCompare);
   assert(idx != -1);
   if (idx >= 0) Vector_remove(this->processes, idx);
   assert(Hashtable_get(this->processTable, pid) == NULL); (void)pid;
   assert(Hashtable_count(this->processTable) == Vector_count(this->processes));
}

Process* ProcessList_get(ProcessList* this, int idx) {
   return (Process*) (Vector_get(this->processes, idx));
}

int ProcessList_size(ProcessList* this) {
   return (Vector_size(this->processes));
}

static void ProcessList_buildTree(ProcessList* this, pid_t pid, int level, int indent, int direction, bool show) {
   Vector* children = Vector_new(Class(Process), false, DEFAULT_SIZE);

   for (int i = Vector_size(this->processes) - 1; i >= 0; i--) {
      Process* process = (Process*) (Vector_get(this->processes, i));
      if (process->show && Process_isChildOf(process, pid)) {
         process = (Process*) (Vector_take(this->processes, i));
         Vector_add(children, process);
      }
   }
   int size = Vector_size(children);
   for (int i = 0; i < size; i++) {
      Process* process = (Process *)Vector_get(children, i);
      process->seen_in_tree_loop = false;
      if (!show) process->show = false;
      int s = this->processes2->items;
      if (direction == 1) Vector_add(this->processes2, process);
      else Vector_insert(this->processes2, 0, process);
      assert(this->processes2->items == s+1); (void)s;
      int nextIndent = level < 0 ? 0 : (indent | (1 << level));
      ProcessList_buildTree(this, process->pid, level+1, (i < size - 1) ? nextIndent : indent, direction, show && process->showChildren);
      process->indent = (i < size - 1) ? nextIndent : -nextIndent;
   }
   Vector_delete(children);
}

static Process *ProcessList_findParent(const ProcessList *this, const Process *proc, int *index) {
   // Bisect the process vector to find parent
   // If PID corresponds with PPID (e.g. "kernel_task" (PID=0, PPID=0)
   // on Mac OS X) cancel bisecting and regard this process as root.
   pid_t ppid = Process_getParentPid(proc);
   if(ppid == proc->pid) return NULL;
   int l = 0, r = Vector_size(this->processes);
   while (l < r) {
      int c = (l + r) / 2;
      Process *p = (Process *)Vector_get(this->processes, c);
      if (ppid == p->pid) {
         if(index) *index = c;
         return p;
      }
      if (ppid < p->pid) r = c;
      else l = c + 1;
   }
   return NULL;
}

void ProcessList_sort(ProcessList* this) {
   if (!this->settings->treeView) {
      Vector_insertionSort(this->processes);
   } else {
      // Save settings
      int direction = this->settings->direction;
      int sortKey = this->settings->sortKey;
      // Sort by PID
      this->settings->sortKey = HTOP_PID_FIELD;
      this->settings->direction = 1;
      Vector_quickSort(this->processes);
      // Restore settings
      this->settings->sortKey = sortKey;
      this->settings->direction = direction;
      int vsize = Vector_size(this->processes);
      // Find all processes whose parent is not visible
      int size;
      while ((size = Vector_size(this->processes))) {
         int i = 0;
         while(i < size) {
            Process* process = (Process *)Vector_get(this->processes, i);
            process->seen_in_tree_loop = false;
            bool root = !ProcessList_findParent(this, process, NULL);
            // If parent not found, then construct the tree with this root
            if (!process->show || root) {
               process = (Process *)Vector_take(this->processes, i);
               process->indent = 0;
               Vector_add(this->processes2, process);
               int level = (!process->show && root) ? -1 : 0;
               ProcessList_buildTree(this, process->pid, level, 0, direction, root && process->showChildren);
               if(root) break;
               size--;
            } else {
               i++;
            }
         }
         /* Under some ptrace(2) implementations, a process
          * ptrace(2)-attaching a parent process in its
          * process tree will causing the traced process to be
          * re-parented to the tracing process; this will
          * creating a loop in the process tree. In this case
          * build separated tree(s) for loop(s) left here.
          */
         if(i >= size) {
            do {
               Process *proc = (Process *)Vector_get(this->processes, size - 1);
               /* This remaining process could
                * either be a node at the loop it
                * self, or a descendant node
                * indirectly attached to the loop.
                */
#ifdef NDEBUG
               i = size - 1;
#endif
               do {
                  /* Make sure we break at the
                   * loop itself, not a
                   * descendant of it. */
                  proc->seen_in_tree_loop = true;
                  Process *parent = ProcessList_findParent(this, proc, &i);
                  assert(parent != NULL);
                  if(!parent) break;
                  proc = parent;
               } while(!proc->seen_in_tree_loop);
#ifdef NDEBUG
               Vector_take(this->processes, i);
#else
               Process *taken = (Process *)Vector_take(this->processes, i);
               assert(taken == proc);
#endif
               proc->indent = 0;
               Vector_add(this->processes2, proc);
               ProcessList_buildTree(this, proc->pid, 0, 0, direction, proc->show);
            } while((size = Vector_size(this->processes)) > 0);
            break;
         }
      }
      assert(Vector_size(this->processes2) == vsize); (void)vsize;
      // Swap listings around
      Vector* t = this->processes;
      this->processes = this->processes2;
      this->processes2 = t;
   }
}


ProcessField ProcessList_keyAt(ProcessList* this, int at) {
   int x = 0;
   const unsigned int *fields = this->settings->fields;
   ProcessField field;
   for (int i = 0; (field = fields[i]); i++) {
      const char* title = Process_fields[field].title;
      if (!title) title = "- ";
      int len = strlen(title);
      if (at >= x && at <= x + len) {
         return field;
      }
      x += len;
   }
   return HTOP_COMM_FIELD;
}

void ProcessList_expandTree(ProcessList* this) {
   int size = Vector_size(this->processes);
   for (int i = 0; i < size; i++) {
      Process* process = (Process*) Vector_get(this->processes, i);
      process->showChildren = true;
   }
}

void ProcessList_rebuildPanel(ProcessList* this) {
   const char* incFilter = this->incFilter;

   int currPos = Panel_getSelectedIndex(this->panel);
   pid_t currPid = this->following != -1 ? this->following : 0;
   int currScrollV = this->panel->scrollV;

   Panel_prune(this->panel);
   int size = ProcessList_size(this);
   int idx = 0;
   for (int i = 0; i < size; i++) {
      bool hidden = false;
      Process* p = ProcessList_get(this, i);

      if ( (!p->show)
         || (this->userId != (uid_t) -1 && p->ruid != this->userId && p->euid != this->userId)
         || (incFilter && !(String_contains_i(p->comm, incFilter)))
         || (this->pidWhiteList && !Hashtable_get(this->pidWhiteList, p->tgid)) ) {
         hidden = true;
      }

      if (!hidden) {
         Panel_set(this->panel, idx, (Object*)p);
         if ((this->following == -1 && idx == currPos) || (this->following != -1 && p->pid == currPid)) {
            Panel_setSelected(this->panel, idx);
            this->panel->scrollV = currScrollV;
         }
         idx++;
      }
   }
}

Process* ProcessList_getProcess(ProcessList* this, pid_t pid, bool* preExisting, Process_New constructor) {
   Process* proc = Hashtable_get(this->processTable, pid);
   if (proc) {
      *preExisting = true;
      assert(Vector_indexOf(this->processes, proc, Process_pidCompare) != -1);
      assert(proc->pid == pid);
   } else {
      *preExisting = false;
      proc = constructor(this->settings);
      assert(proc->comm == NULL);
      proc->pid = pid;
   }
   return proc;
}

static void read_zfs_arc_size(ProcessList *this) {
	if(read_kstat(KSTAT_ZFS_ARCSTATS, KSTAT_ZFS_ARCSTATS_SIZE, KSTAT_DATA_UINT64, &this->zfs_arc_size) < 0) {
		this->zfs_arc_size = 0;
	} else {
		this->zfs_arc_size /= 1024;
		if(this->usedMem < this->zfs_arc_size) this->zfs_arc_size = 0;
		else this->usedMem -= this->zfs_arc_size;
	}
}

void ProcessList_scan(ProcessList* this, bool skip_processes) {
   //if(!skip_processes) {
      // mark all process as "dirty"
      int i = ProcessList_size(this);
      while(i > 0) {
         Process *p = ProcessList_get(this, --i);
         p->created = false;
         p->updated = false;
         p->show = true;
      }
   //}

   this->totalTasks = 0;
   this->thread_count = 0;
   this->kernel_process_count = 0;
   this->kernel_thread_count = 0;
   this->running_process_count = 0;
   this->running_thread_count = 0;

   ProcessList_goThroughEntries(this, skip_processes);
   read_zfs_arc_size(this);

   for (i = Vector_size(this->processes) - 1; i >= 0; i--) {
      Process* p = (Process*) Vector_get(this->processes, i);
      if(!p->updated) ProcessList_remove(this, p);
   }

   if(!skip_processes) this->should_update_names = false;
}
