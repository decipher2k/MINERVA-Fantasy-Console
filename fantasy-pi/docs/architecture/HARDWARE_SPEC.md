# Fantasy Pi Hardware Specification

## Console Overview

The Fantasy Pi console is a virtualized fantasy hardware platform executed by an ARM64 bare-metal kernel on Raspberry Pi 4/5.

## Display Subsystem

- **Resolution**: 640x480 (default), configurable up to 1920x1080
- **Color Depth**: 32-bit RGBA8888 framebuffers (internally)
- **Output**: Physical display via HDMI/DSI through Circle framebuffer driver
- **Refresh**: 60 Hz target, vsync-driven present

## Memory Map

| Region            | Address      | Size       | Purpose                     |
|-------------------|--------------|------------|-----------------------------|
| RAM               | 0x0000_0000  | 16 MB      | General purpose RAM         |
| VRAM              | 0x0100_0000  | 8 MB       | Video RAM (surfaces, etc.)  |
| ROM/Cartridge     | 0x0200_0000  | 32 MB      | Game code and assets        |
| MMIO Registers    | 0x0400_0000  | 64 KB      | Hardware I/O registers      |
| Stack             | 0x000F_FFFC  | grows down | Program stack               |

## VRAM Layout

| Region            | Size       | Purpose                     |
|-------------------|------------|-----------------------------|
| Framebuffer Front | 1.2 MB     | Active display buffer       |
| Framebuffer Back  | 1.2 MB     | Back buffer for page flip   |
| Sprite VRAM       | 4 MB       | Sprite and texture storage  |
| Tilemap VRAM      | 1 MB       | Tilemap and tileset data    |
| Palette RAM       | 16 KB      | 256 palettes x 256 colors   |
| Audio Buffers     | 512 KB     | PCM sample data             |

## CPU Specification

- **Architecture**: 32-bit load/store RISC (virtual)
- **Registers**: 16 general purpose (r0-r15), r15 = PC, r14 = LR, r13 = SP
- **Word Size**: 32-bit
- **Endianness**: Little-endian
- **Instruction Size**: Fixed 32-bit
- **Clock Speed**: Virtual; budget ~1M cycles/frame

## Register Set

| Register | Name | Purpose                          |
|----------|------|----------------------------------|
| r0-r3    |      | Argument / result / scratch      |
| r4-r11   |      | Callee-saved general purpose     |
| r12      | ip   | Intra-procedure scratch          |
| r13      | sp   | Stack pointer                    |
| r14      | lr   | Link register                    |
| r15      | pc   | Program counter                  |

## Audio Subsystem

- **Channels**: 8 mono/stereo channels
- **Sample Rate**: 44100 Hz
- **Format**: 16-bit signed PCM
- **Features**: Volume, pan, frequency modulation

## Input Model

- Keyboard (USB HID)
- Gamepad (USB HID, XInput-style mapping)
- 2 player support

## Timing

- **Frame Rate**: 60 Hz (16.67 ms per frame)
- **Timer**: 1 kHz system timer for delays
- **VSync Trap**: `TRAP_PRESENT` blocks until next frame

## Interrupt / Trap System

No hardware interrupts in VM. All I/O via `trap` instructions with vector numbers.

## ROM / Cartridge Format

See `ROM_FORMAT.md` for detailed cartridge layout.

## Asset Storage

Assets are stored in the ROM image after the code section. The asset table at offset 0x40 in ROM maps asset IDs to offsets and sizes.
