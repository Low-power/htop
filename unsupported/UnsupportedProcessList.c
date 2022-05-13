/*
htop - unsupported/UnsupportedProcessList.c
(C) 2014 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "ProcessList.h"
#include "UnsupportedProcess.h"

#include <stdlib.h>
#include <string.h>

/*{

}*/

ProcessList* ProcessList_new(UsersTable* usersTable, Hashtable* pidWhiteList, uid_t userId) {
   ProcessList* this = xCalloc(1, sizeof(ProcessList));
   ProcessList_init(this, Class(Process), usersTable, pidWhiteList, userId);
   
   return this;
}

void ProcessList_delete(ProcessList* this) {
   ProcessList_done(this);
   free(this);
}

void ProcessList_goThroughEntries(ProcessList *this) {
	bool is_existing;
	Process *proc = ProcessList_getProcess(this, 1, &is_existing, UnsupportedProcess_new);

	proc->time = proc->time + 10;
	proc->updated = true;

	if(is_existing) return;

	/* Empty values */
	proc->pid  = 1;
	proc->ppid = 1;
	proc->tgid = 0;
	proc->name = xStrdup("<unsupported>");
	proc->comm = xStrdup("<unsupported architecture>");
	proc->basenameOffset = 0;

	proc->state = 'R';
	proc->show = true; /* Reflected in proc->settings-> "hideXXX" really */
	proc->pgrp = 0;
	proc->session = 0;
	proc->tty_nr = 0;
	proc->tpgid = 0;
	proc->ruid = 0;
	proc->euid = 0;
	proc->processor = 0;

	proc->percent_cpu = 2.5;
	proc->percent_mem = 2.5;
	proc->real_user = "nobody";
	proc->effective_user = "nobody";

	proc->priority = 0;
	proc->nice = 0;
	proc->nlwp = 1;
	strcpy(proc->starttime_show, "Jun 01 ");
	proc->starttime_ctime = 1433116800; // Jun 01, 2015

	proc->m_size = 100;
	proc->m_resident = 100;

	proc->minflt = 20;
	proc->majflt = 20;

	ProcessList_add(this, proc);
}
