# AGENTS.md - Project Context for AI Agents

## Project: Fantasy Pi

A bare-metal fantasy console for Raspberry Pi 4/5 using Circle library.

## Build Requirements

The project is now fully self-contained for bare-metal kernel builds on Windows.
All required tools are bundled in the repository:

### Bundled Tools (Kernel Build)
- `toolchain/aarch64-none-elf/` - ARM64 cross compiler (GCC 15.2.1)
- `tools/python/` - Python 3.11 with Pillow + numpy (for asset compilation)
- `tools/make/` - GNU Make 4.4.1 (MinGW-w64)
- `circle/` - Circle bare-metal library (pre-built)

### Quick Build (Windows)
Simply run `build.bat` from the project root. No installation required.

### Manual Build
```batch
tools\make\make.exe -C build/make all
```

### Host Build Requirements (Optional)
For the x86_64 emulator build, you still need:
- CMake 3.20+
- SDL2 (optional, for host emulator only)
- A native C++ compiler (MinGW, MSVC, or Clang)

Note: The host emulator build is NOT required for the Raspberry Pi kernel.

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
