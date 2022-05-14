/*
htop - CRT.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
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

typedef enum ColorSchemes_ {
   COLORSCHEME_DEFAULT = 0,
   COLORSCHEME_MONOCHROME = 1,
   COLORSCHEME_BLACKONWHITE = 2,
   COLORSCHEME_LIGHTTERMINAL = 3,
   COLORSCHEME_MIDNIGHT = 4,
   COLORSCHEME_BLACKNIGHT = 5,
   COLORSCHEME_BROKENGRAY = 6,
   LAST_COLORSCHEME = 7,
} ColorSchemes;

typedef enum ColorElements_ {
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
   HTOP_LED_COLOR_COLOR,
   HTOP_UPTIME_COLOR,
   HTOP_BATTERY_COLOR,
   HTOP_TASKS_RUNNING_COLOR,
   HTOP_SWAP_COLOR,
   HTOP_PROCESS_COLOR,
   HTOP_PROCESS_SHADOW_COLOR,
   HTOP_PROCESS_TAG_COLOR,
   HTOP_PROCESS_MEGABYTES_COLOR,
   HTOP_PROCESS_TREE_COLOR,
   HTOP_PROCESS_R_STATE_COLOR,
   HTOP_PROCESS_D_STATE_COLOR,
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
} ColorElements;

#define KEY_ALT(x) (KEY_F(64 - 26) + ((x) - 'A'))

}*/

#include "config.h"
#include "CRT.h"
#include "StringUtils.h"
#include "RichString.h"
#include "local-curses.h"
#include <sys/types.h>
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

static bool CRT_hasColors;

int CRT_delay = 0;

int* CRT_colors;

int CRT_colorSchemes[LAST_COLORSCHEME][LAST_COLORELEMENT] = {
   [COLORSCHEME_DEFAULT] = {
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
      [HTOP_LED_COLOR_COLOR] = ColorPair(Green,Black),
      [HTOP_TASKS_RUNNING_COLOR] = A_BOLD | ColorPair(Green,Black),
      [HTOP_PROCESS_COLOR] = A_NORMAL,
      [HTOP_PROCESS_SHADOW_COLOR] = A_BOLD | ColorPairGrayBlack,
      [HTOP_PROCESS_TAG_COLOR] = A_BOLD | ColorPair(Yellow,Black),
      [HTOP_PROCESS_MEGABYTES_COLOR] = ColorPair(Cyan,Black),
      [HTOP_PROCESS_BASENAME_COLOR] = A_BOLD | ColorPair(Cyan,Black),
      [HTOP_PROCESS_TREE_COLOR] = ColorPair(Cyan,Black),
      [HTOP_PROCESS_R_STATE_COLOR] = ColorPair(Green,Black),
      [HTOP_PROCESS_D_STATE_COLOR] = A_BOLD | ColorPair(Red,Black),
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
      [HTOP_CPU_IOWAIT_COLOR] = A_BOLD | ColorPair(Black, Black),
      [HTOP_CPU_IRQ_COLOR] = ColorPair(Yellow,Black),
      [HTOP_CPU_SOFTIRQ_COLOR] = ColorPair(Magenta,Black),
      [HTOP_CPU_STEAL_COLOR] = ColorPair(Cyan,Black),
      [HTOP_CPU_GUEST_COLOR] = ColorPair(Cyan,Black),
   },
   [COLORSCHEME_MONOCHROME] = {
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
      [HTOP_LED_COLOR_COLOR] = A_NORMAL,
      [HTOP_TASKS_RUNNING_COLOR] = A_BOLD,
      [HTOP_PROCESS_COLOR] = A_NORMAL,
      [HTOP_PROCESS_SHADOW_COLOR] = A_DIM,
      [HTOP_PROCESS_TAG_COLOR] = A_BOLD,
      [HTOP_PROCESS_MEGABYTES_COLOR] = A_BOLD,
      [HTOP_PROCESS_BASENAME_COLOR] = A_BOLD,
      [HTOP_PROCESS_TREE_COLOR] = A_BOLD,
      [HTOP_PROCESS_R_STATE_COLOR] = A_BOLD,
      [HTOP_PROCESS_D_STATE_COLOR] = A_BOLD,
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
   [COLORSCHEME_BLACKONWHITE] = {
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
      [HTOP_LED_COLOR_COLOR] = ColorPair(Green,White),
      [HTOP_TASKS_RUNNING_COLOR] = ColorPair(Green,White),
      [HTOP_PROCESS_COLOR] = ColorPair(Black,White),
      [HTOP_PROCESS_SHADOW_COLOR] = A_BOLD | ColorPair(Black,White),
      [HTOP_PROCESS_TAG_COLOR] = ColorPair(White,Blue),
      [HTOP_PROCESS_MEGABYTES_COLOR] = ColorPair(Blue,White),
      [HTOP_PROCESS_BASENAME_COLOR] = ColorPair(Blue,White),
      [HTOP_PROCESS_TREE_COLOR] = ColorPair(Green,White),
      [HTOP_PROCESS_R_STATE_COLOR] = ColorPair(Green,White),
      [HTOP_PROCESS_D_STATE_COLOR] = A_BOLD | ColorPair(Red,White),
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
   [COLORSCHEME_LIGHTTERMINAL] = {
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
      [HTOP_LED_COLOR_COLOR] = ColorPair(Green,Black),
      [HTOP_TASKS_RUNNING_COLOR] = ColorPair(Green,Black),
      [HTOP_PROCESS_COLOR] = ColorPair(Black,Black),
      [HTOP_PROCESS_SHADOW_COLOR] = A_BOLD | ColorPairGrayBlack,
      [HTOP_PROCESS_TAG_COLOR] = ColorPair(White,Blue),
      [HTOP_PROCESS_MEGABYTES_COLOR] = ColorPair(Blue,Black),
      [HTOP_PROCESS_BASENAME_COLOR] = ColorPair(Green,Black),
      [HTOP_PROCESS_TREE_COLOR] = ColorPair(Blue,Black),
      [HTOP_PROCESS_R_STATE_COLOR] = ColorPair(Green,Black),
      [HTOP_PROCESS_D_STATE_COLOR] = A_BOLD | ColorPair(Red,Black),
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
   [COLORSCHEME_MIDNIGHT] = {
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
      [HTOP_LED_COLOR_COLOR] = ColorPair(Green,Blue),
      [HTOP_TASKS_RUNNING_COLOR] = A_BOLD | ColorPair(Green,Blue),
      [HTOP_PROCESS_COLOR] = ColorPair(White,Blue),
      [HTOP_PROCESS_SHADOW_COLOR] = A_BOLD | ColorPair(Black,Blue),
      [HTOP_PROCESS_TAG_COLOR] = A_BOLD | ColorPair(Yellow,Blue),
      [HTOP_PROCESS_MEGABYTES_COLOR] = ColorPair(Cyan,Blue),
      [HTOP_PROCESS_BASENAME_COLOR] = A_BOLD | ColorPair(Cyan,Blue),
      [HTOP_PROCESS_TREE_COLOR] = ColorPair(Cyan,Blue),
      [HTOP_PROCESS_R_STATE_COLOR] = ColorPair(Green,Blue),
      [HTOP_PROCESS_D_STATE_COLOR] = A_BOLD | ColorPair(Red,Blue),
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
   [COLORSCHEME_BLACKNIGHT] = {
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
      [HTOP_LED_COLOR_COLOR] = ColorPair(Green,Black),
      [HTOP_TASKS_RUNNING_COLOR] = A_BOLD | ColorPair(Green,Black),
      [HTOP_PROCESS_COLOR] = ColorPair(Cyan,Black),
      [HTOP_PROCESS_SHADOW_COLOR] = A_BOLD | ColorPairGrayBlack,
      [HTOP_PROCESS_TAG_COLOR] = A_BOLD | ColorPair(Yellow,Black),
      [HTOP_PROCESS_MEGABYTES_COLOR] = A_BOLD | ColorPair(Green,Black),
      [HTOP_PROCESS_BASENAME_COLOR] = A_BOLD | ColorPair(Green,Black),
      [HTOP_PROCESS_TREE_COLOR] = ColorPair(Cyan,Black),
      [HTOP_PROCESS_KERNEL_PROCESS_COLOR] = ColorPair(Red,Black),
      [HTOP_PROCESS_THREAD_COLOR] = ColorPair(Green,Black),
      [HTOP_PROCESS_THREAD_BASENAME_COLOR] = A_BOLD | ColorPair(Blue,Black),
      [HTOP_PROCESS_R_STATE_COLOR] = ColorPair(Green,Black),
      [HTOP_PROCESS_D_STATE_COLOR] = A_BOLD | ColorPair(Red,Black),
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
   [COLORSCHEME_BROKENGRAY] = { 0 } // dynamically generated.
};

int CRT_cursorX = 0;

int CRT_scrollHAmount = 5;

int CRT_scrollWheelVAmount = 10;

char* CRT_termType;

// TODO move color scheme to Settings, perhaps?

int CRT_colorScheme = 0;

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

// TODO: pass an instance of Settings instead.

void CRT_init(int delay, int colorScheme) {
   initscr();
   noecho();
   CRT_delay = delay;
   if (CRT_delay == 0) {
      CRT_delay = 1;
   }
   CRT_colors = CRT_colorSchemes[colorScheme];
   CRT_colorScheme = colorScheme;
   
   for (int i = 0; i < LAST_COLORELEMENT; i++) {
      unsigned int color = CRT_colorSchemes[COLORSCHEME_DEFAULT][i];
      CRT_colorSchemes[COLORSCHEME_BROKENGRAY][i] = color == (A_BOLD | ColorPairGrayBlack) ? ColorPair(White,Black) : color;
   }
   
   halfdelay(CRT_delay);
   nonl();
   intrflush(stdscr, false);
   keypad(stdscr, true);
   mouseinterval(0);
   curs_set(0);
   CRT_hasColors = has_colors();
   if(CRT_hasColors) start_color();
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
#ifndef DEBUG
   signal(SIGILL, CRT_handleAbnormalSignal);
   signal(SIGBUS, CRT_handleAbnormalSignal);
   signal(SIGSEGV, CRT_handleAbnormalSignal);
#endif
   signal(SIGTERM, CRT_handleSIGTERM);
   signal(SIGQUIT, CRT_handleSIGTERM);
   use_default_colors();
   if (!has_colors())
      CRT_colorScheme = 1;
   CRT_setColors(CRT_colorScheme);

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

#if NCURSES_MOUSE_VERSION > 1
   mousemask(BUTTON1_RELEASED | BUTTON4_PRESSED | BUTTON5_PRESSED, NULL);
#else
   mousemask(BUTTON1_RELEASED, NULL);
#endif

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

int CRT_readKey() {
   nocbreak();
   cbreak();
   nodelay(stdscr, FALSE);
   int ret = getch();
   halfdelay(CRT_delay);
   return ret;
}

void CRT_disableDelay() {
   nocbreak();
   cbreak();
   nodelay(stdscr, TRUE);
}

void CRT_enableDelay() {
   halfdelay(CRT_delay);
}

void CRT_setColors(int colorScheme) {
   CRT_colorScheme = colorScheme;

   for (int i = 0; i < 8; i++) {
      for (int j = 0; j < 8; j++) {
         if (ColorIndex(i,j) != ColorPairGrayBlack) {
            int bg = (colorScheme != COLORSCHEME_BLACKNIGHT)
                     ? (j==0 ? -1 : j)
                     : j;
            init_pair(ColorIndex(i,j), i, bg);
         }
      }
   }

   int grayBlackFg = COLORS > 8 ? 8 : 0;
   int grayBlackBg = (colorScheme != COLORSCHEME_BLACKNIGHT)
                     ? -1
                     : 0;
   init_pair(ColorIndexGrayBlack, grayBlackFg, grayBlackBg);

   CRT_colors = CRT_colorSchemes[colorScheme];
}
