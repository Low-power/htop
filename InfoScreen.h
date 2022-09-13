/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_InfoScreen
#define HEADER_InfoScreen
/*
htop - InfoScreen.h
(C) 2004-2018 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "Panel.h"
#include "FunctionBar.h"
#include "IncSet.h"
#include "Vector.h"
#include "Settings.h"

#undef lines

typedef struct InfoScreen_ InfoScreen;

typedef void(*InfoScreen_Scan)(InfoScreen*);
typedef void(*InfoScreen_Draw)(InfoScreen*);
typedef void(*InfoScreen_OnErr)(InfoScreen*);
typedef bool(*InfoScreen_OnKey)(InfoScreen*, int);

typedef struct InfoScreenClass_ {
   ObjectClass super;
   InfoScreen_Scan scan;
   InfoScreen_Draw draw;
   InfoScreen_OnErr onErr;
   InfoScreen_OnKey onKey;
} InfoScreenClass;

#define As_InfoScreen(this_)          ((InfoScreenClass*)(((InfoScreen*)(this_))->super.klass))
#define InfoScreen_scan(this_)        As_InfoScreen(this_)->scan((InfoScreen*)(this_))
#define InfoScreen_draw(this_)        As_InfoScreen(this_)->draw((InfoScreen*)(this_))
#define InfoScreen_onErr(this_)       As_InfoScreen(this_)->onErr((InfoScreen*)(this_))
#define InfoScreen_onKey(this_, ch_)  As_InfoScreen(this_)->onKey((InfoScreen*)(this_), (ch_))

struct InfoScreen_ {
   Object super;
   const Process *process;
   Panel* display;
   FunctionBar* bar;
   IncSet* inc;
   Vector* lines;
   const Settings *settings;
};

#if defined ERR && ERR > 0
#undef ERR
#define ERR (-1)
#endif

InfoScreen* InfoScreen_init(InfoScreen* this, const Process *process, FunctionBar* bar, int height, char* panelHeader);

InfoScreen* InfoScreen_done(InfoScreen* this);

void InfoScreen_drawTitled(InfoScreen* this, char* fmt, ...);

void InfoScreen_addLine(InfoScreen* this, const char* line, unsigned int color_index);

void InfoScreen_appendLine(InfoScreen* this, const char* line);

void InfoScreen_run(InfoScreen* this);

#endif
