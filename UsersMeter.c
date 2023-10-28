/*
htop - UsersMeter.c
(C) 2004-2011 Hisham H. Muhammad
Copyright 2015-2023 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/


/*{
#include "Meter.h"
}*/

#include "config.h"
#ifdef HAVE_UTMPX
#include "UsersMeter.h"
#include "CRT.h"
#include "XAlloc.h"
#include "StringUtils.h"
#include <utmpx.h>
#include <string.h>

int UsersMeter_attributes[] = {
	HTOP_SESSIONS_COLOR, HTOP_USERS_COLOR
};

static void UsersMeter_updateValues(Meter *this, char *buffer, int len) {
	int session_count = 0;
	int user_count = 0;
	struct {
		char *name;
		size_t len;
	} *user_names = NULL;
	const struct utmpx *utx;
	setutxent();
	while ((utx = getutxent())) {
		if(utx->ut_type != USER_PROCESS) continue;
		session_count++;
		size_t len = strnlen(utx->ut_user, sizeof utx->ut_user);
		int i = user_count;
		do {
			if(--i < 0) {
				user_names = xRealloc(user_names, (user_count + 1) * sizeof *user_names);
				user_names[user_count].len = len;
				user_names[user_count].name = xMalloc(len);
				memcpy(user_names[user_count].name, utx->ut_user, len);
				user_count++;
				break;
			}
		} while(len != user_names[i].len || memcmp(utx->ut_user, user_names[i].name, len));
	}
	endutxent();
	free(user_names);

	xSnprintf(buffer, len, "%d/%d", session_count, user_count);
	this->values[0] = session_count;
	this->values[1] = user_count;
}

static void UsersMeter_display(Object *super, RichString *out) {
	Meter *this = (Meter *)super;
	char buffer[20];
	int session_count = this->values[0];
	int user_count = this->values[1];
	RichString_write(out, CRT_colors[HTOP_METER_TEXT_COLOR], ": ");
	xSnprintf(buffer, sizeof buffer, "%d", session_count);
	RichString_append(out, CRT_colors[HTOP_SESSIONS_COLOR], buffer);
	RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], session_count == 1 ? " session, " : " sessions, ");
	xSnprintf(buffer, sizeof buffer, "%d", user_count);
	RichString_append(out, CRT_colors[HTOP_USERS_COLOR], buffer);
	RichString_append(out, CRT_colors[HTOP_METER_TEXT_COLOR], user_count == 1 ? " user" : " users");
}

static double UsersMeter_getMaximum(Meter *this) {
	double v = this->values[0];
	double r = 64;
	while(v > 64) {
		v /= 2;
		r *= 2;
	}
	return r;
}

MeterClass UsersMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = UsersMeter_display,
   },
   .updateValues = UsersMeter_updateValues,
   .getMaximum = UsersMeter_getMaximum,
   .defaultMode = TEXT_METERMODE,
   .maxItems = 2,
   .total = 64,
   .attributes = UsersMeter_attributes,
   .values_are_overlapped = true,
   .name = "Users",
   .uiName = "User session counter",
   .caption = "Usr"
};
#endif
