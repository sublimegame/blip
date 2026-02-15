//
//  VXAccount.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 4/20/21.
//

#include "VXAccount.hpp"

// xptools
#include "xptools.h"

#include "vxconfig.h"
#include "VXConfig.hpp"
#include "VXJSON.hpp"
#include "VXMenuConfig.hpp"
#include "VXHubClient.hpp"

// C++
#include <cstdio>
#include <cstring>

// using 2 files, one for active account,
// and one for if there's an ongoing connection.
#define ACTIVE_ACCOUNT_FILE "account.json"
#define CONNECTING_ACCOUNT_FILE "account-connecting.json"
#define ACCOUNT_FILE (_connecting ? CONNECTING_ACCOUNT_FILE : ACTIVE_ACCOUNT_FILE)

using namespace vx;

Account *Account::_activeAccount = nullptr;
Account *Account::_connectingAccount = nullptr;

Account::Account(bool connecting) :
_askedForMagicKey(false),
_usernameOrEmail(""),
_approvedUnder13Disclaimer(false),
_connecting(connecting),
_authenticated(false),
_username(""),
_under13(true), // consider user is <13 by default
_parentApproved(false), // consider account isn't parent approved by default
_chatEnabled(false), // consider chat not enabled by default
_hasBirthdate(false),
_hasEstimatedBirthdate(false),
_hasEmail(false),
_hasPassword(false),
_didCustomizeAvatar(false),
_hasVerifiedPhoneNumber(false),
_hasUnverifiedPhoneNumber(false),
_isPhoneNumberExempted(false),
_blockedUsers() {
    
    bool isDir;
    if (fs::storageFileExists(ACCOUNT_FILE, isDir)) {
        if (isDir) {
            vxlog_error("account.json is not supposed to be a directory");
            return;
        }
        
        FILE *accountFile = fs::openStorageFile(ACCOUNT_FILE);
        
        if (accountFile == nullptr) {
            vxlog_error("can't open %s", ACCOUNT_FILE);
            return;
        }
                
        char *accountChars = fs::getFileTextContentAndClose(accountFile);
        
        if (accountChars == nullptr) {
            vxlog_error("can't read %s (1)", ACCOUNT_FILE);
            return;
        }
        
        cJSON *account = cJSON_Parse(accountChars);
        
        free(accountChars);
        
        if (account == nullptr) {
            vxlog_error("can't read %s (2)", ACCOUNT_FILE);
            return;
        }
        
        if (cJSON_IsObject(account) == false) {
            vxlog_error("%s: top level object expected", ACCOUNT_FILE);
            cJSON_Delete(account);
            return;
        }
        
        // load login flow cache
        
        std::string str;
        
        if (JSON::readString(account, "username-or-email", str)) {
            this->_usernameOrEmail = str;
        }
        
        bool b;
        
        if (JSON::readBool(account, "approvedUnder13Disclaimer", b)) {
            this->_approvedUnder13Disclaimer = b;
        }
        
        if (JSON::readBool(account, "asked-for-magic-key", b)) {
            this->_askedForMagicKey = b;
        }
        
        cJSON_Delete(account);
    }
}

Account::~Account() {}

void Account::_save() {
    
    cJSON *jsonObj = cJSON_CreateObject();
    if (jsonObj == nullptr) {
        vxlog_error("can't create account json obj");
        return;
    }
    
    {
        cJSON *usernameOrEmail = cJSON_CreateString(_usernameOrEmail.c_str());
        if (usernameOrEmail == nullptr) {
            vxlog_error("can't create username-or-email json string");
            cJSON_Delete(jsonObj);
            return;
        }
        cJSON_AddItemToObject(jsonObj, "username-or-email", usernameOrEmail);
    }

   
    {
        // save only if user is under 13 and has approved disclaimer
        // default value is false, no need to store it.
        if (_under13 && _approvedUnder13Disclaimer) {
            cJSON *approvedUnder13Disclaimer = cJSON_CreateBool(_approvedUnder13Disclaimer);
            if (approvedUnder13Disclaimer == nullptr) {
                vxlog_error("can't create approvedUnder13Disclaimer json bool");
                cJSON_Delete(jsonObj);
                return;
            }
            cJSON_AddItemToObject(jsonObj, "approvedUnder13Disclaimer", approvedUnder13Disclaimer);
        }
    }
    
    if (_askedForMagicKey) {
        cJSON *askedForMagicKey = cJSON_CreateBool(_askedForMagicKey);
        if (askedForMagicKey == nullptr) {
            vxlog_error("can't create asked-for-magic-key json bool");
            cJSON_Delete(jsonObj);
            return;
        }
        cJSON_AddItemToObject(jsonObj, "asked-for-magic-key", askedForMagicKey);
    }
    
    char *jsonStr = cJSON_Print(jsonObj);
    cJSON_Delete(jsonObj);
    jsonObj = nullptr;

    FILE *accountFile = fs::openStorageFile(ACCOUNT_FILE, "wb");
    if (accountFile == nullptr) {
        vxlog_error("can't open %s", ACCOUNT_FILE);
        free(jsonStr);
        return;
    }
    fputs(jsonStr, accountFile);

    free(jsonStr);
    fclose(accountFile);
}

void Account::setUsername(const std::string& username) {
    this->_username = username;
}

void Account::setUsernameOrEmail(const std::string &usernameOrEmail) {
    this->_usernameOrEmail = usernameOrEmail;
    _save();
}

void Account::setAskedForMagicKey(bool askedForMagicKey) {
    this->_askedForMagicKey = askedForMagicKey;
    _save();
}

std::string Account::userID() {
    return HubClient::getInstance().getUserID();
}

const std::string Account::username() {
    return _username;
}

const std::string& Account::usernameOrEmail() {
    return _usernameOrEmail;
}

bool Account::askedForMagicKey() {
    return _askedForMagicKey;
}

void Account::setUnder13(const bool &b) {
    _under13 = b;
}

bool Account::under13() const {
    return _under13;
}

void Account::setParentApproved(const bool &b) {
    _parentApproved = b;
}

bool Account::parentApproved() const {
    return _parentApproved;
}

void Account::setChatEnabled(const bool &b) {
    _chatEnabled = b;
}

bool Account::chatEnabled() const {
    return _chatEnabled;
}

bool Account::under13DisclaimerRequired() {
    return _under13 && _approvedUnder13Disclaimer == false;
}

void Account::approveUnder13Disclaimer() {
    _approvedUnder13Disclaimer = true;
    _save();
}

void Account::update(const std::string& username,
                     const bool& hasBirthdate,
                     const bool& hasEmail,
                     const bool& hasPassword,
                     const bool& under13,
                     const bool& didCustomizeAvatar,
                     const bool& hasVerifiedPhoneNumber) {
    _username = username;
    _hasBirthdate = hasBirthdate;
    _hasEmail = hasEmail;
    _hasPassword = hasPassword;
    _under13 = under13;
    _didCustomizeAvatar = didCustomizeAvatar;
    _hasVerifiedPhoneNumber = hasVerifiedPhoneNumber;
}

void Account::setAuthenticated(const bool& authenticated) {
    _authenticated = authenticated;
}

bool Account::isAuthenticated() {
    return _authenticated;
}

void Account::reset() {
    _username = "";
    _usernameOrEmail = "";
    _askedForMagicKey = false;
    _under13 = true;
    _parentApproved = false;
    _chatEnabled = false;
    _approvedUnder13Disclaimer = false;
    _hasBirthdate = false;
    _hasEstimatedBirthdate = false;
    _hasEmail = false;
    _hasPassword = false;
    _authenticated = false;
    _hasVerifiedPhoneNumber = false;
    _hasUnverifiedPhoneNumber = false;
    _didCustomizeAvatar = false;
    _save();
}

bool Account::hasUsername() const {
    return _username.empty() == false;
}

bool Account::hasEmail() const {
    return _hasEmail;
}

void Account::setHasEmail(const bool v) {
    _hasEmail = v;
}

bool Account::hasVerifiedPhoneNumber() const {
    return _hasVerifiedPhoneNumber;
}

void Account::setHasVerifiedPhoneNumber(const bool v) {
    _hasVerifiedPhoneNumber = v;
}

bool Account::hasUnverifiedPhoneNumber() const {
    return _hasUnverifiedPhoneNumber;
}

void Account::setHasUnverifiedPhoneNumber(const bool v) {
    _hasUnverifiedPhoneNumber = v;
}

bool Account::isPhoneNumberExempted() const {
    return _isPhoneNumberExempted;
}

void Account::setIsPhoneNumberExempted(const bool v) {
    _isPhoneNumberExempted = v;
}

bool Account::hasPassword() const {
    return _hasPassword;
}

void Account::setHasDOB(const bool v) {
    _hasBirthdate = v;
}

bool Account::hasDOB() const {
    return _hasBirthdate;
}

void Account::setHasEstimatedDOB(const bool v) {
    _hasEstimatedBirthdate = v;
}

bool Account::hasEstimatedDOB() const {
    return _hasEstimatedBirthdate;
}

void Account::setHasPassword(const bool v) {
    _hasPassword = v;
}

void Account::resetBlockedUsers() {
    _blockedUsers.clear();
}

void Account::addBlockedUser(const std::string &userID) {
    _blockedUsers.push_back(userID);
}

const std::vector<std::string>& Account::blockedUsers() {
    return _blockedUsers;
}

