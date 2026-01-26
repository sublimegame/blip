#!/bin/bash

set -e

START_LOCATION="$PWD"
SCRIPT_LOCATION=$(cd -P -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)

INPUT="$1"
OUTPUT_FILE="$2"

echo $INPUT
echo $OUTPUT_FILE

/Applications/Blender.app/Contents/MacOS/Blender -b -P "$SCRIPT_LOCATION"/objs-to-gltf.py -- "$INPUT" "$OUTPUT_FILE"
