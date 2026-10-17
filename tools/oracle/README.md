# The oracle

The oracle runs one function of the original game's executable (`runblack.exe`, 1.20) on inputs that you choose and
gives back its exact result: EAX, EDX, the FPU result (ST0, all 80 bits and as a stored float) and any memory you ask
for. Unit tests then pin our maths to the game's bits from data files. The tests do not need the game.

The oracle never runs the game. It does not run the start-up code, the static constructors or the entry point. Each
import of the executable (Windows, DirectX, sound, network, the game's own DLLs) is a trap: a call to it stops the
call with an error, unless you replace it with a stub. It cannot open a window, play a sound or use the network.

## How it works

- `oracle_loader.cpp` builds `oracle_host.exe`. It is fixed at the game's base (0x400000) and holds a 16 MB empty
  section. Windows maps a program first, before heaps and mapped files take the low addresses, so the game's range
  is free in this process.
- `oracle_core.cpp` builds `oracle_core.dll` (fixed high, at 0x60000000). The loader calls it. It copies the
  executable's headers and sections into the range, fills the rest with zeros (the uninitialised data), points each
  import at its trap, and then reads commands from the standard input.
- A call sets the FPU control word (the game's 0x007F by default: all exceptions masked, round to nearest, 24-bit
  precision, as the game's start-up code sets it), sets EAX, ECX and EDX, pushes the arguments and calls the
  address. The stack pointer comes back from a saved copy, so cdecl, stdcall, thiscall and fastcall all work.
- An access violation or an import trap in the call gives an error line, and the host keeps working.

The x87 code runs on the real CPU, so precision control, rounding, `_ftol` and the FPU stack are exact. A CPU
emulator was not used, because its x87 must honour the precision control bits exactly and that needs a proof. On
2000 random inputs to the game's inverse square root, 53-bit precision gave different 80-bit results for 1999 and
different stored floats for 448; 24-bit precision gave results with no bits below the 24-bit mantissa.

## Build

    python tools/oracle/oracle.py build

It uses MSVC (Visual Studio 2022, x86 tools) and writes to `E:\openblack\builds\oracle-x86` (`OB_ORACLE_BUILD`
changes it). It is not part of the game build. `Oracle()` builds the host if it is missing. `OB_ORACLE_EXE` changes
the executable (default `C:\projects\black\runblack.exe`).

## Calling a function from Python

```python
import sys; sys.path.insert(0, "tools/oracle")
from oracle import Oracle, f32, u2f

with Oracle() as o:
    # cdecl float f(float a, float b), result on the FPU
    r = o.call(0x74F170, args=[f32(0.5), f32(0.7)])
    print(hex(r.st0f), u2f(r.st0f), r.depth)       # the stored float, and the FPU stack depth (1)

    # thiscall: this goes in ECX; r.popped tells how many argument bytes the function removed
    obj = o.object(0x200, {0x78: 1000.0, 0x4A3C: 400})   # floats as float bits, ints as dwords
    thing = o.object(0x200)
    r = o.call(0x47C9D0, this=obj, args=[0, thing])
    print(r.eax & 0xFF)
```

- Arguments are stack dwords from first to last: `int`, `float` or `f32(...)` (`f32.of_bits(n)` for exact bits).
  A double takes two dwords: `doubles(x)`.
- `this=` (thiscall), `ecx=`, `edx=`, `eax=` set registers. `control_word=` changes the FPU control word.
- The result: `eax`, `edx`, `popped` (0 for cdecl), `fsw` (FPU status, with the exception flags), `depth` (values
  left on the FPU stack), `st0` (10 bytes), `st0f` (ST0 stored as a float), `st0_value` (ST0 as a Python float).

## Setting up memory and objects

- `o.alloc(size)` gives zeroed, 16-byte aligned memory in the host's arena; `o.object(size, {offset: value})` sets
  fields too. Pointers between your objects are plain addresses: `o.write_u32(a + 0x160, b)`.
- `o.write(address, bytes)`, `o.write_u32`, `o.write_f32`, `o.read`, `o.read_u32` work on the arena and on the
  executable's range, so you can set globals and tables at their addresses.
- `o.reset()` copies the sections again, empties the arena and traps every import again. Memory and stubs stay
  between calls until then.
- Only the bytes in the file are there. Data that the game makes at start-up (tables, objects made by constructors,
  heaps) is zero until you make it. Either lay it out yourself, or call the game's own function that makes it (for
  example the inverse square root table: hook the 3D engine's allocator, then call the function that fills it).

## Handling calls the function makes

- A small getter can run as it is: lay out the memory it reads.
- `o.stub(address, eax=..., pop=..., st0=...)` replaces a function in the executable with "return EAX (and push
  ST0); remove `pop` bytes". Use it for calls that you do not want to run (a lookup, a random number, a virtual call
  target, an alliance check). Give `pop` from the function's `RET n`.
- `o.hook_alloc(address)` and `o.hook_free(address)` send a cdecl allocator of the game to the arena.
- `o.stub_import("KERNEL32.dll!GetTickCount", eax=1234)` replaces an import's trap. `o.imports()` lists them. There
  is no way to call a real import: that keeps the game's Windows, DirectX, sound and network code from running.
- A virtual call reads a vtable from your object: put a pointer to a small table of function addresses there, and
  stub those addresses or point them at real functions.

## The JSON request

`python tools/oracle/oracle.py run request.json` (or `run -` for the standard input) prints the results as JSON:

```json
{
  "memory": [
    {"name": "clip", "size": 64, "fields": [{"off": 0, "u32": 800}, {"off": 16, "f32": -8.0}]},
    {"at": "0x00C23284", "hex": "00000000"}
  ],
  "stubs": [
    {"addr": "0x619650", "eax": {"ptr": "clip"}, "pop": 8},
    {"addr": "0x842CE0", "kind": "alloc"},
    {"kind": "import", "name": "KERNEL32.dll!GetTickCount", "eax": 1234}
  ],
  "calls": [
    {"addr": "0x74F170", "args": [{"f32": 0.5}, {"f32": 0.7}]},
    {"addr": "0x47C9D0", "this": "creature", "args": [0, {"ptr": "thing"}],
     "read": [{"name": "clip", "off": 0, "len": 16, "label": "clip"}]}
  ]
}
```

Values are `{"u32": n}`, `{"f32": x}`, `{"f32bits": n}`, `{"f64": x}` (two dwords) or `{"ptr": "name", "offset": n}`.

## The host's line protocol

`oracle_host.exe <exe>` prints `ok ready ...`, then answers each command line with one `ok ...` or `err ...` line.
Numbers are hexadecimal with no prefix. Commands: `info`, `reset`, `alloc <size>`, `write <addr> <hexbytes>`,
`read <addr> <len>`, `stub <addr> [eax=v] [pop=n] [st0=floatbits]`, `hook <addr> alloc|free`,
`import <DLL!name> stub [eax=v] [pop=n] [st0=floatbits]`, `imports`,
`call <addr> [cw=v] [eax=v] [ecx=v] [edx=v] [arg ...]`, `quit`. A call answers
`ok eax=.. edx=.. esp=.. fsw=.. depth=.. st0=<20 hex digits, high first>|- st0f=..`.

## Rule: where vanilla test data comes from

Make each "expected vanilla" value in a test with the oracle: the game's own code on the real FPU, at the game's
24-bit precision. Do not make vanilla data with a CPU emulator (Unicorn or other), unless a proof shows that its x87
obeys the precision control bits. A Python or numpy float32 model is not vanilla data. Use a model only for code that
the oracle cannot call, and write in the test where the values come from.

The cause of this rule: the scripts in `test/audio/scripts` run the game and the sound DLL under Unicorn. They did not
set the FPU control word, thus the code ran at 64-bit precision. The sound map volumes that they made were not the
game's values (`cases/atmos_volumes_cases.py` made them again).

Data from an emulator can stay only when each x87 step of the code gives the same result at all precisions. Write
that proof in the script that makes the data. An example is the atmosphere scheduler
(`test/audio/scenarios/scheduler_*.json`): its float steps change small integers to floats, add them, multiply them
by 4 and compare them with 0 and 1. Each of these steps is exact at all precisions.

## Pinned data for the unit tests

`write_pinned(path, title, notes, columns, rows)` writes a data file: `#` lines (the title, the date and the
executable's size and SHA-256, and your notes), one `columns` line and one row of 32-bit hex words for each case.
Put only inputs and results in it, never bytes of the executable.

In a test, `test/oracle/OracleData.h` reads it: `oracle::Table::Load("name.txt")`, then `Column("x")`,
`Rows()`, `Bits(row, column)` and `Float(row, column)`. Register the test in `test/CMakeLists.txt` with
`openblack_setup_and_add_test` and add it to the list of oracle tests that get `OPENBLACK_ORACLE_DATA`.

## Guide for lanes

1. Find the function in the Ghidra catalogue (`ghcat.py get <addr> --sections asm`). From the disassembly, write
   down its convention (`RET n`, ECX use), the memory it reads (object fields, globals, tables) and the calls it
   makes.
2. Write a case script in `tools/oracle/cases/` (copy `review_cases.py`): lay out the objects, stub or hook the
   calls, choose inputs (chosen cases, random cases from a fixed seed, and edge cases where one ulp changes the
   answer; `edge_pair` finds those by a binary search on the game's own answers) and write the data with
   `write_pinned` to `test/oracle/data/`.
3. Write a test that compares our function with each row, bit for bit, and reports the first rows that differ.
4. If our code does not match yet and your lane does not fix it, disable the test (`DISABLED_`) with a note that
   tells which lane turns it on. Never change the data to match our code.
5. Commit the case script, the data and the test together, so that the data can be made again.

Limits: one thread; no start-up code or constructors; no real imports; game code that throws a C++ exception or
needs a window, a device or a file does not run. A function that runs for a long time blocks the host (the Python
side has no time limit on a call).

## The review cases

`python tools/oracle/cases/review_cases.py` makes `test/oracle/data/catch_reaches.txt` (the creature catch reach),
`sigmoid.txt` (the creature sigmoid and the shared sigmoid) and `inverse_square_root.txt` (the 3D engine's inverse
square root). The tests are `test/oracle/test_oracle_*.cpp`.
