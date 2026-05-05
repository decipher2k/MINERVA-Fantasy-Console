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

constexpr uint32_t RAM_SIZE = 16 * 1024 * 1024;    // 16 MB
constexpr uint32_t VRAM_SIZE = 8 * 1024 * 1024;    // 8 MB
constexpr uint32_t ROM_MAX_SIZE = 32 * 1024 * 1024; // 32 MB
constexpr uint32_t ROM_HEADER_SIZE = 256;          // ROM header bytes
constexpr uint32_t ROM_BASE = 0x02000000;          // ROM mapped address for ld
constexpr uint32_t REG_COUNT = 16;
constexpr uint32_t STACK_TOP = 0x000FFFFC;

enum class TrapVector : uint32_t {
    HALT = 0x00,
    SLEEP = 0x01,
    GFX_INIT = 0x10,
    GFX_PRESENT = 0x11,
    GFX_CLEAR = 0x12,
    GFX_DRAW_SPRITE = 0x13,
    GFX_DRAW_IMAGE = 0x14,
    GFX_BITBLT = 0x15,
    GFX_DRAW_TILEMAP = 0x16,
    GFX_SET_PALETTE = 0x17,
    GFX_DRAW_TEXT = 0x18,
    AUDIO_INIT = 0x20,
    AUDIO_PLAY = 0x21,
    AUDIO_STOP = 0x22,
    INPUT_POLL = 0x30,
    INPUT_KEY = 0x31,
    INPUT_GAMEPAD = 0x32,
    MEM_COPY = 0x40,
    MEM_FILL = 0x41,
    MATH_RAND = 0x50,
    MATH_SIN = 0x51,
    MATH_COS = 0x52,
    DEBUG_LOG = 0x60,
    DEBUG_LOG_STR = 0x61,
};

struct VMState {
    uint32_t regs[REG_COUNT];
    uint32_t flags;
    bool halted;
    uint64_t cycles;
};

// Trap callback signature: returns true if trap was handled, false otherwise
using TrapCallback = bool(*)(uint32_t vector, uint32_t* regs, void* user_data);

class FantasyVM {
public:
    FantasyVM();
    ~FantasyVM();

    // Cartridge loading
    void LoadCartridge(const uint8_t* data, size_t size);
    void Reset();

    // Execution
    void Step();            // Execute one instruction
    void RunFrame();        // Run until GFX_PRESENT or HALT
    bool IsHalted() const;
    bool IsPresentRequested() const { return present_requested_; }
    void ClearPresentRequest() { present_requested_ = false; }

    // Trap callback registration
    void SetTrapCallback(TrapCallback cb, void* user_data = nullptr);

    // Memory access (for traps / debugging)
    uint8_t* GetRAM() { return ram_; }
    uint8_t* GetVRAM() { return vram_; }
    const uint8_t* GetROM() const { return rom_; }
    size_t GetROMSize() const { return rom_size_; }
    VMState& GetState() { return state_; }

    // Debug
    void SetTrace(bool enable) { trace_ = enable; }
    void DumpState();

private:
    VMState state_;
    uint8_t* ram_;
    uint8_t* vram_;
    uint8_t* rom_;
    size_t rom_size_;
    bool trace_;
    bool present_requested_;
    TrapCallback trap_cb_;
    void* trap_user_data_;

    // Instruction decode helpers
    uint32_t Fetch();
    void Execute(uint32_t instr);
    void UpdateFlags(uint64_t result, uint32_t opA, uint32_t opB, bool is_sub);
    uint32_t GetReg(uint32_t idx) const;
    void SetReg(uint32_t idx, uint32_t value);
    bool CheckCondition(uint32_t cond) const;

    // Trap handlers
    void HandleTrap(uint32_t vector);
};

} // namespace fantasy
