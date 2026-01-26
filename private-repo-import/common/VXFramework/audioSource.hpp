//
//  audioSource.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 19/08/2022.
//

#pragma once

class AudioSource;

#ifndef P3S_CLIENT_HEADLESS

#include <stdint.h>
#include "transform.h"

#ifdef __cplusplus
extern "C" {
#endif

AudioSource *audioSource_new(void);
void audioSource_free(AudioSource * const as);

void audioSource_pause(const AudioSource * const as);
void audioSource_play(const AudioSource * const as);
void audioSource_stop(const AudioSource * const as);

// accessors / modifiers

bool audioSource_isPlaying(AudioSource * const as);

float audioSource_getPan(AudioSource * const as);
void audioSource_setPan(AudioSource * const as, const float pan);

float3 audioSource_getPosition(AudioSource * const as);
void audioSource_setPosition(AudioSource * const as, const float3 position);

const char *audioSource_getSoundName(AudioSource * const as);
void audioSource_setSound(AudioSource * const as, const char * soundName);

void audioSource_setSoundBuffer(AudioSource * const as, const void *bufferPtr, const size_t bufferLen);

bool audioSource_getSpatialized(AudioSource * const as);
void audioSource_setSpatialized(AudioSource * const as, const bool spatialized);

bool audioSource_getLooping(AudioSource *const as);
void audioSource_setLooping(AudioSource *const as, const bool looping);

bool audioSource_getStartAtEnabled(AudioSource *const as);
float audioSource_getStartAt(AudioSource *const as);
void audioSource_disableStartAt(AudioSource *const as);
bool audioSource_setStartAt(AudioSource *const as, const float s);

bool audioSource_getStopAtEnabled(AudioSource *const as);
float audioSource_getStopAt(AudioSource * const as);
void audioSource_disableStopAt(AudioSource *const as);
bool audioSource_setStopAt(AudioSource *const as, const float s);

float audioSource_getVolume(AudioSource * const as);
void audioSource_setVolume(AudioSource * const as, const float volume);

float audioSource_getOriginalDuration(AudioSource *const as);

float audioSource_getRadius(AudioSource * const as);
void audioSource_setRadius(AudioSource * const as, const float radius);

float audioSource_getMinRadius(AudioSource * const as);
void audioSource_setMinRadius(AudioSource * const as, const float radius);

float audioSource_getPitch(AudioSource *const as);
void audioSource_setPitch(AudioSource *const as, const float pitch);

Transform *audioSource_getTransform(const AudioSource * const as);

void audioSource_refresh(AudioSource * const as);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
