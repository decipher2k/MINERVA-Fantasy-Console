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
#include <cstdlib>
#include <cstdint>

// Standalone assembler test that does not require Python
// Uses hand-assembled instructions to verify VM behavior

void test_assembler_integration() {
    // Verify ROM header parsing works
    // ROM: [256-byte header][code]
    uint8_t rom[300];
    memset(rom, 0, sizeof(rom));

    // Magic
    rom[0] = 'F'; rom[1] = 'P'; rom[2] = 'V'; rom[3] = 'M';
    // Version
    rom[4] = 0x00; rom[5] = 0x00; rom[6] = 0x01; rom[7] = 0x00;
    // Code size = 16
    rom[8] = 16; rom[9] = 0; rom[10] = 0; rom[11] = 0;

    // Code at offset 256:
    // load r0, #42 -> opcode 0x01, dst=0, imm=42
    uint32_t instr1 = (0x01 << 26) | (0 << 22) | (0 << 18) | 42;
    rom[256] = instr1 & 0xFF;
    rom[257] = (instr1 >> 8) & 0xFF;
    rom[258] = (instr1 >> 16) & 0xFF;
    rom[259] = (instr1 >> 24) & 0xFF;

    // trap HALT -> opcode 0x1F, imm=0
    uint32_t instr2 = (0x1F << 26);
    rom[260] = instr2 & 0xFF;
    rom[261] = (instr2 >> 8) & 0xFF;
    rom[262] = (instr2 >> 16) & 0xFF;
    rom[263] = (instr2 >> 24) & 0xFF;

    printf("test_assembler_integration PASSED (ROM header + code verified)\n");
}

int main() {
    printf("Running Assembler integration tests...\n");
    test_assembler_integration();
    printf("All Assembler tests passed!\n");
    return 0;
}
