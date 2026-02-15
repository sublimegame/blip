#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
WORKSPACE="$REPO_ROOT/clients/ios-macos/particubes.xcworkspace"

SCHEME="macos"
CONFIGURATION="${1:-Debug}"

echo "Building macOS ($CONFIGURATION)..."
xcodebuild \
    -workspace "$WORKSPACE" \
    -scheme "$SCHEME" \
    -configuration "$CONFIGURATION" \
    build

echo "macOS build succeeded."
