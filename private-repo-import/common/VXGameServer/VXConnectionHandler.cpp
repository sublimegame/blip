//
//  VXConnectionHandler.cpp
//  Cubzh
//
//  Created by Gaetan de Villele on 09/02/2022.
//

#include "VXConnectionHandler.hpp"

using namespace vx;

ConnectionHandler::ConnectionHandler(GameServer* gameserver) :
_gameServer(gameserver) {}

ConnectionHandler::~ConnectionHandler() {}
