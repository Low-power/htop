/*	Dump built-in color scheme of htop
	Copyright 2015-2022 Rivoreo

	This program is free software; you can redistribute it and/or modify
	it under the terms of any version of the GNU General Public License
	as published by the Free Software Foundation.

	This program is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.
*/

#include "CRT.h"
#include <stdio.h>

int main(int argc, char **argv) {
	if(argc != 2) {
		fprintf(stderr, "Usage: %s <scheme-name>|<scheme-index>\n", argv[0]);
		return -1;
	}
	CRT_initColorSchemes();
	endwin();	// CRT_initColorSchemes calls initscr(3X)
	int i = CRT_getColorSchemeIndexForName(argv[1]);
	if(i < 0) {
		char *end_p;
		i = strtol(argv[1], &end_p, 10);
		if(*end_p) {
			fprintf(stderr, "%s: Scheme '%s' not found\n", argv[0], argv[1]);
			return 1;
		}
		if(i < 0 || i >= (int)LAST_COLORSCHEME) {
			fprintf(stderr, "%s: Index value %d out of range (0~%u)\n",
				argv[0], i, (unsigned int)LAST_COLORSCHEME);
			return 1;
		}
	}
	printf("NAME=%s\n", CRT_color_scheme_names[i]);
	const int *colors = CRT_colorSchemes[i];
	printf("DEFAULT=%d\n", colors[HTOP_DEFAULT_COLOR]);
	printf("FUNCTION_BAR=%d\n", colors[HTOP_FUNCTION_BAR_COLOR]);
	printf("FUNCTION_KEY=%d\n", colors[HTOP_FUNCTION_KEY_COLOR]);
	printf("FAILED_SEARCH=%d\n", colors[HTOP_FAILED_SEARCH_COLOR]);
	printf("PANEL_HEADER_FOCUS=%d\n", colors[HTOP_PANEL_HEADER_FOCUS_COLOR]);
	printf("PANEL_HEADER_UNFOCUS=%d\n", colors[HTOP_PANEL_HEADER_UNFOCUS_COLOR]);
	printf("PANEL_SELECTION_FOCUS=%d\n", colors[HTOP_PANEL_SELECTION_FOCUS_COLOR]);
	printf("PANEL_SELECTION_FOLLOW=%d\n", colors[HTOP_PANEL_SELECTION_FOLLOW_COLOR]);
	printf("PANEL_SELECTION_UNFOCUS=%d\n", colors[HTOP_PANEL_SELECTION_UNFOCUS_COLOR]);
	printf("LARGE_NUMBER=%d\n", colors[HTOP_LARGE_NUMBER_COLOR]);
	printf("METER_TEXT=%d\n", colors[HTOP_METER_TEXT_COLOR]);
	printf("METER_VALUE=%d\n", colors[HTOP_METER_VALUE_COLOR]);
	printf("LED_COLOR=%d\n", colors[HTOP_LED_COLOR_COLOR]);
	printf("UPTIME=%d\n", colors[HTOP_UPTIME_COLOR]);
	printf("BATTERY=%d\n", colors[HTOP_BATTERY_COLOR]);
	printf("TASKS_RUNNING=%d\n", colors[HTOP_TASKS_RUNNING_COLOR]);
	printf("SWAP=%d\n", colors[HTOP_SWAP_COLOR]);
	printf("PROCESS=%d\n", colors[HTOP_PROCESS_COLOR]);
	printf("PROCESS_SHADOW=%d\n", colors[HTOP_PROCESS_SHADOW_COLOR]);
	printf("PROCESS_TAG=%d\n", colors[HTOP_PROCESS_TAG_COLOR]);
	printf("PROCESS_MEGABYTES=%d\n", colors[HTOP_PROCESS_MEGABYTES_COLOR]);
	printf("PROCESS_TREE=%d\n", colors[HTOP_PROCESS_TREE_COLOR]);
	printf("PROCESS_R_STATE=%d\n", colors[HTOP_PROCESS_R_STATE_COLOR]);
	printf("PROCESS_D_STATE=%d\n", colors[HTOP_PROCESS_D_STATE_COLOR]);
	printf("PROCESS_BASENAME=%d\n", colors[HTOP_PROCESS_BASENAME_COLOR]);
	printf("PROCESS_HIGH_PRIORITY=%d\n", colors[HTOP_PROCESS_HIGH_PRIORITY_COLOR]);
	printf("PROCESS_LOW_PRIORITY=%d\n", colors[HTOP_PROCESS_LOW_PRIORITY_COLOR]);
	printf("PROCESS_KERNEL_PROCESS=%d\n", colors[HTOP_PROCESS_KERNEL_PROCESS_COLOR]);
	printf("PROCESS_THREAD=%d\n", colors[HTOP_PROCESS_THREAD_COLOR]);
	printf("PROCESS_THREAD_BASENAME=%d\n", colors[HTOP_PROCESS_THREAD_BASENAME_COLOR]);
	printf("BAR_BORDER=%d\n", colors[HTOP_BAR_BORDER_COLOR]);
	printf("BAR_SHADOW=%d\n", colors[HTOP_BAR_SHADOW_COLOR]);
	printf("GRAPH_1=%d\n", colors[HTOP_GRAPH_1_COLOR]);
	printf("GRAPH_2=%d\n", colors[HTOP_GRAPH_2_COLOR]);
	printf("MEMORY_USED=%d\n", colors[HTOP_MEMORY_USED_COLOR]);
	printf("MEMORY_BUFFERS=%d\n", colors[HTOP_MEMORY_BUFFERS_COLOR]);
	printf("MEMORY_BUFFERS_TEXT=%d\n", colors[HTOP_MEMORY_BUFFERS_TEXT_COLOR]);
	printf("MEMORY_CACHE=%d\n", colors[HTOP_MEMORY_CACHE_COLOR]);
	printf("MEMORY_ZFS_ARC=%d\n", colors[HTOP_MEMORY_ZFS_ARC_COLOR]);
	printf("LOAD=%d\n", colors[HTOP_LOAD_COLOR]);
	printf("LOAD_AVERAGE_FIFTEEN=%d\n", colors[HTOP_LOAD_AVERAGE_FIFTEEN_COLOR]);
	printf("LOAD_AVERAGE_FIVE=%d\n", colors[HTOP_LOAD_AVERAGE_FIVE_COLOR]);
	printf("LOAD_AVERAGE_ONE=%d\n", colors[HTOP_LOAD_AVERAGE_ONE_COLOR]);
	printf("CHECK_BOX=%d\n", colors[HTOP_CHECK_BOX_COLOR]);
	printf("CHECK_MARK=%d\n", colors[HTOP_CHECK_MARK_COLOR]);
	printf("CHECK_TEXT=%d\n", colors[HTOP_CHECK_TEXT_COLOR]);
	printf("CLOCK=%d\n", colors[HTOP_CLOCK_COLOR]);
	printf("HELP_BOLD=%d\n", colors[HTOP_HELP_BOLD_COLOR]);
	printf("HOSTNAME=%d\n", colors[HTOP_HOSTNAME_COLOR]);
	printf("CPU_NICE=%d\n", colors[HTOP_CPU_NICE_COLOR]);
	printf("CPU_NICE_TEXT=%d\n", colors[HTOP_CPU_NICE_TEXT_COLOR]);
	printf("CPU_NORMAL=%d\n", colors[HTOP_CPU_NORMAL_COLOR]);
	printf("CPU_KERNEL=%d\n", colors[HTOP_CPU_KERNEL_COLOR]);
	printf("CPU_IOWAIT=%d\n", colors[HTOP_CPU_IOWAIT_COLOR]);
	printf("CPU_IRQ=%d\n", colors[HTOP_CPU_IRQ_COLOR]);
	printf("CPU_SOFTIRQ=%d\n", colors[HTOP_CPU_SOFTIRQ_COLOR]);
	printf("CPU_STEAL=%d\n", colors[HTOP_CPU_STEAL_COLOR]);
	printf("CPU_GUEST=%d\n", colors[HTOP_CPU_GUEST_COLOR]);
	return 0;
}
