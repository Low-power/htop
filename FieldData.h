/*
htop - FieldData.h
(C) 2004-2015 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#ifndef HTOP_FIELDDATA_H
#define HTOP_FIELDDATA_H

typedef struct {
   const char* name;
   const char* title;
   const char* description;
   int flags;
} FieldData;

#endif
