#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
WORKSPACE="$REPO_ROOT/clients/ios-macos/particubes.xcworkspace"

SCHEME="ios"
CONFIGURATION="${1:-Debug}"

echo "Building iOS ($CONFIGURATION)..."
xcodebuild \
    -workspace "$WORKSPACE" \
    -scheme "$SCHEME" \
    -sdk iphoneos \
    -destination 'generic/platform=iOS' \
    -configuration "$CONFIGURATION" \
    build \
    CODE_SIGN_IDENTITY=- \
    CODE_SIGNING_REQUIRED=NO \
    CODE_SIGNING_ALLOWED=NO

echo "iOS build succeeded."
