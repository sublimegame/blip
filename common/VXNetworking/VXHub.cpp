//
//  VXHub.cpp
//  Blip
//
//  Created by Gaetan de Villele on 29/08/2024.
//

#include "VXHub.hpp"

using namespace vx::hub;

// MARK: - Static functions

std::string PushToken::Value(const Type tokenType) {
    switch (tokenType) {
        case PushToken::Type::Apple:
            return "apple";
        case PushToken::Type::Firebase:
            return "firebase";
            // case PushToken::Type::Windows:
            // return "windows";
    }
    return "";
}

// MARK: - Constructor / Destructor

PushToken::PushToken(const std::string& type, 
                     const std::string& token,
                     const std::string& variant) :
_type(type),
_token(token),
_variant(variant) {}

PushToken::~PushToken() {}

// MARK: - Public functions

std::string PushToken::encodeToJSONString() const {
    cJSON * const jsonObj = _encodeToJSON();
    char *cStr = cJSON_PrintUnformatted(jsonObj);
    const std::string str(cStr);
    free(cStr);
    cJSON_Delete(jsonObj);
    return str;
}

// MARK: - Private functions

cJSON *PushToken::_encodeToJSON() const {
    cJSON *json = cJSON_CreateObject();
    if (json == nullptr) {
        return nullptr; // failure
    }

    bool ok = false;
    ok = vx::json::writeStringField(json, "type", _type);
    if (!ok) {
        return nullptr; // failure
    }
    ok = vx::json::writeStringField(json, "token", _token);
    if (!ok) {
        return nullptr; // failure
    }
    ok = vx::json::writeStringField(json, "variant", _variant);
    if (!ok) {
        return nullptr; // failure
    }
    return json; // success
}
