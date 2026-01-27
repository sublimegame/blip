//
//  VXMenuLoading.hpp
//  Cubzh
//
//  Created by Xavier Legland on 24/02/2020.
//

#pragma once

#include <string>

// vx
#include "VXNode.hpp"
#include "VXLabel.hpp"
#include "VXMenuConfig.hpp"
#include "VXButton.hpp"

namespace vx {

class RootNodeLoading;
typedef std::shared_ptr<RootNodeLoading> RootNodeLoading_SharedPtr;
typedef std::weak_ptr<RootNodeLoading> RootNodeLoading_WeakPtr;

class RootNodeLoading final : public Node, public NotificationListener {
    
public:
    
    ///
    static RootNodeLoading_SharedPtr make();
    
    ///
    ~RootNodeLoading() override;
    
    ///
    void didReceiveNotification(const vx::Notification &notification) override;
    
private:
    
    ///
    void _init(const RootNodeLoading_SharedPtr& ref);
    
    ///
    void _setText();
    
    ///
    RootNodeLoading();
    
    Label_SharedPtr _loadingLabel;
};

} // namespace vx
