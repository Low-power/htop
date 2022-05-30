/*
htop - XAlloc.c
(C) 2004-2017 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

/*{
#include <stdlib.h>
}*/

#include "config.h"
#include "XAlloc.h"
#include "RichString.h"
#include <unistd.h>
#include <string.h>

#if !defined __GNUC__ && !defined __attribute__
#define __attribute__(A)
#endif

void __attribute__((__noreturn__)) xFail() {
   curs_set(1);
   endwin();
   perror(NULL);
   _exit(1);
}

void* xMalloc(size_t size) {
   void* data = malloc(size);
   if (!data && size > 0) {
      xFail();
   }
   return data;
}

void* xCalloc(size_t nmemb, size_t size) {
   void* data = calloc(nmemb, size);
   if (!data && nmemb > 0 && size > 0) {
      xFail();
   }
   return data;
}

void* xRealloc(void* ptr, size_t size) {
   void* data = realloc(ptr, size);
   if (!data && size > 0) {
      xFail();
   }
   return data;
}

#define xSnprintf(fmt, len, ...) do { int _l=len; int _n=snprintf(fmt, _l, __VA_ARGS__); if (!(_n > -1 && _n < _l)) xFail(); } while(0)

#undef xStrdup
#undef xStrdup_
#define xStrdup_ xStrdup

#ifndef __has_attribute // Clang's macro
# define __has_attribute(x) 0
#endif
#if (__has_attribute(nonnull) || \
    (defined __GNUC__ && ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3))))
char* xStrdup_(const char* str) __attribute__((nonnull));
#endif // __has_attribute(nonnull) || GNU C 3.3 or later

char* xStrdup_(const char* str) {
   char* data = strdup(str);
   if (!data) {
      xFail();
   }
   return data;
}
