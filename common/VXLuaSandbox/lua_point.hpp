//
//  lua_point.hpp
//  Cubzh
//
//  Created by Gaetan de Villele on 20/05/2021.
//

#pragma once

// C++
#include <cstddef>
#include <string>

typedef struct lua_State lua_State;
typedef struct _Shape Shape;

// --------------------------------------------------
// MARK: - Point instances -
// --------------------------------------------------

/// Pushes a shape point representation or nil if not found.
/// Creates the point at 0,0,0 if createIfNotFound == true
void lua_point_push(lua_State *L,
                    const std::string& pointName,
                    Shape *parentShape,
                    bool createIfNotFound);

/// Extracts the Position and Rotation values of the Lua Point at the provided index `idx`.
/// Returns true on success.
bool lua_point_getValues(lua_State *L,
                         const int idx,
                         Shape **parentShape,
                         float *posX, float *posY, float *posZ,
                         float *rotX, float *rotY, float *rotZ);

bool lua_point_setValues(lua_State *L,
                         const int idx,
                         const float *posX, const float *posY, const float *posZ,
                         const float *rotX, const float *rotY, const float *rotZ);

/// Lua print function for Shape instance tables
int lua_point_snprintf(lua_State *L,
                       char *result,
                       size_t maxLen,
                       bool spacePrefix);
