# Fantasy Pi Graphics System

## Overview

The graphics system is a retained-mode software renderer with command-buffer style operations exposed through the VM trap interface. The kernel maintains a back buffer which is presented via `GFX_PRESENT` (trap 0x11).

## Pixel Formats

| Format        | Bits  | Channels     | Alpha | Use Case                    |
|---------------|-------|--------------|-------|-----------------------------|
| RGBA8888      | 32    | RGBA 8-bit   | Full  | High-quality sprites        |
| ARGB8888      | 32    | ARGB 8-bit   | Full  | Internal framebuffer        |
| RGB565        | 16    | RGB 5/6/5    | None  | Backgrounds, opaque images  |
| RGBA4444      | 16    | RGBA 4-bit   | 16-level | Memory-efficient sprites |
| Indexed8      | 8     | Index 8-bit  | None  | Paletted backgrounds        |
| Indexed8Alpha | 8     | Index 8-bit  | 1-bit | Paletted sprites            |

## Internal Format Decision

The internal framebuffer and all blending operations use **ARGB8888** (premultiplied alpha) for surfaces that participate in blending. Opaque backgrounds may use RGB565 for memory efficiency.

Premultiplied alpha is chosen because:
- Simpler blend operations: `out = src + dst * (1 - src.a)`
- No darkening artifacts when scaling
- Consistent filtering behavior

All RGBA8888 source assets are converted to premultiplied alpha at build time when requested.

## Blend Modes

| Mode        | Formula (Premultiplied)                                      |
|-------------|--------------------------------------------------------------|
| Copy        | `out = src`                                                  |
| Alpha       | `out.rgb = src.rgb + dst.rgb * (1 - src.a)`                  |
|             | `out.a   = src.a + dst.a * (1 - src.a)`                      |
| Additive    | `out.rgb = min(1.0, src.rgb + dst.rgb)`                      |
|             | `out.a   = min(1.0, src.a + dst.a)`                          |
| Multiply    | `out.rgb = src.rgb * dst.rgb`                                |
|             | `out.a   = src.a * dst.a`                                    |
| ColorKey    | If src == key, skip pixel; else copy                         |
| Mask        | If src.a > 0, write dst color; else skip                     |

## Integer Alpha Blending (8-bit)

For performance on ARM64 without FPU in the VM loop, blending uses 8-bit integer arithmetic with reciprocal multiplication:

```cpp
uint32_t inv_a = 255 - src_a;
uint32_t r = (src_r * 255 + dst_r * inv_a) / 255;  // optimized with fastdiv
uint32_t g = (src_g * 255 + dst_g * inv_a) / 255;
uint32_t b = (src_b * 255 + dst_b * inv_a) / 255;
uint32_t a = (src_a * 255 + dst_a * inv_a) / 255;
```

Division by 255 is optimized via multiply-shift: `(x * 0x8081) >> 23` for 16-bit values.

## Surface Structure

```cpp
struct Surface {
    uint16_t width;
    uint16_t height;
    uint16_t pitch;         // Bytes per row
    uint8_t  format;        // PixelFormat enum
    uint8_t  flags;
    uint32_t palette_id;    // For indexed formats
    uint8_t* pixels;
};
```

## Sprite Structure

```cpp
struct Sprite {
    uint16_t width;
    uint16_t height;
    uint16_t hotspot_x;
    uint16_t hotspot_y;
    uint8_t  format;
    uint8_t  flags;
    uint16_t surface_id;
};
```

## Tilemap Structure

```cpp
struct TileMap {
    uint16_t width;         // In tiles
    uint16_t height;        // In tiles
    uint16_t tile_width;
    uint16_t tile_height;
    uint32_t tileset_id;
    uint8_t* tiles;         // Tile indices + optional flip bits
};
```

## BlitRect Structure

```cpp
struct BlitRect {
    int16_t src_x;
    int16_t src_y;
    int16_t dst_x;
    int16_t dst_y;
    uint16_t width;
    uint16_t height;
};
```

## BitBlt API

BitBlt is exposed as `GFX_BITBLT` trap with arguments in registers:

```
r0 = source surface/asset id
r1 = dst_x
r2 = dst_y
r3 = src_x
r4 = src_y
r5 = width
r6 = height
r7 = blend_mode
r8 = global_alpha (0-255)
```

### Clipping Behavior

- Source coordinates outside surface bounds are clipped to valid range
- Destination coordinates outside framebuffer are clipped
- Negative dst_x/dst_y are supported (clipped to 0)
- Partial off-screen blits are fully supported

### Flip and Rotate

Flags in blend_mode upper bits:
- Bit 8: Flip X
- Bit 9: Flip Y
- Bits 10-11: Rotate (0=0°, 1=90°, 2=180°, 3=270°)

## Layer System

The renderer supports 4 compositing layers (ordered back to front):
1. Background layer (tilemap or solid color)
2. Background sprite layer
3. Foreground sprite layer
4. UI/text layer

Layers are not independent framebuffers; they are rendered sequentially into the back buffer during the frame. The layer priority determines draw order.

## Drawing Primitives

| Function        | Trap              | Description                    |
|-----------------|-------------------|--------------------------------|
| Clear           | GFX_CLEAR         | Fill screen with color         |
| RectFill        | (via BitBlt)      | Fill rectangle with color      |
| Line            | (not in v1)       | Bresenham line                 |
| DrawSprite      | GFX_DRAW_SPRITE   | Draw sprite with hotspot       |
| DrawImage       | GFX_DRAW_IMAGE    | Draw full image at position    |
| DrawTileMap     | GFX_DRAW_TILEMAP  | Render tilemap with scrolling  |
| DrawText        | GFX_DRAW_TEXT     | Render text using bitmap font  |
| BitBlt          | GFX_BITBLT        | General blit with all options  |

## Performance Budget

At 640x480 60Hz on Raspberry Pi 4 (1.5 GHz):
- Full-screen alpha blit: ~2-3 ms
- 100 sprites (32x32 RGBA): ~5 ms
- Tilemap render (40x30 tiles, 16x16): ~2 ms
- Total budget: 16.67 ms per frame

## Alignment Requirements

- Surface pixel data must be 8-byte aligned for NEON optimizations
- Pitch must be multiple of 8 bytes
- All VRAM allocations aligned to 64-byte cache line boundary
