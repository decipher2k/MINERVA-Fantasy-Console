# Fantasy Pi ROM / Cartridge Format

## Overview

The ROM is the binary image containing game code, assets, and metadata. It can be:
1. Embedded directly into the kernel binary (monolithic)
2. Appended to the kernel image (kernel + cartridge)
3. Loaded from SD card FAT partition at runtime

## ROM Layout

```
Offset      Size        Content
-------------------------------------------
0x000000    0x000100    ROM Header
0x000100    code_size   Code Section (.text)
code_end    variable    Asset Directory
asset_dir   variable    Asset Data
```

## ROM Header (256 bytes)

| Offset | Size | Field            | Description                      |
|--------|------|------------------|----------------------------------|
| 0x00   | 4    | magic            | "FPVM"                           |
| 0x04   | 4    | version          | 0x00010000 (major.minor.patch)   |
| 0x08   | 4    | code_size        | Size of code section in bytes    |
| 0x0C   | 4    | code_entry       | Entry point offset (default 0)   |
| 0x10   | 4    | asset_count      | Number of assets                 |
| 0x14   | 4    | asset_dir_offset | Offset to asset directory        |
| 0x18   | 4    | flags            | Bit 0: compressed, Bit 1: signed |
| 0x1C   | 4    | checksum         | CRC32 of ROM (excluding header)  |
| 0x20   | 32   | title            | Game title (ASCII, zero-padded)  |
| 0x40   | 32   | author           | Author name                      |
| 0x60   | 16   | reserved         | Reserved for future use          |
| 0x70   | 144  | padding          | Zero padding to 256 bytes        |

## Asset Directory Entry (32 bytes each)

| Offset | Size | Field       | Description                      |
|--------|------|-------------|----------------------------------|
| 0x00   | 28   | name        | Asset name (null-terminated)     |
| 0x1C   | 4    | data_offset | Offset to asset data from ROM start |
| 0x20   | 4    | data_size   | Size of asset data in bytes      |

## Asset Data Format

### Image / Sprite Asset Header (8 bytes + pixel data)

| Offset | Size | Field       | Description                      |
|--------|------|-------------|----------------------------------|
| 0x00   | 2    | width       | Width in pixels                  |
| 0x02   | 2    | height      | Height in pixels                 |
| 0x04   | 1    | format      | PixelFormat enum value           |
| 0x05   | 1    | flags       | Bit 0: premultiplied alpha       |
| 0x06   | 2    | reserved    | Reserved                         |

### Audio Asset Header (16 bytes + PCM data)

| Offset | Size | Field       | Description                      |
|--------|------|-------------|----------------------------------|
| 0x00   | 2    | channels    | 1=mono, 2=stereo                 |
| 0x02   | 2    | sample_bits | 8 or 16                          |
| 0x04   | 4    | sample_rate | e.g. 44100                       |
| 0x08   | 4    | frame_count | Number of PCM frames             |
| 0x0C   | 4    | reserved    | Reserved                         |

## Alignment

- Code section aligned to 4 bytes
- Asset directory aligned to 8 bytes
- Asset data aligned to 8 bytes

## Cartridge Variants

### Monolithic Kernel
The kernel image is linked with the ROM data in a special `.rom` section. The kernel copies this section to the VM's ROM area at startup.

### Separate Cartridge
The kernel searches for `cartridge.bin` on the SD card root or accepts a cartridge appended to `kernel8.img` by looking for the "FPVM" magic at the end of the kernel image.
