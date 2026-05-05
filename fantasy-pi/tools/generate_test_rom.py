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
generate_test_rom.py - Create a minimal test ROM without needing real assets.

Usage:
    python3 tools/generate_test_rom.py examples/hello_world/src/main.fasm -o test.rom
"""

import sys
import os
import struct
import argparse

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'assembler', 'src'))
from fasm import Assembler, AssemblerError

def create_minimal_rom(asm_code: str) -> bytes:
    """Assemble code and prepend a minimal ROM header."""
    asm = Assembler()
    binary = asm.assemble(asm_code)

    # ROM Header
    header = bytearray(256)
    header[0:4] = b'FPVM'
    header[4:8] = struct.pack('<I', 0x00010000)  # Version
    header[8:12] = struct.pack('<I', len(binary))  # Code size
    header[12:16] = struct.pack('<I', 0)  # Entry point offset
    header[16:20] = struct.pack('<I', 0)  # Asset count
    header[20:24] = struct.pack('<I', 0)  # Asset dir offset
    header[24:28] = struct.pack('<I', 0)  # Flags

    return bytes(header) + binary


def main():
    parser = argparse.ArgumentParser(description='Generate a test ROM')
    parser.add_argument('source', help='Input .fasm file')
    parser.add_argument('-o', '--output', required=True, help='Output ROM file')
    args = parser.parse_args()

    with open(args.source, 'r') as f:
        source = f.read()

    try:
        rom = create_minimal_rom(source)
        with open(args.output, 'wb') as f:
            f.write(rom)
        print(f"Created ROM: {args.output} ({len(rom)} bytes)")
    except AssemblerError as e:
        print(f"Assembly error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
