# Fantasy Pi Development Roadmap

## Phase 1: Foundation (Weeks 1-2)
**Goal**: Bare-metal boot with Circle, basic framebuffer

- [ ] Set up aarch64-none-elf cross compiler
- [ ] Integrate Circle as git submodule
- [ ] Build minimal `kernel8.img` that boots to colored screen
- [ ] UART debug output working
- [ ] Host emulator skeleton (SDL2 window)

**Files**: `kernel/src/kernel.cpp`, `build/make/Makefile`
**Tests**: Boot test on real Pi, emulator window opens

## Phase 2: Fantasy VM Core (Weeks 3-4)
**Goal**: Functional virtual CPU

- [ ] Implement all 32 opcodes
- [ ] Register file and flag handling
- [ ] Memory access (RAM/ROM/VRAM)
- [ ] Trap dispatch mechanism
- [ ] Host emulator runs test programs

**Files**: `vm/src/fantasy_vm.cpp`, `tests/vm/test_vm.cpp`
**Tests**: All VM unit tests pass

## Phase 3: Assembler (Week 5)
**Goal**: Two-pass assembler producing ROM images

- [ ] `fasm.py` handles labels, instructions, data directives
- [ ] `mfasm.py` macro expansion and includes
- [ ] ROM header generation
- [ ] Basic trap constant definitions

**Files**: `assembler/src/fasm.py`, `macroassembler/src/mfasm.py`
**Tests**: Assembler golden-output tests

## Phase 4: Software Renderer Baseline (Weeks 6-7)
**Goal**: BitBlt, clipping, basic sprites

- [ ] Surface allocation and pixel format abstraction
- [ ] BitBlt copy (no alpha)
- [ ] Rectangle clipping
- [ ] Flip X/Y
- [ ] Color-key transparency
- [ ] Framebuffer double buffering

**Files**: `renderer/src/renderer.cpp`
**Tests**: `tests/renderer/test_renderer.cpp`

## Phase 5: Alpha Blending & Advanced Graphics (Weeks 8-9)
**Goal**: Full sprite rendering with alpha

- [ ] Premultiplied alpha pipeline
- [ ] Normal alpha blending (fast integer divide by 255)
- [ ] Additive blend mode
- [ ] Global alpha support
- [ ] Sprite registration and drawing API
- [ ] Tilemap rendering

**Files**: `renderer/src/renderer.cpp` (blit functions)
**Tests**: `tests/renderer/test_blit.cpp`

## Phase 6: Asset Pipeline (Week 10)
**Goal**: PNG/WAV to console-native formats

- [ ] `assetc.py` PNG loading via Pillow
- [ ] RGBA8888, RGB565, RGBA4444 conversion
- [ ] Premultiplied alpha conversion
- [ ] Indexed color quantization
- [ ] WAV to PCM16
- [ ] Asset pack format (.fpak)

**Files**: `asset_compiler/src/assetc.py`
**Tests**: Asset compiler golden-image tests

## Phase 7: Input & Audio (Weeks 11-12)
**Goal**: Playable with sound

- [ ] USB keyboard polling via Circle
- [ ] USB gamepad support (XInput mapping)
- [ ] Audio subsystem initialization
- [ ] PCM playback on 8 channels
- [ ] Volume and pan controls

**Files**: `kernel/src/kernel.cpp` (input/audio traps)
**Tests**: Input state unit tests, audio playback test

## Phase 8: Cartridge System (Week 13)
**Goal**: Game distribution as ROM files

- [ ] Monolithic kernel build (embedded cartridge)
- [ ] Separate cartridge.bin loading from SD card
- [ ] ROM header parsing
- [ ] Asset table lookups
- [ ] Asset registration with renderer at load time

**Files**: `docs/architecture/ROM_FORMAT.md`
**Tests**: Load test ROMs on Pi hardware

## Phase 9: Example Games & Stress Tests (Weeks 14-15)
**Goal**: Prove the system works

- [ ] `hello_world`: Basic text and colors
- [ ] `bitblt_demo`: Stress test 100+ sprites
- [ ] `sprite_demo`: Alpha blending showcase
- [ ] `test_game`: Platformer with physics
- [ ] Performance profiling and optimization

**Files**: `examples/`, `games/`
**Tests**: 60 FPS maintained on Pi 4 at 640x480

## Phase 10: Toolchain & GCC Port (Weeks 16-20)
**Goal**: C-to-Fantasy-ASM compilation path

- [ ] Document GCC backend porting requirements
- [ ] Custom ELF target definition (BFD)
- [ ] Minimal GAS backend or direct object generation
- [ ] C frontend producing Fantasy ASM intermediate
- [ ] `libc` stub for freestanding environment
- [ ] crt0 / startup code

**Files**: `toolchain/`
**Tests**: Compile simple C program to working ROM

## Phase 11: Debugger & Development Tools (Weeks 21-22)
**Goal**: Developer experience improvements

- [ ] VM step debugger via UART
- [ ] Disassembler (`fdisasm`)
- [ ] Memory dump commands
- [ ] Register display
- [ ] Frame time overlay on screen

**Files**: `tools/debugger/`
**Tests**: Interactive debugging session

## Phase 12: Polish & Release (Week 23+)
**Goal**: Stable v1.0

- [ ] Complete documentation
- [ ] Performance benchmarks
- [ ] Pi 5 compatibility verification
- [ ] SD card image generator script
- [ ] Release binaries and example games

**Files**: `docs/`, `scripts/`
**Tests**: Full system integration tests

## Risk Assessment

| Risk                              | Impact | Mitigation                                   |
|-----------------------------------|--------|----------------------------------------------|
| Circle framebuffer performance    | High   | Use ARM NEON blitting, DMA if available      |
| GCC port complexity               | High   | Macroassembler first, C compiler second      |
| USB gamepad compatibility         | Medium | Test with popular pads, fallback to keyboard |
| Audio latency                     | Medium | Double-buffered DMA audio                    |
| Asset memory limits               | Medium | Compression, streaming from SD card          |
| Pi 5 hardware differences         | Medium | Abstract hardware behind Circle layer        |
