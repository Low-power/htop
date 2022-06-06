/*
htop - OpenFilesScreen.c
(C) 2005-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "InfoScreen.h"
#include <sys/types.h>

typedef struct OpenFiles_Data_ {
   char* data[256];
} OpenFiles_Data;

typedef struct OpenFiles_ProcessData_ {
   OpenFiles_Data data;
   int error;
   struct OpenFiles_FileData_* files;
} OpenFiles_ProcessData;

typedef struct OpenFiles_FileData_ {
   OpenFiles_Data data;
   struct OpenFiles_FileData_* next;
} OpenFiles_FileData;

typedef struct OpenFilesScreen_ {
   InfoScreen super;
   pid_t pid;
} OpenFilesScreen;
}*/

#include "OpenFilesScreen.h"
#include "CRT.h"
#include "StringUtils.h"
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

InfoScreenClass OpenFilesScreen_class = {
   .super = {
      .extends = Class(Object),
      .delete = OpenFilesScreen_delete
   },
   .scan = OpenFilesScreen_scan,
   .draw = OpenFilesScreen_draw
};

OpenFilesScreen* OpenFilesScreen_new(Process* process) {
   OpenFilesScreen* this = xMalloc(sizeof(OpenFilesScreen));
   Object_setClass(this, Class(OpenFilesScreen));
   this->pid = Process_isExtraThreadProcess(process) ? process->tgid : process->pid;
   return (OpenFilesScreen*) InfoScreen_init(&this->super, process, NULL, LINES-3, "   FD TYPE     DEVICE       SIZE       NODE NAME");
}

void OpenFilesScreen_delete(Object* this) {
   free(InfoScreen_done((InfoScreen*)this));
}

void OpenFilesScreen_draw(InfoScreen* this) {
   InfoScreen_drawTitled(this, "Snapshot of files open in process %d - %s",
      (int)((OpenFilesScreen *)this)->pid, this->process->comm);
}

static OpenFiles_ProcessData* OpenFilesScreen_getProcessData(pid_t pid) {
   char buffer[1024];
   xSnprintf(buffer, sizeof buffer, "%d", (int)pid);
   OpenFiles_ProcessData* pdata = xCalloc(1, sizeof(OpenFiles_ProcessData));
   OpenFiles_FileData* fdata = NULL;
   OpenFiles_Data* item = &(pdata->data);
   int fdpair[2];
   if (pipe(fdpair) == -1) {
      pdata->error = 1;
      return pdata;
   }
   pid_t child = fork();
   if (child == -1) {
      close(fdpair[0]);
      close(fdpair[1]);
      pdata->error = 1;
      return pdata;
   }
   if (child == 0) {
      close(fdpair[0]);
      dup2(fdpair[1], STDOUT_FILENO);
      if(fdpair[1] != STDOUT_FILENO) close(fdpair[1]);
      int fd = open("/dev/null", O_WRONLY);
      if(fd == -1) _exit(1);
      dup2(fd, STDERR_FILENO);
      if(fd != STDERR_FILENO) close(fd);
      execlp("lsof", "lsof", "-P", "-p", buffer, "-F", (char *)NULL);
      _exit(127);
   }
   close(fdpair[1]);
   FILE *f = fdopen(fdpair[0], "r");
   for (;;) {
      char* line = String_readLine(f);
      if (!line) {
         break;
      }
      unsigned char cmd = line[0];
      if (cmd == 'f') {
         OpenFiles_FileData* nextFile = xCalloc(1, sizeof(OpenFiles_FileData));
         if (fdata == NULL) {
            pdata->files = nextFile;
         } else {
            fdata->next = nextFile;
         }
         fdata = nextFile;
         item = &(fdata->data);
      }
      item->data[cmd] = xStrdup(line + 1);
      free(line);
   }
   fclose(f);
   int wstatus;
   if (waitpid(child, &wstatus, 0) == -1) {
      pdata->error = 1;
      return pdata;
   }
   if (!WIFEXITED(wstatus)) pdata->error = 1;
   else pdata->error = WEXITSTATUS(wstatus);
   return pdata;
}

static inline void OpenFiles_Data_clear(OpenFiles_Data* data) {
   for (int i = 0; i < 255; i++) {
      free(data->data[i]);
   }
}

void OpenFilesScreen_scan(InfoScreen* this) {
   Panel* panel = this->display;
   int idx = Panel_getSelectedIndex(panel);
   Panel_prune(panel);
   OpenFiles_ProcessData* pdata = OpenFilesScreen_getProcessData(((OpenFilesScreen*)this)->pid);
   if (pdata->error == 127) {
      InfoScreen_addLine(this,
         "Could not execute 'lsof'. Please make sure it is available in your $PATH.",
         HTOP_LARGE_NUMBER_COLOR);
   } else if (pdata->error == 1) {
      InfoScreen_addLine(this, "Failed listing open files.", HTOP_LARGE_NUMBER_COLOR);
   } else {
      OpenFiles_FileData* fdata = pdata->files;
      while (fdata) {
         char** data = fdata->data.data;
         int lenN = data['n'] ? strlen(data['n']) : 0;
         int sizeEntry = 5 + 7 + 10 + 10 + 10 + lenN + 5 /*spaces*/ + 1 /*null*/;
         char entry[sizeEntry];
         xSnprintf(entry, sizeEntry, "%5.5s %7.7s %10.10s %10.10s %10.10s %s",
            data['f'] ? data['f'] : "",
            data['t'] ? data['t'] : "",
            data['D'] ? data['D'] : "",
            data['s'] ? data['s'] : "",
            data['i'] ? data['i'] : "",
            data['n'] ? data['n'] : "");
         InfoScreen_addLine(this, entry, HTOP_DEFAULT_COLOR);
         OpenFiles_Data_clear(&fdata->data);
         OpenFiles_FileData* old = fdata;
         fdata = fdata->next;
         free(old);
      }
      OpenFiles_Data_clear(&pdata->data);
   }
   free(pdata);
   Vector_insertionSort(this->lines);
   Vector_insertionSort(panel->items);
   Panel_setSelected(panel, idx);
}
