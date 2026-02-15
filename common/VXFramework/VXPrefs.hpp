//
//  VXPrefs.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 04/08/2022.
//

#pragma once

// C++
#include <string>

namespace vx {

/// Account is used to keep account state information.
/// The first goal was to improve the sign up/in flow.
/// But it can also be used after that for the tip system.
class Prefs final {
    
public:
    
    static Prefs& shared();

    static const std::string NOTIF_USERCHOICE_LATER;
    static const std::string NOTIF_USERCHOICE_GRANTED;
    static const std::string NOTIF_USERCHOICE_DENIED;
    
    void setSensitivity(float sensitivity);
    void setZoomSensitivity(float zoomSensitivity);
    void setMasterVolume(float masterVolume);
    void setCanVibrate(bool canVibrate);
    void setFullscreen(bool fs);
    void setRenderQualityTier(int tier);
    bool setNotifPushTokenToUpload(const std::string& token);
    bool setNotifPushToken(const std::string& pushToken);

    float sensitivity();
    float zoomSensitivity();
    float masterVolume();
    bool canVibrate();
    bool fullscreen();
    int renderQualityTier();
    const std::string& notifPushTokenToUpload() const;
    const std::string& notifPushToken() const;

    void reset();
    
private:
    
    Prefs();
    ~Prefs();

    /// Returns true on success, false otherwise.
    bool _save();
    
    float _sensitivity;
    float _zoomSensitivity;
    float _masterVolume;
    bool _canVibrate;
    bool _fullscreen;
    int _renderQualityTier;
    std::string _notificationsPushTokenToUpload;
    std::string _notificationsPushToken;
};
}

