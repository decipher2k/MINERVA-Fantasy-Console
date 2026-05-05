#!/usr/bin/env python3
# Copyright 2026 Dennis Michael Heine
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.

"""
fasm.py - Fantasy Assembler

Assembles .fasm source files into binary object code or ROM images.
Supports:
- Labels and local labels
- .section directives
- .word, .byte, .half, .asciiz data directives
- Basic instructions per ISA.md
"""

import sys
import re
import struct
import argparse
from typing import Dict, List, Tuple, Optional

# Opcode table
OPCODES = {
    'nop': 0x00, 'load': 0x01, 'mov': 0x02,
    'add': 0x03, 'sub': 0x04, 'mul': 0x05,
    'div': 0x06, 'mod': 0x07, 'and': 0x08,
    'or': 0x09, 'xor': 0x0A, 'not': 0x0B,
    'shl': 0x0C, 'shr': 0x0D, 'sar': 0x0E,
    'cmp': 0x0F, 'ld': 0x10, 'st': 0x11,
    'ldb': 0x12, 'stb': 0x13, 'ldh': 0x14,
    'sth': 0x15, 'push': 0x16, 'pop': 0x17,
    'jmp': 0x18, 'jeq': 0x19, 'jne': 0x1A,
    'jgt': 0x1B, 'jlt': 0x1C, 'call': 0x1D,
    'ret': 0x1E, 'trap': 0x1F,
}

CONDITIONS = {
    'al': 0x0, 'eq': 0x1, 'ne': 0x2, 'gt': 0x3,
    'lt': 0x4, 'ge': 0x5, 'le': 0x6, 'cs': 0x7,
    'cc': 0x8, 'mi': 0x9, 'pl': 0xA, 'vs': 0xB,
    'vc': 0xC,
}

TRAPS = {
    'TRAP_HALT': 0x00, 'TRAP_SLEEP': 0x01,
    'GFX_INIT': 0x10, 'GFX_PRESENT': 0x11, 'GFX_CLEAR': 0x12,
    'GFX_DRAW_SPRITE': 0x13, 'GFX_DRAW_IMAGE': 0x14,
    'GFX_BITBLT': 0x15, 'GFX_DRAW_TILEMAP': 0x16,
    'GFX_SET_PALETTE': 0x17, 'GFX_DRAW_TEXT': 0x18,
    'AUDIO_INIT': 0x20, 'AUDIO_PLAY': 0x21, 'AUDIO_STOP': 0x22,
    'INPUT_POLL': 0x30, 'INPUT_KEY': 0x31, 'INPUT_GAMEPAD': 0x32,
    'MEM_COPY': 0x40, 'MEM_FILL': 0x41,
    'MATH_RAND': 0x50, 'MATH_SIN': 0x51, 'MATH_COS': 0x52,
    'DEBUG_LOG': 0x60, 'DEBUG_LOG_STR': 0x61,
}

BLEND_MODES = {
    'BLEND_COPY': 0, 'BLEND_ALPHA': 1, 'BLEND_ADDITIVE': 2,
    'BLEND_MULTIPLY': 3, 'BLEND_COLORKEY': 4, 'BLEND_MASK': 5,
}


class AssemblerError(Exception):
    pass


class Assembler:
    def __init__(self):
        self.labels: Dict[str, int] = {}
        self.fixups: List[Tuple[int, str, int]] = []  # (offset, label, instr_line)
        self.output = bytearray()
        self.line_num = 0
        self.current_section = 'code'
        self.sections: Dict[str, int] = {'code': 0}

    def parse_register(self, tok: str) -> int:
        tok = tok.lower().strip(',')
        if tok.startswith('r') and tok[1:].isdigit():
            return int(tok[1:])
        if tok == 'sp': return 13
        if tok == 'lr': return 14
        if tok == 'pc': return 15
        raise AssemblerError(f"Unknown register: {tok}")

    def parse_imm(self, tok: str) -> int:
        tok = tok.strip(',').replace('#', '')
        if tok in self.labels:
            return self.labels[tok]
        if tok in TRAPS:
            return TRAPS[tok]
        if tok in BLEND_MODES:
            return BLEND_MODES[tok]
        if tok.startswith('0x') or tok.startswith('0X'):
            return int(tok, 16)
        if tok.startswith('0b'):
            return int(tok, 2)
        return int(tok)

    def encode_instr(self, opcode: int, cond: int, dst: int, srca: int, srcb: int, imm: int) -> int:
        return ((opcode & 0x3F) << 26) | ((cond & 0x0F) << 22) | ((dst & 0x0F) << 18) | \
               ((srca & 0x0F) << 14) | ((srcb & 0x0F) << 10) | (imm & 0x3FF)

    def encode_jmp(self, opcode: int, cond: int, addr: int) -> int:
        return ((opcode & 0x3F) << 26) | ((cond & 0x0F) << 22) | (addr & 0x3FFFFF)

    def encode_load(self, dst: int, imm: int) -> int:
        # load uses the condition field as destination register, leaving
        # the low 22 bits intact for a signed immediate.
        return ((OPCODES['load'] & 0x3F) << 26) | ((dst & 0x0F) << 22) | (imm & 0x3FFFFF)

    def emit_word(self, word: int):
        self.output += struct.pack('<I', word & 0xFFFFFFFF)

    def process_line(self, line: str):
        self.line_num += 1
        line = line.split(';')[0].strip()
        if not line:
            return

        # Labels
        if ':' in line and not line.startswith('.'):
            label, rest = line.split(':', 1)
            label = label.strip()
            self.labels[label] = len(self.output)
            line = rest.strip()
            if not line:
                return

        # Directives
        if line.startswith('.'):
            parts = line.split(None, 1)
            directive = parts[0].lower()
            arg = parts[1] if len(parts) > 1 else ''

            if directive == '.section':
                self.current_section = arg.strip()
                if self.current_section not in self.sections:
                    self.sections[self.current_section] = len(self.output)
            elif directive == '.equ':
                name, val = arg.split(',', 1)
                self.labels[name.strip()] = self.parse_imm(val.strip())
            elif directive == '.word':
                for tok in arg.split(','):
                    tok = tok.strip()
                    if tok in self.labels:
                        self.emit_word(self.labels[tok])
                    else:
                        try:
                            self.emit_word(self.parse_imm(tok))
                        except ValueError:
                            self.fixups.append((len(self.output), tok, self.line_num))
                            self.emit_word(0)
            elif directive == '.byte':
                for tok in arg.split(','):
                    self.output.append(self.parse_imm(tok.strip()) & 0xFF)
            elif directive == '.half':
                for tok in arg.split(','):
                    self.output += struct.pack('<H', self.parse_imm(tok.strip()) & 0xFFFF)
            elif directive == '.asciiz':
                s = arg.strip().strip('"')
                s = s.replace('\\n', '\n').replace('\\t', '\t').replace('\\0', '\0')
                self.output += s.encode('utf-8') + b'\x00'
            elif directive == '.align':
                align = self.parse_imm(arg.strip())
                while len(self.output) % align:
                    self.output.append(0)
            else:
                raise AssemblerError(f"Unknown directive: {directive}")
            return

        # Instructions
        parts = line.split()
        if not parts:
            return

        mnemonic = parts[0].lower()
        cond = 0x0
        if '.' in mnemonic:
            mnemonic, cond_str = mnemonic.split('.', 1)
            cond = CONDITIONS.get(cond_str, 0x0)

        opcode = OPCODES.get(mnemonic)
        if opcode is None:
            raise AssemblerError(f"Unknown instruction: {mnemonic}")

        args = parts[1:]

        if mnemonic in ('nop', 'ret'):
            self.emit_word(self.encode_instr(opcode, cond, 0, 0, 0, 0))
        elif mnemonic == 'load':
            dst = self.parse_register(args[0])
            imm = self.parse_imm(args[1])
            if imm > 0x1FFFFF or imm < -0x200000:
                raise AssemblerError(f"Immediate out of range: {imm}")
            if cond != 0:
                raise AssemblerError("Conditional load is not supported")
            self.emit_word(self.encode_load(dst, imm))
        elif mnemonic == 'trap':
            imm = self.parse_imm(args[0])
            self.emit_word(self.encode_jmp(opcode, cond, imm))
        elif mnemonic in ('jmp', 'jeq', 'jne', 'jgt', 'jlt', 'call'):
            target = args[0]
            if target in self.labels:
                addr = self.labels[target]
                self.emit_word(self.encode_jmp(opcode, cond, addr))
            else:
                self.fixups.append((len(self.output), target, self.line_num))
                self.emit_word(self.encode_jmp(opcode, cond, 0))
        elif mnemonic in ('push', 'pop'):
            dst = self.parse_register(args[0])
            self.emit_word(self.encode_instr(opcode, cond, dst, 0, 0, 0))
        elif mnemonic == 'not':
            dst = self.parse_register(args[0])
            srca = self.parse_register(args[1]) if len(args) > 1 else 0
            self.emit_word(self.encode_instr(opcode, cond, dst, srca, 0, 0))
        elif mnemonic in ('ld', 'st', 'ldb', 'stb', 'ldh', 'sth'):
            dst = self.parse_register(args[0])
            # Parse [reg, #imm]
            mem = ' '.join(args[1:]).strip()
            mem = mem.strip('[]')
            mem_parts = [p.strip() for p in mem.split(',')]
            base = self.parse_register(mem_parts[0])
            off = self.parse_imm(mem_parts[1]) if len(mem_parts) > 1 else 0
            if mnemonic in ('ld', 'ldb', 'ldh'):
                self.emit_word(self.encode_instr(opcode, cond, dst, base, 0, off & 0x3FF))
            else:
                self.emit_word(self.encode_instr(opcode, cond, 0, base, dst, off & 0x3FF))
        elif mnemonic == 'cmp':
            srca = self.parse_register(args[0])
            if args[1].startswith('#'):
                imm = self.parse_imm(args[1])
                self.emit_word(self.encode_instr(opcode, cond, 0, srca, 15, imm & 0x3FF))
            else:
                srcb = self.parse_register(args[1])
                self.emit_word(self.encode_instr(opcode, cond, 0, srca, srcb, 0))
        else:
            dst = self.parse_register(args[0])
            srca = self.parse_register(args[1])
            if args[2].startswith('#'):
                srcb = 15
                imm = self.parse_imm(args[2])
                self.emit_word(self.encode_instr(opcode, cond, dst, srca, srcb, imm & 0x3FF))
            else:
                srcb = self.parse_register(args[2])
                self.emit_word(self.encode_instr(opcode, cond, dst, srca, srcb, 0))

    def assemble(self, source: str) -> bytes:
        self.labels = {}
        self.line_num = 0

        # First pass: collect labels and build preliminary output
        self.output = bytearray()
        self.fixups = []
        for line in source.splitlines():
            self.process_line(line)

        # Second pass: rebuild with known labels, resolve most fixups
        self.output = bytearray()
        self.fixups = []
        self.line_num = 0
        for line in source.splitlines():
            self.process_line(line)

        # Apply remaining fixups (should only be unresolved forward refs or external)
        for offset, label, line_num in self.fixups:
            if label not in self.labels:
                raise AssemblerError(f"Undefined label '{label}' at line {line_num}")
            addr = self.labels[label]
            word = struct.unpack('<I', self.output[offset:offset+4])[0]
            word = (word & 0xFFC00000) | (addr & 0x3FFFFF)
            self.output[offset:offset+4] = struct.pack('<I', word)

        return bytes(self.output)


def main():
    parser = argparse.ArgumentParser(description='Fantasy Assembler')
    parser.add_argument('input', help='Input .fasm file')
    parser.add_argument('-o', '--output', required=True, help='Output binary file')
    args = parser.parse_args()

    with open(args.input, 'r') as f:
        source = f.read()

    asm = Assembler()
    try:
        binary = asm.assemble(source)
        with open(args.output, 'wb') as f:
            f.write(binary)
        print(f"Assembled {len(source.splitlines())} lines -> {len(binary)} bytes")
    except AssemblerError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
