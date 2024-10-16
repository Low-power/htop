/*
htop - SignalsPanel.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
typedef struct SignalItem_ {
   const char* name;
   int number;
} SignalItem;
}*/

#include "Panel.h"
#include "SignalsPanel.h"
#include "Platform.h"
#include "ListItem.h"
#include "CRT.h"
#include <signal.h>
#include <stdlib.h>
#include <assert.h>

#define DEFAULT_SIGNAL SIGTERM

Panel* SignalsPanel_new() {
   static char buf[16];
   Panel* this = Panel_new(1, 1, 1, 1, true, Class(ListItem), FunctionBar_newEnterEsc("Send   ", "Cancel "));
   int defaultPosition = 15;
   unsigned int i;
   for (i = 0; i < Platform_numberOfSignals; i++) {
      const SignalItem *sig = Platform_signals + i;
      xSnprintf(buf, sizeof buf, "%3d %s", sig->number, sig->name);
      Panel_set(this, i, (Object *)ListItem_new(buf, HTOP_DEFAULT_COLOR, sig->number, NULL));
      // signal 15 is not always the 15th signal in the table
      if (sig->number == DEFAULT_SIGNAL) {
         defaultPosition = i;
      }
   }
   #if (defined(SIGRTMIN) && defined(SIGRTMAX))
   if (SIGRTMAX - SIGRTMIN <= 100) {
      for (int sig = SIGRTMIN; sig <= SIGRTMAX; i++, sig++) {
         int n = sig - SIGRTMIN;
         xSnprintf(buf, 16, "%3d RTMIN%-+3d", sig, n);
         if (n == 0) {
            buf[11] = '\0';
         }
         Panel_set(this, i, (Object *)ListItem_new(buf, HTOP_DEFAULT_COLOR, sig, NULL));
      }
   }
   #endif
   Panel_setHeader(this, "Send signal:");
   Panel_setSelected(this, defaultPosition);
   return this;
}
