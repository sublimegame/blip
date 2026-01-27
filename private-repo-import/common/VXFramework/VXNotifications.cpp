//
//  VXNotifications.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 3/16/20.
//

#include "VXNotifications.hpp"

// C++
#include <algorithm>

// xptools
#include "xptools.h"
#include "OperationQueue.hpp"

using namespace vx;

// ---------------------
// Notification
// ---------------------

Notification::Notification() {
    this->_type = NotificationType::noValue;
}

int Notification::intValue() const {
    if (this->_type == NotificationType::intValue) {
        return static_cast<const NotificationWithInt*>(this)->value;
    }
    return 0;
}

std::string Notification::stringValue() const {
    if (this->_type == NotificationType::stringValue) {
        return static_cast<const NotificationWithString*>(this)->value;
    }
    return "";
}

NotificationWithInt::NotificationWithInt() {
    this->_type = NotificationType::intValue;
}

NotificationWithString::NotificationWithString() {
    this->_type = NotificationType::stringValue;
}

// ---------------------
// NotificationListener
// ---------------------

NotificationListener::NotificationListener() {
    this->nbSubscriptions = 0;
}

NotificationListener::~NotificationListener() {
    // To make sure future notification aren't sent to this NotificationListener
    if (this->nbSubscriptions > 0) {
        vxlog_info("⚠️ NotificationListener destroyed with %d active subscription(s)", this->nbSubscriptions);
        NotificationCenter::shared().removeListener(this);
    }
}


// ---------------------
// NotificationCenter
// ---------------------

NotificationCenter::NotificationCenter() {}

NotificationCenter::~NotificationCenter() {
    
    std::map<NotificationName, std::list<NotificationListener*>*>::iterator it;
    
    for(it = this->_listeners.begin(); it != this->_listeners.end(); ++it)
    {
        it->second->clear();
        delete it->second;
        it->second = nullptr;
    }
    
    this->_listeners.clear();
}

void NotificationCenter::notify(NotificationName notificationName) {
        
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
    vx::OperationQueue::getMain()->dispatch([this, notificationName](){
#pragma clang diagnostic pop
        
        std::map<NotificationName, std::list<NotificationListener*>*>::iterator it;
        
        it = this->_listeners.find(notificationName);
        
        if (it == this->_listeners.end()) {
            // no listener for such notification name
            return;
        }
        
        if (it->second == nullptr) {
            // should not happen, but safer to return
            return;
        }
        
        Notification n = Notification();
        n.name = notificationName;
        
        for (NotificationListener *l : *(it->second)) {
            l->didReceiveNotification(n);
        }
    });
}

void NotificationCenter::notify(NotificationName notificationName, int value) {
    
    vx::OperationQueue::getMain()->dispatch([this, notificationName, value](){
        
        std::map<NotificationName, std::list<NotificationListener*>*>::iterator it;
        
        it = this->_listeners.find(notificationName);
        
        if (it == this->_listeners.end()) {
            // no listener for such notification name
            return;
        }
        
        if (it->second == nullptr) {
            // should not happen, but safer to return
            return;
        }
        
        NotificationWithInt n = NotificationWithInt();
        n.name = notificationName;
        n.value = value;
        
        for (NotificationListener *l : *(it->second)) {
            l->didReceiveNotification(n);
        }
    });
}

void NotificationCenter::notify(NotificationName notificationName, std::string value) {
    
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
    vx::OperationQueue::getMain()->dispatch([this, notificationName, value](){
#pragma clang diagnostic pop
        
        std::map<NotificationName, std::list<NotificationListener*>*>::iterator it;
        
        it = this->_listeners.find(notificationName);
        
        if (it == this->_listeners.end()) {
            // no listener for such notification name
            return;
        }
        
        if (it->second == nullptr) {
            // should not happen, but safer to return
            return;
        }
        
        NotificationWithString n = NotificationWithString();
        n.name = notificationName;
        n.value = value;
        
        for (NotificationListener *l : *(it->second)) {
            l->didReceiveNotification(n);
        }
    });
}



void NotificationCenter::addListener(NotificationListener *l, NotificationName notificationName, bool first) {
    
    std::map<NotificationName, std::list<NotificationListener*>*>::iterator it;
    
    std::list<NotificationListener*> *list = nullptr;
    
    it = this->_listeners.find(notificationName);
    
    if (it == this->_listeners.end()) {
        list = new std::list<NotificationListener*>();
        this->_listeners[notificationName] = list;
    } else {
        list = it->second;
    }
    
    if (list == nullptr) {
        // should not happen
        vxlog_error("🔥 can't add listener");
        return;
    }
    
    if (std::find(list->begin(), list->end(), l) != list->end()) {
        // listener found for same notification name
        // should not be duplicated
        return;
    }
    
    l->nbSubscriptions++;
    
    if (first) {
        list->push_front(l);
    } else {
        list->push_back(l);
    }
}
    
void NotificationCenter::removeListener(NotificationListener *l, NotificationName notificationName) {
    
    std::map<NotificationName, std::list<NotificationListener*>*>::iterator it;
    
    it = this->_listeners.find(notificationName);
    
    if (it == this->_listeners.end()) {
        // no listeners for this notification
        return;
    }
    
    std::list<NotificationListener*> *list = it->second;
    
    if (list == nullptr) { return; }
    
    size_t sizeBefore = list->size();
    list->remove(l);
    size_t sizeAfter = list->size();
    
    l->nbSubscriptions -= (sizeBefore - sizeAfter);
}

void NotificationCenter::removeListener(NotificationListener *l) {
    
    std::map<NotificationName, std::list<NotificationListener*>*>::iterator it;
    
    std::list<NotificationListener*> *list = nullptr;
    
    for(it = this->_listeners.begin(); it != this->_listeners.end(); ++it)
    {
        list = it->second;
        if (list == nullptr) {
            continue;
        }
        
        size_t sizeBefore = list->size();
        
        list->remove(l);
        size_t sizeAfter = list->size();
        
        l->nbSubscriptions -= (sizeBefore - sizeAfter);
    }
}
