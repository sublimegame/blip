#!/bin/bash

set -e

START_LOCATION="$PWD"
SCRIPT_LOCATION=$(cd -P -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
# Go back to start location when script exits
trap "cd $START_LOCATION" EXIT
# Go to script location before running git command
# to make sure it runs within project tree
cd "$SCRIPT_LOCATION"
# Use git command to get root project directory.
PROJECT_ROOT=$(git rev-parse --show-toplevel)
# The script is now executed from project root directory
cd $SCRIPT_LOCATION # cd "$PROJECT_ROOT"

# Destination directory for compiled binaries
binariesPath="$PROJECT_ROOT/deps/hasher"

# Compile for Linux (Intel 64-bit)
GOOS=linux GOARCH=amd64 go build -o "$binariesPath/linux/amd64/hasher"

# Compile for Linux (ARM 64-bit)
GOOS=linux GOARCH=arm64 go build -o "$binariesPath/linux/arm64/hasher"

# Compile for macOS (Intel 64-bit)
GOOS=darwin GOARCH=amd64 go build -o "$binariesPath/macos/amd64/hasher"

# Compile for macOS (Apple Silicon)
GOOS=darwin GOARCH=arm64 go build -o "$binariesPath/macos/arm64/hasher"

# Compile for Windows (64-bit)
GOOS=windows GOARCH=amd64 go build -o "$binariesPath/windows/amd64/hasher.exe"
