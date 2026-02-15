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
cd "$PROJECT_ROOT"

MODULES="./lua/modules"
LUA_REQUIRE="./common/VXLuaSandbox/lua_require.cpp"

BLOCK="// HASHES START\n\
static std::map<std::string, std::string> hashes = {\n"

SALT="${LUAU_MODULES_HASH_SALT:?Error: LUAU_MODULES_HASH_SALT environment variable is not set}"


TMP_FILE=$(mktemp)

while IFS= read -r file; do
    # Compute the MD5 hash of the file's content
    file_name=$(basename "$file")

	SALTED="$SALT$(cat "$file")"
	echo "$SALTED" > "$TMP_FILE"
	
    hash=$(md5 -q "$TMP_FILE")
    BLOCK+="    {\"$file_name\", \"$hash\"},\n"
done < <(find "$MODULES" -type f -name "*.lua")

BLOCK+="};"

# echo -e "$BLOCK"

# Use sed to perform the replacement
awk -v new_content="$BLOCK" '
    BEGIN { found = 0; }
    /\/\/ HASHES START/ { print new_content; found = 1; next; }
    /\/\/ HASHES END/ { found = 0; }
    !found
' "$LUA_REQUIRE" > temp && mv temp "$LUA_REQUIRE"


