//
//  audioSource.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 19/08/2022.
//

#ifndef P3S_CLIENT_HEADLESS

#include "audioSource.hpp"

#include "vxconfig.h"

#include "BZMD5.hpp"

// xptools
#include "vxlog.h"
#include "xptools.h"
#include "audio.hpp"

#define SOUND_FILE_EXTENSION ".ogg"

// local functions' prototypes
void _audioSource_void_free(void *o);

// MARK: - Unexposed functions -

typedef std::shared_ptr<vx::audio::Sound> Sound_SharedPtr;
typedef std::weak_ptr<vx::audio::Sound> Sound_WeakPtr;

class AudioSource final {
  public:
    AudioSource();
    ~AudioSource();

    Transform *_transform;

    Sound_SharedPtr _sound;

    float _pan;
    bool _spatialized;
    // uint64_t _startAt;
    // bool startAtEnabled;
    // uint64_t _stopAt;
    // bool stopAtEnabled;
    float _volume;
    float _pitch;
    bool _loop;

    // char pad[5];
};

void _audioSource_void_free(void *o) {
    AudioSource *as = static_cast<AudioSource *>(o);
    vx_assert(as != nullptr);

    // free AudioSource instance
    delete as;
}

AudioSource::AudioSource() :
_sound(nullptr),
_pan(0.0f),
_spatialized(true),
_volume(1.0f),
_pitch(1.0f),
_loop(false) {

    _transform = transform_new_with_ptr(AudioSourceTransform,
                                        this,
                                        &_audioSource_void_free);
}

AudioSource::~AudioSource() {
    if (_sound != nullptr) {
        _sound->stop();
        _sound = nullptr;
    }
}

#ifdef __cplusplus
extern "C" {
#endif

// MARK: - Exposed functions -

AudioSource *audioSource_new(void) {
    AudioSource *as = new AudioSource();
    
    return as;
}

void audioSource_free(AudioSource * const as) {
    if (as != nullptr && as->_transform != nullptr) {
        transform_release(as->_transform);
    }
}


void audioSource_pause(const AudioSource * const as) {
    vx_assert(as != nullptr);
    if (as->_sound != nullptr) {
        as->_sound->pause();
    } else {
        // trying to pause an audio source having no sound
        vxlog_debug("[audioSource_pause] trying to pause an audio source having no sound");
        // TODO: add error handling
    }
}

void audioSource_play(const AudioSource * const as) {
    vx_assert(as != nullptr);
    if (as->_sound != nullptr) {
        as->_sound->play();
    } else {
        // trying to play an audio source having no sound
        vxlog_debug("[audioSource_play] trying to play an audio source having no sound");
        // TODO: add error handling
    }
}

void audioSource_stop(const AudioSource * const as) {
    vx_assert(as != nullptr);
    if (as->_sound != nullptr) {
        as->_sound->stop();
    } else {
        // trying to play an audio source having no sound
        vxlog_debug("[audioSource_stop] trying to stop an audio source having no sound");
        // TODO: add error handling
    }
}

bool audioSource_isPlaying(AudioSource * const as) {
    if (as == nullptr) {
        return false;
    }
    return as->_sound->isPlaying();
}

float audioSource_getPan(AudioSource * const as) {
    if (as == nullptr) {
        return 0.0f;
    }
    return as->_pan;
}

void audioSource_setPan(AudioSource * const as, const float pan) {
    if (as == nullptr) {
        return;
    }
    as->_pan = pan;

    if (as->_sound != nullptr) {
        as->_sound->setPan(as->_pan);
    }
}

float3 audioSource_getPosition(AudioSource * const as) {
    if (as == nullptr || as->_sound == nullptr) {
        return float3_zero;
    }
    float3 result = float3_zero;
    as->_sound->getPosition(result.x, result.y, result.z);
    return result;
}

void audioSource_setPosition(AudioSource * const as, const float3 position) {
    if (as == nullptr || as->_sound == nullptr) {
        return;
    }
    // NOTE: should we store position in AudioSource?
    as->_sound->setPosition(position.x, position.y, position.z);
}

const char *audioSource_getSoundName(AudioSource * const as) {
    if (as == nullptr || as->_sound == nullptr) {
        return nullptr;
    }
    return as->_sound->getSoundName().c_str();
}

void audioSource_setSound(AudioSource * const as, const char * soundName) {
    if (as == nullptr || soundName == nullptr) {
        return;
    }

    // there is a sound already loaded, we must release it
    if (as->_sound != nullptr) {
        as->_sound->stop();
        as->_sound = nullptr;
    }

    const std::string fullname = std::string(soundName) + std::string(SOUND_FILE_EXTENSION);
    as->_sound = vx::audio::Sound::make(vx::audio::AudioEngine::shared(), fullname, audioSource_getLooping(as));
    if (as->_sound != nullptr) {
        // preserve settings for the new sound
        audioSource_setPan(as, audioSource_getPan(as));
        audioSource_setSpatialized(as, audioSource_getSpatialized(as));
        // audioSource_setStartAtMs(as, startAt);
        // audioSource_setStopAtMs(as, stopAt);
        audioSource_setVolume(as, audioSource_getVolume(as));
        audioSource_setPitch(as, audioSource_getPitch(as));
        audioSource_setLooping(as, audioSource_getLooping(as));
    }
}

// TODO: might want to copy bufferPtr content
void audioSource_setSoundBuffer(AudioSource * const as, const void *bufferPtr, const size_t bufferLen) {
    if (as == nullptr || bufferPtr == nullptr) {
        return;
    }

    // there is a sound already loaded, we must release it
    if (as->_sound != nullptr) {
        as->_sound->stop();
        as->_sound = nullptr;
    }

    // construct hash from buffer data
    const std::string buf = std::string(static_cast<const char*>(bufferPtr), bufferLen);
    const std::string digest = md5(buf);

    // TODO: avoid writing the buffer in the storage, instead decode it in memory
    // write audio buffer in storage
    const std::string storageFilepath = "cache/_audio_" + digest + SOUND_FILE_EXTENSION;
    if (vx::fs::storageFileExists(storageFilepath) == false) {
        FILE *fd = vx::fs::openStorageFile(storageFilepath, "wb");
        if (fd != nullptr) {
            fwrite(bufferPtr, 1, bufferLen, fd);
            fclose(fd);
        }
    }

    vxlog_debug("AUDIO FILEPATH %s", storageFilepath.c_str());

    as->_sound = vx::audio::Sound::make(vx::audio::AudioEngine::shared(), storageFilepath, audioSource_getLooping(as));
    if (as->_sound != nullptr) {
        // preserve settings for the new sound
        audioSource_setPan(as, audioSource_getPan(as));
        audioSource_setSpatialized(as, audioSource_getSpatialized(as));
        // audioSource_setStartAtMs(as, startAt);
        // audioSource_setStopAtMs(as, stopAt);
        audioSource_setVolume(as, audioSource_getVolume(as));
        audioSource_setPitch(as, audioSource_getPitch(as));
        audioSource_setLooping(as, audioSource_getLooping(as));
    }
}

bool audioSource_getSpatialized(AudioSource * const as) {
    if (as == nullptr) {
        return true;
    }
    return as->_spatialized;
}

void audioSource_setSpatialized(AudioSource * const as, const bool spatialized) {
    if (as == nullptr) {
        return;
    }
    as->_spatialized = spatialized;
    if (as->_sound != nullptr) {
        as->_sound->setSpatialized(as->_spatialized);
    }
}

bool audioSource_getLooping(AudioSource *const as) {
    if (as == nullptr) {
        return false;
    }
    return as->_loop;
}

void audioSource_setLooping(AudioSource *const as, const bool looping) {
    if (as == nullptr) {
        return;
    }
    as->_loop = looping;
    if (as->_sound != nullptr) {
        as->_sound->setLooping(as->_loop);
    }
}

bool audioSource_getStartAtEnabled(AudioSource *const as) {
    if (as == nullptr || as->_sound == nullptr) {
        return false;
    }
    return as->_sound->getStartAtEnabled();
}

float audioSource_getStartAt(AudioSource * const as) {
    if (as == nullptr || as->_sound == nullptr) {
        return 0;
    }
    return as->_sound->getStartAt();
}

void audioSource_disableStartAt(AudioSource *const as) {
    if (as == nullptr || as->_sound == nullptr) {
        return;
    }
    as->_sound->disableStartAt();
}

bool audioSource_setStartAt(AudioSource *const as, const float s) {
    if (as == nullptr || as->_sound == nullptr) {
        return false;
    }
    return as->_sound->setStartAt(s);
}

bool audioSource_getStopAtEnabled(AudioSource *const as) {
    if (as == nullptr || as->_sound == nullptr) {
        return false;
    }
    return as->_sound->getStopAtEnabled();
}

float audioSource_getStopAt(AudioSource * const as) {
    if (as == nullptr || as->_sound == nullptr) {
        return 0;
    }
    return as->_sound->getStopAt();
}

void audioSource_disableStopAt(AudioSource* const as) {
    if (as == nullptr || as->_sound == nullptr) {
        return;
    }
    as->_sound->disableStopAt();
}

bool audioSource_setStopAt(AudioSource * const as, const float s) {
    if (as == nullptr || as->_sound == nullptr) {
        return false;
    }
    return as->_sound->setStopAt(s);
}

float audioSource_getVolume(AudioSource * const as) {
    if (as == nullptr) {
        return 1.0f;
    }
    return as->_volume;
}

void audioSource_setVolume(AudioSource * const as, const float volume) {
    if (as == nullptr) {
        return;
    }
    as->_volume = volume;
    if (as->_sound != nullptr) {
        as->_sound->setVolume(as->_volume);
    }
}

float audioSource_getOriginalDuration(AudioSource *const as) {
    if (as == nullptr || as->_sound == nullptr) {
        return false;
    }
    return as->_sound->getOriginalDuration();
}

float audioSource_getRadius(AudioSource * const as) {
    if (as == nullptr || as->_sound == nullptr) {
        return 0.0f;
    }
    return as->_sound->getMaxDistance();
}

void audioSource_setRadius(AudioSource * const as, const float radius) {
    if (as == nullptr || as->_sound == nullptr) {
        return;
    }
    as->_sound->setMaxDistance(radius);
}

float audioSource_getMinRadius(AudioSource * const as) {
    if (as == nullptr || as->_sound == nullptr) {
        return 0.0f;
    }
    return as->_sound->getMinDistance();
}

void audioSource_setMinRadius(AudioSource * const as, const float radius) {
    if (as == nullptr || as->_sound == nullptr) {
        return;
    }
    as->_sound->setMinDistance(radius);
}

float audioSource_getPitch(AudioSource *const as) {
    if (as == nullptr) {
        return 1.0f;
    }
    return as->_pitch;
}

void audioSource_setPitch(AudioSource *const as, const float pitch) {
    if (as == nullptr) {
        return;
    }
    as->_pitch = pitch;
    if (as->_sound != nullptr) {
        as->_sound->setPitch(as->_pitch);
    }
}

Transform *audioSource_getTransform(const AudioSource * const as) {
    vx_assert(as != nullptr);
    return as->_transform;
}

void audioSource_refresh(AudioSource * const as) {
    // update position
    const float3 *pos = transform_get_position(as->_transform, true);
    audioSource_setPosition(as, *pos);
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif
