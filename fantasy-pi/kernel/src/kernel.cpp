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

#include "kernel.h"
#include <circle/startup.h>
#include <circle/bcmpropertytags.h>
#include <circle/memory.h>
#include <circle/util.h>
#include <circle/gpiopin.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <cstdio>
#include <assert.h>

extern "C" {
    extern const unsigned char _binary_cartridge_bin_start[];
    extern const unsigned char _binary_cartridge_bin_end[];
}

CKernel *CKernel::s_pThis = 0;

// Address in VM RAM where input state is mapped
constexpr uint32_t INPUT_STATE_ADDR = 0x00010000;

#define SCREEN_WRITE_LITERAL(text) m_Screen.Write(text, sizeof(text) - 1)

static int32_t FixedSin16(uint32_t angle) {
    uint32_t phase = angle & 0xFFFF;
    bool negative = phase >= 0x8000;
    uint32_t folded = negative ? 0x10000 - phase : phase;
    if (folded > 0x4000) {
        folded = 0x8000 - folded;
    }

    int32_t value = (int32_t)((folded * 65536U) / 0x4000);
    return negative ? -value : value;
}

CKernel::CKernel(void)
    : m_Screen(m_Options.GetWidth(), m_Options.GetHeight()),
      m_Timer(&m_Interrupt),
      m_Logger(m_Options.GetLogLevel(), &m_Timer),
      m_USBHCI(&m_Interrupt, &m_Timer, TRUE),
      m_EMMC(&m_Interrupt, &m_Timer, &m_ActLED),
      m_pSoundDevice(nullptr),
      m_VM(),
      m_Renderer(),
      m_pKeyboard(0),
      m_pCartridgeData(0),
      m_nCartridgeSize(0),
      m_bOwnCartridgeData(FALSE),
      m_nTileMapDataCount(0)
{
    s_pThis = this;
    memset(&m_InputState, 0, sizeof(m_InputState));
    memset(&m_GamePadState, 0, sizeof(m_GamePadState));
    for (unsigned i = 0; i < 2; ++i) {
        m_pGamePad[i] = 0;
    }
    for (unsigned i = 0; i < 16; ++i) {
        m_pTileMapData[i] = 0;
    }
    m_ActLED.Blink(5);
}

CKernel::~CKernel(void) {
    ReleaseCartridgeData();
    ClearTileMapStorage();
    s_pThis = 0;
}

boolean CKernel::Initialize(void) {
    boolean bOK = TRUE;

    if (bOK) {
        bOK = m_Screen.Initialize();
        if (bOK) {
            SCREEN_WRITE_LITERAL("Fantasy Pi boot\n");
        }
    }
    if (bOK) {
        SCREEN_WRITE_LITERAL("Serial...\n");
        bOK = m_Serial.Initialize(115200);
    }
    if (bOK) {
        CDevice *pTarget = m_DeviceNameService.GetDevice(m_Options.GetLogDevice(), FALSE);
        if (pTarget == 0) pTarget = &m_Screen;
        bOK = m_Logger.Initialize(pTarget);
    }
    if (bOK) {
        SCREEN_WRITE_LITERAL("Interrupts...\n");
        bOK = m_Interrupt.Initialize();
    }
    if (bOK) {
        SCREEN_WRITE_LITERAL("Timer...\n");
        bOK = m_Timer.Initialize();
    }
    if (bOK) {
        SCREEN_WRITE_LITERAL("USB...\n");
        bOK = m_USBHCI.Initialize();
    }
    if (bOK) {
        SCREEN_WRITE_LITERAL("eMMC...\n");
        bOK = m_EMMC.Initialize();
    }
    if (bOK) {
        SCREEN_WRITE_LITERAL("Renderer...\n");
        m_Renderer.Init(m_Screen.GetWidth(), m_Screen.GetHeight());
    }
    if (bOK) {
        SCREEN_WRITE_LITERAL("HDMI Audio...\n");
        m_pSoundDevice = new CHDMISoundBaseDevice(&m_Interrupt, 44100, 3840);
        if (!m_pSoundDevice->AllocateQueue(100)) {
            SCREEN_WRITE_LITERAL("Audio queue failed\n");
        }
        m_pSoundDevice->SetWriteFormat(SoundFormatSigned16, 2);
        if (!m_pSoundDevice->Start()) {
            SCREEN_WRITE_LITERAL("Audio start failed\n");
        }
        m_AudioMixer.Init();
    }

    // Register VM trap callback
    m_VM.SetTrapCallback(VMTrapHandler, this);

    return bOK;
}

TShutdownMode CKernel::Run(void) {
    m_Logger.Write("fantasy-pi", LogNotice, "Fantasy Pi Kernel started");

    bool cartridge_loaded = false;
    CDevice* pPartition = m_DeviceNameService.GetDevice("emmc1-1", FALSE);
    if (!pPartition) {
        m_Logger.Write("fantasy-pi", LogWarning, "SD partition emmc1-1 not found");
    } else {
        if (!m_FileSystem.Mount(pPartition)) {
            m_Logger.Write("fantasy-pi", LogWarning, "Failed to mount emmc1-1");
        } else {
            m_Logger.Write("fantasy-pi", LogNotice, "SD mounted, loading cartridge.bin");
            cartridge_loaded = LoadCartridge("cartridge.bin");
            if (!cartridge_loaded) {
                m_Logger.Write("fantasy-pi", LogWarning, "cartridge.bin not found on SD");
            }
        }
    }
    if (!cartridge_loaded) {
        m_Logger.Write("fantasy-pi", LogNotice, "Falling back to embedded cartridge");
        cartridge_loaded = LoadEmbeddedCartridge();
    }

    if (!cartridge_loaded) {
        m_Logger.Write("fantasy-pi", LogError, "No cartridge found");
        m_Screen.Write("NO CARTRIDGE", 12);
        while (1) {
            m_Screen.Rotor(0, 0);
            m_Timer.MsDelay(100);
        }
    }

    m_Logger.Write("fantasy-pi", LogNotice, "Cartridge loaded, starting VM");
    InitInputState();

    m_pKeyboard = (CUSBKeyboardDevice *)m_DeviceNameService.GetDevice("ukbd1", FALSE);
    if (m_pKeyboard) {
        m_pKeyboard->RegisterKeyStatusHandlerRaw(KeyStatusHandlerRaw);
    }

    while (1) {
        m_USBHCI.UpdatePlugAndPlay();
        RegisterGamepads();
        UpdateInputState();

        // Run VM until it requests present or halts
        m_VM.RunFrame();

        // Present renderer output to screen
        if (m_VM.IsPresentRequested()) {
            m_Renderer.Present();
            CopyFrameBufferToScreen();
            m_VM.ClearPresentRequest();
        }

        // Feed audio
        if (m_pSoundDevice && m_pSoundDevice->IsActive()) {
            unsigned nFramesAvail = m_pSoundDevice->GetQueueFramesAvail();
            if (nFramesAvail > 0) {
                const unsigned nMaxFrames = 512;
                if (nFramesAvail > nMaxFrames) {
                    nFramesAvail = nMaxFrames;
                }
                int16_t buffer[nMaxFrames * 2];
                m_AudioMixer.Mix(buffer, nFramesAvail);
                m_pSoundDevice->Write(buffer, nFramesAvail * 2 * sizeof(int16_t));
            }
        }

        // Frame rate limit ~60Hz
        m_Timer.MsDelay(16);
    }

    return ShutdownHalt;
}

void CKernel::RegisterGamepads(void) {
    for (unsigned nDevice = 1; nDevice <= 2; ++nDevice) {
        unsigned index = nDevice - 1;
        if (m_pGamePad[index] != 0) {
            continue;
        }

        CUSBGamePadDevice* pGamePad =
            (CUSBGamePadDevice*)m_DeviceNameService.GetDevice("upad", nDevice, FALSE);
        if (pGamePad == 0) {
            continue;
        }

        m_pGamePad[index] = pGamePad;
        m_pGamePad[index]->RegisterRemovedHandler(GamepadRemovedHandler, this);
        m_pGamePad[index]->RegisterStatusHandler(GamepadStatusHandler);
    }
}

void CKernel::InitInputState(void) {
    // Write InputState structure into VM RAM at fixed address
    uint8_t* ram = m_VM.GetRAM();
    if (INPUT_STATE_ADDR + sizeof(InputState) <= fantasy::RAM_SIZE) {
        memcpy(ram + INPUT_STATE_ADDR, &m_InputState, sizeof(m_InputState));
    }
}

void CKernel::UpdateInputState(void) {
    // Copy current input state into VM RAM
    uint8_t* ram = m_VM.GetRAM();
    if (INPUT_STATE_ADDR + sizeof(InputState) <= fantasy::RAM_SIZE) {
        memcpy(ram + INPUT_STATE_ADDR, &m_InputState, sizeof(m_InputState));
    }
}

static uint16_t ReadLE16(const uint8_t* p) {
    return p[0] | (p[1] << 8);
}

static uint32_t ReadLE32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static uint32_t RendererBytesPerPixel(fantasy::PixelFormat fmt) {
    switch (fmt) {
        case fantasy::PixelFormat::RGB565:
        case fantasy::PixelFormat::RGBA4444:
            return 2;
        case fantasy::PixelFormat::Indexed8:
        case fantasy::PixelFormat::Indexed8Alpha:
            return 1;
        case fantasy::PixelFormat::RGBA8888:
        case fantasy::PixelFormat::ARGB8888:
        default:
            return 4;
    }
}

void CKernel::ReleaseCartridgeData(void) {
    if (m_bOwnCartridgeData && m_pCartridgeData) {
        delete[] m_pCartridgeData;
    }
    m_pCartridgeData = 0;
    m_nCartridgeSize = 0;
    m_bOwnCartridgeData = FALSE;
}

void CKernel::ClearTileMapStorage(void) {
    for (unsigned i = 0; i < m_nTileMapDataCount; ++i) {
        delete[] m_pTileMapData[i];
        m_pTileMapData[i] = 0;
    }
    m_nTileMapDataCount = 0;
}

void CKernel::LoadAssetsFromCartridge(const uint8_t* data, size_t size) {
    ClearTileMapStorage();

    if (size < fantasy::ROM_HEADER_SIZE || memcmp(data, "FPVM", 4) != 0) return;

    uint32_t asset_count = ReadLE32(data + 0x10);
    uint32_t asset_dir_offset = ReadLE32(data + 0x14);
    if (asset_count == 0 || asset_dir_offset == 0) return;
    if (asset_dir_offset + asset_count * 32 > size) return;

    const uint8_t* asset_dir = data + asset_dir_offset;
    uint32_t asset_data_offset = asset_dir_offset + asset_count * 32;
    uint32_t last_tileset_id = 0xFFFFFFFFu;

    for (uint32_t i = 0; i < asset_count; ++i) {
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
}

void CKernel::LoadImageLikeAsset(uint32_t id, uint32_t type, uint32_t aux0, uint32_t aux1,
                                 const uint8_t* asset, uint32_t asset_size) {
    if (asset_size < 10) return;

    uint16_t width = ReadLE16(asset + 0);
    uint16_t height = ReadLE16(asset + 2);
    fantasy::PixelFormat format = static_cast<fantasy::PixelFormat>(asset[4]);
    uint8_t flags = asset[5];
    uint32_t bpp = RendererBytesPerPixel(format);
    uint32_t source_pitch = width * bpp;
    const uint8_t* pixels = asset + 10;

    if (width == 0 || height == 0 || 10u + source_pitch * height > asset_size) return;

    fantasy::Surface* surface = m_Renderer.AllocateSurfaceAt(id, width, height, format);
    if (!surface) return;
    surface->flags = flags;

    for (uint16_t y = 0; y < height; ++y) {
        memcpy(surface->pixels + y * surface->pitch, pixels + y * source_pitch, source_pitch);
    }

    if (type == 2) {
        fantasy::SpriteDef sprite{};
        sprite.width = width;
        sprite.height = height;
        sprite.hotspot_x = 0;
        sprite.hotspot_y = 0;
        sprite.format = format;
        sprite.flags = flags;
        sprite.surface_id = id;
        m_Renderer.RegisterSprite(id, sprite);
    } else if (type == 3) {
        fantasy::TileSet tileset{};
        tileset.tile_width = aux0 ? static_cast<uint16_t>(aux0) : width;
        tileset.tile_height = aux1 ? static_cast<uint16_t>(aux1) : height;
        tileset.tile_count = (tileset.tile_width && tileset.tile_height)
            ? static_cast<uint16_t>((width / tileset.tile_width) * (height / tileset.tile_height))
            : 0;
        tileset.format = format;
        tileset.surface_id = id;
        m_Renderer.RegisterTileSet(id, tileset);
    }
}

void CKernel::LoadTileMapAsset(uint32_t id, uint32_t tileset_id,
                               const uint8_t* asset, uint32_t asset_size) {
    if (asset_size < 12 || tileset_id == 0xFFFFFFFFu) return;
    if (m_nTileMapDataCount >= 16) return;

    uint16_t width = ReadLE16(asset + 0);
    uint16_t height = ReadLE16(asset + 2);
    uint16_t tile_width = ReadLE16(asset + 4);
    uint16_t tile_height = ReadLE16(asset + 6);
    uint32_t tile_count = ReadLE32(asset + 8);
    const uint8_t* tiles = asset + 12;
    uint32_t tile_bytes = tile_count * 3;

    if (width == 0 || height == 0 || tile_count < static_cast<uint32_t>(width) * height) return;
    if (12u + tile_bytes > asset_size) return;

    uint8_t* tile_copy = new uint8_t[tile_bytes];
    memcpy(tile_copy, tiles, tile_bytes);
    m_pTileMapData[m_nTileMapDataCount++] = tile_copy;

    fantasy::TileMapDef tilemap{};
    tilemap.width = width;
    tilemap.height = height;
    tilemap.tile_width = tile_width;
    tilemap.tile_height = tile_height;
    tilemap.tileset_id = tileset_id;
    tilemap.tiles = tile_copy;
    m_Renderer.RegisterTileMap(id, tilemap);
}

void CKernel::CopyFrameBufferToScreen(void) {
    // Copy renderer front buffer to Circle screen framebuffer
    uint8_t* front = m_Renderer.GetFrontBufferPixels();
    if (!front) return;

    unsigned screen_w = m_Screen.GetWidth();
    unsigned screen_h = m_Screen.GetHeight();
    CBcmFrameBuffer* screen_fb = m_Screen.GetFrameBuffer();
    if (!screen_fb) return;

    unsigned screen_pitch = screen_fb->GetPitch();
    unsigned screen_depth = screen_fb->GetDepth();
    uint8_t* screen_ptr = (uint8_t*)(uintptr_t)screen_fb->GetBuffer();

    // Circle screen is typically 16-bit RGB565 or 32-bit BGRA.
    unsigned depth_bytes = screen_depth / 8;

    if (depth_bytes == 4) {
        // Fast path: 32-bit copy (may need byte swap depending on format)
        uint32_t* src = (uint32_t*)front;
        uint32_t* dst = (uint32_t*)screen_ptr;
        unsigned copy_w = screen_w < m_Renderer.GetFramebuffer()->width ? screen_w : m_Renderer.GetFramebuffer()->width;
        unsigned copy_h = screen_h < m_Renderer.GetFramebuffer()->height ? screen_h : m_Renderer.GetFramebuffer()->height;
        unsigned src_pitch_pixels = m_Renderer.GetFramebuffer()->pitch / 4;
        unsigned dst_pitch_pixels = screen_pitch / 4;
        for (unsigned y = 0; y < copy_h; y++) {
            for (unsigned x = 0; x < copy_w; x++) {
                uint32_t c = src[y * src_pitch_pixels + x];
                // ARGB8888 to ABGR8888 or direct depending on Circle's format
                // Circle usually uses BGRA for 32-bit on VC4
                dst[y * dst_pitch_pixels + x] = c;
            }
        }
    } else if (depth_bytes == 2) {
        // 16-bit BGR565 conversion
        uint32_t* src = (uint32_t*)front;
        uint16_t* dst = (uint16_t*)screen_ptr;
        unsigned src_pitch_pixels = m_Renderer.GetFramebuffer()->pitch / 4;
        unsigned dst_pitch_pixels = screen_pitch / 2;
        unsigned copy_w = screen_w < m_Renderer.GetFramebuffer()->width ? screen_w : m_Renderer.GetFramebuffer()->width;
        unsigned copy_h = screen_h < m_Renderer.GetFramebuffer()->height ? screen_h : m_Renderer.GetFramebuffer()->height;
        for (unsigned y = 0; y < copy_h; y++) {
            for (unsigned x = 0; x < copy_w; x++) {
                uint32_t c = src[y * src_pitch_pixels + x];
                uint32_t r = (c >> 16) & 0xFF;
                uint32_t g = (c >> 8) & 0xFF;
                uint32_t b = c & 0xFF;
                uint16_t v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                dst[y * dst_pitch_pixels + x] = v;
            }
        }
    }

    m_Screen.Update(0);
}

bool CKernel::LoadCartridge(const char* filename) {
    unsigned hFile = m_FileSystem.FileOpen(filename);
    if (hFile == 0) return false;

    const unsigned max_size = 32 * 1024 * 1024;
    uint8_t* buffer = new uint8_t[max_size];
    unsigned total = 0;

    while (total < max_size) {
        unsigned result = m_FileSystem.FileRead(hFile, buffer + total, max_size - total);
        if (result == 0) {
            break;
        }
        if (result == FS_ERROR) {
            m_FileSystem.FileClose(hFile);
            delete[] buffer;
            return false;
        }
        total += result;
    }

    m_FileSystem.FileClose(hFile);
    if (total == 0 || total >= max_size) {
        delete[] buffer;
        return false;
    }

    ReleaseCartridgeData();
    m_pCartridgeData = buffer;
    m_nCartridgeSize = total;
    m_bOwnCartridgeData = TRUE;

    m_VM.LoadCartridge(buffer, total);
    LoadAssetsFromCartridge(buffer, total);
    return true;
}

bool CKernel::LoadEmbeddedCartridge(void) {
    size_t size = _binary_cartridge_bin_end - _binary_cartridge_bin_start;
    if (size == 0) return false;
    ReleaseCartridgeData();
    m_pCartridgeData = const_cast<uint8_t*>(_binary_cartridge_bin_start);
    m_nCartridgeSize = size;
    m_bOwnCartridgeData = FALSE;
    m_VM.LoadCartridge(_binary_cartridge_bin_start, size);
    LoadAssetsFromCartridge(_binary_cartridge_bin_start, size);
    return true;
}

bool CKernel::VMTrapHandler(uint32_t vector, uint32_t* regs, void* user_data) {
    if (!s_pThis) return false;
    return s_pThis->HandleVMTrap(vector, regs);
}

bool CKernel::HandleVMTrap(uint32_t vector, uint32_t* regs) {
    switch (vector) {
        case 0x00: // HALT - handled internally by VM
            return false;

        case 0x01: { // SLEEP
            uint32_t ms = regs[0];
            m_Timer.MsDelay(ms);
            return true;
        }

        case 0x10: { // GFX_INIT
            uint32_t w = regs[0] ? regs[0] : 640;
            uint32_t h = regs[1] ? regs[1] : 480;
            m_Renderer.Init(w, h);
            if (m_pCartridgeData && m_nCartridgeSize) {
                LoadAssetsFromCartridge(m_pCartridgeData, m_nCartridgeSize);
            }
            return true;
        }

        case 0x11: // GFX_PRESENT
            // Frame present triggered; main loop handles actual flip
            return true;

        case 0x12: // GFX_CLEAR
            m_Renderer.Clear(regs[0]);
            return true;

        case 0x13: // GFX_DRAW_SPRITE
            m_Renderer.DrawSprite(regs[0], (int)regs[1], (int)regs[2],
                                  static_cast<fantasy::BlendMode>(regs[3] & 0xFF), 255);
            return true;

        case 0x14: // GFX_DRAW_IMAGE
            m_Renderer.DrawImage(regs[0], (int)regs[1], (int)regs[2],
                                 static_cast<fantasy::BlendMode>(regs[3] & 0xFF), 255);
            return true;

        case 0x15: { // GFX_BITBLT
            fantasy::BlitRect rect;
            rect.src_x = (int16_t)regs[3];
            rect.src_y = (int16_t)regs[4];
            rect.dst_x = (int16_t)regs[1];
            rect.dst_y = (int16_t)regs[2];
            rect.width = (uint16_t)regs[5];
            rect.height = (uint16_t)regs[6];
            fantasy::Surface* src = m_Renderer.GetSurface(regs[0]);
            if (src) {
                m_Renderer.BitBlt(*src, *m_Renderer.GetFramebuffer(), rect,
                                  static_cast<fantasy::BlendMode>(regs[7] & 0xFF),
                                  (uint8_t)(regs[8] & 0xFF));
            }
            return true;
        }

        case 0x16: // GFX_DRAW_TILEMAP
            m_Renderer.DrawTileMap(regs[0], (int)regs[1], (int)regs[2]);
            return true;

        case 0x17: { // GFX_SET_PALETTE
            uint32_t pal_id = regs[0];
            uint32_t idx = regs[1];
            uint32_t color = regs[2];
            if (pal_id < 16 && idx < 256) {
                m_Renderer.RegisterPalette(pal_id, &color, 1);
            }
            return true;
        }

        case 0x18: { // GFX_DRAW_TEXT
            // regs[0] = font_id, regs[1] = string_addr, regs[2] = (y<<16)|x
            uint32_t str_addr = regs[1];
            int x = regs[2] & 0xFFFF;
            int y = (regs[2] >> 16) & 0xFFFF;
            uint32_t color = regs[3];
            const char* text = (const char*)(m_VM.GetRAM() + str_addr);
            m_Renderer.DrawText(regs[0], text, x, y, color);
            return true;
        }

        case 0x20: // AUDIO_INIT
            m_AudioMixer.Init();
            return true;

        case 0x21: { // AUDIO_PLAY
            uint32_t sample_id = regs[0];
            uint32_t channel = regs[1];
            uint32_t volume = regs[2];
            m_AudioMixer.Play(sample_id, channel, volume);
            return true;
        }

        case 0x22: { // AUDIO_STOP
            uint32_t channel = regs[0];
            m_AudioMixer.Stop(channel);
            return true;
        }

        case 0x30: // INPUT_POLL
            UpdateInputState();
            return true;

        case 0x31: { // INPUT_KEY
            uint32_t keycode = regs[0];
            if (keycode < 256) {
                uint32_t word = keycode / 32;
                uint32_t bit = keycode % 32;
                uint8_t* ram = m_VM.GetRAM();
                InputState* state = (InputState*)(ram + INPUT_STATE_ADDR);
                regs[0] = (state->keyboard_keys[word] >> bit) & 1;
            } else {
                regs[0] = 0;
            }
            return true;
        }

        case 0x32: { // INPUT_GAMEPAD
            uint32_t player = regs[0];
            if (player < 2) {
                uint8_t* ram = m_VM.GetRAM();
                InputState* state = (InputState*)(ram + INPUT_STATE_ADDR);
                regs[0] = state->gamepad_buttons[player];
                regs[1] = (uint32_t)(uint16_t)state->gamepad_axis[player][0]; // LX
                regs[2] = (uint32_t)(uint16_t)state->gamepad_axis[player][1]; // LY
            } else {
                regs[0] = 0;
            }
            return true;
        }

        case 0x40: { // MEM_COPY
            uint8_t* ram = m_VM.GetRAM();
            uint32_t src = regs[0];
            uint32_t dst = regs[1];
            uint32_t len = regs[2];
            if (src + len <= fantasy::RAM_SIZE && dst + len <= fantasy::RAM_SIZE) {
                memmove(ram + dst, ram + src, len);
            }
            return true;
        }

        case 0x41: { // MEM_FILL
            uint8_t* ram = m_VM.GetRAM();
            uint32_t dst = regs[0];
            uint32_t value = regs[1] & 0xFF;
            uint32_t len = regs[2];
            if (dst + len <= fantasy::RAM_SIZE) {
                memset(ram + dst, value, len);
            }
            return true;
        }

        case 0x50: { // MATH_RAND
            // Simple LCG; seed in regs[0], returns next value
            static uint32_t seed = 12345;
            if (regs[0] != 0) seed = regs[0];
            seed = seed * 1103515245 + 12345;
            regs[0] = seed;
            return true;
        }

        case 0x51: { // MATH_SIN
            // Fixed point sin: angle 0-65535 maps to 0-2PI, result is 16.16 fixed point
            uint32_t angle = regs[0];
            regs[0] = (uint32_t)FixedSin16(angle);
            return true;
        }

        case 0x52: { // MATH_COS
            uint32_t angle = regs[0];
            regs[0] = (uint32_t)FixedSin16(angle + 0x4000);
            return true;
        }

        case 0x60: // DEBUG_LOG
            m_Logger.Write("vm", LogDebug, "R0=%u", regs[0]);
            return true;

        case 0x61: { // DEBUG_LOG_STR
            uint32_t addr = regs[0];
            const char* str = (const char*)(m_VM.GetRAM() + addr);
            m_Logger.Write("vm", LogDebug, str);
            return true;
        }

        default:
            return false;
    }
}

void CKernel::KeyStatusHandlerRaw(unsigned char ucModifiers, const unsigned char RawKeys[6]) {
    if (!s_pThis) return;
    InputState* state = &s_pThis->m_InputState;
    memset(state->keyboard_keys, 0, sizeof(state->keyboard_keys));
    for (int i = 0; i < 6; i++) {
        unsigned char key = RawKeys[i];
        if (key) {
            unsigned word = key / 32;
            unsigned bit = key % 32;
            if (word < 8) {
                state->keyboard_keys[word] |= (1U << bit);
            }
        }
    }
}

void CKernel::GamepadStatusHandler(unsigned nDeviceIndex, const TGamePadState *pState) {
    if (!s_pThis) return;
    InputState* state = &s_pThis->m_InputState;
    if (nDeviceIndex < 2) {
        uint32_t buttons = pState->buttons;
        uint32_t mapped = 0;

        // Map Circle button bits to normalized emulator button bits
        if (buttons & GamePadButtonUp)     mapped |= 1U << 0;
        if (buttons & GamePadButtonDown)   mapped |= 1U << 1;
        if (buttons & GamePadButtonLeft)   mapped |= 1U << 2;
        if (buttons & GamePadButtonRight)  mapped |= 1U << 3;
        if (buttons & GamePadButtonA)      mapped |= 1U << 4;
        if (buttons & GamePadButtonB)      mapped |= 1U << 5;
        if (buttons & GamePadButtonStart)  mapped |= 1U << 6;
        if (buttons & GamePadButtonSelect) mapped |= 1U << 7;

        // Start with analog stick values
        int16_t lx = (int16_t)pState->axes[GamePadAxisLeftX].value;
        int16_t ly = (int16_t)pState->axes[GamePadAxisLeftY].value;

        // Map D-Pad to left analog stick axes for digital movement
        if (buttons & GamePadButtonLeft)  lx = -32768;
        if (buttons & GamePadButtonRight) lx = 32767;
        if (buttons & GamePadButtonUp)    ly = -32768;
        if (buttons & GamePadButtonDown)  ly = 32767;

        state->gamepad_buttons[nDeviceIndex] = mapped;
        state->gamepad_axis[nDeviceIndex][0] = lx;
        state->gamepad_axis[nDeviceIndex][1] = ly;
        state->gamepad_axis[nDeviceIndex][2] = (int16_t)pState->axes[GamePadAxisRightX].value;
        state->gamepad_axis[nDeviceIndex][3] = (int16_t)pState->axes[GamePadAxisRightY].value;
    }
}

void CKernel::GamepadRemovedHandler(CDevice *pDevice, void *pContext) {
    CKernel* pThis = (CKernel*)pContext;
    if (!pThis) return;

    for (unsigned i = 0; i < 2; ++i) {
        if (pThis->m_pGamePad[i] == (CUSBGamePadDevice*)pDevice) {
            pThis->m_pGamePad[i] = 0;
            pThis->m_InputState.gamepad_buttons[i] = 0;
            memset(pThis->m_InputState.gamepad_axis[i], 0,
                   sizeof(pThis->m_InputState.gamepad_axis[i]));
            break;
        }
    }
}
