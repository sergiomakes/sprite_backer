#!/bin/bash

# Get the root directory (parent of scripts directory)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

###############################################################################
# Configuration
###############################################################################

SPRITE_BACKER_EXECUTABLE="sprite_backer"

###############################################################################
# Check if compiler is available
###############################################################################

if ! command -v clang &> /dev/null && ! command -v gcc &> /dev/null; then
    echo "Error: No C compiler found!"
    echo "Please install Xcode Command Line Tools: xcode-select --install"
    exit 1
fi

# Prefer clang on macOS, fall back to gcc
if command -v clang &> /dev/null; then
    CC=clang
else
    CC=gcc
fi

###############################################################################
# Compiler settings
###############################################################################

CFLAGS_COMMON="-Wall -Wextra -I${ROOT_DIR}/vendor -I${ROOT_DIR}/src -ffast-math -std=c17 -DPLATFORM_MACOS"
CFLAGS_DEBUG="-O0 -g -D_DEBUG -DDEBUG -DBUILD_INTERNAL"
CFLAGS_INTERNAL="-O2 -DNDEBUG -DBUILD_INTERNAL"
CFLAGS_RELEASE="-O2 -DNDEBUG"

LDFLAGS="-lm"

###############################################################################
# Create directories
###############################################################################

mkdir -p "${ROOT_DIR}/build"
cd "${ROOT_DIR}/build" || exit 1

###############################################################################
# Parse command line arguments
###############################################################################

BUILD_TYPE="debug"
if [ "$1" = "release" ]; then
    BUILD_TYPE="release"
elif [ "$1" = "internal" ]; then
    BUILD_TYPE="internal"
fi

# Set build-specific flags
if [ "$BUILD_TYPE" = "debug" ]; then
    CFLAGS="$CFLAGS_COMMON $CFLAGS_DEBUG"
elif [ "$BUILD_TYPE" = "release" ]; then
    CFLAGS="$CFLAGS_COMMON $CFLAGS_RELEASE"
elif [ "$BUILD_TYPE" = "internal" ]; then
    CFLAGS="$CFLAGS_COMMON $CFLAGS_INTERNAL"
else
    echo "Error: Unknown build type!"
    exit 1
fi

###############################################################################
# Compile sprite backer
###############################################################################

echo "Building sprite backer ($BUILD_TYPE) with $CC"
$CC $CFLAGS \
    "${ROOT_DIR}/src/main.c" \
    -o "$SPRITE_BACKER_EXECUTABLE" \
    $LDFLAGS

if [ $? -ne 0 ]; then
    echo "Error: Compilation failed!"
    exit 1
fi

