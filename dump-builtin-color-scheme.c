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
#include "local-curses.h"
#include <unistd.h>
#include <stdio.h>

//#define ColorIndex(i,j) ( ( 7 - (i) ) * 8  +  (j) )

//#if COLOR_PAIR(1) == NCURSES_BITS(1, 0)
#if 1
static const char *color_names[] = {
	[COLOR_BLACK] = "BLACK",
	[COLOR_RED] = "RED",
	[COLOR_GREEN] = "GREEN",
	[COLOR_YELLOW] = "YELLOW",
	[COLOR_BLUE] = "BLUE",
	[COLOR_MAGENTA] = "MAGENTA",
	[COLOR_CYAN] = "CYAN",
	[COLOR_WHITE] = "WHITE"
};

static void print_font_attribute(int attr) {
	attr &= ~A_COLOR;
	if(attr & A_STANDOUT) fputs(",STANDOUT", stdout);
	if(attr & A_UNDERLINE) fputs(",UNDERLINE", stdout);
	if(attr & A_REVERSE) fputs(",REVERSE", stdout);
	if(attr & A_BLINK) fputs(",BLINK", stdout);
	if(attr & A_DIM) fputs(",DIM", stdout);
	if(attr & A_BOLD) fputs(",BOLD", stdout);
	if(attr & A_ALTCHARSET) fputs(",ALTCHARSET", stdout);
	if(attr & A_INVIS) fputs(",INVIS", stdout);
	if(attr & A_PROTECT) fputs(",PROTECT", stdout);
	if(attr & A_HORIZONTAL) fputs(",HORIZONTAL", stdout);
	if(attr & A_LEFT) fputs(",LEFT", stdout);
	if(attr & A_LOW) fputs(",LOW", stdout);
	if(attr & A_RIGHT) fputs(",RIGHT", stdout);
	if(attr & A_TOP) fputs(",TOP", stdout);
	if(attr & A_VERTICAL) fputs(",VERTICAL", stdout);
	putchar('\n');
}
#endif

int main(int argc, char **argv) {
	if(argc != 2) {
		fprintf(stderr, "Usage: %s <scheme-name>|<scheme-index>\n", argv[0]);
		return -1;
	}
	int fd = dup(STDOUT_FILENO);
	if(fd == -1) {
		perror("dup");
		return 1;
	}
	close(STDOUT_FILENO);
	CRT_initColorSchemes();
	endwin();	// CRT_initColorSchemes calls initscr(3X)
	dup2(fd, STDOUT_FILENO);
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
				argv[0], i, (unsigned int)(LAST_COLORSCHEME - 1));
			return 1;
		}
	}
	printf("NAME=%s\n", CRT_color_scheme_names[i]);
	printf("MONOCHROME=%s\n", CRT_color_scheme_is_monochrome[i] ? "YES" : "NO");
	const int *colors = CRT_colorSchemes[i];
#if 1
#define PRINT_COLOR_ATTRIBUTE(KEY) \
	do { \
		printf(#KEY "=%s,%s", \
			color_names[7 - (int)((colors[HTOP_##KEY##_COLOR]&A_COLOR)>>NCURSES_ATTR_SHIFT) / 8 % 8], \
			color_names[(int)((colors[HTOP_##KEY##_COLOR]&A_COLOR)>>NCURSES_ATTR_SHIFT) % 8]); \
		print_font_attribute(colors[HTOP_##KEY##_COLOR]); \
	} while(0);
#else
#define PRINT_COLOR_ATTRIBUTE(KEY) printf(#KEY "=0x%x\n", colors[HTOP_##KEY##_COLOR])
#endif
	PRINT_COLOR_ATTRIBUTE(DEFAULT);
	PRINT_COLOR_ATTRIBUTE(FUNCTION_BAR);
	PRINT_COLOR_ATTRIBUTE(FUNCTION_KEY);
	PRINT_COLOR_ATTRIBUTE(FAILED_SEARCH);
	PRINT_COLOR_ATTRIBUTE(PANEL_HEADER_FOCUS);
	PRINT_COLOR_ATTRIBUTE(PANEL_HEADER_UNFOCUS);
	PRINT_COLOR_ATTRIBUTE(PANEL_SELECTION_FOCUS);
	PRINT_COLOR_ATTRIBUTE(PANEL_SELECTION_FOLLOW);
	PRINT_COLOR_ATTRIBUTE(PANEL_SELECTION_UNFOCUS);
	PRINT_COLOR_ATTRIBUTE(LARGE_NUMBER);
	PRINT_COLOR_ATTRIBUTE(METER_TEXT);
	PRINT_COLOR_ATTRIBUTE(METER_VALUE);
	PRINT_COLOR_ATTRIBUTE(LED);
	PRINT_COLOR_ATTRIBUTE(UPTIME);
	PRINT_COLOR_ATTRIBUTE(BATTERY);
	PRINT_COLOR_ATTRIBUTE(TASKS_RUNNING);
	PRINT_COLOR_ATTRIBUTE(SWAP);
	PRINT_COLOR_ATTRIBUTE(PROCESS);
	PRINT_COLOR_ATTRIBUTE(PROCESS_SHADOW);
	PRINT_COLOR_ATTRIBUTE(PROCESS_CREATED);
	PRINT_COLOR_ATTRIBUTE(PROCESS_TAG);
	PRINT_COLOR_ATTRIBUTE(PROCESS_MEGABYTES);
	PRINT_COLOR_ATTRIBUTE(PROCESS_TREE);
	PRINT_COLOR_ATTRIBUTE(PROCESS_R_STATE);
	PRINT_COLOR_ATTRIBUTE(PROCESS_D_STATE);
	PRINT_COLOR_ATTRIBUTE(PROCESS_Z_STATE);
	PRINT_COLOR_ATTRIBUTE(PROCESS_BASENAME);
	PRINT_COLOR_ATTRIBUTE(PROCESS_HIGH_PRIORITY);
	PRINT_COLOR_ATTRIBUTE(PROCESS_LOW_PRIORITY);
	PRINT_COLOR_ATTRIBUTE(PROCESS_KERNEL_PROCESS);
	PRINT_COLOR_ATTRIBUTE(PROCESS_THREAD);
	PRINT_COLOR_ATTRIBUTE(PROCESS_THREAD_BASENAME);
	PRINT_COLOR_ATTRIBUTE(BAR_BORDER);
	PRINT_COLOR_ATTRIBUTE(BAR_SHADOW);
	PRINT_COLOR_ATTRIBUTE(GRAPH_1);
	PRINT_COLOR_ATTRIBUTE(GRAPH_2);
	PRINT_COLOR_ATTRIBUTE(MEMORY_USED);
	PRINT_COLOR_ATTRIBUTE(MEMORY_BUFFERS);
	PRINT_COLOR_ATTRIBUTE(MEMORY_BUFFERS_TEXT);
	PRINT_COLOR_ATTRIBUTE(MEMORY_CACHE);
	PRINT_COLOR_ATTRIBUTE(MEMORY_ZFS_ARC);
	PRINT_COLOR_ATTRIBUTE(LOAD);
	PRINT_COLOR_ATTRIBUTE(LOAD_AVERAGE_FIFTEEN);
	PRINT_COLOR_ATTRIBUTE(LOAD_AVERAGE_FIVE);
	PRINT_COLOR_ATTRIBUTE(LOAD_AVERAGE_ONE);
	PRINT_COLOR_ATTRIBUTE(CHECK_BOX);
	PRINT_COLOR_ATTRIBUTE(CHECK_MARK);
	PRINT_COLOR_ATTRIBUTE(CHECK_TEXT);
	PRINT_COLOR_ATTRIBUTE(CLOCK);
	PRINT_COLOR_ATTRIBUTE(HELP_BOLD);
	PRINT_COLOR_ATTRIBUTE(HOSTNAME);
	PRINT_COLOR_ATTRIBUTE(CPU_NICE);
	PRINT_COLOR_ATTRIBUTE(CPU_NICE_TEXT);
	PRINT_COLOR_ATTRIBUTE(CPU_NORMAL);
	PRINT_COLOR_ATTRIBUTE(CPU_KERNEL);
	PRINT_COLOR_ATTRIBUTE(CPU_IOWAIT);
	PRINT_COLOR_ATTRIBUTE(CPU_IRQ);
	PRINT_COLOR_ATTRIBUTE(CPU_SOFTIRQ);
	PRINT_COLOR_ATTRIBUTE(CPU_STEAL);
	PRINT_COLOR_ATTRIBUTE(CPU_GUEST);
#undef PRINT_COLOR_ATTRIBUTE
	return 0;
}
