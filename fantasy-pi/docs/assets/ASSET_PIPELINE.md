# Asset Pipeline Manual

## Overview

The asset pipeline converts source files (PNG, WAV) into console-native binary formats and packs them into the ROM image.

## Supported Source Formats

| Source | Output Type | Notes                          |
|--------|-------------|--------------------------------|
| PNG    | sprite, image, tileset, font | All channel configurations     |
| WAV    | audio       | 8/16-bit, mono/stereo          |
| JSON   | tilemap     | Tiled-compatible format        |
| RAW    | binary      | Direct embedding               |

## Asset Configuration

Assets are declared in assembly:

```asm
.asset name, "path", type=sprite, format=rgba8888, alpha=premultiplied
```

### Parameters

| Parameter  | Values                                   | Default     |
|------------|------------------------------------------|-------------|
| type       | sprite, image, tileset, spritesheet, font, audio, palette | sprite |
| format     | rgba8888, argb8888, rgb565, rgba4444, indexed8, indexed8alpha | rgba8888 |
| alpha      | straight, premultiplied, none            | straight    |
| tilew      | 1-256                                    | 0           |
| tileh      | 1-256                                    | 0           |
| framew     | 1-512                                    | 0           |
| frameh     | 1-512                                    | 0           |
| glyphw     | 1-32                                     | 0           |
| glyphh     | 1-32                                     | 0           |
| compress   | true, false                              | false       |
| colorkey   | #RRGGBB                                  | none        |

## Alpha Handling

### Straight Alpha (Source)
```
RGBA values are independent: a=128 red pixel is (255, 0, 0, 128)
```

### Premultiplied Alpha (Recommended for Runtime)
```
Color channels are pre-multiplied by alpha: a=128 red pixel is (128, 0, 0, 128)
```

The asset compiler converts straight PNG to premultiplied on request:
```asm
.asset player, "player.png", format=rgba8888, alpha=premultiplied
```

## Format Selection Guide

| Use Case              | Recommended Format | VRAM/Byte | Quality |
|-----------------------|--------------------|-----------|---------|
| Opaque backgrounds    | rgb565             | 2         | Good    |
| UI elements, fonts    | rgba8888           | 4         | Best    |
| Sprites with alpha    | rgba8888           | 4         | Best    |
| Memory-constrained    | rgba4444           | 2         | Medium  |
| Retro palette look    | indexed8           | 1         | Palette |
| Paletted sprites      | indexed8alpha      | 1         | Palette |

## Build Pipeline

```
1. Parse .asset directives from .fasm source
2. Load source files (PNG/WAV)
3. Convert pixel format and alpha mode
4. Quantize if indexed (optional dithering)
5. Generate mipmaps (future)
6. Compress if requested (zlib or RLE)
7. Write asset binary blobs
8. Build asset directory table
9. Append to ROM after code section
```

## Manifest Format (JSON)

For batch processing without assembly:

```json
{
    "assets": [
        {
            "name": "player",
            "source": "assets/player.png",
            "type": "sprite",
            "format": "rgba8888",
            "alpha": "premultiplied"
        },
        {
            "name": "bg",
            "source": "assets/bg.png",
            "type": "image",
            "format": "rgb565",
            "alpha": "none"
        }
    ]
}
```

Build:
```bash
python3 asset_compiler/src/assetc.py manifest.json -o assets.fpak
```

## Runtime Access

Assets are accessed by ID (label) via traps:

```asm
load r0, player       ; asset ID
load r1, #100         ; x
load r2, #200         ; y
load r3, #BLEND_ALPHA
trap GFX_DRAW_SPRITE
```

The macroassembler resolves `player` to the asset ID assigned during packing.
