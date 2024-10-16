/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_solaris_Battery
#define HEADER_solaris_Battery
/*
htop - solaris/Battery.h
Copyright 2015-2022 Rivoreo
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#ifdef HAVE_SYS_ACPI_DRV_H

void Battery_getData(double *level, ACPresence *is_on_ac);
#else
void Battery_getData(double *level, ACPresence *is_on_ac);
#endif

#endif
