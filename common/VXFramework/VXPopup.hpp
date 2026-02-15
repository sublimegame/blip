//
//  VXPopup.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 03/02/2021.
//

#pragma once

// vx
#include "VXNode.hpp"
#include "VXNotifications.hpp"

namespace vx {

class Popup;
typedef std::shared_ptr<Popup> Popup_SharedPtr;
typedef std::weak_ptr<Popup> Popup_WeakPtr;

class Popup : public Node {
    
public:
    
    /// Closes the popup
    virtual void close() final;

    /// Destructor
    virtual ~Popup() override;
    
    /// Returns Popup's frame.
    /// Should not be used to add content,
    /// But in some cases, it's useful to
    /// adopt different constraints.
    Node_SharedPtr getFrame();
    
    /// Returns Popup's container,
    /// Where all content should be added.
    Node_SharedPtr getContainer();
    
    static int getNbPopups();
    
protected:
    
    /// Constructor
    Popup();

    ///
    void init(const Popup_SharedPtr &ref);
    
private:
    
    static int nbPopups;
    
    ///
    Node_SharedPtr _frame;
    
    ///
    Node_SharedPtr _container;
    
private:
    
};

}

