#!/bin/bash

#
# STORING CREDENTIALS FOR NOTARY TOOL
#
# xcrun notarytool store-credentials
#
# Profile name:
# cubzh-notary-credentials
#
# Developer Apple ID:
# adrien@voxowl.com
# App-specific password for adrien@voxowl.com:
# Developer Team ID:
# 9JFN8QQG65
#
# Validating your credentials...
# Success. Credentials validated.
# Credentials saved to Keychain.
# To use them, specify `--keychain-profile "cubzh-notary-credentials"`
#

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
CONFIG_JSON="$PROJECT_ROOT/common/assets/config.json"

# The script is now executed from project root directory
cd "$PROJECT_ROOT"

TARGET=$1

while true; do
    if [ "$TARGET" = "mac" ] || [ "$TARGET" = "ios" ] || [ "$TARGET" = "android" ]
    then
        break
    fi
    read -p "TARGET: mac, ios, android? " TARGET;
    # echo "option not supported"
done

echo "Target: $TARGET"

DIRTY=$(git diff --quiet || echo '-dirty')
COMMIT=$(git rev-parse --short=8 HEAD)
COMMIT="$COMMIT$DIRTY" # add -dirty suffix if needed

echo "Commit: $COMMIT"

if [ "$TARGET" = "mac" ]
then
	# https://developer.apple.com/documentation/xcode/notarizing_macos_software_before_distribution/customizing_the_notarization_workflow?preferredLanguage=occ

	PRODUCT_NAME="Cubzh"
	EXPORT_PATH="./distribution/builds"
	BUILD_PATH="./distribution/macOS/build"
	EXPORT_LOGS="$BUILD_PATH/distribute-mac.log"
	ARCHIVE_PATH="$BUILD_PATH/particubes.xcarchive"
	EXPORT_OPTIONS="./distribution/macOS/ExportOptions.plist"

	APP_PATH="$BUILD_PATH/$PRODUCT_NAME.app"
	ZIP_PATH="$BUILD_PATH/$PRODUCT_NAME.zip"

	# first zip is used to send the app to the notarization service
	# this one is for the final result, zipped notarized app
	END_ZIP_PATH="$EXPORT_PATH/$PRODUCT_NAME.zip"
	END_VERSION_LOG_PATH="$EXPORT_PATH/mac.txt"

	# remove everything from build directory
	set +e
	rm "$BUILD_PATH/"* 2> /dev/null
	set -e

	# remove last macOS build
	set +e
	rm "$END_ZIP_PATH" 2> /dev/null
	set -e

	# ARCHIVE
	echo "-- ARCHIVE --" >> "$EXPORT_LOGS"
	printf "ARCHIVE ⏳"

	xcodebuild clean -workspace "ios-macos/particubes.xcworkspace" -scheme "macos" >> "$EXPORT_LOGS" 2>&1

	xcodebuild \
	-workspace "ios-macos/particubes.xcworkspace" \
	-scheme "macos" \
	-configuration Release \
	-archivePath "$ARCHIVE_PATH" \
	archive >> "$EXPORT_LOGS" 2>&1

	printf "\rARCHIVE ✅\n"

	# EXPORT .APP
	echo "-- EXPORT .APP --" >> "$EXPORT_LOGS"
	printf "EXPORT .APP ⏳"

	xcodebuild \
	-allowProvisioningUpdates -exportArchive \
	-archivePath "$ARCHIVE_PATH" \
	-exportOptionsPlist "$EXPORT_OPTIONS" \
	-exportPath "$BUILD_PATH" >> "$EXPORT_LOGS" 2>&1

	printf "\rEXPORT .APP ✅\n"

	# ZIP
	echo "-- ZIP --" >> "$EXPORT_LOGS"
	printf "ZIP ⏳"
	ditto -c -k --keepParent "$APP_PATH" "$ZIP_PATH" >> "$EXPORT_LOGS" 2>&1
	printf "\rZIP ✅\n"

	# NOTARIZE
	echo "-- NOTARIZE --" >> "$EXPORT_LOGS"
	printf "NOTARIZE ⏳"
	set +e
	rm ~/Library/Caches/com.apple.amp.itmstransporter/UploadTokens/*.token 2> /dev/null
	set -e

	xcrun notarytool submit \
	--keychain-profile "cubzh-notary-credentials" \
	--wait \
	"$ZIP_PATH" \
	>> "$EXPORT_LOGS" 2>&1

	no_errors=$(tail -n 4 "$EXPORT_LOGS" | grep "  status: Accepted")

	if [ -z "$no_errors" ]
	then
		printf "\rNOTARIZE ❌\n"
		exit 1
	fi

	request_uuid=$(tail -n 4 "$EXPORT_LOGS" | grep "  id: ")
	request_uuid=$(cut -d '=' -f 2 <<< "$request_uuid")
	request_uuid=$(echo "$request_uuid" | sed 's/ //g')

	printf "\rNOTARIZE ✅ - request UUID: $request_uuid\n"

	# STAPLE
	echo "-- STAPLE --" >> "$EXPORT_LOGS"
	printf "STAPLE ⏳"
	xcrun stapler staple "$APP_PATH" >> "$EXPORT_LOGS" 2>&1
	printf "\rSTAPLE ✅\n"

	# ZIP NOTARIZED APP
	echo "-- ZIP NOTARIZED APP --" >> "$EXPORT_LOGS"
	printf "ZIP NOTARIZED APP ⏳"
	ditto -c -k --keepParent "$APP_PATH" "$END_ZIP_PATH" >> "$EXPORT_LOGS" 2>&1
	printf "\rZIP NOTARIZED APP ✅\n"

	# WRITE LAST VERSION EXPORTED
	echo "$COMMIT" > "$END_VERSION_LOG_PATH"
	printf "DONE! ✅\n"
fi
