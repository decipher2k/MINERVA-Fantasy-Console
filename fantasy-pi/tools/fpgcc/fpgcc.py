#!/usr/bin/env python3
"""
fpgcc.py - GCC-based Fantasy Pi C/C++ to FASM compiler driver.

This tool deliberately uses GCC as the frontend gate: it preprocesses and
syntax-checks project sources with the generated Fantasy Pi SDK headers, then
lowers a small, IDE-friendly C/C++ gameplay subset to Fantasy Pi .fasm.

It is not a GCC backend. The custom Fantasy Pi VM ISA is not a GCC target, so a
true full C++ compiler would require a large backend/runtime project. This
driver provides the practical crosscompiler path used by the IDE today:

    C/C++ source + SDK headers -> .fasm -> mfasm.py -> .rom
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from typing import Dict, Iterable, List, Optional, Sequence, Tuple


TOOL_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(TOOL_DIR, "..", ".."))
SDK_INCLUDE = os.path.join(PROJECT_ROOT, "sdk", "include")
PYTHON = os.path.join(PROJECT_ROOT, "tools", "python", "python.exe")
if not os.path.exists(PYTHON):
    PYTHON = sys.executable


API_TRAPS = {
    "GFX_INIT": "GFX_INIT",
    "GFX_PRESENT": "GFX_PRESENT",
    "GFX_CLEAR": "GFX_CLEAR",
    "GFX_DRAW_SPRITE": "GFX_DRAW_SPRITE",
    "GFX_DRAW_IMAGE": "GFX_DRAW_IMAGE",
    "GFX_BITBLT": "GFX_BITBLT",
    "GFX_DRAW_TILEMAP": "GFX_DRAW_TILEMAP",
    "GFX_SET_PALETTE": "GFX_SET_PALETTE",
    "GFX_DRAW_TEXT": "GFX_DRAW_TEXT",
    "AUDIO_INIT": "AUDIO_INIT",
    "AUDIO_PLAY": "AUDIO_PLAY",
    "AUDIO_STOP": "AUDIO_STOP",
    "INPUT_POLL": "INPUT_POLL",
    "INPUT_KEY": "INPUT_KEY",
    "INPUT_GAMEPAD": "INPUT_GAMEPAD",
    "MATH_RAND": "MATH_RAND",
    "MATH_SIN": "MATH_SIN",
    "MATH_COS": "MATH_COS",
    "DEBUG_LOG": "DEBUG_LOG",
    "DEBUG_LOG_STR": "DEBUG_LOG_STR",
}

BUILTIN_DEFINES = {
    "true": 1,
    "false": 0,
    "nullptr": 0,
    "BLEND_COPY": 0,
    "BLEND_ALPHA": 1,
    "BLEND_ADDITIVE": 2,
    "BLEND_MULTIPLY": 3,
    "BLEND_COLORKEY": 4,
    "BLEND_MASK": 5,
    "fe::BLEND_COPY": 0,
    "fe::BLEND_ALPHA": 1,
    "fe::BLEND_ADDITIVE": 2,
    "fe::BLEND_MULTIPLY": 3,
    "fe::BLEND_COLORKEY": 4,
    "fe::BLEND_MASK": 5,
    "fe::BTN_UP": 1 << 0,
    "fe::BTN_DOWN": 1 << 1,
    "fe::BTN_LEFT": 1 << 2,
    "fe::BTN_RIGHT": 1 << 3,
    "fe::BTN_A": 1 << 4,
    "fe::BTN_B": 1 << 5,
    "fe::BTN_START": 1 << 6,
    "fe::BTN_SELECT": 1 << 7,
    "fe::AXIS_LX": 0,
    "fe::AXIS_LY": 1,
    "fe::AXIS_RX": 2,
    "fe::AXIS_RY": 3,
}


class CompileError(Exception):
    pass


@dataclass
class Function:
    name: str
    args: str
    body: str


@dataclass
class CompilerState:
    defines: Dict[str, int] = field(default_factory=lambda: dict(BUILTIN_DEFINES))
    globals: Dict[str, str] = field(default_factory=dict)
    locals: Dict[str, str] = field(default_factory=dict)
    strings: Dict[str, str] = field(default_factory=dict)
    warnings: List[str] = field(default_factory=list)
    label_counter: int = 0
    current_function: str = ""

    def new_label(self, prefix: str) -> str:
        self.label_counter += 1
        return f"__{prefix}_{self.label_counter}"

    def storage_for(self, name: str) -> Optional[str]:
        if name in self.locals:
            return self.locals[name]
        if name in self.globals:
            return name
        return None


def run(cmd: Sequence[str], cwd: Optional[str] = None, check: bool = True) -> subprocess.CompletedProcess:
    result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if check and result.returncode != 0:
        raise CompileError((result.stdout or "") + (result.stderr or ""))
    return result


def find_gcc(cxx: bool = True) -> Optional[str]:
    names = ["g++", "gcc"] if cxx else ["gcc", "g++"]
    for name in names:
        found = shutil.which(name)
        if found:
            return found

    bundled = os.path.join(PROJECT_ROOT, "toolchain", "aarch64-none-elf", "bin", "aarch64-none-elf-g++.exe")
    if cxx and os.path.exists(bundled):
        return bundled
    bundled = os.path.join(PROJECT_ROOT, "toolchain", "aarch64-none-elf", "bin", "aarch64-none-elf-gcc.exe")
    return bundled if os.path.exists(bundled) else None


def strip_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)
    source = re.sub(r"//.*", "", source)
    return source


def split_args(text: str) -> List[str]:
    args: List[str] = []
    cur = []
    depth = 0
    in_string = False
    quote = ""
    for ch in text:
        if in_string:
            cur.append(ch)
            if ch == quote:
                in_string = False
            continue
        if ch in ("'", '"'):
            in_string = True
            quote = ch
            cur.append(ch)
            continue
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        if ch == "," and depth == 0:
            args.append("".join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    tail = "".join(cur).strip()
    if tail:
        args.append(tail)
    return args


def split_statements(body: str) -> List[str]:
    stmts: List[str] = []
    cur = []
    depth = 0
    paren_depth = 0
    in_string = False
    quote = ""
    i = 0
    while i < len(body):
        ch = body[i]
        if in_string:
            cur.append(ch)
            if ch == quote:
                in_string = False
            i += 1
            continue
        if ch in ("'", '"'):
            in_string = True
            quote = ch
            cur.append(ch)
            i += 1
            continue
        if ch == "{":
            depth += 1
            cur.append(ch)
            i += 1
            continue
        if ch == "(":
            paren_depth += 1
            cur.append(ch)
            i += 1
            continue
        if ch == ")":
            paren_depth -= 1
            cur.append(ch)
            i += 1
            continue
        if ch == "}":
            depth -= 1
            cur.append(ch)
            if depth == 0:
                stmts.append("".join(cur).strip())
                cur = []
            i += 1
            continue
        if ch == ";" and depth == 0 and paren_depth == 0:
            cur.append(ch)
            stmts.append("".join(cur).strip())
            cur = []
            i += 1
            continue
        cur.append(ch)
        i += 1
    tail = "".join(cur).strip()
    if tail:
        stmts.append(tail)
    return stmts


def find_matching(text: str, start: int, open_ch: str, close_ch: str) -> int:
    depth = 0
    in_string = False
    quote = ""
    for i in range(start, len(text)):
        ch = text[i]
        if in_string:
            if ch == quote:
                in_string = False
            continue
        if ch in ("'", '"'):
            in_string = True
            quote = ch
            continue
        if ch == open_ch:
            depth += 1
        elif ch == close_ch:
            depth -= 1
            if depth == 0:
                return i
    raise CompileError(f"Unmatched {open_ch}")


def extract_functions(source: str) -> Tuple[List[Function], str]:
    functions: List[Function] = []
    pattern = re.compile(
        r"(?:(?:extern\s+\"C\"\s+)?)"
        r"(?:static\s+|inline\s+|constexpr\s+)*"
        r"(?:void|int|int32_t|uint32_t|bool|auto)\s+"
        r"([A-Za-z_]\w*)\s*\(([^)]*)\)\s*\{",
        re.M,
    )
    spans: List[Tuple[int, int]] = []
    pos = 0
    while True:
        m = pattern.search(source, pos)
        if not m:
            break
        start_brace = source.find("{", m.end() - 1)
        end_brace = find_matching(source, start_brace, "{", "}")
        functions.append(Function(m.group(1), m.group(2), source[start_brace + 1:end_brace]))
        spans.append((m.start(), end_brace + 1))
        pos = end_brace + 1

    remainder = []
    last = 0
    for start, end in spans:
        remainder.append(source[last:start])
        last = end
    remainder.append(source[last:])
    return functions, "\n".join(remainder)


def load_defines(paths: Iterable[str], state: CompilerState) -> None:
    define_re = re.compile(r"^\s*#\s*define\s+([A-Za-z_]\w*)\s+(.+?)\s*$")
    for path in paths:
        if not os.path.exists(path):
            continue
        with open(path, "r", encoding="utf-8") as f:
            for line in f:
                m = define_re.match(line)
                if not m:
                    continue
                name, value = m.group(1), m.group(2).split("//", 1)[0].strip()
                if "(" in name:
                    continue
                try:
                    state.defines[name] = parse_int(value, state)
                except Exception:
                    pass


def parse_int(expr: str, state: CompilerState) -> int:
    expr = expr.strip()
    expr = expr.replace("u", "").replace("U", "").replace("l", "").replace("L", "")
    if expr in state.defines:
        return state.defines[expr]
    if expr.startswith("'") and expr.endswith("'") and len(expr) >= 3:
        return ord(expr[1])
    return int(expr, 0)


def normalize_expr(expr: str) -> str:
    expr = expr.strip()
    while expr.startswith("(") and expr.endswith(")"):
        try:
            if find_matching(expr, 0, "(", ")") == len(expr) - 1:
                expr = expr[1:-1].strip()
            else:
                break
        except CompileError:
            break
    return expr


def find_top_operator(expr: str, operators: Sequence[str]) -> Optional[Tuple[int, str]]:
    depth = 0
    in_string = False
    quote = ""
    i = len(expr) - 1
    while i >= 0:
        ch = expr[i]
        if in_string:
            if ch == quote:
                in_string = False
            i -= 1
            continue
        if ch in ("'", '"'):
            in_string = True
            quote = ch
            i -= 1
            continue
        if ch in ")]}":
            depth += 1
        elif ch in "([{":
            depth -= 1
        if depth == 0:
            for op in operators:
                start = i - len(op) + 1
                if start >= 0 and expr[start:i + 1] == op:
                    if op in ("+", "-") and start == 0:
                        continue
                    return start, op
        i -= 1
    return None


class FasmEmitter:
    def __init__(self, state: CompilerState):
        self.state = state
        self.lines: List[str] = []

    def emit(self, line: str = "") -> None:
        self.lines.append(line)

    def load_value(self, expr: str, reg: str = "r0") -> None:
        expr = normalize_expr(expr)
        if not expr:
            self.emit(f"    load {reg}, #0")
            return

        if expr.startswith('"') and expr.endswith('"'):
            label = self.string_label(expr[1:-1])
            self.emit(f"    load {reg}, {label}")
            return

        for ops in (["==", "!=", ">=", "<=", ">", "<"], ["|"], ["^"], ["&"], ["+", "-"], ["*", "/", "%"], ["<<", ">>"]):
            found = find_top_operator(expr, ops)
            if found:
                idx, op = found
                left = expr[:idx].strip()
                right = expr[idx + len(op):].strip()
                self.load_value(left, "r0")
                self.emit("    push r0")
                self.load_value(right, "r0")
                self.emit("    pop r1")
                self.emit_binary(op, "r0", "r1", "r0")
                if reg != "r0":
                    self.emit(f"    mov {reg}, r0")
                return

        if re.match(r"^[A-Za-z_][\w:]*\s*\(", expr):
            self.emit_call(expr)
            if reg != "r0":
                self.emit(f"    mov {reg}, r0")
            return

        try:
            value = parse_int(expr, self.state)
            self.emit(f"    load {reg}, #{value}")
            return
        except Exception:
            pass

        storage = self.state.storage_for(expr)
        if storage:
            self.emit(f"    load r12, {storage}")
            self.emit(f"    ld {reg}, [r12, #0]")
            return

        if expr in self.state.defines:
            self.emit(f"    load {reg}, #{self.state.defines[expr]}")
            return

        self.emit(f"    load {reg}, {expr}")

    def emit_binary(self, op: str, dst: str, left: str, right: str) -> None:
        if op == "+":
            self.emit(f"    add {dst}, {left}, {right}")
        elif op == "-":
            self.emit(f"    sub {dst}, {left}, {right}")
        elif op == "*":
            self.emit(f"    mul {dst}, {left}, {right}")
        elif op == "/":
            self.emit(f"    div {dst}, {left}, {right}")
        elif op == "%":
            self.emit(f"    mod {dst}, {left}, {right}")
        elif op == "&":
            self.emit(f"    and {dst}, {left}, {right}")
        elif op == "|":
            self.emit(f"    or {dst}, {left}, {right}")
        elif op == "^":
            self.emit(f"    xor {dst}, {left}, {right}")
        elif op == "<<":
            self.emit(f"    shl {dst}, {left}, {right}")
        elif op == ">>":
            self.emit(f"    shr {dst}, {left}, {right}")
        elif op in ("==", "!=", ">", "<", ">=", "<="):
            true_label = self.state.new_label("cmp_true")
            end_label = self.state.new_label("cmp_end")
            self.emit(f"    cmp {left}, {right}")
            jump = {
                "==": "jeq",
                "!=": "jne",
                ">": "jgt",
                "<": "jlt",
                ">=": "jlt",
                "<=": "jgt",
            }[op]
            if op in (">=", "<="):
                self.emit(f"    {jump} {end_label}")
                self.emit(f"    load {dst}, #1")
                self.emit(f"    jmp {true_label}")
                self.emit(f"{end_label}:")
                self.emit(f"    load {dst}, #0")
                self.emit(f"{true_label}:")
            else:
                self.emit(f"    {jump} {true_label}")
                self.emit(f"    load {dst}, #0")
                self.emit(f"    jmp {end_label}")
                self.emit(f"{true_label}:")
                self.emit(f"    load {dst}, #1")
                self.emit(f"{end_label}:")
        else:
            raise CompileError(f"Unsupported operator: {op}")

    def store_value(self, name: str, from_reg: str = "r0") -> None:
        storage = self.state.storage_for(name)
        if not storage:
            storage = self.make_local(name, "0")
        self.emit(f"    load r12, {storage}")
        self.emit(f"    st {from_reg}, [r12, #0]")

    def make_local(self, name: str, initial: str) -> str:
        label = f"__local_{self.state.current_function}_{name}"
        self.state.locals[name] = label
        self.state.globals[label] = initial
        return label

    def string_label(self, text: str) -> str:
        if text not in self.state.strings:
            self.state.strings[text] = f"__str_{len(self.state.strings)}"
        return self.state.strings[text]

    def emit_api_call(self, name: str, args: List[str]) -> bool:
        n = name.replace("fe::", "")
        if n == "Engine_Init":
            self.emit("    load r0, #640")
            self.emit("    load r1, #480")
            self.emit("    trap GFX_INIT")
            self.emit("    trap AUDIO_INIT")
            return True
        if n == "Engine_LoadScene":
            self.load_value(args[0] if args else "0", "r0")
            self.emit("    trap GFX_DRAW_TILEMAP")
            return True
        if n == "Engine_PollInput":
            self.emit("    trap INPUT_POLL")
            return True
        if n in ("Engine_Update", "Engine_Draw", "Audio_SetVolume"):
            self.emit(f"    ; {name} is handled by generated/runtime systems")
            return True
        if n == "Engine_PlaySFX":
            self.load_value(args[0] if len(args) > 0 else "0", "r0")
            self.load_value(args[1] if len(args) > 1 else "0", "r1")
            self.emit("    load r2, #255")
            self.emit("    trap AUDIO_PLAY")
            return True
        if n == "Gfx_Clear":
            self.load_value(args[0] if args else "0", "r0")
            self.emit("    trap GFX_CLEAR")
            return True
        if n == "Gfx_Present":
            self.emit("    trap GFX_PRESENT")
            return True
        if n == "Image_Draw":
            self.load_value(args[0] if len(args) > 0 else "0", "r0")
            self.load_value(args[1] if len(args) > 1 else "0", "r1")
            self.load_value(args[2] if len(args) > 2 else "0", "r2")
            self.load_value(args[3] if len(args) > 3 else "BLEND_COPY", "r3")
            self.emit("    trap GFX_DRAW_IMAGE")
            return True
        if n == "Sprite_Draw":
            self.load_value(args[0] if len(args) > 0 else "0", "r0")
            self.load_value(args[1] if len(args) > 1 else "0", "r1")
            self.load_value(args[2] if len(args) > 2 else "0", "r2")
            self.emit("    load r3, #BLEND_ALPHA")
            self.emit("    trap GFX_DRAW_SPRITE")
            return True
        if n == "Sprite_DrawEx":
            self.load_value(args[0] if len(args) > 0 else "0", "r0")
            self.load_value(args[1] if len(args) > 1 else "0", "r1")
            self.load_value(args[2] if len(args) > 2 else "0", "r2")
            self.load_value(args[3] if len(args) > 3 else "BLEND_ALPHA", "r3")
            self.emit("    trap GFX_DRAW_SPRITE")
            return True
        if n == "Tilemap_DrawLayer":
            self.load_value(args[0] if len(args) > 0 else "0", "r0")
            self.load_value(args[1] if len(args) > 1 else "0", "r1")
            self.load_value(args[2] if len(args) > 2 else "0", "r2")
            self.emit("    trap GFX_DRAW_TILEMAP")
            return True
        if n == "Palette_SetColor":
            self.load_value(args[0] if len(args) > 0 else "0", "r0")
            self.load_value(args[1] if len(args) > 1 else "0", "r1")
            self.load_value(args[2] if len(args) > 2 else "0", "r2")
            self.emit("    trap GFX_SET_PALETTE")
            return True
        if n == "Input_IsHeld":
            self.emit("    load r0, #0")
            self.emit("    trap INPUT_GAMEPAD")
            self.load_value(args[0] if args else "0", "r1")
            self.emit("    and r0, r0, r1")
            return True
        if n in ("Input_IsPressed", "Input_IsReleased"):
            self.emit("    load r0, #0")
            self.emit("    trap INPUT_GAMEPAD")
            self.load_value(args[0] if args else "0", "r1")
            self.emit("    and r0, r0, r1")
            return True
        if n == "Input_GamepadButtons":
            self.load_value(args[0] if args else "0", "r0")
            self.emit("    trap INPUT_GAMEPAD")
            return True
        if n == "Input_GamepadAxis":
            self.load_value(args[0] if len(args) > 0 else "0", "r0")
            self.emit("    trap INPUT_GAMEPAD")
            axis = args[1].strip() if len(args) > 1 else "0"
            if axis in ("1", "fe::AXIS_LY", "AXIS_LY"):
                self.emit("    mov r0, r2")
            else:
                self.emit("    mov r0, r1")
            return True
        if n == "Audio_Init":
            self.emit("    trap AUDIO_INIT")
            return True
        if n == "Audio_Play":
            self.load_value(args[0] if len(args) > 0 else "0", "r0")
            self.load_value(args[1] if len(args) > 1 else "0", "r1")
            self.load_value(args[2] if len(args) > 2 else "255", "r2")
            self.emit("    trap AUDIO_PLAY")
            return True
        if n == "Audio_Stop":
            self.load_value(args[0] if args else "0", "r0")
            self.emit("    trap AUDIO_STOP")
            return True
        if n == "Math_Rand":
            self.load_value(args[0] if args else "0", "r0")
            self.emit("    trap MATH_RAND")
            return True
        if n == "Math_Sin":
            self.load_value(args[0] if args else "0", "r0")
            self.emit("    trap MATH_SIN")
            return True
        if n == "Math_Cos":
            self.load_value(args[0] if args else "0", "r0")
            self.emit("    trap MATH_COS")
            return True
        if n == "fp_from_int":
            self.load_value(args[0] if args else "0", "r0")
            self.emit("    shl r0, r0, #16")
            return True
        if n == "fp_to_int":
            self.load_value(args[0] if args else "0", "r0")
            self.emit("    sar r0, r0, #16")
            return True
        if n == "fp_mul":
            self.load_value(args[0] if len(args) > 0 else "0", "r0")
            self.emit("    push r0")
            self.load_value(args[1] if len(args) > 1 else "0", "r0")
            self.emit("    pop r1")
            self.emit("    mul r0, r1, r0")
            self.emit("    sar r0, r0, #16")
            return True
        if n == "fp_div":
            self.load_value(args[0] if len(args) > 0 else "0", "r0")
            self.emit("    shl r0, r0, #16")
            self.emit("    push r0")
            self.load_value(args[1] if len(args) > 1 else "1", "r0")
            self.emit("    pop r1")
            self.emit("    div r0, r1, r0")
            return True
        if n == "Debug_Log":
            self.load_value(args[0] if args else "0", "r0")
            self.emit("    trap DEBUG_LOG")
            return True
        if n == "Debug_LogString":
            self.load_value(args[0] if args else '""', "r0")
            self.emit("    trap DEBUG_LOG_STR")
            return True
        return False

    def emit_call(self, expr: str) -> None:
        m = re.match(r"([A-Za-z_][\w:]*)\s*\((.*)\)\s*$", expr, re.S)
        if not m:
            raise CompileError(f"Invalid call expression: {expr}")
        name, arg_text = m.group(1), m.group(2)
        args = split_args(arg_text)
        if self.emit_api_call(name, args):
            return

        for idx, arg in enumerate(args[:4]):
            self.load_value(arg, f"r{idx}")
        self.emit(f"    call {name}")

    def emit_condition_jump_false(self, expr: str, false_label: str) -> None:
        expr = normalize_expr(expr)
        for op in ("==", "!=", ">=", "<=", ">", "<"):
            found = find_top_operator(expr, [op])
            if found:
                idx, _ = found
                left = expr[:idx].strip()
                right = expr[idx + len(op):].strip()
                self.load_value(left, "r0")
                self.emit("    push r0")
                self.load_value(right, "r0")
                self.emit("    pop r1")
                self.emit("    cmp r1, r0")
                false_jump = {
                    "==": "jne",
                    "!=": "jeq",
                    ">": "jlt",
                    "<": "jgt",
                    ">=": "jlt",
                    "<=": "jgt",
                }[op]
                self.emit(f"    {false_jump} {false_label}")
                return
        self.load_value(expr, "r0")
        self.emit("    cmp r0, #0")
        self.emit(f"    jeq {false_label}")


def parse_globals(source: str, state: CompilerState) -> None:
    source = re.sub(r"#.*", "", source)
    for stmt in split_statements(source):
        stmt = stmt.strip().rstrip(";")
        m = re.match(r"(?:static\s+|const\s+)?(?:int|int32_t|uint32_t|bool|uint16_t|uint8_t)\s+([A-Za-z_]\w*)\s*(?:=\s*(.+))?$", stmt)
        if m:
            name, init = m.group(1), m.group(2) or "0"
            state.globals[name] = init


def compile_block(em: FasmEmitter, body: str) -> None:
    for stmt in split_statements(body):
        compile_statement(em, stmt.strip())


def compile_statement(em: FasmEmitter, stmt: str) -> None:
    if not stmt:
        return
    stmt = stmt.strip()
    if stmt.endswith(";"):
        stmt = stmt[:-1].strip()

    m = re.match(r"while\s*\((.*?)\)\s*\{(.*)\}\s*$", stmt, re.S)
    if m:
        start = em.state.new_label("while")
        end = em.state.new_label("wend")
        cond, body = m.group(1), m.group(2)
        em.emit(f"{start}:")
        if cond.strip() not in ("true", "1"):
            em.emit_condition_jump_false(cond, end)
        compile_block(em, body)
        em.emit(f"    jmp {start}")
        em.emit(f"{end}:")
        return

    m = re.match(r"for\s*\(\s*;\s*;\s*\)\s*\{(.*)\}\s*$", stmt, re.S)
    if m:
        start = em.state.new_label("for")
        em.emit(f"{start}:")
        compile_block(em, m.group(1))
        em.emit(f"    jmp {start}")
        return

    m = re.match(r"if\s*\((.*?)\)\s*\{(.*?)\}(?:\s*else\s*\{(.*)\})?\s*$", stmt, re.S)
    if m:
        else_label = em.state.new_label("else")
        end_label = em.state.new_label("endif")
        em.emit_condition_jump_false(m.group(1), else_label)
        compile_block(em, m.group(2))
        em.emit(f"    jmp {end_label}")
        em.emit(f"{else_label}:")
        if m.group(3):
            compile_block(em, m.group(3))
        em.emit(f"{end_label}:")
        return

    if stmt.startswith("return"):
        expr = stmt[6:].strip()
        if expr:
            em.load_value(expr, "r0")
        em.emit("    pop lr")
        em.emit("    ret")
        return

    m = re.match(r"(?:int|int32_t|uint32_t|bool|uint16_t|uint8_t)\s+([A-Za-z_]\w*)\s*(?:=\s*(.+))?$", stmt)
    if m:
        name, init = m.group(1), m.group(2) or "0"
        em.make_local(name, "0")
        em.load_value(init, "r0")
        em.store_value(name, "r0")
        return

    m = re.match(r"([A-Za-z_]\w*)\s*(\+\+|--)$", stmt)
    if m:
        name, op = m.group(1), m.group(2)
        em.load_value(name, "r0")
        em.emit(f"    {'add' if op == '++' else 'sub'} r0, r0, #1")
        em.store_value(name)
        return

    m = re.match(r"([A-Za-z_]\w*)\s*([+\-*/&|]?=)\s*(.+)$", stmt, re.S)
    if m:
        name, op, expr = m.group(1), m.group(2), m.group(3)
        if op == "=":
            em.load_value(expr, "r0")
        else:
            base_op = op[0]
            em.load_value(name, "r0")
            em.emit("    push r0")
            em.load_value(expr, "r0")
            em.emit("    pop r1")
            em.emit_binary(base_op, "r0", "r1", "r0")
        em.store_value(name)
        return

    if re.match(r"[A-Za-z_][\w:]*\s*\(", stmt):
        em.emit_call(stmt)
        return

    em.state.warnings.append(f"Unsupported statement in {em.state.current_function}: {stmt}")
    em.emit(f"    ; unsupported: {stmt}")


def compile_functions(functions: List[Function], state: CompilerState) -> List[str]:
    em = FasmEmitter(state)
    em.emit("; Auto-generated by fpgcc.py. Edit the C/C++ sources, not this file.")
    em.emit(".section code")
    em.emit("start:")
    em.emit("    load sp, #0x000FFFFC")
    em.emit("    call main")
    em.emit("    trap TRAP_HALT")
    em.emit("")

    ordered = sorted(functions, key=lambda fn: 0 if fn.name == "main" else 1)
    for fn in ordered:
        state.current_function = fn.name
        state.locals = {}
        em.emit(f"{fn.name}:")
        em.emit("    push lr")
        compile_block(em, fn.body)
        em.emit("    load r0, #0")
        em.emit("    pop lr")
        em.emit("    ret")
        em.emit("")
    return em.lines


def emit_data(state: CompilerState) -> List[str]:
    lines = [".section data"]
    for name, init in state.globals.items():
        if name.startswith("__local_"):
            label = name
        else:
            label = name
        try:
            value = parse_int(init, state)
            lines.append(f"{label}: .word {value}")
        except Exception:
            if init in state.defines:
                lines.append(f"{label}: .word {state.defines[init]}")
            else:
                lines.append(f"{label}: .word 0")
    for text, label in state.strings.items():
        escaped = text.replace("\\", "\\\\").replace('"', '\\"')
        lines.append(f'{label}: .asciiz "{escaped}"')
    lines.append("")
    return lines


def collect_sources(paths: List[str]) -> List[str]:
    result: List[str] = []
    for path in paths:
        if os.path.isdir(path):
            for root, _, files in os.walk(path):
                for f in files:
                    if os.path.splitext(f)[1].lower() in (".c", ".cc", ".cpp", ".cxx"):
                        result.append(os.path.join(root, f))
        else:
            result.append(path)
    return result


def gcc_check(sources: List[str], includes: List[str], flags: List[str]) -> None:
    compiler = find_gcc(cxx=True)
    if not compiler:
        print("warning: GCC/G++ not found; skipping frontend syntax check", file=sys.stderr)
        return
    cmd = [compiler, "-fsyntax-only", "-ffreestanding", "-std=c++17", "-I", SDK_INCLUDE]
    for inc in includes:
        cmd += ["-I", inc]
    cmd += flags + sources
    run(cmd)


def emit_assets(asset_manifest: Optional[str], output: str) -> List[str]:
    if not asset_manifest or not os.path.exists(asset_manifest):
        return []

    with open(asset_manifest, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    assets = manifest.get("assets", manifest if isinstance(manifest, list) else [])
    if not assets:
        return []

    manifest_dir = os.path.dirname(os.path.abspath(asset_manifest))
    project_root = os.path.dirname(manifest_dir) if os.path.basename(manifest_dir).lower() == "assets" else manifest_dir
    output_dir = os.path.dirname(os.path.abspath(output))

    lines = [".section assets"]
    for asset in assets:
        asset_id = asset.get("id") or asset.get("name")
        source = asset.get("source") or asset.get("path") or asset.get("file")
        if not asset_id or not source:
            continue

        asset_path = source if os.path.isabs(source) else os.path.join(project_root, source)
        rel_path = os.path.relpath(asset_path, output_dir).replace("\\", "/")
        options = []
        for key in ("type", "format", "alpha", "tilew", "tileh"):
            if key in asset:
                options.append(f"{key}={asset[key]}")

        suffix = ", " + ", ".join(options) if options else ""
        lines.append(f'.asset {asset_id}, "{rel_path}"{suffix}')

    lines.append("")
    return lines


def write_fasm(sources: List[str], output: str, includes: List[str], flags: List[str], asset_manifest: Optional[str] = None) -> CompilerState:
    state = CompilerState()
    define_candidates = []
    for inc in includes:
        if os.path.isdir(inc):
            for root, _, files in os.walk(inc):
                for file in files:
                    if file.endswith(".h"):
                        define_candidates.append(os.path.join(root, file))
    load_defines(define_candidates, state)

    combined = []
    for source in sources:
        with open(source, "r", encoding="utf-8") as f:
            combined.append(f.read())

    source_text = strip_comments("\n".join(combined))
    functions, globals_text = extract_functions(source_text)
    if not any(fn.name == "main" for fn in functions):
        raise CompileError("No int main() or void main() function found.")
    parse_globals(globals_text, state)

    code = compile_functions(functions, state)
    fasm = emit_assets(asset_manifest, output) + code + emit_data(state)
    os.makedirs(os.path.dirname(os.path.abspath(output)), exist_ok=True)
    with open(output, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(fasm))
    return state


def assemble_rom(fasm_path: str, rom_path: str, includes: List[str]) -> None:
    assembler = os.path.join(PROJECT_ROOT, "macroassembler", "src", "mfasm.py")
    cmd = [PYTHON, assembler, fasm_path, "-o", rom_path]
    for inc in includes:
        cmd += ["-I", inc]
    cmd += ["-I", os.path.dirname(os.path.abspath(fasm_path))]
    run(cmd, cwd=PROJECT_ROOT)


def infer_asset_manifest(sources: List[str]) -> Optional[str]:
    seen = set()
    for source in sources:
        current = os.path.abspath(source if os.path.isdir(source) else os.path.dirname(source))
        while current and current not in seen:
            seen.add(current)
            candidate = os.path.join(current, "assets", "manifest.json")
            if os.path.exists(candidate):
                return candidate

            parent_candidate = os.path.join(os.path.dirname(current), "assets", "manifest.json")
            if os.path.exists(parent_candidate):
                return parent_candidate

            parent = os.path.dirname(current)
            if parent == current:
                break
            current = parent
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description="Fantasy Pi GCC-based C/C++ to FASM compiler")
    parser.add_argument("sources", nargs="+", help="C/C++ source files or directories")
    parser.add_argument("-o", "--output", required=True, help="Output .fasm file")
    parser.add_argument("--rom", help="Optional output .rom file assembled with mfasm.py")
    parser.add_argument("-I", "--include", action="append", default=[], help="Include directory")
    parser.add_argument("--no-gcc-check", action="store_true", help="Skip GCC syntax check")
    parser.add_argument("--gcc-flag", action="append", default=[], help="Extra GCC syntax-check flag")
    parser.add_argument("--asset-manifest", help="Optional Fantasy Pi assets/manifest.json to embed")
    args = parser.parse_args(normalize_argv(sys.argv[1:]))

    sources = collect_sources(args.sources)
    if not sources:
        raise CompileError("No C/C++ source files found.")
    includes = [os.path.abspath(p) for p in args.include]
    if not args.no_gcc_check:
        gcc_check(sources, includes, args.gcc_flag)
    asset_manifest = args.asset_manifest or infer_asset_manifest(sources)
    state = write_fasm(sources, args.output, includes, args.gcc_flag, asset_manifest)
    print(f"fpgcc: wrote {args.output}")
    if asset_manifest:
        print(f"fpgcc: embedded assets from {asset_manifest}")
    for warning in state.warnings:
        print(f"warning: {warning}", file=sys.stderr)
    if args.rom:
        assemble_rom(args.output, args.rom, includes)
        print(f"fpgcc: wrote {args.rom}")
    return 0


def normalize_argv(argv: List[str]) -> List[str]:
    normalized: List[str] = []
    i = 0
    while i < len(argv):
        if argv[i] == "--gcc-flag" and i + 1 < len(argv):
            normalized.append("--gcc-flag=" + argv[i + 1])
            i += 2
            continue
        normalized.append(argv[i])
        i += 1
    return normalized


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except CompileError as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1)
