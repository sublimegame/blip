#!/bin/bash

set -e

START_LOCATION="$PWD"
SCRIPT_LOCATION=$(cd -P -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)

INPUT_DIR="$1"
OUTPUT_DIR="$2"

echo $INPUT_DIR
echo $OUTPUT_DIR

OUTPUT_EXTENTION=".flac"

rm -f "$OUTPUT_DIR"/*.wav
rm -f "$OUTPUT_DIR"/*.mp3
rm -f "$OUTPUT_DIR"/*.flac
rm -f "$OUTPUT_DIR"/*.ogg

for f in "$INPUT_DIR"/*.wav
do
	filename=$(basename -- "$f")
	
	newName=${filename#*_} 
	newName=$(echo "$newName" | tr '[:upper:]' '[:lower:]')
	newName="${newName/-/_}"
	newName="${newName/ — /_}"
	newName="${newName/—/_}"
	newName="${newName/ /}"
	newName="${newName/_0/_}"
	newName="${newName/_alt_/_}"

	if [ "$newName" = "piano_1.wav" ]
	then
		newName="piano_attack.wav"
	elif [ "$newName" = "piano_2.wav" ]
	then
		newName="piano_sustain.wav"
	elif [ "$newName" = "laser_gun_shot_laser_gun_shot_child_1.wav" ]
	then
		continue
	fi

	echo "$filename -> $newName"

	cp "$f" "$OUTPUT_DIR"/"$newName"
done

for f in "$OUTPUT_DIR"/*.wav
do 
	ffmpeg -i "$f" "${f%.*}$OUTPUT_EXTENTION"
done

rm -f "$OUTPUT_DIR"/*.wav

echo "DONE!"


