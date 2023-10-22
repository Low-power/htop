/*
htop - StringUtils.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include <string.h>
#include <stdio.h>

#define String_startsWith(s, match) (strncmp((s),(match),strlen(match)) == 0)
#define String_contains_i(s1, s2) (strcasestr(s1, s2) != NULL)
}*/

#include "config.h"
#include "StringUtils.h"
#include "XAlloc.h"
#include <strings.h>
#include <stdlib.h>

/*
 * String_startsWith gives better performance if strlen(match) can be computed
 * at compile time (e.g. when they are immutable string literals). :)
 */

#ifndef HAVE_STRCASESTR
/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <ctype.h>

char* strcasestr(const char* s, const char* find) {
	char c = *find++;
	if (c) {
		c = tolower((unsigned char)c);
		size_t len = strlen(find);
		do {
			char sc;
			do {
				if (!(sc = *s++)) return NULL;
			} while (tolower((unsigned char)sc) != c);
		} while (strncasecmp(s, find, len) != 0);
		s--;
	}
	return (char *)s;
}
#endif

#ifndef HAVE_STRNLEN
size_t strnlen(const char *s, size_t max_len) {
	size_t len = 0;
	while(len < max_len && s[len]) len++;
	return len;
}
#endif

char* String_cat(const char* s1, const char* s2) {
   size_t l1 = strlen(s1);
   size_t l2 = strlen(s2);
   char* out = xMalloc(l1 + l2 + 1);
   memcpy(out, s1, l1);
   memcpy(out+l1, s2, l2+1);
   return out;
}

char* String_trim(const char* in) {
   while (in[0] == ' ' || in[0] == '\t' || in[0] == '\n') {
      in++;
   }
   size_t len = strlen(in);
   while (len > 0 && (in[len-1] == ' ' || in[len-1] == '\t' || in[len-1] == '\n')) {
      len--;
   }
   char* out = xMalloc(len+1);
   memcpy(out, in, len);
   out[len] = '\0';
   return out;
}

inline int String_eq(const char* s1, const char* s2) {
   if (s1 == NULL || s2 == NULL) {
      return s1 == NULL && s2 == NULL;
   }
   return (strcmp(s1, s2) == 0);
}

char** String_split(const char* s, char sep, int* n) {
#define rate 10
   *n = 0;
   char** out = xCalloc(rate, sizeof(char*));
   int ctr = 0;
   int blocks = rate;
   char* where;
   while ((where = strchr(s, sep)) != NULL) {
      size_t size = where - s;
      char* token = xMalloc(size + 1);
      memcpy(token, s, size);
      token[size] = '\0';
      out[ctr] = token;
      ctr++;
      if (ctr == blocks) {
         blocks += rate;
         out = xRealloc(out, sizeof(char*) * blocks);
      }
      s += size + 1;
   }
   if (s[0] != '\0') {
      size_t size = strlen(s);
      char* token = xMalloc(size + 1);
      memcpy(token, s, size + 1);
      out[ctr] = token;
      ctr++;
   }
   out = xRealloc(out, sizeof(char*) * (ctr + 1));
   out[ctr] = NULL;
   *n = ctr;
   return out;
#undef rate
}

void String_freeArray(char** s) {
   if (!s) {
      return;
   }
   for (int i = 0; s[i] != NULL; i++) {
      free(s[i]);
   }
   free(s);
}

char* String_getToken(const char* line, const unsigned int numMatch) {
   const unsigned int len = strlen(line);
   char inWord = 0;
   unsigned int count = 0;
   char match[50];

   unsigned int foundCount = 0;

   for (unsigned int i = 0; i < len; i++) {
      char lastState = inWord;
      inWord = line[i] == ' ' ? 0:1;

      if (lastState == 0 && inWord == 1) count++;
      if(inWord == 1){
         if (count == numMatch && line[i] != ' ' && line[i] != '\0' && line[i] != '\n') {
            match[foundCount] = line[i];
            foundCount++;
         }
      }
   }

   match[foundCount] = '\0';
   return xStrdup(match);
}

char* String_readLine(FILE *f) {
   const int step = 1024;
   int bufSize = step;
   char* buffer = xMalloc(step + 1);
   char* at = buffer;
   for (;;) {
      char* ok = fgets(at, step + 1, f);
      if (!ok) {
         free(buffer);
         return NULL;
      }
      char* newLine = strrchr(at, '\n');
      if (newLine) {
         *newLine = '\0';
         return buffer;
      } else {
         if (feof(f)) {
            return buffer;
         }
      }
      bufSize += step;
      buffer = xRealloc(buffer, bufSize + 1);
      at = buffer + bufSize - step;
   }
}
