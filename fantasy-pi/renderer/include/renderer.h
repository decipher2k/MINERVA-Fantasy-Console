/*
Copyright 2026 Dennis Michael Heine

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#pragma once

#include <cstdint>
#include <cstddef>

namespace fantasy {

enum class PixelFormat : uint8_t {
    RGB565 = 0,
    RGBA8888 = 1,
    ARGB8888 = 2,
    RGBA4444 = 3,
    Indexed8 = 4,
    Indexed8Alpha = 5,
};

enum class BlendMode : uint8_t {
    Copy = 0,
    Alpha = 1,
    Additive = 2,
    Multiply = 3,
    ColorKey = 4,
    Mask = 5,
};

enum class FlipFlags : uint8_t {
    None = 0,
    FlipX = 1 << 0,
    FlipY = 1 << 1,
    Rotate90 = 1 << 2,
    Rotate180 = 1 << 3,
    Rotate270 = 1 << 4,
};

struct Surface {
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    PixelFormat format;
    uint8_t flags;
    uint32_t palette_id;
    uint8_t* pixels;
};

struct BlitRect {
    int16_t src_x;
    int16_t src_y;
    int16_t dst_x;
    int16_t dst_y;
    uint16_t width;
    uint16_t height;
};

struct SpriteDef {
    uint16_t width;
    uint16_t height;
    uint16_t hotspot_x;
    uint16_t hotspot_y;
    PixelFormat format;
    uint8_t flags;
    uint32_t surface_id;
};

struct TileSet {
    uint16_t tile_width;
    uint16_t tile_height;
    uint16_t tile_count;
    PixelFormat format;
    uint32_t surface_id;
};

struct TileMapDef {
    uint16_t width;
    uint16_t height;
    uint16_t tile_width;
    uint16_t tile_height;
    uint32_t tileset_id;
    uint8_t* tiles; // 3 bytes per tile: uint16 index + uint8 flags
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Initialization
    bool Init(uint32_t width, uint32_t height);
    void Shutdown();

    // Framebuffer
    void Clear(uint32_t color);
    void Present(); // Flip to display
    Surface* GetFramebuffer() { return &framebuffer_; }
    uint8_t* GetFrontBufferPixels() const { return front_buffer_pixels_; }

    // BitBlt operations
    void BitBlt(const Surface& src, Surface& dst, const BlitRect& rect,
                BlendMode mode, uint8_t global_alpha = 255,
                uint32_t color_key = 0, uint8_t flip_flags = 0);

    // High-level drawing
    void DrawSprite(uint32_t sprite_id, int x, int y,
                    BlendMode mode = BlendMode::Alpha, uint8_t global_alpha = 255);
    void DrawImage(uint32_t image_id, int x, int y,
                   BlendMode mode = BlendMode::Copy, uint8_t global_alpha = 255);
    void DrawTileMap(uint32_t tilemap_id, int scroll_x, int scroll_y);
    void DrawText(uint32_t font_id, const char* text, int x, int y,
                  uint32_t color = 0xFFFFFFFF);

    // Surface management
    Surface* AllocateSurface(uint16_t width, uint16_t height, PixelFormat format);
    Surface* AllocateSurfaceAt(uint32_t id, uint16_t width, uint16_t height, PixelFormat format);
    void FreeSurface(Surface* surface);
    Surface* GetSurface(uint32_t id);

    // Asset binding (maps asset IDs to surfaces/sprites)
    void RegisterSprite(uint32_t id, const SpriteDef& def);
    void RegisterTileSet(uint32_t id, const TileSet& set);
    void RegisterTileMap(uint32_t id, const TileMapDef& map);
    void RegisterPalette(uint32_t id, const uint32_t* colors, uint32_t count);

    // Debug
    void SetDebugOverlay(bool enable) { debug_overlay_ = enable; }

private:
    Surface framebuffer_;
    uint8_t* back_buffer_pixels_;
    uint8_t* front_buffer_pixels_;
    uint32_t fb_width_;
    uint32_t fb_height_;
    bool debug_overlay_;

    // VRAM pools
    uint8_t* vram_base_;
    size_t vram_used_;
    static constexpr size_t MAX_SURFACES = 256;
    Surface surfaces_[MAX_SURFACES];
    bool surface_used_[MAX_SURFACES];

    SpriteDef sprites_[MAX_SURFACES];
    bool sprite_used_[MAX_SURFACES];

    TileSet tilesets_[MAX_SURFACES];
    bool tileset_used_[MAX_SURFACES];

    TileMapDef tilemaps_[MAX_SURFACES];
    bool tilemap_used_[MAX_SURFACES];

    uint32_t palettes_[16][256];

    // Internal blitting helpers
    void BlitCopy(const Surface& src, Surface& dst, const BlitRect& rect, uint8_t flip);
    void BlitAlpha(const Surface& src, Surface& dst, const BlitRect& rect,
                   uint8_t global_alpha, uint8_t flip);
    void BlitColorKey(const Surface& src, Surface& dst, const BlitRect& rect,
                      uint32_t color_key, uint8_t flip);
    void BlitAdditive(const Surface& src, Surface& dst, const BlitRect& rect,
                      uint8_t global_alpha, uint8_t flip);

    // Pixel format converters
    static uint32_t ReadPixel(const uint8_t* ptr, PixelFormat fmt, const uint32_t* palette);
    static void WritePixel(uint8_t* ptr, PixelFormat fmt, uint32_t color);
    static uint32_t UnpackRGBA8888(uint32_t c);
    static uint32_t PackRGBA8888(uint32_t r, uint32_t g, uint32_t b, uint32_t a);

    // Clipping
    bool ClipRect(BlitRect& rect, int dst_w, int dst_h);
};

} // namespace fantasy
