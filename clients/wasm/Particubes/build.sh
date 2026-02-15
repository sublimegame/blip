#!/bin/bash
set -e

# --------------------------------------------------
# Detect host OS
# --------------------------------------------------

# Get the OS type using 'uname'
OS=$(uname)

if [[ "$OS" == "Linux" ]]; then
    echo "The operating system is Linux."
    OS_IS_LINUX=1
    OS_IS_MACOS=0
elif [[ "$OS" == "Darwin" ]]; then
    echo "The operating system is macOS."
    OS_IS_LINUX=0
    OS_IS_MACOS=1
else
    echo "Unknown operating system: $OS"
    exit 1
fi

# --------------------------------------------------
# Get CUBZH_TARGET envar
# --------------------------------------------------
# Supported targets:
# - wasm
# - wasm-discord

# Get the target from the environment variable
# If the environment variable is not set, default to "wasm"
CUBZH_TARGET=${CUBZH_TARGET:-wasm}

# If the target is empty, exit with an error
if [ -z "$CUBZH_TARGET" ]; then
	echo "❌ CUBZH_TARGET is empty"
	exit 1
fi

# Validate the target
# If the target is not supported, exit with an error
if [ "$CUBZH_TARGET" != "wasm" ] && [ "$CUBZH_TARGET" != "wasm-discord" ]; then
	echo "❌ CUBZH_TARGET has unsupported value: $CUBZH_TARGET"
	exit 1
fi

# Print the target
echo "⚙️ Building Cubzh Wasm... TARGET: $CUBZH_TARGET"

# create build directory if it doesn't exist
mkdir -p ./build

# --------------------------------------------------
# copy asset bundle in the wasm build directory
# --------------------------------------------------
rm -Rf ./build/bundle
# bundle files
cp -R ../../../bundle ./build/bundle
cp -R ../../../i18n ./build/bundle
# open-source Lua modules
cp -R ../../../lua/modules ./build/bundle

# --------------------------------------------------
# Execute the hasher program
# --------------------------------------------------

# Detect the operating system
OSNAME=$(uname -s)

# Detect the CPU architecture
ARCHNAME=$(uname -m)

HASHER_EXE="hasher"

# Select the correct hasher binary
case "$OSNAME" in
    Linux*)
        OS="linux"
        case "$ARCHNAME" in
            x86_64)
                ARCH="amd64"
                ;;
            arm*|aarch64)
                ARCH="arm64"
                ;;
            *)
                echo "Unsupported architecture: $ARCHNAME"
                exit 1
                ;;
        esac
        ;;
    Darwin*)
        OS="macos"
        case "$ARCHNAME" in
            x86_64)
                ARCH="amd64"
                ;;
            arm64)
                ARCH="arm64"
                ;;
            *)
                echo "Unsupported architecture: $ARCHNAME"
                exit 1
                ;;
        esac
        ;;
    CYGWIN*|MINGW32*|MSYS*|MINGW*)
        OS="windows"
        HASHER_EXE="hasher.exe"
        case "$ARCHNAME" in
            x86_64)
                ARCH="amd64"
                ;;
            arm*|aarch64)
                ARCH="arm64"
                ;;
            *)
                echo "Unsupported architecture: $ARCHNAME"
                exit 1
                ;;
        esac
        ;;
    *)
        echo "Unknown OS: $OSNAME"
        exit 1
        ;;
esac

# Execute the appropriate hasher binary
HASHER_BIN="../../deps/hasher/$OS/$ARCH/$HASHER_EXE"
$HASHER_BIN /repo

# --------------------------------------------------
# Build the wasm application
# --------------------------------------------------

# remove output (products)
rm -Rf ./build/output/*

# emcmake cmake -B ./build

if [ "$CUBZH_TARGET" = "wasm" ]; then
	emcmake cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Release -DSINGLE_THREAD=ON
elif [ "$CUBZH_TARGET" = "wasm-discord" ]; then
	emcmake cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Release -DSINGLE_THREAD=ON -DVX_ENABLE_URL_REWRITE_FOR_DISCORD=ON
else
	echo "❌ CUBZH_TARGET has unsupported value: $CUBZH_TARGET"
	exit 1
fi

emmake cmake --build ./build --parallel 10

# copy static files in the wasm output directory
cp -R ./static/* ./build/output

if [ "$CUBZH_TARGET" = "wasm" ]; then
	# Target "wasm" is for regular wasm version (app.cu.bzh)

	# sed command is a bit different on Linux and macOS
	if [ "$OS_IS_LINUX" = 1 ]; then
		# Add .gz extention to cubzh.wasm & cubzh.data in cubzh.js
		sed -i 's|cubzh\.wasm|cubzh.wasm.gz|g' ./build/output/cubzh.js
		sed -i 's|cubzh\.data|cubzh.data.gz|g' ./build/output/cubzh.js
	elif [ "$OS_IS_MACOS" = 1 ]; then
		# Add .gz extention to cubzh.wasm & cubzh.data in cubzh.js
		sed -i '' 's|cubzh\.wasm|cubzh.wasm.gz|g' ./build/output/cubzh.js
		sed -i '' 's|cubzh\.data|cubzh.data.gz|g' ./build/output/cubzh.js
	fi

elif [ "$CUBZH_TARGET" = "wasm-discord" ]; then
	# Target "wasm-discord" is for Discord Activity version (discord.cu.bzh)

	# sed command is a bit different on Linux and macOS
	if [ "$OS_IS_LINUX" = 1 ]; then
		# `\.` matches a literal dot and not any character
		sed -i 's|cubzh\.wasm|.proxy/cubzh.wasm.gz|g' ./build/output/cubzh.js
		sed -i 's|cubzh\.data|.proxy/cubzh.data.gz|g' ./build/output/cubzh.js
	elif [ "$OS_IS_MACOS" = 1 ]; then
		# `\.` matches a literal dot and not any character
		sed -i '' 's|cubzh\.wasm|.proxy/cubzh.wasm.gz|g' ./build/output/cubzh.js
		sed -i '' 's|cubzh\.data|.proxy/cubzh.data.gz|g' ./build/output/cubzh.js
	fi

fi
