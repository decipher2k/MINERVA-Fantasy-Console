# Fantasy Pi Instruction Set Architecture (ISA)

## Instruction Format

All instructions are 32-bit fixed width, little-endian.

```
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
|  Opcode (6)  |  Cond (4)  |  Dst (4)  |  SrcA (4)  |  SrcB (4)  |         Imm10 (10)        |
|  Opcode (6)  |  Cond (4)  |  Dst (4)  |  SrcA (4)  |  SrcB (4)  |  Func (4)  |   Shift (4)   |
|  Opcode (6)  |  Cond (4)  |            Immediate / Address (22)                             |
```

## Condition Codes

| Code | Mnemonic | Condition          |
|------|----------|--------------------|
| 0x0  | al       | Always             |
| 0x1  | eq       | Equal (Z=1)        |
| 0x2  | ne       | Not equal (Z=0)    |
| 0x3  | gt       | Greater (N=0,Z=0)  |
| 0x4  | lt       | Less (N=1)         |
| 0x5  | ge       | Greater or equal   |
| 0x6  | le       | Less or equal      |
| 0x7  | cs       | Carry set          |
| 0x8  | cc       | Carry clear        |
| 0x9  | mi       | Negative           |
| 0xA  | pl       | Positive or zero   |
| 0xB  | vs       | Overflow set       |
| 0xC  | vc       | Overflow clear     |

## Opcodes

| Opcode | Mnemonic | Type           | Description                              |
|--------|----------|----------------|------------------------------------------|
| 0x00   | nop      | System         | No operation                             |
| 0x01   | load     | Data           | Load immediate: `dst = imm`              |
| 0x02   | mov      | Data           | Move register: `dst = src`               |
| 0x03   | add      | ALU            | `dst = srcA + srcB`                      |
| 0x04   | sub      | ALU            | `dst = srcA - srcB`                      |
| 0x05   | mul      | ALU            | `dst = srcA * srcB` (lower 32 bits)      |
| 0x06   | div      | ALU            | `dst = srcA / srcB` (unsigned)           |
| 0x07   | mod      | ALU            | `dst = srcA % srcB` (unsigned)           |
| 0x08   | and      | ALU            | `dst = srcA & srcB`                      |
| 0x09   | or       | ALU            | `dst = srcA \| srcB`                     |
| 0x0A   | xor      | ALU            | `dst = srcA ^ srcB`                      |
| 0x0B   | not      | ALU            | `dst = ~srcA`                            |
| 0x0C   | shl      | ALU            | `dst = srcA << srcB`                     |
| 0x0D   | shr      | ALU            | `dst = srcA >> srcB` (logical)           |
| 0x0E   | sar      | ALU            | `dst = srcA >> srcB` (arithmetic)        |
| 0x0F   | cmp      | ALU            | Compare, set flags (srcA - srcB)         |
| 0x10   | ld       | Memory         | Load word: `dst = mem[srcA + imm]`       |
| 0x11   | st       | Memory         | Store word: `mem[srcA + imm] = srcB`     |
| 0x12   | ldb      | Memory         | Load byte (zero-extend)                  |
| 0x13   | stb      | Memory         | Store byte                               |
| 0x14   | ldh      | Memory         | Load halfword (zero-extend)              |
| 0x15   | sth      | Memory         | Store halfword                           |
| 0x16   | push     | Stack          | Push register to stack                   |
| 0x17   | pop      | Stack          | Pop register from stack                  |
| 0x18   | jmp      | Control        | Jump to address: `pc = imm`              |
| 0x19   | jeq      | Control        | Jump if equal                            |
| 0x1A   | jne      | Control        | Jump if not equal                        |
| 0x1B   | jgt      | Control        | Jump if greater                          |
| 0x1C   | jlt      | Control        | Jump if less                             |
| 0x1D   | call     | Control        | Call subroutine                          |
| 0x1E   | ret      | Control        | Return from subroutine                   |
| 0x1F   | trap     | System         | System trap / syscall                    |

## Addressing Modes

### Register Direct
```asm
add r0, r1, r2      ; r0 = r1 + r2
```

### Immediate
```asm
load r0, #42        ; r0 = 42
load r0, #0xFF00    ; r0 = 0xFF00
```

### Register Indirect
```asm
ld r0, [r1, #4]     ; r0 = mem[r1 + 4]
st r0, [r1, #4]     ; mem[r1 + 4] = r0
```

### PC-Relative
```asm
jmp label           ; pc = label address
call function       ; lr = pc+4, pc = function address
```

## Flag Register

| Bit | Name | Description          |
|-----|------|----------------------|
| 0   | Z    | Zero flag            |
| 1   | N    | Negative flag        |
| 2   | C    | Carry flag           |
| 3   | V    | Overflow flag        |

## Trap Vectors

| Vector | Name              | Description                          | Arguments                     |
|--------|-------------------|--------------------------------------|-------------------------------|
| 0x00   | TRAP_HALT         | Halt VM execution                    | -                             |
| 0x01   | TRAP_SLEEP        | Sleep for N milliseconds             | r0 = ms                       |
| 0x10   | GFX_INIT          | Initialize graphics                  | r0 = width, r1 = height       |
| 0x11   | GFX_PRESENT       | Present frame (vsync)                | -                             |
| 0x12   | GFX_CLEAR         | Clear screen                         | r0 = color                    |
| 0x13   | GFX_DRAW_SPRITE   | Draw sprite                          | r0=id, r1=x, r2=y, r3=blend   |
| 0x14   | GFX_DRAW_IMAGE    | Draw image (full surface)            | r0=id, r1=x, r2=y, r3=blend   |
| 0x15   | GFX_BITBLT        | BitBlt rectangle                     | r0=id, r1..r6=rect/blend      |
| 0x16   | GFX_DRAW_TILEMAP  | Draw tilemap                         | r0=id, r1=scrollX, r2=scrollY |
| 0x17   | GFX_SET_PALETTE   | Set palette entry                    | r0=palette, r1=index, r2=color|
| 0x18   | GFX_DRAW_TEXT     | Draw text                            | r0=font, r1=str_addr, r2=x,y  |
| 0x20   | AUDIO_INIT        | Initialize audio                     | -                             |
| 0x21   | AUDIO_PLAY        | Play sound/channel                   | r0=sample, r1=channel, r2=vol |
| 0x22   | AUDIO_STOP        | Stop channel                         | r0 = channel                  |
| 0x30   | INPUT_POLL        | Poll input state                     | -                             |
| 0x31   | INPUT_KEY         | Get key state                        | r0 = keycode                  |
| 0x32   | INPUT_GAMEPAD     | Get gamepad state                    | r0 = player                   |
| 0x40   | MEM_COPY          | Copy memory                          | r0=src, r1=dst, r2=len        |
| 0x41   | MEM_FILL          | Fill memory                          | r0=dst, r1=value, r2=len      |
| 0x50   | MATH_RAND         | Random number                        | r0 = seed / result            |
| 0x51   | MATH_SIN          | Fixed-point sine                     | r0 = angle (0-65535)          |
| 0x52   | MATH_COS          | Fixed-point cosine                   | r0 = angle (0-65535)          |
| 0x60   | DEBUG_LOG         | Log integer to debug output          | r0 = value                    |
| 0x61   | DEBUG_LOG_STR     | Log string to debug output           | r0 = string address           |

## Assembly Syntax

```asm
; Comments start with semicolon

.section code
start:
    load r0, #0
    load sp, #0x000FFFFC
    call main
    trap TRAP_HALT

main:
    push lr
    load r0, #640
    load r1, #480
    trap GFX_INIT
    pop lr
    ret

.section data
my_var:
    .word 0x12345678

my_string:
    .asciiz "Hello, World!"
```
