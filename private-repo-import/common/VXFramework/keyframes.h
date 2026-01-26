// -------------------------------------------------------------
//  Cubzh Core
//  keyframes.h
//  Created by Arthur Cormerais on February 11, 2021.
// -------------------------------------------------------------

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "doubly_linked_list.h"
#include "easings.h"
#include "transform.h"

typedef struct _Keyframes Keyframes;
typedef struct _KeyframeData KeyframeData;

/// MARK: - Lifecycle -

Keyframes *keyframes_new(Transform *t, const char *name);
Keyframes *keyframes_new_copy(Keyframes *k);
void keyframes_free(Keyframes *k);
void keyframes_free_func(void *k);
void keyframes_set_transform(Keyframes *k, Transform *t);
Transform *keyframes_get_transform(const Keyframes *k);
const char *keyframes_get_name(const Keyframes *k);
void keyframes_toggle(Keyframes *k, bool value);

/// MARK: - Keyframes -

void keyframes_add(Keyframes *k, KeyframeData *kd);
void keyframes_add_new(Keyframes *k,
                       const float weight,
                       const float3 *pos,
                       const float3 *rot,
                       const float3 *scale,
                       pointer_easing_func f);
void keyframes_clear(Keyframes *k);
uint16_t keyframes_get_count(Keyframes *k);
DoublyLinkedListNode *keyframes_get_iterator(Keyframes *k);
void keyframes_normalize(Keyframes *k);
bool keyframes_is_normalized(Keyframes *k);
void keyframes_evaluate(Keyframes *k, float v);
void keyframes_step(Keyframes *k, float dv);

/// MARK: - Keyframe Data -

KeyframeData *keyframe_data_new(const float weight);
KeyframeData *keyframe_data_new_copy(KeyframeData *kd);
void keyframe_data_free(KeyframeData *kd);
void keyframe_data_free_func(void *kd);
void keyframe_data_set_position(KeyframeData *kd, const float3 *pos);
float3 *keyframe_data_get_position(const KeyframeData *kd);
void keyframe_data_set_rotation(KeyframeData *kd, const float3 *rot);
float3 *keyframe_data_get_rotation(const KeyframeData *kd);
void keyframe_data_set_scale(KeyframeData *kd, const float3 *scale);
float3 *keyframe_data_get_scale(const KeyframeData *kd);
void keyframe_data_set_easing(KeyframeData *kd, pointer_easing_func f);
pointer_easing_func keyframe_data_get_easing(const KeyframeData *kd);
void keyframe_data_set_weight(KeyframeData *kd, float weight);
float keyframe_data_get_weight(const KeyframeData *kd);

#ifdef __cplusplus
} // extern "C"
#endif
