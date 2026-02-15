//
//  VXPopup.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 03/02/2021.
//

#include "VXPopup.hpp"

// vx
#include "GameCoordinator.hpp"
#include "VXMenuConfig.hpp"
#include "OperationQueue.hpp"

using namespace vx;

int Popup::nbPopups = 0;

int Popup::getNbPopups() {
    return nbPopups;
}

/// Closes the popup
void Popup::close() {
    this->removeFromParent();
}

Popup::Popup() :
_frame(nullptr),
_container(nullptr) {
    nbPopups = nbPopups + 1;
    
    vx::OperationQueue::getMain()->dispatch([](){
        Game_SharedPtr g = GameCoordinator::getInstance()->getActiveGame();
        if (g == nullptr) return;
        g->sendCPPMenuStateChanged();
    });
}

Popup::~Popup() {
    nbPopups = nbPopups - 1;
    
    vx::OperationQueue::getMain()->dispatch([](){
        Game_SharedPtr g = GameCoordinator::getInstance()->getActiveGame();
        if (g == nullptr) return;
        g->sendCPPMenuStateChanged();
    });
}

Node_SharedPtr Popup::getFrame() {
    return _frame;
}

Node_SharedPtr Popup::getContainer() {
    return _container;
}

void Popup::init(const Popup_SharedPtr& ref) {
    this->_weakSelf = ref;
    
    this->setHittable(true);
    this->setPassthrough(false);

    this->setColor(Color(uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(230)));
    this->parentDidResize = [](Node_SharedPtr n){
        n->setSize(Screen::widthInPoints, Screen::heightInPoints);
        n->setPos(0.0f, Screen::heightInPoints);
    };
   
    // Popup box
    
    _frame = Node::make();
    _frame->setColor(MenuConfig::shared().colorWhite);
    _frame->parentDidResize = [](Node_SharedPtr n){
        n->setWidth(minimum(Screen::widthInPoints, 500.0f));
        n->setHeight(400.0f); // TODO: based on content
        n->setTop(Screen::heightInPoints * 0.5f + n->getHeight() * 0.5f);
        n->setLeft(Screen::widthInPoints * 0.5f - n->getWidth() * 0.5f);
    };
    _frame->contentDidResize = [](Node_SharedPtr n) {
        // TODO
    };
    this->addChild(_frame);
    
    _container = Node::make();
    _container->setColor(MenuConfig::shared().colorTransparentBlack90);
    _container->parentDidResize = [](Node_SharedPtr n) {
        Node_SharedPtr parent = n->getParent();
        if (parent == nullptr) { return; }
        n->setWidth(parent->getWidth() - BEVEL * 2);
        n->setHeight(parent->getHeight() - BEVEL * 2); // TODO: based on content
        n->setTop(parent->getHeight() - BEVEL);
        n->setLeft(BEVEL);
    };
    _container->contentDidResize = [](Node_SharedPtr n) {
        // TODO
    };

    _frame->addChild(_container);
}
