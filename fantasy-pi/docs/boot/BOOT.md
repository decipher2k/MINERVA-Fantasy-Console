# Raspberry Pi Boot Guide

## Supported Hardware

- Raspberry Pi 4 Model B (1GB, 2GB, 4GB, 8GB)
- Raspberry Pi 400
- Raspberry Pi 5 (with appropriate firmware)

## Boot Sequence

1. GPU loads `bootcode.bin` from SD card
2. GPU reads `config.txt` for configuration
3. GPU loads `start4.elf` / `start5.elf` (VideoCore firmware)
4. GPU loads `kernel8.img` (our bare-metal kernel) at address 0x80000
5. CPU starts in AArch64 mode, executing kernel code
6. Circle initializes hardware and calls `CKernel::Run()`

## Serial Debug Output

Connect USB-to-TTL adapter to GPIO pins:
- GPIO 14 (TX) -> RX on adapter
- GPIO 15 (RX) -> TX on adapter
- GND -> GND

Baud rate: 115200

## Troubleshooting

| Symptom                    | Cause                          | Solution                              |
|----------------------------|--------------------------------|---------------------------------------|
| Black screen, no activity  | Missing firmware files         | Copy bootcode.bin, start4.elf         |
| Rainbow screen             | kernel8.img not found          | Verify file on SD root                |
| Green LED flashes pattern  | Boot failure                   | Check config.txt                      |
| No serial output           | Wrong baud or wiring           | Verify 115200, TX/RX not swapped      |
| Garbled serial             | Voltage level mismatch         | Use 3.3V logic level adapter          |

## Firmware Files

Download from: https://github.com/raspberrypi/firmware/tree/master/boot

Required minimum:
- `bootcode.bin`
- `start4.elf` (Pi 4/400)
- `fixup4.dat` (Pi 4/400)

For Pi 5:
- `start5.elf`
- `fixup5.dat`

## Circle-Specific Notes

Circle handles:
- ARM timer and interrupts
- MMU configuration
- Framebuffer via mailbox interface (Pi 4) or DRM (Pi 5)
- USB host controller initialization
- VCHIQ for audio

Our kernel sits on top of Circle and does not need to interact with firmware directly after boot.
