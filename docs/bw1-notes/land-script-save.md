# Writing land-script lines: SaveObject and WriteCommand

The original can write an object back as the land-script line that re-creates it. Each class has its own
`SaveObject` (vt +0x82C). The vortex uses it to carry what an In swallows to the next land (`vortex.txt`, see
[vortex.md](vortex.md)); the land editor's save uses the same writers. openblack reads land scripts already
(LHScriptX, [map-loading.md](map-loading.md)); this page is the writing side.

## The line format

- `GSetup::GetCommandAsText(cmd)` 0x715130 -> `LHScriptCommandX::GetCommandAsText` 0x7E7FB0 builds a **printf format**
  from the command table 0xC20F00. There are 105 entries of 16 bytes: the name pointer, then 12 inline type letters
  (spaces are padding). The order: strcpy of the name and strcat "(" (0x7E7FBE..0x7E7FD5); the 12 letters, each
  through (letter − 'A') in the byte table 0x7E8078 and the jump table 0x7E8064 (0x7E7FE1..0x7E802A); the last two
  characters cut if anything was added (0x7E8035..0x7E8042); then ")\n" (0x7E8047, literal 0xC22098).
- The format is the name, `(`, then per letter:

  | Letter | Appended | Meaning |
  |---|---|---|
  | `A` | `%s, ` | a text without quotes: a position, or a name such as a villager's type (`CELTIC_FISHERMAN`) |
  | `L` | `"%s", ` | a quoted string |
  | `N` | `%d, ` | an int |
  | `F` | `%f, ` | a float, printed with 6 decimals |
  | any other | nothing | padding |

  The per-letter literals: A `%s, ` 0xC34DDC, L `"%s", ` 0xC34DE4, N `%d, ` 0xBE83AC, F `%f, ` 0xC34DD4.

- The last `, ` is cut if anything was added, then `)\n` closes it. A command with no parameters is `NAME()\n`.
- **A position** is `MapCoords::ConvertToText` 0x602880: `"%0.2f,%0.2f"` of x then z, with the quotes in the
  literal (0xBF4198). Each value is `fixed × 10.0f × 2^-16` on the x87 at 24 bits (`fild`, `fmul [0x930050]` (10.0),
  `fmul [0x8AC41C]` (2^-16)), so the first product is rounded to a float and the second is exact.
- The exe's CRT (`sprintf` 0x7C57D2) rounds the digits **half up**: 0.625 m is written `0.63`, and 2^-7 is written `0.007813`. Today's
  C runtimes round half to even here.
- Example (Villager, CREATE_TOWN_VILLAGER `NAAN`): `CREATE_TOWN_VILLAGER(1, "3079.69,3117.83", CELTIC_FISHERMAN, 26)`.
  The shipped Land2 / Land3 use this form, and openblack's reader takes the bare name as a string.

## The writers

- Every writer first calls `CheckAndSetSaved` 0x56FEF0: an object whose GameThing +0xC already equals
  GlobalSaveCount [0xD99710] has been saved in this pass and is skipped.
  - The count goes up (`inc word`) in SaveAllMap 0x7183AE, fn_00718C10 (at 0x718C93, and near 0x719154) and the
    VortexSave ctor 0x76F84E.
- The position written is `MapCoords − at` when an `at` is given (the vortex's In, MapCoords::operator- 0x6055C0).
- The physics classes' writers:

  | Writer | Used by | Skips when | Line |
  |---|---|---|---|
  | MobileStatic 0x6088E0 | MobileStatic, Rock, Fragment | +0x7C set | `CREATE_MOBILE_STATIC(pos, info, altitude, x, y, z, scale)` |
  | MobileObject 0x607270 | MobileObject, Ball | +0x58 set | `CREATE_MOBILEOBJECT(pos, info, ftol(y × 1000), ftol(scale × 1000))` |
  | Pot 0x66D550 | Pot, the piles | it is part of a structure (a storage pit's or a workshop's pile, 0x66DA00) | `CREATE_POT(pos, info, resource type, amount)` |

- MobileStatic and MobileObject then mark every identical object (the same type, info and MapCoords) as saved, so a
  stack is written once.
  - The identical-object pass: MobileStatic 0x608A20..0x608A9E, MobileObject 0x607366..0x6073E6: every other object
    of the same class at exactly the same MapCoords (x, z and the altitude) with the same info is marked saved
    (`FindType(MapCoords, OBJECT_TYPE)` 0x6045C0 over the cell).
  - MobileObject's pass goes through `FindTypeOnMap` (0x601668) filtered on the info's object type (+0x10):
    MOBILE_OBJECT 20 only, so a Ball (type 15) never marks its twins.
- **MobileStatic** `SaveObject` 0x6088E0: CheckAndSetSaved at 0x6088F1; the position 0x6088FE..0x608929; +0x7C (an
  Object*: what it belongs to) checked at 0x60892D; `GetScale` (vt +0x120), `GetZAngle` (+0x50C), `GetYAngle`
  (+0x508), `GetXAngle` (+0x504), each as a double (0x608947..0x608977): the stored angles as they are.
  - The info's index = (+0x28 − 0xD3A6D8) / 0x12C and the altitude +8 as the vertical offset (0x60897D..0x6089A5).
  - An `IsTownArtifact` (vt +0x800) object writes `TownArtifact::SaveObject` 0x4269C0's own line instead
    (0x6089F2..0x608A1B).
- **MobileObject** `SaveObject` 0x607270: CheckAndSetSaved at 0x60727B; the position 0x60728C..0x6072BB; +0x58 (what
  holds it) checked at 0x6072BF; `ftol(GetYAngle × 1000.0f [0x8AB228])`, `ftol(GetScale × 1000.0f)`, the info's index
  (+0x28 − 0xD38448) / 0x114 (0x6072CF..0x607301).
- **Pot** `SaveObject` 0x66D550: CheckAndSetSaved at 0x66D55A; the position 0x66D569..0x66D59B; `IsPartOfStructure`
  (vt +0x860) at 0x66D59F → nothing (0x66D632). A plain Pot's (0x55D4B0) is 0; a PotStructure's (0x66DA00) is its
  +0x78 structure when that IsAvailable (vt +0x2C); 0x66DA00 also clears +0x78 when its owner is not available.
  - The info's index (+0x28 − 0xD4C660) / 0x144, `GetResourceType` (vt +0x690), the amount +0x70
    (0x66D5AD..0x66D5D7).
- **DeadTree** `SaveObject` 0x511430: `CREATE_DEAD_TREE` (43, "ALNFFFF"). CheckAndSetSaved at 0x511439; the position
  0x511446..0x511478; the player `GetPlayer` (vt +0x1C), else the local player (g_game +0x205A5B), its name from the
  table 0xBFF094 (`GetPlayerNumber` 0x64A790) (0x51147C..0x5114BB).
  - `GetZAngle`, `GetYAngle`, `GetXAngle`, `GetLife` (vt +0x11C), each a double (0x5114C4..0x5114F4); the tree
    info's index (+0x94 − 0xDA3AD8) / 0x140 (0x5114FA..0x511512).
- **OneOffSpellSeed** `SaveObject` 0x72AB80: `CREATE_ONE_SHOT_SPELL_PU` (84, "AL"; not 83). CheckAndSetSaved at
  0x72AB8A; the position 0x72AB99..0x72ABCA.
  - The name (0x72ABCE..0x72ABDA) is `GetMagicInfo` 0x72A610 (the seed's info, `GetMagicInfoFromPULevel` 0x72AFE0 at
    its power-up level +0x78: the base one when that level has no magic type) → `GetMagicInfoText` 0x5FB3F0 =
    `GetMagicEffectInfo` 0x5FB680 of that GMagicInfo's magicType (+0x10, table 0xD37D10) + 0x34 (debugString), as
    `GetInfoFromText` reads it.
- **A fragment is written as a rock**: its ctor gives every fragment GMobileStaticInfo row 2 (0xD3A930), Rock (0x76E9E4). It comes
  back on the next land as a plain rock.
- Scaffold (0x6EAF30), Bonfire (0x4398A0), Villager (0x751AF0), Abode, Tree, Feature, Creature and some forty other
  classes have their own writers.

## In openblack

> **Code rules.** The save pass reads ECS components and each class's writer is registered by its owner, never a
> global; the writer and its format are pure functions, unit tested with fakes in `test/`; comments describe
> behaviour in plain English, with no decompiled names or addresses (those belong here). See
> [openblack-internals.md](openblack-internals.md).

- `lhscriptx::WriteCommand(name, {values})` (LHScriptX/ScriptWriter): the table copied from the exe, the format of
  0x7E7FB0, ConvertToText's position text and the half-up rounding. A test checks that every command it can write has
  a reader with the same parameters.
- `ecs::land_script_save` (ECS/LandScriptSave): the save pass, a writer per class (registered by its owner), and the
  physics writers above.
- (approximate): openblack keeps no object angles, so the MobileStatic angles come from the rotation matrix.

## Pending

- The writers of the other classes (each class's owner).
- The saved-game path, since openblack has no saved games.
- (pending) a building site's pile is linked through +0x74: whether its +0x78 is set too is not read.

## Sources

- runblack.exe W120: GSetup 0x715130 / 0x7E7FB0, MapCoords::ConvertToText 0x602880, the SaveObject writers above.
