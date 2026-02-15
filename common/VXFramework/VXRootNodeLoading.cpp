//
//  VXMenuLoading.cpp
//  Cubzh
//
//  Created by Xavier Legland on 24/02/2020.
//

#include "VXRootNodeLoading.hpp"

#include "VXHubClient.hpp"
#include "GameCoordinator.hpp"

// xptools
#include "OperationQueue.hpp"

using namespace vx;

RootNodeLoading_SharedPtr RootNodeLoading::make() {
    RootNodeLoading_SharedPtr p(new RootNodeLoading);
    p->_init(p);
    return p;
}

void RootNodeLoading::_init(const RootNodeLoading_SharedPtr& ref) {
    Node::init(ref);

    this->setColor(MenuConfig::shared().colorTransparent);

    _loadingLabel = Label::make("", Memory::Strategy::WEAK);
    _loadingLabel->setTextColor(MenuConfig::shared().colorWhite);
    _loadingLabel->parentDidResize = [](Node_SharedPtr n){
        n->setTop(Screen::heightInPoints * 0.5f + n->getHeight() * 0.5f);
        n->setLeft(Screen::widthInPoints * 0.5f - n->getWidth() * 0.5f);
    };
    _loadingLabel->contentDidResize = [](Node_SharedPtr n){
        n->setTop(Screen::heightInPoints * 0.5f + n->getHeight() * 0.5f);
        n->setLeft(Screen::widthInPoints * 0.5f - n->getWidth() * 0.5f);
    };

    this->addChild(_loadingLabel);
    _setText();

    NotificationCenter::shared().addListener(this, NotificationName::stateChanged);
    NotificationCenter::shared().addListener(this, NotificationName::loadingGameStateChanged);
    NotificationCenter::shared().addListener(this, NotificationName::hideLoadingLabel);
    NotificationCenter::shared().addListener(this, NotificationName::showLoadingLabel);
}

RootNodeLoading::RootNodeLoading() :
Node(),
NotificationListener(),
_loadingLabel(nullptr) {}

/// Destructor
RootNodeLoading::~RootNodeLoading() {
    NotificationCenter::shared().removeListener(this);
}

void RootNodeLoading::_setText() {
    _loadingLabel->setText(GameCoordinator::getInstance()->getIdealLoadingMessage().c_str(),
                           Memory::Strategy::MAKE_COPY);
}

void RootNodeLoading::didReceiveNotification(const Notification &notification) {
    // the displayed text change according to the GameCoordinator's state
    switch (notification.name) {
        case NotificationName::stateChanged:
        case NotificationName::loadingGameStateChanged:
            _setText();
            break;
        case NotificationName::hideLoadingLabel:
            _loadingLabel->setVisible(false);
            break;
        case NotificationName::showLoadingLabel:
            _loadingLabel->setVisible(true);
            break;
        default:
            break;
    }
}
