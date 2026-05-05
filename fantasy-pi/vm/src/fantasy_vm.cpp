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

#include "fantasy_vm.h"
#include <cstring>
#include <cstdio>

namespace fantasy {

// Flag bits
constexpr uint32_t FLAG_Z = 1 << 0;
constexpr uint32_t FLAG_N = 1 << 1;
constexpr uint32_t FLAG_C = 1 << 2;
constexpr uint32_t FLAG_V = 1 << 3;

// Instruction field extraction
constexpr uint32_t OPCODE(uint32_t i) { return (i >> 26) & 0x3F; }
constexpr uint32_t COND(uint32_t i)  { return (i >> 22) & 0x0F; }
constexpr uint32_t DST(uint32_t i)   { return (i >> 18) & 0x0F; }
constexpr uint32_t SRCA(uint32_t i)  { return (i >> 14) & 0x0F; }
constexpr uint32_t SRCB(uint32_t i)  { return (i >> 10) & 0x0F; }
constexpr uint32_t IMM10(uint32_t i) { return i & 0x03FF; }
constexpr uint32_t IMM22(uint32_t i) { return i & 0x3FFFFF; }
constexpr int32_t SIMM10(uint32_t i) {
    uint32_t v = IMM10(i);
    return (v & 0x200) ? (v | 0xFFFFFC00) : v; // sign extend 10-bit
}
constexpr int32_t SIMM22(uint32_t i) {
    uint32_t v = IMM22(i);
    return (v & 0x200000) ? (v | 0xFFC00000) : v; // sign extend 22-bit
}

FantasyVM::FantasyVM()
    : ram_(new uint8_t[RAM_SIZE]),
      vram_(new uint8_t[VRAM_SIZE]),
      rom_(nullptr),
      rom_size_(0),
      trace_(false),
      present_requested_(false),
      trap_cb_(nullptr),
      trap_user_data_(nullptr)
{
    Reset();
}

FantasyVM::~FantasyVM() {
    delete[] ram_;
    delete[] vram_;
    delete[] rom_;
}

void FantasyVM::Reset() {
    memset(&state_, 0, sizeof(state_));
    memset(ram_, 0, RAM_SIZE);
    memset(vram_, 0, VRAM_SIZE);
    state_.regs[13] = STACK_TOP; // SP
    state_.halted = false;
    state_.cycles = 0;
    present_requested_ = false;
}

void FantasyVM::LoadCartridge(const uint8_t* data, size_t size) {
    if (size > ROM_MAX_SIZE) size = ROM_MAX_SIZE;
    delete[] rom_;
    rom_ = new uint8_t[ROM_MAX_SIZE];
    memcpy(rom_, data, size);
    rom_size_ = size;
    state_.regs[15] = 0; // PC starts at first instruction

    // Mirror code+data section into RAM so ld/st can access it
    if (size > ROM_HEADER_SIZE) {
        size_t code_size = size - ROM_HEADER_SIZE;
        if (code_size > RAM_SIZE) code_size = RAM_SIZE;
        memcpy(ram_, rom_ + ROM_HEADER_SIZE, code_size);
    } else {
        // Raw code without header: mirror directly to RAM
        size_t code_size = size;
        if (code_size > RAM_SIZE) code_size = RAM_SIZE;
        memcpy(ram_, rom_, code_size);
    }
}

uint32_t FantasyVM::Fetch() {
    uint32_t pc = state_.regs[15];
    uint32_t rom_addr = (rom_size_ >= ROM_HEADER_SIZE) ? (ROM_HEADER_SIZE + pc) : pc;
    if (rom_addr + 4 > rom_size_) {
        state_.halted = true;
        return 0; // nop
    }
    uint32_t instr = rom_[rom_addr] | (rom_[rom_addr+1] << 8) | (rom_[rom_addr+2] << 16) | (rom_[rom_addr+3] << 24);
    state_.regs[15] += 4;
    return instr;
}

uint32_t FantasyVM::GetReg(uint32_t idx) const {
    if (idx == 15) return state_.regs[15] + 4; // PC reads as current + 4
    return state_.regs[idx];
}

void FantasyVM::SetReg(uint32_t idx, uint32_t value) {
    state_.regs[idx] = value;
}

void FantasyVM::UpdateFlags(uint64_t result, uint32_t opA, uint32_t opB, bool is_sub) {
    state_.flags = 0;
    if ((result & 0xFFFFFFFF) == 0) state_.flags |= FLAG_Z;
    if (result & 0x80000000) state_.flags |= FLAG_N;
    if (result & 0x100000000ULL) state_.flags |= FLAG_C;
    // Overflow (V): signed overflow detection
    if (is_sub) {
        // Subtraction: overflow if operands have different signs and result has same sign as subtrahend (opB)
        if (((opA ^ opB) & 0x80000000) && ((opA ^ (uint32_t)result) & 0x80000000)) {
            state_.flags |= FLAG_V;
        }
    } else {
        // Addition: overflow if operands have same sign and result has different sign
        if (((opA ^ opB) & 0x80000000) == 0 && ((opA ^ (uint32_t)result) & 0x80000000)) {
            state_.flags |= FLAG_V;
        }
    }
}

bool FantasyVM::CheckCondition(uint32_t cond) const {
    switch (cond) {
        case 0x0: return true;
        case 0x1: return (state_.flags & FLAG_Z) != 0;
        case 0x2: return (state_.flags & FLAG_Z) == 0;
        case 0x3: return (state_.flags & FLAG_N) == 0 && (state_.flags & FLAG_Z) == 0;
        case 0x4: return (state_.flags & FLAG_N) != 0;
        case 0x5: return (state_.flags & FLAG_N) == 0;
        case 0x6: return (state_.flags & FLAG_Z) != 0 || (state_.flags & FLAG_N) != 0;
        case 0x7: return (state_.flags & FLAG_C) != 0;
        case 0x8: return (state_.flags & FLAG_C) == 0;
        case 0x9: return (state_.flags & FLAG_N) != 0;
        case 0xA: return (state_.flags & FLAG_N) == 0;
        case 0xB: return (state_.flags & FLAG_V) != 0;
        case 0xC: return (state_.flags & FLAG_V) == 0;
        default: return true;
    }
}

void FantasyVM::Step() {
    if (state_.halted) return;

    uint32_t instr = Fetch();
    state_.cycles++;

    uint32_t opcode = OPCODE(instr);
    uint32_t cond = COND(instr);

    uint32_t dst = DST(instr);
    uint32_t srcA = SRCA(instr);
    uint32_t srcB = SRCB(instr);
    int32_t simm10 = SIMM10(instr);
    uint32_t imm10 = IMM10(instr);
    int32_t simm22 = SIMM22(instr);
    uint32_t imm22 = IMM22(instr);

    // load uses the condition field as destination register so it can keep a
    // full signed 22-bit immediate. Other instructions use cond normally.
    if (opcode != 0x01 && !CheckCondition(cond)) return;

    uint32_t a = GetReg(srcA);
    uint32_t b = (srcB == 15) ? static_cast<uint32_t>(simm10) : GetReg(srcB);
    uint64_t result = 0;

    switch (opcode) {
        case 0x00: // nop
            break;
        case 0x01: // load
            SetReg(cond, static_cast<uint32_t>(simm22));
            break;
        case 0x02: // mov
            SetReg(dst, a);
            break;
        case 0x03: // add
            result = (uint64_t)a + b;
            SetReg(dst, (uint32_t)result);
            UpdateFlags(result, a, b, false);
            break;
        case 0x04: // sub
            result = (uint64_t)a - b;
            SetReg(dst, (uint32_t)result);
            UpdateFlags(result, a, b, true);
            break;
        case 0x05: // mul
            result = (uint64_t)a * b;
            SetReg(dst, (uint32_t)result);
            UpdateFlags(result, a, b, false);
            break;
        case 0x06: // div
            if (b != 0) SetReg(dst, a / b);
            else SetReg(dst, 0);
            break;
        case 0x07: // mod
            if (b != 0) SetReg(dst, a % b);
            else SetReg(dst, 0);
            break;
        case 0x08: // and
            result = a & b;
            SetReg(dst, (uint32_t)result);
            UpdateFlags(result, a, b, false);
            break;
        case 0x09: // or
            result = a | b;
            SetReg(dst, (uint32_t)result);
            UpdateFlags(result, a, b, false);
            break;
        case 0x0A: // xor
            result = a ^ b;
            SetReg(dst, (uint32_t)result);
            UpdateFlags(result, a, b, false);
            break;
        case 0x0B: // not
            result = ~a;
            SetReg(dst, (uint32_t)result);
            UpdateFlags(result, a, 0, false);
            break;
        case 0x0C: // shl
            result = a << (b & 0x1F);
            SetReg(dst, (uint32_t)result);
            UpdateFlags(result, a, b, false);
            break;
        case 0x0D: // shr
            result = a >> (b & 0x1F);
            SetReg(dst, (uint32_t)result);
            UpdateFlags(result, a, b, false);
            break;
        case 0x0E: // sar
            result = (uint32_t)(((int32_t)a) >> (b & 0x1F));
            SetReg(dst, (uint32_t)result);
            UpdateFlags(result, a, b, false);
            break;
        case 0x0F: // cmp
            result = (uint64_t)a - b;
            UpdateFlags(result, a, b, true);
            break;
        case 0x10: { // ld
            uint32_t addr = a + simm10;
            if (addr < RAM_SIZE) {
                uint32_t val = ram_[addr] | (ram_[addr+1] << 8) | (ram_[addr+2] << 16) | (ram_[addr+3] << 24);
                SetReg(dst, val);
            } else if (addr >= ROM_BASE && addr - ROM_BASE + 3 < rom_size_) {
                uint32_t rom_addr = addr - ROM_BASE;
                uint32_t val = rom_[rom_addr] | (rom_[rom_addr+1] << 8) | (rom_[rom_addr+2] << 16) | (rom_[rom_addr+3] << 24);
                SetReg(dst, val);
            }
            break;
        }
        case 0x11: { // st
            uint32_t addr = a + simm10;
            if (addr + 3 < RAM_SIZE) {
                uint32_t val = b;
                ram_[addr]   = val & 0xFF;
                ram_[addr+1] = (val >> 8) & 0xFF;
                ram_[addr+2] = (val >> 16) & 0xFF;
                ram_[addr+3] = (val >> 24) & 0xFF;
            }
            break;
        }
        case 0x12: { // ldb
            uint32_t addr = a + simm10;
            if (addr < RAM_SIZE) {
                SetReg(dst, ram_[addr]);
            } else if (addr >= ROM_BASE && addr - ROM_BASE < rom_size_) {
                SetReg(dst, rom_[addr - ROM_BASE]);
            }
            break;
        }
        case 0x13: { // stb
            uint32_t addr = a + simm10;
            if (addr < RAM_SIZE) ram_[addr] = b & 0xFF;
            break;
        }
        case 0x14: { // ldh
            uint32_t addr = a + simm10;
            if (addr + 1 < RAM_SIZE) {
                SetReg(dst, ram_[addr] | (ram_[addr+1] << 8));
            } else if (addr >= ROM_BASE && addr - ROM_BASE + 1 < rom_size_) {
                uint32_t rom_addr = addr - ROM_BASE;
                SetReg(dst, rom_[rom_addr] | (rom_[rom_addr+1] << 8));
            }
            break;
        }
        case 0x15: { // sth
            uint32_t addr = a + simm10;
            if (addr + 1 < RAM_SIZE) {
                ram_[addr] = b & 0xFF;
                ram_[addr+1] = (b >> 8) & 0xFF;
            }
            break;
        }
        case 0x16: { // push
            uint32_t sp = GetReg(13) - 4;
            SetReg(13, sp);
            if (sp + 3 < RAM_SIZE) {
                uint32_t val = GetReg(dst);
                ram_[sp]   = val & 0xFF;
                ram_[sp+1] = (val >> 8) & 0xFF;
                ram_[sp+2] = (val >> 16) & 0xFF;
                ram_[sp+3] = (val >> 24) & 0xFF;
            }
            break;
        }
        case 0x17: { // pop
            uint32_t sp = GetReg(13);
            if (sp + 3 < RAM_SIZE) {
                uint32_t val = ram_[sp] | (ram_[sp+1] << 8) | (ram_[sp+2] << 16) | (ram_[sp+3] << 24);
                SetReg(dst, val);
            }
            SetReg(13, sp + 4);
            break;
        }
        case 0x18: // jmp
            state_.regs[15] = (state_.regs[15] & 0xFFC00000) | (imm22 & 0x3FFFFF);
            break;
        case 0x19: // jeq
            if (state_.flags & FLAG_Z) state_.regs[15] = (state_.regs[15] & 0xFFC00000) | (imm22 & 0x3FFFFF);
            break;
        case 0x1A: // jne
            if (!(state_.flags & FLAG_Z)) state_.regs[15] = (state_.regs[15] & 0xFFC00000) | (imm22 & 0x3FFFFF);
            break;
        case 0x1B: // jgt
            if (!(state_.flags & FLAG_Z) && !(state_.flags & FLAG_N)) state_.regs[15] = (state_.regs[15] & 0xFFC00000) | (imm22 & 0x3FFFFF);
            break;
        case 0x1C: // jlt
            if (state_.flags & FLAG_N) state_.regs[15] = (state_.regs[15] & 0xFFC00000) | (imm22 & 0x3FFFFF);
            break;
        case 0x1D: // call
            SetReg(14, state_.regs[15]);
            state_.regs[15] = (state_.regs[15] & 0xFFC00000) | (imm22 & 0x3FFFFF);
            break;
        case 0x1E: // ret
            state_.regs[15] = GetReg(14);
            break;
        case 0x1F: // trap
            HandleTrap(imm10);
            break;
        default:
            // Unknown opcode = nop
            break;
    }
}

void FantasyVM::SetTrapCallback(TrapCallback cb, void* user_data) {
    trap_cb_ = cb;
    trap_user_data_ = user_data;
}

void FantasyVM::HandleTrap(uint32_t vector) {
    // Always handle HALT internally
    if (vector == 0x00) {
        state_.halted = true;
        return;
    }
    // Delegate to registered callback if available
    if (trap_cb_) {
        bool handled = trap_cb_(vector, state_.regs, trap_user_data_);
        if (handled) {
            if (vector == 0x11) { // GFX_PRESENT
                present_requested_ = true;
            }
            return;
        }
    }
    // Fallback for unhandled traps: SLEEP is safe to ignore in single-step mode.
    // All other unhandled traps are silently ignored to allow forward compatibility.
    switch (vector) {
        case 0x01: // SLEEP
            break;
        default:
            break;
    }
}

void FantasyVM::RunFrame() {
    uint64_t max_cycles = 1000000; // ~1M cycles per frame budget
    uint64_t start_cycles = state_.cycles;
    present_requested_ = false;
    while (!state_.halted && !present_requested_ && state_.cycles - start_cycles < max_cycles) {
        Step();
    }
}

bool FantasyVM::IsHalted() const {
    return state_.halted;
}

void FantasyVM::DumpState() {
    printf("PC: %08X  SP: %08X  LR: %08X  Flags: %08X\n",
           state_.regs[15], state_.regs[13], state_.regs[14], state_.flags);
    for (int i = 0; i < 16; i++) {
        printf("r%02d: %08X ", i, state_.regs[i]);
        if ((i+1) % 4 == 0) printf("\n");
    }
}

} // namespace fantasy
