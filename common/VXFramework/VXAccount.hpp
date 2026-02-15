//
//  VXAccount.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 4/20/21.
//

#pragma once

// C++
#include <string>
#include <vector>

// xptools
#include "xptools.h"

namespace vx {

/// Account is used to keep account state information.
/// The first goal was to improve the sign up/in flow.
/// But it can also be used after that for the tip system.
class Account final {
    
public:
    static Account& active() {
        if (_activeAccount == nullptr) {
            _activeAccount = new Account();
        }
        return *_activeAccount;
    }
    
    // Account about to be connected
    static Account& connecting() {
        if (_connectingAccount == nullptr) {
            _connectingAccount = new Account(true);
        }
        return *_connectingAccount;
    }
    
    static void doneConnecting() {
        if (_activeAccount != nullptr) {
            delete _activeAccount;
        }
        _connectingAccount->_connecting = false;
        _activeAccount = _connectingAccount;
        _connectingAccount = nullptr;
        _activeAccount->_save();
    }
    
    static void abortConnection() {
        if (_connectingAccount != nullptr) {
            delete _connectingAccount;
            _connectingAccount = nullptr;
        }
    }
    
    void setUsername(const std::string& username);
    
    void setUsernameOrEmail(const std::string& usernameOrEmail);
    void setAskedForMagicKey(bool askedForMagicKey);
    
    std::string userID();
    const std::string username();
    const std::string& usernameOrEmail();
    
    bool askedForMagicKey();
    void setUnder13(const bool &b);
    bool under13() const;

    void setParentApproved(const bool &b);
    bool parentApproved() const;

    void setChatEnabled(const bool &b);
    bool chatEnabled() const;

    // Returns true if user is under 13 & didn't approve disclaimer.
    bool under13DisclaimerRequired();
    void approveUnder13Disclaimer();

    // stores info in memory
    void update(const std::string& username,
                const bool& hasBirthdate,
                const bool& hasEmail,
                const bool& hasPassword,
                const bool& under13,
                const bool& didCustomizeAvatar,
                const bool& hasVerifiedPhoneNumber);

    void setAuthenticated(const bool& authenticated);
    bool isAuthenticated();

    /// Resets default Account values
    void reset();

    bool hasUsername() const;
    
    bool hasEmail() const;
    void setHasEmail(const bool v);

    bool hasVerifiedPhoneNumber() const;
    void setHasVerifiedPhoneNumber(const bool v);

    bool hasUnverifiedPhoneNumber() const;
    void setHasUnverifiedPhoneNumber(const bool v);

    bool isPhoneNumberExempted() const;
    void setIsPhoneNumberExempted(const bool v);

    bool hasPassword() const;
    void setHasPassword(const bool v);
    
    bool hasDOB() const;
    void setHasDOB(const bool v);

    bool hasEstimatedDOB() const;
    void setHasEstimatedDOB(const bool v);

    void resetBlockedUsers();
    void addBlockedUser(const std::string &userID);
    const std::vector<std::string>& blockedUsers();

private:
    
    Account(bool connecting = false);
    ~Account();
    
    static Account *_activeAccount;
    static Account *_connectingAccount;
    
    void _save();
    
    // saved in account.json (just a cache for login flow)
    bool _askedForMagicKey;
    // Cache for what's been entered as username.
    // Never rely on this within Lua sandbox, this is just a cache for login flow.
    // After logout, it's used to pre-fill the username field.
    std::string _usernameOrEmail;
    bool _approvedUnder13Disclaimer;
    
    // NOT saved in account.json
    // This is only kept in memory, set when skipping title screen,
    // logging in or updating information.
    bool _connecting;
    bool _authenticated; // false by default, becomes true when authenticated
    std::string _username;
    bool _under13;
    bool _parentApproved;
    bool _chatEnabled;
    bool _hasBirthdate;
    bool _hasEstimatedBirthdate; // when user provided an age, not a precise date
    bool _hasEmail;
    bool _hasPassword;
    bool _didCustomizeAvatar;
    bool _hasVerifiedPhoneNumber;
    bool _hasUnverifiedPhoneNumber;
    bool _isPhoneNumberExempted;
    std::vector<std::string> _blockedUsers;
};

}
