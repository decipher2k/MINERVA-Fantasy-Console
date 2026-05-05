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
assetc.py - Asset Compiler for Fantasy Pi

Converts source assets (PNG, WAV, etc.) into console-native formats.
Supports:
- PNG -> RGBA8888, RGB565, RGBA4444, Indexed8, Indexed8Alpha
- PNG with alpha channel handling (straight vs premultiplied)
- Sprite sheets and tilesets
- WAV -> PCM16
- Font generation
- Palette extraction
- Compression (optional, run-length encoding baseline)
"""

import sys
import os
import struct
import argparse
import zlib
import json
from typing import Optional, Tuple, List, Dict
from dataclasses import dataclass

try:
    from PIL import Image
    import numpy as np
    HAS_PIL = True
except ImportError:
    HAS_PIL = False
    print("Warning: Pillow not installed. Image conversion disabled.", file=sys.stderr)

try:
    import wave
    HAS_WAVE = True
except ImportError:
    HAS_WAVE = False


@dataclass
class AssetConfig:
    name: str
    source_path: str
    asset_type: str  # sprite, image, tileset, spritesheet, font, audio, palette
    pixel_format: str = 'rgba8888'
    alpha_mode: str = 'straight'  # straight or premultiplied
    tile_width: int = 0
    tile_height: int = 0
    frame_width: int = 0
    frame_height: int = 0
    glyph_width: int = 0
    glyph_height: int = 0
    color_key: Optional[Tuple[int, int, int]] = None
    compress: bool = False
    palette_size: int = 256


class AssetError(Exception):
    pass


class AssetCompiler:
    def __init__(self):
        self.assets: List[Tuple[AssetConfig, bytes]] = []

    def premultiply_alpha(self, img: 'Image.Image') -> 'Image.Image':
        """Convert straight RGBA to premultiplied alpha"""
        if img.mode != 'RGBA':
            return img
        arr = np.array(img)
        alpha = arr[:, :, 3:4].astype(np.float32) / 255.0
        rgb = arr[:, :, :3].astype(np.float32)
        premul = (rgb * alpha).astype(np.uint8)
        result = np.dstack((premul, arr[:, :, 3]))
        return Image.fromarray(result, 'RGBA')

    def quantize_to_indexed(self, img: 'Image.Image', colors: int = 256) -> Tuple['Image.Image', List[Tuple[int, int, int, int]]]:
        """Convert image to indexed color with palette"""
        img_rgb = img.convert('RGB')
        img_p = img_rgb.quantize(colors=colors, method=Image.Quantize.MEDIANCUT)
        palette = []
        pal_bytes = img_p.getpalette()
        for i in range(colors):
            r = pal_bytes[i * 3]
            g = pal_bytes[i * 3 + 1]
            b = pal_bytes[i * 3 + 2]
            palette.append((r, g, b, 255))
        return img_p, palette

    def convert_image(self, config: AssetConfig) -> bytes:
        if not HAS_PIL:
            raise AssetError("Pillow required for image conversion")

        img = Image.open(config.source_path)
        has_alpha = img.mode in ('RGBA', 'LA', 'P')

        if config.alpha_mode == 'premultiplied' and has_alpha:
            img = self.premultiply_alpha(img.convert('RGBA'))
        elif has_alpha:
            img = img.convert('RGBA')
        else:
            img = img.convert('RGB')

        width, height = img.size
        output = bytearray()

        # Header: width (2), height (2), format (1), flags (1), reserved (4)
        fmt_code = {
            'rgb565': 0, 'rgba8888': 1, 'argb8888': 2,
            'rgba4444': 3, 'indexed8': 4, 'indexed8alpha': 5,
        }.get(config.pixel_format, 1)

        flags = 0
        if config.alpha_mode == 'premultiplied':
            flags |= 1
        if config.compress:
            flags |= 2

        output += struct.pack('<HHBB', width, height, fmt_code, flags)
        output += struct.pack('<I', 0)  # reserved

        pixels = bytearray()

        if config.pixel_format == 'rgba8888':
            if img.mode != 'RGBA':
                img = img.convert('RGBA')
            arr = np.array(img)
            for y in range(height):
                for x in range(width):
                    r, g, b, a = arr[y, x]
                    pixels += bytes([r, g, b, a])

        elif config.pixel_format == 'argb8888':
            if img.mode != 'RGBA':
                img = img.convert('RGBA')
            arr = np.array(img)
            for y in range(height):
                for x in range(width):
                    r, g, b, a = arr[y, x]
                    pixels += bytes([a, r, g, b])

        elif config.pixel_format == 'rgb565':
            if img.mode != 'RGB':
                img = img.convert('RGB')
            arr = np.array(img)
            for y in range(height):
                for x in range(width):
                    r, g, b = arr[y, x]
                    r5 = (r >> 3) & 0x1F
                    g6 = (g >> 2) & 0x3F
                    b5 = (b >> 3) & 0x1F
                    v = (r5 << 11) | (g6 << 5) | b5
                    pixels += struct.pack('<H', v)

        elif config.pixel_format == 'rgba4444':
            if img.mode != 'RGBA':
                img = img.convert('RGBA')
            arr = np.array(img)
            for y in range(height):
                for x in range(width):
                    r, g, b, a = arr[y, x]
                    r4 = (r >> 4) & 0x0F
                    g4 = (g >> 4) & 0x0F
                    b4 = (b >> 4) & 0x0F
                    a4 = (a >> 4) & 0x0F
                    v = (r4 << 12) | (g4 << 8) | (b4 << 4) | a4
                    pixels += struct.pack('<H', v)

        elif config.pixel_format in ('indexed8', 'indexed8alpha'):
            img_idx, palette = self.quantize_to_indexed(img, config.palette_size)
            arr = np.array(img_idx)
            for y in range(height):
                for x in range(width):
                    pixels.append(arr[y, x])
            # Append palette after pixels (256 colors * 4 bytes)
            for r, g, b, a in palette:
                pixels += bytes([r, g, b, a])

        else:
            raise AssetError(f"Unsupported pixel format: {config.pixel_format}")

        if config.compress:
            pixels = zlib.compress(bytes(pixels))
            output += struct.pack('<I', len(pixels))

        output += pixels
        return bytes(output)

    def convert_audio(self, config: AssetConfig) -> bytes:
        if not HAS_WAVE:
            raise AssetError("wave module required for audio conversion")

        with wave.open(config.source_path, 'rb') as wf:
            nchannels = wf.getnchannels()
            sampwidth = wf.getsampwidth()
            framerate = wf.getframerate()
            nframes = wf.getnframes()
            frames = wf.readframes(nframes)

        output = bytearray()
        output += struct.pack('<HHII', nchannels, sampwidth, framerate, nframes)
        output += frames
        return bytes(output)

    def convert_tilemap(self, config: AssetConfig) -> bytes:
        """Parse Tiled JSON and output binary tilemap"""
        with open(config.source_path, 'r') as f:
            data = json.load(f)

        width = data.get('width', 0)
        height = data.get('height', 0)
        tile_width = data.get('tilewidth', 16)
        tile_height = data.get('tileheight', 16)
        layers = data.get('layers', [])

        # Find first tile layer
        tile_data = []
        for layer in layers:
            if layer.get('type') == 'tilelayer':
                tile_data = layer.get('data', [])
                break

        output = bytearray()
        output += struct.pack('<HHHH', width, height, tile_width, tile_height)
        output += struct.pack('<I', len(tile_data))

        for gid in tile_data:
            # Tiled gid: lower bits = tile index, upper bits = flip flags
            tile_idx = gid & 0x0FFFFFFF
            flip_h = 1 if (gid & 0x80000000) else 0
            flip_v = 1 if (gid & 0x40000000) else 0
            flip_d = 1 if (gid & 0x20000000) else 0
            flags = (flip_h) | (flip_v << 1) | (flip_d << 2)
            output += struct.pack('<HB', tile_idx & 0xFFFF, flags)

        return bytes(output)

    def convert(self, config: AssetConfig) -> bytes:
        if config.asset_type in ('sprite', 'image', 'tileset', 'spritesheet', 'font', 'palette'):
            return self.convert_image(config)
        elif config.asset_type == 'audio':
            return self.convert_audio(config)
        elif config.asset_type == 'tilemap':
            return self.convert_tilemap(config)
        else:
            # Raw binary
            with open(config.source_path, 'rb') as f:
                return f.read()

    def build_manifest(self, configs: List[AssetConfig]) -> Tuple[bytes, Dict[str, Tuple[int, int]]]:
        """Build combined asset blob with directory"""
        # Asset directory at start
        num_assets = len(configs)
        header = bytearray()
        header += b'FPAK'  # Magic
        header += struct.pack('<I', 0x00010000)  # Version
        header += struct.pack('<I', num_assets)

        dir_size = num_assets * 36
        header += struct.pack('<I', dir_size)  # Directory size
        data_offset = len(header) + dir_size

        directory = bytearray()
        data = bytearray()
        offsets = {}

        for idx, cfg in enumerate(configs):
            blob = self.convert(cfg)
            name_bytes = cfg.name.encode('utf-8')[:28]
            name_padded = name_bytes + b'\x00' * (28 - len(name_bytes))

            off = data_offset + len(data)
            size = len(blob)
            directory += name_padded
            directory += struct.pack('<II', off, size)
            offsets[cfg.name] = (off, size)
            data += blob
            # Align to 8 bytes
            while len(data) % 8:
                data.append(0)

        return bytes(header + directory + data), offsets


def main():
    parser = argparse.ArgumentParser(description='Fantasy Pi Asset Compiler')
    parser.add_argument('manifest', help='JSON manifest file')
    parser.add_argument('-o', '--output', required=True, help='Output asset pack')
    args = parser.parse_args()

    with open(args.manifest, 'r') as f:
        manifest = json.load(f)

    configs = []
    for item in manifest.get('assets', []):
        cfg = AssetConfig(
            name=item['name'],
            source_path=item['source'],
            asset_type=item.get('type', 'sprite'),
            pixel_format=item.get('format', 'rgba8888'),
            alpha_mode=item.get('alpha', 'straight'),
            tile_width=item.get('tilew', 0),
            tile_height=item.get('tileh', 0),
            frame_width=item.get('framew', 0),
            frame_height=item.get('frameh', 0),
            compress=item.get('compress', False),
        )
        configs.append(cfg)

    compiler = AssetCompiler()
    try:
        blob, offsets = compiler.build_manifest(configs)
        with open(args.output, 'wb') as f:
            f.write(blob)
        print(f"Asset pack: {len(blob)} bytes ({len(configs)} assets)")
        for name, (off, size) in offsets.items():
            print(f"  {name}: offset={off}, size={size}")
    except AssetError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
