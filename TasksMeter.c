/*
htop - TasksMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "TasksMeter.h"

#include "Platform.h"
#include "CRT.h"

/*{
#include "Meter.h"
}*/

int TasksMeter_attributes[] = {
   PROCESS, PROCESS_THREAD, CPU_KERNEL, CPU_KERNEL, TASKS_RUNNING
};

static void TasksMeter_updateValues(Meter* this, char* buffer, int len) {
   ProcessList* pl = this->pl;
   this->values[0] = pl->totalTasks;
   this->values[1] = pl->thread_count > pl->totalTasks ? pl->thread_count : 0;
   if (this->pl->settings->hide_kernel_processes) {
      this->values[2] = 0;
      this->values[3] = 0;
   } else {
      this->values[2] = pl->kernel_process_count;
      this->values[3] = pl->kernel_thread_count > pl->kernel_process_count ? pl->kernel_thread_count : 0;
   }
   this->values[4] = MIN(pl->running_thread_count, pl->cpuCount);
   if (pl->totalTasks > this->total) {
      this->total = pl->totalTasks;
   }
   xSnprintf(buffer, len, "%d/%d", (int) this->values[3], (int) this->total);
}

static void TasksMeter_display(Object* cast, RichString* out) {
   Meter* this = (Meter*)cast;
   Settings* settings = this->pl->settings;
   char buffer[20];
   xSnprintf(buffer, sizeof(buffer), "%d", (int)this->values[0]);
   RichString_write(out, CRT_colors[METER_VALUE], buffer);
   RichString_append(out, CRT_colors[METER_TEXT], " proc");
   int threadValueColor, threadCaptionColor;
   if (settings->highlightThreads) {
      threadValueColor = CRT_colors[PROCESS_THREAD_BASENAME];
      threadCaptionColor = CRT_colors[PROCESS_THREAD];
   } else {
      threadValueColor = CRT_colors[METER_VALUE];
      threadCaptionColor = CRT_colors[METER_TEXT];
   }
   if (!settings->hide_thread_processes && this->values[1] > 0) {
      RichString_append(out, CRT_colors[METER_TEXT], ", ");
      xSnprintf(buffer, sizeof(buffer), "%d", (int)this->values[1]);
      RichString_append(out, threadValueColor, buffer);
      RichString_append(out, threadCaptionColor, " thr");
   }
   if (!settings->hide_kernel_processes) {
      RichString_append(out, CRT_colors[METER_TEXT], ", ");
      xSnprintf(buffer, sizeof(buffer), "%d", (int)this->values[2]);
      RichString_append(out, CRT_colors[METER_VALUE], buffer);
      RichString_append(out, CRT_colors[METER_TEXT], " kproc");
      if(this->values[3] > 0) {
         RichString_append(out, CRT_colors[METER_TEXT], ", ");
         xSnprintf(buffer, sizeof(buffer), "%d", (int)this->values[3]);
         RichString_append(out, threadValueColor, buffer);
         RichString_append(out, threadCaptionColor, " kthr");
      }
   }
   RichString_append(out, CRT_colors[METER_TEXT], "; ");
   xSnprintf(buffer, sizeof(buffer), "%d", (int)this->values[4]);
   RichString_append(out, CRT_colors[TASKS_RUNNING], buffer);
   RichString_append(out, CRT_colors[METER_TEXT], " running");
}

MeterClass TasksMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = TasksMeter_display,
   },
   .updateValues = TasksMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 5,
   .total = 100.0,
   .attributes = TasksMeter_attributes, 
   .name = "Tasks",
   .uiName = "Task counter",
   .caption = "Tasks: "
};
