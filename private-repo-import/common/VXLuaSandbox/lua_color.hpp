//
//  lua_color.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 7/8/20.
//

#pragma once

// C++
#include <cstddef>
#include <map>
#include <vector>

// engine
#include "float3.h"
#include "int3.h"
#include "colors.h"
#include "color_palette.h"

typedef struct lua_State lua_State;

// default handlers

typedef struct color_C_handler_userdata {
    RGBAColor color;
    hsv *hsvColor; // NULL by default, RGB always source of truth
    bool light;
} color_C_handler_userdata;

typedef void (*color_C_handler_set) (const uint8_t *r,
                                     const uint8_t *g,
                                     const uint8_t *b,
                                     const uint8_t *a,
                                     const bool *light,
                                     lua_State *L,
                                     void *userdata);

typedef void (*color_C_handler_get) (uint8_t *r,
                                     uint8_t *g,
                                     uint8_t *b,
                                     uint8_t *a,
                                     bool *light,
                                     lua_State *L,
                                     const void *userdata);

typedef void (*color_C_handler_set_hsv) (const double *h,
                                         const double *s,
                                         const double *v,
                                         lua_State *L,
                                         void *userdata);

typedef void (*color_C_handler_get_hsv) (double *h,
                                         double *s,
                                         double *v,
                                         lua_State *L,
                                         void *userdata);

typedef void (*color_C_handler_free) (void *userdata);

//
void lua_g_color_create_and_push(lua_State *L);

/// Prints global Color
int lua_g_color_snprintf(lua_State *L,
                         char* result,
                         size_t maxLen,
                         bool spacePrefix);

/// Prints Color instance
int lua_color_snprintf(lua_State *L,
                       char* result,
                       size_t maxLen,
                       bool spacePrefix);

// Creates Color table and pushes it on the top of the stack
void lua_color_create_and_push_table(lua_State *L,
                                     const float r,
                                     const float g,
                                     const float b,
                                     const float a,
                                     const bool light,
                                     const bool readOnly);

// Creates Color table and pushes it on the top of the stack
void lua_color_create_and_push_table(lua_State *L,
                                     const uint8_t r,
                                     const uint8_t g,
                                     const uint8_t b,
                                     const uint8_t a,
                                     const bool light,
                                     const bool readOnly);

//
bool lua_color_setHandlers(lua_State *L,
                           const int idx,
                           color_C_handler_set set,
                           color_C_handler_get get,
                           color_C_handler_free free_userdata,
                           void *userdata);

bool lua_color_getHSV(lua_State *L, const int idx, double *h, double *s, double *v);
bool lua_color_setHSV(lua_State *L, const int idx, const double *h, const double *s, const double *v);

bool lua_color_getRGBAL(lua_State *L, const int idx, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a, bool *light);
bool lua_color_setRGBAL(lua_State *L, const int idx, const uint8_t *r, const uint8_t *g, const uint8_t *b, const uint8_t *a, const bool *light);

bool lua_color_getRGBAL_float(lua_State *L, const int idx, float *r, float *g, float *b, float *a, bool *light);
bool lua_color_setRGBAL_float(lua_State *L, const int idx, const float *r, const float *g, const float *b, const float *a, const bool *light);
