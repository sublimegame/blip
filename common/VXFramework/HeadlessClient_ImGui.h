//
//  HeadlessClient_ImGui.h
//  Cubzh
//
//  Created by Gaetan de Villele on 09/07/2020.
//

#pragma once

#ifdef P3S_CLIENT_HEADLESS

typedef struct {
    float x, y;
} ImVec2;

//#define ImVec2 struct {float x, y;}
#define ImU32 unsigned int
#define ImWchar32 unsigned int
#define ImWchar unsigned int
#define ImFont void

#endif
