//
//  VXNotifications.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 3/16/20.
//

#pragma once

// C++
#include <string>
#include <map>
#include <list>

// xptools
#include "Macros.h"

namespace vx {

enum class NotificationType {
    noValue,
    intValue,
    stringValue
};

///
enum class NotificationName {
    worldsListed,                          // sent when listed worlds change
    itemslisted,                           // sent when listed items change
    stateChanged,                          // sent when GameCoordinator state changed
    mainMenuNeedsContentRefresh,           // sent when something changes in main menu's content node
    mainMenuNeedsSubMenuRefresh,           // sent when something changes in main menu's submenu
    toggleBackgroundGame,                  // sent when showing/hiding background game
    escKeyUp,                              // sent when ESC key is released
    showKeyboard,                          // sent when keyboard is shown
    hideKeyboard,                          // sent when keyboard is hidden
    returnKeyUp,                           // sent when return key is released
    softReturnKeyUp,                       // sent when return key is activated on a soft keyboard
    slashKeyUp,                            // sent when '/' key is released
    thumbnailUploaded,                     // sent when thumbnail has been successfully uploaded (posted with gameID)
    didResize,
    orientationChanged,                    // valid for both desktop & mobile : do we have more horizontal vs. vertical space?
    fontScaleDidChange,                    // font scale changed
    accountUsernameChanged,                // the anonymous user successfully chose a username
    accountChooseEmailSuccessful,          // the user successfully associated an email with its account
    accountChoosePasswordSuccessful,       // the user successfully associated a password with its account
    hideLoadingLabel,                      // hides loading label if there's one
    showLoadingLabel,                       // shows loading label if there's one
    gotoLuaMenu,
    sensitivityUpdated,
    loadingGameStateChanged,
};

///
class Notification {
public:
    Notification();
    NotificationName name;
    
    int intValue() const;
    std::string stringValue() const;
protected:
    NotificationType _type;
};

struct NotificationWithInt : Notification {
public:
    NotificationWithInt();
    int value;
};

struct NotificationWithString : Notification {
public:
    NotificationWithString();
    std::string value;
};

///
class NotificationListener {

public:
    NotificationListener();
    virtual ~NotificationListener();
    
    /// called when receiving notification
    virtual void didReceiveNotification(const Notification &notification) = 0;
    
    int nbSubscriptions;
    
private:
};

///
class NotificationCenter {
    
public:
    static NotificationCenter& shared() {
        static NotificationCenter instance;
        return instance;
    }
    
    /**
     * Notifies all listeners
     * Notification will be received in same thread where `notify` is called.
     */
    void notify(NotificationName notificationName);
    void notify(NotificationName notificationName, int value);
    void notify(NotificationName notificationName, std::string value);
    
    void addListener(NotificationListener *l, NotificationName notificationName, bool first = false);
    
    void removeListener(NotificationListener *l, NotificationName notificationName);
    void removeListener(NotificationListener *l);
    
    VX_DISALLOW_COPY_AND_ASSIGN(NotificationCenter)
    
private:
    NotificationCenter();
    ~NotificationCenter();
    
    std::map<NotificationName, std::list<NotificationListener*>*> _listeners;
};
    
}
