#!/bin/bash

# Check if the user provided a directory as an argument
if [ -z "$1" ]; then
  echo "Usage: $0 <directory>"
  exit 1
fi

# Function to extract and print unique file extensions
function find_extensions() {
  local dir="$1"
  
  find "$dir" -type f | sed -n 's/.*\.\([a-zA-Z0-9]*\)$/\1/p' | sort -u
}

# Call the function with the provided directory
find_extensions "$1"