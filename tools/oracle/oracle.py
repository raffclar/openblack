#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""The oracle: run one function of the original game's executable on chosen inputs and get its exact result.

The native host (oracle_host.cpp, 32-bit x86) maps the executable at its fixed base and calls single functions. It
never runs the game. This module builds the host, starts it and gives a Python interface to it:

    from oracle import Oracle, f32, u2f
    with Oracle() as o:
        r = o.call(0x841170, args=[f32(2.0)])
        print(hex(r.st0f), u2f(r.st0f))

Command line:
    python oracle.py build                 builds the host (MSVC x86) into E:/openblack/builds/oracle-x86
    python oracle.py run request.json      runs a JSON request (or "-" for the standard input), prints JSON
    python oracle.py imports               lists the executable's imports

See README.md for the JSON request and for how to set up objects and imports.
"""

from __future__ import annotations

import json
import os
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

HERE = Path(__file__).resolve().parent
DEFAULT_EXE = Path(os.environ.get("OB_ORACLE_EXE", r"C:\projects\black\runblack.exe"))
BUILD_DIR = Path(os.environ.get("OB_ORACLE_BUILD", r"E:\openblack\builds\oracle-x86"))
HOST = BUILD_DIR / "oracle_host.exe"
VCVARS = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat")

# The game's FPU control word: all exceptions masked, round to nearest, 24-bit precision (the game's start-up code
# clears the precision bits of the default 0x037F)
GAME_CONTROL_WORD = 0x007F


def f2u(value: float) -> int:
    """The bits of a value as a 32-bit float."""
    return struct.unpack("<I", struct.pack("<f", value))[0]


def u2f(bits: int) -> float:
    return struct.unpack("<f", struct.pack("<I", bits & 0xFFFFFFFF))[0]


class f32:
    """A stack argument or field given as a float (or as exact bits with f32.bits)."""

    def __init__(self, value: float | None = None, bits: int | None = None):
        self.bits = f2u(value) if bits is None else bits & 0xFFFFFFFF

    @staticmethod
    def of_bits(bits: int) -> "f32":
        return f32(bits=bits)


def x80_to_float(raw: bytes) -> float:
    """An 80-bit x87 value (10 bytes, little end first) as a Python float (exact for 24 and 53 bit mantissas)."""
    mantissa = int.from_bytes(raw[0:8], "little")
    sign_exp = int.from_bytes(raw[8:10], "little")
    sign = -1.0 if sign_exp & 0x8000 else 1.0
    exponent = sign_exp & 0x7FFF
    if exponent == 0x7FFF:
        return sign * float("inf") if (mantissa << 1) & 0xFFFFFFFFFFFFFFFF == 0 else float("nan")
    if exponent == 0 and mantissa == 0:
        return sign * 0.0
    import math

    return sign * math.ldexp(mantissa, exponent - 16383 - 63)


@dataclass
class Result:
    eax: int
    edx: int
    popped: int  # How many argument bytes the function removed (0 for cdecl)
    fsw: int  # The FPU status word after the call (exception flags in its low bits)
    depth: int  # How many values the FPU stack held after the call
    st0: bytes | None  # ST0 as 10 bytes, or None when the FPU stack was empty
    st0f: int | None  # ST0 stored as a 32-bit float (bits), or None

    @property
    def st0_value(self) -> float | None:
        return None if self.st0 is None else x80_to_float(self.st0)


class OracleError(RuntimeError):
    pass


def build(verbose: bool = True) -> Path:
    """Builds the host with MSVC for x86 into BUILD_DIR. Not part of the game build.

    oracle_host.exe is the loader: fixed at the game's base with a 16 MB empty section, so that the range is free.
    oracle_core.dll is the host's code, fixed high, out of the game's range.
    """
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    core = HERE / "oracle_core.cpp"
    loader = HERE / "oracle_loader.cpp"
    out = str(BUILD_DIR)
    lines = [
        "@echo off",
        f'call "{VCVARS}" x86 >nul || exit /b 1',
        f'cl /nologo /std:c++17 /O2 /EHa /W4 /WX /MT /D_CRT_SECURE_NO_WARNINGS /LD "{core}" '
        f'/Fo"{out}\\oracle_core.obj" /Fe"{out}\\oracle_core.dll" '
        "/link /BASE:0x60000000 /FIXED /DYNAMICBASE:NO || exit /b 1",
        f'cl /nologo /O1 /GS- /W4 /WX "{loader}" /Fo"{out}\\oracle_loader.obj" /Fe"{HOST}" '
        "/link /NODEFAULTLIB /ENTRY:LoaderEntry /SUBSYSTEM:CONSOLE /BASE:0x400000 /FIXED /DYNAMICBASE:NO "
        "kernel32.lib || exit /b 1",
    ]
    script = BUILD_DIR / "build_host.bat"
    script.write_text("\n".join(lines) + "\n")
    process = subprocess.run(["cmd", "/c", str(script)], capture_output=True, text=True)
    if verbose or process.returncode != 0:
        sys.stdout.write(process.stdout)
        sys.stderr.write(process.stderr)
    if process.returncode != 0:
        raise OracleError("the host build failed")
    return HOST


def _signed(value: int) -> int:
    return value - (1 << 32) if value & 0x80000000 else value


def _hex(value: int) -> str:
    return format(value & 0xFFFFFFFF, "x")


class Oracle:
    """One host process with the executable mapped. Memory and stubs stay between calls until reset()."""

    def __init__(self, exe: Path | str = DEFAULT_EXE, host: Path | str = HOST):
        host = Path(host)
        if not host.exists():
            build(verbose=False)
        self.process = subprocess.Popen(
            [str(host), str(exe)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
            bufsize=1,
        )
        ready = self._answer()
        info = self._send("info")
        fields = dict(word.split("=") for word in info.split())
        self.base = int(fields["base"], 16)
        self.image_size = int(fields["size"], 16)
        self.arena = int(fields["arena"], 16)
        del ready

    # The protocol

    def _answer(self) -> str:
        line = self.process.stdout.readline()
        if not line:
            raise OracleError("the host stopped")
        line = line.strip()
        if line.startswith("err"):
            raise OracleError(line[4:])
        return line[3:] if line.startswith("ok ") else ""

    def _send(self, line: str) -> str:
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        return self._answer()

    def close(self) -> None:
        if self.process.poll() is None:
            try:
                self.process.stdin.write("quit\n")
                self.process.stdin.flush()
                self.process.wait(timeout=5)
            except Exception:
                self.process.kill()

    def __enter__(self) -> "Oracle":
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    # Memory

    def reset(self) -> None:
        """Copies the executable's sections again, frees the arena and traps every import again."""
        self._send("reset")

    def alloc(self, size: int, data: bytes | None = None) -> int:
        """Zeroed, 16-byte aligned memory in the host's arena, for objects, buffers and tables."""
        address = int(self._send(f"alloc {_hex(size)}"), 16)
        if data:
            self.write(address, data)
        return address

    def write(self, address: int, data: bytes) -> None:
        for start in range(0, len(data), 1 << 18):
            chunk = data[start : start + (1 << 18)]
            self._send(f"write {_hex(address + start)} {chunk.hex()}")

    def read(self, address: int, length: int) -> bytes:
        return bytes.fromhex(self._send(f"read {_hex(address)} {_hex(length)}"))

    def write_u32(self, address: int, *values: int) -> None:
        self.write(address, b"".join(struct.pack("<I", v & 0xFFFFFFFF) for v in values))

    def write_f32(self, address: int, *values: float | f32) -> None:
        self.write_u32(address, *[(v.bits if isinstance(v, f32) else f2u(v)) for v in values])

    def read_u32(self, address: int, count: int = 1) -> list[int]:
        return list(struct.unpack(f"<{count}I", self.read(address, 4 * count)))

    def read_f32_bits(self, address: int, count: int = 1) -> list[int]:
        return self.read_u32(address, count)

    def object(self, size: int, fields: dict[int, int | float | f32] | None = None) -> int:
        """A zeroed object of a size, with fields set by offset: int -> dword, float or f32 -> float bits."""
        address = self.alloc(size)
        for offset, value in (fields or {}).items():
            if isinstance(value, (float, f32)):
                self.write_f32(address + offset, value)
            else:
                self.write_u32(address + offset, value)
        return address

    # Stubs

    def stub(self, address: int, eax: int = 0, pop: int = 0, st0: float | f32 | None = None) -> None:
        """Replaces the function at an address with "return eax (and st0); remove pop bytes of arguments"."""
        words = [f"stub {_hex(address)}", f"eax={_hex(eax)}", f"pop={_hex(pop)}"]
        if st0 is not None:
            words.append(f"st0={_hex(st0.bits if isinstance(st0, f32) else f2u(st0))}")
        self._send(" ".join(words))

    def hook_alloc(self, address: int) -> None:
        """Sends a cdecl allocator of the game (size) -> pointer to zeroed arena memory."""
        self._send(f"hook {_hex(address)} alloc")

    def hook_free(self, address: int) -> None:
        self._send(f"hook {_hex(address)} free")

    def stub_import(self, name: str, eax: int = 0, pop: int = 0, st0: float | f32 | None = None) -> None:
        """Replaces an import's trap ("KERNEL32.dll!GetTickCount") with a stub."""
        words = [f"import {name} stub", f"eax={_hex(eax)}", f"pop={_hex(pop)}"]
        if st0 is not None:
            words.append(f"st0={_hex(st0.bits if isinstance(st0, f32) else f2u(st0))}")
        self._send(" ".join(words))

    def imports(self) -> list[str]:
        return self._send("imports").split()

    # Calls

    def call(
        self,
        address: int,
        args: Sequence[int | float | f32] = (),
        this: int | None = None,
        ecx: int | None = None,
        edx: int | None = None,
        eax: int | None = None,
        control_word: int = GAME_CONTROL_WORD,
    ) -> Result:
        """Calls a function. Arguments are stack dwords, first to last; the convention only changes who removes them.

        cdecl and stdcall: give args. thiscall: give this (ECX) and args. fastcall: ecx and edx, then args.
        A float argument (float or f32) goes as its 32-bit bits. A double takes two dwords: use doubles().
        """
        words = [f"call {_hex(address)}", f"cw={_hex(control_word)}"]
        if this is not None:
            ecx = this
        for name, value in (("eax", eax), ("ecx", ecx), ("edx", edx)):
            if value is not None:
                words.append(f"{name}={_hex(value)}")
        for value in args:
            if isinstance(value, f32):
                words.append(_hex(value.bits))
            elif isinstance(value, float):
                words.append(_hex(f2u(value)))
            else:
                words.append(_hex(value))
        answer = self._send(" ".join(words))
        fields = dict(word.split("=") for word in answer.split())
        st0 = None if fields["st0"] == "-" else bytes.fromhex(fields["st0"])[::-1]
        return Result(
            eax=int(fields["eax"], 16),
            edx=int(fields["edx"], 16),
            popped=4 * (len(words) - 2 - sum(1 for v in (eax, ecx, edx) if v is not None)) + _signed(int(fields["esp"], 16)),
            fsw=int(fields["fsw"], 16),
            depth=int(fields["depth"], 16),
            st0=st0,
            st0f=None if st0 is None else int(fields["st0f"], 16),
        )


def doubles(value: float) -> list[int]:
    """A double stack argument as its two dwords, low first."""
    low, high = struct.unpack("<II", struct.pack("<d", value))
    return [low, high]


# The JSON request


def _value(item, names: dict[str, int]) -> list[int]:
    """One argument or field: {"u32": n}, {"f32": x}, {"f32bits": n}, {"ptr": "name"}, {"f64": x}, or a plain int."""
    if isinstance(item, int):
        return [item]
    if "u32" in item:
        return [int(item["u32"], 0) if isinstance(item["u32"], str) else item["u32"]]
    if "f32" in item:
        return [f2u(float(item["f32"]))]
    if "f32bits" in item:
        return [int(item["f32bits"], 0) if isinstance(item["f32bits"], str) else item["f32bits"]]
    if "f64" in item:
        return doubles(float(item["f64"]))
    if "ptr" in item:
        return [names[item["ptr"]] + int(item.get("offset", 0))]
    raise OracleError(f"bad value {item}")


def _address(item, names: dict[str, int]) -> int:
    if isinstance(item, int):
        return item
    if isinstance(item, str):
        return names[item] if item in names else int(item, 0)
    return names[item["ptr"]] + int(item.get("offset", 0))


def run_request(request: dict, oracle: Oracle | None = None) -> dict:
    """Runs a JSON request. See README.md for its form."""
    own = oracle is None
    oracle = oracle or Oracle(request.get("exe", DEFAULT_EXE))
    try:
        names: dict[str, int] = {}
        for block in request.get("memory", []):
            if "name" in block and "size" in block:
                names[block["name"]] = oracle.alloc(int(block["size"]))
            address = _address(block.get("at", block.get("name")), names)
            if "hex" in block:
                oracle.write(address, bytes.fromhex(block["hex"]))
            for field in block.get("fields", []):
                words = _value(field, names)
                oracle.write_u32(address + int(field.get("off", 0)), *words)
        for stub in request.get("stubs", []):
            kind = stub.get("kind", "return")
            if kind == "alloc":
                oracle.hook_alloc(_address(stub["addr"], names))
            elif kind == "free":
                oracle.hook_free(_address(stub["addr"], names))
            elif kind == "import":
                st0 = stub.get("st0")
                oracle.stub_import(stub["name"], int(stub.get("eax", 0)), int(stub.get("pop", 0)),
                                   None if st0 is None else float(st0))
            else:
                st0 = stub.get("st0")
                eax = stub.get("eax", 0)
                eax = _address(eax, names) if isinstance(eax, (str, dict)) else int(eax)
                oracle.stub(_address(stub["addr"], names), eax, int(stub.get("pop", 0)),
                            None if st0 is None else float(st0))
        results = []
        for call in request.get("calls", []):
            args: list[int] = []
            for item in call.get("args", []):
                args.extend(_value(item, names))
            this = call.get("this")
            result = oracle.call(
                _address(call["addr"], names),
                args=args,
                this=None if this is None else _address(this, names),
                ecx=call.get("ecx"),
                edx=call.get("edx"),
                control_word=int(call.get("cw", GAME_CONTROL_WORD)),
            )
            out = {
                "eax": f"0x{result.eax:08x}",
                "edx": f"0x{result.edx:08x}",
                "popped": result.popped,
                "fsw": f"0x{result.fsw:04x}",
                "fpu_depth": result.depth,
                "st0": None if result.st0 is None else result.st0[::-1].hex(),
                "st0_f32bits": None if result.st0f is None else f"0x{result.st0f:08x}",
                "st0_value": result.st0_value,
            }
            reads = {}
            for read in call.get("read", []):
                address = _address(read.get("at", read.get("name")), names) + int(read.get("off", 0))
                reads[read.get("label", hex(address))] = oracle.read(address, int(read["len"])).hex()
            if reads:
                out["memory"] = reads
            results.append(out)
        return {"results": results}
    finally:
        if own:
            oracle.close()


# Pinned data files for the unit tests (test/oracle/data). Only the inputs and the exe's results go in them, never
# bytes of the exe.


def exe_identity(exe: Path | str = DEFAULT_EXE) -> str:
    import hashlib

    data = Path(exe).read_bytes()
    return f"{Path(exe).name} size {len(data)} sha256 {hashlib.sha256(data).hexdigest()}"


def write_pinned(path: Path | str, title: str, notes: Sequence[str], columns: Sequence[str],
                 rows: Iterable[Sequence[int]], exe: Path | str = DEFAULT_EXE) -> int:
    """Writes a pinned table: '#' lines, then 'columns ...', then one row of 32-bit hex words per case."""
    import datetime

    lines = [f"# {title}", f"# Made by tools/oracle on {datetime.date.today().isoformat()} from {exe_identity(exe)}"]
    lines += [f"# {note}" for note in notes]
    lines.append("columns " + " ".join(columns))
    count = 0
    for row in rows:
        if len(row) != len(columns):
            raise OracleError("a row does not match the columns")
        lines.append(" ".join(f"{value & 0xFFFFFFFF:08x}" for value in row))
        count += 1
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text("\n".join(lines) + "\n", newline="\n")
    return count


def main(argv: Iterable[str]) -> int:
    argv = list(argv)
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0
    if argv[0] == "build":
        build()
        return 0
    if argv[0] == "imports":
        with Oracle() as oracle:
            print("\n".join(oracle.imports()))
        return 0
    if argv[0] == "run" and len(argv) == 2:
        text = sys.stdin.read() if argv[1] == "-" else Path(argv[1]).read_text()
        print(json.dumps(run_request(json.loads(text)), indent=2))
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
