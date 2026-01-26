// -------------------------------------------------------------
//  Cubzh Core
//  keyframes.c
//  Created by Arthur Cormerais on February 11, 2021.
// -------------------------------------------------------------

#include "keyframes.h"

#include <math.h>

#include "config.h"
#include "vxlog.h"
#include "quaternion.h"
#include "utils.h"
#include "weakptr.h"

struct _Keyframes {
    // the transform these keyframes evaluate onto, can be changed at any time
    Weakptr *transform;

    // you may give a name to this keyframes group
    const char *name;

    // list of keyframes, they will be evaluated in order
    DoublyLinkedList *keyframes;

    // current node
    DoublyLinkedListNode *current;

    // current evaluation, normalized value
    float v; /* 4 bytes */

    // keyframes count
    uint16_t count; /* 2 bytes */

    // keyframes have to be normalized before evaluating (unless the caller is aware of the total
    // weight)
    bool isNormalized; /* 1 byte */

    // keyframes group may be toggled individually inside an animation
    bool isEnabled; /* 1 byte */
};

struct _KeyframeData {
    float3 *position;
    float3 *rotation;
    float3 *scale;
    pointer_easing_func easing;

    // weight can be thought of as a frame number, or a cumulated duration,
    // that is normalized later when done editing
    float weight; /* 4 bytes */

    char pad[4];
};

// MARK: - Private functions -

bool _keyframes_is_empty_or_disabled(Keyframes *k) {
    return k->transform == NULL || weakptr_get(k->transform) == NULL || k->count == 0 || k->isEnabled == false;
}

bool _keyframe_is_empty(KeyframeData *kd) {
    return kd->position == NULL && kd->rotation == NULL && kd->scale == NULL;
}

void _keyframes_fetch_current(Keyframes *k, float v) {
    DoublyLinkedListNode *n = doubly_linked_list_last(k->keyframes);
    KeyframeData *kd = NULL;
    while (n != NULL) {
        kd = (KeyframeData *)doubly_linked_list_node_pointer(n);
        if (v >= kd->weight) {
            break;
        }
        n = doubly_linked_list_node_previous(n);
    }
    k->current = n;
    k->v = v;
}

void _keyframes_write_transform_lerp(Transform *t, KeyframeData *kd1, KeyframeData *kd2, float v) {
    if (kd1 == NULL || kd2 == NULL) {
        return;
    }

    const float ev = kd1 != NULL && kd1->easing != NULL ? kd1->easing(v) : v;

    if (kd1->position != NULL || kd2->position != NULL) {
        if (kd1->position == NULL) {
            transform_set_local_position_vec(t, kd2->position);
        } else if (kd2->position == NULL) {
            transform_set_local_position_vec(t, kd1->position);
        } else {
            transform_set_local_position(t,
                                         LERP(kd1->position->x, kd2->position->x, ev),
                                         LERP(kd1->position->y, kd2->position->y, ev),
                                         LERP(kd1->position->z, kd2->position->z, ev));
        }
    }

    if (kd1->rotation != NULL || kd2->rotation != NULL) {
        if (kd1->rotation == NULL) {
            transform_set_local_rotation_euler_vec(t, kd2->rotation);
        } else if (kd2->rotation == NULL) {
            transform_set_local_rotation_euler_vec(t, kd1->rotation);
        } else {
            Quaternion q1;
            euler_to_quaternion_vec(kd1->rotation, &q1);
            Quaternion q2;
            euler_to_quaternion_vec(kd2->rotation, &q2);
            Quaternion l;
            quaternion_op_lerp(&q1, &q2, &l, ev);
            float3 r;
            quaternion_to_euler(&l, &r);
            transform_set_local_rotation_euler_vec(t, &r);
        }
    }

    if (kd1->scale != NULL || kd2->scale != NULL) {
        if (kd1->scale == NULL) {
            transform_set_local_scale_vec(t, kd2->scale);
        } else if (kd2->scale == NULL) {
            transform_set_local_scale_vec(t, kd1->scale);
        } else {
            transform_set_local_scale(t,
                                      LERP(kd1->scale->x, kd2->scale->x, ev),
                                      LERP(kd1->scale->y, kd2->scale->y, ev),
                                      LERP(kd1->scale->z, kd2->scale->z, ev));
        }
    }
}

void _keyframes_write_transform_set(Transform *t, KeyframeData *kd) {
    if (kd == NULL) {
        return;
    }

    if (kd->position != NULL) {
        transform_set_local_position_vec(t, kd->position);
    }

    if (kd->rotation != NULL) {
        transform_set_local_rotation_euler_vec(t, kd->rotation);
    }

    if (kd->scale != NULL) {
        transform_set_local_scale_vec(t, kd->scale);
    }
}

void _keyframes_write_transform(Transform *t, KeyframeData *kd1, KeyframeData *kd2, float v) {
    if (kd1 != NULL && kd2 != NULL) {
        _keyframes_write_transform_lerp(t,
                                        kd1,
                                        kd2,
                                        (v - kd1->weight) / (kd2->weight - kd1->weight));
    } else if (kd1 != NULL) {
        _keyframes_write_transform_set(t, kd1);
    } else if (kd2 != NULL) {
        _keyframes_write_transform_set(t, kd2);
    }
}

bool _keyframes_sort_func(DoublyLinkedListNode *n1, DoublyLinkedListNode *n2) {
    return ((KeyframeData *)doubly_linked_list_node_pointer(n1))->weight >
           ((KeyframeData *)doubly_linked_list_node_pointer(n2))->weight;
}

// MARK: - Lifecycle -

Keyframes *keyframes_new(Transform *t, const char *name) {
    Keyframes *k = (Keyframes *)malloc(sizeof(Keyframes));

    k->transform = t != NULL ? transform_get_and_retain_weakptr(t) : NULL;
    k->name = name;
    k->keyframes = doubly_linked_list_new();
    k->current = NULL;
    k->v = 0;
    k->count = 0;
    k->isNormalized = true;
    k->isEnabled = true;

    return k;
}

Keyframes *keyframes_new_copy(Keyframes *k) {
    Keyframes *copy = keyframes_new(weakptr_get(k->transform), k->name);

    DoublyLinkedListNode *n = doubly_linked_list_first(k->keyframes);
    while (n != NULL) {
        keyframes_add(copy, keyframe_data_new_copy(doubly_linked_list_node_pointer(n)));
        n = doubly_linked_list_node_next(n);
    }

    return copy;
}

void keyframes_free(Keyframes *k) {
    doubly_linked_list_flush(k->keyframes, keyframe_data_free_func);
    doubly_linked_list_free(k->keyframes);
    weakptr_release(k->transform);

    free(k);
}

void keyframes_free_func(void *k) {
    keyframes_free((Keyframes *)k);
}

void keyframes_set_transform(Keyframes *k, Transform *t) {
    if (k->transform != NULL) {
        weakptr_release(k->transform);
    }
    k->transform = t != NULL ? transform_get_and_retain_weakptr(t) : NULL;
}

Transform *keyframes_get_transform(const Keyframes *k) {
    return weakptr_get(k->transform);
}

const char *keyframes_get_name(const Keyframes *k) {
    return k->name;
}

void keyframes_toggle(Keyframes *k, bool value) {
    k->isEnabled = value;
}

/// MARK: - Keyframes -

void keyframes_add(Keyframes *k, KeyframeData *kd) {
    doubly_linked_list_push_last(k->keyframes, kd);
    k->count++;
    k->isNormalized = false;
}

void keyframes_add_new(Keyframes *k,
                       const float weight,
                       const float3 *pos,
                       const float3 *rot,
                       const float3 *scale,
                       pointer_easing_func f) {
    KeyframeData *kd = keyframe_data_new(weight);
    if (pos != NULL) {
        keyframe_data_set_position(kd, pos);
    }
    if (rot != NULL) {
        keyframe_data_set_rotation(kd, rot);
    }
    if (scale != NULL) {
        keyframe_data_set_scale(kd, scale);
    }
    if (f != NULL) {
        keyframe_data_set_easing(kd, f);
    }
    keyframes_add(k, kd);
}

void keyframes_clear(Keyframes *k) {
    doubly_linked_list_flush(k->keyframes, keyframe_data_free_func);
    k->count = 0;
    k->isNormalized = true;
}

uint16_t keyframes_get_count(Keyframes *k) {
    return k->count;
}

DoublyLinkedListNode *keyframes_get_iterator(Keyframes *k) {
    return doubly_linked_list_first(k->keyframes);
}

void keyframes_normalize(Keyframes *k) {
    if (k->isNormalized) {
        return;
    }

    // sort keyframes in ascending weight order
    doubly_linked_list_sort_ascending(k->keyframes, _keyframes_sort_func);

    // normalize weights, total weight is given by the last keyframe
    DoublyLinkedListNode *n = doubly_linked_list_last(k->keyframes);
    KeyframeData *kd = (KeyframeData *)doubly_linked_list_node_pointer(n);
    const float total = kd->weight;
    n = doubly_linked_list_first(k->keyframes);
    while (n != NULL) {
        kd = (KeyframeData *)doubly_linked_list_node_pointer(n);
        kd->weight /= total;
        n = doubly_linked_list_node_next(n);
    }
    k->isNormalized = true;
}

bool keyframes_is_normalized(Keyframes *k) {
    return k->isNormalized;
}

void keyframes_evaluate(Keyframes *k, float v) {
    if (_keyframes_is_empty_or_disabled(k)) {
        return;
    }
    Transform *t = (Transform*)weakptr_get(k->transform);

    // if animations are disabled on the Transform, then do nothing
    if (transform_is_animations_enabled(t) == false) {
        return;
    }

    if (keyframes_is_normalized(k) == false) {
        keyframes_normalize(k);
    }

    /// set current node
    _keyframes_fetch_current(k, v);

    /// write transform
    KeyframeData *kd1 = NULL, *kd2 = NULL;
    if (k->current == NULL) {
        kd2 = doubly_linked_list_node_pointer(doubly_linked_list_first(k->keyframes));
    } else {
        kd1 = doubly_linked_list_node_pointer(k->current);
        DoublyLinkedListNode *n2 = doubly_linked_list_node_next(k->current);
        kd2 = n2 != NULL ? doubly_linked_list_node_pointer(n2) : NULL;
    }
    _keyframes_write_transform(t, kd1, kd2, k->v);
}

void keyframes_step(Keyframes *k, float dv) {
    if (_keyframes_is_empty_or_disabled(k)) {
        return;
    }
    Transform *t = (Transform*)weakptr_get(k->transform);

    // if animations are disabled on the Transform, then do nothing
    if (transform_is_animations_enabled(t) == false) {
        return;
    }

    if (keyframes_is_normalized(k) == false) {
        keyframes_normalize(k);
    }

    k->v = CLAMP01F(k->v + dv);

    DoublyLinkedListNode *n1 =
        k->current != NULL ? k->current : doubly_linked_list_first(k->keyframes);
    KeyframeData *kd1 = doubly_linked_list_node_pointer(n1);
    DoublyLinkedListNode *n2 = NULL;
    KeyframeData *kd2 = NULL;

    /// stepping backward
    while (kd1 != NULL && k->v < kd1->weight) {
        n1 = doubly_linked_list_node_previous(n1);
        kd1 = n1 != NULL ? doubly_linked_list_node_pointer(n1) : NULL;
        k->current = n1;
    }

    /// stepping forward
    n2 = n1 != NULL ? doubly_linked_list_node_next(n1) : doubly_linked_list_first(k->keyframes);
    if (n2 != NULL) {
        kd2 = doubly_linked_list_node_pointer(n2);
    }
    while (kd2 != NULL && k->v > kd2->weight) {
        k->current = n2;
        n2 = doubly_linked_list_node_next(n2);
        kd1 = kd2;
        kd2 = n2 != NULL ? doubly_linked_list_node_pointer(n2) : NULL;
    }

    /// write transform
    // kd1 or kd2 may be null, if there is a single keyframe or if there is no keyframe at 0 or 1
    _keyframes_write_transform(t, kd1, kd2, k->v);
}

/// MARK: - Keyframe Data -

KeyframeData *keyframe_data_new(const float weight) {
    KeyframeData *kd = (KeyframeData *)malloc(sizeof(KeyframeData));

    kd->position = NULL;
    kd->rotation = NULL;
    kd->scale = NULL;
    kd->easing = NULL;
    kd->weight = weight;

    return kd;
}

KeyframeData *keyframe_data_new_copy(KeyframeData *kd) {
    KeyframeData *copy = keyframe_data_new(kd->weight);

    keyframe_data_set_position(copy, kd->position);
    keyframe_data_set_rotation(copy, kd->rotation);
    keyframe_data_set_scale(copy, kd->scale);
    keyframe_data_set_easing(copy, kd->easing);

    return copy;
}

void keyframe_data_free(KeyframeData *kd) {
    if (kd->position != NULL) {
        float3_free(kd->position);
        kd->position = NULL;
    }
    if (kd->rotation != NULL) {
        float3_free(kd->rotation);
        kd->rotation = NULL;
    }
    if (kd->scale != NULL) {
        float3_free(kd->scale);
        kd->scale = NULL;
    }
    kd->easing = NULL;

    free(kd);
}

void keyframe_data_free_func(void *kd) {
    keyframe_data_free((KeyframeData *) kd);
}

void keyframe_data_set_position(KeyframeData *kd, const float3 *pos) {
    if (pos != NULL) {
        if (kd->position == NULL) {
            kd->position = float3_new(pos->x, pos->y, pos->z);
        } else {
            float3_copy(kd->position, pos);
        }
    } else if (kd->position != NULL) {
        float3_free(kd->position);
        kd->position = NULL;
    }
}

float3 *keyframe_data_get_position(const KeyframeData *kd) {
    return kd->position;
}

void keyframe_data_set_rotation(KeyframeData *kd, const float3 *rot) {
    if (rot != NULL) {
        if (kd->rotation == NULL) {
            kd->rotation = float3_new(rot->x, rot->y, rot->z);
        } else {
            float3_copy(kd->rotation, rot);
        }
    } else if (kd->rotation != NULL) {
        float3_free(kd->rotation);
        kd->rotation = NULL;
    }
}
float3 *keyframe_data_get_rotation(const KeyframeData *kd) {
    return kd->rotation;
}

void keyframe_data_set_scale(KeyframeData *kd, const float3 *scale) {
    if (scale != NULL) {
        if (kd->scale == NULL) {
            kd->scale = float3_new(scale->x, scale->y, scale->z);
        } else {
            float3_copy(kd->scale, scale);
        }
    } else if (kd->scale != NULL) {
        float3_free(kd->scale);
        kd->scale = NULL;
    }
}

float3 *keyframe_data_get_scale(const KeyframeData *kd) {
    return kd->scale;
}

void keyframe_data_set_easing(KeyframeData *kd, pointer_easing_func f) {
    kd->easing = f;
}

pointer_easing_func keyframe_data_get_easing(const KeyframeData *kd) {
    return kd->easing;
}

void keyframe_data_set_weight(KeyframeData *kd, float weight) {
    kd->weight = fabsf(weight);
}

float keyframe_data_get_weight(const KeyframeData *kd) {
    return kd->weight;
}
