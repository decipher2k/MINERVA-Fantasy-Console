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

#include "renderer.h"
#include <cstring>
#include <algorithm>

namespace fantasy {

// Fast divide by 255: (v * 0x8081) >> 23 for 16-bit inputs
static inline uint32_t div255(uint32_t v) {
    return (v * 0x8081) >> 23;
}

static inline uint32_t BytesPerPixel(PixelFormat fmt) {
    switch (fmt) {
        case PixelFormat::RGB565: return 2;
        case PixelFormat::RGBA8888: return 4;
        case PixelFormat::ARGB8888: return 4;
        case PixelFormat::RGBA4444: return 2;
        case PixelFormat::Indexed8: return 1;
        case PixelFormat::Indexed8Alpha: return 1;
        default: return 4;
    }
}

static inline uint32_t blend_premul(uint32_t src, uint32_t dst) {
    uint32_t sa = (src >> 24) & 0xFF;
    uint32_t sr = (src >> 16) & 0xFF;
    uint32_t sg = (src >> 8) & 0xFF;
    uint32_t sb = src & 0xFF;

    uint32_t da = (dst >> 24) & 0xFF;
    uint32_t dr = (dst >> 16) & 0xFF;
    uint32_t dg = (dst >> 8) & 0xFF;
    uint32_t db = dst & 0xFF;

    uint32_t inv = 255 - sa;
    uint32_t r = div255(sr * sa + dr * inv);
    uint32_t g = div255(sg * sa + dg * inv);
    uint32_t b = div255(sb * sa + db * inv);
    uint32_t a = sa + div255(da * inv);

    return (a << 24) | (r << 16) | (g << 8) | b;
}

static inline uint32_t blend_additive(uint32_t src, uint32_t dst) {
    uint32_t sr = (src >> 16) & 0xFF;
    uint32_t sg = (src >> 8) & 0xFF;
    uint32_t sb = src & 0xFF;
    uint32_t dr = (dst >> 16) & 0xFF;
    uint32_t dg = (dst >> 8) & 0xFF;
    uint32_t db = dst & 0xFF;
    uint32_t r = std::min(255u, sr + dr);
    uint32_t g = std::min(255u, sg + dg);
    uint32_t b = std::min(255u, sb + db);
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

Renderer::Renderer()
    : back_buffer_pixels_(nullptr),
      front_buffer_pixels_(nullptr),
      fb_width_(640),
      fb_height_(480),
      debug_overlay_(false),
      vram_base_(nullptr),
      vram_used_(0)
{
    memset(surfaces_, 0, sizeof(surfaces_));
    memset(surface_used_, 0, sizeof(surface_used_));
    memset(sprite_used_, 0, sizeof(sprite_used_));
    memset(tileset_used_, 0, sizeof(tileset_used_));
    memset(tilemap_used_, 0, sizeof(tilemap_used_));
    memset(palettes_, 0, sizeof(palettes_));
}

Renderer::~Renderer() {
    Shutdown();
}

bool Renderer::Init(uint32_t width, uint32_t height) {
    if (back_buffer_pixels_ || front_buffer_pixels_ || vram_base_) {
        Shutdown();
    }

    fb_width_ = width;
    fb_height_ = height;
    size_t fb_size = width * height * 4;
    back_buffer_pixels_ = new uint8_t[fb_size];
    front_buffer_pixels_ = new uint8_t[fb_size];
    memset(back_buffer_pixels_, 0, fb_size);
    memset(front_buffer_pixels_, 0, fb_size);

    framebuffer_.width = width;
    framebuffer_.height = height;
    framebuffer_.pitch = width * 4;
    framebuffer_.format = PixelFormat::ARGB8888;
    framebuffer_.flags = 0;
    framebuffer_.palette_id = 0;
    framebuffer_.pixels = back_buffer_pixels_;

    vram_base_ = new uint8_t[8 * 1024 * 1024];
    vram_used_ = 0;
    return true;
}

void Renderer::Shutdown() {
    delete[] back_buffer_pixels_;
    delete[] front_buffer_pixels_;
    delete[] vram_base_;
    back_buffer_pixels_ = nullptr;
    front_buffer_pixels_ = nullptr;
    vram_base_ = nullptr;
    vram_used_ = 0;
    framebuffer_.pixels = nullptr;
    memset(surfaces_, 0, sizeof(surfaces_));
    memset(surface_used_, 0, sizeof(surface_used_));
    memset(sprite_used_, 0, sizeof(sprite_used_));
    memset(tileset_used_, 0, sizeof(tileset_used_));
    memset(tilemap_used_, 0, sizeof(tilemap_used_));
}

void Renderer::Clear(uint32_t color) {
    size_t count = fb_width_ * fb_height_;
    uint32_t* ptr = reinterpret_cast<uint32_t*>(back_buffer_pixels_);
    for (size_t i = 0; i < count; i++) {
        ptr[i] = color;
    }
}

void Renderer::Present() {
    // Swap front/back buffer pointers
    std::swap(back_buffer_pixels_, front_buffer_pixels_);
    framebuffer_.pixels = back_buffer_pixels_;
    // Swap front/back buffers. The bare-metal kernel copies front_buffer_pixels_ to the display.
}

bool Renderer::ClipRect(BlitRect& rect, int dst_w, int dst_h) {
    if (rect.dst_x < 0) {
        rect.src_x -= rect.dst_x;
        rect.width += rect.dst_x;
        rect.dst_x = 0;
    }
    if (rect.dst_y < 0) {
        rect.src_y -= rect.dst_y;
        rect.height += rect.dst_y;
        rect.dst_y = 0;
    }
    if (rect.dst_x + rect.width > dst_w) {
        rect.width = dst_w - rect.dst_x;
    }
    if (rect.dst_y + rect.height > dst_h) {
        rect.height = dst_h - rect.dst_y;
    }
    if (rect.width <= 0 || rect.height <= 0) return false;
    return true;
}

uint32_t Renderer::ReadPixel(const uint8_t* ptr, PixelFormat fmt, const uint32_t* palette) {
    switch (fmt) {
        case PixelFormat::RGB565: {
            uint16_t v = ptr[0] | (ptr[1] << 8);
            uint32_t r = ((v >> 11) & 0x1F) << 3;
            uint32_t g = ((v >> 5) & 0x3F) << 2;
            uint32_t b = (v & 0x1F) << 3;
            return 0xFF000000 | (r << 16) | (g << 8) | b;
        }
        case PixelFormat::RGBA8888:
            return (ptr[3] << 24) | (ptr[0] << 16) | (ptr[1] << 8) | ptr[2];
        case PixelFormat::ARGB8888:
            return ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
        case PixelFormat::RGBA4444: {
            uint16_t v = ptr[0] | (ptr[1] << 8);
            uint32_t r = ((v >> 12) & 0xF) << 4;
            uint32_t g = ((v >> 8) & 0xF) << 4;
            uint32_t b = ((v >> 4) & 0xF) << 4;
            uint32_t a = (v & 0xF) << 4;
            return (a << 24) | (r << 16) | (g << 8) | b;
        }
        case PixelFormat::Indexed8:
            if (palette) return palette[ptr[0]];
            return 0xFF000000 | (ptr[0] * 0x010101);
        case PixelFormat::Indexed8Alpha:
            if (palette) {
                uint32_t c = palette[ptr[0] & 0x7F];
                if (ptr[0] & 0x80) c |= 0xFF000000;
                else c &= 0x00FFFFFF;
                return c;
            }
            return 0xFF000000;
        default:
            return 0;
    }
}

void Renderer::WritePixel(uint8_t* ptr, PixelFormat fmt, uint32_t color) {
    switch (fmt) {
        case PixelFormat::RGB565: {
            uint32_t r = ((color >> 16) & 0xFF) >> 3;
            uint32_t g = ((color >> 8) & 0xFF) >> 2;
            uint32_t b = (color & 0xFF) >> 3;
            uint16_t v = (r << 11) | (g << 5) | b;
            ptr[0] = v & 0xFF;
            ptr[1] = (v >> 8) & 0xFF;
            break;
        }
        case PixelFormat::RGBA8888:
            ptr[0] = (color >> 16) & 0xFF;
            ptr[1] = (color >> 8) & 0xFF;
            ptr[2] = color & 0xFF;
            ptr[3] = (color >> 24) & 0xFF;
            break;
        case PixelFormat::ARGB8888:
            ptr[0] = color & 0xFF;
            ptr[1] = (color >> 8) & 0xFF;
            ptr[2] = (color >> 16) & 0xFF;
            ptr[3] = (color >> 24) & 0xFF;
            break;
        case PixelFormat::RGBA4444: {
            uint32_t r = ((color >> 16) & 0xFF) >> 4;
            uint32_t g = ((color >> 8) & 0xFF) >> 4;
            uint32_t b = (color & 0xFF) >> 4;
            uint32_t a = ((color >> 24) & 0xFF) >> 4;
            uint16_t v = (r << 12) | (g << 8) | (b << 4) | a;
            ptr[0] = v & 0xFF;
            ptr[1] = (v >> 8) & 0xFF;
            break;
        }
        case PixelFormat::Indexed8:
        case PixelFormat::Indexed8Alpha:
            ptr[0] = color & 0xFF;
            break;
    }
}

void Renderer::BitBlt(const Surface& src, Surface& dst, const BlitRect& rect,
                      BlendMode mode, uint8_t global_alpha,
                      uint32_t color_key, uint8_t flip_flags) {
    BlitRect r = rect;
    if (!ClipRect(r, dst.width, dst.height)) return;
    if (r.src_x >= src.width || r.src_y >= src.height) return;
    if (r.src_x + r.width > src.width) r.width = src.width - r.src_x;
    if (r.src_y + r.height > src.height) r.height = src.height - r.src_y;

    switch (mode) {
        case BlendMode::Copy:
            BlitCopy(src, dst, r, flip_flags);
            break;
        case BlendMode::Alpha:
            BlitAlpha(src, dst, r, global_alpha, flip_flags);
            break;
        case BlendMode::ColorKey:
            BlitColorKey(src, dst, r, color_key, flip_flags);
            break;
        case BlendMode::Additive:
            BlitAdditive(src, dst, r, global_alpha, flip_flags);
            break;
        default:
            BlitCopy(src, dst, r, flip_flags);
            break;
    }
}

void Renderer::BlitCopy(const Surface& src, Surface& dst, const BlitRect& rect, uint8_t flip) {
    bool flip_x = flip & 1;
    bool flip_y = flip & 2;
    uint32_t src_bpp = BytesPerPixel(src.format);
    uint32_t dst_bpp = BytesPerPixel(dst.format);
    for (uint16_t y = 0; y < rect.height; y++) {
        uint16_t sy = flip_y ? (rect.src_y + rect.height - 1 - y) : (rect.src_y + y);
        for (uint16_t x = 0; x < rect.width; x++) {
            uint16_t sx = flip_x ? (rect.src_x + rect.width - 1 - x) : (rect.src_x + x);
            const uint8_t* sp = src.pixels + sy * src.pitch + sx * src_bpp;
            uint8_t* dp = dst.pixels + (rect.dst_y + y) * dst.pitch + (rect.dst_x + x) * dst_bpp;
            uint32_t color = ReadPixel(sp, src.format, nullptr);
            WritePixel(dp, dst.format, color);
        }
    }
}

void Renderer::BlitAlpha(const Surface& src, Surface& dst, const BlitRect& rect,
                         uint8_t global_alpha, uint8_t flip) {
    bool flip_x = flip & 1;
    bool flip_y = flip & 2;
    uint32_t ga = global_alpha;
    uint32_t src_bpp = BytesPerPixel(src.format);
    uint32_t dst_bpp = BytesPerPixel(dst.format);
    for (uint16_t y = 0; y < rect.height; y++) {
        uint16_t sy = flip_y ? (rect.src_y + rect.height - 1 - y) : (rect.src_y + y);
        for (uint16_t x = 0; x < rect.width; x++) {
            uint16_t sx = flip_x ? (rect.src_x + rect.width - 1 - x) : (rect.src_x + x);
            const uint8_t* sp = src.pixels + sy * src.pitch + sx * src_bpp;
            uint8_t* dp = dst.pixels + (rect.dst_y + y) * dst.pitch + (rect.dst_x + x) * dst_bpp;
            uint32_t sc = ReadPixel(sp, src.format, nullptr);
            uint32_t sa = (sc >> 24) & 0xFF;
            sa = (sa * ga) / 255;
            sc = (sc & 0x00FFFFFF) | (sa << 24);
            uint32_t dc = ReadPixel(dp, dst.format, nullptr);
            uint32_t result = blend_premul(sc, dc);
            WritePixel(dp, dst.format, result);
        }
    }
}

void Renderer::BlitColorKey(const Surface& src, Surface& dst, const BlitRect& rect,
                            uint32_t color_key, uint8_t flip) {
    bool flip_x = flip & 1;
    bool flip_y = flip & 2;
    uint32_t ck = color_key & 0x00FFFFFF;
    uint32_t src_bpp = BytesPerPixel(src.format);
    uint32_t dst_bpp = BytesPerPixel(dst.format);
    for (uint16_t y = 0; y < rect.height; y++) {
        uint16_t sy = flip_y ? (rect.src_y + rect.height - 1 - y) : (rect.src_y + y);
        for (uint16_t x = 0; x < rect.width; x++) {
            uint16_t sx = flip_x ? (rect.src_x + rect.width - 1 - x) : (rect.src_x + x);
            const uint8_t* sp = src.pixels + sy * src.pitch + sx * src_bpp;
            uint32_t sc = ReadPixel(sp, src.format, nullptr);
            if ((sc & 0x00FFFFFF) != ck) {
                uint8_t* dp = dst.pixels + (rect.dst_y + y) * dst.pitch + (rect.dst_x + x) * dst_bpp;
                WritePixel(dp, dst.format, sc);
            }
        }
    }
}

void Renderer::BlitAdditive(const Surface& src, Surface& dst, const BlitRect& rect,
                            uint8_t global_alpha, uint8_t flip) {
    bool flip_x = flip & 1;
    bool flip_y = flip & 2;
    uint32_t ga = global_alpha;
    uint32_t src_bpp = BytesPerPixel(src.format);
    uint32_t dst_bpp = BytesPerPixel(dst.format);
    for (uint16_t y = 0; y < rect.height; y++) {
        uint16_t sy = flip_y ? (rect.src_y + rect.height - 1 - y) : (rect.src_y + y);
        for (uint16_t x = 0; x < rect.width; x++) {
            uint16_t sx = flip_x ? (rect.src_x + rect.width - 1 - x) : (rect.src_x + x);
            const uint8_t* sp = src.pixels + sy * src.pitch + sx * src_bpp;
            uint8_t* dp = dst.pixels + (rect.dst_y + y) * dst.pitch + (rect.dst_x + x) * dst_bpp;
            uint32_t sc = ReadPixel(sp, src.format, nullptr);
            uint32_t sa = (sc >> 24) & 0xFF;
            sa = (sa * ga) / 255;
            sc = (sc & 0x00FFFFFF) | (sa << 24);
            uint32_t dc = ReadPixel(dp, dst.format, nullptr);
            uint32_t result = blend_additive(sc, dc);
            WritePixel(dp, dst.format, result);
        }
    }
}

Surface* Renderer::AllocateSurface(uint16_t width, uint16_t height, PixelFormat format) {
    for (size_t i = 0; i < MAX_SURFACES; i++) {
        if (!surface_used_[i]) {
            return AllocateSurfaceAt(static_cast<uint32_t>(i), width, height, format);
        }
    }
    return nullptr;
}

Surface* Renderer::AllocateSurfaceAt(uint32_t id, uint16_t width, uint16_t height, PixelFormat format) {
    if (id >= MAX_SURFACES || surface_used_[id]) return nullptr;

    size_t bpp = 4;
    if (format == PixelFormat::RGB565 || format == PixelFormat::RGBA4444) bpp = 2;
    else if (format == PixelFormat::Indexed8 || format == PixelFormat::Indexed8Alpha) bpp = 1;
    size_t pitch = ((width * bpp + 7) / 8) * 8; // align to 8 bytes
    size_t size = pitch * height;
    if (vram_used_ + size > 8 * 1024 * 1024) return nullptr;

    surfaces_[id].width = width;
    surfaces_[id].height = height;
    surfaces_[id].pitch = pitch;
    surfaces_[id].format = format;
    surfaces_[id].flags = 0;
    surfaces_[id].palette_id = 0;
    surfaces_[id].pixels = vram_base_ + vram_used_;
    memset(surfaces_[id].pixels, 0, size);
    vram_used_ += size;
    surface_used_[id] = true;
    return &surfaces_[id];
}

void Renderer::FreeSurface(Surface* surface) {
    if (!surface) return;
    for (size_t i = 0; i < MAX_SURFACES; i++) {
        if (&surfaces_[i] == surface) {
            surface_used_[i] = false;
            surface->pixels = nullptr;
            return;
        }
    }
}

Surface* Renderer::GetSurface(uint32_t id) {
    if (id < MAX_SURFACES && surface_used_[id]) return &surfaces_[id];
    return nullptr;
}

void Renderer::RegisterSprite(uint32_t id, const SpriteDef& def) {
    if (id < MAX_SURFACES) {
        sprites_[id] = def;
        sprite_used_[id] = true;
    }
}

void Renderer::RegisterTileSet(uint32_t id, const TileSet& set) {
    if (id < MAX_SURFACES) {
        tilesets_[id] = set;
        tileset_used_[id] = true;
    }
}

void Renderer::RegisterTileMap(uint32_t id, const TileMapDef& map) {
    if (id < MAX_SURFACES) {
        tilemaps_[id] = map;
        tilemap_used_[id] = true;
    }
}

void Renderer::RegisterPalette(uint32_t id, const uint32_t* colors, uint32_t count) {
    if (id < 16 && colors) {
        uint32_t n = count > 256 ? 256 : count;
        memcpy(palettes_[id], colors, n * sizeof(uint32_t));
    }
}

void Renderer::DrawSprite(uint32_t sprite_id, int x, int y,
                          BlendMode mode, uint8_t global_alpha) {
    if (sprite_id >= MAX_SURFACES || !sprite_used_[sprite_id]) return;
    const SpriteDef& spr = sprites_[sprite_id];
    Surface* src = GetSurface(spr.surface_id);
    if (!src) return;
    BlitRect rect;
    rect.src_x = 0;
    rect.src_y = 0;
    rect.dst_x = x - spr.hotspot_x;
    rect.dst_y = y - spr.hotspot_y;
    rect.width = spr.width;
    rect.height = spr.height;
    BitBlt(*src, framebuffer_, rect, mode, global_alpha);
}

void Renderer::DrawImage(uint32_t image_id, int x, int y,
                         BlendMode mode, uint8_t global_alpha) {
    Surface* src = GetSurface(image_id);
    if (!src) return;
    BlitRect rect;
    rect.src_x = 0;
    rect.src_y = 0;
    rect.dst_x = x;
    rect.dst_y = y;
    rect.width = src->width;
    rect.height = src->height;
    BitBlt(*src, framebuffer_, rect, mode, global_alpha);
}

void Renderer::DrawTileMap(uint32_t tilemap_id, int scroll_x, int scroll_y) {
    if (tilemap_id >= MAX_SURFACES || !tilemap_used_[tilemap_id]) return;
    const TileMapDef& tm = tilemaps_[tilemap_id];
    const TileSet* ts = nullptr;
    if (tm.tileset_id < MAX_SURFACES && tileset_used_[tm.tileset_id]) {
        ts = &tilesets_[tm.tileset_id];
    }
    if (!ts) return;
    Surface* tileset_surf = GetSurface(ts->surface_id);
    if (!tileset_surf) return;

    int tw = tm.tile_width;
    int th = tm.tile_height;
    int cols = fb_width_ / tw + 2;
    int rows = fb_height_ / th + 2;

    int start_tx = scroll_x / tw;
    int start_ty = scroll_y / th;
    int off_x = -(scroll_x % tw);
    int off_y = -(scroll_y % th);

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            int tx = start_tx + col;
            int ty = start_ty + row;
            if (tx < 0 || ty < 0 || tx >= tm.width || ty >= tm.height) continue;
            size_t tile_offset = (ty * tm.width + tx) * 3;
            uint16_t tile_idx = tm.tiles[tile_offset] | (tm.tiles[tile_offset + 1] << 8);
            uint8_t flags = tm.tiles[tile_offset + 2];
            int src_x = (tile_idx % (tileset_surf->width / tw)) * tw;
            int src_y = (tile_idx / (tileset_surf->width / tw)) * th;
            BlitRect rect;
            rect.src_x = src_x;
            rect.src_y = src_y;
            rect.dst_x = off_x + col * tw;
            rect.dst_y = off_y + row * th;
            rect.width = tw;
            rect.height = th;
            BitBlt(*tileset_surf, framebuffer_, rect, BlendMode::Copy, 255, 0, flags);
        }
    }
}

void Renderer::DrawText(uint32_t font_id, const char* text, int x, int y, uint32_t color) {
    if (!text) return;

    // Font is stored as a tileset: 16x16 grid of 8x8 glyphs (ASCII 0-255)
    // font_id refers to a TileSet registered via RegisterTileSet
    if (font_id >= MAX_SURFACES || !tileset_used_[font_id]) return;
    const TileSet& ts = tilesets_[font_id];
    Surface* tileset_surf = GetSurface(ts.surface_id);
    if (!tileset_surf) return;

    int tw = ts.tile_width;
    int th = ts.tile_height;
    int tiles_per_row = tileset_surf->width / tw;

    int cx = x;
    int cy = y;
    uint32_t fg = color;
    uint32_t bg = 0x00000000; // Transparent background

    for (const char* p = text; *p; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch == '\n') {
            cx = x;
            cy += th;
            continue;
        }

        int tile_idx = ch;
        int src_x = (tile_idx % tiles_per_row) * tw;
        int src_y = (tile_idx / tiles_per_row) * th;

        BlitRect rect;
        rect.src_x = src_x;
        rect.src_y = src_y;
        rect.dst_x = cx;
        rect.dst_y = cy;
        rect.width = tw;
        rect.height = th;

        // Font rendering: use source alpha to decide whether to draw with foreground color.
        for (int row = 0; row < th; row++) {
            for (int col = 0; col < tw; col++) {
                int sx = src_x + col;
                int sy = src_y + row;
                int dx = cx + col;
                int dy = cy + row;
                if (dx < 0 || dy < 0 || dx >= framebuffer_.width || dy >= framebuffer_.height) continue;
                if (sx < 0 || sy < 0 || sx >= tileset_surf->width || sy >= tileset_surf->height) continue;

                uint32_t font_bpp = BytesPerPixel(tileset_surf->format);
                uint32_t fb_bpp = BytesPerPixel(framebuffer_.format);
                const uint8_t* sp = tileset_surf->pixels + sy * tileset_surf->pitch + sx * font_bpp;
                uint32_t sc = ReadPixel(sp, tileset_surf->format, nullptr);
                uint32_t sa = (sc >> 24) & 0xFF;
                if (sa > 0) {
                    uint8_t* dp = framebuffer_.pixels + dy * framebuffer_.pitch + dx * fb_bpp;
                    uint32_t out = (sc & 0xFF000000) | (fg & 0x00FFFFFF);
                    WritePixel(dp, framebuffer_.format, out);
                }
            }
        }

        cx += tw;
    }
}

} // namespace fantasy
