//
//  audioListener.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 26/08/2022.
//

#ifndef P3S_CLIENT_HEADLESS

#include "audioListener.hpp"

#include "vxconfig.h"

// xptools
#include "audio.hpp"

// --------------------------------------------------
// MARK: - Types -
// --------------------------------------------------

struct _AudioListener {
    Transform *_transform;

    // vx::audio
    vx::audio::Listener *_listener;

    //    char pad[5];
};

// --------------------------------------------------
// MARK: - Static functions -
// --------------------------------------------------

void _audioListener_void_free(void *o) {
    AudioListener * const al = static_cast<AudioListener *>(o);
    vx_assert(al != nullptr);
    // free fields
    delete al->_listener;
    al->_listener = nullptr;
    // free AudioSource struct
    free(al);
}

// --------------------------------------------------
// MARK: - Exposed functions -
// --------------------------------------------------

AudioListener *audioListener_new(void) {
    AudioListener *al = static_cast<AudioListener *>(malloc(sizeof(AudioListener)));
    vx_assert(al != nullptr);
    
    al->_transform = transform_new_with_ptr(AudioListenerTransform, al, &_audioListener_void_free);
    
    // init miniaudio stuff
    al->_listener = new vx::audio::Listener(vx::audio::AudioEngine::shared());

    return al;
}

void audioListener_free(AudioListener * const al) {
    if (al != nullptr && al->_transform != nullptr) {
        transform_release(al->_transform);
    }
}

// accessors / modifiers

float3 audioListener_getPosition(AudioListener * const al) {
    vx_assert(al != nullptr);
    if (al == nullptr || al->_listener == nullptr) {
        return float3_zero;
    }
    float3 result = float3_zero;
    al->_listener->getPosition(result.x, result.y, result.z);
    return result;
}

void audioListener_setPosition(AudioListener * const al, const float3 position) {
    vx_assert(al != nullptr);
    if (al == nullptr || al->_listener == nullptr) {
        return;
    }
    al->_listener->setPosition(position.x, position.y, position.z);
}

float3 audioListener_getDirection(AudioListener * const al) {
    vx_assert(al != nullptr);
    if (al == nullptr || al->_listener == nullptr) {
        return float3_zero;
    }
    float3 result = float3_zero;
    al->_listener->getDirection(result.x, result.y, result.z);
    return result;
}

void audioListener_setDirection(AudioListener * const al, const float3 direction) {
    vx_assert(al != nullptr);
    if (al == nullptr || al->_listener == nullptr) {
        return;
    }
    al->_listener->setDirection(direction.x, direction.y, direction.z);
}

Transform *audioListener_getTransform(const AudioListener * const al) {
    vx_assert(al != nullptr);
    return al->_transform;
}

void audioListener_refresh(AudioListener * const al) {
    vx_assert(al != nullptr);
    // update position
    const float3 *pos = transform_get_position(al->_transform, true);
    audioListener_setPosition(al, *pos);
    
    // update direction
    float3 forward; transform_get_forward(al->_transform, &forward, true);
    // invert direction to match the audio engine reference frame
    forward.x *= -1.0f;
    forward.y *= -1.0f;
    forward.z *= -1.0f;
    audioListener_setDirection(al, forward);
}

#endif
