/*
htop - darwin/DarwinProcess.c
(C) 2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h"
#include "Process.h"
#include "DarwinProcess.h"
#include "bsd/BSDProcess.h"
#include "CRT.h"
#include <sys/sysctl.h>
#ifdef HAVE_LIBPROC
#include <libproc.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <mach/mach.h>

/*{
#include "DarwinProcessList.h"
#include "Settings.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct DarwinProcess_ {
   Process super;
   bool is_kernel_process;
   bool libproc_accessible;
   bool mach_task_accessible;
   uint64_t utime;
   uint64_t stime;
} DarwinProcess;
}*/

ProcessClass DarwinProcess_class = {
   .super = {
      .extends = Class(Process),
      .display = Process_display,
      .delete = Process_delete,
      .compare = Process_compare
   },
   .writeField = BSDProcess_writeField,
};

DarwinProcess* DarwinProcess_new(Settings* settings) {
   DarwinProcess* this = xCalloc(1, sizeof(DarwinProcess));
   Object_setClass(this, Class(DarwinProcess));
   Process_init(&this->super, settings);

   this->utime = 0;
   this->stime = 0;
   this->mach_task_accessible = true;
   this->libproc_accessible = false;

   return this;
}

void Process_delete(Object* cast) {
   DarwinProcess* this = (DarwinProcess*) cast;
   Process_done(&this->super);
   // free platform-specific fields here
   free(this);
}

bool Process_isKernelProcess(const Process* this) {
	return ((const DarwinProcess *)this)->is_kernel_process;
}

bool Process_isExtraThreadProcess(const Process* this) {
   (void) this;
   return false;
}

void DarwinProcess_setStartTime(Process *proc, const struct extern_proc *ep, time_t now) {
   proc->starttime_ctime = ep->p_starttime.tv_sec;
}

char *DarwinProcess_getCmdLine(const struct kinfo_proc *k, int* argv0_length) {
   /* This function is from the old Mac version of htop. Originally from ps? */
   int mib[3], argmax, nargs, c = 0;
   size_t size;
   char *procargs, *sp, *np, *cp, *retval;

   /* Get the maximum process arguments size. */
   mib[0] = CTL_KERN;
   mib[1] = KERN_ARGMAX;

   size = sizeof( argmax );
   if ( sysctl( mib, 2, &argmax, &size, NULL, 0 ) == -1 ) {
      goto ERROR_A;
   }

   /* Allocate space for the arguments. */
   procargs = ( char * ) xMalloc( argmax );
   if ( procargs == NULL ) {
      goto ERROR_A;
   }

   /*
    * Make a sysctl() call to get the raw argument space of the process.
    * The layout is documented in start.s, which is part of the Csu
    * project.  In summary, it looks like:
    *
    * /---------------\ 0x00000000
    * :               :
    * :               :
    * |---------------|
    * | argc          |
    * |---------------|
    * | arg[0]        |
    * |---------------|
    * :               :
    * :               :
    * |---------------|
    * | arg[argc - 1] |
    * |---------------|
    * | 0             |
    * |---------------|
    * | env[0]        |
    * |---------------|
    * :               :
    * :               :
    * |---------------|
    * | env[n]        |
    * |---------------|
    * | 0             |
    * |---------------| <-- Beginning of data returned by sysctl() is here.
    * | argc          |
    * |---------------|
    * | exec_path     |
    * |:::::::::::::::|
    * |               |
    * | String area.  |
    * |               |
    * |---------------| <-- Top of stack.
    * :               :
    * :               :
    * \---------------/ 0xffffffff
    */
   mib[0] = CTL_KERN;
   mib[1] = KERN_PROCARGS2;
   mib[2] = k->kp_proc.p_pid;

   size = ( size_t ) argmax;
   if ( sysctl( mib, 3, procargs, &size, NULL, 0 ) == -1 ) {
      goto ERROR_B;
   }

   memcpy( &nargs, procargs, sizeof( nargs ) );
   cp = procargs + sizeof( nargs );

   /* Skip the saved exec_path. */
   for ( ; cp < &procargs[size]; cp++ ) {
      if ( *cp == '\0' ) {
         /* End of exec_path reached. */
         break;
      }
   }
   if ( cp == &procargs[size] ) {
      goto ERROR_B;
   }

   /* Skip trailing '\0' characters. */
   for ( ; cp < &procargs[size]; cp++ ) {
      if ( *cp != '\0' ) {
         /* Beginning of first argument reached. */
         break;
      }
   }
   if ( cp == &procargs[size] ) {
      goto ERROR_B;
   }
   /* Save where the argv[0] string starts. */
   sp = cp;

   *argv0_length = 0;
   for ( np = NULL; c < nargs && cp < &procargs[size]; cp++ ) {
      if ( *cp == '\0' ) {
         c++;
         if ( np != NULL ) {
            /* Convert previous '\0'. */
            *np = ' ';
         }
        /* Note location of current '\0'. */
        np = cp;
        if (*argv0_length == 0) {
           *argv0_length = cp - sp;
        }
     }
   }

   /*
    * sp points to the beginning of the arguments/environment string, and
    * np should point to the '\0' terminator for the string.
    */
   if ( np == NULL || np == sp ) {
      /* Empty or unterminated string. */
      goto ERROR_B;
   }
   if (*argv0_length == 0) {
      *argv0_length = np - sp;
   }

   /* Make a copy of the string. */
   retval = xStrdup(sp);

   /* Clean up. */
   free( procargs );

   return retval;

ERROR_B:
   free( procargs );
ERROR_A:
   retval = xStrdup(k->kp_proc.p_comm);
   *argv0_length = strlen(retval);
   return retval;
}

void DarwinProcess_setFromKInfoProc(Process *proc, const struct kinfo_proc *ps, const ProcessList *pl, time_t now, bool exists) {
   const struct extern_proc *ep = &ps->kp_proc;

   /* UNSET HERE :
    *
    * processor
    * user (set at ProcessList level)
    * nlwp
    * percent_cpu
    * percent_mem
    * m_size
    * m_resident
    * minflt
    * majflt
    */

   if(exists) {
      if(proc->ruid != ps->kp_eproc.e_pcred.p_ruid) proc->real_user = NULL;
      if(proc->euid != ps->kp_eproc.e_ucred.cr_uid) proc->effective_user = NULL;
      if(ProcessList_shouldUpdateProcessNames(pl)) {
         free(proc->name);
         free(proc->comm);
         proc->name = xStrdup(ps->kp_proc.p_comm);
         proc->comm = DarwinProcess_getCmdLine(ps, &proc->argv0_length);
      }
   } else {
      /* First, the "immutable" parts */
      proc->tgid = proc->pid;
      proc->session = 0; /* TODO Get the session id */
      DarwinProcess_setStartTime(proc, ep, now);
      proc->name = xStrdup(ps->kp_proc.p_comm);
      proc->comm = DarwinProcess_getCmdLine(ps, &(proc->argv0_length));
      ((DarwinProcess *)proc)->is_kernel_process = ep->p_flag & P_SYSTEM;
   }

   /* Mutable information */
   proc->ppid = ps->kp_eproc.e_ppid;
   proc->pgrp = ps->kp_eproc.e_pgid;
   proc->tpgid = ps->kp_eproc.e_tpgid;
   /* e_tdev = (major << 24) | (minor & 0xffffff) */
   /* e_tdev == -1 for "no device" */
   proc->tty_nr = ps->kp_eproc.e_tdev;
   proc->ruid = ps->kp_eproc.e_pcred.p_ruid;
   proc->euid = ps->kp_eproc.e_ucred.cr_uid;
   proc->nice = ep->p_nice;
   proc->priority = ep->p_priority;

   proc->state = (ep->p_stat == SZOMB) ? 'Z' : '?';

   /* Make sure the updated flag is set */
   proc->updated = true;
}

static void set_time(DarwinProcess *proc, DarwinProcessList *dpl, uint64_t utime, uint64_t stime) {
   if(proc->utime || proc->stime) {
      uint64_t diff = (utime - proc->utime) + (stime - proc->stime);
      proc->super.percent_cpu = (double)diff * (double)dpl->super.cpuCount /
         ((double)dpl->global_diff * 100000.0);
   }
   proc->super.time = utime / 10000000 + stime / 10000000;
   proc->utime = utime;
   proc->stime = stime;
}

void DarwinProcess_setFromLibprocPidinfo(DarwinProcess *proc, DarwinProcessList *dpl) {
#ifdef HAVE_LIBPROC
   struct proc_taskinfo pti;
   if(proc_pidinfo(proc->super.pid, PROC_PIDTASKINFO, 0, &pti, sizeof pti) != sizeof pti) return;

   set_time(proc, dpl, pti.pti_total_user, pti.pti_total_system);
   proc->super.nlwp = pti.pti_threadnum;
   proc->super.m_size = pti.pti_virtual_size / CRT_page_size;
   proc->super.m_resident = pti.pti_resident_size / CRT_page_size;
   proc->super.majflt = pti.pti_faults;
   proc->super.percent_mem = (double)pti.pti_resident_size / (double)dpl->host_info.max_mem * 100;

   dpl->super.thread_count += pti.pti_threadnum;
   if(Process_isKernelProcess(&proc->super)) {
      dpl->super.kernel_thread_count += pti.pti_threadnum;
   }
   dpl->super.running_thread_count += pti.pti_numrunning;

   proc->libproc_accessible = true;
#endif
}

/*
 * Scan threads for process state information.
 * Based on: http://stackoverflow.com/questions/6788274/ios-mac-cpu-usage-for-thread
 * and       https://github.com/max-horvath/htop-osx/blob/e86692e869e30b0bc7264b3675d2a4014866ef46/ProcessList.c
 */
void DarwinProcess_setFromMachTaskInfo(DarwinProcess *dp, DarwinProcessList *dpl) {
   Process* proc = (Process*) dp;
   kern_return_t ret;
   if (!dp->mach_task_accessible) {
      return;
   }
   if (proc->state == 'Z') {
      return;
   }

   task_t port;
   ret = task_for_pid(mach_task_self(), proc->pid, &port);
   if (ret != KERN_SUCCESS) {
      dp->mach_task_accessible = false;
      return;
   }

   uint64_t utime, stime;

   if(!dp->libproc_accessible) {
      // Fill some process information if they didn't get filled from libproc
      struct task_basic_info t_info;
      mach_msg_type_number_t task_info_count = TASK_BASIC_INFO_COUNT;
      ret = task_info(port, TASK_BASIC_INFO, (task_info_t)&t_info, &task_info_count);
      if (ret != KERN_SUCCESS) {
         dp->mach_task_accessible = false;
         return;
      }

      utime = (uint64_t)t_info.user_time.seconds * 1000000000 + (uint64_t)t_info.user_time.microseconds * 1000;
      stime = (uint64_t)t_info.system_time.seconds * 1000000000 + (uint64_t)t_info.system_time.microseconds * 1000;

      proc->m_size = t_info.virtual_size / CRT_page_size;
      proc->m_resident = t_info.resident_size / CRT_page_size;
   }

   thread_array_t thread_list;
   mach_msg_type_number_t thread_count;
   ret = task_threads(port, &thread_list, &thread_count);
   if (ret != KERN_SUCCESS) {
      dp->mach_task_accessible = false;
      mach_port_deallocate(mach_task_self(), port);
      return;
   }
   integer_t run_state = 999, sleep_time = 999;
   for (unsigned int i = 0; i < thread_count; i++) {
      struct thread_basic_info thr_info;
      mach_msg_type_number_t thread_info_count = THREAD_BASIC_INFO_COUNT;
      ret = thread_info(thread_list[i], THREAD_BASIC_INFO, (thread_info_t)&thr_info, &thread_info_count);
      if (ret == KERN_SUCCESS) {
         if (thr_info.run_state < run_state) {
            run_state = thr_info.run_state;
         }
         if(run_state == TH_STATE_WAITING && thr_info.sleep_time < sleep_time) {
            sleep_time = thr_info.sleep_time;
         }
         if(!dp->libproc_accessible) {
            utime += (uint64_t)thr_info.user_time.seconds * 1000000000 + (uint64_t)thr_info.user_time.microseconds * 1000;
            stime += (uint64_t)thr_info.system_time.seconds * 1000000000 + (uint64_t)thr_info.system_time.microseconds * 1000;
         }
         mach_port_deallocate(mach_task_self(), thread_list[i]);
      }
   }
   vm_deallocate(mach_task_self(), (vm_address_t) thread_list, sizeof(thread_port_array_t) * thread_count);
   mach_port_deallocate(mach_task_self(), port);

   if(!dp->libproc_accessible) {
      set_time(dp, dpl, utime, stime);
      proc->nlwp = thread_count;
   }

   switch (run_state) {
      case TH_STATE_RUNNING:
         proc->state = 'R';
         break;
      case TH_STATE_STOPPED:
         proc->state = 'T';
         break;
      case TH_STATE_WAITING:
         proc->state = sleep_time > 20 ? 'I' : 'S';
         break;
      case TH_STATE_UNINTERRUPTIBLE:
         proc->state = 'D';
         break;
      case TH_STATE_HALTED:
         proc->state = 'H';
         break;
      default:
         proc->state = '?';
         break;
   }
}

char **Process_getKernelStackTrace(const Process *this) {
	return NULL;
}
