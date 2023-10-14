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
   HTOP_PROCESS_COLOR, HTOP_PROCESS_THREAD_COLOR, HTOP_CPU_KERNEL_COLOR, HTOP_CPU_KERNEL_COLOR, HTOP_TASKS_RUNNING_COLOR
};

static void TasksMeter_updateValues(Meter* this, char* buffer, int len) {
   ProcessList* pl = this->pl;
   this->values[0] = pl->totalTasks;
   this->values[1] = pl->thread_count > pl->totalTasks ? pl->thread_count : 0;
   if (this->pl->settings->tasks_meter_show_kernel_process_count) {
      this->values[2] = pl->kernel_process_count;
      this->values[3] = pl->kernel_thread_count > pl->kernel_process_count ? pl->kernel_thread_count : 0;
   } else {
      this->values[2] = 0;
      this->values[3] = 0;
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
   RichString_write(out, CRT_colors[HTOP_METER_TEXT_COLOR], ": ");
   char buffer[20];
   xSnprintf(buffer, sizeof(buffer), "%d", (int)this->values[0]);
   RichString_append(out, CRT_colors[HTOP_METER_VALUE_COLOR], buffer);
   RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], " proc");
   int threadValueColor, threadCaptionColor;
   if (settings->highlightThreads) {
      threadValueColor = CRT_colors[HTOP_PROCESS_THREAD_BASENAME_COLOR];
      threadCaptionColor = CRT_colors[HTOP_PROCESS_THREAD_COLOR];
   } else {
      threadValueColor = CRT_colors[HTOP_METER_VALUE_COLOR];
      threadCaptionColor = CRT_colors[HTOP_METER_TEXT_COLOR];
   }
   if (this->values[1] > 0) {
      RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], ", ");
      xSnprintf(buffer, sizeof(buffer), "%d", (int)this->values[1]);
      RichString_append(out, threadValueColor, buffer);
      RichString_append(out, threadCaptionColor, " thr");
   }
   if (settings->tasks_meter_show_kernel_process_count) {
      RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], ", ");
      xSnprintf(buffer, sizeof(buffer), "%d", (int)this->values[2]);
      RichString_append(out, CRT_colors[HTOP_METER_VALUE_COLOR], buffer);
      RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], " kproc");
      if(this->values[3] > 0) {
         RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], ", ");
         xSnprintf(buffer, sizeof(buffer), "%d", (int)this->values[3]);
         RichString_append(out, threadValueColor, buffer);
         RichString_append(out, threadCaptionColor, " kthr");
      }
   }
   RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], "; ");
   xSnprintf(buffer, sizeof(buffer), "%d", (int)this->values[4]);
   RichString_append(out, CRT_colors[HTOP_TASKS_RUNNING_COLOR], buffer);
   RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], " running");
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
   .caption = "Tasks"
};
