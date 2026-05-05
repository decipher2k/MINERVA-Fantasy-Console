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
#include "fantasy_vm.h"

using namespace fantasy;

// Helper to encode instructions in little-endian
static inline void emit(uint8_t* buf, uint32_t instr) {
    buf[0] = instr & 0xFF;
    buf[1] = (instr >> 8) & 0xFF;
    buf[2] = (instr >> 16) & 0xFF;
    buf[3] = (instr >> 24) & 0xFF;
}

void test_load_store() {
    FantasyVM vm;
    uint8_t code[32];
    memset(code, 0, sizeof(code));

    // load r0, #42  -> opcode 0x01, dst encoded in bits 25..22, imm22=42
    uint32_t instr_load = (0x01 << 26) | (0 << 22) | (42 & 0x3FFFFF);
    emit(code + 0, instr_load);

    // st r0, [r0, #0] -> opcode 0x11, srcA=0, srcB=0, imm10=0
    uint32_t instr_st = (0x11 << 26) | (0 << 22) | (0 << 18) | (0 << 14) | (0 << 10) | 0;
    emit(code + 4, instr_st);

    // ld r1, [r0, #0] -> opcode 0x10, dst=1, srcA=0, imm10=0
    uint32_t instr_ld = (0x10 << 26) | (0 << 22) | (1 << 18) | (0 << 14) | 0;
    emit(code + 8, instr_ld);

    // trap HALT -> opcode 0x1F, imm22=0
    uint32_t instr_halt = (0x1F << 26);
    emit(code + 12, instr_halt);

    vm.LoadCartridge(code, 16);
    vm.Step();
    assert(vm.GetState().regs[0] == 42);
    vm.Step();
    vm.Step();
    assert(vm.GetState().regs[1] == 42);
    vm.Step();
    assert(vm.IsHalted());
    printf("test_load_store PASSED\n");
}

void test_load_register_encoding() {
    FantasyVM vm;
    uint8_t code[16];
    memset(code, 0, sizeof(code));

    emit(code + 0, (0x01 << 26) | (1 << 22) | (480 & 0x3FFFFF)); // load r1, #480
    emit(code + 4, (0x01 << 26) | (13 << 22) | (0x000FFFFC & 0x3FFFFF)); // load sp, #0x000FFFFC
    emit(code + 8, (0x01 << 26) | (2 << 22) | ((uint32_t)-12 & 0x3FFFFF)); // load r2, #-12
    emit(code + 12, (0x1F << 26)); // trap HALT

    vm.LoadCartridge(code, sizeof(code));
    vm.Step();
    assert(vm.GetState().regs[1] == 480);
    vm.Step();
    assert(vm.GetState().regs[13] == 0x000FFFFC);
    vm.Step();
    assert((int32_t)vm.GetState().regs[2] == -12);
    vm.Step();
    assert(vm.IsHalted());
    printf("test_load_register_encoding PASSED\n");
}

void test_arithmetic() {
    FantasyVM vm;
    uint8_t code[32];
    memset(code, 0, sizeof(code));

    // load r0, #10
    emit(code + 0, (0x01 << 26) | (0 << 22) | (10 & 0x3FFFFF));
    // load r1, #20
    emit(code + 4, (0x01 << 26) | (1 << 22) | (20 & 0x3FFFFF));
    // add r2, r0, r1 -> opcode 0x03, dst=2, srcA=0, srcB=1
    emit(code + 8, (0x03 << 26) | (0 << 22) | (2 << 18) | (0 << 14) | (1 << 10));
    // trap HALT
    emit(code + 12, (0x1F << 26));

    vm.LoadCartridge(code, 16);
    vm.Step(); // load r0
    vm.Step(); // load r1
    vm.Step(); // add
    assert(vm.GetState().regs[2] == 30);
    printf("test_arithmetic PASSED\n");
}

void test_immediate_arithmetic_and_cmp() {
    FantasyVM vm;
    uint8_t code[32];
    memset(code, 0, sizeof(code));

    emit(code + 0, (0x01 << 26) | (1 << 22) | (20 & 0x3FFFFF)); // load r1, #20
    emit(code + 4, (0x03 << 26) | (2 << 18) | (1 << 14) | (15 << 10) | 8); // add r2, r1, #8
    emit(code + 8, (0x04 << 26) | (3 << 18) | (2 << 14) | (15 << 10) | ((uint32_t)-4 & 0x3FF)); // sub r3, r2, #-4
    emit(code + 12, (0x0F << 26) | (0 << 18) | (3 << 14) | (15 << 10) | 32); // cmp r3, #32
    emit(code + 16, (0x19 << 26) | (24 & 0x3FFFFF)); // jeq success
    emit(code + 20, (0x1F << 26)); // trap HALT
    emit(code + 24, (0x01 << 26) | (4 << 22) | (1 & 0x3FFFFF)); // success: load r4, #1
    emit(code + 28, (0x1F << 26)); // trap HALT

    vm.LoadCartridge(code, sizeof(code));
    while (!vm.IsHalted()) vm.Step();
    assert(vm.GetState().regs[2] == 28);
    assert(vm.GetState().regs[3] == 32);
    assert(vm.GetState().regs[4] == 1);
    printf("test_immediate_arithmetic_and_cmp PASSED\n");
}

void test_trap_halt() {
    FantasyVM vm;
    uint8_t code[8];
    memset(code, 0, sizeof(code));
    emit(code, (0x1F << 26)); // trap HALT
    vm.LoadCartridge(code, 4);
    vm.Step();
    assert(vm.IsHalted());
    printf("test_trap_halt PASSED\n");
}

void test_trap_callback() {
    FantasyVM vm;
    bool called = false;
    uint32_t trap_vector = 0xFF;

    auto cb = [](uint32_t vector, uint32_t* regs, void* data) -> bool {
        bool* p_called = (bool*)data;
        *p_called = true;
        regs[0] = 123;
        return true;
    };

    vm.SetTrapCallback(cb, &called);

    uint8_t code[8];
    memset(code, 0, sizeof(code));
    // trap 0xFF
    emit(code, (0x1F << 26) | (0xFF & 0x3FFFFF));
    vm.LoadCartridge(code, 4);
    vm.Step();

    assert(called);
    assert(vm.GetState().regs[0] == 123);
    assert(!vm.IsHalted());
    printf("test_trap_callback PASSED\n");
}

int main() {
    printf("Running VM tests...\n");
    test_load_store();
    test_load_register_encoding();
    test_arithmetic();
    test_immediate_arithmetic_and_cmp();
    test_trap_halt();
    test_trap_callback();
    printf("All VM tests passed!\n");
    return 0;
}
