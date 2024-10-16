/*
htop - CPUMeter.c
(C) 2004-2011 Hisham H. Muhammad
Copyright 2015-2024 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "CPUMeter.h"

#include "CRT.h"
#include "Settings.h"
#include "Platform.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*{
#include "Meter.h"

typedef enum {
   CPU_METER_NICE = 0,
   CPU_METER_NORMAL = 1,
   CPU_METER_KERNEL = 2,
   CPU_METER_IRQ = 3,
   CPU_METER_SOFTIRQ = 4,
   CPU_METER_STEAL = 5,
   CPU_METER_GUEST = 6,
   CPU_METER_IOWAIT = 7,
   CPU_METER_ITEMCOUNT = 8, // number of entries in this enum
} CPUMeterValue;

}*/

static const int CPUMeter_detailed_attributes[] = {
   HTOP_CPU_NICE_COLOR, HTOP_CPU_NORMAL_COLOR, HTOP_CPU_KERNEL_COLOR, HTOP_CPU_IRQ_COLOR, HTOP_CPU_SOFTIRQ_COLOR, HTOP_CPU_STEAL_COLOR, HTOP_CPU_GUEST_COLOR, HTOP_CPU_IOWAIT_COLOR
};

static const int CPUMeter_attributes[] = {
   HTOP_CPU_NICE_COLOR, HTOP_CPU_NORMAL_COLOR, HTOP_CPU_KERNEL_COLOR, HTOP_CPU_GUEST_COLOR
};

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

static void CPUMeter_init(Meter* this) {
   if (this->param == 0) {
      Meter_setCaption(this, "Avg");
      return;
   }
   int cpu_id = Settings_cpuId(this->pl->settings, this->param - 1);
   if (this->pl->cpuCount > 1) {
      char caption[10];
      xSnprintf(caption, sizeof(caption), "%-3d", cpu_id);
      Meter_setShortCaption(this, caption);
      xSnprintf(caption, sizeof caption, "CPU%d", cpu_id);
      Meter_setCaption(this, caption);
   }
}

static int CPUMeter_getAttribute(Meter *this, int i) {
	return (this->pl->settings->detailedCPUTime ? CPUMeter_detailed_attributes : CPUMeter_attributes)[i];
}

static void CPUMeter_updateValues(Meter* this, char* buffer, int size) {
   int cpu = this->param;
   if (cpu > this->pl->cpuCount) {
      xSnprintf(buffer, size, "absent");
      return;
   }
   memset(this->values, 0, sizeof(double) * CPU_METER_ITEMCOUNT);
   double percent = Platform_updateCPUValues(this, cpu);
   xSnprintf(buffer, size, "%5.1f%%", percent);
}

static void CPUMeter_display(Object* cast, RichString* out) {
   char buffer[50];
   Meter* this = (Meter*)cast;
   RichString_prune(out);
   if (this->param > this->pl->cpuCount) {
      RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], "absent");
      return;
   }
   xSnprintf(buffer, sizeof(buffer), " %5.1f%%", this->values[CPU_METER_NORMAL]);
   RichString_append(out, CRT_colors[HTOP_CPU_NORMAL_COLOR], buffer);
   if (this->pl->settings->detailedCPUTime) {
      RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], "us ");
      xSnprintf(buffer, sizeof(buffer), " %5.1f%%", this->values[CPU_METER_KERNEL]);
      RichString_append(out, CRT_colors[HTOP_CPU_KERNEL_COLOR], buffer);
      RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], "sy");
      xSnprintf(buffer, sizeof(buffer), " %5.1f%%", this->values[CPU_METER_NICE]);
      RichString_append(out, CRT_colors[HTOP_CPU_NICE_TEXT_COLOR], buffer);
      RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], "ni");
      xSnprintf(buffer, sizeof(buffer), " %5.1f%%", this->values[CPU_METER_IRQ]);
      RichString_append(out, CRT_colors[HTOP_CPU_IRQ_COLOR], buffer);
      RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], "hi");
      xSnprintf(buffer, sizeof(buffer), " %5.1f%%", this->values[CPU_METER_SOFTIRQ]);
      RichString_append(out, CRT_colors[HTOP_CPU_SOFTIRQ_COLOR], buffer);
      RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], "si");
      if (this->values[CPU_METER_STEAL] > 0) {
         xSnprintf(buffer, sizeof(buffer), " %5.1f%%", this->values[CPU_METER_STEAL]);
         RichString_append(out, CRT_colors[HTOP_CPU_STEAL_COLOR], buffer);
         RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], "st");
      }
      if (this->values[CPU_METER_GUEST] > 0) {
         xSnprintf(buffer, sizeof(buffer), " %5.1f%%", this->values[CPU_METER_GUEST]);
         RichString_append(out, CRT_colors[HTOP_CPU_GUEST_COLOR], buffer);
         RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], "gu");
      }
      xSnprintf(buffer, sizeof(buffer), " %5.1f%%", this->values[CPU_METER_IOWAIT]);
      RichString_append(out, CRT_colors[HTOP_CPU_IOWAIT_COLOR], buffer);
      RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], "wa");
   } else {
      RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], "usr");
      xSnprintf(buffer, sizeof(buffer), " %5.1f%%", this->values[CPU_METER_KERNEL]);
      RichString_append(out, CRT_colors[HTOP_CPU_KERNEL_COLOR], buffer);
      RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], "sys");
      xSnprintf(buffer, sizeof(buffer), " %5.1f%%", this->values[2]);
      RichString_append(out, CRT_colors[HTOP_CPU_NICE_TEXT_COLOR], buffer);
      RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], "low");
      if (this->values[3] > 0) {
         xSnprintf(buffer, sizeof(buffer), " %5.1f%%", this->values[3]);
         RichString_append(out, CRT_colors[HTOP_CPU_GUEST_COLOR], buffer);
         RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], "vir");
      }
   }
}

static void AllCPUsMeter_getRange(Meter* this, int* start, int* count) {
   int cpus = this->pl->cpuCount;
   switch(Meter_name(this)[0]) {
      default:
      case 'A': // All
         *start = 0;
         *count = cpus;
         break;
      case 'L': // First Half
         *start = 0;
         *count = (cpus+1) / 2;
         break;
      case 'R': // Second Half
         *start = (cpus+1) / 2;
         *count = cpus / 2;
         break;
   }
}

static void AllCPUsMeter_init(Meter* this) {
   int cpus = this->pl->cpuCount;
   if (!this->drawData) this->drawData = xCalloc(cpus, sizeof(Meter*));
   Meter **meters = this->drawData;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++) {
      if (!meters[i]) {
         meters[i] = Meter_new(this->pl, start+i+1, (MeterClass*) Class(CPUMeter));
      }
      Meter_init(meters[i]);
   }
   if (this->mode == 0) this->mode = BAR_METERMODE;
   int h = Meter_modes[this->mode]->h;
   this->h = h * (strchr(Meter_name(this), '2') ? (count+1) / 2 : count);
}

static void AllCPUsMeter_done(Meter* this) {
   Meter **meters = this->drawData;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++) Meter_delete((Object*)meters[i]);
}

static void AllCPUsMeter_updateMode(Meter* this, int mode) {
   Meter **meters = this->drawData;
   this->mode = mode;
   int h = Meter_modes[mode]->h;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++) {
      Meter_setMode(meters[i], mode);
   }
   this->h = h * (strchr(Meter_name(this), '2') ? (count+1) / 2 : count);
}

static void DualColCPUsMeter_draw(Meter* this, int x, int y, int w) {
   Meter **meters = this->drawData;
   int start, count;
   int pad = this->pl->settings->headerMargin ? 2 : 0;
   AllCPUsMeter_getRange(this, &start, &count);
   int height = (count+1)/2;
   int startY = y;
   for (int i = 0; i < height; i++) {
      meters[i]->draw(meters[i], x, y, (w-pad)/2);
      y += meters[i]->h;
   }
   y = startY;
   for (int i = height; i < count; i++) {
      meters[i]->draw(meters[i], x+(w-1)/2+1+(pad/2), y, (w-pad)/2);
      y += meters[i]->h;
   }
}

static void SingleColCPUsMeter_draw(Meter* this, int x, int y, int w) {
   Meter **meters = this->drawData;
   int start, count;
   AllCPUsMeter_getRange(this, &start, &count);
   for (int i = 0; i < count; i++) {
      meters[i]->draw(meters[i], x, y, w);
      y += meters[i]->h;
   }
}

MeterClass CPUMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .updateValues = CPUMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .maxItems = CPU_METER_ITEMCOUNT,
   .total = 100.0,
   .getAttribute = CPUMeter_getAttribute,
   .name = "CPU",
   .uiName = "CPU",
   .caption = "CPU",
   .init = CPUMeter_init
};

MeterClass AllCPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .getAttribute = CPUMeter_getAttribute,
   .name = "AllCPUs",
   .uiName = "CPUs (1/1)",
   .description = "CPUs (1/1): all CPUs",
   .caption = "CPU",
   .draw = SingleColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

MeterClass AllCPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .getAttribute = CPUMeter_getAttribute,
   .name = "AllCPUs2",
   .uiName = "CPUs (1&2/2)",
   .description = "CPUs (1&2/2): all CPUs in 2 shorter columns",
   .caption = "CPU",
   .draw = DualColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

MeterClass LeftCPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .getAttribute = CPUMeter_getAttribute,
   .name = "LeftCPUs",
   .uiName = "CPUs (1/2)",
   .description = "CPUs (1/2): first half of list",
   .caption = "CPU",
   .draw = SingleColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

MeterClass RightCPUsMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .getAttribute = CPUMeter_getAttribute,
   .name = "RightCPUs",
   .uiName = "CPUs (2/2)",
   .description = "CPUs (2/2): second half of list",
   .caption = "CPU",
   .draw = SingleColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

MeterClass LeftCPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .getAttribute = CPUMeter_getAttribute,
   .name = "LeftCPUs2",
   .uiName = "CPUs (1&2/4)",
   .description = "CPUs (1&2/4): first half in 2 shorter columns",
   .caption = "CPU",
   .draw = DualColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

MeterClass RightCPUs2Meter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = CPUMeter_display
   },
   .defaultMode = CUSTOM_METERMODE,
   .total = 100.0,
   .getAttribute = CPUMeter_getAttribute,
   .name = "RightCPUs2",
   .uiName = "CPUs (3&4/4)",
   .description = "CPUs (3&4/4): second half in 2 shorter columns",
   .caption = "CPU",
   .draw = DualColCPUsMeter_draw,
   .init = AllCPUsMeter_init,
   .updateMode = AllCPUsMeter_updateMode,
   .done = AllCPUsMeter_done
};

