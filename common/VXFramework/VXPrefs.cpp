//
//  VXPrefs.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 04/08/2022.
//

#include "VXPrefs.hpp"

#include "VXConfig.hpp"
#include "VXJSON.hpp"

#define PREFS_FILE "prefs.json"

#define PREFS_KEY_NOTIFPUSHTOKENTOUPLOAD "notifPushTokenToUpload"
#define PREFS_KEY_NOTIFPUSHTOKEN "notifPushToken"

using namespace vx;

const std::string Prefs::NOTIF_USERCHOICE_GRANTED = "granted";
const std::string Prefs::NOTIF_USERCHOICE_LATER = "later";
const std::string Prefs::NOTIF_USERCHOICE_DENIED = "denied";

Prefs &Prefs::shared() {
    static Prefs p;
    return p;
}

Prefs::Prefs() :
_sensitivity(SENSITIVITY_DEFAULT),
_zoomSensitivity(ZOOM_SENSITIVITY_DEFAULT),
_masterVolume(1.0f),
_canVibrate(true),
_fullscreen(true),
_renderQualityTier(RENDER_QUALITY_TIER_INVALID),
_notificationsPushTokenToUpload(),
_notificationsPushToken() {

    bool isDir;
    const bool exists = fs::storageFileExists(PREFS_FILE, isDir);
    if (exists && isDir == false) {

        if (isDir) {
            vxlog_error("%s is not supposed to be a directory", PREFS_FILE);
            return;
        }

        FILE *prefsFile = fs::openStorageFile(PREFS_FILE);

        if (prefsFile == nullptr) {
            vxlog_error("can't open %s", PREFS_FILE);
            return;
        }

        char *prefsChars = fs::getFileTextContentAndClose(prefsFile);

        if (prefsChars == nullptr) {
            vxlog_error("can't read %s (1)", PREFS_FILE);
            return;
        }

        cJSON *prefs = cJSON_Parse(prefsChars);

        free(prefsChars);

        if (prefs == nullptr) {
            vxlog_error("can't read %s (2)", PREFS_FILE);
            return;
        }

        if (cJSON_IsObject(prefs) == false) {
            vxlog_error("%s: top level object expected", PREFS_FILE);
            cJSON_Delete(prefs);
            return;
        }

        // all fields are optional

        std::string str;
        bool b;
        double d;
        int i;

        if (JSON::readDouble(prefs, "sensitivity", d)) {
            this->_sensitivity = static_cast<float>(d);
        }

        if (JSON::readDouble(prefs, "zoom-sensitivity", d)) {
            this->_zoomSensitivity = static_cast<float>(d);
        }

        if (JSON::readDouble(prefs, "master-volume", d)) {
            this->_masterVolume = static_cast<float>(d);
        }

        if (JSON::readBool(prefs, "can-vibrate", b)) {
            this->_canVibrate = b;
        }

        if (JSON::readBool(prefs, "fullscreen", b)) {
            this->_fullscreen = b;
        }

        if (JSON::readInt(prefs, "renderQualityTier", i)) {
            if (i >= RENDER_QUALITY_TIER_MIN && i <= RENDER_QUALITY_TIER_MAX) {
                this->_renderQualityTier = i;
            }
        }

        if (JSON::readString(prefs, PREFS_KEY_NOTIFPUSHTOKENTOUPLOAD, str)) {
            this->_notificationsPushTokenToUpload = str;
        }

        if (JSON::readString(prefs, PREFS_KEY_NOTIFPUSHTOKEN, str)) {
            this->_notificationsPushToken = str;
        }

        cJSON_Delete(prefs);
    }
}

Prefs::~Prefs() {}

bool Prefs::_save() {

    cJSON *jsonObj = cJSON_CreateObject();
    if (jsonObj == nullptr) {
        vxlog_error("can't create prefs json obj");
        return false;
    }

    if (_sensitivity >= SENSITIVITY_MIN && _sensitivity < SENSITIVITY_MAX) {
        cJSON *sensitivity = cJSON_CreateNumber(static_cast<double>(_sensitivity));
        if (sensitivity == nullptr) {
            vxlog_error("can't create sensitivity json number");
            cJSON_Delete(jsonObj);
            return false;
        }
        cJSON_AddItemToObject(jsonObj, "sensitivity", sensitivity);
    }

    if (_zoomSensitivity >= SENSITIVITY_MIN && _zoomSensitivity < SENSITIVITY_MAX) {
        cJSON *zoomSensitivity = cJSON_CreateNumber(static_cast<double>(_zoomSensitivity));
        if (zoomSensitivity == nullptr) {
            vxlog_error("can't create zoomSensitivity json number");
            cJSON_Delete(jsonObj);
            return false;
        }
        cJSON_AddItemToObject(jsonObj, "zoom-sensitivity", zoomSensitivity);
    }

    {
        cJSON *masterVolume = cJSON_CreateNumber(static_cast<double>(_masterVolume));
        if (masterVolume == nullptr) {
            vxlog_error("can't create masterVolume json number");
            cJSON_Delete(jsonObj);
            return false;
        }
        cJSON_AddItemToObject(jsonObj, "master-volume", masterVolume);
    }

    {
        cJSON *canVibrate = cJSON_CreateBool(_canVibrate);
        if (canVibrate == nullptr) {
            vxlog_error("can't create can-vibrate json bool");
            cJSON_Delete(jsonObj);
            return false;
        }
        cJSON_AddItemToObject(jsonObj, "can-vibrate", canVibrate);
    }

    {
        cJSON *fullscreen = cJSON_CreateBool(_fullscreen);
        if (fullscreen == nullptr) {
            vxlog_error("can't create fullscreen json bool");
            cJSON_Delete(jsonObj);
            return false;
        }
        cJSON_AddItemToObject(jsonObj, "fullscreen", fullscreen);
    }

    if (_renderQualityTier >= RENDER_QUALITY_TIER_MIN && _renderQualityTier <= RENDER_QUALITY_TIER_MAX) {
        cJSON *rqTier = cJSON_CreateNumber(static_cast<double>(_renderQualityTier));
        if (rqTier == nullptr) {
            vxlog_error("can't create renderQualityTier json number");
            cJSON_Delete(jsonObj);
            return false;
        }
        cJSON_AddItemToObject(jsonObj, "renderQualityTier", rqTier);
    }

    // Notification Push Token to upload
    {
        cJSON *token = cJSON_CreateString(_notificationsPushTokenToUpload.c_str());
        if (token == nullptr) {
            vxlog_error("can't create notifPushTokenToUpload json string");
            cJSON_Delete(jsonObj);
            return false;
        }
        cJSON_AddItemToObject(jsonObj, PREFS_KEY_NOTIFPUSHTOKENTOUPLOAD, token);
    }

    // Notification Push Token
    {
        cJSON *token = cJSON_CreateString(_notificationsPushToken.c_str());
        if (token == nullptr) {
            vxlog_error("can't create notifPushToken json string");
            cJSON_Delete(jsonObj);
            return false;
        }
        cJSON_AddItemToObject(jsonObj, PREFS_KEY_NOTIFPUSHTOKEN, token);
    }

    char *jsonStr = cJSON_Print(jsonObj);
    cJSON_Delete(jsonObj);
    jsonObj = nullptr;

    FILE *prefsFile = fs::openStorageFile(PREFS_FILE, "wb");
    if (prefsFile == nullptr) {
        vxlog_error("can't open %s", PREFS_FILE);
        free(jsonStr);
        return false;
    }
    fputs(jsonStr, prefsFile);

    free(jsonStr);
    fclose(prefsFile);

    // TODO: gdevillele: hide this somewhere else
    //                   replace this by a call in willResignActive maybe?
    vx::fs::syncFSToDisk();

    return true;
}

void Prefs::setSensitivity(float sensitivity) {
    this->_sensitivity = sensitivity;
    _save();
}

void Prefs::setZoomSensitivity(float zoomSensitivity) {
    this->_zoomSensitivity = zoomSensitivity;
    _save();
}

void Prefs::setMasterVolume(float masterVolume) {
    this->_masterVolume = masterVolume;
    _save();
}

void Prefs::setCanVibrate(bool canVibrate) {
    this->_canVibrate = canVibrate;
    _save();
}

void Prefs::setFullscreen(bool fs) {
    this->_fullscreen = fs;
    _save();
}

void Prefs::setRenderQualityTier(int tier) {
    this->_renderQualityTier = tier;
    _save();
}

bool Prefs::setNotifPushTokenToUpload(const std::string& token) {
    this->_notificationsPushTokenToUpload.assign(token);
    return _save();
}

bool Prefs::setNotifPushToken(const std::string& token) {
    this->_notificationsPushToken.assign(token);
    return _save();
}

float Prefs::sensitivity() {
    return _sensitivity;
}

float Prefs::zoomSensitivity() {
    return _zoomSensitivity;
}

float Prefs::masterVolume() {
    return _masterVolume;
}

bool Prefs::canVibrate() {
    return _canVibrate;
}

bool Prefs::fullscreen() {
    return _fullscreen;
}

int Prefs::renderQualityTier() {
    if (_renderQualityTier < RENDER_QUALITY_TIER_MIN || _renderQualityTier > RENDER_QUALITY_TIER_MAX) {
        return RENDER_QUALITY_TIER_INVALID;
    }
    return _renderQualityTier;
}

const std::string& Prefs::notifPushTokenToUpload() const {
    return _notificationsPushTokenToUpload;
}

const std::string& Prefs::notifPushToken() const {
    return _notificationsPushToken;
}

