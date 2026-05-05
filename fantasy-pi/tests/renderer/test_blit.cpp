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

#include <cstdio>
#include <cstring>
#include <cassert>
#include "renderer.h"

using namespace fantasy;

static void write_rgba(Surface* surface, int x, int y, uint32_t argb) {
    uint8_t* p = surface->pixels + y * surface->pitch + x * 4;
    p[0] = (argb >> 16) & 0xFF;
    p[1] = (argb >> 8) & 0xFF;
    p[2] = argb & 0xFF;
    p[3] = (argb >> 24) & 0xFF;
}

void test_colorkey() {
    Renderer r;
    r.Init(640, 480);
    r.Clear(0xFF000000);

    Surface* src = r.AllocateSurface(16, 16, PixelFormat::RGBA8888);
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            if (x < 8) write_rgba(src, x, y, 0xFFFF00FF); // Magenta (key color)
            else write_rgba(src, x, y, 0xFFFFFFFF); // White
        }
    }

    BlitRect rect;
    rect.src_x = 0; rect.src_y = 0;
    rect.dst_x = 10; rect.dst_y = 10;
    rect.width = 16; rect.height = 16;
    r.BitBlt(*src, *r.GetFramebuffer(), rect, BlendMode::ColorKey, 255, 0xFF00FF);

    uint32_t* fb = reinterpret_cast<uint32_t*>(r.GetFramebuffer()->pixels);
    assert(fb[10 * 640 + 10] == 0xFF000000); // Keyed out
    assert(fb[10 * 640 + 18] == 0xFFFFFFFF); // White copied
    printf("test_colorkey PASSED\n");
}

void test_additive_blend() {
    Renderer r;
    r.Init(640, 480);
    r.Clear(0xFF404040); // Dark grey

    Surface* src = r.AllocateSurface(8, 8, PixelFormat::RGBA8888);
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            write_rgba(src, x, y, 0xFF808080); // Mid grey
        }
    }

    BlitRect rect;
    rect.src_x = 0; rect.src_y = 0;
    rect.dst_x = 20; rect.dst_y = 20;
    rect.width = 8; rect.height = 8;
    r.BitBlt(*src, *r.GetFramebuffer(), rect, BlendMode::Additive);

    uint32_t* fb = reinterpret_cast<uint32_t*>(r.GetFramebuffer()->pixels);
    uint32_t result = fb[20 * 640 + 20];
    uint32_t r_val = (result >> 16) & 0xFF;
    assert(r_val > 0x40); // Should be brighter
    printf("test_additive_blend PASSED (r=%02X)\n", r_val);
}

void test_flip_x() {
    Renderer r;
    r.Init(640, 480);
    r.Clear(0xFF000000);

    Surface* src = r.AllocateSurface(4, 4, PixelFormat::RGBA8888);
    write_rgba(src, 0, 0, 0xFFFF0000); // Red at (0,0)
    write_rgba(src, 3, 0, 0xFF00FF00); // Green at (3,0)

    BlitRect rect;
    rect.src_x = 0; rect.src_y = 0;
    rect.dst_x = 0; rect.dst_y = 0;
    rect.width = 4; rect.height = 4;
    r.BitBlt(*src, *r.GetFramebuffer(), rect, BlendMode::Copy, 255, 0, 1); // FlipX

    uint32_t* fb = reinterpret_cast<uint32_t*>(r.GetFramebuffer()->pixels);
    assert(fb[0] == 0xFF00FF00); // Green now at left
    assert(fb[3] == 0xFFFF0000); // Red now at right
    printf("test_flip_x PASSED\n");
}

void test_global_alpha() {
    Renderer r;
    r.Init(640, 480);
    r.Clear(0xFFFFFFFF); // White

    Surface* src = r.AllocateSurface(4, 4, PixelFormat::RGBA8888);
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            write_rgba(src, x, y, 0xFFFF0000); // Opaque red
        }
    }

    BlitRect rect;
    rect.src_x = 0; rect.src_y = 0;
    rect.dst_x = 0; rect.dst_y = 0;
    rect.width = 4; rect.height = 4;
    r.BitBlt(*src, *r.GetFramebuffer(), rect, BlendMode::Alpha, 128); // 50% global alpha

    uint32_t* fb = reinterpret_cast<uint32_t*>(r.GetFramebuffer()->pixels);
    uint32_t result = fb[0];
    uint32_t r_val = (result >> 16) & 0xFF;
    uint32_t g_val = (result >> 8) & 0xFF;
    assert(r_val == 0xFF);
    assert(g_val > 0x70 && g_val < 0x90); // Green/blue fade from white toward red
    printf("test_global_alpha PASSED (r=%02X g=%02X)\n", r_val, g_val);
}

int main() {
    printf("Running Blit tests...\n");
    test_colorkey();
    test_additive_blend();
    test_flip_x();
    test_global_alpha();
    printf("All Blit tests passed!\n");
    return 0;
}
