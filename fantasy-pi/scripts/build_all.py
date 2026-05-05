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
build_all.py - Master build script for Fantasy Pi

Usage:
    python3 scripts/build_all.py           # Build everything
    python3 scripts/build_all.py --tests   # Build and run tests
    python3 scripts/build_all.py --rom     # Build example ROMs only
"""

import sys
import os
import subprocess
import argparse

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def run(cmd, cwd=None, desc=None):
    if desc:
        print(f"[BUILD] {desc}")
    print(f"  $ {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd or PROJECT_ROOT, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"ERROR: {result.stderr}", file=sys.stderr)
        return False
    if result.stdout:
        print(result.stdout)
    return True


def build_tests():
    print("\n=== Building Host Tests ===")
    build_dir = os.path.join(PROJECT_ROOT, "build", "host")
    os.makedirs(build_dir, exist_ok=True)
    if not run(["cmake", "../../build/cmake", "-DBUILD_TESTS=ON", "-DBUILD_HOST_EMULATOR=OFF"],
               cwd=build_dir, desc="Configuring CMake"):
        return False
    if not run(["cmake", "--build", "."], cwd=build_dir, desc="Building tests"):
        return False
    if not run(["ctest", "--output-on-failure"], cwd=build_dir, desc="Running tests"):
        return False
    print("=== All tests passed ===\n")
    return True


def build_roms():
    print("\n=== Building Example ROMs ===")
    examples = [
        ("examples/hello_world/src/main.fasm", "build/hello.rom"),
        ("examples/bitblt_demo/src/main.fasm", "build/bitblt.rom"),
        ("examples/sprite_demo/src/main.fasm", "build/sprite.rom"),
    ]
    for src, dst in examples:
        src_path = os.path.join(PROJECT_ROOT, src)
        dst_path = os.path.join(PROJECT_ROOT, dst)
        if os.path.exists(src_path):
            if not run([sys.executable, "tools/generate_test_rom.py", src_path, "-o", dst_path],
                       desc=f"Building {dst}"):
                return False
    print("=== ROMs built ===\n")
    return True


def build_emulator():
    print("\n=== Building Host Emulator ===")
    build_dir = os.path.join(PROJECT_ROOT, "build", "host")
    os.makedirs(build_dir, exist_ok=True)
    if not run(["cmake", "../../build/cmake", "-DBUILD_TESTS=OFF", "-DBUILD_HOST_EMULATOR=ON"],
               cwd=build_dir, desc="Configuring CMake with emulator"):
        return False
    if not run(["cmake", "--build", "."], cwd=build_dir, desc="Building emulator"):
        return False
    print("=== Emulator built ===\n")
    return True


def main():
    parser = argparse.ArgumentParser(description='Build Fantasy Pi')
    parser.add_argument('--tests', action='store_true', help='Build and run tests')
    parser.add_argument('--rom', action='store_true', help='Build example ROMs')
    parser.add_argument('--emulator', action='store_true', help='Build emulator')
    parser.add_argument('--all', action='store_true', help='Build everything')
    args = parser.parse_args()

    if not any([args.tests, args.rom, args.emulator, args.all]):
        args.all = True

    success = True
    if args.all or args.tests:
        success = success and build_tests()
    if args.all or args.rom:
        success = success and build_roms()
    if args.all or args.emulator:
        success = success and build_emulator()

    if success:
        print("\n" + "="*50)
        print("BUILD SUCCESSFUL")
        print("="*50)
        print("\nNext steps:")
        print("  - Run emulator:  build/host/fantasy_emulator build/hello.rom")
        print("  - Run tests:     cd build/host && ctest")
        print("  - Deploy to Pi:  Copy build/make/kernel8.img to SD card")
    else:
        print("\nBUILD FAILED", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
