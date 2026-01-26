// -------------------------------------------------------------
//  Cubzh Core
//  animation.c
//  Created by Arthur Cormerais on February 11, 2021.
// -------------------------------------------------------------

#include "animation.h"

#include <string.h>

#include "config.h"
#include "keyframes.h"

struct _Animation {
    // all keyframes groups are evaluated and stepped together
    DoublyLinkedList *groups;

    char *name;

    // animation duration can be changed at any time
    float duration; /* 4 bytes */
    float timer;    /* 4 bytes */

    bool removeWhenDone; /* 1 byte */
    uint8_t priority;    /* 1 byte */
    bool reverse;        /* 1 byte */ // true: starts from end frame // TODO: implement
    bool mirror;         /* 1 byte */ // true: plays to the end and back // TODO: implement
    bool active;         /* 1 byte */
    bool side;           /* 1 byte */
    uint8_t count;       /* 1 byte */ // 1: plays once, 2: plays twice, 0: plays indefinitely
    uint8_t counter;     /* 1 byte */
};

// MARK: - Lifecycle -

Animation *animation_new(void) {
    Animation *a = (Animation *)malloc(sizeof(Animation));

    a->groups = doubly_linked_list_new();
    a->duration = 1.0f;
    a->timer = 0.0f;
    a->removeWhenDone = true;
    a->priority = 0;
    a->reverse = false;
    a->mirror = false;
    a->active = false;
    a->side = true;
    a->count = 1; // loop once by default
    a->name = NULL;

    return a;
}

Animation *animation_new_copy(Animation *a) {
    Animation *copy = animation_new();

    DoublyLinkedListNode *n = doubly_linked_list_first(a->groups);
    while (n != NULL) {
        animation_add(copy, keyframes_new_copy(doubly_linked_list_node_pointer(n)));
        n = doubly_linked_list_node_next(n);
    }
    copy->duration = a->duration;
    a->removeWhenDone = a->removeWhenDone;
    a->priority = a->priority;
    copy->reverse = a->reverse;
    copy->mirror = a->mirror;
    copy->count = a->count;
    copy->name = string_new_copy(a->name);

    return copy;
}

void animation_free(Animation *a) {
    doubly_linked_list_flush(a->groups, keyframes_free_func);
    doubly_linked_list_free(a->groups);
    if (a->name != NULL) {
        free(a->name);
    }

    free(a);
}

void animation_tick(Animation *a, float dt) {
    if (a == NULL) {
        return;
    }
    if (a->active == false) {
        return;
    }

    float inte;
    const float dv = modff((a->side ? 1.0f : -1.0f) * dt / a->duration, &inte);
    DoublyLinkedListNode *n = doubly_linked_list_first(a->groups);
    while (n != NULL) {
        keyframes_step(doubly_linked_list_node_pointer(n), dv);
        n = doubly_linked_list_node_next(n);
    }

    if (a->timer < a->duration) {
        a->timer += dt;
    } else {
        return;
    }

    if (a->timer > a->duration) {
        // TODO: handle mirror & reverse
        if (a->count == 0) { // loop indefinitely
            animation_evaluate(a, 0.0f);
        } else  {
            a->counter++;
            if (a->counter >= a->count) {
                if (a->removeWhenDone) {
                    animation_stop(a, true);
                }
            }
        }
    }
}

// MARK: - Timing -

void animation_play(Animation * const a) {
    a->active = true;
    a->side = true;
    a->counter = 0;

    animation_evaluate(a, 0.0f);
}

void animation_play_reverse(Animation * const a) {
    a->active = true;
    a->side = false;
    a->counter = 0;

    animation_evaluate(a, 1.0f);
}

void animation_pause(Animation * const a) {
    a->active = false;
}

void animation_stop(Animation * const a, const bool reset) {
    if (a->active == false) {
        return;
    }
    a->active = false;
    if (reset) {
        if (a->mirror) {
            animation_evaluate(a, a->side ? 0.0f : 1.0f);
        } else {
            animation_evaluate(a, a->side ? 1.0f : 0.0f);
        }
    }
}

bool animation_is_playing(Animation * const a) {
    if (a == NULL) {
        return false;
    }
    return a->active;
}

void animation_evaluate(Animation * const a, const float v) {
    if (float_isEqual(a->timer, v, EPSILON_ZERO)) {
        return;
    }

    const float cv = CLAMP01F(v);
    a->timer = (a->side ? cv : (1 - cv)) * a->duration;

    DoublyLinkedListNode *n = doubly_linked_list_first(a->groups);
    while (n != NULL) {
        keyframes_evaluate(doubly_linked_list_node_pointer(n), cv);
        n = doubly_linked_list_node_next(n);
    }
}

void animation_set_duration(Animation * const a, const float value) {
    a->duration = value;
}

void animation_set_speed(Animation * const a, const float value) {
    a->duration = 1 / value;
}

void animation_set_count(Animation * const a, const uint8_t value) {
    a->count = value;
}

void animation_set_name(Animation * const a, const char * value) {
    a->name = string_new_copy(value);
}

float animation_get_duration(Animation * const a) {
    return a->duration;
}

float animation_get_speed(Animation * const a) {
    return 1 / a->duration;
}

uint8_t animation_get_count(Animation * const a) {
    return a->count;
}

char *animation_get_name(Animation * const a) {
    return a->name;
}

void animation_set_priority(Animation * const a, const uint8_t value) {
    a->priority = value;
}

uint8_t animation_get_priority(Animation * const a) {
    return a->priority;
}

void animation_set_remove_when_done(Animation * const a, const bool b) {
    a->removeWhenDone = b;
}

bool animation_get_remove_when_done(Animation * const a) {
    return a->removeWhenDone;
}

// MARK: - Keyframes -

void animation_add(Animation *a, Keyframes *k) {
    doubly_linked_list_push_last(a->groups, k);
}

void animation_clear(Animation *a) {
    doubly_linked_list_flush(a->groups, keyframes_free_func);
}

Keyframes *animation_get_keyframes(Animation *a, const char *name) {
    if (a == NULL) {
        return NULL;
    }
    DoublyLinkedListNode *n = doubly_linked_list_first(a->groups);
    Keyframes *k = NULL;
    const char *kn = NULL;
    while (n != NULL) {
        k = doubly_linked_list_node_pointer(n);
        kn = keyframes_get_name(k);
        if (strcmp(name, kn) == 0) {
            return k;
        }
        n = doubly_linked_list_node_next(n);
    }
    return NULL;
}

void animation_get_keyframes_group_names(Animation *a, const char ***names, size_t *len) {
    if (a == NULL || names == NULL || *names != NULL || len == NULL) { return; }

    size_t _len = doubly_linked_list_node_count(a->groups);
    *names = (const char **)malloc(sizeof(const char*) * _len);

    DoublyLinkedListNode *n = doubly_linked_list_first(a->groups);
    size_t i = 0;
    Keyframes *k = NULL;
    while (n != NULL) {
        k = doubly_linked_list_node_pointer(n);
        (*names)[i] = keyframes_get_name(k);
        ++i;
        n = doubly_linked_list_node_next(n);
    }

    *len = _len;
}

void animation_toggle_keyframes(Animation *a, const char *name, bool value, float evaluate) {
    DoublyLinkedListNode *n = doubly_linked_list_first(a->groups);
    Keyframes *k = NULL;
    const char *kn = NULL;
    while (n != NULL) {
        k = doubly_linked_list_node_pointer(n);
        kn = keyframes_get_name(k);
        if (strcmp(name, kn) == 0) {
            keyframes_toggle(k, value);
            if (evaluate > 0) {
                keyframes_evaluate(k, evaluate);
            }
            return;
        }
        n = doubly_linked_list_node_next(n);
    }
}
