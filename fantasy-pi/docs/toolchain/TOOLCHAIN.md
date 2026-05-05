# GCC / Binutils Port Plan

## Overview

Porting GCC to a new target is a significant undertaking. This document outlines the pragmatic path from macroassembler to full C compiler support.

## Phase 1: Macroassembler (COMPLETE)

The current `mfasm.py` provides:
- Labels, macros, includes
- Asset embedding
- Conditional assembly
- ROM generation

This is sufficient for game development and proves the platform.

## Phase 2: C-to-Fantasy-ASM Transpiler (RECOMMENDED NEXT STEP)

Instead of a full GCC backend, write a C frontend that generates `.fasm`:

```c
int add(int a, int b) {
    return a + b;
}
```

Generates:
```asm
add:
    add r0, r0, r1
    ret
```

### Implementation Strategy

Use `clang -emit-llvm` to generate LLVM IR, then write an LLVM IR -> Fantasy ASM backend:

```bash
clang -target fantasy-pi -S -emit-llvm input.c -o input.ll
llc -march=fantasy input.ll -o output.fasm
```

**Advantage**: Reuses Clang frontend (C11/C17 parser, optimizations, diagnostics)
**Disadvantage**: Still requires writing an LLVM backend (significant but well-documented)

## Phase 3: Minimal GCC Port (LONG TERM)

If a native GCC toolchain is desired:

### Required Components

1. **BFD Target** (`bfd/fantasy.c`)
   - Define architecture constants
   - Object file format (use ELF32)
   - Relocation types

2. **GAS Backend** (`gas/config/tc-fantasy.c`)
   - Parse Fantasy ASM syntax
   - Generate object code
   - Handle relocations

3. **LD Linker Script**
   - Define ROM layout
   - Place sections (.text, .data, .rodata, .assets)
   - Generate final ROM image

4. **GCC Backend** (`gcc/config/fantasy/`)
   - Register definitions
   - Instruction patterns (`.md` files)
   - Calling convention
   - ABI definition

5. **libgcc**
   - 32-bit division/modulo routines
   - Multiplication helpers
   - Floating point emulation (if needed)

6. **Newlib or Picolibc**
   - Minimal C standard library
   - `printf`, `malloc`, string functions

### Calling Convention

```
r0-r3: Argument / return registers (caller-saved)
r4-r11: Callee-saved registers
r12 (ip): Intra-procedure call temp
r13 (sp): Stack pointer
r14 (lr): Link register
r15 (pc): Program counter

Stack frame:
    [sp+0]  saved lr
    [sp+4]  saved r4
    ...     saved registers
    [sp+N]  local variables
    sp points to lowest used address
```

### Pragmatic GCC Patch Scope

Estimated effort for minimal GCC 13 port:
- BFD target: 200 lines (adapt from existing small target)
- GAS backend: 800 lines (simpler than ARM due to fixed 32-bit encoding)
- GCC backend: 3000-5000 lines (register allocator, patterns, built-ins)
- libgcc: 500 lines (division, multiplication)
- Linker script: 100 lines

Total: ~2-3 months full-time for experienced GCC developer.

### Alternative: Reuse Binutils, Custom Compiler

Skip GCC backend entirely:
1. Use `clang` + custom LLVM backend (Phase 2)
2. Or write custom C compiler in Python/C++ targeting `.fasm`
3. Use `mfasm` for final assembly and ROM generation

This reduces scope from months to weeks.

## Phase 4: Standard Library

Even with a C compiler, the standard library must be tailored:

- No file system (unless SD card FAT)
- No process model
- Custom memory allocator over VM RAM
- Custom `printf` writing to debug UART or screen

Recommended: Port `picolibc` (https://github.com/picolibc/picolibc) with custom syscalls.

## Recommended Path

```
Now      -> mfasm.py (complete, use for games)
Week 10  -> LLVM IR -> fasm backend (enables C development)
Week 20  -> Evaluate if GCC port is worth the effort
Month 6+ -> GCC port if community demands it
```

## References

- GCC Internals: https://gcc.gnu.org/onlinedocs/gccint/
- LLVM Backend Tutorial: https://llvm.org/docs/WritingAnLLVMBackend.html
- Binutils Port: https://sourceware.org/binutils/docs/binutils/
- Picolibc: https://github.com/picolibc/picolibc
