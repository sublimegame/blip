#!/bin/bash

# 
# This is designed to be used inside the GameServer Docker container
# 

set -e

# Use provided parallel jobs argument or default to 4
parallel_jobs=${1:-4}

cd /project/common/VXGameServer
cmake -DCMAKE_BUILD_TYPE=Release -G Ninja .
cmake --build . --parallel $parallel_jobs

# generated executable is /project/common/VXGameServer/gameserver
