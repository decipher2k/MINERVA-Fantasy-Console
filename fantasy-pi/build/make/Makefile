#
# Makefile for Fantasy Pi Bare-Metal Kernel (Raspberry Pi 4/5)
#
# Invoke from this directory with mingw32-make, or from the project root with:
#   mingw32-make -C build/make
#

RASPPI ?= 5
PREFIX ?= aarch64-none-elf-
PYTHON ?= $(PROJECT_ROOT)/tools/python/python.exe
TOOLCHAIN_DIR ?= $(PROJECT_ROOT)/toolchain/aarch64-none-elf/bin

PROJECT_ROOT := ../..
CIRCLE_HOME ?= $(PROJECT_ROOT)/circle

NEEDS_CIRCLE := $(if $(MAKECMDGOALS),$(filter all kernel8.img kernel.elf,$(MAKECMDGOALS)),yes)
ifneq ($(NEEDS_CIRCLE),)
ifeq ($(wildcard $(CIRCLE_HOME)/include/circle/actled.h),)
$(error Missing Circle headers. Expected $(CIRCLE_HOME)/include/circle/actled.h. Set CIRCLE_HOME=... or clone Circle next to fantasy-pi)
endif
ifeq ($(wildcard $(CIRCLE_HOME)/lib/libcircle.a),)
$(error Missing Circle library. Build Circle first for RASPPI=$(RASPPI): configure -p $(RASPPI), then make)
endif
endif

ifeq ($(strip $(TOOLCHAIN_DIR)),)
TOOL_PREFIX :=
TOOL_SUFFIX :=
else
TOOL_PREFIX := $(TOOLCHAIN_DIR)/
TOOL_SUFFIX := .exe
endif

CC := "$(TOOL_PREFIX)$(PREFIX)gcc$(TOOL_SUFFIX)"
CXX := "$(TOOL_PREFIX)$(PREFIX)g++$(TOOL_SUFFIX)"
AS := "$(TOOL_PREFIX)$(PREFIX)as$(TOOL_SUFFIX)"
OBJCOPY := "$(TOOL_PREFIX)$(PREFIX)objcopy$(TOOL_SUFFIX)"
OBJDUMP := "$(TOOL_PREFIX)$(PREFIX)objdump$(TOOL_SUFFIX)"

OBJDIR := obj

CFLAGS := -march=armv8-a+crc -mtune=cortex-a72 -mgeneral-regs-only \
          -O2 -Wall -fsigned-char -ffreestanding \
          -D__circle__ -DRASPPI=$(RASPPI)

CXXFLAGS := $(CFLAGS) -fno-exceptions -fno-rtti -std=c++17
ASFLAGS := -mcpu=cortex-a72

LDSCRIPT := $(CIRCLE_HOME)/circle.ld
LDFLAGS := -nostdlib -Wl,--build-id=none -Wl,--section-start=.init=0x80000
LIBGCC := "$(shell $(CXX) $(CFLAGS) -print-libgcc-file-name)"
CRTBEGIN := "$(shell $(CXX) $(CFLAGS) -print-file-name=crtbegin.o)"
CRTEND := "$(shell $(CXX) $(CFLAGS) -print-file-name=crtend.o)"

INCLUDES := -I$(PROJECT_ROOT)/kernel/include \
            -I$(PROJECT_ROOT)/vm/include \
            -I$(PROJECT_ROOT)/renderer/include \
            -I$(CIRCLE_HOME)/include \
            -I$(CIRCLE_HOME)/addon

KERNEL_SRCS := $(PROJECT_ROOT)/kernel/src/kernel.cpp \
               $(PROJECT_ROOT)/kernel/src/main.cpp
VM_SRCS := $(PROJECT_ROOT)/vm/src/fantasy_vm.cpp
RENDERER_SRCS := $(PROJECT_ROOT)/renderer/src/renderer.cpp \
                 $(PROJECT_ROOT)/renderer/src/audio_mixer.cpp

OBJS := $(OBJDIR)/kernel.o \
        $(OBJDIR)/main.o \
        $(OBJDIR)/fantasy_vm.o \
        $(OBJDIR)/renderer.o \
        $(OBJDIR)/audio_mixer.o

CIRCLE_LIBS := $(CIRCLE_HOME)/addon/linux/liblinuxemu.a \
               $(CIRCLE_HOME)/addon/SDCard/libsdcard.a \
               $(CIRCLE_HOME)/lib/sched/libsched.a \
               $(CIRCLE_HOME)/lib/sound/libsound.a \
               $(CIRCLE_HOME)/lib/usb/libusb.a \
               $(CIRCLE_HOME)/lib/input/libinput.a \
               $(CIRCLE_HOME)/lib/fs/fat/libfatfs.a \
               $(CIRCLE_HOME)/lib/fs/libfs.a \
               $(CIRCLE_HOME)/lib/libcircle.a

GAME_SRC := $(PROJECT_ROOT)/input_asm/src/main.fasm
ASSET_DEPS := $(wildcard $(PROJECT_ROOT)/input_asm/assets/*.png) \
              $(wildcard $(PROJECT_ROOT)/input_asm/assets/*.wav) \
              $(wildcard $(PROJECT_ROOT)/input_asm/assets/*.json)

all: kernel8.img

kernel8.img: kernel.elf
	$(OBJCOPY) kernel.elf -O binary kernel8.img
	$(OBJDUMP) -d kernel.elf > kernel.lst

kernel.elf: $(OBJS) cartridge.o $(LDSCRIPT) $(CIRCLE_LIBS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -T $(LDSCRIPT) $(CRTBEGIN) $(OBJS) cartridge.o \
		-Wl,--start-group $(CIRCLE_LIBS) $(LIBGCC) -Wl,--end-group $(CRTEND) -o $@

cartridge.bin: $(GAME_SRC) $(ASSET_DEPS)
	$(PYTHON) $(PROJECT_ROOT)/macroassembler/src/mfasm.py $(GAME_SRC) -o $@ \
		-I $(PROJECT_ROOT)/input_asm/src \
		-I $(PROJECT_ROOT)/input_asm/assets

cartridge.o: cartridge.bin
	$(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 \
		--rename-section .data=.rom \
		cartridge.bin $@

$(OBJDIR):
	powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(OBJDIR)' | Out-Null"

$(OBJDIR)/kernel.o: $(PROJECT_ROOT)/kernel/src/kernel.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR)/main.o: $(PROJECT_ROOT)/kernel/src/main.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR)/fantasy_vm.o: $(PROJECT_ROOT)/vm/src/fantasy_vm.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR)/renderer.o: $(PROJECT_ROOT)/renderer/src/renderer.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR)/audio_mixer.o: $(PROJECT_ROOT)/renderer/src/audio_mixer.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	-powershell -NoProfile -Command "Remove-Item -Recurse -Force '$(OBJDIR)' -ErrorAction SilentlyContinue"
	-powershell -NoProfile -Command "Remove-Item -Force cartridge.o,cartridge.bin,kernel.elf,kernel8.img,kernel.lst -ErrorAction SilentlyContinue"

test:
	cd $(PROJECT_ROOT) && cmake -S build/cmake -B build/host && cmake --build build/host && ctest --test-dir build/host --output-on-failure

deploy: kernel8.img
	@echo Copy kernel8.img to the SD card boot partition.

.PHONY: all clean test deploy
