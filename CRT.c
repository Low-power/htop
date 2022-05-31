/*
htop - CRT.c
(C) 2004-2011 Hisham H. Muhammad
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
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

}*/

#include "config.h"
#include "CRT.h"
#include "StringUtils.h"
#include "RichString.h"
#include "XAlloc.h"
#include "local-curses.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#ifndef __ANDROID__
#include <langinfo.h>
#endif
#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#endif
#include <pwd.h>
#include <dirent.h>
#include <ctype.h>

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

const char *CRT_treeStrAscii[TREE_STR_COUNT] = {
   "-", // TREE_STR_HORZ
   "|", // TREE_STR_VERT
   "|", // TREE_STR_RTEE
   "`", // TREE_STR_BEND
   ",", // TREE_STR_TEND
   "+", // TREE_STR_OPEN
   "-", // TREE_STR_SHUT
};

#ifdef HAVE_LIBNCURSESW

const char *CRT_treeStrUtf8[TREE_STR_COUNT] = {
   "\xe2\x94\x80", // TREE_STR_HORZ ─
   "\xe2\x94\x82", // TREE_STR_VERT │
   "\xe2\x94\x9c", // TREE_STR_RTEE ├
   "\xe2\x94\x94", // TREE_STR_BEND └
   "\xe2\x94\x8c", // TREE_STR_TEND ┌
   "+",            // TREE_STR_OPEN +
   "\xe2\x94\x80", // TREE_STR_SHUT ─
};

bool CRT_utf8 = false;

#endif

const char **CRT_treeStr = CRT_treeStrAscii;

int CRT_delay = 0;

int* CRT_colors;

int CRT_color_scheme_count = LAST_COLORSCHEME;

const char **CRT_color_scheme_names;

bool *CRT_color_scheme_is_monochrome;

int CRT_colorSchemes[LAST_COLORSCHEME][LAST_COLORELEMENT] = {
   [DEFAULT_COLOR_SCHEME] = {
      [HTOP_DEFAULT_COLOR] = ColorPair(White,Black),
      [HTOP_FUNCTION_BAR_COLOR] = ColorPair(Black,Cyan),
      [HTOP_FUNCTION_KEY_COLOR] = ColorPair(White,Black),
      [HTOP_PANEL_HEADER_FOCUS_COLOR] = ColorPair(Black,Green),
      [HTOP_PANEL_HEADER_UNFOCUS_COLOR] = ColorPair(Black,Green),
      [HTOP_PANEL_SELECTION_FOCUS_COLOR] = ColorPair(Black,Cyan),
      [HTOP_PANEL_SELECTION_FOLLOW_COLOR] = ColorPair(Black,Yellow),
      [HTOP_PANEL_SELECTION_UNFOCUS_COLOR] = ColorPair(Black,White),
      [HTOP_FAILED_SEARCH_COLOR] = ColorPair(Red,Cyan),
      [HTOP_UPTIME_COLOR] = A_BOLD | ColorPair(Cyan,Black),
      [HTOP_BATTERY_COLOR] = A_BOLD | ColorPair(Cyan,Black),
      [HTOP_LARGE_NUMBER_COLOR] = A_BOLD | ColorPair(Red,Black),
      [HTOP_METER_TEXT_COLOR] = ColorPair(Cyan,Black),
      [HTOP_METER_VALUE_COLOR] = A_BOLD | ColorPair(Cyan,Black),
      [HTOP_LED_COLOR] = ColorPair(Green,Black),
      [HTOP_TASKS_RUNNING_COLOR] = A_BOLD | ColorPair(Green,Black),
      [HTOP_PROCESS_COLOR] = A_NORMAL,
      [HTOP_PROCESS_SHADOW_COLOR] = A_BOLD | ColorPairGrayBlack,
      [HTOP_PROCESS_CREATED_COLOR] = ColorPair(Black,Green),
      [HTOP_PROCESS_TAG_COLOR] = A_BOLD | ColorPair(Yellow,Black),
      [HTOP_PROCESS_MEGABYTES_COLOR] = ColorPair(Cyan,Black),
      [HTOP_PROCESS_BASENAME_COLOR] = A_BOLD | ColorPair(Cyan,Black),
      [HTOP_PROCESS_TREE_COLOR] = ColorPair(Cyan,Black),
      [HTOP_PROCESS_R_STATE_COLOR] = A_BOLD | ColorPair(Green,Black),
      [HTOP_PROCESS_D_STATE_COLOR] = A_BOLD | ColorPair(Red,Black),
      [HTOP_PROCESS_Z_STATE_COLOR] = A_BOLD | ColorPairGrayBlack,
      [HTOP_PROCESS_HIGH_PRIORITY_COLOR] = ColorPair(Red,Black),
      [HTOP_PROCESS_LOW_PRIORITY_COLOR] = ColorPair(Green,Black),
      [HTOP_PROCESS_KERNEL_PROCESS_COLOR] = ColorPair(Yellow,Black),
      [HTOP_PROCESS_THREAD_COLOR] = ColorPair(Green,Black),
      [HTOP_PROCESS_THREAD_BASENAME_COLOR] = A_BOLD | ColorPair(Green,Black),
      [HTOP_BAR_BORDER_COLOR] = A_BOLD,
      [HTOP_BAR_SHADOW_COLOR] = A_BOLD | ColorPairGrayBlack,
      [HTOP_SWAP_COLOR] = ColorPair(Red,Black),
      [HTOP_GRAPH_1_COLOR] = A_BOLD | ColorPair(Cyan,Black),
      [HTOP_GRAPH_2_COLOR] = ColorPair(Cyan,Black),
      [HTOP_MEMORY_USED_COLOR] = ColorPair(Green,Black),
      [HTOP_MEMORY_BUFFERS_COLOR] = ColorPair(Blue,Black),
      [HTOP_MEMORY_BUFFERS_TEXT_COLOR] = A_BOLD | ColorPair(Blue,Black),
      [HTOP_MEMORY_CACHE_COLOR] = ColorPair(Yellow,Black),
      [HTOP_MEMORY_ZFS_ARC_COLOR] = ColorPair(Magenta,Black),
      [HTOP_LOAD_AVERAGE_FIFTEEN_COLOR] = ColorPair(Cyan,Black),
      [HTOP_LOAD_AVERAGE_FIVE_COLOR] = A_BOLD | ColorPair(Cyan,Black),
      [HTOP_LOAD_AVERAGE_ONE_COLOR] = A_BOLD | ColorPair(White,Black),
      [HTOP_LOAD_COLOR] = A_BOLD,
      [HTOP_HELP_BOLD_COLOR] = A_BOLD | ColorPair(Cyan,Black),
      [HTOP_CLOCK_COLOR] = A_BOLD,
      [HTOP_CHECK_BOX_COLOR] = ColorPair(Cyan,Black),
      [HTOP_CHECK_MARK_COLOR] = A_BOLD,
      [HTOP_CHECK_TEXT_COLOR] = A_NORMAL,
      [HTOP_HOSTNAME_COLOR] = A_BOLD,
      [HTOP_CPU_NICE_COLOR] = ColorPair(Blue,Black),
      [HTOP_CPU_NICE_TEXT_COLOR] = A_BOLD | ColorPair(Blue,Black),
      [HTOP_CPU_NORMAL_COLOR] = ColorPair(Green,Black),
      [HTOP_CPU_KERNEL_COLOR] = ColorPair(Red,Black),
      [HTOP_CPU_IOWAIT_COLOR] = A_BOLD | ColorPairGrayBlack,
      [HTOP_CPU_IRQ_COLOR] = ColorPair(Yellow,Black),
      [HTOP_CPU_SOFTIRQ_COLOR] = ColorPair(Magenta,Black),
      [HTOP_CPU_STEAL_COLOR] = ColorPair(Cyan,Black),
      [HTOP_CPU_GUEST_COLOR] = ColorPair(Cyan,Black),
   },
   [MONOCHROME_COLOR_SCHEME] = {
      [HTOP_DEFAULT_COLOR] = A_NORMAL,
      [HTOP_FUNCTION_BAR_COLOR] = A_REVERSE,
      [HTOP_FUNCTION_KEY_COLOR] = A_NORMAL,
      [HTOP_PANEL_HEADER_FOCUS_COLOR] = A_REVERSE,
      [HTOP_PANEL_HEADER_UNFOCUS_COLOR] = A_REVERSE,
      [HTOP_PANEL_SELECTION_FOCUS_COLOR] = A_REVERSE,
      [HTOP_PANEL_SELECTION_FOLLOW_COLOR] = A_REVERSE,
      [HTOP_PANEL_SELECTION_UNFOCUS_COLOR] = A_BOLD,
      [HTOP_FAILED_SEARCH_COLOR] = A_REVERSE | A_BOLD,
      [HTOP_UPTIME_COLOR] = A_BOLD,
      [HTOP_BATTERY_COLOR] = A_BOLD,
      [HTOP_LARGE_NUMBER_COLOR] = A_BOLD,
      [HTOP_METER_TEXT_COLOR] = A_NORMAL,
      [HTOP_METER_VALUE_COLOR] = A_BOLD,
      [HTOP_LED_COLOR] = A_NORMAL,
      [HTOP_TASKS_RUNNING_COLOR] = A_BOLD,
      [HTOP_PROCESS_COLOR] = A_NORMAL,
      [HTOP_PROCESS_SHADOW_COLOR] = A_DIM,
      [HTOP_PROCESS_CREATED_COLOR] = A_REVERSE,
      [HTOP_PROCESS_TAG_COLOR] = A_BOLD,
      [HTOP_PROCESS_MEGABYTES_COLOR] = A_BOLD,
      [HTOP_PROCESS_BASENAME_COLOR] = A_BOLD,
      [HTOP_PROCESS_TREE_COLOR] = A_BOLD,
      [HTOP_PROCESS_R_STATE_COLOR] = A_BOLD,
      [HTOP_PROCESS_D_STATE_COLOR] = A_BOLD,
      [HTOP_PROCESS_Z_STATE_COLOR] = A_DIM,
      [HTOP_PROCESS_HIGH_PRIORITY_COLOR] = A_BOLD,
      [HTOP_PROCESS_LOW_PRIORITY_COLOR] = A_DIM,
      [HTOP_PROCESS_KERNEL_PROCESS_COLOR] = A_BOLD,
      [HTOP_PROCESS_THREAD_COLOR] = A_BOLD,
      [HTOP_PROCESS_THREAD_BASENAME_COLOR] = A_REVERSE,
      [HTOP_BAR_BORDER_COLOR] = A_BOLD,
      [HTOP_BAR_SHADOW_COLOR] = A_DIM,
      [HTOP_SWAP_COLOR] = A_BOLD,
      [HTOP_GRAPH_1_COLOR] = A_BOLD,
      [HTOP_GRAPH_2_COLOR] = A_NORMAL,
      [HTOP_MEMORY_USED_COLOR] = A_BOLD,
      [HTOP_MEMORY_BUFFERS_COLOR] = A_NORMAL,
      [HTOP_MEMORY_BUFFERS_TEXT_COLOR] = A_NORMAL,
      [HTOP_MEMORY_CACHE_COLOR] = A_NORMAL,
      [HTOP_MEMORY_ZFS_ARC_COLOR] = A_NORMAL,
      [HTOP_LOAD_AVERAGE_FIFTEEN_COLOR] = A_DIM,
      [HTOP_LOAD_AVERAGE_FIVE_COLOR] = A_NORMAL,
      [HTOP_LOAD_AVERAGE_ONE_COLOR] = A_BOLD,
      [HTOP_LOAD_COLOR] = A_BOLD,
      [HTOP_HELP_BOLD_COLOR] = A_BOLD,
      [HTOP_CLOCK_COLOR] = A_BOLD,
      [HTOP_CHECK_BOX_COLOR] = A_BOLD,
      [HTOP_CHECK_MARK_COLOR] = A_NORMAL,
      [HTOP_CHECK_TEXT_COLOR] = A_NORMAL,
      [HTOP_HOSTNAME_COLOR] = A_BOLD,
      [HTOP_CPU_NICE_COLOR] = A_NORMAL,
      [HTOP_CPU_NICE_TEXT_COLOR] = A_NORMAL,
      [HTOP_CPU_NORMAL_COLOR] = A_BOLD,
      [HTOP_CPU_KERNEL_COLOR] = A_BOLD,
      [HTOP_CPU_IOWAIT_COLOR] = A_NORMAL,
      [HTOP_CPU_IRQ_COLOR] = A_BOLD,
      [HTOP_CPU_SOFTIRQ_COLOR] = A_BOLD,
      [HTOP_CPU_STEAL_COLOR] = A_REVERSE,
      [HTOP_CPU_GUEST_COLOR] = A_REVERSE,
   },
   [BLACKONWHITE_COLOR_SCHEME] = {
      [HTOP_DEFAULT_COLOR] = ColorPair(Black,White),
      [HTOP_FUNCTION_BAR_COLOR] = ColorPair(Black,Cyan),
      [HTOP_FUNCTION_KEY_COLOR] = ColorPair(Black,White),
      [HTOP_PANEL_HEADER_FOCUS_COLOR] = ColorPair(Black,Green),
      [HTOP_PANEL_HEADER_UNFOCUS_COLOR] = ColorPair(Black,Green),
      [HTOP_PANEL_SELECTION_FOCUS_COLOR] = ColorPair(Black,Cyan),
      [HTOP_PANEL_SELECTION_FOLLOW_COLOR] = ColorPair(Black,Yellow),
      [HTOP_PANEL_SELECTION_UNFOCUS_COLOR] = ColorPair(Blue,White),
      [HTOP_FAILED_SEARCH_COLOR] = ColorPair(Red,Cyan),
      [HTOP_UPTIME_COLOR] = ColorPair(Yellow,White),
      [HTOP_BATTERY_COLOR] = ColorPair(Yellow,White),
      [HTOP_LARGE_NUMBER_COLOR] = ColorPair(Red,White),
      [HTOP_METER_TEXT_COLOR] = ColorPair(Blue,White),
      [HTOP_METER_VALUE_COLOR] = ColorPair(Black,White),
      [HTOP_LED_COLOR] = ColorPair(Green,White),
      [HTOP_TASKS_RUNNING_COLOR] = ColorPair(Green,White),
      [HTOP_PROCESS_COLOR] = ColorPair(Black,White),
      [HTOP_PROCESS_SHADOW_COLOR] = A_BOLD | ColorPair(Black,White),
      [HTOP_PROCESS_CREATED_COLOR] = A_BOLD | ColorPair(Green,White),
      [HTOP_PROCESS_TAG_COLOR] = ColorPair(White,Blue),
      [HTOP_PROCESS_MEGABYTES_COLOR] = ColorPair(Blue,White),
      [HTOP_PROCESS_BASENAME_COLOR] = ColorPair(Blue,White),
      [HTOP_PROCESS_TREE_COLOR] = ColorPair(Green,White),
      [HTOP_PROCESS_R_STATE_COLOR] = ColorPair(Green,White),
      [HTOP_PROCESS_D_STATE_COLOR] = A_BOLD | ColorPair(Red,White),
      [HTOP_PROCESS_Z_STATE_COLOR] = A_BOLD | ColorPair(Black,White),
      [HTOP_PROCESS_HIGH_PRIORITY_COLOR] = ColorPair(Red,White),
      [HTOP_PROCESS_LOW_PRIORITY_COLOR] = ColorPair(Green,White),
      [HTOP_PROCESS_KERNEL_PROCESS_COLOR] = ColorPair(Magenta,White),
      [HTOP_PROCESS_THREAD_COLOR] = ColorPair(Blue,White),
      [HTOP_PROCESS_THREAD_BASENAME_COLOR] = A_BOLD | ColorPair(Blue,White),
      [HTOP_BAR_BORDER_COLOR] = ColorPair(Blue,White),
      [HTOP_BAR_SHADOW_COLOR] = ColorPair(Black,White),
      [HTOP_SWAP_COLOR] = ColorPair(Red,White),
      [HTOP_GRAPH_1_COLOR] = A_BOLD | ColorPair(Blue,White),
      [HTOP_GRAPH_2_COLOR] = ColorPair(Blue,White),
      [HTOP_MEMORY_USED_COLOR] = ColorPair(Green,White),
      [HTOP_MEMORY_BUFFERS_COLOR] = ColorPair(Cyan,White),
      [HTOP_MEMORY_BUFFERS_TEXT_COLOR] = ColorPair(Cyan,White),
      [HTOP_MEMORY_CACHE_COLOR] = ColorPair(Yellow,White),
      [HTOP_MEMORY_ZFS_ARC_COLOR] = ColorPair(Magenta,White),
      [HTOP_LOAD_AVERAGE_FIFTEEN_COLOR] = ColorPair(Black,White),
      [HTOP_LOAD_AVERAGE_FIVE_COLOR] = ColorPair(Black,White),
      [HTOP_LOAD_AVERAGE_ONE_COLOR] = ColorPair(Black,White),
      [HTOP_LOAD_COLOR] = ColorPair(Black,White),
      [HTOP_HELP_BOLD_COLOR] = ColorPair(Blue,White),
      [HTOP_CLOCK_COLOR] = ColorPair(Black,White),
      [HTOP_CHECK_BOX_COLOR] = ColorPair(Blue,White),
      [HTOP_CHECK_MARK_COLOR] = ColorPair(Black,White),
      [HTOP_CHECK_TEXT_COLOR] = ColorPair(Black,White),
      [HTOP_HOSTNAME_COLOR] = ColorPair(Black,White),
      [HTOP_CPU_NICE_COLOR] = ColorPair(Cyan,White),
      [HTOP_CPU_NICE_TEXT_COLOR] = ColorPair(Cyan,White),
      [HTOP_CPU_NORMAL_COLOR] = ColorPair(Green,White),
      [HTOP_CPU_KERNEL_COLOR] = ColorPair(Red,White),
      [HTOP_CPU_IOWAIT_COLOR] = A_BOLD | ColorPair(Black, White),
      [HTOP_CPU_IRQ_COLOR] = ColorPair(Blue,White),
      [HTOP_CPU_SOFTIRQ_COLOR] = ColorPair(Blue,White),
      [HTOP_CPU_STEAL_COLOR] = ColorPair(Cyan,White),
      [HTOP_CPU_GUEST_COLOR] = ColorPair(Cyan,White),
   },
   [LIGHTTERMINAL_COLOR_SCHEME] = {
      [HTOP_DEFAULT_COLOR] = ColorPair(Black,Black),
      [HTOP_FUNCTION_BAR_COLOR] = ColorPair(Black,Cyan),
      [HTOP_FUNCTION_KEY_COLOR] = ColorPair(Black,Black),
      [HTOP_PANEL_HEADER_FOCUS_COLOR] = ColorPair(Black,Green),
      [HTOP_PANEL_HEADER_UNFOCUS_COLOR] = ColorPair(Black,Green),
      [HTOP_PANEL_SELECTION_FOCUS_COLOR] = ColorPair(Black,Cyan),
      [HTOP_PANEL_SELECTION_FOLLOW_COLOR] = ColorPair(Black,Yellow),
      [HTOP_PANEL_SELECTION_UNFOCUS_COLOR] = ColorPair(Blue,Black),
      [HTOP_FAILED_SEARCH_COLOR] = ColorPair(Red,Cyan),
      [HTOP_UPTIME_COLOR] = ColorPair(Yellow,Black),
      [HTOP_BATTERY_COLOR] = ColorPair(Yellow,Black),
      [HTOP_LARGE_NUMBER_COLOR] = ColorPair(Red,Black),
      [HTOP_METER_TEXT_COLOR] = ColorPair(Blue,Black),
      [HTOP_METER_VALUE_COLOR] = ColorPair(Black,Black),
      [HTOP_LED_COLOR] = ColorPair(Green,Black),
      [HTOP_TASKS_RUNNING_COLOR] = ColorPair(Green,Black),
      [HTOP_PROCESS_COLOR] = ColorPair(Black,Black),
      [HTOP_PROCESS_SHADOW_COLOR] = A_BOLD | ColorPairGrayBlack,
      [HTOP_PROCESS_CREATED_COLOR] = ColorPair(Black,Green),
      [HTOP_PROCESS_TAG_COLOR] = ColorPair(White,Blue),
      [HTOP_PROCESS_MEGABYTES_COLOR] = ColorPair(Blue,Black),
      [HTOP_PROCESS_BASENAME_COLOR] = ColorPair(Green,Black),
      [HTOP_PROCESS_TREE_COLOR] = ColorPair(Blue,Black),
      [HTOP_PROCESS_R_STATE_COLOR] = ColorPair(Green,Black),
      [HTOP_PROCESS_D_STATE_COLOR] = A_BOLD | ColorPair(Red,Black),
      [HTOP_PROCESS_Z_STATE_COLOR] = A_BOLD | ColorPairGrayBlack,
      [HTOP_PROCESS_HIGH_PRIORITY_COLOR] = ColorPair(Red,Black),
      [HTOP_PROCESS_LOW_PRIORITY_COLOR] = ColorPair(Green,Black),
      [HTOP_PROCESS_KERNEL_PROCESS_COLOR] = ColorPair(Magenta,Black),
      [HTOP_PROCESS_THREAD_COLOR] = ColorPair(Blue,Black),
      [HTOP_PROCESS_THREAD_BASENAME_COLOR] = A_BOLD | ColorPair(Blue,Black),
      [HTOP_BAR_BORDER_COLOR] = ColorPair(Blue,Black),
      [HTOP_BAR_SHADOW_COLOR] = ColorPairGrayBlack,
      [HTOP_SWAP_COLOR] = ColorPair(Red,Black),
      [HTOP_GRAPH_1_COLOR] = A_BOLD | ColorPair(Cyan,Black),
      [HTOP_GRAPH_2_COLOR] = ColorPair(Cyan,Black),
      [HTOP_MEMORY_USED_COLOR] = ColorPair(Green,Black),
      [HTOP_MEMORY_BUFFERS_COLOR] = ColorPair(Cyan,Black),
      [HTOP_MEMORY_BUFFERS_TEXT_COLOR] = ColorPair(Cyan,Black),
      [HTOP_MEMORY_CACHE_COLOR] = ColorPair(Yellow,Black),
      [HTOP_MEMORY_ZFS_ARC_COLOR] = ColorPair(Magenta,Black),
      [HTOP_LOAD_AVERAGE_FIFTEEN_COLOR] = ColorPair(Black,Black),
      [HTOP_LOAD_AVERAGE_FIVE_COLOR] = ColorPair(Black,Black),
      [HTOP_LOAD_AVERAGE_ONE_COLOR] = ColorPair(Black,Black),
      [HTOP_LOAD_COLOR] = ColorPair(White,Black),
      [HTOP_HELP_BOLD_COLOR] = ColorPair(Blue,Black),
      [HTOP_CLOCK_COLOR] = ColorPair(White,Black),
      [HTOP_CHECK_BOX_COLOR] = ColorPair(Blue,Black),
      [HTOP_CHECK_MARK_COLOR] = ColorPair(Black,Black),
      [HTOP_CHECK_TEXT_COLOR] = ColorPair(Black,Black),
      [HTOP_HOSTNAME_COLOR] = ColorPair(White,Black),
      [HTOP_CPU_NICE_COLOR] = ColorPair(Cyan,Black),
      [HTOP_CPU_NICE_TEXT_COLOR] = ColorPair(Cyan,Black),
      [HTOP_CPU_NORMAL_COLOR] = ColorPair(Green,Black),
      [HTOP_CPU_KERNEL_COLOR] = ColorPair(Red,Black),
      [HTOP_CPU_IOWAIT_COLOR] = A_BOLD | ColorPair(Black, Black),
      [HTOP_CPU_IRQ_COLOR] = A_BOLD | ColorPair(Blue,Black),
      [HTOP_CPU_SOFTIRQ_COLOR] = ColorPair(Blue,Black),
      [HTOP_CPU_STEAL_COLOR] = ColorPair(Black,Black),
      [HTOP_CPU_GUEST_COLOR] = ColorPair(Black,Black),
   },
   [MIDNIGHT_COLOR_SCHEME] = {
      [HTOP_DEFAULT_COLOR] = ColorPair(White,Blue),
      [HTOP_FUNCTION_BAR_COLOR] = ColorPair(Black,Cyan),
      [HTOP_FUNCTION_KEY_COLOR] = A_NORMAL,
      [HTOP_PANEL_HEADER_FOCUS_COLOR] = ColorPair(Black,Cyan),
      [HTOP_PANEL_HEADER_UNFOCUS_COLOR] = ColorPair(Black,Cyan),
      [HTOP_PANEL_SELECTION_FOCUS_COLOR] = ColorPair(Black,White),
      [HTOP_PANEL_SELECTION_FOLLOW_COLOR] = ColorPair(Black,Yellow),
      [HTOP_PANEL_SELECTION_UNFOCUS_COLOR] = A_BOLD | ColorPair(Yellow,Blue),
      [HTOP_FAILED_SEARCH_COLOR] = ColorPair(Red,Cyan),
      [HTOP_UPTIME_COLOR] = A_BOLD | ColorPair(Yellow,Blue),
      [HTOP_BATTERY_COLOR] = A_BOLD | ColorPair(Yellow,Blue),
      [HTOP_LARGE_NUMBER_COLOR] = A_BOLD | ColorPair(Red,Blue),
      [HTOP_METER_TEXT_COLOR] = ColorPair(Cyan,Blue),
      [HTOP_METER_VALUE_COLOR] = A_BOLD | ColorPair(Cyan,Blue),
      [HTOP_LED_COLOR] = ColorPair(Green,Blue),
      [HTOP_TASKS_RUNNING_COLOR] = A_BOLD | ColorPair(Green,Blue),
      [HTOP_PROCESS_COLOR] = ColorPair(White,Blue),
      [HTOP_PROCESS_SHADOW_COLOR] = A_BOLD | ColorPair(Black,Blue),
      [HTOP_PROCESS_CREATED_COLOR] = ColorPair(Blue,Green),
      [HTOP_PROCESS_TAG_COLOR] = A_BOLD | ColorPair(Yellow,Blue),
      [HTOP_PROCESS_MEGABYTES_COLOR] = ColorPair(Cyan,Blue),
      [HTOP_PROCESS_BASENAME_COLOR] = A_BOLD | ColorPair(Cyan,Blue),
      [HTOP_PROCESS_TREE_COLOR] = ColorPair(Cyan,Blue),
      [HTOP_PROCESS_R_STATE_COLOR] = A_BOLD | ColorPair(Green,Blue),
      [HTOP_PROCESS_D_STATE_COLOR] = A_BOLD | ColorPair(Red,Blue),
      [HTOP_PROCESS_Z_STATE_COLOR] = A_BOLD | ColorPair(Black,Blue),
      [HTOP_PROCESS_HIGH_PRIORITY_COLOR] = ColorPair(Red,Blue),
      [HTOP_PROCESS_LOW_PRIORITY_COLOR] = ColorPair(Green,Blue),
      [HTOP_PROCESS_KERNEL_PROCESS_COLOR] = ColorPair(Yellow,Blue),
      [HTOP_PROCESS_THREAD_COLOR] = ColorPair(Green,Blue),
      [HTOP_PROCESS_THREAD_BASENAME_COLOR] = A_BOLD | ColorPair(Green,Blue),
      [HTOP_BAR_BORDER_COLOR] = A_BOLD | ColorPair(Yellow,Blue),
      [HTOP_BAR_SHADOW_COLOR] = ColorPair(Cyan,Blue),
      [HTOP_SWAP_COLOR] = ColorPair(Red,Blue),
      [HTOP_GRAPH_1_COLOR] = A_BOLD | ColorPair(Cyan,Blue),
      [HTOP_GRAPH_2_COLOR] = ColorPair(Cyan,Blue),
      [HTOP_MEMORY_USED_COLOR] = A_BOLD | ColorPair(Green,Blue),
      [HTOP_MEMORY_BUFFERS_COLOR] = A_BOLD | ColorPair(Cyan,Blue),
      [HTOP_MEMORY_BUFFERS_TEXT_COLOR] = A_BOLD | ColorPair(Cyan,Blue),
      [HTOP_MEMORY_CACHE_COLOR] = A_BOLD | ColorPair(Yellow,Blue),
      [HTOP_MEMORY_ZFS_ARC_COLOR] = ColorPair(Magenta,Blue),
      [HTOP_LOAD_AVERAGE_FIFTEEN_COLOR] = A_BOLD | ColorPair(Black,Blue),
      [HTOP_LOAD_AVERAGE_FIVE_COLOR] = A_NORMAL | ColorPair(White,Blue),
      [HTOP_LOAD_AVERAGE_ONE_COLOR] = A_BOLD | ColorPair(White,Blue),
      [HTOP_LOAD_COLOR] = A_BOLD | ColorPair(White,Blue),
      [HTOP_HELP_BOLD_COLOR] = A_BOLD | ColorPair(Cyan,Blue),
      [HTOP_CLOCK_COLOR] = ColorPair(White,Blue),
      [HTOP_CHECK_BOX_COLOR] = ColorPair(Cyan,Blue),
      [HTOP_CHECK_MARK_COLOR] = A_BOLD | ColorPair(White,Blue),
      [HTOP_CHECK_TEXT_COLOR] = A_NORMAL | ColorPair(White,Blue),
      [HTOP_HOSTNAME_COLOR] = ColorPair(White,Blue),
      [HTOP_CPU_NICE_COLOR] = A_BOLD | ColorPair(Cyan,Blue),
      [HTOP_CPU_NICE_TEXT_COLOR] = A_BOLD | ColorPair(Cyan,Blue),
      [HTOP_CPU_NORMAL_COLOR] = A_BOLD | ColorPair(Green,Blue),
      [HTOP_CPU_KERNEL_COLOR] = A_BOLD | ColorPair(Red,Blue),
      [HTOP_CPU_IOWAIT_COLOR] = A_BOLD | ColorPair(Blue,Blue),
      [HTOP_CPU_IRQ_COLOR] = A_BOLD | ColorPair(Black,Blue),
      [HTOP_CPU_SOFTIRQ_COLOR] = ColorPair(Black,Blue),
      [HTOP_CPU_STEAL_COLOR] = ColorPair(White,Blue),
      [HTOP_CPU_GUEST_COLOR] = ColorPair(White,Blue),
   },
   [BLACKNIGHT_COLOR_SCHEME] = {
      [HTOP_DEFAULT_COLOR] = ColorPair(Cyan,Black),
      [HTOP_FUNCTION_BAR_COLOR] = ColorPair(Black,Green),
      [HTOP_FUNCTION_KEY_COLOR] = ColorPair(Cyan,Black),
      [HTOP_PANEL_HEADER_FOCUS_COLOR] = ColorPair(Black,Green),
      [HTOP_PANEL_HEADER_UNFOCUS_COLOR] = ColorPair(Black,Green),
      [HTOP_PANEL_SELECTION_FOCUS_COLOR] = ColorPair(Black,Cyan),
      [HTOP_PANEL_SELECTION_FOLLOW_COLOR] = ColorPair(Black,Yellow),
      [HTOP_PANEL_SELECTION_UNFOCUS_COLOR] = ColorPair(Black,White),
      [HTOP_FAILED_SEARCH_COLOR] = ColorPair(Red,Cyan),
      [HTOP_UPTIME_COLOR] = ColorPair(Green,Black),
      [HTOP_BATTERY_COLOR] = ColorPair(Green,Black),
      [HTOP_LARGE_NUMBER_COLOR] = A_BOLD | ColorPair(Red,Black),
      [HTOP_METER_TEXT_COLOR] = ColorPair(Cyan,Black),
      [HTOP_METER_VALUE_COLOR] = ColorPair(Green,Black),
      [HTOP_LED_COLOR] = ColorPair(Green,Black),
      [HTOP_TASKS_RUNNING_COLOR] = A_BOLD | ColorPair(Green,Black),
      [HTOP_PROCESS_COLOR] = ColorPair(Cyan,Black),
      [HTOP_PROCESS_SHADOW_COLOR] = A_BOLD | ColorPairGrayBlack,
      [HTOP_PROCESS_CREATED_COLOR] = ColorPair(Black,Green),
      [HTOP_PROCESS_TAG_COLOR] = A_BOLD | ColorPair(Yellow,Black),
      [HTOP_PROCESS_MEGABYTES_COLOR] = A_BOLD | ColorPair(Green,Black),
      [HTOP_PROCESS_BASENAME_COLOR] = A_BOLD | ColorPair(Green,Black),
      [HTOP_PROCESS_TREE_COLOR] = ColorPair(Cyan,Black),
      [HTOP_PROCESS_KERNEL_PROCESS_COLOR] = ColorPair(Red,Black),
      [HTOP_PROCESS_THREAD_COLOR] = ColorPair(Green,Black),
      [HTOP_PROCESS_THREAD_BASENAME_COLOR] = A_BOLD | ColorPair(Blue,Black),
      [HTOP_PROCESS_R_STATE_COLOR] = A_BOLD | ColorPair(Green,Black),
      [HTOP_PROCESS_D_STATE_COLOR] = A_BOLD | ColorPair(Red,Black),
      [HTOP_PROCESS_Z_STATE_COLOR] = A_BOLD | ColorPairGrayBlack,
      [HTOP_PROCESS_HIGH_PRIORITY_COLOR] = ColorPair(Red,Black),
      [HTOP_PROCESS_LOW_PRIORITY_COLOR] = ColorPair(Green,Black),
      [HTOP_BAR_BORDER_COLOR] = A_BOLD | ColorPair(Green,Black),
      [HTOP_BAR_SHADOW_COLOR] = ColorPair(Cyan,Black),
      [HTOP_SWAP_COLOR] = ColorPair(Red,Black),
      [HTOP_GRAPH_1_COLOR] = A_BOLD | ColorPair(Green,Black),
      [HTOP_GRAPH_2_COLOR] = ColorPair(Green,Black),
      [HTOP_MEMORY_USED_COLOR] = ColorPair(Green,Black),
      [HTOP_MEMORY_BUFFERS_COLOR] = ColorPair(Blue,Black),
      [HTOP_MEMORY_BUFFERS_TEXT_COLOR] = A_BOLD | ColorPair(Blue,Black),
      [HTOP_MEMORY_CACHE_COLOR] = ColorPair(Yellow,Black),
      [HTOP_MEMORY_ZFS_ARC_COLOR] = ColorPair(Magenta,Black),
      [HTOP_LOAD_AVERAGE_FIFTEEN_COLOR] = ColorPair(Green,Black),
      [HTOP_LOAD_AVERAGE_FIVE_COLOR] = ColorPair(Green,Black),
      [HTOP_LOAD_AVERAGE_ONE_COLOR] = A_BOLD | ColorPair(Green,Black),
      [HTOP_LOAD_COLOR] = A_BOLD,
      [HTOP_HELP_BOLD_COLOR] = A_BOLD | ColorPair(Cyan,Black),
      [HTOP_CLOCK_COLOR] = ColorPair(Green,Black),
      [HTOP_CHECK_BOX_COLOR] = ColorPair(Green,Black),
      [HTOP_CHECK_MARK_COLOR] = A_BOLD | ColorPair(Green,Black),
      [HTOP_CHECK_TEXT_COLOR] = ColorPair(Cyan,Black),
      [HTOP_HOSTNAME_COLOR] = ColorPair(Green,Black),
      [HTOP_CPU_NICE_COLOR] = ColorPair(Blue,Black),
      [HTOP_CPU_NICE_TEXT_COLOR] = A_BOLD | ColorPair(Blue,Black),
      [HTOP_CPU_NORMAL_COLOR] = ColorPair(Green,Black),
      [HTOP_CPU_KERNEL_COLOR] = ColorPair(Red,Black),
      [HTOP_CPU_IOWAIT_COLOR] = ColorPair(Yellow,Black),
      [HTOP_CPU_IRQ_COLOR] = A_BOLD | ColorPair(Blue,Black),
      [HTOP_CPU_SOFTIRQ_COLOR] = ColorPair(Blue,Black),
      [HTOP_CPU_STEAL_COLOR] = ColorPair(Cyan,Black),
      [HTOP_CPU_GUEST_COLOR] = ColorPair(Cyan,Black),
   },
   [BROKENGRAY_COLOR_SCHEME] = { 0 } // dynamically generated.
};

int (*CRT_user_defined_color_schemes)[LAST_COLORELEMENT];

int CRT_cursorX = 0;

int CRT_scrollHAmount = 5;

int CRT_scrollWheelVAmount = 10;

char* CRT_termType;

// TODO move color scheme to Settings, perhaps?

int CRT_color_scheme_index;

void CRT_setMouse(bool enabled) {
	mmask_t mask = enabled ?
#if NCURSES_MOUSE_VERSION > 1
		BUTTON1_RELEASED | BUTTON4_PRESSED | BUTTON5_PRESSED
#else
		BUTTON1_RELEASED
#endif
		: 0;
	mousemask(mask, NULL);
}

static void CRT_handleAbnormalSignal(int sgn) {
   CRT_done();
   fprintf(stderr, "\n\nhtop " VERSION " aborting due to signal %d.\n", sgn);
   #ifdef BUG_REPORTING_URL
   if(*BUG_REPORTING_URL) fprintf(stderr, "Please report bug at " BUG_REPORTING_URL "\n");
   #endif
   #ifdef HAVE_BACKTRACE
   static void *backtrace_buffer[128];
   size_t size = backtrace(backtrace_buffer, sizeof backtrace_buffer / sizeof(void *));
   fprintf(stderr, "\nPlease include in your report the following backtrace: \n");
   backtrace_symbols_fd(backtrace_buffer, size, 2);
   fprintf(stderr, "\nAdditionally, in order to make the above backtrace useful,");
   fprintf(stderr, "\nplease also run the following command to generate a disassembly of your binary:");
   fprintf(stderr, "\n\n   objdump -d `which htop` > ~/htop.objdump");
   fprintf(stderr, "\n\nand then attach the file ~/htop.objdump to your bug report.");
   fprintf(stderr, "\n\nThank you for helping to improve htop!\n");
   #endif
   fputc('\n', stderr);
   if(sgn == SIGABRT) signal(SIGABRT, SIG_DFL);
   abort();
}

static void CRT_handleSIGTERM(int sgn) {
   (void) sgn;
   CRT_done();
   exit(0);
}

#if HAVE_SETUID_ENABLED

static int CRT_euid = -1;

static int CRT_egid = -1;

#define DIE(msg) do { CRT_done(); fputs((msg), stderr); exit(1); } while(0)

void CRT_dropPrivileges() {
   CRT_egid = getegid();
   CRT_euid = geteuid();
   if (setegid(getgid()) == -1) {
      DIE("Fatal error: failed dropping group privileges.\n");
   }
   if (seteuid(getuid()) == -1) {
      DIE("Fatal error: failed dropping user privileges.\n");
   }
}

void CRT_restorePrivileges() {
   if (CRT_egid == -1 || CRT_euid == -1) {
      DIE("Fatal error: internal inconsistency.\n");
   }
   if (setegid(CRT_egid) == -1) {
      DIE("Fatal error: failed restoring group privileges.\n");
   }
   if (seteuid(CRT_euid) == -1) {
      DIE("Fatal error: failed restoring user privileges.\n");
   }
}

#else

/* Turn setuid operations into NOPs */

#ifndef CRT_dropPrivileges
#define CRT_dropPrivileges()
#define CRT_restorePrivileges()
#endif

#endif

unsigned int CRT_page_size;
unsigned int CRT_page_size_kib;

static const char *CRT_getHomePath() {
	const char *home = getenv("HOME");
	if(!home) {
		const struct passwd *pw = getpwuid(getuid());
		home = pw ? pw->pw_dir : "";
	}
	return home;
}

char *CRT_getConfigDirPath(const char **home_path_p) {
   CRT_dropPrivileges();
   if(home_path_p) *home_path_p = CRT_getHomePath();
   char *config_dir_path;
   const char* xdgConfigHome = getenv("XDG_CONFIG_HOME");
   if (xdgConfigHome) {
      if(access(xdgConfigHome, F_OK) < 0) mkdir(xdgConfigHome, 0700);
      config_dir_path = String_cat(xdgConfigHome, "/htop/");
   } else {
      const char *home = home_path_p ? *home_path_p : CRT_getHomePath();
      size_t home_len = strlen(home);
      config_dir_path = xMalloc(home_len + 15);
      memcpy(config_dir_path, home, home_len);
      strcpy(config_dir_path + home_len, "/.config/");
      if(access(config_dir_path, F_OK) < 0) mkdir(config_dir_path, 0700);
      strcpy(config_dir_path + home_len + 9, "htop/");
   }
   if(access(config_dir_path, F_OK) < 0) mkdir(config_dir_path, 0700);
   CRT_restorePrivileges();
   return config_dir_path;
}

static bool parse_boolean_value(const char *s, bool *v) {
	switch(*s) {
		case 0:
		case '\n':
			*v = false;
			return true;
		case '0':
		case '1':
			if(s[1] && s[1] != '\n') return false;
			*v = *s == '1';
			return true;
		case 'y':
		case 'Y':
			*v = true;
			return true;
		case 'n':
		case 'N':
			*v = false;
			return true;
		case 't':
		case 'T':
			if(strncasecmp(s, "true", 4)) return false;
			*v = true;
			return true;
		case 'f':
		case 'F':
			if(strncasecmp(s, "false", 5)) return false;
			*v = false;
			return true;
		case 'o':
		case 'O':
			if((s[1] == 'f' || s[1] == 'F') && (s[2] == 'f' || s[2] == 'F') && (!s[3] || s[3] == '\n')) {
				*v = false;
				return true;
			}
			if((s[1] == 'n' || s[1] == 'N') && (!s[2] || s[2] == '\n')) {
				*v = true;
				return true;
			}
			// Fallthrough
		default:
			return false;
	}
}

static const struct key_name {
	const char *key_prefix;
	unsigned int key_length;
} color_element_name_map[] = {
#define COLOR_ELEMENT_NAME(KEY) [HTOP_##KEY##_COLOR] = { #KEY "=", sizeof #KEY }
	COLOR_ELEMENT_NAME(DEFAULT),
	COLOR_ELEMENT_NAME(FUNCTION_BAR),
	COLOR_ELEMENT_NAME(FUNCTION_KEY),
	COLOR_ELEMENT_NAME(FAILED_SEARCH),
	COLOR_ELEMENT_NAME(PANEL_HEADER_FOCUS),
	COLOR_ELEMENT_NAME(PANEL_HEADER_UNFOCUS),
	COLOR_ELEMENT_NAME(PANEL_SELECTION_FOCUS),
	COLOR_ELEMENT_NAME(PANEL_SELECTION_FOLLOW),
	COLOR_ELEMENT_NAME(PANEL_SELECTION_UNFOCUS),
	COLOR_ELEMENT_NAME(LARGE_NUMBER),
	COLOR_ELEMENT_NAME(METER_TEXT),
	COLOR_ELEMENT_NAME(METER_VALUE),
	COLOR_ELEMENT_NAME(LED),
	COLOR_ELEMENT_NAME(UPTIME),
	COLOR_ELEMENT_NAME(BATTERY),
	COLOR_ELEMENT_NAME(TASKS_RUNNING),
	COLOR_ELEMENT_NAME(SWAP),
	COLOR_ELEMENT_NAME(PROCESS),
	COLOR_ELEMENT_NAME(PROCESS_SHADOW),
	COLOR_ELEMENT_NAME(PROCESS_CREATED),
	COLOR_ELEMENT_NAME(PROCESS_TAG),
	COLOR_ELEMENT_NAME(PROCESS_MEGABYTES),
	COLOR_ELEMENT_NAME(PROCESS_TREE),
	COLOR_ELEMENT_NAME(PROCESS_R_STATE),
	COLOR_ELEMENT_NAME(PROCESS_D_STATE),
	COLOR_ELEMENT_NAME(PROCESS_Z_STATE),
	COLOR_ELEMENT_NAME(PROCESS_BASENAME),
	COLOR_ELEMENT_NAME(PROCESS_HIGH_PRIORITY),
	COLOR_ELEMENT_NAME(PROCESS_LOW_PRIORITY),
	COLOR_ELEMENT_NAME(PROCESS_KERNEL_PROCESS),
	COLOR_ELEMENT_NAME(PROCESS_THREAD),
	COLOR_ELEMENT_NAME(PROCESS_THREAD_BASENAME),
	COLOR_ELEMENT_NAME(BAR_BORDER),
	COLOR_ELEMENT_NAME(BAR_SHADOW),
	COLOR_ELEMENT_NAME(GRAPH_1),
	COLOR_ELEMENT_NAME(GRAPH_2),
	COLOR_ELEMENT_NAME(MEMORY_USED),
	COLOR_ELEMENT_NAME(MEMORY_BUFFERS),
	COLOR_ELEMENT_NAME(MEMORY_BUFFERS_TEXT),
	COLOR_ELEMENT_NAME(MEMORY_CACHE),
	COLOR_ELEMENT_NAME(MEMORY_ZFS_ARC),
	COLOR_ELEMENT_NAME(LOAD),
	COLOR_ELEMENT_NAME(LOAD_AVERAGE_FIFTEEN),
	COLOR_ELEMENT_NAME(LOAD_AVERAGE_FIVE),
	COLOR_ELEMENT_NAME(LOAD_AVERAGE_ONE),
	COLOR_ELEMENT_NAME(CHECK_BOX),
	COLOR_ELEMENT_NAME(CHECK_MARK),
	COLOR_ELEMENT_NAME(CHECK_TEXT),
	COLOR_ELEMENT_NAME(CLOCK),
	COLOR_ELEMENT_NAME(HELP_BOLD),
	COLOR_ELEMENT_NAME(HOSTNAME),
	COLOR_ELEMENT_NAME(CPU_NICE),
	COLOR_ELEMENT_NAME(CPU_NICE_TEXT),
	COLOR_ELEMENT_NAME(CPU_NORMAL),
	COLOR_ELEMENT_NAME(CPU_KERNEL),
	COLOR_ELEMENT_NAME(CPU_IOWAIT),
	COLOR_ELEMENT_NAME(CPU_IRQ),
	COLOR_ELEMENT_NAME(CPU_SOFTIRQ),
	COLOR_ELEMENT_NAME(CPU_STEAL),
	COLOR_ELEMENT_NAME(CPU_GUEST),
#undef COLOR_ELEMENT_NAME
};

static int get_color_by_name(const char *name, unsigned int len) {
	switch(len) {
		case 3:
			if(memcmp(name, "RED", 3) == 0) return Red;
			break;
		case 4:
			if(memcmp(name, "BLUE", 4) == 0) return Blue;
			if(memcmp(name, "CYAN", 4) == 0) return Cyan;
			break;
		case 5:
			if(memcmp(name, "BLACK", 5) == 0) return Black;
			if(memcmp(name, "GREEN", 5) == 0) return Green;
			if(memcmp(name, "WHITE", 5) == 0) return White;
			break;
		case 6:
			if(memcmp(name, "YELLOW", 6) == 0) return Yellow;
			break;
		case 7:
			if(memcmp(name, "MAGENTA", 7) == 0) return Magenta;
			break;
	}
	return -1;
}

static int get_attribute_by_name(const char *name, unsigned int len) {
#define CHECK_ATTRIBUTE_NAME(A) if(len == sizeof #A - 1 && memcmp(name, #A, sizeof #A - 1) == 0) return A_##A
	CHECK_ATTRIBUTE_NAME(STANDOUT);
	CHECK_ATTRIBUTE_NAME(UNDERLINE);
	CHECK_ATTRIBUTE_NAME(REVERSE);
	CHECK_ATTRIBUTE_NAME(BLINK);
	CHECK_ATTRIBUTE_NAME(DIM);
	CHECK_ATTRIBUTE_NAME(BOLD);
	CHECK_ATTRIBUTE_NAME(ALTCHARSET);
	CHECK_ATTRIBUTE_NAME(INVIS);
	CHECK_ATTRIBUTE_NAME(PROTECT);
	CHECK_ATTRIBUTE_NAME(HORIZONTAL);
	CHECK_ATTRIBUTE_NAME(LEFT);
	CHECK_ATTRIBUTE_NAME(LOW);
	CHECK_ATTRIBUTE_NAME(RIGHT);
	CHECK_ATTRIBUTE_NAME(TOP);
	CHECK_ATTRIBUTE_NAME(VERTICAL);
	return -1;
#undef CHECK_ATTRIBUTE_NAME
}

static int parse_attribute(const char *s) {
	const char *sep = strchr(s, ',');
	if(!sep) return -1;
	int foreground = get_color_by_name(s, sep - s);
	if(foreground < 0) return -1;
	s = sep + 1;
	unsigned int len = 0;
	while(s[len] && s[len] != '\n' && s[len] != ',') len++;
	if(!len) return -1;
	int background = get_color_by_name(s, len);
	if(background < 0) return -1;
	int attributes = ColorPair(foreground, background);
	while(s[len] == ',') {
		s += len + 1;
		len = 0;
		while(s[len] && s[len] != '\n' && s[len] != ',') len++;
		int attr = get_attribute_by_name(s, len);
		if(attr < 0) return -1;
		attributes |= attr;
	}
	return attributes;
}

static void load_user_defined_color_scheme(const char *path) {
	unsigned int i;
	FILE *f = fopen(path, "r");
	if(!f) {
		perror(path);
		return;
	}
	char *name = NULL;
	bool monochrome = false;
	//int *colors = xCalloc(LAST_COLORELEMENT, sizeof(int));
	int colors[LAST_COLORELEMENT];
	for(i = 0; i < LAST_COLORELEMENT; i++) colors[i] = A_NORMAL;
	char buffer[128];
	unsigned int nlines = 0;
	while(fgets(buffer, sizeof buffer, f)) {
		nlines++;
		char *p = buffer;
		while(isspace((unsigned char)*p)) p++;
		if(!*p) continue;
		if(*p == '#') continue;
		if(strncmp(p, "NAME=", 5) == 0) {
			if(name) {
				free(name);
				fprintf(stderr, "%s:%u: duplicated NAME\r\n", path, nlines);
				return;
			}
			if(!p[5]) {
				fprintf(stderr, "%s:%u: NAME is empty\r\n", path, nlines);
				return;
			}
			size_t len = strlen(p + 5);
			if(p[5 + len - 1] == '\n') len--;
			name = xMalloc(len + 1);
			memcpy(name, p + 5, len);
			name[len] = 0;
			if(CRT_getColorSchemeIndexForName(name) >= 0) {
				fprintf(stderr, "%s: color scheme '%s' already exist\r\n",
					path, name);
				free(name);
				return;
			}
		} else if(strncmp(p, "MONOCHROME=", 11) == 0) {
			if(!parse_boolean_value(p + 11, &monochrome)) {
				fprintf(stderr, "%s:%u: Warning: incorrect value for MONOCHROME",
					path, nlines);
			}
		} else {
			int attr = -1;
			for(i = 0; i < LAST_COLORELEMENT; i++) {
				const struct key_name *key = color_element_name_map + i;
				if(strncmp(p, key->key_prefix, key->key_length)) continue;
				attr = parse_attribute(p + key->key_length);
				if(attr < 0) {
					p[key->key_length - 1] = 0;
					fprintf(stderr, "%s:%u: Warning: failed to parse attribute for %s\r\n",
						path, nlines, p);
					attr = 0;
				} else {
					colors[i] = attr;
				}
				break;
			}
			if(attr < 0) {
				fprintf(stderr, "%s:%u: Warning: unrecognized key\r\n", path, nlines);
			}
		}
	}
	fclose(f);
	if(!name) {
		fprintf(stderr, "%s: NAME is not defined\r\n", path);
		return;
	}
	CRT_color_scheme_names =
		xRealloc(CRT_color_scheme_names, (CRT_color_scheme_count + 1) * sizeof(const char *));
	CRT_color_scheme_names[CRT_color_scheme_count] = name;
	CRT_color_scheme_is_monochrome =
		xRealloc(CRT_color_scheme_is_monochrome, (CRT_color_scheme_count + 1) * sizeof(bool));
	CRT_color_scheme_is_monochrome[CRT_color_scheme_count] = monochrome;
	i = CRT_color_scheme_count++ - LAST_COLORSCHEME;
	CRT_user_defined_color_schemes =
		xRealloc(CRT_user_defined_color_schemes, (i + 1) * sizeof colors);
	memcpy(CRT_user_defined_color_schemes + i, colors, sizeof colors);
}

void CRT_initColorSchemes() {
   initscr();

   assert(CRT_color_scheme_names == NULL);

   CRT_color_scheme_names = xMalloc(LAST_COLORSCHEME * sizeof(const char *));
   CRT_color_scheme_names[DEFAULT_COLOR_SCHEME] = "Default",
   CRT_color_scheme_names[MONOCHROME_COLOR_SCHEME] = "Monochromatic",
   CRT_color_scheme_names[BLACKONWHITE_COLOR_SCHEME] = "Black on White",
   CRT_color_scheme_names[LIGHTTERMINAL_COLOR_SCHEME] = "Light Terminal",
   CRT_color_scheme_names[MIDNIGHT_COLOR_SCHEME] = "MC",
   CRT_color_scheme_names[BLACKNIGHT_COLOR_SCHEME] = "Black Night",
   CRT_color_scheme_names[BROKENGRAY_COLOR_SCHEME] = "Broken Gray",

   CRT_color_scheme_is_monochrome = xMalloc(LAST_COLORSCHEME * sizeof(bool));
   CRT_color_scheme_is_monochrome[DEFAULT_COLOR_SCHEME] = false;
   CRT_color_scheme_is_monochrome[MONOCHROME_COLOR_SCHEME] = true;
   CRT_color_scheme_is_monochrome[BLACKONWHITE_COLOR_SCHEME] = false;
   CRT_color_scheme_is_monochrome[LIGHTTERMINAL_COLOR_SCHEME] = false;
   CRT_color_scheme_is_monochrome[MIDNIGHT_COLOR_SCHEME] = false;
   CRT_color_scheme_is_monochrome[BLACKNIGHT_COLOR_SCHEME] = false;
   CRT_color_scheme_is_monochrome[BROKENGRAY_COLOR_SCHEME] = false;

   assert(CRT_user_defined_color_schemes == NULL);
   char *config_dir_path = CRT_getConfigDirPath(NULL);
   DIR *config_dir = opendir(config_dir_path);
   if(config_dir) {
      struct dirent *e;
      while((e = readdir(config_dir))) {
         if(*e->d_name == '.') continue;
         size_t len = strlen(e->d_name);
         if(len < 13) continue;
         if(strncmp(e->d_name, "htoprc.", 7) == 0) continue;
         if(strcmp(e->d_name + (len - 12), ".colorscheme")) continue;
         char *path = String_cat(config_dir_path, e->d_name);
         struct stat st;
         if(stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            load_user_defined_color_scheme(path);
         }
         free(path);
      }
      closedir(config_dir);
   }
   free(config_dir_path);

   for (int i = 0; i < LAST_COLORELEMENT; i++) {
      unsigned int color = CRT_colorSchemes[DEFAULT_COLOR_SCHEME][i];
      CRT_colorSchemes[BROKENGRAY_COLOR_SCHEME][i] =
         color == (A_BOLD | ColorPairGrayBlack) ? ColorPair(White,Black) : color;
   }
}

int CRT_getDefaultColorScheme() {
	return has_colors() ? DEFAULT_COLOR_SCHEME : MONOCHROME_COLOR_SCHEME;
}

int CRT_getColorSchemeIndexForName(const char *name) {
   for(int i = 0; i < CRT_color_scheme_count; i++) {
      if(strcmp(name, CRT_color_scheme_names[i]) == 0) return i;
   }
   return -1;
}

void CRT_init(const Settings *settings) {
   noecho();
   CRT_delay = settings->delay;
   if (CRT_delay == 0) {
      CRT_delay = 1;
   }

   halfdelay(CRT_delay);
   nonl();
   intrflush(stdscr, false);
   keypad(stdscr, true);
   mouseinterval(0);
   curs_set(0);
   if(has_colors()) start_color();
   CRT_termType = getenv("TERM");
   CRT_scrollHAmount = String_eq(CRT_termType, "linux") ? 20 : 5;
   if (String_startsWith(CRT_termType, "xterm") || String_eq(CRT_termType, "vt220")) {
      define_key("\033[H", KEY_HOME);
      define_key("\033[F", KEY_END);
      define_key("\033[7~", KEY_HOME);
      define_key("\033[8~", KEY_END);
      define_key("\033OP", KEY_F(1));
      define_key("\033OQ", KEY_F(2));
      define_key("\033OR", KEY_F(3));
      define_key("\033OS", KEY_F(4));
      define_key("\033[11~", KEY_F(1));
      define_key("\033[12~", KEY_F(2));
      define_key("\033[13~", KEY_F(3));
      define_key("\033[14~", KEY_F(4));
      define_key("\033[17;2~", KEY_F(18));
      char sequence[3] = { 0x1b };
      for (char c = 'a'; c <= 'z'; c++) {
         sequence[1] = c;
         define_key(sequence, KEY_ALT('A' + (c - 'a')));
      }
   }
#ifdef HAVE_SET_ESCDELAY
   set_escdelay(25);
#endif

#ifndef DEBUG
   signal(SIGILL, CRT_handleAbnormalSignal);
   signal(SIGBUS, CRT_handleAbnormalSignal);
   signal(SIGSEGV, CRT_handleAbnormalSignal);
#endif
   signal(SIGTERM, CRT_handleSIGTERM);
   signal(SIGQUIT, CRT_handleSIGTERM);
   use_default_colors();
   CRT_setColors(settings->colorScheme);

   /* initialize locale */
   setlocale(LC_CTYPE, "");

#ifdef HAVE_LIBNCURSESW
   CRT_utf8 = strcmp(nl_langinfo(CODESET), "UTF-8") == 0;
#endif

   CRT_treeStr =
#ifdef HAVE_LIBNCURSESW
      CRT_utf8 ? CRT_treeStrUtf8 :
#endif
      CRT_treeStrAscii;

   CRT_page_size = sysconf(_SC_PAGESIZE);
   CRT_page_size_kib = CRT_page_size / ONE_BINARY_K;
}

void CRT_done() {
   curs_set(1);
   endwin();
}

void __attribute__((__noreturn__)) CRT_fatalError(const char* note) {
   char* sysMsg = strerror(errno);
   CRT_done();
   fprintf(stderr, "%s: %s\n", note, sysMsg);
   exit(2);
}

static bool explicit_delay_mode = false;

void CRT_setExplicitDelay(bool enabled) {
	explicit_delay_mode = enabled;
	if(enabled) {
		nocbreak();
		cbreak();
		nodelay(stdscr, true);
	} // else call CRT_enableDelay afterward
}

// Wait a key forever
int CRT_readKey() {
   nocbreak();
   cbreak();
   nodelay(stdscr, FALSE);
   int k = getch();
   if(explicit_delay_mode) nodelay(stdscr, true);
   else halfdelay(CRT_delay);
   return k;
}

void CRT_disableDelay() {
   if(explicit_delay_mode) return;
   nocbreak();
   cbreak();
   nodelay(stdscr, TRUE);
}

void CRT_enableDelay() {
   if(explicit_delay_mode) return;
   halfdelay(CRT_delay);
}

void CRT_explicitDelay() {
	if(!explicit_delay_mode) return;
	fd_set rfdset;
	FD_ZERO(&rfdset);
	FD_SET(STDIN_FILENO, &rfdset);
	struct timeval timeout = { .tv_sec = CRT_delay / 10, .tv_usec = CRT_delay % 10 * 100000 };
	select(STDIN_FILENO + 1, &rfdset, NULL, NULL, &timeout);
}

void CRT_setColors(int color_scheme_i) {
   CRT_color_scheme_index = color_scheme_i;

   for (int i = 0; i < 8; i++) {
      for (int j = 0; j < 8; j++) {
         if (ColorIndex(i,j) != ColorPairGrayBlack) {
            int bg = (color_scheme_i != BLACKNIGHT_COLOR_SCHEME && j == 0) ? -1 : j;
            init_pair(ColorIndex(i, j), i, bg);
         }
      }
   }

   int grayBlackFg = COLORS > 8 ? 8 : 0;
   int grayBlackBg = (color_scheme_i != BLACKNIGHT_COLOR_SCHEME) ? -1 : 0;
   init_pair(ColorIndexGrayBlack, grayBlackFg, grayBlackBg);

   CRT_colors = color_scheme_i < LAST_COLORSCHEME ?
      CRT_colorSchemes[color_scheme_i] :
         CRT_user_defined_color_schemes[color_scheme_i - LAST_COLORSCHEME];
}
