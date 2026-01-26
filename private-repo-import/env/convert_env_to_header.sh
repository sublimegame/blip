#!/usr/bin/env sh
set -e

ENV_FILE="${1:-../.env}"
OUT_FILE="${2:-envvars.h}"

echo "// Auto-generated from $ENV_FILE" > "$OUT_FILE"

while IFS= read -r line; do
    # skip empty lines or comments
    [ -z "$line" ] && continue
    echo "$line" | grep -q '^[[:space:]]*#' && continue

    key=$(echo "$line" | cut -d '=' -f1 | xargs)
    value=$(echo "$line" | cut -d '=' -f2- | xargs)

    # if value is all digits, don't quote it
    if echo "$value" | grep -Eq '^[0-9]+$'; then
        echo "#define $key $value" >> "$OUT_FILE"
    else
        echo "#define $key \"$value\"" >> "$OUT_FILE"
    fi
done < "$ENV_FILE"
