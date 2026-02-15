// -------------------------------------------------------------
//  Cubzh Core
//  game_config.h
//  Created by Adrien Duermael on February 18, 2021.
// -------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "colors.h"
#include "game_item.h"

typedef struct _GameConfig GameConfig;

// allocates new game configuration
GameConfig *game_config_new(void);

void game_config_reset(GameConfig *config);

void game_config_free(GameConfig *config);

// returns nb items
int game_config_items_len(const GameConfig *config);
Item *game_config_items_get_at(const GameConfig *config, const int i);
// sets item at given position, doesn't do anything if the array has to
// be increased more than 1 element (returns false in that case)
Item *game_config_items_set_at(const GameConfig *config, const char *fullname, const int i);
Item *game_config_items_append(GameConfig *config, const char *fullname);
void game_config_items_flush(GameConfig *config);

Item *game_config_set_map(GameConfig *config, const char *fullname);
Item *game_config_get_map(const GameConfig *config);

#ifdef __cplusplus
} // extern "C"
#endif
