/*
htop - Hashtable.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPL, see the COPYING file
in the source distribution for its full text.
*/

#include "Hashtable.h"
#include "XAlloc.h"

#include <stdlib.h>
#include <assert.h>

/*{
#include <stdbool.h>

typedef struct Hashtable_ Hashtable;

typedef void(*Hashtable_PairFunction)(int, void*, void*);

typedef struct HashtableItem {
   unsigned int key;
   void* value;
   struct HashtableItem* next;
} HashtableItem;

struct Hashtable_ {
   int size;
   HashtableItem** buckets;
   int items;
   bool owner;
};
}*/

#ifndef NDEBUG

static bool Hashtable_isConsistent(const Hashtable *this) {
   int items = 0;
   for (int i = 0; i < this->size; i++) {
      const HashtableItem *bucket = this->buckets[i];
      while (bucket) {
         items++;
         bucket = bucket->next;
      }
   }
   return items == this->items;
}

int Hashtable_count(const Hashtable *this) {
   int items = 0;
   for (int i = 0; i < this->size; i++) {
      const HashtableItem *bucket = this->buckets[i];
      while (bucket) {
         items++;
         bucket = bucket->next;
      }
   }
   assert(items == this->items);
   return items;
}

#endif

static HashtableItem* HashtableItem_new(unsigned int key, void* value) {
   HashtableItem *this = xMalloc(sizeof(HashtableItem));
   this->key = key;
   this->value = value;
   this->next = NULL;
   return this;
}

Hashtable* Hashtable_new(int size, bool owner) {
   Hashtable *this = xMalloc(sizeof(Hashtable));
   this->items = 0;
   this->size = size;
   this->buckets = (HashtableItem**) xCalloc(size, sizeof(HashtableItem*));
   this->owner = owner;
   assert(Hashtable_isConsistent(this));
   return this;
}

void Hashtable_delete(Hashtable* this) {
   assert(Hashtable_isConsistent(this));
   for (int i = 0; i < this->size; i++) {
      HashtableItem* walk = this->buckets[i];
      while (walk != NULL) {
         if (this->owner)
            free(walk->value);
         HashtableItem* savedWalk = walk;
         walk = savedWalk->next;
         free(savedWalk);
      }
   }
   free(this->buckets);
   free(this);
}

void Hashtable_put(Hashtable* this, unsigned int key, void* value) {
   unsigned int index = key % this->size;
   HashtableItem** bucketPtr = &(this->buckets[index]);
   while (true)
      if (*bucketPtr == NULL) {
         *bucketPtr = HashtableItem_new(key, value);
         this->items++;
         break;
      } else if ((*bucketPtr)->key == key) {
         if (this->owner)
            free((*bucketPtr)->value);
         (*bucketPtr)->value = value;
         break;
      } else
         bucketPtr = &((*bucketPtr)->next);
   assert(Hashtable_isConsistent(this));
}

void* Hashtable_remove(Hashtable* this, unsigned int key) {
   unsigned int index = key % this->size;
   assert(Hashtable_isConsistent(this));

   HashtableItem **bucket = this->buckets + index;
   while(*bucket) {
      if ((*bucket)->key == key) {
         void* value = (*bucket)->value;
         HashtableItem* next = (*bucket)->next;
         free(*bucket);
         (*bucket) = next;
         this->items--;
         if (this->owner) {
            free(value);
            assert(Hashtable_isConsistent(this));
            return NULL;
         } else {
            assert(Hashtable_isConsistent(this));
            return value;
         }
      }
      bucket = &(*bucket)->next;
   }
   assert(Hashtable_isConsistent(this));
   return NULL;
}

inline void* Hashtable_get(const Hashtable *this, unsigned int key) {
   unsigned int index = key % this->size;
   const HashtableItem *bucketPtr = this->buckets[index];
   while (true) {
      if (bucketPtr == NULL) {
         assert(Hashtable_isConsistent(this));
         return NULL;
      } else if (bucketPtr->key == key) {
         assert(Hashtable_isConsistent(this));
         return bucketPtr->value;
      } else bucketPtr = bucketPtr->next;
   }
}

void Hashtable_foreach(const Hashtable *this, Hashtable_PairFunction f, void* userData) {
   assert(Hashtable_isConsistent(this));
   for (int i = 0; i < this->size; i++) {
      const HashtableItem *walk = this->buckets[i];
      while (walk != NULL) {
         f(walk->key, walk->value, userData);
         walk = walk->next;
      }
   }
   assert(Hashtable_isConsistent(this));
}
