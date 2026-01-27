// -------------------------------------------------------------
//  Cubzh Core
//  game_item.h
//  Created by Adrien Duermael on September 12, 2021.
// -------------------------------------------------------------

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

typedef struct _Item Item;
struct _Item {
    char *repo;
    char *name;
    char *tag;
    char *fullname;
    char *relPath;
    int refCount;
    bool loaded;
    bool valid;
};

void item_retain(Item *item);
void item_release(Item *item);
Item *item_make(const char *fullname);

typedef struct _Items Items;

Items *items_new(void);
void items_free(Items *items);
Item *items_set_at(Items *items, const char *fullname, size_t i);

Item *items_add(Items *items, const char *fullname);
size_t items_count(const Items *items);
Item *items_get_at(const Items *items, size_t i);
Item *items_get_with_repo_and_name(const Items *items, const char *repo, const char *name);

#ifdef __cplusplus
} // extern "C"
#endif
