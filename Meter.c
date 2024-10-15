/*
htop - Meter.c
(C) 2004-2011 Hisham H. Muhammad
Copyright 2015-2024 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include "ProcessList.h"
#include "ListItem.h"
#include <sys/time.h>

#define METER_BUFFER_LEN 256

typedef struct Meter_ Meter;

typedef void(*Meter_Init)(Meter*);
typedef void(*Meter_Done)(Meter*);
typedef void(*Meter_UpdateMode)(Meter*, int);
typedef void(*Meter_UpdateValues)(Meter*, char*, int);
typedef void(*Meter_Draw)(Meter*, int, int, int);
typedef double (*MeterGetDoubleFunction)(Meter *);
typedef int (*MeterGetAttributeFunction)(Meter *, int);

typedef struct MeterClass_ {
   ObjectClass super;
   Meter_Init init;
   Meter_Done done;
   Meter_UpdateMode updateMode;
   Meter_Draw draw;
   Meter_UpdateValues updateValues;
   MeterGetDoubleFunction getMaximum;
   MeterGetAttributeFunction getAttribute;
   int defaultMode;
   double total;
   const int* attributes;
   // For internal use only
   const char* name;
   // For display in setup screen
   const char* uiName;
   // For Text mode and LED mode display
   const char* caption;
   // For Bar mode and Graph mode display, default to caption if NULL
   const char *short_caption;
   const char* description;
   const char maxItems;
   char curItems;
   bool values_are_overlapped;
} MeterClass;

#define As_Meter(this_)                ((MeterClass*)((this_)->super.klass))
#define Meter_initFn(this_)            As_Meter(this_)->init
#define Meter_init(this_)              As_Meter(this_)->init((Meter*)(this_))
#define Meter_done(this_)              As_Meter(this_)->done((Meter*)(this_))
#define Meter_updateModeFn(this_)      As_Meter(this_)->updateMode
#define Meter_updateMode(this_, m_)    As_Meter(this_)->updateMode((Meter*)(this_), (m_))
#define Meter_drawFn(this_)            As_Meter(this_)->draw
#define Meter_doneFn(this_)            As_Meter(this_)->done
#define Meter_updateValues(this_, buf_, sz_) \
                                       As_Meter(this_)->updateValues((Meter*)(this_), (buf_), (sz_))
#define Meter_defaultMode(this_)       As_Meter(this_)->defaultMode
#define Meter_getItems(this_)          As_Meter(this_)->curItems
#define Meter_setItems(this_, n_)      As_Meter(this_)->curItems = (n_)
#define Meter_attributes(this_)        As_Meter(this_)->attributes
#define Meter_name(this_)              As_Meter(this_)->name
#define Meter_uiName(this_)            As_Meter(this_)->uiName
#define Meter_getMaximum(this_)        As_Meter(this_)->getMaximum(this_)
#define Meter_getAttribute(this_,n_)   As_Meter(this_)->getAttribute((this_),(n_))

struct Meter_ {
   Object super;
   Meter_Draw draw;
   char* caption;
   char *short_caption;
   int mode;
   int param;
   void* drawData;
   int h;
   ProcessList *pl;
   double* values;
   double total;
};

typedef struct MeterMode_ {
   Meter_Draw draw;
   const char* uiName;
   int h;
} MeterMode;

typedef enum {
   CUSTOM_METERMODE = 0,
   BAR_METERMODE,
   TEXT_METERMODE,
   GRAPH_METERMODE,
   LED_METERMODE,
   LAST_METERMODE
} MeterModeId;

typedef struct GraphData_ {
   struct timeval time;
   double values[METER_BUFFER_LEN];
} GraphData;

}*/

#include "config.h"
#include "Meter.h"
#include "RichString.h"
#include "Object.h"
#include "CRT.h"
#include "StringUtils.h"
#include "Settings.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define GRAPH_HEIGHT 4 /* Unit: rows (lines) */

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef CLAMP
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))
#endif

#ifndef HAVE_LROUND
static long int lround(double v) {
	long int n = v;
	v -= (double)n;
	if(v < -0.5) n--;
	else if(v > 0.5) n++;
	return n;
}
#endif

static double base_Meter_getMaximum(Meter *this) {
	return this->total;
}

static int base_Meter_getAttribute(Meter *this, int i) {
	return Meter_attributes(this)[i];
}

static void Meter_inherit(ObjectClass *super_class) {
	MeterClass *class = (MeterClass *)super_class;
	if(!class->getMaximum) class->getMaximum = base_Meter_getMaximum;
	if(!class->getAttribute) class->getAttribute = base_Meter_getAttribute;
}

MeterClass Meter_class = {
   .super = {
      .extends = Class(Object),
      .inherit = Meter_inherit,
   }
};

Meter* Meter_new(ProcessList *pl, int param, MeterClass* type) {
   Meter* this = xCalloc(1, sizeof(Meter));
   Object_setClass(this, type);
   this->h = 1;
   this->param = param;
   this->pl = pl;
   type->curItems = type->maxItems;
   this->values = xCalloc(type->maxItems, sizeof(double));
   this->total = type->total;
   this->caption = xStrdup(type->caption);
   if(type->short_caption) this->short_caption = xStrdup(type->short_caption);
   if (Meter_initFn(this))
      Meter_init(this);
   Meter_setMode(this, type->defaultMode);
   return this;
}

int Meter_humanUnit(char* buffer, unsigned long int value, int size) {
   const char * prefix = "KMGTPEZY";
   unsigned long int powi = 1;
   unsigned int written, powj = 1, precision = 2;

   for(;;) {
      if (value / 1024 < powi)
         break;

      if (prefix[1] == '\0')
         break;

      powi *= 1024;
      ++prefix;
   }

   if (*prefix == 'K')
      precision = 0;

   for (; precision > 0; precision--) {
      powj *= 10;
      if (value / powi < powj)
         break;
   }

   written = snprintf(buffer, size, "%.*f%ci",
      precision, (double) value / powi, *prefix);

   return written;
}

void Meter_delete(Object* cast) {
   if (!cast)
      return;
   Meter* this = (Meter*) cast;
   if (Meter_doneFn(this)) {
      Meter_done(this);
   }
   free(this->drawData);
   free(this->caption);
   free(this->short_caption);
   free(this->values);
   free(this);
}

void Meter_setCaption(Meter* this, const char* caption) {
   free(this->caption);
   this->caption = xStrdup(caption);
}

void Meter_setShortCaption(Meter *this, const char *caption) {
   free(this->short_caption);
   this->short_caption = xStrdup(caption);
}

static inline void Meter_displayBuffer(Meter* this, char* buffer, RichString* out) {
   if (Object_displayFn(this)) {
      Object_display(this, out);
   } else {
      RichString_write(out, CRT_colors[HTOP_METER_TEXT_COLOR], ": ");
      RichString_append(out, CRT_colors[Meter_getAttribute(this, 0)], buffer);
   }
}

void Meter_setMode(Meter* this, int modeIndex) {
   if (modeIndex > 0 && modeIndex == this->mode)
      return;
   if (!modeIndex)
      modeIndex = 1;
   assert(modeIndex < LAST_METERMODE);
   if (Meter_defaultMode(this) == CUSTOM_METERMODE) {
      this->draw = Meter_drawFn(this);
      if (Meter_updateModeFn(this))
         Meter_updateMode(this, modeIndex);
   } else {
      assert(modeIndex >= 1);
      free(this->drawData);
      this->drawData = NULL;

      MeterMode* mode = Meter_modes[modeIndex];
      this->draw = mode->draw;
      this->h = mode->h;
   }
   this->mode = modeIndex;
}

ListItem* Meter_toListItem(Meter* this, bool moving) {
   char mode[20];
   if (this->mode) {
      xSnprintf(mode, sizeof mode, " [%s]", Meter_modes[this->mode]->uiName);
   } else {
      mode[0] = '\0';
   }
   char number[11];
   if (this->param > 0) {
      xSnprintf(number, sizeof number, " %d", this->param);
   } else {
      number[0] = '\0';
   }
   char buffer[50];
   xSnprintf(buffer, sizeof buffer, "%s%s%s", Meter_uiName(this), number, mode);
   ListItem* li = ListItem_new(buffer, HTOP_DEFAULT_COLOR, 0, NULL);
   li->moving = moving;
   return li;
}

/* ---------- TextMeterMode ---------- */

static void TextMeterMode_draw(Meter* this, int x, int y, int w) {
   char buffer[METER_BUFFER_LEN];
   Meter_updateValues(this, buffer, METER_BUFFER_LEN);
   (void) w;

   attrset(CRT_colors[HTOP_METER_TEXT_COLOR]);
   mvaddstr(y, x, this->caption);
   x += strlen(this->caption);
   attrset(CRT_colors[HTOP_DEFAULT_COLOR]);
   RichString_begin(out);
   Meter_displayBuffer(this, buffer, &out);
   RichString_printVal(out, y, x);
   RichString_end(out);
}

/* ---------- BarMeterMode ---------- */

static const char BarMeterMode_characters[] = "|#*@$%&.";

static void BarMeterMode_draw(Meter* this, int x, int y, int w) {
   char buffer[METER_BUFFER_LEN];
   Meter_updateValues(this, buffer, METER_BUFFER_LEN);

   w -= 2;
   attrset(CRT_colors[HTOP_METER_TEXT_COLOR]);
   int captionLen = 3;
   mvaddnstr(y, x, this->short_caption ? this->short_caption : this->caption, captionLen);
   x += captionLen;
   w -= captionLen;
   attrset(CRT_colors[HTOP_BAR_BORDER_COLOR]);
   mvaddch(y, x, '[');
   mvaddch(y, x + w, ']');
   w--;
   x++;

   if (w < 1) {
      attrset(CRT_colors[HTOP_DEFAULT_COLOR]);
      return;
   }

   char bar[w + 1];
   xSnprintf(bar, w + 1, "%*.*s", w, w, buffer);

   // First draw in the bar[] buffer...
   double total = Meter_getMaximum(this);
   int nitems = Meter_getItems(this);
   assert((size_t)nitems < sizeof BarMeterMode_characters);
   int blockSizes[nitems];
   int indexes[nitems];
   if(As_Meter(this)->values_are_overlapped) {
      bool mask[nitems];
      memset(mask, 0, nitems * sizeof(bool));
      for(int i = 0; i < nitems; i++) {
         int cur_min_i = -1;
         for(int j = 0; j < nitems; j++) {
            if(mask[j]) continue;
            if(cur_min_i < 0 || this->values[j] < this->values[cur_min_i]) cur_min_i = j;
         }
         assert(cur_min_i >= 0);
         mask[indexes[i] = cur_min_i] = true;
      }
      int offset = 0;
      for(int j = 0; j < nitems; j++) {
         int i = indexes[j];
         double value = this->values[i];
         value = CLAMP(value, 0, total);
         int block_size = value > 0 ? ceil((value/total) * w) : 0;
         assert(block_size >= offset);
         for(int k = offset; k < block_size; k++) {
            if(bar[k] != ' ') continue;
            bar[k] =
               CRT_color_scheme_is_monochrome[CRT_color_scheme_index] ?
                  BarMeterMode_characters[i] : '|';
         }
         blockSizes[i] = block_size - offset;
         offset = block_size;
      }
   } else for (int i = 0, offset = 0; i < nitems; i++) {
      double value = this->values[i];
      value = CLAMP(value, 0.0, total);
      if (value > 0) {
         blockSizes[i] = ceil((value/total) * w);
      } else {
         blockSizes[i] = 0;
      }
      int nextOffset = offset + blockSizes[i];
      // (Control against invalid values)
      nextOffset = CLAMP(nextOffset, 0, w);
      for (int j = offset; j < nextOffset; j++) {
         if (bar[j] == ' ') {
            bar[j] =
               CRT_color_scheme_is_monochrome[CRT_color_scheme_index] ?
                  BarMeterMode_characters[i] : '|';
         }
      }
      offset = nextOffset;
      indexes[i] = i;
   }

   // ...then print the buffer.
   int offset = 0;
   for (int j = 0; j < nitems; j++) {
      int i = indexes[j];
      attrset(CRT_colors[Meter_getAttribute(this, i)]);
      mvaddnstr(y, x + offset, bar + offset, blockSizes[i]);
      offset += blockSizes[i];
      offset = CLAMP(offset, 0, w);
   }
   if (offset < w) {
      attrset(CRT_colors[HTOP_BAR_SHADOW_COLOR]);
      mvaddnstr(y, x + offset, bar + offset, w - offset);
   }

   move(y, x + w + 1);
   attrset(CRT_colors[HTOP_DEFAULT_COLOR]);
}

/* ---------- GraphMeterMode ---------- */

#ifdef HAVE_LIBNCURSESW

#define PIXPERROW_UTF8 4
static const char* const GraphMeterMode_dotsUtf8[] = {
   /*00*/" ", /*01*/"⢀", /*02*/"⢠", /*03*/"⢰", /*04*/ "⢸",
   /*10*/"⡀", /*11*/"⣀", /*12*/"⣠", /*13*/"⣰", /*14*/ "⣸",
   /*20*/"⡄", /*21*/"⣄", /*22*/"⣤", /*23*/"⣴", /*24*/ "⣼",
   /*30*/"⡆", /*31*/"⣆", /*32*/"⣦", /*33*/"⣶", /*34*/ "⣾",
   /*40*/"⡇", /*41*/"⣇", /*42*/"⣧", /*43*/"⣷", /*44*/ "⣿"
};

#endif

#define PIXPERROW_ASCII 2
static const char* const GraphMeterMode_dotsAscii[] = {
   /*00*/" ", /*01*/".", /*02*/":",
   /*10*/".", /*11*/".", /*12*/":",
   /*20*/":", /*21*/":", /*22*/":"
};

static const char* const* GraphMeterMode_dots;
static int GraphMeterMode_pixPerRow;

static void GraphMeterMode_draw(Meter* this, int x, int y, int w) {

   if (!this->drawData) this->drawData = xCalloc(1, sizeof(GraphData));
   GraphData *data = this->drawData;
   const int nValues = METER_BUFFER_LEN;

#ifdef HAVE_LIBNCURSESW
   if (CRT_utf8) {
      GraphMeterMode_dots = GraphMeterMode_dotsUtf8;
      GraphMeterMode_pixPerRow = PIXPERROW_UTF8;
   } else
#endif
   {
      GraphMeterMode_dots = GraphMeterMode_dotsAscii;
      GraphMeterMode_pixPerRow = PIXPERROW_ASCII;
   }

   attrset(CRT_colors[HTOP_METER_TEXT_COLOR]);
   int captionLen = 3;
   mvaddnstr(y, x, this->short_caption ? this->short_caption : this->caption, captionLen);
   x += captionLen;
   w -= captionLen;
   struct timeval now;
   gettimeofday(&now, NULL);
   if (!timercmp(&now, &(data->time), <)) {
      struct timeval delay = { .tv_sec = CRT_delay/10, .tv_usec = (CRT_delay-((CRT_delay/10)*10)) * 100000 };
#ifndef timeradd
      data->time.tv_sec = now.tv_sec + delay.tv_sec;
      data->time.tv_usec = now.tv_usec + delay.tv_usec;
      if (data->time.tv_usec >= 1000000) {
              data->time.tv_sec++;
              data->time.tv_usec -= 1000000;
      }
#else
      timeradd(&now, &delay, &(data->time));
#endif

      for (int i = 0; i < nValues - 1; i++) data->values[i] = data->values[i+1];
      char buffer[nValues];
      Meter_updateValues(this, buffer, nValues);
      double value = 0.0;
      int nitems = Meter_getItems(this);
      for (int i = 0; i < nitems; i++) value += this->values[i];
      value /= Meter_getMaximum(this);
      data->values[nValues - 1] = value;
   }
   int i = nValues - (w*2) + 2, k = 0;
   if (i < 0) {
      k = -i/2;
      i = 0;
   }
   for (; i < nValues - 1; i+=2, k++) {
      int pix = GraphMeterMode_pixPerRow * GRAPH_HEIGHT;
      int v1 = CLAMP((int) lround(data->values[i] * pix), 1, pix);
      int v2 = CLAMP((int) lround(data->values[i+1] * pix), 1, pix);

      int colorIdx = HTOP_GRAPH_1_COLOR;
      for (int line = 0; line < GRAPH_HEIGHT; line++) {
         int line1 = CLAMP(v1 - (GraphMeterMode_pixPerRow * (GRAPH_HEIGHT - 1 - line)), 0, GraphMeterMode_pixPerRow);
         int line2 = CLAMP(v2 - (GraphMeterMode_pixPerRow * (GRAPH_HEIGHT - 1 - line)), 0, GraphMeterMode_pixPerRow);

         attrset(CRT_colors[colorIdx]);
         mvaddstr(y+line, x+k, GraphMeterMode_dots[line1 * (GraphMeterMode_pixPerRow + 1) + line2]);
         colorIdx = HTOP_GRAPH_2_COLOR;
      }
   }
   attrset(CRT_colors[HTOP_DEFAULT_COLOR]);
}

/* ---------- LEDMeterMode ---------- */

static const char* const LEDMeterMode_digitsAscii[] = {
   " __ ","    "," __ "," __ ","    "," __ "," __ "," __ "," __ "," __ ",
   "|  |","   |"," __|"," __|","|__|","|__ ","|__ ","   |","|__|","|__|",
   "|__|","   |","|__ "," __|","   |"," __|","|__|","   |","|__|"," __|"
};

#ifdef HAVE_LIBNCURSESW

static const char* const LEDMeterMode_digitsUtf8[] = {
   "┌──┐","  ┐ ","╶──┐","╶──┐","╷  ╷","┌──╴","┌──╴","╶──┐","┌──┐","┌──┐",
   "│  │","  │ ","┌──┘"," ──┤","└──┤","└──┐","├──┐","   │","├──┤","└──┤",
   "└──┘","  ╵ ","└──╴","╶──┘","   ╵","╶──┘","└──┘","   ╵","└──┘"," ──┘"
};

#endif

static const char* const* LEDMeterMode_digits;

static void LEDMeterMode_drawDigit(int x, int y, int n) {
   for (int i = 0; i < 3; i++)
      mvaddstr(y+i, x, LEDMeterMode_digits[i * 10 + n]);
}

static void LEDMeterMode_draw(Meter* this, int x, int y, int w) {
   (void) w;

#ifdef HAVE_LIBNCURSESW
   if (CRT_utf8)
      LEDMeterMode_digits = LEDMeterMode_digitsUtf8;
   else
#endif
      LEDMeterMode_digits = LEDMeterMode_digitsAscii;

   char buffer[METER_BUFFER_LEN];
   Meter_updateValues(this, buffer, METER_BUFFER_LEN);
   RichString_begin(out);
   Meter_displayBuffer(this, buffer, &out);

   int yText =
#ifdef HAVE_LIBNCURSESW
      CRT_utf8 ? y+1 :
#endif
      y+2;
   attrset(CRT_colors[HTOP_LED_COLOR]);
   mvaddstr(yText, x, this->caption);
   int xx = x + strlen(this->caption);
   int len = RichString_sizeVal(out);
   for (int i = 0; i < len; i++) {
      char c = RichString_getCharVal(out, i);
      if (c >= '0' && c <= '9') {
         LEDMeterMode_drawDigit(xx, y, c-48);
         xx += 4;
      } else {
         mvaddch(yText, xx, c);
         xx += 1;
      }
   }
   attrset(CRT_colors[HTOP_DEFAULT_COLOR]);
   RichString_end(out);
}

static MeterMode BarMeterMode = {
   .uiName = "Bar",
   .h = 1,
   .draw = BarMeterMode_draw,
};

static MeterMode TextMeterMode = {
   .uiName = "Text",
   .h = 1,
   .draw = TextMeterMode_draw,
};

static MeterMode GraphMeterMode = {
   .uiName = "Graph",
   .h = GRAPH_HEIGHT,
   .draw = GraphMeterMode_draw,
};

static MeterMode LEDMeterMode = {
   .uiName = "LED",
   .h = 3,
   .draw = LEDMeterMode_draw,
};

MeterMode* Meter_modes[] = {
   NULL,
   &BarMeterMode,
   &TextMeterMode,
   &GraphMeterMode,
   &LEDMeterMode,
   NULL
};

/* Blank meter */

static void BlankMeter_updateValues(Meter* this, char* buffer, int size) {
	*buffer = 0;
}

static void BlankMeter_display(Object* cast, RichString* out) {
   (void) cast;
   RichString_prune(out);
}

static const int BlankMeter_attributes[] = {
   HTOP_DEFAULT_COLOR
};

MeterClass BlankMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = BlankMeter_display,
   },
   .updateValues = BlankMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 0,
   .total = 100.0,
   .attributes = BlankMeter_attributes,
   .name = "Blank",
   .uiName = "Blank",
   .caption = ""
};
