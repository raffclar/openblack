#!/usr/bin/env python3
# ******************************************************************************
# Copyright (c) 2018-2026 openblack developers
#
# For a complete list of all authors, please refer to contributors.md
# Interested in contributing? Visit https://github.com/openblack/openblack
#
# openblack is licensed under the GNU General Public License version 3.
# ******************************************************************************
"""Makes pinned data of the game's float to integer conversion, in test/oracle/data:

- ftol_portable.txt: the result of the conversion for float and double values, in range, at the edges of the 32-bit
  and the 64-bit range, out of range, NaN and infinity. The conversion has two paths, and a flag of the C library
  selects the path:
  - flag clear (each network game, and a single-player game on a CPU that is not of the "Pentium 4" class): an x87
    64-bit integer, and the result is its low 32 bits (0 for NaN, infinity and a value out of the 64-bit range);
  - flag set (a single-player game on a "Pentium 4" class CPU): the SSE2 conversion, 0x80000000 for a value out of
    the 32-bit range or NaN.
  openblack copies the first path on every target (deviations row, FTL-1). The data keeps both results.

The game's conversion takes its value on the FPU stack, so the case runs two small pieces of code that load a float
or a double and call the conversion. The code goes over a function that no case calls.

Usage: python tools/oracle/cases/ftol_portable_cases.py [output folder]   (default: test/oracle/data)
The random inputs come from a fixed seed, so a second run gives the same file (except the date line).
"""

from __future__ import annotations

import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from oracle import Oracle, OracleError, f2u, write_pinned  # noqa: E402

ROOT = Path(__file__).resolve().parents[3]

# The game's code (BW1W120, runblack.exe 1.20)
FTOL = 0x7A1400  # the C library's float to int conversion
PATH_FLAG = 0xE83A20  # the C library's flag that selects the SSE2 path of the conversion
CODE = 0x526E00  # the falling spell's frame: no case calls it, so the small pieces of code go over it

KIND_FLOAT = 0
KIND_DOUBLE = 1


def call_ftol() -> bytes:
    return b"\xB8" + struct.pack("<I", FTOL) + b"\xFF\xD0"  # mov eax, FTOL; call eax


# cdecl int from_float(float value): fld dword [esp+4]; ftol; ret
FROM_FLOAT = b"\xD9\x44\x24\x04" + call_ftol() + b"\xC3"
# cdecl int from_double(double value): fld qword [esp+4]; ftol; ret
FROM_DOUBLE = b"\xDD\x44\x24\x04" + call_ftol() + b"\xC3"


def float_bits() -> list[int]:
    fixed = [
        0.0, -0.0, 0.5, -0.5, 1.0, -1.0, 1.9, -1.9, 2.5, -2.5, 3.5, -3.5, 0.99999994, 123456.7, -123456.7,
        8388607.5, -8388607.5, 16777215.0, 16777216.0, 65536.0 * 6553.6, -65536.0 * 6553.6,
        2147483520.0,  # the largest float below 2^31
        -2147483648.0,  # -2^31
        2147483648.0,  # 2^31
        -2147483904.0,  # the next float below -2^31
        4294967040.0,  # the largest float below 2^32
        4294967296.0,  # 2^32
        4294967808.0,  # 2^32 + 512
        -4294967808.0,
        3.0e9, -3.0e9, 1.0e10, -1.0e10, 54975582208.0, -54975582208.0,
        1099511627776.0 + 65536.0,  # 2^40 + 2^16
        9223371487098961920.0,  # the largest float below 2^63
        -9223371487098961920.0,
        9223372036854775808.0,  # 2^63
        -9223372036854775808.0,  # -2^63
        -9223373136366403584.0,  # the next float below -2^63
        1.0e19, -1.0e19, 3.4028234663852886e38, -3.4028234663852886e38,
    ]
    bits = [f2u(value) for value in fixed]
    # 8388608 x 6553.6 as the game's float multiply gives it (exact, because 8388608 is a power of 2)
    bits.append(f2u(8388608.0 * struct.unpack("<f", struct.pack("<f", 6553.6))[0]))
    bits += [
        0x7F800000, 0xFF800000,  # infinity
        0x7FC00000, 0xFFC00000, 0x7F800001, 0x7FFFFFFF,  # NaN, quiet and signalling
        0x00000001, 0x80000001, 0x00800000,  # denormal and the smallest normal
    ]
    rng = random.Random(0xF701)
    for _ in range(1500):
        # A random float with an exponent from 2^-2 to 2^70, so most of them are near or out of the two ranges
        exponent = rng.randint(125, 197)
        bits.append((rng.getrandbits(1) << 31) | (exponent << 23) | rng.getrandbits(23))
    for _ in range(500):
        bits.append(rng.getrandbits(32))
    return bits


def double_values() -> list[float]:
    fixed = [
        2147483647.0, 2147483647.9, -2147483648.9, -2147483649.0, 2147483648.5, 4294967301.0, -4294967301.0,
        1099511627783.0, -1099511627783.0,  # 2^40 + 7
        9223372036854774784.0,  # the largest double below 2^63
        -9223372036854774784.0,
        9223372036854775808.0, -9223372036854775808.0, -9223372036854777856.0, 1.0e300, -1.0e300,
        math.inf, -math.inf, math.nan, 0.49999999999999994, -0.49999999999999994, 1.5, 2.5, -1.5, -2.5,
    ]
    rng = random.Random(0xF702)
    randoms = []
    for _ in range(1000):
        exponent = rng.randint(1021, 1088)
        raw = (rng.getrandbits(1) << 63) | (exponent << 52) | rng.getrandbits(52)
        randoms.append(struct.unpack("<d", struct.pack("<Q", raw))[0])
    return fixed + randoms


def convert(o: Oracle, address: int, args: list[int]) -> int:
    result = o.call(address, args=args)
    if result.depth != 0:
        raise OracleError(f"the conversion left {result.depth} values on the FPU stack")
    return result.eax & 0xFFFFFFFF


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "test" / "oracle" / "data"
    rows = []
    with Oracle() as o:
        from_float = CODE
        from_double = CODE + 0x40
        o.write(from_float, FROM_FLOAT)
        o.write(from_double, FROM_DOUBLE)
        cases = [(KIND_FLOAT, [bits, 0], [bits]) for bits in float_bits()]
        for value in double_values():
            low, high = struct.unpack("<II", struct.pack("<d", value))
            cases.append((KIND_DOUBLE, [low, high], [low, high]))
        for kind, words, args in cases:
            address = from_float if kind == KIND_FLOAT else from_double
            o.write_u32(PATH_FLAG, 0)
            wide = convert(o, address, args)
            o.write_u32(PATH_FLAG, 1)
            sse = convert(o, address, args)
            rows.append([kind, words[0], words[1], wide, sse])
    n = write_pinned(out / "ftol_portable.txt", "The game's float to integer conversion on both of its paths",
                     ["kind 0: low is the float bits and high is 0; kind 1: low and high are the double bits",
                      "wide: the x87 64-bit path (flag clear), the result that openblack copies on every target",
                      "sse: the SSE2 path (flag set), a single-player game on a Pentium 4 class CPU"],
                     ["kind", "low", "high", "wide", "sse"], rows)
    print(f"ftol_portable.txt: {n} rows")


if __name__ == "__main__":
    main()
