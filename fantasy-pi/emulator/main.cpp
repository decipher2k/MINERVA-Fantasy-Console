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
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>

#ifdef HAS_SDL2
#include <SDL2/SDL.h>
#include "sdl_backend.h"
#endif

#include "fantasy_vm.h"
#include "renderer.h"

using namespace fantasy;

struct InputState {
    uint32_t keyboard_keys[8];
    uint32_t gamepad_buttons[2];
    int16_t  gamepad_axis[2][4];
    uint32_t mouse_x;
    uint32_t mouse_y;
    uint32_t mouse_buttons;
};

constexpr uint32_t INPUT_STATE_ADDR = 0x00010000;

class Emulator {
public:
    bool Init(int width, int height) {
        width_ = width; height_ = height;
#ifdef HAS_SDL2
        if (backend_.Init(width, height, "Fantasy Pi Emulator")) {
            has_display_ = true;
        } else {
            printf("SDL init failed, running headless\n");
            has_display_ = false;
        }
#else
        has_display_ = false;
        printf("Headless mode (no SDL2)\n");
#endif
        return renderer_.Init(width, height);
    }

    bool LoadCartridge(const char* path) {
        FILE* f = fopen(path, "rb");
        if (!f) return false;
        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        fseek(f, 0, SEEK_SET);
        uint8_t* data = new uint8_t[size];
        fread(data, 1, size, f);
        fclose(f);
        cartridge_data_.assign(data, data + size);
        vm_.LoadCartridge(data, size);
        LoadAssetsFromCartridge(data, size);
        delete[] data;
        return true;
    }

    void Run() {
        vm_.SetTrapCallback(EmulatorTrapHandler, this);
        InitInputState();
        bool running = true;
        while (running && !vm_.IsHalted()) {
#ifdef HAS_SDL2
            if (has_display_) {
                running = backend_.ProcessEvents();
                UpdateInputStateSDL();
            }
#endif
            vm_.RunFrame();
            if (vm_.IsPresentRequested()) {
                renderer_.Present();
#ifdef HAS_SDL2
                if (has_display_) {
                    Surface display = *renderer_.GetFramebuffer();
                    display.pixels = renderer_.GetFrontBufferPixels();
                    backend_.Update(&display);
                }
#endif
                vm_.ClearPresentRequest();
                frame_count_++;
                if (frame_count_ % 60 == 0) printf("Frame %d\n", frame_count_);
#ifdef HAS_SDL2
                if (has_display_) SDL_Delay(16);
#endif
            }
        }
        if (vm_.IsHalted()) {
            printf("VM halted after %d frames\n", frame_count_);
        } else {
            printf("Emulator stopped after %d frames\n", frame_count_);
        }
    }

private:
    static uint16_t ReadLE16(const uint8_t* p) {
        return p[0] | (p[1] << 8);
    }

    static uint32_t ReadLE32(const uint8_t* p) {
        return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
    }

    static uint32_t BytesPerPixel(PixelFormat fmt) {
        switch (fmt) {
            case PixelFormat::RGB565:
            case PixelFormat::RGBA4444:
                return 2;
            case PixelFormat::Indexed8:
            case PixelFormat::Indexed8Alpha:
                return 1;
            case PixelFormat::RGBA8888:
            case PixelFormat::ARGB8888:
            default:
                return 4;
        }
    }

    void LoadAssetsFromCartridge(const uint8_t* data, size_t size) {
        if (size < ROM_HEADER_SIZE || memcmp(data, "FPVM", 4) != 0) return;

        uint32_t asset_count = ReadLE32(data + 0x10);
        uint32_t asset_dir_offset = ReadLE32(data + 0x14);

        if (asset_count == 0 || asset_dir_offset == 0) return;
        if (asset_dir_offset + asset_count * 32 > size) return;

        const uint8_t* asset_dir = data + asset_dir_offset;
        uint32_t asset_data_offset = asset_dir_offset + asset_count * 32;
        uint32_t last_tileset_id = 0xFFFFFFFFu;

        tilemap_storage_.clear();
        tilemap_storage_.reserve(asset_count);

        for (uint32_t i = 0; i < asset_count; i++) {
            const uint8_t* entry = asset_dir + i * 32;
            uint32_t id = ReadLE32(entry + 0);
            uint32_t relative_offset = ReadLE32(entry + 4);
            uint32_t asset_size = ReadLE32(entry + 8);
            uint32_t type = ReadLE32(entry + 12);
            uint32_t aux0 = ReadLE32(entry + 16);
            uint32_t aux1 = ReadLE32(entry + 20);
            uint32_t asset_offset = asset_data_offset + relative_offset;

            if (asset_offset + asset_size > size) continue;
            const uint8_t* asset = data + asset_offset;

            if (type == 1 || type == 2 || type == 3) {
                LoadImageLikeAsset(id, type, aux0, aux1, asset, asset_size);
                if (type == 3) last_tileset_id = id;
            } else if (type == 4) {
                LoadTileMapAsset(id, last_tileset_id, asset, asset_size);
            }
        }

        printf("Loaded %u cartridge assets\n", asset_count);
    }

    void LoadImageLikeAsset(uint32_t id, uint32_t type, uint32_t aux0, uint32_t aux1,
                            const uint8_t* asset, uint32_t asset_size) {
        if (asset_size < 10) return;

        uint16_t width = ReadLE16(asset + 0);
        uint16_t height = ReadLE16(asset + 2);
        PixelFormat format = static_cast<PixelFormat>(asset[4]);
        uint8_t flags = asset[5];
        uint32_t bpp = BytesPerPixel(format);
        uint32_t source_pitch = width * bpp;
        const uint8_t* pixels = asset + 10;

        if (width == 0 || height == 0 || 10u + source_pitch * height > asset_size) return;

        Surface* surface = renderer_.AllocateSurfaceAt(id, width, height, format);
        if (!surface) return;
        surface->flags = flags;

        for (uint16_t y = 0; y < height; y++) {
            memcpy(surface->pixels + y * surface->pitch, pixels + y * source_pitch, source_pitch);
        }

        if (type == 2) {
            SpriteDef sprite{};
            sprite.width = width;
            sprite.height = height;
            sprite.hotspot_x = 0;
            sprite.hotspot_y = 0;
            sprite.format = format;
            sprite.flags = flags;
            sprite.surface_id = id;
            renderer_.RegisterSprite(id, sprite);
        } else if (type == 3) {
            TileSet tileset{};
            tileset.tile_width = aux0 ? static_cast<uint16_t>(aux0) : width;
            tileset.tile_height = aux1 ? static_cast<uint16_t>(aux1) : height;
            tileset.tile_count = (tileset.tile_width && tileset.tile_height)
                ? static_cast<uint16_t>((width / tileset.tile_width) * (height / tileset.tile_height))
                : 0;
            tileset.format = format;
            tileset.surface_id = id;
            renderer_.RegisterTileSet(id, tileset);
        }
    }

    void LoadTileMapAsset(uint32_t id, uint32_t tileset_id, const uint8_t* asset, uint32_t asset_size) {
        if (asset_size < 12 || tileset_id == 0xFFFFFFFFu) return;

        uint16_t width = ReadLE16(asset + 0);
        uint16_t height = ReadLE16(asset + 2);
        uint16_t tile_width = ReadLE16(asset + 4);
        uint16_t tile_height = ReadLE16(asset + 6);
        uint32_t tile_count = ReadLE32(asset + 8);
        const uint8_t* tiles = asset + 12;
        uint32_t tile_bytes = tile_count * 3;

        if (width == 0 || height == 0 || tile_count < static_cast<uint32_t>(width) * height) return;
        if (12u + tile_bytes > asset_size) return;

        tilemap_storage_.emplace_back(tiles, tiles + tile_bytes);

        TileMapDef tilemap{};
        tilemap.width = width;
        tilemap.height = height;
        tilemap.tile_width = tile_width;
        tilemap.tile_height = tile_height;
        tilemap.tileset_id = tileset_id;
        tilemap.tiles = tilemap_storage_.back().data();
        renderer_.RegisterTileMap(id, tilemap);
    }

    static bool EmulatorTrapHandler(uint32_t vector, uint32_t* regs, void* ud) {
        return static_cast<Emulator*>(ud)->HandleTrap(vector, regs);
    }

    bool HandleTrap(uint32_t vector, uint32_t* regs) {
        switch (vector) {
            case 0x10: { // GFX_INIT
                uint32_t w = regs[0] ? regs[0] : 640;
                uint32_t h = regs[1] ? regs[1] : 480;
                renderer_.Init(w, h); width_ = w; height_ = h;
                if (!cartridge_data_.empty()) {
                    LoadAssetsFromCartridge(cartridge_data_.data(), cartridge_data_.size());
                }
#ifdef HAS_SDL2
                if (has_display_) backend_.Init(w, h, "Fantasy Pi Emulator");
#endif
                return true;
            }
            case 0x11: return true; // GFX_PRESENT
            case 0x12: renderer_.Clear(regs[0]); return true; // GFX_CLEAR
            case 0x13: // GFX_DRAW_SPRITE
                renderer_.DrawSprite(regs[0], (int)regs[1], (int)regs[2],
                                     static_cast<BlendMode>(regs[3] & 0xFF), 255);
                return true;
            case 0x14: // GFX_DRAW_IMAGE
                renderer_.DrawImage(regs[0], (int)regs[1], (int)regs[2],
                                    static_cast<BlendMode>(regs[3] & 0xFF), 255);
                return true;
            case 0x15: { // GFX_BITBLT
                BlitRect r;
                r.dst_x = (int16_t)regs[1]; r.dst_y = (int16_t)regs[2];
                r.src_x = (int16_t)regs[3]; r.src_y = (int16_t)regs[4];
                r.width = (uint16_t)regs[5]; r.height = (uint16_t)regs[6];
                Surface* src = renderer_.GetSurface(regs[0]);
                if (src) renderer_.BitBlt(*src, *renderer_.GetFramebuffer(), r,
                                          static_cast<BlendMode>(regs[7] & 0xFF), (uint8_t)(regs[8] & 0xFF));
                return true;
            }
            case 0x16: // GFX_DRAW_TILEMAP
                renderer_.DrawTileMap(regs[0], (int)regs[1], (int)regs[2]);
                return true;
            case 0x18: { // GFX_DRAW_TEXT
                const char* text = (const char*)(vm_.GetRAM() + regs[1]);
                renderer_.DrawText(regs[0], text, regs[2] & 0xFFFF, (regs[2] >> 16) & 0xFFFF, regs[3]);
                return true;
            }
            case 0x20: return true; // AUDIO_INIT (no-op: emulator has no audio output)
            case 0x21: return true; // AUDIO_PLAY (no-op: emulator has no audio output)
            case 0x22: return true; // AUDIO_STOP (no-op: emulator has no audio output)
            case 0x30: return true; // INPUT_POLL
            case 0x31: { // INPUT_KEY
                if (regs[0] < 256) {
                    InputState* s = (InputState*)(vm_.GetRAM() + INPUT_STATE_ADDR);
                    regs[0] = (s->keyboard_keys[regs[0] / 32] >> (regs[0] % 32)) & 1;
                } else regs[0] = 0;
                return true;
            }
            case 0x32: { // INPUT_GAMEPAD
                uint32_t player = regs[0] < 2 ? regs[0] : 0;
                InputState* s = (InputState*)(vm_.GetRAM() + INPUT_STATE_ADDR);
                regs[0] = s->gamepad_buttons[player];
                regs[1] = static_cast<uint16_t>(s->gamepad_axis[player][0]) |
                          (static_cast<uint32_t>(static_cast<uint16_t>(s->gamepad_axis[player][1])) << 16);
                regs[2] = static_cast<uint16_t>(s->gamepad_axis[player][2]) |
                          (static_cast<uint32_t>(static_cast<uint16_t>(s->gamepad_axis[player][3])) << 16);
                return true;
            }
            case 0x40: { // MEM_COPY
                uint8_t* ram = vm_.GetRAM();
                if (regs[0] + regs[2] <= RAM_SIZE && regs[1] + regs[2] <= RAM_SIZE)
                    memmove(ram + regs[1], ram + regs[0], regs[2]);
                return true;
            }
            case 0x41: { // MEM_FILL
                uint8_t* ram = vm_.GetRAM();
                if (regs[0] + regs[2] <= RAM_SIZE)
                    memset(ram + regs[0], regs[1] & 0xFF, regs[2]);
                return true;
            }
            case 0x50: { // MATH_RAND
                static uint32_t seed = 12345;
                if (regs[0]) seed = regs[0];
                seed = seed * 1103515245 + 12345;
                regs[0] = seed;
                return true;
            }
            case 0x51: { // MATH_SIN
                regs[0] = (uint32_t)(int32_t)(sinf((regs[0] / 65535.0f) * 6.2831853f) * 65536.0f);
                return true;
            }
            case 0x52: { // MATH_COS
                regs[0] = (uint32_t)(int32_t)(cosf((regs[0] / 65535.0f) * 6.2831853f) * 65536.0f);
                return true;
            }
            case 0x60: printf("[VM] %u\n", regs[0]); return true;
            case 0x61: printf("[VM] %s\n", (const char*)(vm_.GetRAM() + regs[0])); return true;
            default: return false;
        }
    }

    void InitInputState() {
        uint8_t* ram = vm_.GetRAM();
        if (INPUT_STATE_ADDR + sizeof(InputState) <= RAM_SIZE)
            memset(ram + INPUT_STATE_ADDR, 0, sizeof(InputState));
    }

#ifdef HAS_SDL2
    void UpdateInputStateSDL() {
        uint8_t* ram = vm_.GetRAM();
        if (INPUT_STATE_ADDR + sizeof(InputState) > RAM_SIZE) return;
        InputState* state = (InputState*)(ram + INPUT_STATE_ADDR);
        memset(state, 0, sizeof(InputState));
        const int map[][2] = {
            {SDL_SCANCODE_UP,0}, {SDL_SCANCODE_DOWN,1}, {SDL_SCANCODE_LEFT,2}, {SDL_SCANCODE_RIGHT,3},
            {SDL_SCANCODE_Z,4}, {SDL_SCANCODE_X,5}, {SDL_SCANCODE_RETURN,6}, {SDL_SCANCODE_ESCAPE,7}
        };
        for (auto& m : map) {
            if (backend_.IsKeyDown(m[0]))
                state->keyboard_keys[m[1]/32] |= (1U << (m[1]%32));
        }

        state->gamepad_buttons[0] = backend_.GetGamepadButtons(0);

        const int key_to_button[][2] = {
            {SDL_SCANCODE_UP, 0}, {SDL_SCANCODE_W, 0},
            {SDL_SCANCODE_DOWN, 1}, {SDL_SCANCODE_S, 1},
            {SDL_SCANCODE_LEFT, 2}, {SDL_SCANCODE_A, 2},
            {SDL_SCANCODE_RIGHT, 3}, {SDL_SCANCODE_D, 3},
            {SDL_SCANCODE_Z, 4}, {SDL_SCANCODE_SPACE, 4},
            {SDL_SCANCODE_X, 5}, {SDL_SCANCODE_LSHIFT, 5},
            {SDL_SCANCODE_RETURN, 6},
            {SDL_SCANCODE_ESCAPE, 7}
        };
        for (auto& m : key_to_button) {
            if (backend_.IsKeyDown(m[0])) {
                state->gamepad_buttons[0] |= 1U << m[1];
            }
        }

        for (int axis = 0; axis < 4; axis++) {
            state->gamepad_axis[0][axis] = backend_.GetGamepadAxis(0, axis);
        }
        if (state->gamepad_buttons[0] & (1U << 2)) state->gamepad_axis[0][0] = -32768;
        if (state->gamepad_buttons[0] & (1U << 3)) state->gamepad_axis[0][0] = 32767;
        if (state->gamepad_buttons[0] & (1U << 0)) state->gamepad_axis[0][1] = -32768;
        if (state->gamepad_buttons[0] & (1U << 1)) state->gamepad_axis[0][1] = 32767;
    }
#endif

private:
    FantasyVM vm_;
    Renderer renderer_;
    std::vector<uint8_t> cartridge_data_;
    std::vector<std::vector<uint8_t>> tilemap_storage_;
#ifdef HAS_SDL2
    SDLBackend backend_;
#endif
    int width_ = 640, height_ = 480;
    bool has_display_ = false;
    int frame_count_ = 0;
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <cartridge.bin>\n", argv[0]);
        return 1;
    }
    Emulator emu;
    if (!emu.Init(640, 480)) return 1;
    if (!emu.LoadCartridge(argv[1])) return 1;
    emu.Run();
    return 0;
}
