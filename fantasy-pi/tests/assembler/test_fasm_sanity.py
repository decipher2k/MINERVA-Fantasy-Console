#!/usr/bin/env python3
"""Quick sanity test for the assembler."""
import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'assembler', 'src'))

from fasm import Assembler, AssemblerError

def test_basic():
    src = """
start:
    load r0, #10
    load r1, #20
    add r2, r0, r1
    jmp start
    trap TRAP_HALT
"""
    asm = Assembler()
    binary = asm.assemble(src)
    assert len(binary) == 20  # 5 instructions * 4 bytes
    # Check first instruction encoding: load r0, #10
    import struct
    word = struct.unpack('<I', binary[0:4])[0]
    assert (word >> 26) & 0x3F == 0x01  # load opcode
    assert (word >> 18) & 0x0F == 0     # dst = r0
    print("test_basic PASSED")

def test_forward_label():
    src = """
    jmp target
    nop
    nop
target:
    trap TRAP_HALT
"""
    asm = Assembler()
    binary = asm.assemble(src)
    # jmp target should resolve to offset 12 (fourth instruction)
    import struct
    word = struct.unpack('<I', binary[0:4])[0]
    addr = word & 0x3FFFFF
    assert addr == 12, f"Expected 12, got {addr}"
    print("test_forward_label PASSED")

def test_data():
    src = """
    load r0, #0
my_data:
    .word 0x12345678
    .byte 0xAB, 0xCD
"""
    asm = Assembler()
    binary = asm.assemble(src)
    import struct
    word = struct.unpack('<I', binary[4:8])[0]
    assert word == 0x12345678
    assert binary[8] == 0xAB
    assert binary[9] == 0xCD
    print("test_data PASSED")

if __name__ == '__main__':
    test_basic()
    test_forward_label()
    test_data()
    print("All assembler sanity tests passed!")
