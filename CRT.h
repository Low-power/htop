/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_CRT
#define HEADER_CRT
/*
htop - CRT.h
(C) 2004-2011 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Settings.h"
#include <stdbool.h>

typedef enum TreeStr_ {
   TREE_STR_HORZ,
   TREE_STR_VERT,
   TREE_STR_RTEE,
   TREE_STR_BEND,
   TREE_STR_TEND,
   TREE_STR_OPEN,
   TREE_STR_SHUT,
   TREE_STR_COUNT
} TreeStr;

typedef enum {
   DEFAULT_COLOR_SCHEME = 0,
   MONOCHROME_COLOR_SCHEME = 1,
   BLACKONWHITE_COLOR_SCHEME = 2,
   LIGHTTERMINAL_COLOR_SCHEME = 3,
   MIDNIGHT_COLOR_SCHEME = 4,
   BLACKNIGHT_COLOR_SCHEME = 5,
   BROKENGRAY_COLOR_SCHEME = 6,
   LAST_COLORSCHEME = 7,
} ColorScheme;

typedef enum {
   HTOP_DEFAULT_COLOR,
   HTOP_FUNCTION_BAR_COLOR,
   HTOP_FUNCTION_KEY_COLOR,
   HTOP_FAILED_SEARCH_COLOR,
   HTOP_PANEL_HEADER_FOCUS_COLOR,
   HTOP_PANEL_HEADER_UNFOCUS_COLOR,
   HTOP_PANEL_SELECTION_FOCUS_COLOR,
   HTOP_PANEL_SELECTION_FOLLOW_COLOR,
   HTOP_PANEL_SELECTION_UNFOCUS_COLOR,
   HTOP_LARGE_NUMBER_COLOR,
   HTOP_METER_TEXT_COLOR,
   HTOP_METER_VALUE_COLOR,
   HTOP_LED_COLOR,
   HTOP_UPTIME_COLOR,
   HTOP_BATTERY_COLOR,
   HTOP_TASKS_RUNNING_COLOR,
   HTOP_SWAP_COLOR,
   HTOP_PROCESS_COLOR,
   HTOP_PROCESS_SHADOW_COLOR,
   HTOP_PROCESS_CREATED_COLOR,
   HTOP_PROCESS_TAG_COLOR,
   HTOP_PROCESS_MEGABYTES_COLOR,
   HTOP_PROCESS_TREE_COLOR,
   HTOP_PROCESS_R_STATE_COLOR,
   HTOP_PROCESS_D_STATE_COLOR,
   HTOP_PROCESS_Z_STATE_COLOR,
   HTOP_PROCESS_BASENAME_COLOR,
   HTOP_PROCESS_HIGH_PRIORITY_COLOR,
   HTOP_PROCESS_LOW_PRIORITY_COLOR,
   HTOP_PROCESS_KERNEL_PROCESS_COLOR,
   HTOP_PROCESS_THREAD_COLOR,
   HTOP_PROCESS_THREAD_BASENAME_COLOR,
   HTOP_BAR_BORDER_COLOR,
   HTOP_BAR_SHADOW_COLOR,
   HTOP_GRAPH_1_COLOR,
   HTOP_GRAPH_2_COLOR,
   HTOP_MEMORY_USED_COLOR,
   HTOP_MEMORY_BUFFERS_COLOR,
   HTOP_MEMORY_BUFFERS_TEXT_COLOR,
   HTOP_MEMORY_CACHE_COLOR,
   HTOP_MEMORY_ZFS_ARC_COLOR,
   HTOP_LOAD_COLOR,
   HTOP_LOAD_AVERAGE_FIFTEEN_COLOR,
   HTOP_LOAD_AVERAGE_FIVE_COLOR,
   HTOP_LOAD_AVERAGE_ONE_COLOR,
   HTOP_CHECK_BOX_COLOR,
   HTOP_CHECK_MARK_COLOR,
   HTOP_CHECK_TEXT_COLOR,
   HTOP_CLOCK_COLOR,
   HTOP_HELP_BOLD_COLOR,
   HTOP_HOSTNAME_COLOR,
   HTOP_CPU_NICE_COLOR,
   HTOP_CPU_NICE_TEXT_COLOR,
   HTOP_CPU_NORMAL_COLOR,
   HTOP_CPU_KERNEL_COLOR,
   HTOP_CPU_IOWAIT_COLOR,
   HTOP_CPU_IRQ_COLOR,
   HTOP_CPU_SOFTIRQ_COLOR,
   HTOP_CPU_STEAL_COLOR,
   HTOP_CPU_GUEST_COLOR,
   LAST_COLORELEMENT
} ColorElement;

#define KEY_ALT(x) (KEY_F(64 - 26) + ((x) - 'A'))


#ifndef __ANDROID__
#endif
#ifdef HAVE_BACKTRACE
#endif

#if defined ERR && ERR > 0
#undef ERR
#define ERR (-1)
#endif

#define ONE_BINARY_K 1024L
#define ONE_BINARY_M (ONE_BINARY_K * ONE_BINARY_K)
#define ONE_BINARY_G (ONE_BINARY_M * ONE_BINARY_K)

#define ONE_DECIMAL_K 1000L
#define ONE_DECIMAL_M (ONE_DECIMAL_K * ONE_DECIMAL_K)
#define ONE_DECIMAL_G (ONE_DECIMAL_M * ONE_DECIMAL_K)

#define ColorIndex(i,j) ((7-(i))*8+(j))

#define ColorPair(i,j) COLOR_PAIR(ColorIndex((i),(j)))

#define Black COLOR_BLACK
#define Red COLOR_RED
#define Green COLOR_GREEN
#define Yellow COLOR_YELLOW
#define Blue COLOR_BLUE
#define Magenta COLOR_MAGENTA
#define Cyan COLOR_CYAN
#define White COLOR_WHITE

#define ColorPairGrayBlack ColorPair(Magenta,Magenta)
#define ColorIndexGrayBlack ColorIndex(Magenta,Magenta)

#define KEY_WHEELUP KEY_F(20)
#define KEY_WHEELDOWN KEY_F(21)
#define KEY_RECLICK KEY_F(22)

//#link curses

extern const char *CRT_treeStrAscii[TREE_STR_COUNT];

#ifdef HAVE_LIBNCURSESW

extern const char *CRT_treeStrUtf8[TREE_STR_COUNT];

extern bool CRT_utf8;

#endif

extern const char **CRT_treeStr;

extern int CRT_delay;

extern int* CRT_colors;

extern int CRT_color_scheme_count;

extern const char **CRT_color_scheme_names;

extern bool *CRT_color_scheme_is_monochrome;

extern int CRT_colorSchemes[LAST_COLORSCHEME][LAST_COLORELEMENT];

extern int (*CRT_user_defined_color_schemes)[LAST_COLORELEMENT];

extern int CRT_cursorX;

extern int CRT_scrollHAmount;

extern int CRT_scrollWheelVAmount;

extern char* CRT_termType;

// TODO move color scheme to Settings, perhaps?

extern int CRT_color_scheme_index;

void CRT_setMouse(bool enabled);

#if HAVE_SETUID_ENABLED

#define DIE(msg) do { CRT_done(); fputs((msg), stderr); exit(1); } while(0)

void CRT_dropPrivileges();

void CRT_restorePrivileges();

#else

/* Turn setuid operations into NOPs */

#ifndef CRT_dropPrivileges
#define CRT_dropPrivileges()
#define CRT_restorePrivileges()
#endif

#endif

extern unsigned int CRT_page_size;
extern unsigned int CRT_page_size_kib;

char *CRT_getConfigDirPath(const char **home_path_p);

#ifndef A_HORIZONTAL
#define A_HORIZONTAL 0
#endif
#ifndef A_LEFT
#define A_LEFT 0
#endif
#ifndef A_LOW
#define A_LOW 0
#endif
#ifndef A_RIGHT
#define A_RIGHT 0
#endif
#ifndef A_TOP
#define A_TOP 0
#endif
#ifndef A_VERTICAL
#define A_VERTICAL 0
#endif

void CRT_initColorSchemes();

int CRT_getDefaultColorScheme();

int CRT_getColorSchemeIndexForName(const char *name);

void CRT_init(const Settings *settings);

void CRT_done();

void __attribute__((__noreturn__)) CRT_fatalError(const char* note);

void CRT_setExplicitDelay(bool enabled);

// Wait a key forever
int CRT_readKey();

void CRT_disableDelay();

void CRT_enableDelay();

void CRT_explicitDelay();

void CRT_setColors(int color_scheme_i);

#endif
