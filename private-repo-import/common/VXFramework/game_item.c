// -------------------------------------------------------------
//  Cubzh Core
//  game_item.c
//  Created by Adrien Duermael on September 12, 2021.
// -------------------------------------------------------------

#include "game_item.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "utils.h"
#include "vxlog.h"

void item_release(Item *item) {
    if (item == NULL)
        return;
    --item->refCount;

    if (item->refCount == 0) {
        free(item->repo);
        free(item->name);
        free(item->tag);
        free(item->fullname);
        free(item->relPath);
        free(item);
    }
}

void item_retain(Item *item) {
    if (item == NULL)
        return;
    ++item->refCount;
}

// used in item_new
void _item_set_invalid(Item *item) {
    if (item->repo != NULL)
        free(item->repo);
    if (item->name != NULL)
        free(item->name);
    if (item->tag != NULL)
        free(item->tag);
    if (item->relPath != NULL)
        free(item->relPath);
    item->repo = string_new_copy("");
    item->name = string_new_copy("");
    item->tag = string_new_copy("");
    item->relPath = string_new_copy("");
    item->valid = false;
}

Item *item_make(const char *fullname) {

    Item *item = (Item *)malloc(sizeof(Item));
    if (item == NULL)
        return NULL;

    item->repo = NULL;
    item->name = NULL;
    item->tag = NULL;
    item->relPath = NULL;

    item->loaded = false;
    item->valid = true;
    item->refCount = 1;

    if (fullname == NULL) {
        item->fullname = string_new_copy("");
        _item_set_invalid(item);
        return item;
    }

    item->fullname = string_new_copy(fullname);

    // check if fullname is of this form:
    // [a-z][a-z0-9_]*.[a-z][a-z0-9_]*:[a-z][a-z0-9]

    // check / sanitize
    size_t len = strlen(item->fullname);
    if (len == 0) {
        _item_set_invalid(item);
        return item;
    }

    size_t i = 0;
    char *cursor = item->fullname + i;

    char *repoStart = cursor;
    char *repoEnd = cursor;

    char *nameStart = cursor;
    char *nameEnd = cursor;

    char *tagStart = NULL;
    char *tagEnd = NULL;

    // first char (lowercase alpha)
    if (isalpha(*cursor) == false) {
        _item_set_invalid(item);
        return item;
    }
    *cursor = tolower(*cursor);
    ++i;

    bool foundSeparator = false;

    // to the end of repo or name (lowercase alpha or digit)
    while (i < len) {
        cursor = item->fullname + i;

        if (isalpha(*cursor) || isdigit(*cursor) || *cursor == '_') {
            *cursor = tolower(*cursor);
            ++i;
        } else if (*cursor == '.') {
            repoEnd = cursor;
            foundSeparator = true;
            ++i;
            break;
        } else if (*cursor == ':') {
            nameEnd = cursor;
            foundSeparator = true;
            ++i;
            break;
        } else {
            _item_set_invalid(item);
            return item;
        }
    }

    // reached the end, just a name
    if (i == len) {
        // error
        // repo. (without name) or name: (without tag)
        if (foundSeparator) {
            _item_set_invalid(item);
            return item;
        }

        nameEnd = item->fullname + i;

        item->repo = string_new_copy("official");
        item->name = string_new_substring(nameStart, nameEnd);
        item->tag = string_new_copy("");

        item->relPath = string_new_join(item->repo, "/", item->name, ".3zh", NULL);

        return item;
    }

    // found tag separator (official repo item with tag)
    if (nameEnd > nameStart) {
        cursor = item->fullname + i;
        tagStart = cursor;

        // first char (lowercase alpha)
        cursor = item->fullname + i;
        if (isalpha(*cursor) == false) {
            _item_set_invalid(item);
            return item;
        }
        *cursor = tolower(*cursor);
        ++i;

        // to the end of tag (lowercase alpha or digit)
        while (i < len) {
            cursor = item->fullname + i;

            if (isalpha(*cursor) || isdigit(*cursor)) {
                *cursor = tolower(*cursor);
                ++i;
            } else {
                _item_set_invalid(item);
                return item;
            }
        }

        tagEnd = item->fullname + i;

        item->repo = string_new_copy("official");
        item->name = string_new_substring(nameStart, nameEnd);
        item->tag = string_new_substring(tagStart, tagEnd);

        item->relPath =
            string_new_join(item->repo, "/", item->name, ".", item->tag, ".3zh", NULL);

        return item;
    }

    // not supposed to happen, if we get here, it means a repo was parsed
    if (repoEnd == repoStart) {
        _item_set_invalid(item);
        return item;
    }

    // PARSED REPO

    // parse name
    if (i >= len) {
        // "repo." (nothing after repo name)
        _item_set_invalid(item);
        return item;
    }

    cursor = item->fullname + i;
    nameStart = cursor;
    nameEnd = nameStart;

    // first char (lowercase alpha)
    cursor = item->fullname + i;
    if (isalpha(*cursor) == false) {
        _item_set_invalid(item);
        return item;
    }
    *cursor = tolower(*cursor);
    ++i;

    foundSeparator = false;

    // to the end of name (lowercase alpha, digit or _)
    while (i < len) {
        cursor = item->fullname + i;

        if (isalpha(*cursor) || isdigit(*cursor) || *cursor == '_') {
            *cursor = tolower(*cursor);
            ++i;
        } else if (*cursor == ':') {
            nameEnd = cursor;
            foundSeparator = true;
            ++i;
            break;
        } else {
            _item_set_invalid(item);
            return item;
        }
    }

    // reached the end: repo.name
    if (i == len) {
        // error
        // repo.name: (without tag)
        if (foundSeparator) {
            _item_set_invalid(item);
            return item;
        }

        nameEnd = item->fullname + i;

        item->repo = string_new_substring(repoStart, repoEnd);
        item->name = string_new_substring(nameStart, nameEnd);
        item->tag = string_new_copy("");

        item->relPath = string_new_join(item->repo, "/", item->name, ".3zh", NULL);

        return item;
    }

    // not supposed to happen, if we get here, it means a name was parsed
    if (nameEnd == nameStart) {
        _item_set_invalid(item);
        return item;
    }

    // PARSED NAME

    // parse tag
    if (i >= len) {
        // "repo.name:" (nothing after name)
        _item_set_invalid(item);
        return item;
    }

    cursor = item->fullname + i;
    tagStart = cursor;

    // first char (lowercase alpha)
    cursor = item->fullname + i;
    if (isalpha(*cursor) == false) {
        _item_set_invalid(item);
        return item;
    }
    *cursor = tolower(*cursor);
    ++i;

    // to the end of tag (lowercase alpha or digit)
    while (i < len) {
        cursor = item->fullname + i;

        if (isalpha(*cursor) || isdigit(*cursor)) {
            *cursor = tolower(*cursor);
            ++i;
        } else {
            _item_set_invalid(item);
            return item;
        }
    }

    tagEnd = item->fullname + i;

    item->repo = string_new_substring(repoStart, repoEnd);
    item->name = string_new_substring(nameStart, nameEnd);
    item->tag = string_new_substring(tagStart, tagEnd);

    item->relPath =
        string_new_join(item->repo, "/", item->name, ".", item->tag, ".3zh", NULL);

    return item;
}

struct _Items {
    size_t count;
    size_t allocated;
    Item **items;
};

Items *items_new(void) {
    Items *items = (Items *)malloc(sizeof(Items));
    if (items == NULL)
        return NULL;

    items->count = 0;
    items->allocated = 1;
    items->items = (Item **)malloc(sizeof(Item *) * items->allocated);

    return items;
}

void items_free(Items *items) {
    if (items == NULL)
        return;
    for (size_t i = 0; i < items->count; ++i) {
        item_release(items->items[i]);
    }
    free(items->items);
    free(items);
}

Item *items_set_at(Items *items, const char *fullname, size_t i) {
    if (i >= items->count) {
        return NULL;
    }

    Item *item = item_make(fullname);
    if (item == NULL)
        return NULL;

    item_release(items->items[i]);
    items->items[i] = item;

    return item;
}

Item *items_add(Items *items, const char *fullname) {
    Item *item = item_make(fullname);
    if (item == NULL)
        return NULL;

    ++items->count;
    if (items->count > items->allocated) {
        items->allocated = items->count;
        items->items = (Item **)realloc(items->items, sizeof(Item *) * items->allocated);
    }

    items->items[items->count - 1] = item;

    return item;
}

size_t items_count(const Items *items) {
    return items->count;
}

Item *items_get_at(const Items *items, size_t i) {
    if (i >= items->count) {
        return NULL;
    }
    return items->items[i];
}

Item *items_get_with_repo_and_name(const Items *items, const char *repo, const char *name) {
    return NULL;
}
