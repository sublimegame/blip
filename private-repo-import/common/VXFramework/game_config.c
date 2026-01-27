// -------------------------------------------------------------
//  Cubzh Core
//  game_config.c
//  Created by Adrien Duermael on February 18, 2021.
// -------------------------------------------------------------

#include "game_config.h"

#include <ctype.h>
#include <stdlib.h>

#include "config.h"
#include "game_item.h"
#include "vxlog.h"

struct _GameConfig {

    // Imported game items
    Items *items;

    // Map used by the game
    Item *map;

    /// mapPaletteOverrides contains a list of overriden colors and emissivities
    //PaletteOverrides *mapPaletteOverrides;
};

GameConfig *game_config_new(void) {

    GameConfig *config = (GameConfig *)malloc(sizeof(GameConfig));

    if (config == NULL) {
        return NULL;
    }

    config->items = items_new();

    config->map = NULL;

    return config;
}

void game_config_reset(GameConfig *config) {
    // NOTE: some fields are referenced within Lua tables,
    // do not destroy them here, pointers should remain the same,
    // just reset values.

    game_config_items_flush(config);

    if (config->map != NULL) {
        item_release(config->map);
        config->map = NULL;
    }
}

void game_config_free(GameConfig *config) {
    if (config == NULL)
        return;

    items_free(config->items);

    if (config->map != NULL) {
        item_release(config->map);
    }

    free(config);
}

int game_config_items_len(const GameConfig *config) {
    if (config == NULL) {
        return 0;
    }
    return (int)items_count(config->items);
}

Item *game_config_items_get_at(const GameConfig *config, const int i) {
    if (config == NULL) {
        return NULL;
    }
    return items_get_at(config->items, i);
}

Item *game_config_items_set_at(const GameConfig *config, const char *fullname, const int i) {
    if (config == NULL) {
        return NULL;
    }
    return items_set_at(config->items, fullname, i);
}

Item *game_config_items_append(GameConfig *config, const char *fullname) {
    if (config == NULL) {
        return NULL;
    }
    return items_add(config->items, fullname);
}

void game_config_items_flush(GameConfig *config) {
    if (config == NULL) {
        return;
    }
    items_free(config->items);
    config->items = items_new();
}

Item *game_config_set_map(GameConfig *config, const char *fullname) {
    if (config == NULL) {
        return NULL;
    }

    if (config->map != NULL) {
        item_release(config->map);
        config->map = NULL;
    }

    config->map = item_make(fullname);
    return config->map;
}

Item *game_config_get_map(const GameConfig *config) {
    if (config == NULL) {
        return NULL;
    }
    if (config->map == NULL)
        return NULL;
    return config->map;
}
