// -------------------------------------------------------------
//  Cubzh Core
//  animation.h
//  Created by Arthur Cormerais on February 11, 2021.
// -------------------------------------------------------------

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// forward declarations
typedef struct _Keyframes Keyframes;
typedef struct _Animation Animation;

/// MARK: - Lifecycle -

Animation *animation_new(void);
Animation *animation_new_copy(Animation *a);
void animation_free(Animation *a);
void animation_tick(Animation *a, float dt);

/// MARK: - Timing -

void animation_play(Animation * const a);
void animation_play_reverse(Animation * const a);
void animation_pause(Animation * const a);
void animation_stop(Animation * const a, const bool reset);
bool animation_is_playing(Animation * const a);
void animation_evaluate(Animation * const a, const float v);
void animation_set_duration(Animation * const a, float value);
void animation_set_speed(Animation * const a, const float value);
void animation_set_count(Animation * const a, const uint8_t value);
void animation_set_name(Animation * const a, const char * value);
float animation_get_duration(Animation * const a);
float animation_get_speed(Animation * const a);
uint8_t animation_get_count(Animation * const a);
char *animation_get_name(Animation * const a);

void animation_set_priority(Animation * const a, const uint8_t value);
uint8_t animation_get_priority(Animation * const a);
void animation_set_remove_when_done(Animation * const a, const bool b);
bool animation_get_remove_when_done(Animation * const a);

/// MARK: - Keyframes -

void animation_add(Animation *a, Keyframes *k);
void animation_clear(Animation *a);
Keyframes *animation_get_keyframes(Animation *a, const char *name);
void animation_toggle_keyframes(Animation *a, const char *name, bool value, float evaluate);

void animation_get_keyframes_group_names(Animation *a, const char ***names, size_t *len);

#ifdef __cplusplus
} // extern "C"
#endif
