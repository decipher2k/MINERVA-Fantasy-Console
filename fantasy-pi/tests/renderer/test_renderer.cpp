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

void test_init_clear() {
    Renderer r;
    assert(r.Init(640, 480));
    r.Clear(0xFFFF0000); // Red
    Surface* fb = r.GetFramebuffer();
    assert(fb->width == 640);
    assert(fb->height == 480);
    uint32_t* pixels = reinterpret_cast<uint32_t*>(fb->pixels);
    assert(pixels[0] == 0xFFFF0000);
    printf("test_init_clear PASSED\n");
}

void test_blit_copy() {
    Renderer r;
    r.Init(640, 480);
    Surface* src = r.AllocateSurface(32, 32, PixelFormat::RGBA8888);
    assert(src);
    memset(src->pixels, 0xFF, src->pitch * 32); // Fill with 0xFF

    BlitRect rect;
    rect.src_x = 0; rect.src_y = 0;
    rect.dst_x = 100; rect.dst_y = 100;
    rect.width = 32; rect.height = 32;
    r.BitBlt(*src, *r.GetFramebuffer(), rect, BlendMode::Copy);

    uint32_t* fb = reinterpret_cast<uint32_t*>(r.GetFramebuffer()->pixels);
    assert(fb[100 * 640 + 100] == 0xFFFFFFFF);
    printf("test_blit_copy PASSED\n");
}

void test_blit_alpha() {
    Renderer r;
    r.Init(640, 480);
    r.Clear(0xFF0000FF); // Blue background

    Surface* src = r.AllocateSurface(32, 32, PixelFormat::RGBA8888);
    assert(src);
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            write_rgba(src, x, y, 0x80FF0000); // Half-transparent red
        }
    }

    BlitRect rect;
    rect.src_x = 0; rect.src_y = 0;
    rect.dst_x = 50; rect.dst_y = 50;
    rect.width = 32; rect.height = 32;
    r.BitBlt(*src, *r.GetFramebuffer(), rect, BlendMode::Alpha, 255);

    uint32_t* fb = reinterpret_cast<uint32_t*>(r.GetFramebuffer()->pixels);
    uint32_t result = fb[50 * 640 + 50];
    // Should be blended between red and blue
    assert(result != 0xFF0000FF);
    assert(result != 0x80FF0000);
    printf("test_blit_alpha PASSED (result=0x%08X)\n", result);
}

void test_clip_rect() {
    Renderer r;
    r.Init(640, 480);
    Surface* src = r.AllocateSurface(64, 64, PixelFormat::RGBA8888);
    memset(src->pixels, 0xFF, src->pitch * 64);

    BlitRect rect;
    rect.src_x = 0; rect.src_y = 0;
    rect.dst_x = 620; rect.dst_y = 460; // Partially off-screen
    rect.width = 64; rect.height = 64;
    r.BitBlt(*src, *r.GetFramebuffer(), rect, BlendMode::Copy);

    uint32_t* fb = reinterpret_cast<uint32_t*>(r.GetFramebuffer()->pixels);
    assert(fb[460 * 640 + 620] == 0xFFFFFFFF);
    assert(fb[479 * 640 + 639] == 0xFFFFFFFF);
    printf("test_clip_rect PASSED\n");
}

int main() {
    printf("Running Renderer tests...\n");
    test_init_clear();
    test_blit_copy();
    test_blit_alpha();
    test_clip_rect();
    printf("All Renderer tests passed!\n");
    return 0;
}
