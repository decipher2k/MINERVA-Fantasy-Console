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
sd_image.py - Create SD card deployment package

Instead of creating a raw disk image (which requires admin privileges
and platform-specific tools), this script creates a deployment directory
that can be copied to a FAT32-formatted SD card.
"""

import argparse
import os
import shutil
import sys


def create_sdcard_package(kernel_path, cartridge_path, output_dir):
    """Create a deployment directory for SD card"""
    os.makedirs(output_dir, exist_ok=True)

    # Copy kernel
    shutil.copy(kernel_path, os.path.join(output_dir, 'kernel8.img'))

    # Copy cartridge if provided
    if cartridge_path and os.path.exists(cartridge_path):
        shutil.copy(cartridge_path, os.path.join(output_dir, 'cartridge.bin'))

    # Copy config.txt
    script_dir = os.path.dirname(os.path.abspath(__file__))
    config_src = os.path.join(script_dir, 'config.txt')
    if os.path.exists(config_src):
        shutil.copy(config_src, os.path.join(output_dir, 'config.txt'))
    else:
        # Create minimal config.txt
        with open(os.path.join(output_dir, 'config.txt'), 'w') as f:
            f.write("arm_64bit=1\n")
            f.write("disable_overscan=1\n")
            f.write("hdmi_force_hotplug=1\n")
            f.write("hdmi_group=2\n")
            f.write("hdmi_mode=4\n")
            f.write("gpu_mem=16\n")
            f.write("enable_uart=1\n")

    print(f"SD card package created in: {output_dir}")
    print("\nFiles to copy to SD card boot partition:")
    for f in sorted(os.listdir(output_dir)):
        size = os.path.getsize(os.path.join(output_dir, f))
        print(f"  {f} ({size} bytes)")
    print("\nInstructions:")
    print("  1. Format SD card as FAT32")
    print("  2. Copy firmware files (bootcode.bin, start4.elf, fixup4.dat)")
    print("  3. Copy all files from the package directory to SD card root")
    print("  4. Insert SD card into Raspberry Pi and power on")


def main():
    parser = argparse.ArgumentParser(description='Create SD card deployment package')
    parser.add_argument('--kernel', required=True, help='Path to kernel8.img')
    parser.add_argument('--cartridge', default=None, help='Path to cartridge.bin')
    parser.add_argument('-o', '--output', default='sdcard_package', help='Output directory')
    args = parser.parse_args()

    if not os.path.exists(args.kernel):
        print(f"Error: Kernel not found: {args.kernel}", file=sys.stderr)
        sys.exit(1)

    create_sdcard_package(args.kernel, args.cartridge, args.output)


if __name__ == '__main__':
    main()
