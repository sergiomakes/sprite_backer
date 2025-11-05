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

if ! command -v gcc &> /dev/null && ! command -v clang &> /dev/null; then
    echo "Error: No C compiler found!"
    echo "Please install gcc or clang."
    exit 1
fi

# Prefer gcc, fall back to clang
if command -v gcc &> /dev/null; then
    CC=gcc
else
    CC=clang
fi

###############################################################################
# Compiler settings
###############################################################################

CFLAGS_COMMON="-Wall -Wextra -I${ROOT_DIR}/vendor -I${ROOT_DIR}/src -ffast-math -std=c17 -DPLATFORM_LINUX"
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

