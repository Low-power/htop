/*
htop - local-curses.h
(C) 2004,2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#ifndef LOCAL_CURSES_H
#define LOCAL_CURSES_H

#include "config.h"

#ifdef HAVE_LIBNCURSESW
#define NCURSES_WIDECHAR 1
#endif

#ifdef HAVE_NCURSESW_CURSES_H
#include <ncursesw/curses.h>
#elif defined HAVE_NCURSES_NCURSES_H
#include <ncurses/ncurses.h>
#elif defined HAVE_NCURSES_CURSES_H
#include <ncurses/curses.h>
#elif defined HAVE_NCURSES_H
#include <ncurses.h>
#else	/* defined HAVE_CURSES_H */
#include <curses.h>
#endif

#endif
