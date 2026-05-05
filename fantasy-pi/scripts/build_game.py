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
build_game.py - Convenience script to build a game ROM

Usage:
    python3 scripts/build_game.py games/test_game/src/main.fasm -o test_game.rom
"""

import argparse
import os
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)


def run_cmd(cmd, cwd=None):
    print(f"  > {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Error: {result.stderr}", file=sys.stderr)
        sys.exit(1)
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description='Build a Fantasy Pi game')
    parser.add_argument('source', help='Main .fasm or .mfasm file')
    parser.add_argument('-o', '--output', required=True, help='Output ROM file')
    parser.add_argument('--assets', default=None, help='Asset directory')
    parser.add_argument('--include', '-I', action='append', default=[], help='Include paths')
    args = parser.parse_args()

    abs_source = os.path.abspath(args.source)
    abs_output = os.path.abspath(args.output)
    game_dir = os.path.dirname(abs_source)
    asset_dir = os.path.abspath(args.assets) if args.assets else os.path.join(game_dir, 'assets')

    includes = [game_dir, asset_dir] + [os.path.abspath(p) for p in args.include]

    # Determine if macroassembler is needed
    ext = os.path.splitext(abs_source)[1].lower()
    if ext == '.mfasm' or os.path.exists(abs_source.replace('.fasm', '.mfasm')):
        assembler = os.path.join(PROJECT_ROOT, 'macroassembler', 'src', 'mfasm.py')
    else:
        assembler = os.path.join(PROJECT_ROOT, 'assembler', 'src', 'fasm.py')

    cmd = ['python3', assembler, abs_source, '-o', abs_output]
    for inc in includes:
        cmd += ['-I', inc]

    run_cmd(cmd, cwd=PROJECT_ROOT)
    print(f"Build complete: {abs_output} ({os.path.getsize(abs_output)} bytes)")


if __name__ == '__main__':
    main()
