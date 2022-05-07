/*
htop - TraceScreen.c
(C) 2005-2006 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "TraceScreen.h"

#include "CRT.h"
#include "InfoScreen.h"
#include "ProcessList.h"
#include "ListItem.h"
#include "IncSet.h"
#include "StringUtils.h"
#include "FunctionBar.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

/*{
#include "InfoScreen.h"

typedef struct TraceScreen_ {
   InfoScreen super;
   bool tracing;
   int child;
   int trace_fd;
   bool contLine;
   bool follow;
} TraceScreen;

}*/

static const char* const TraceScreenFunctions[] = {"Search ", "Filter ", "AutoScroll ", "Stop Tracing   ", "Done   ", NULL};

static const char* const TraceScreenKeys[] = {"F3", "F4", "F8", "F9", "Esc"};

static int TraceScreenEvents[] = {KEY_F(3), KEY_F(4), KEY_F(8), KEY_F(9), 27};

InfoScreenClass TraceScreen_class = {
   .super = {
      .extends = Class(Object),
      .delete = TraceScreen_delete
   },
   .draw = TraceScreen_draw,
   .onErr = TraceScreen_updateTrace,
   .onKey = TraceScreen_onKey,
};

TraceScreen* TraceScreen_new(Process* process) {
   TraceScreen* this = xMalloc(sizeof(TraceScreen));
   Object_setClass(this, Class(TraceScreen));
   this->tracing = true;
   this->contLine = false;
   this->follow = false;
   FunctionBar* fuBar = FunctionBar_new(TraceScreenFunctions, TraceScreenKeys, TraceScreenEvents);
   CRT_disableDelay();
   return (TraceScreen*) InfoScreen_init(&this->super, process, fuBar, LINES-2, "");
}

void TraceScreen_delete(Object* cast) {
   TraceScreen* this = (TraceScreen*) cast;
   if (this->child > 0) {
      kill(this->child, SIGTERM);
      waitpid(this->child, NULL, 0);
      close(this->trace_fd);
   }
   CRT_enableDelay();
   free(InfoScreen_done((InfoScreen*)cast));
}

void TraceScreen_draw(InfoScreen* this) {
   attrset(CRT_colors[PANEL_HEADER_FOCUS]);
   mvhline(0, 0, ' ', COLS);
   mvprintw(0, 0, "Trace of process %d - %s", this->process->pid, this->process->comm);
   attrset(CRT_colors[DEFAULT_COLOR]);
   IncSet_drawBar(this->inc);
}

bool TraceScreen_forkTracer(TraceScreen* this) {
   int fdpair[2];
   if(pipe(fdpair) < 0) return false;
   this->child = fork();
   if (this->child == -1) return false;
   if (this->child == 0) {
      CRT_dropPrivileges();
      close(fdpair[0]);
      dup2(fdpair[1], STDERR_FILENO);
      if(fdpair[1] != STDERR_FILENO) close(fdpair[1]);
      const char *message;
      if(fcntl(STDERR_FILENO, F_SETFL, O_NONBLOCK) == -1) {
         message = strerror(errno);
      } else {
         char buffer[22];
         xSnprintf(buffer, sizeof(buffer), "%d", (int)this->super.process->pid);
#ifdef TRUSS_SUPPORT_STRING_SIZE
         execlp("truss", "truss", "-s", "512", "-p", buffer, NULL);
#else
         execlp("truss", "truss", "-p", buffer, NULL);
#endif
         execlp("strace", "strace", "-s", "512", "-p", buffer, NULL);
         message = "Could not execute 'truss' or 'strace'. Please make sure it is available in your $PATH.";
      }
      ssize_t written = write(STDERR_FILENO, message, strlen(message));
      (void) written;
      _exit(1);
   }
   close(fdpair[1]);
   if(fcntl(fdpair[0], F_SETFL, O_NONBLOCK) == -1) return false;
   this->trace_fd = fdpair[0];
   return true;
}

void TraceScreen_updateTrace(InfoScreen* super) {
   TraceScreen* this = (TraceScreen*) super;
   char buffer[1024];
   int nready, len;
   fd_set fds;
   FD_ZERO(&fds);
   FD_SET(this->trace_fd, &fds);
   struct timeval timeout = { .tv_sec = 0, .tv_usec = 500 };
   do {
      nready = select(this->trace_fd + 1, &fds, NULL, NULL, &timeout);
   } while(nready < 0 && errno == EINTR);
   if(nready <= 0 || !FD_ISSET(this->trace_fd, &fds)) return;
   do {
      len = read(this->trace_fd, buffer, sizeof buffer - 1);
   } while(len < 0 && errno == EINTR);
   if(len < 1 || !this->tracing) return;

   char* line = buffer;
   buffer[len] = '\0';
   bool end_of_call = false;
   bool skip_space = true;
   for (int i = 0; i < len; i++) switch(buffer[i]) {
      case 0:
      case '\n':
         buffer[i] = '\0';
         if (this->contLine) {
            InfoScreen_appendLine(&this->super, line);
            this->contLine = false;
         } else {
            InfoScreen_addLine(&this->super, line);
         }
         line = buffer + i + 1;
         skip_space = true;
         break;
      case ')':
         end_of_call = true;
         break;
      case '	':
      //case ' ':
         if(end_of_call && skip_space) {
            int j = i;
            while(++j < len && (buffer[j] == ' ' || buffer[j] == '	'));
            size_t skip_len = j - i - 1;
            if(skip_len > 0) memmove(buffer + i + 1, buffer + j, len - j);
            len -= skip_len;
            buffer[i] = ' ';
            skip_space = false;
         }
         break;
      default:
         end_of_call = false;
         break;
   }
   if (line < buffer + len) {
      InfoScreen_addLine(&this->super, line);
      this->contLine = true;
   }
   if (this->follow) {
      Panel_setSelected(this->super.display, Panel_size(this->super.display)-1);
   }
}

bool TraceScreen_onKey(InfoScreen* super, int ch) {
   TraceScreen* this = (TraceScreen*) super;
   switch(ch) {
      case 'f':
      case KEY_F(8):
         this->follow = !this->follow;
         if (this->follow) {
            Panel_setSelected(super->display, Panel_size(super->display)-1);
         }
         return true;
      case 't':
      case KEY_F(9):
         this->tracing = !this->tracing;
         FunctionBar_setLabel(super->display->defaultBar, KEY_F(9), this->tracing?"Stop Tracing   ":"Resume Tracing ");
         InfoScreen_draw(this);
         return true;
   }
   this->follow = false;
   return false;
}
