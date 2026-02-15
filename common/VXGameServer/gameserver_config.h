//
//  gameserver_config.h
//  Cubzh
//
//  Created by Adrien Duermael on 17/03/2021.
//

#pragma once

// whem true, the gameserver does not interract with the hub
// and remains alive even if there's no connected player.
#define LOCAL_DEBUG_GAMESERVER false

#if LOCAL_DEBUG_GAMESERVER
#define SHUTDOWN_DELAY 1000000 // milliseconds
#else
#define SHUTDOWN_DELAY 30000 // milliseconds
#endif

#define HUB_UPDATE_DELAY 20000 // milliseconds

// Waiting for inputs for that number of frames.
// Server is always that amount of frames late for simulation.
// Inputs that are too old are discarded.
// From client perspective:
// - distant entities are rendered with info from the past
//   (you actually see other players a where they were a few frames ago)
// - local entities extrapolate info received from server,
//   re-applying unprocessed inputs to see if everything is on track.
// NOTE: when the client sends its frame, it's already 50ms old.
// So FRAME_BUFFER_FOR_INPUTS == 3 means we allow 100ms to travel from client to server
#define FRAME_BUFFER_FOR_INPUTS 4

// Server uses fixed frame durations
#define FRAME_DURATION_MS 50
#define FRAME_DURATION_S 0.05

