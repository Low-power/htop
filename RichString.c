/*
htop - RichString.c
(C) 2004,2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "RichString.h"
#include "XAlloc.h"

#include <stdlib.h>
#include <string.h>

#define RICHSTRING_MAXLEN 350

/*{
#include "config.h"
#include <ctype.h>
#include <assert.h>
#include "local-curses.h"
#ifdef HAVE_LIBNCURSESW
#include <wctype.h>
#endif

#define RichString_size(this) ((this)->chlen)
#define RichString_sizeVal(this) ((this).chlen)

#define RichString_begin(this) RichString (this); memset(&this, 0, sizeof(RichString)); (this).chptr = (this).chstr;
#define RichString_beginAllocated(this) memset(&this, 0, sizeof(RichString)); (this).chptr = (this).chstr;
#define RichString_end(this) RichString_prune(&(this));

#ifdef HAVE_LIBNCURSESW
#define RichString_printVal(this, y, x) mvadd_wchstr(y, x, (this).chptr)
#define RichString_printoffnVal(this, y, x, off, n) mvadd_wchnstr(y, x, (this).chptr + off, n)
#define RichString_getCharVal(this, i) ((this).chptr[i].chars[0] & 255)
#define RichString_setChar(this, at, ch) do{ (this)->chptr[(at)] = (CharType) { .chars = { ch, 0 } }; } while(0)
#define CharType cchar_t
#else
#define RichString_printVal(this, y, x) mvaddchstr(y, x, (this).chptr)
#define RichString_printoffnVal(this, y, x, off, n) mvaddchnstr(y, x, (this).chptr + off, n)
#define RichString_getCharVal(this, i) ((this).chptr[i])
#define RichString_setChar(this, at, ch) do{ (this)->chptr[(at)] = ch; } while(0)
#define CharType chtype
#endif

typedef struct RichString_ {
   int chlen;
   CharType* chptr;
   CharType chstr[RICHSTRING_MAXLEN+1];
} RichString;

}*/

#ifndef CLAMP
#define CLAMP(x,low,high) (((x)>(high))?(high):(((x)<(low))?(low):(x)))
#endif

#define charBytes(n) (sizeof(CharType) * (n))

static void RichString_extendLen(RichString* this, int len) {
   if (this->chlen <= RICHSTRING_MAXLEN) {
      if (len > RICHSTRING_MAXLEN) {
         this->chptr = xMalloc(charBytes(len + 1));
         memcpy(this->chptr, this->chstr, charBytes(this->chlen));
      }
   } else {
      if (len <= RICHSTRING_MAXLEN) {
         memcpy(this->chstr, this->chptr, charBytes(len));
         free(this->chptr);
         this->chptr = this->chstr;
      } else {
         this->chptr = xRealloc(this->chptr, charBytes(len + 1));
      }
   }

   RichString_setChar(this, len, 0);
   this->chlen = len;
}

#define RichString_setLen(this, len) do{ if(len < RICHSTRING_MAXLEN && this->chlen < RICHSTRING_MAXLEN) { RichString_setChar(this,len,0); this->chlen=len; } else RichString_extendLen(this,len); }while(0)

#ifdef HAVE_LIBNCURSESW

static inline void RichString_writeFrom(RichString* this, int attrs, const char *s, unsigned int from, size_t len) {
   wchar_t wcs[len];
   size_t wcs_len = mbstowcs(wcs, s, len);
   size_t new_len = from + (wcs_len == (size_t)-1 ? len : wcs_len);
   RichString_setLen(this, new_len);
   for (unsigned int i = from, j = 0; i < new_len; i++, j++) {
      wchar_t c = wcs_len == (size_t)-1 ? s[j] : wcs[j];
      this->chptr[i] = (CharType){ .attr = attrs & 0xffffff, .chars = { iswprint(c) ? c : L'?' } };
   }
}

inline void RichString_setAttrn(RichString* this, int attrs, int start, int finish) {
   cchar_t* ch = this->chptr + start;
   finish = CLAMP(finish, 0, this->chlen - 1);
   for (int i = start; i <= finish; i++) {
      ch->attr = attrs;
      ch++;
   }
}

int RichString_findChar(RichString* this, char c, int start) {
   wchar_t wc = btowc(c);
   cchar_t* ch = this->chptr + start;
   for (int i = start; i < this->chlen; i++) {
      if (ch->chars[0] == wc)
         return i;
      ch++;
   }
   return -1;
}

#else

static inline void RichString_writeFrom(RichString* this, int attrs, const char *s, unsigned int from, size_t len) {
   size_t new_len = from + len;
   RichString_setLen(this, new_len);
   for (unsigned int i = from, j = 0; i < new_len; i++, j++) {
      this->chptr[i] = (s[j] >= 32 ? s[j] : '?') | attrs;
   }
   this->chptr[new_len] = 0;
}

void RichString_setAttrn(RichString* this, int attrs, int start, int finish) {
   chtype* ch = this->chptr + start;
   finish = CLAMP(finish, 0, this->chlen - 1);
   for (int i = start; i <= finish; i++) {
      *ch = (*ch & 0xff) | attrs;
      ch++;
   }
}

int RichString_findChar(RichString* this, char c, int start) {
   chtype* ch = this->chptr + start;
   for (int i = start; i < this->chlen; i++) {
      if ((*ch & 0xff) == (chtype) c)
         return i;
      ch++;
   }
   return -1;
}

#endif

void RichString_prune(RichString* this) {
   if (this->chlen > RICHSTRING_MAXLEN)
      free(this->chptr);
   memset(this, 0, sizeof(RichString));
   this->chptr = this->chstr;
}

void RichString_setAttr(RichString* this, int attrs) {
   RichString_setAttrn(this, attrs, 0, this->chlen - 1);
}

void RichString_append(RichString* this, int attrs, const char* data) {
   RichString_writeFrom(this, attrs, data, this->chlen, strlen(data));
}

void RichString_appendn(RichString* this, int attrs, const char* data, int len) {
   RichString_writeFrom(this, attrs, data, this->chlen, len);
}

void RichString_write(RichString* this, int attrs, const char* data) {
   RichString_writeFrom(this, attrs, data, 0, strlen(data));
}
