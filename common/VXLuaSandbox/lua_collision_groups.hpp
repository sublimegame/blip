//
//  lua_collision_groups.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 19/09/2021.
//

// A CollisionGroups table is an array of collision groups.
// Each collision group is an integer, from 1 to 8.
// The same group can't be present twice in the array.

#pragma once

#include <stddef.h>
#include <cstdint>

typedef struct lua_State lua_State;

// Creates and pushes global CollisionGroups table
void lua_g_collision_groups_create_and_push(lua_State *L);

// Pushes a CollisionGroups table with given mask
void lua_collision_groups_create_and_push(lua_State *L, uint16_t mask);

// Converts the groupNumber into a mask and pushes a CollisionGroups table with the mask
// If return value is false, pushes nothing (groupNumber is invalid)
bool lua_collision_groups_create_and_push_with_group_number(lua_State *L, int groupNumber);

bool lua_collision_groups_group_number_is_valid(uint16_t groupNumber);

uint16_t lua_collision_groups_group_number_to_mask(uint16_t groupNumber);

// Tries to convert value at idx into CollisionGroups mask.
// Returns true on success, false otherwise.
bool lua_collision_groups_get_mask(lua_State *L, const int idx, uint16_t *mask);

// Creates CollisionGroups from table at idx
// Removing incorrect values and duplicates.
// Also making sure group numbers are sorted in ascending order.
// This call can fail, it returns false in that case, and nothing
// is pushed over the stack.
bool lua_collision_groups_push_from_table(lua_State *L, const int idx);

/// Prints global CollisionGroups
int lua_g_collision_groups_snprintf(lua_State *L,
                                    char* result,
                                    size_t maxLen,
                                    bool spacePrefix);

/// Prints CollisionGroups instance
int lua_collision_groups_snprintf(lua_State *L,
                                  char* result,
                                  size_t maxLen,
                                  bool spacePrefix);

