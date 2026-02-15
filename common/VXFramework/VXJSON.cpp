//
//  VXJSON.cpp
//  Cubzh
//
//  Created by Adrien Duermael on 04/08/2022.
//

#include "VXJSON.hpp"

using namespace vx;

bool JSON::readString(const cJSON* node, const char* field, std::string& outValue) {
    if (cJSON_HasObjectItem(node, field)) {
        const cJSON *tagEntry = cJSON_GetObjectItem(node, field);
        if (cJSON_IsString(tagEntry)) {
            char *str = tagEntry->valuestring;
            if (str != nullptr) {
                outValue = std::string(str);
                return true;
            }
        }
    }
    return false;
}

bool JSON::readInt(const cJSON* node, const char* field, int& outValue) {
    if (cJSON_HasObjectItem(node, field)) {
        const cJSON *tagEntry = cJSON_GetObjectItem(node, field);
        if (cJSON_IsNumber(tagEntry)) {
            outValue = tagEntry->valueint;
            return true;
        }
    }
    return false;
}

bool JSON::readDouble(const cJSON* node, const char* field, double& outValue) {
    if (cJSON_HasObjectItem(node, field)) {
        const cJSON *tagEntry = cJSON_GetObjectItem(node, field);
        if (cJSON_IsNumber(tagEntry)) {
            outValue = tagEntry->valuedouble;
            return true;
        }
    }
    return false;
}

bool JSON::readBool(const cJSON* node, const char* field, bool& outValue) {
    if (cJSON_HasObjectItem(node, field)) {
        const cJSON *tagEntry = cJSON_GetObjectItem(node, field);
        if (cJSON_IsBool(tagEntry)) {
            outValue = cJSON_IsTrue(tagEntry);
            return true;
        }
    }
    return false;
}
