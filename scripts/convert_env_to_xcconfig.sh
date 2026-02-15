#!/bin/bash

# Script to convert .env file to Xcode configuration format
# Usage: ./convert_env_to_xcconfig.sh [path_to_env_file]

set -e

# Default paths
ENV_FILE="${1:-.env}"
XCCONFIG_FILE="clients/ios-macos/Particubes/env.xcconfig"

# Check if .env file exists
if [[ ! -f "$ENV_FILE" ]]; then
    echo "Error: .env file '$ENV_FILE' not found!"
    echo "Usage: $0 [path_to_env_file]"
    exit 1
fi

# Create xcconfig file and parent directories if they don't exist
if [[ ! -f "$XCCONFIG_FILE" ]]; then
    echo "Creating xcconfig file '$XCCONFIG_FILE'..."
    mkdir -p "$(dirname "$XCCONFIG_FILE")"
fi

echo "Converting $ENV_FILE to $XCCONFIG_FILE..."

# Start the GCC_PREPROCESSOR_DEFINITIONS line
echo -n "GCC_PREPROCESSOR_DEFINITIONS = \$(inherited)" > "$XCCONFIG_FILE"

# Process each line in the .env file
while IFS= read -r line || [[ -n "$line" ]]; do
    # Skip empty lines and comments
    if [[ -z "$line" || "$line" =~ ^[[:space:]]*# ]]; then
        continue
    fi
    
    # Extract key=value pairs
    if [[ "$line" =~ ^[[:space:]]*([A-Za-z_][A-Za-z0-9_]*)=(.*)$ ]]; then
        key="${BASH_REMATCH[1]}"
        value="${BASH_REMATCH[2]}"
        
        # Remove surrounding quotes if present
        if [[ "$value" =~ ^\"(.*)\"$ ]]; then
            value="${BASH_REMATCH[1]}"
        elif [[ "$value" =~ ^\'(.*)\'$ ]]; then
            value="${BASH_REMATCH[1]}"
        fi
        
        # Escape quotes in the value
        escaped_value="${value//\"/\\\"}"
        
        # Add to preprocessor definitions
        echo -n " $key=\"\\\"$escaped_value\\\"\"" >> "$XCCONFIG_FILE"
    fi
done < "$ENV_FILE"

# Add newline at the end
echo "" >> "$XCCONFIG_FILE"

echo "Successfully converted $ENV_FILE to $XCCONFIG_FILE"
echo "Generated xcconfig content:"
cat "$XCCONFIG_FILE"
