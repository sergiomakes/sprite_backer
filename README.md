# Sprite Backer

[![Build Release](https://github.com/wherd/sprite_backer/actions/workflows/release.yml/badge.svg)](https://github.com/wherd/sprite_backer/actions/workflows/release.yml)

A lightweight, efficient sprite atlas generator that packs images and font glyphs into a single texture atlas using the MaxRects bin packing algorithm.

## Features

- **Image Packing**: Combine multiple PNG images into a single texture atlas
- **Font Rendering**: Rasterize TrueType fonts and pack glyphs into the atlas
- **Efficient Packing**: Uses MaxRects bin packing algorithm for optimal space utilization
- **C Header Export**: Generates ready-to-use C header files with sprite definitions and UV coordinates
- **Simple Configuration**: Text-based config file format
- **Zero Dependencies**: Uses only stb single-header libraries (included)

## Installation

### Pre-built Binaries

Download the latest release for your platform from the [Releases page](https://github.com/wherd/sprite_backer/releases):

- **Windows**: `sprite_backer-windows-x64.exe`
- **Linux**: `sprite_backer-linux-x64.tar.gz`
- **macOS**: `sprite_backer-macos-universal.tar.gz`

Extract and run the executable directly.

### Building from Source

### Windows

Run the build script:

```bash
scripts\build-windows.bat
```

This will create `build\sprite_backer.exe`.

### Linux

Run the build script:

```bash
chmod +x scripts/build-linux.sh
scripts/build-linux.sh
```

This will create `build/sprite_backer`.

### macOS

Run the build script:

```bash
chmod +x scripts/build-macos.sh
scripts/build-macos.sh
```

This will create `build/sprite_backer`.

**Build Types:**

All platforms support different build types:
- `debug` (default) - No optimization, debug symbols
- `internal` - Optimized with internal tools enabled
- `release` - Full optimization, no debug symbols

Example:
```bash
# Windows
scripts\build-windows.bat release

# Linux/macOS
scripts/build-linux.sh release
```

## Usage

```bash
sprite_backer <config_file> <output_name>
```

**Example:**
```bash
sprite_backer config.txt spritesheet
```

This will generate:
- `spritesheet.gen.png` - The packed texture atlas
- `spritesheet.gen.h` - C header file with sprite definitions

## Configuration File Format

The config file is a simple text format with the following commands:

### ATLAS_SIZE

Sets the width and height of the atlas (square).

```
ATLAS_SIZE 1024
```

### IMAGE

Adds an image to the atlas.

```
IMAGE <filename> <name>
```

**Example:**
```
IMAGE sprites/player.png PLAYER
IMAGE sprites/enemy.png ENEMY
```

### FONT

Adds a font with specific glyphs to the atlas.

```
FONT <filename> <size> <charset> <name>
```

**Example:**
```
FONT fonts/RobotoMono.ttf 16 ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 BODY
FONT fonts/Arial.ttf 24 0123456789:. TIMER
```

### Comments

Lines starting with `#` are treated as comments.

```
# This is a comment
```

### Complete Example

```
# Sprite atlas configuration
ATLAS_SIZE 2048

# UI sprites
IMAGE sprites/button.png BUTTON
IMAGE sprites/panel.png PANEL

# Fonts
FONT fonts/RobotoMono.ttf 16 ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!?.,;:'"()[]{}+-*/=<>@#$%&_ BODY
FONT fonts/RobotoMono.ttf 32 ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 TITLE
```

## Generated Header File

The generated header file contains:

- `sprite_t` struct with position, size, and UV coordinates
- `sprite_id` enum with all sprite names
- `BACKED_SPRITE_LIST[]` array with sprite data
- Font glyph data structures with kerning information

**Example usage in your code:**

```c
#include "spritesheet.gen.h"

// Access sprite data
sprite_t player_sprite = BACKED_SPRITE_LIST[SPRITE_PLAYER];

// Use UV coordinates for rendering
float u0 = player_sprite.u0;
float v0 = player_sprite.v0;
float u1 = player_sprite.u1;
float v1 = player_sprite.v1;
```

## Technical Details

- **Packing Algorithm**: MaxRects (Maximal Rectangles) with Best Short Side Fit heuristic
- **Padding**: 2-pixel padding around each sprite to prevent texture bleeding
- **Image Format**: RGBA PNG (32-bit)
- **Font Rendering**: Uses stb_truetype for high-quality font rasterization

## Dependencies

All dependencies are included in the `vendor/stb` directory:

- [stb_image.h](https://github.com/nothings/stb/blob/master/stb_image.h) - Image loading
- [stb_image_write.h](https://github.com/nothings/stb/blob/master/stb_image_write.h) - PNG writing
- [stb_truetype.h](https://github.com/nothings/stb/blob/master/stb_truetype.h) - Font rendering

## License

This project uses the stb libraries which are in the public domain. See the individual stb header files for details.
