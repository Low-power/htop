/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_Meter
#define HEADER_Meter
/*
htop - Meter.h
(C) 2004-2011 Hisham H. Muhammad
Copyright 2015-2024 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

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
#endif

extern MeterClass Meter_class;

Meter* Meter_new(ProcessList *pl, int param, MeterClass* type);

int Meter_humanUnit(char* buffer, unsigned long int value, int size);

void Meter_delete(Object* cast);

void Meter_setCaption(Meter* this, const char* caption);

void Meter_setShortCaption(Meter *this, const char *caption);

void Meter_setMode(Meter* this, int modeIndex);

ListItem* Meter_toListItem(Meter* this, bool moving);

/* ---------- TextMeterMode ---------- */

/* ---------- BarMeterMode ---------- */

/* ---------- GraphMeterMode ---------- */

#ifdef HAVE_LIBNCURSESW

#define PIXPERROW_UTF8 4
#endif

#define PIXPERROW_ASCII 2
/* ---------- LEDMeterMode ---------- */

#ifdef HAVE_LIBNCURSESW

#endif

extern MeterMode* Meter_modes[];

/* Blank meter */

extern MeterClass BlankMeter_class;

#endif
