# MINERVA - Bare-Metal Fantasy Console for Raspberry Pi 4/5

## Overview

MINERVA is a bare-metal fantasy console running directly on Raspberry Pi 4 and 5 hardware without Linux. It consists of:

- A custom Fantasy VM with its own instruction set architecture (AEGIS)
- A software renderer with BitBlt, alpha blending, sprite compositing, and tilemap support
- A macroassembler with asset embedding directives
- An asset compiler converting PNG/WAV/etc. to console-native formats
- A Circle-based bare-metal kernel that hosts the VM and provides hardware abstraction

## Architecture

```
+------------------------------------------+
|            Bare-Metal Kernel             |
|         (Circle + ARM64 Startup)         |
+------------------------------------------+
|  Fantasy VM  |  Renderer  |  Audio/Input |
+------------------------------------------+
|         ROM / Cartridge Image            |
|  (Game Code + Assets + Metadata)         |
+------------------------------------------+
```

## Quick Start

See `docs/build/BUILD.md` for setup instructions.

## Directory Layout

- `kernel/` - Circle-based bare-metal kernel
- `vm/` - Fantasy VM implementation
- `renderer/` - Software rendering engine
- `assembler/` - Native assembler (fasm)
- `macroassembler/` - Macro assembler (mfasm)
- `asset_compiler/` - Asset pipeline tool (assetc)
- `toolchain/` - GCC/binutils porting support
- `examples/` - Example projects
- `games/` - Full game projects
- `docs/` - Technical documentation
- `tests/` - Unit and integration tests

## Build System

The project uses CMake for host builds (emulator, tools) and Make for the bare-metal kernel.

## Hardware Target

- Raspberry Pi 4 (BCM2711) and Pi 5 (BCM2712)
- AArch64 execution mode
- Framebuffer via Circle's VC4/V3D/Display driver
- USB HID input (keyboard, gamepad)
- PWM/HDMI audio output

## License

MIT License - See LICENSE file for details.
