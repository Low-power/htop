/*
htop - CheckItem.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "CheckItem.h"

#include "CRT.h"

#include <assert.h>
#include <stdlib.h>

/*{
#include "Object.h"

#include <stdbool.h>

typedef struct CheckItem_ {
   Object super;
   char* text;
   bool* ref;
   bool value;
} CheckItem;

}*/

static void CheckItem_delete(Object* cast) {
   CheckItem* this = (CheckItem*)cast;
   assert (this != NULL);

   free(this->text);
   free(this);
}

static void CheckItem_display(Object* cast, RichString* out) {
   CheckItem* this = (CheckItem*)cast;
   assert (this != NULL);
   RichString_write(out, CRT_colors[HTOP_CHECK_BOX_COLOR], "[");
   RichString_append(out, CRT_colors[HTOP_CHECK_MARK_COLOR], CheckItem_get(this) ? "x" : " ");
   RichString_append(out, CRT_colors[HTOP_CHECK_BOX_COLOR], "] ");
   RichString_append(out, CRT_colors[HTOP_CHECK_TEXT_COLOR], this->text);
}

ObjectClass CheckItem_class = {
   .display = CheckItem_display,
   .delete = CheckItem_delete
};

CheckItem* CheckItem_newByRef(char* text, bool* ref) {
   CheckItem* this = AllocThis(CheckItem);
   this->text = text;
   this->value = false;
   this->ref = ref;
   return this;
}

CheckItem* CheckItem_newByVal(char* text, bool value) {
   CheckItem* this = AllocThis(CheckItem);
   this->text = text;
   this->value = value;
   this->ref = NULL;
   return this;
}

void CheckItem_set(CheckItem* this, bool value) {
   if (this->ref) 
      *(this->ref) = value;
   else
      this->value = value;
}

bool CheckItem_get(CheckItem* this) {
   if (this->ref) 
      return *(this->ref);
   else
      return this->value;
}
