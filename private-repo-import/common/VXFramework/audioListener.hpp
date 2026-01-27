//
//  audioListener.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 26/08/2022.
//

#pragma once

typedef struct _AudioListener AudioListener;

#ifndef P3S_CLIENT_HEADLESS

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "transform.h"

AudioListener *audioListener_new(void);
void audioListener_free(AudioListener * const al);

// accessors / modifiers

float3 audioListener_getPosition(AudioListener * const al);
void audioListener_setPosition(AudioListener * const al, const float3 position);

float3 audioListener_getDirection(AudioListener * const al);
void audioListener_setDirection(AudioListener * const al, const float3 direction);

Transform *audioListener_getTransform(const AudioListener * const al);

void audioListener_refresh(AudioListener * const al);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
