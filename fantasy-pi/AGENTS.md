# AGENTS.md - Project Context for AI Agents

## Project: Fantasy Pi

A bare-metal fantasy console for Raspberry Pi 4/5 using Circle library.

## Build Requirements

- aarch64-none-elf-gcc (cross compiler for ARM64 bare metal)
- Circle library (https://github.com/rsta2/circle) as submodule in `circle/`
- Python 3.9+ for assembler and asset compiler tools
- CMake 3.20+ for host builds
- libpng, libogg, libvorbis for asset compiler host builds

## Coding Style

- C++17 for kernel, VM, renderer
- Python 3 for tooling
- 4 spaces indentation (no tabs)
- snake_case for functions/variables, PascalCase for classes
- Headers use `#pragma once`

## Important Paths

- `circle/` - Circle bare-metal library (external dependency)
- `kernel/src/` - Circle kernel entry point and main loop
- `vm/src/` - Fantasy VM interpreter
- `renderer/src/` - Software renderer
- `assembler/src/` - Command-line assembler
- `macroassembler/src/` - Command-line macro assembler
- `asset_compiler/src/` - Asset pipeline
- `build/cmake/CMakeLists.txt` - Host build configuration
- `build/make/Makefile` - Bare-metal kernel build configuration

## Testing

- Host emulator build runs on x86_64 Windows/Linux
- `make test` runs unit tests
- Integration tests require real Raspberry Pi hardware

## Boot Process

1. `kernel8.img` is loaded by Raspberry Pi firmware
2. Circle initializes ARM cores, MMU, interrupts, USB, framebuffer
3. `CKernel::Run()` loads cartridge from embedded binary or SD card
4. Fantasy VM is initialized with cartridge ROM
5. VM main loop begins execution

## Asset Pipeline

1. Source assets (PNG, WAV) placed in game `assets/`
2. `.asset` directives in assembly reference source files
3. `assetc` converts and packs assets into binary blobs
4. `mfasm` resolves labels and produces ROM image
5. ROM is either linked into kernel or appended as cartridge

## Constraints

- No dynamic memory allocation in renderer hot path
- VM cycle budget: ~1M cycles per frame at 60 FPS
- Framebuffer: 640x480 default, up to 1920x1080
- Audio: 44100 Hz, 16-bit stereo, 8 channels
- Input polled once per frame
