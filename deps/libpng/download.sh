#!/bin/bash

# Flags:
# -v: the version of libpng to download. Can be "latest" or a specific version number (e.g. "1.6.47"). Defaults to "latest".

# exit on error
set -e

# exit on undefined variable
set -u

# exit on pipe error
set -o pipefail

# print commands
# set -x

# Parse flags and store them in variables
# Flags:
# -v: The version of libpng to download. Can be "" or a specific version number (e.g. "1.6.47"). Defaults to "".
#     If it is "", the latest version will be downloaded.
# -o: The output directory for the downloaded source code. Defaults to the version number. 
#     The archive will be downloaded to this directory (as "archive.tar.gz") and extracted to a "src" subdirectory.
# LATER: 
# - add a "force" flag to force the download of the source code (even if it was already downloaded)
# - add a "cleanup" flag to remove the downloaded archive after it has been extracted
version=""
output_dir=""
quiet=false
while getopts ":v:o:q" opt; do
  case ${opt} in
    v ) version=$OPTARG ;;
    o ) output_dir=$OPTARG ;;
    q ) quiet=true ;;
    \? ) echo "Usage: $0 [-v version] [-o output_dir] [-q]" ;;
  esac
done

# if version is "", get the latest version from the GitHub API
# if [ -z "$version" ]; then
#   version=$(curl -s https://api.github.com/repos/luau-lang/luau/releases/latest | jq -r '.tag_name')
# fi

# Validate required parameters
if [ -z "$version" ]; then
  echo "⚠️ Version is not specified"
  echo "Usage: $0 [-v version] [-o output_dir]"
  exit 1
fi

# if output directory is not provided, use the version as the output directory
if [ -z "$output_dir" ]; then
  output_dir="$version"
fi

if [ "$quiet" = false ]; then
  echo "🛠️ Creating directory for libpng ${version}..."
fi

# SCRIPT_PARENT_DIR_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# VERSION_PATH="$SCRIPT_PARENT_DIR_PATH/$version"
VERSION_PATH="$output_dir"

# Creates the directory
# (does nothing if it already exists)
mkdir -p $VERSION_PATH

if [ "$quiet" = false ]; then
  echo -n "📡 Downloading libpng $version source code..."
fi

# Destination path for the archive file to download
ARCHIVE_PATH="${VERSION_PATH}/archive.tar.gz"

# Download the source code from the release
if [ ! -f "$ARCHIVE_PATH" ]; then
  curl -L https://github.com/pnggroup/libpng/archive/refs/tags/v$version.tar.gz -o $ARCHIVE_PATH > /dev/null 2>&1
  if [ "$quiet" = false ]; then
    echo " Done."
  fi
else
  if [ "$quiet" = false ]; then
    echo " Done. (was already downloaded)"
  fi
fi

# --- Extract archive ---

# Path of the archive content, once extracted
EXTRACTED_DIR_PATH="$VERSION_PATH/libpng-$version"

# Delete the extracted directory if it already exists
rm -rf $EXTRACTED_DIR_PATH

# Extract the archive next to the archive file
tar -xvzf $ARCHIVE_PATH -C $VERSION_PATH > /dev/null 2>&1

# --- Rename the extracted directory to "src" ---

SRC_DIR_PATH="$VERSION_PATH/src"

# Remove "src" directory if it exists
rm -rf $SRC_DIR_PATH

# Rename the extracted directory to "src"
mv $EXTRACTED_DIR_PATH $SRC_DIR_PATH

# Remove the archive (tar.gz) file
rm $ARCHIVE_PATH

if [ "$quiet" = false ]; then
  echo -e "✅ Downloaded libpng $version source code \n  -> ${SRC_DIR_PATH}"
fi
