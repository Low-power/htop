/*
htop - bsd/BSDProcess.c
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Process.h"
#include "CRT.h"
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>

void BSDProcess_writeField(const Process *this, RichString *str, ProcessField field) {
	char buffer[256];
	int attr = CRT_colors[HTOP_DEFAULT_COLOR];
	switch(field) {
		case HTOP_TTY_FIELD:
			if(this->tty_nr == NODEV) {
				attr = CRT_colors[HTOP_PROCESS_SHADOW_COLOR];
				xSnprintf(buffer, sizeof buffer, "?       ");
			} else {
				xSnprintf(buffer, sizeof buffer, "%-8s",
					devname(this->tty_nr, S_IFCHR));
			}
			break;
		default:
			return Process_writeField(this, str, field);
	}
	RichString_append(str, attr, buffer);
}
