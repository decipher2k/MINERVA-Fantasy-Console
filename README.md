# Fantasy Pi Build Guide

This document describes how to build MINERVA on Windows, Linux, and macOS:

- the SDL2 host emulator for running ROMs
- `fasm.py` and `mfasm.py` for building ROM cartridges
- the host GCC/Clang toolchain used to build the emulator and tools
- the current status of a GCC toolchain for Fantasy Pi ROMs
- the Arm AArch64 cross-toolchain used for the Raspberry Pi kernel
- Circle, the bare-metal Raspberry Pi support library

The commands assume this repository is checked out as `fantasy-pi`.

## Quickly Build a Game
First, create a game. A manual with opcodes can be found in fantasy-pi\docs\guide.html<br>
Copy the source files and the assets to fantasy-pi\input_asm\<br>
<br>
cd fantasy-pi<br>
.\build.bat<br>
<br>
## Tool Links

Use official package managers where possible. These links are stable entry points rather than version-pinned download files.

| Tool | Purpose | Link |
| --- | --- | --- |
| Git | Source checkout and Git Bash on Windows | https://git-scm.com/downloads |
| Git for Windows | Windows Git + Unix tools (`bash`, `cp`, `rm`, `wc`) | https://gitforwindows.org/ |
| Python | ROM tools and asset pipeline | https://www.python.org/downloads/ |
| CMake | Host emulator build generation | https://cmake.org/download/ |
| Ninja | Fast CMake build backend | https://ninja-build.org/ |
| SDL2 | Emulator window, audio, keyboard, gamepad | https://github.com/libsdl-org/SDL/releases |
| MSYS2 | Recommended native Windows MinGW environment | https://www.msys2.org/ |
| Visual Studio Build Tools | Optional MSVC compiler on Windows | https://visualstudio.microsoft.com/vs/cplusplus/ |
| vcpkg | Optional cross-platform C/C++ dependency manager | https://learn.microsoft.com/en-us/vcpkg/get_started/overview |
| Homebrew | Recommended macOS package manager | https://brew.sh/ |
| Arm GNU Toolchain | AArch64 bare-metal Raspberry Pi kernel compiler | https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads |
| Circle | Raspberry Pi bare-metal C++ environment | https://github.com/rsta2/circle |
| GCC sources | Source release for a future Fantasy Pi GCC port | https://gcc.gnu.org/releases.html |
| Binutils sources | Source release for a future Fantasy Pi assembler/linker port | https://www.gnu.org/software/binutils/ |
| GCC prerequisites | GMP/MPFR/MPC/ISL requirements | https://gcc.gnu.org/install/prerequisites.html |
| Raspberry Pi config.txt docs | Boot partition configuration | https://www.raspberrypi.com/documentation/computers/config_txt.html |
| Raspberry Pi Imager | SD card preparation utility | https://www.raspberrypi.com/software/ |

## Repository Layout

Important build directories:

| Path | Purpose |
| --- | --- |
| `build/cmake/` | CMake host build for emulator and tests |
| `build/make/` | Raspberry Pi bare-metal kernel Makefile |
| `assembler/src/fasm.py` | Base Fantasy assembler |
| `macroassembler/src/mfasm.py` | ROM/cartridge macroassembler with assets |
| `asset_compiler/src/assetc.py` | PNG/WAV/JSON asset converter |
| `games/test_game/` | Example ROM project |
| `emulator/` | Host emulator frontend |
| `vm/`, `renderer/` | Shared VM and renderer code |
| `kernel/` | Circle-based Raspberry Pi kernel |

## Python Setup for ROM Builds

The ROM builders are Python scripts. Image assets require Pillow and NumPy; without them the macroassembler falls back to dummy asset data.

Windows PowerShell:

```powershell
py -3 -m pip install --upgrade pip
py -3 -m pip install Pillow numpy
```

Linux/macOS:

```sh
python3 -m pip install --upgrade pip
python3 -m pip install --user Pillow numpy
```

If your distribution blocks `pip --user`, create a virtual environment:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install Pillow numpy
```

On Windows PowerShell:

```powershell
py -3 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install Pillow numpy
```

## Build ROMs with `fasm.py`

`fasm.py` assembles raw Fantasy Assembly into a binary instruction/data image. It does not build a full ROM header or assets. Use it for low-level tests and experiments.

Windows:

```powershell
cd C:\path\to\fantasy-pi
py -3 assembler\src\fasm.py games\test_game\src\main.fasm -o build\test_game.bin
```

Linux/macOS:

```sh
cd /path/to/fantasy-pi
python3 assembler/src/fasm.py games/test_game/src/main.fasm -o build/test_game.bin
```

## Build ROMs with `mfasm.py`

`mfasm.py` is the normal ROM builder. It supports macros, includes, `.asset` declarations, and produces a complete `.rom` or `cartridge.bin`.

Windows:

```powershell
cd C:\path\to\fantasy-pi
New-Item -ItemType Directory -Force build | Out-Null
py -3 macroassembler\src\mfasm.py games\test_game\src\main.fasm `
  -o build\test_game.rom `
  -I games\test_game\src `
  -I games\test_game\assets
```

Linux/macOS:

```sh
cd /path/to/fantasy-pi
mkdir -p build
python3 macroassembler/src/mfasm.py games/test_game/src/main.fasm \
  -o build/test_game.rom \
  -I games/test_game/src \
  -I games/test_game/assets
```

For the Raspberry Pi kernel build, the Makefile creates `build/make/cartridge.bin` automatically from `games/test_game/src/main.fasm`.

## Run ROMs in the Host Emulator

The emulator takes a ROM path as its first argument.

Windows:

```powershell
.\build\host\fantasy_emulator.exe build\test_game.rom
```

Linux/macOS:

```sh
./build/host/fantasy_emulator build/test_game.rom
```

Input mapping in the SDL2 emulator:

| Control | Keyboard |
| --- | --- |
| D-pad | Arrow keys or WASD |
| A / jump | `Z` or Space |
| B | `X` or Left Shift |
| Start | Enter |
| Back / quit | Escape |

SDL2 game controllers are also supported through SDL's controller API.

## Windows Host Emulator Build

Two Windows setups are supported. MSYS2/MinGW is the easiest if you want the same GCC-style workflow on all platforms. MSVC + vcpkg is also fine.

### Option A: MSYS2 UCRT64

Install MSYS2 from https://www.msys2.org/ and open the **UCRT64** shell.

```sh
pacman -Syu
pacman -S --needed \
  git \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-SDL2 \
  mingw-w64-ucrt-x86_64-python \
  mingw-w64-ucrt-x86_64-python-pillow \
  mingw-w64-ucrt-x86_64-python-numpy
```

Build:

```sh
cd /c/path/to/fantasy-pi
cmake -S build/cmake -B build/host -G Ninja \
  -DBUILD_HOST_EMULATOR=ON \
  -DBUILD_TOOLS=ON \
  -DBUILD_TESTS=ON
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Build a test ROM and run it:

```sh
python macroassembler/src/mfasm.py games/test_game/src/main.fasm \
  -o build/test_game.rom \
  -I games/test_game/src \
  -I games/test_game/assets
./build/host/fantasy_emulator.exe build/test_game.rom
```

### Option B: MSVC + vcpkg

Install Visual Studio Build Tools with the **Desktop development with C++** workload. Install Ninja and CMake from the links above, or through Visual Studio.

Install vcpkg and SDL2:

```powershell
cd C:\
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg.exe install sdl2:x64-windows
```

Build from a **Developer PowerShell for VS**:

```powershell
cd C:\path\to\fantasy-pi
cmake -S build\cmake -B build\host -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DBUILD_HOST_EMULATOR=ON `
  -DBUILD_TOOLS=ON `
  -DBUILD_TESTS=ON
cmake --build build\host
ctest --test-dir build\host --output-on-failure
```

If CMake cannot find SDL2, set `SDL2_DIR` to the SDL2 CMake package directory or use the vcpkg toolchain file above.

## Linux Host Emulator Build

Ubuntu/Debian:

```sh
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  ninja-build \
  python3 \
  python3-pip \
  libsdl2-dev
python3 -m pip install --user Pillow numpy
```

Fedora:

```sh
sudo dnf install -y \
  gcc-c++ \
  cmake \
  ninja-build \
  python3 \
  python3-pip \
  SDL2-devel
python3 -m pip install --user Pillow numpy
```

Build:

```sh
cd /path/to/fantasy-pi
cmake -S build/cmake -B build/host -G Ninja \
  -DBUILD_HOST_EMULATOR=ON \
  -DBUILD_TOOLS=ON \
  -DBUILD_TESTS=ON
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Build and run the test ROM:

```sh
python3 macroassembler/src/mfasm.py games/test_game/src/main.fasm \
  -o build/test_game.rom \
  -I games/test_game/src \
  -I games/test_game/assets
./build/host/fantasy_emulator build/test_game.rom
```

## macOS Host Emulator Build

Install Homebrew from https://brew.sh/.

```sh
brew install cmake ninja sdl2 python
python3 -m pip install --user Pillow numpy
```

Build:

```sh
cd /path/to/fantasy-pi
cmake -S build/cmake -B build/host -G Ninja \
  -DBUILD_HOST_EMULATOR=ON \
  -DBUILD_TOOLS=ON \
  -DBUILD_TESTS=ON
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Build and run the test ROM:

```sh
python3 macroassembler/src/mfasm.py games/test_game/src/main.fasm \
  -o build/test_game.rom \
  -I games/test_game/src \
  -I games/test_game/assets
./build/host/fantasy_emulator build/test_game.rom
```

If SDL2 is installed by Homebrew but CMake does not find it, pass:

```sh
cmake -S build/cmake -B build/host -G Ninja \
  -DCMAKE_PREFIX_PATH="$(brew --prefix sdl2)"
```

## Host GCC/Clang Toolchain for Building the Emulator and ROM Tools

This is the compiler used on your development machine to build `fantasy_emulator`, tests, and native helper code. It is not the Raspberry Pi kernel cross-compiler and not a Fantasy VM ROM compiler.

Recommended host compilers:

| Host | Recommended compiler |
| --- | --- |
| Windows | MSYS2 MinGW GCC or MSVC Build Tools |
| Linux | distro GCC or Clang |
| macOS | Apple Clang from Xcode Command Line Tools |

Minimum requirement: C++17.

Check versions:

```sh
cmake --version
ninja --version
python3 --version
c++ --version
```

Windows PowerShell equivalents:

```powershell
cmake --version
ninja --version
py -3 --version
g++ --version
```

## Fantasy Pi GCC Toolchain for ROMs

The working ROM build path today is `mfasm.py`. A full GCC/binutils target for Fantasy Pi ROMs is not currently implemented in this repository.

The intended future toolchain would look like this:

```sh
fantasy-elf-gcc -O2 -ffreestanding -c game.c -o game.o
fantasy-elf-ld -T fantasy-rom.ld game.o -o game.elf
fantasy-elf-objcopy -O binary game.elf game.code
python3 macroassembler/src/mfasm.py wrapper.fasm -o game.rom
```

To build such a toolchain on any platform, the missing pieces are:

- a binutils BFD target for the Fantasy VM object format or ELF relocations
- a GAS parser for Fantasy assembly, or a compiler that emits `.fasm`
- a GCC backend under `gcc/config/fantasy/`
- libgcc helpers for division, multiplication, and possibly software floating point
- a ROM linker script and startup ABI

Generic GCC/binutils source-build prerequisites:

Linux:

```sh
sudo apt install -y build-essential bison flex texinfo gawk libgmp-dev libmpfr-dev libmpc-dev libisl-dev
```

macOS:

```sh
brew install gcc binutils gmp mpfr libmpc isl bison flex texinfo
```

Windows:

Use MSYS2 UCRT64:

```sh
pacman -S --needed \
  base-devel \
  git \
  texinfo \
  bison \
  flex \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-gmp \
  mingw-w64-ucrt-x86_64-mpfr \
  mingw-w64-ucrt-x86_64-mpc \
  mingw-w64-ucrt-x86_64-isl
```

Generic source build layout:

```sh
mkdir -p $HOME/src $HOME/cross/fantasy-elf
cd $HOME/src
curl -LO https://ftp.gnu.org/gnu/binutils/binutils-2.43.1.tar.xz
curl -LO https://ftp.gnu.org/gnu/gcc/gcc-14.2.0/gcc-14.2.0.tar.xz
tar xf binutils-2.43.1.tar.xz
tar xf gcc-14.2.0.tar.xz
```

After Fantasy-specific patches exist, the shape would be:

```sh
mkdir build-binutils
cd build-binutils
../binutils-2.43.1/configure \
  --target=fantasy-elf \
  --prefix=$HOME/cross/fantasy-elf \
  --disable-nls \
  --disable-werror
make -j$(nproc)
make install

cd ..
mkdir build-gcc
cd build-gcc
../gcc-14.2.0/configure \
  --target=fantasy-elf \
  --prefix=$HOME/cross/fantasy-elf \
  --enable-languages=c \
  --without-headers \
  --disable-nls \
  --disable-shared \
  --disable-threads \
  --disable-libssp \
  --disable-libquadmath \
  --disable-libgomp \
  --disable-multilib
make all-gcc all-target-libgcc -j$(nproc)
make install-gcc install-target-libgcc
```

On macOS, replace `$(nproc)` with `$(sysctl -n hw.ncpu)`. On Windows/MSYS2, `$(nproc)` is available in the MSYS2 shell.

Until those Fantasy target patches exist, use `mfasm.py` for production ROMs.

## Arm AArch64 Cross-Toolchain for Raspberry Pi Kernel Builds

This toolchain builds `kernel8.img` for Raspberry Pi. It is different from the Fantasy ROM toolchain.

Download the `aarch64-none-elf` Arm GNU Toolchain package for your host from:

https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads

Select the package for your host OS:

| Host | Toolchain flavor |
| --- | --- |
| Windows | `mingw-w64-i686-aarch64-none-elf` |
| Linux x86_64 | `x86_64-aarch64-none-elf` |
| macOS Intel | Darwin x86_64 AArch64 bare-metal package if available |
| macOS Apple Silicon | Darwin arm64 AArch64 bare-metal package if available |

Add the toolchain `bin` directory to `PATH`.

Windows PowerShell example:

```powershell
$env:PATH = "C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-aarch64-none-elf\bin;$env:PATH"
aarch64-none-elf-gcc --version
```

Linux example:

```sh
tar xf arm-gnu-toolchain-*-x86_64-aarch64-none-elf.tar.xz -C $HOME/opt
TOOLCHAIN_DIR=$(find "$HOME/opt" -maxdepth 1 -type d -name 'arm-gnu-toolchain-*aarch64-none-elf' | head -n 1)
export PATH="$TOOLCHAIN_DIR/bin:$PATH"
aarch64-none-elf-gcc --version
```

macOS example:

```sh
tar xf arm-gnu-toolchain-*-darwin-*-aarch64-none-elf.tar.xz -C $HOME/opt
TOOLCHAIN_DIR=$(find "$HOME/opt" -maxdepth 1 -type d -name 'arm-gnu-toolchain-*aarch64-none-elf' | head -n 1)
export PATH="$TOOLCHAIN_DIR/bin:$PATH"
aarch64-none-elf-gcc --version
```

## Build Circle for Raspberry Pi 4

Clone Circle next to `fantasy-pi`:

```sh
cd /path/to/parent
git clone https://github.com/rsta2/circle.git circle
```

Expected layout:

```text
parent/
  fantasy-pi/
  circle/
```

### Linux/macOS/Git Bash/MSYS2

Make sure `aarch64-none-elf-gcc` is in `PATH`.

```sh
cd /path/to/parent/circle
./configure -r 4 -p aarch64-none-elf- -f
./makeall
make -C addon/linux
make -C addon/vc4/vchiq
make -C addon/vc4/sound
```

The important output libraries are:

```text
lib/libcircle.a
lib/usb/libusb.a
lib/input/libinput.a
lib/fs/libfs.a
lib/fs/fat/libfatfs.a
lib/sched/libsched.a
lib/sound/libsound.a
addon/linux/liblinuxemu.a
addon/vc4/vchiq/libvchiq.a
addon/vc4/sound/libvchiqsound.a
```

### Windows PowerShell without Bash

If `configure` and `makeall` are not available in your shell, create `Config.mk` manually:

```powershell
cd C:\path\to\circle
@"
PREFIX64 = aarch64-none-elf-
AARCH = 64
RASPPI = 4
"@ | Set-Content Config.mk -Encoding ASCII
```

Then build the required directories. Put the Arm toolchain, Git Unix tools, and Make in `PATH`:

```powershell
$make = "C:\path\to\mingw64\bin\mingw32-make.exe"
$env:PATH = "C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-aarch64-none-elf\bin;C:\Program Files\Git\usr\bin;$env:PATH"

$dirs = @(
  "tools",
  "lib",
  "lib\usb",
  "lib\usb\gadget",
  "lib\input",
  "lib\fs",
  "lib\fs\fat",
  "lib\sched",
  "lib\net",
  "lib\sound",
  "addon\linux",
  "addon\vc4\vchiq",
  "addon\vc4\sound"
)

foreach ($dir in $dirs) {
  Push-Location $dir
  & $make
  if ($LASTEXITCODE -ne 0) { throw "Circle build failed in $dir" }
  Pop-Location
}
```

For Raspberry Pi 5, use `RASPPI = 5` and rebuild Circle. The current `build/make/Makefile` is tuned for Raspberry Pi 4 (`cortex-a72`); update its CPU flags before producing a Pi 5 kernel.

## Build the Raspberry Pi Kernel

The kernel build expects Circle next to the repository:

```text
parent/
  fantasy-pi/
  circle/
```

If Circle lives somewhere else, pass `CIRCLE_HOME=/path/to/circle`.

Windows PowerShell:

```powershell
cd C:\path\to\fantasy-pi\build\make
$env:PATH = "C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-aarch64-none-elf\bin;C:\Program Files\Git\usr\bin;$env:PATH"
C:\path\to\mingw64\bin\mingw32-make.exe clean
C:\path\to\mingw64\bin\mingw32-make.exe
```

Linux/macOS:

```sh
cd /path/to/fantasy-pi/build/make
make clean
make
```

Output:

```text
build/make/kernel8.img
build/make/kernel.elf
build/make/kernel.lst
build/make/cartridge.bin
```

## SD Card Boot Files

Format the boot partition as FAT32 and copy:

```text
kernel8.img
config.txt
start4.elf
fixup4.dat
```

You can get Raspberry Pi firmware files from a Raspberry Pi OS boot partition or from the official firmware repository:

https://github.com/raspberrypi/firmware/tree/master/boot

Minimal `config.txt` for Raspberry Pi 4:

```ini
arm_64bit=1
kernel=kernel8.img
disable_overscan=1
hdmi_force_hotplug=1
```

The kernel embeds `build/make/cartridge.bin`. You may also place a `cartridge.bin` on the SD card root; the kernel tries the SD-card cartridge first and falls back to the embedded cartridge.

## Clean Builds

Host emulator:

```sh
rm -rf build/host
cmake -S build/cmake -B build/host -G Ninja
cmake --build build/host
```

Windows PowerShell:

```powershell
Remove-Item -Recurse -Force build\host -ErrorAction SilentlyContinue
cmake -S build\cmake -B build\host -G Ninja
cmake --build build\host
```

Kernel:

```sh
make -C build/make clean
make -C build/make
```

Circle:

```sh
cd ../circle
make -C lib clean
make -C lib/usb clean
make -C lib/input clean
make -C lib/fs clean
make -C lib/fs/fat clean
make -C lib/sched clean
make -C lib/sound clean
make -C addon/linux clean
make -C addon/vc4/vchiq clean
make -C addon/vc4/sound clean
```

## Troubleshooting

### Emulator builds but exits immediately on Windows

Make sure the runtime DLLs are next to `fantasy_emulator.exe`:

- `SDL2.dll`
- MinGW runtime DLLs such as `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`, `libmcfgthread-2.dll`

The CMake build attempts to copy these automatically for MinGW builds.

### Emulator is headless

CMake did not find SDL2. Install SDL2 and reconfigure from a clean `build/host` directory.

### ROM builds with dummy assets

Install Pillow and NumPy:

```sh
python3 -m pip install Pillow numpy
```

or on Windows:

```powershell
py -3 -m pip install Pillow numpy
```

### Circle Makefile cannot find `cp`, `rm`, or `wc` on Windows

Use MSYS2 or put Git for Windows Unix tools in `PATH`:

```powershell
$env:PATH = "C:\Program Files\Git\usr\bin;$env:PATH"
```

### Kernel link errors mention `_etext`, `__tbss_start`, or `main`

Use `build/make/Makefile` from this repository. It links with Circle's `circle.ld`, adds `kernel/src/main.cpp`, and groups static libraries with `--start-group`.

### Kernel link errors mention `__aarch64_*`

Link with `libgcc.a` from the same Arm GNU Toolchain used to compile the objects. The current `build/make/Makefile` resolves this automatically via:

```make
$(CXX) -print-libgcc-file-name
```
