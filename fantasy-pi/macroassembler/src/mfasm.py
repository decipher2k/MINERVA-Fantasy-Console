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
mfasm.py - Macro Assembler for Fantasy Pi

Extends fasm with:
- Macros (.macro / .endmacro)
- Includes (.include)
- Asset declarations (.asset)
- Conditional assembly (.if / .else / .endif)
- Repeat blocks (.rept)
- Symbol export/import
- Produces final ROM image with asset table
"""

import sys
import os
import re
import struct
import argparse
import json
import subprocess
import tempfile
from typing import Dict, List, Tuple, Any

# Import base assembler
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(SCRIPT_DIR, '..', '..', 'assembler', 'src'))
sys.path.insert(0, os.path.join(SCRIPT_DIR, '..', '..', 'asset_compiler', 'src'))
from fasm import Assembler, AssemblerError, OPCODES, CONDITIONS, TRAPS, BLEND_MODES


class MacroAssembler:
    def __init__(self, asset_compiler_path: str = None):
        self.asm = Assembler()
        self.macros: Dict[str, Tuple[List[str], List[str]]] = {}  # name -> (params, body)
        self.symbols: Dict[str, Any] = {}
        self.assets: List[Dict[str, Any]] = []
        self.asset_compiler = asset_compiler_path or 'python3 -m asset_compiler.src.assetc'
        self.include_paths = ['.']
        self.if_stack: List[bool] = []
        self.rept_count = 0

    @staticmethod
    def asset_type_code(asset_type: str) -> int:
        return {
            'image': 1,
            'sprite': 2,
            'tileset': 3,
            'tilemap': 4,
            'audio': 5,
            'spritesheet': 2,
            'font': 3,
            'palette': 6,
        }.get(asset_type, 0)

    def expand_macros(self, source: str) -> str:
        lines = source.splitlines()
        output = []
        i = 0
        while i < len(lines):
            line = lines[i]
            stripped = line.strip()

            # Macro definition
            if stripped.startswith('.macro'):
                parts = stripped.split(None, 2)
                name = parts[1].strip()
                params = [p.strip() for p in parts[2].split(',')] if len(parts) > 2 else []
                body = []
                i += 1
                while i < len(lines) and not lines[i].strip().startswith('.endmacro'):
                    body.append(lines[i])
                    i += 1
                self.macros[name] = (params, body)
                i += 1
                continue

            # Macro invocation
            tokens = stripped.split()
            if tokens and tokens[0] in self.macros:
                name = tokens[0]
                params, body = self.macros[name]
                args = [a.strip() for a in ' '.join(tokens[1:]).split(',')] if len(tokens) > 1 else []
                for bline in body:
                    expanded = bline
                    for pi, param in enumerate(params):
                        placeholder = f'\\{param}'
                        replacement = args[pi] if pi < len(args) else ''
                        expanded = expanded.replace(placeholder, replacement)
                    output.append(expanded)
                i += 1
                continue

            # Conditional assembly
            if stripped.startswith('.if'):
                expr = stripped[3:].strip()
                result = self.eval_expr(expr)
                self.if_stack.append(bool(result))
                i += 1
                continue
            if stripped.startswith('.else'):
                if self.if_stack:
                    self.if_stack[-1] = not self.if_stack[-1]
                i += 1
                continue
            if stripped.startswith('.endif'):
                if self.if_stack:
                    self.if_stack.pop()
                i += 1
                continue
            if stripped.startswith('.rept'):
                count = self.eval_expr(stripped[5:].strip())
                block = []
                i += 1
                while i < len(lines) and not lines[i].strip().startswith('.endrept'):
                    block.append(lines[i])
                    i += 1
                for _ in range(count):
                    output.extend(block)
                i += 1
                continue

            # Include
            if stripped.startswith('.include'):
                path = stripped[8:].strip().strip('"')
                inc_source = self.load_include(path)
                output.extend(inc_source.splitlines())
                i += 1
                continue

            # Asset declaration
            if stripped.startswith('.asset'):
                self.parse_asset(stripped)
                i += 1
                continue

            # If inside false .if block, skip
            if self.if_stack and not all(self.if_stack):
                i += 1
                continue

            output.append(line)
            i += 1

        return '\n'.join(output)

    def eval_expr(self, expr: str) -> int:
        expr = expr.strip()
        if expr in self.symbols:
            return self.symbols[expr]
        try:
            # Safe eval with limited context
            return eval(expr, {"__builtins__": {}}, self.symbols)
        except Exception:
            return 0

    def load_include(self, path: str) -> str:
        for base in self.include_paths:
            full = os.path.join(base, path)
            if os.path.exists(full):
                with open(full, 'r') as f:
                    return f.read()
        raise AssemblerError(f"Include not found: {path}")

    def parse_asset(self, line: str):
        # .asset name, "path", type=sprite, format=rgba8888, alpha=premultiplied
        m = re.match(r'\.asset\s+(\w+),\s*"([^"]+)"(?:\s*,\s*(.*))?', line)
        if not m:
            raise AssemblerError(f"Invalid .asset directive: {line}")
        name, path, attrs_str = m.groups()
        attrs = {}
        if attrs_str:
            for part in attrs_str.split(','):
                if '=' in part:
                    k, v = part.split('=', 1)
                    attrs[k.strip()] = v.strip()
        self.assets.append({
            'name': name,
            'path': path,
            'attrs': attrs,
        })

    def build_assets(self, output_dir: str, source_dir: str = '.') -> Dict[str, Tuple[int, int, int]]:
        """Run asset compiler and return mapping name -> (id, offset, size)"""
        if not self.assets:
            return {}

        # Build JSON manifest for asset compiler
        manifest = {'assets': []}
        for asset in self.assets:
            entry = {
                'name': asset['name'],
                'source': os.path.join(source_dir, asset['path']),
                'type': asset['attrs'].get('type', 'sprite'),
                'format': asset['attrs'].get('format', 'rgba8888'),
                'alpha': asset['attrs'].get('alpha', 'straight'),
            }
            if 'tilew' in asset['attrs']:
                entry['tilew'] = int(asset['attrs']['tilew'])
            if 'tileh' in asset['attrs']:
                entry['tileh'] = int(asset['attrs']['tileh'])
            if 'framew' in asset['attrs']:
                entry['framew'] = int(asset['attrs']['framew'])
            if 'frameh' in asset['attrs']:
                entry['frameh'] = int(asset['attrs']['frameh'])
            if 'compress' in asset['attrs']:
                entry['compress'] = asset['attrs']['compress'].lower() == 'true'
            manifest['assets'].append(entry)

        os.makedirs(output_dir, exist_ok=True)
        manifest_path = os.path.join(output_dir, 'manifest.json')
        with open(manifest_path, 'w') as f:
            json.dump(manifest, f, indent=2)

        # Call asset compiler
        asset_pack_path = os.path.join(output_dir, 'assets.fpak')
        asset_compiler_path = os.path.join(SCRIPT_DIR, '..', '..', 'asset_compiler', 'src', 'assetc.py')

        try:
            result = subprocess.run(
                [sys.executable, asset_compiler_path, manifest_path, '-o', asset_pack_path],
                capture_output=True, text=True, check=True
            )
            if result.stdout:
                print(result.stdout)
        except subprocess.CalledProcessError as e:
            print(f"Asset compiler error: {e.stderr}", file=sys.stderr)
            print("Falling back to dummy asset data (game will show missing assets)", file=sys.stderr)
            return self._build_dummy_assets(output_dir)
        except FileNotFoundError:
            print("Asset compiler not found. Install Pillow: pip install Pillow", file=sys.stderr)
            print("Falling back to dummy asset data", file=sys.stderr)
            return self._build_dummy_assets(output_dir)

        # Read asset pack and build ROM asset table
        with open(asset_pack_path, 'rb') as f:
            pack_data = f.read()

        # Parse FPAK header
        if pack_data[0:4] != b'FPAK':
            raise AssemblerError("Invalid asset pack format")

        num_assets = struct.unpack('<I', pack_data[8:12])[0]
        dir_size = struct.unpack('<I', pack_data[12:16])[0]
        header_size = 16

        asset_table = bytearray()
        asset_data = bytearray()
        result_map = {}

        for idx in range(num_assets):
            entry_offset = header_size + idx * 36
            name_bytes = pack_data[entry_offset:entry_offset+28]
            name = name_bytes.rstrip(b'\x00').decode('utf-8')
            data_offset = struct.unpack('<I', pack_data[entry_offset+28:entry_offset+32])[0]
            data_size = struct.unpack('<I', pack_data[entry_offset+32:entry_offset+36])[0]

            # Copy asset data
            rom_offset = len(asset_data)
            asset_data += pack_data[data_offset:data_offset+data_size]
            # Align to 8 bytes
            while len(asset_data) % 8:
                asset_data.append(0)

            asset_cfg = next((a for a in self.assets if a['name'] == name), None)
            attrs = asset_cfg.get('attrs', {}) if asset_cfg else {}
            asset_type = attrs.get('type', 'sprite')
            type_code = self.asset_type_code(asset_type)
            aux0 = int(attrs.get('tilew', attrs.get('framew', 0))) if attrs else 0
            aux1 = int(attrs.get('tileh', attrs.get('frameh', 0))) if attrs else 0
            asset_table += struct.pack('<IIIIIIII', idx, rom_offset, data_size, type_code, aux0, aux1, 0, 0)
            result_map[name] = (idx, rom_offset, data_size)

        # Write combined asset blob
        with open(os.path.join(output_dir, 'assets.bin'), 'wb') as f:
            f.write(bytes(asset_table) + bytes(asset_data))

        return result_map

    def _build_dummy_assets(self, output_dir: str) -> Dict[str, Tuple[int, int, int]]:
        """Fallback: generate minimal dummy asset data when asset compiler is unavailable"""
        asset_data = bytearray()
        asset_table = bytearray()

        result = {}
        for idx, asset in enumerate(self.assets):
            name = asset['name']
            dummy = bytearray(64)
            dummy[:len(name)] = name.encode('ascii')[:64]

            offset = len(asset_data)
            size = len(dummy)
            asset_data += dummy
            while len(asset_data) % 8:
                asset_data.append(0)

            asset_type = asset['attrs'].get('type', 'sprite')
            type_code = self.asset_type_code(asset_type)
            aux0 = int(asset['attrs'].get('tilew', asset['attrs'].get('framew', 0)))
            aux1 = int(asset['attrs'].get('tileh', asset['attrs'].get('frameh', 0)))
            asset_table += struct.pack('<IIIIIIII', idx, offset, size, type_code, aux0, aux1, 0, 0)
            result[name] = (idx, offset, size)

        with open(os.path.join(output_dir, 'assets.bin'), 'wb') as f:
            f.write(bytes(asset_table) + bytes(asset_data))

        return result

    def assemble(self, source: str, output_dir: str = 'build', source_dir: str = '.') -> bytes:
        expanded = self.expand_macros(source)
        asset_map = self.build_assets(output_dir, source_dir)

        # Inject asset labels into assembly
        asset_lines = []
        for name, (idx, offset, size) in asset_map.items():
            asset_lines.append(f".equ {name}, {idx}")
            asset_lines.append(f".equ {name}_offset, {offset}")
            asset_lines.append(f".equ {name}_size, {size}")

        full_source = '\n'.join(asset_lines) + '\n' + expanded
        binary = self.asm.assemble(full_source)

        # Combine into ROM: header + code + asset table + asset data
        rom = bytearray(0x100)  # ROM header space
        rom[0:4] = b'FPVM'  # Magic
        rom[4:8] = struct.pack('<I', 0x00010000)  # Version
        rom[8:12] = struct.pack('<I', len(binary))  # Code size
        rom[12:16] = struct.pack('<I', 0)  # Entry point offset
        rom[16:20] = struct.pack('<I', len(self.assets))  # Asset count
        rom[20:24] = struct.pack('<I', 0x100 + len(binary))  # Asset directory offset

        rom += binary

        # Align and append assets
        asset_bin_path = os.path.join(output_dir, 'assets.bin')
        if os.path.exists(asset_bin_path):
            with open(asset_bin_path, 'rb') as f:
                asset_blob = f.read()
            rom += asset_blob

        return bytes(rom)


def main():
    parser = argparse.ArgumentParser(description='Fantasy Macro Assembler')
    parser.add_argument('input', help='Input .mfasm file')
    parser.add_argument('-o', '--output', required=True, help='Output ROM file')
    parser.add_argument('-I', '--include', action='append', default=['.'], help='Include paths')
    parser.add_argument('--asset-compiler', default=None, help='Path to asset compiler')
    args = parser.parse_args()

    with open(args.input, 'r') as f:
        source = f.read()

    masm = MacroAssembler(asset_compiler_path=args.asset_compiler)
    masm.include_paths = args.include
    try:
        source_dir = os.path.dirname(os.path.abspath(args.input))
        output_dir = os.path.dirname(args.output) or '.'
        rom = masm.assemble(source, output_dir, source_dir)
        with open(args.output, 'wb') as f:
            f.write(rom)
        print(f"Assembled ROM: {len(rom)} bytes ({len(masm.assets)} assets)")
    except AssemblerError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
