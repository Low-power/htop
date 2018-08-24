/* Do not edit this file. It was automatically generated. */

#ifndef HEADER_StringUtils
#define HEADER_StringUtils
/*
htop - StringUtils.h
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include <stdio.h>
#include <string.h>

#ifndef HAVE_STRCASESTR
#include <ctype.h>
/* OpenBSD strcasestr */
static inline char *
strcasestr(const char *s, const char *find)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != 0) {
		c = (char)tolower((unsigned char)c);
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
			} while ((char)tolower((unsigned char)sc) != c);
		} while (strncasecmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}
#endif

#define String_startsWith(s, match) (strncmp((s),(match),strlen(match)) == 0)
#define String_contains_i(s1, s2) (strcasestr(s1, s2) != NULL)

/*
 * String_startsWith gives better performance if strlen(match) can be computed
 * at compile time (e.g. when they are immutable string literals). :)
 */

char* String_cat(const char* s1, const char* s2);

char* String_trim(const char* in);

extern int String_eq(const char* s1, const char* s2);

char** String_split(const char* s, char sep, int* n);

void String_freeArray(char** s);

char* String_getToken(const char* line, const unsigned short int numMatch);

char* String_readLine(FILE* fd);

#endif
