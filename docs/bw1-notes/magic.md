# Magic: the core of the miracles

Core of the original's magic system and of its port: the info.dat tables, the spell life cycle, the chants, events and
effects, the cast rules, seeds and one-off miracles, casting from the hand and gestures, worship and prayer power,
influence, alignment, reactions, life, the fire model, the order within the turn and the hooks. Each miracle has its own
section in [miracles.md](miracles.md); the particle engine (PSys) is in [particles.md](particles.md), and time and weather
in [day-night-weather.md](day-night-weather.md#weather-and-climate-srcecsweather).

Addresses are those of W120 (`runblack.exe`). This page collects what was verified in the executable while porting it.

> **Code rules.** Spells, seeds, icons and fires are ECS entities with their data in components, and the magic services
> are reached through the Locator, never through globals; info.dat, the meshes and the effects load through the resource
> caches; pure logic (the chants, the gesture recogniser, the alignment) is unit tested in `test/` with fakes;
> comments describe behaviour in plain English, with no decompiled names or addresses (those belong here). See
> [the conventions](../refactor/README.md).

- [info.dat tables](#infodat-tables-srcmagicmagictables)
- [Spell core](#spell-core-srcmagiccore-srcmagicspells-srcecseffects)
- [Casting from the hand, gestures and hand effects](#casting-from-the-hand-gestures-and-hand-effects-srcmagicgestures-srcmagichand-handspellseedcpp)
- [Worship: where miracles come from](#worship-where-miracles-come-from-srcworship-ecssystemsimplementationsvillagerworship)
- [Influence](#influence-srcecsinfluence)
- [Player alignment](#player-alignment-galignment-gplayer-0x60-srcecseffectsalignment-componentsalignment)
- [Reactions](#reactions-ecseffectsreactions)
- [Object life](#object-life-srcecslife)
- [Fire](#fire-srcecsfire)
- [Time and weather](#time-and-weather)
- [Miracles one by one](#miracles-one-by-one)
- [Review of the whole chains](#review-of-the-whole-chains)
- [Audited assumptions](#audited-assumptions-2026-10-01)
- [Test hooks](#test-hooks)
- [Pending](#pending)

## info.dat tables (`src/Magic/MagicTables`)

- `GMagicInfo*` per MAGIC_TYPE at `0xD37D10` (42), `GMagicEffectInfo[42]` at `0xCC6630` (0x11C in memory),
  `GSpellSeedInfo[30]` at `0xD9D678` (0x190). In memory each record sits 0x10 bytes behind the file (vtable and
  header): exe offset = file offset + 0x10.
- `load_variables` creates one object per record, one class per section, in MAGIC_TYPE order. The sections in file
  order give exactly 0..41: general 10 (0-9), heal 2, teleport 1, forest 1, food 2, storm/tornado 3, shield 2, wood 1,
  water 2, flock flying 1, flock ground 1, creature 16. Checked against the real info.dat: the `magicType` field of each
  record matches its position (`test_magic_tables`, `realInfoDat`).
- The GMagicInfo rows live in per-class sections: types 0..9 are magicGeneral (`SlotOf` 0x5FB700).
- `GetMagicInfoAs<T>` returns the record with its class (`GMagicResourceInfo` works for food and wood,
  `GMagicRadiusSpellInfo` for storm and shield).
- `timerWhen{OneShot,PlayerCasting,CreatureCasting,ComputerPlayerCasting}` are **float** (seconds, -1 = no limit; the
  getters 0x5FB7A0..0x5FB7D0 do `fld`). MAGIC_TYPE 0 carries the integer 10 there (garbage). Storm: 40 s.
- `GWorshipSiteInfo::chantsToReserveForMaintaining` is a float in the code but the file holds an integer: it is read as
  ~7e-43 (bug in the original, kept).
- Names of the tail of `GSpellSeedInfo` (file 0xF0..0x17C): `selectionGesture` (1 SPIRAL / 2 INVERSE_SPIRAL),
  `gesture`, `gestureStage2` (0), `sizingGesture` (4 CIRCLE on STORM, SHIELD, PHYSICAL_SHIELD), `castType`
  (SPELL_CAST_TYPE), `isKeptInHand`, `castOnObject`, `seedFollowsSpell`, `magicTypes[4]` (no PU, PU 0, 1, 2),
  `powerUpGestures[3]`, `mesh`, `scale`, `holdLoweringMultiplier`, `holdRadius`, `holdYRotate`, `holdType`,
  `attachInHandEffectToBone`, `deleteSeedOnceCast`, `holderParticle`, `exists`, `iconIndex`, `tooltip`. No meaning
  yet: 0x10C, 0x138 (equal to the scale except on the flocks), 0x150/0x154, 0x15C (0.1), 0x160 (1), 0x168, 0x178,
  0x17C (1 on FIRE, LIGHTNING_BOLT, HEAL, WEAK, STRONG).

### Ported helpers

| Function | Address | Detail |
|---|---|---|
| `GMagicInfo::GetInfoFromText` | 0x5FB3B0 | stricmp against the effect's `debugString` ("STORM_PU2"); 42 = not found |
| `IsMaintainedSpell` | 0x5FB810 | FOREST (13), SHIELD, PHYSICAL_SHIELD (19, 20) |
| `GetChantsRequiredToCreate` | 0x5FB830 | `costToCreate` (FIRE 3500, STORM 8000); `GScript::GetManaForSpell` uses the same (0x5FB800) |
| `IsCreatureCastFromAbove` | 0x5FB7E0 | `== 1` |
| `IsInAggressiveRange` | 0x5FB840 | 1.0 if min ≤ d ≤ max, otherwise 0.0 |
| `GMagicEffectInfo::GetTribalPower` | 0x5FB6A0 | product of the player's `TribalPower[t]` (GPlayer+0x68) over the flagged tribes; <0 → 0.5, >100 → 100, ≤0.5 → 0.5; no player 1 |
| `GetTribalPowerTribe` | 0x5FB710 | the first flagged tribe with power > 1, otherwise -1 |
| `GSpellSeedInfo::GetPowerUpFromMagicType` | 0x72AF70 | -1 for `magicTypes[0]`, 0/1/2 for `[1..3]`, -1 if not there. Oddity: with `[3] = 0`, MAGIC_TYPE NONE gives 2 |
| `fn_0072AFA0` | | levels = 1 + non-null `powerUpGestures` |
| `GetMagicTypeFromPULevel` | 0x72AFC0 | -1 → `[0]`, pu → `[pu+1]` (no bounds check) |
| `GetMagicInfoFromPULevel` | 0x72AFE0 | the one for that level; if its type is 0, the base one |
| `fn_0072B010` | | the power-up gesture of the type and its level (the base type: 0 and -1) |
| `SpellSeedIsOfMagicType` | 0x72B060 | any of `magicTypes[0..3]` (NONE matches the first seed with a 0: STORM) |
| `GetFirstSpellSeedForMagicType` | 0x72B090 | -1 if none (not 30) |
| `fn_0072B100` | | `fn_0072B010` on the first seed of the type |
| `fn_0072B0D0` | | the first seed with `exists` and an equal `iconIndex`; otherwise -1 |
| `fn_0072B170` | | seed by name (stricmp against the `debugString`: "HEAL"); 30 = none |
| `fn_0072B1C0` | | the first seed of the type; 30 = none |

- `GMagicInfo` +0x28 `spellSeedType`, +0x2C `gestureType` and +0x30 `powerupType` are −1, 0 (none) and −1 in every
  record of info.dat, but the game **fills them at load** (`fn_0042B400`, see
  [Dispensers and fireflies](#dispensers-and-fireflies-worshipspelldispensercpp-worshipfireflyrewardcpp)): for each
  power-up type, the gesture and level that power its first seed up to it (TORNADO 1, storm lightning 0); the base
  types keep −1. openblack: `MagicTables::GetSpellSeedOfMagicInfo`, `GetGestureOfMagicInfo`,
  `GetPowerUpLevelOfMagicInfo`.
- `GetMagicInfoText` 0x5FB3F0: the effect info of table[i]'s own magicType, + 0x34 (its `debugString`); `GetInfoFromText` 0x5FB3B0 compares against it.
- `GMagicInfo::GetMagicEffectInfo` 0x5FB680.
- The power-up level that the PSys rules read is `PSysManager::GetPowerUpLevel` 0x673510 (storm, fireball rows): the
  filled +0x30 of its spell's `GMagicInfo` (0x67351F), −1 without a spell. openblack: `GetPowerUpLevelOfMagicInfo`
  (`Spell.cpp`). Before 2026-10-08 it was computed as `GetPowerUpFromMagicType` on the first seed, the same for every
  type but NONE (2 there; the fill leaves NONE at −1).
- `GSpellSeedInfo.magicTypes[0]` is at file offset 0x114 (memory +0x124): `fn_0072AF10(0)` reads it (0x72AF1C), the same field as `GetMagicTypeFromPULevel(-1)` (0x72AFC9); `fn_0072AF50(0)` gives its `GMagicInfo` [0xD37D10 + 4 × type]; `fn_00727700` gives the seed row (0xD9D678 + type × 0x190).

## Spell core (`src/Magic/Core`, `src/Magic/Spells`, `src/ECS/Effects`)

Each spell is an entity with `components::Spell` (the 0xEC-byte `Spell`); the original's virtual functions are a
`SpellOps` table per class (`SpellClass`, the class of the `GMagicInfo` that allocates it with vt 0x34), each class in
its own file under `Spells/`. An unregistered class runs as a plain `Spell`.

### Life cycle (Spell.cpp 0x71FB40..)

- Constructor 0x71FB40: without a creator it sets neither creator nor player; with one, `player = creator->GetPlayer()`
  (or the neutral player), it goes **at the start** of the list (`g_game+0x205BC4`), `+0x48 = IsCreature`, `+0x4C` = the
  player of the creator (or of the SpellIcon) has +0x8E0 == 1.
- `GMagicInfo::CastAtPos` fn_005FB490: without a creator it uses the neutral player; `AllocSpell`, `InitWithPos`; if it
  does not return 1 the spell is deleted. fn_005FB520 does the same with `InitWithObject` if the `castOnObject` flag it
  reads is 1. It reads that flag at `GetSpellSeedInfo(spellSeedType = -1) + 0x118` = 0xD9D600, inside GSpellIconInfo[1]
  (unresolved); here the `castOnObject` of the first seed of that magic type is used.
- `Spell::InitWithPos` 0x71FE50, in order: creature desires (fn_00721730, creature, not ported), `originalCastPos`, player statistic,
  `SetChants(castData.chants)` (+0x38 = +0x3C), `maxObjects`, `duration`, pos and castPos, copy of the
  PSysProcessInfo, `dir = info+0x24`, magnitude (`castData.magnitude`, 40 without castData), `PSysInterface::Create` at
  `(x, altura del suelo + pos.y, z)`, the player record +0xDC {castPos, magic (+0xC), turn (+0x10)}. With a PSys:
  `psys->SetPlayer`. **Without a PSys and with `particleType != 0` it returns 0**: the spell is not cast, and there is
  no reaction either. Without a PSys and with `particleType == 0`: `SpellEvent{11}`. Afterwards, the reaction
  `createReactionOnCast`.
- `Spell::ProcessSpells` 0x720300, once per turn: decay of the spell grid, worship sites (fn_0072BF80), icons
  (fn_00727350), `GPlayer::ProcessSpellIcons`, **first all the `ProcessMaintainRequest`**, and then, per
  spell, `ProcessSpellSeed` (vt 0x500) and `Process` (vt 0x528); a 5 deletes it.
- `ProcessMaintainRequest` 0x7204D0 (always 1): age += 0.1 s; `age > duration` (with duration ≥ 0) → CloseDown; a
  creator that is not functional → CloseDown and creator = NULL; `enabled = 1`; `creator->UpdateSpellInfo`; hand spells
  (castType IN_HAND) move castPos to the hand. If it is open, **the strength is read before paying for the turn**
  (`psInfo.power` is the previous strength), it pays for the turn and marks the grid. If it is closed: `enabled = 0`,
  `power = 0`.
- `CoreProcess` 0x720660: if it is open, `Recharge` and, with `power <= 0`, CloseDown. Then one PSys step with the
  PSysProcessInfo; if it returns 5, its reactions and the PSys go away. `Process` 0x720710 returns 5 when there is no
  PSys left.
- `CoreCloseDown` 0x720160: `closedDown = 1` and CloseDown of the PSys (vt 0x118).
- `ToBeDeleted` 0x71FD90: leaves the list, deletes the PSys and the reactions, **also deletes the linked seed** (vt 0xC
  on +0xAC) and then CloseDown.
- Accessors: `Spell::GetMagicInfo` 0x7201D0, `GMagicEffectInfo::GetInfo` 0x720730, `GetParticleType` 0x720130, `Spell::IsCastFromHand` 0x721510 (the seed's castType is IN_HAND), `Spell::IsScriptCasting` 0x720270 (the creator is the player at g_game +0x205A5B, the neutral one).
- The magic type of a spell is at spell +0xB4 (`SpellStormAndTornado::GetMagicInfo` 0x72DB80 = 0xD37D10[spell +0xB4]).
- Plain `Spell` (class General, MAGIC_TYPE 0-9): vtable 0x9805B0, `GMagicInfo::AllocSpell` 0x5FB450. Base virtuals: `InitWithObject` vt 0x538 = 0x7200E0, `CalculateCostToMaintain` 0x720810 (= costPerGameTurn), `CloseDown` vt 0x530 = 0x55CE40 (→ `CoreCloseDown` 0x720160), `HasEnoughChantsAndLifeForRecast` vt 0x548 = 0x55CE00 (`mov al, 1`: always 1).
- `InitWithObject` 0x7200E0: `InitWithPos(creator, the object's +0x14 ...)` and, if it worked, `psys->AddTarget(object)`.
- Statistic in `InitWithPos`: player +0xA44, `fn_0056A4D0` (a jump table of `inc`): one more spell of that type.
- `InitWithPos` also adds a minimap blip when g_game +0x205A28 == 1 (`fn_0079DDF0`); `ToBeDeleted` removes it (0x79DD20). Not ported.
- In `ProcessMaintainRequest`, castPos follows the hand quantised to MapCoords (ftol(x × 6553.6)), with altitude 0 (0x72056B..0x720595). For a human caster, `fn_00720460` / `fn_005D1260` give the interface's "can't cast here" feedback.
- The `ProcessMaintainRequest` loop of `Spell::ProcessSpells` is `Spell::ProcessMaintainRequests` 0x720390.
- In `Spell::CloseDown` 0x55CE40 → `CoreCloseDown` 0x720160: the first close-down of a spell cast by this computer's interface stops the hand's grain raise (`fn_005B2F10` → `HandStateGrain` `fn_005B2F40`).
- `ToBeDeleted`: after deleting the seed (vt 0xC on +0xAC), `fn_007213B0` clears the link.
- `Spell::SetInterfaceStatus` 0x7201F0 stores the casting `GInterfaceStatus*`. "My interface" means iface == `MyInterfaceStatus`; the check is `NetUnsafeIsMyInterfaceCasting` 0x7201E0.
- Spell class sizes and vtables: SpellHeal 0xEC bytes, vtable 0x8F4A8C, `GMagicHealInfo::AllocSpell` 0x5FBD40 (no state of its own). SpellResource 0xF0 bytes, vtable 0x8F6AD8 (AllocSpell 0x5FAC20). SpellShield vtable 0x9828D8, `GMagicShieldInfo::AllocSpell` 0x5FBA70. SpellStormAndTornado save type 0x10, `GMagicStormAndTornadoInfo::AllocSpell` 0x5FBAB0. SpellTeleport vtable 0x8F3FD4, `GMagicTeleportInfo::AllocSpell` 0x5FBDF0.
- Base functions of `SpellWithObjects`: `Process` 0x721290, `CloseDown` 0x721300, `ToBeDeleted` 0x720FD0, `ProcessSpellSeed` 0x7212F0; `Spell::SpellEvent` 0x720F40.

### Chants (`Magic/Core/Chants`, 0x720750..0x720A90)

Verified instruction by instruction:
- Safety level 0x720880: the maintained ones (FOREST, SHIELD, PHYSICAL_SHIELD) → `initialChants`; the others
  `max(min(coste/turno × (1000/ms por turno) × 5, initialChants), costPerEvent)`.
- Strength 0x720750: without a creator, 0. With `S > 0`, `chants/S` clamped to 0..1; with `S <= 0`, 1 if there are
  chants left. Then × `GetTribalPower` (0x7216F0, of the spell's player) × the seed's `+0x8C` × `+0xE4`.
- `PayFor(coste, forzado)` 0x720990: without a creator, 0; free (+0x5C), 1. With `divideCostsByTribalPower == 1`,
  cost / max(tribal power, 1). Subtracts the cost; if it ends up below the level and `isSpellRecharged`, the creator
  refills the whole deficit (forced) or at most the cost. Returns the strength.
- fn_00720830 (pay for the turn): **with cost 0 it returns 1 without calling PayFor**. `PayForOneEvent` 0x720A90 pays
  `costPerEvent` (without a creator it returns 0 without creating the mana path point). `Recharge` fn_00720910 refills
  the full deficit.
- Who pays (`MaintainSpell`, vt 0x58): `GPlayer` 0x64C430 gives everything **only if it is the neutral player**;
  otherwise 0, so the spells of a seed without an icon live off their initial chants. `GameThing` 0x56FED0 gives
  everything. The worship icon and the creature: see the creator callbacks below.
- Lightning cast by a player (real trace): 5000 chants, −50 per turn, strength 1 until it drops below 2500 (turn 50),
  0.98 on turn 51 and 0.8 on turn 60. It closes on turn 61 (the age is a sum of 0.1 floats and on turn 60 it still does
  not exceed 6.0) and the PSys ends on that same turn.
- `SetChants` 0x720FA0: +0x38 = +0x3C = chants.
- Inside `PayFor`: `fn_00720970` tests `divideCostsByTribalPower == 1`; `fn_00720FB0` adds what the creator's `MaintainSpell` gives.
- `CreateSpellPoint(chants, perTurn)` 0x7213D0 makes the mana path sprites. Only spells whose creator is a worship site get them. Not ported.
- Creator callbacks: `WorshipSpellIcon::MaintainSpell` 0x77F6F0 → `WorshipSite::MaintainSpell` 0x77BC50 / `UseChants` 0x77BBB0 (the site pays; an icon without a site pays from its own store). `Creature::MaintainSpell` 0x4F8350 (the creature's physical energy; not ported).
- `UpdateSpellInfo` (vt 0x5C): `GPlayer::UpdateSpellInfo` 0x64C470 does nothing for the neutral player. It forwards only for a spell that has an interface status (a hand cast) → `GInterfaceStatus::UpdateSpellInfo` 0x5DC8F0, which fills +0x00 the status's position, +0x0C the hand, +0x18 the camera's forward and +0x24 the hand's velocity. `WorshipSpellIcon::UpdateSpellInfo` 0x77F750 uses the icon's player's when +0x8E0 == 1. `Creature::UpdateSpellInfo` 0x4F8750 (not ported).

### Events and effects (`SpellEvent`, `ECS/Effects`)

- `Spell::SpellEvent` 0x720F40 ignores types 1 and 11. `ApplyDefaultSpellEffect` 0x720C30:
  - if it is closed, nothing;
  - the spell moves to the event (and 0);
  - EffectValues of the effect × the strength returned by `PayForOneEvent` (with 0 it is not applied and returns 0),
    × tribal power × `event.strength`;
  - type 4: `SpellHitSpell` with the target; if the other one does not fall, it returns 0;
  - type 7 (after paying for the event): with a target, `CanBeDestroyedBySpell == 1`, no reaction and no direction;
    **without a target it applies nothing but still goes on to the reaction and returns 1** (0x720DB4 jumps to 0x720EBC);
  - type 5 with a target: if it accepts it, `ApplyEffect` (and the heal one removes poison);
  - the others (and 5 without a target): `ApplyEffectToMapPos` at the spell's position;
  - at the end, the reaction `createReactionOnEvent` and `+0x2C = event.velocity`.
- With `checkShields` it looks for a shield (fn_006D0BC0) and sends **itself** a type 4 event with itself as the
  target (0x720D84). Pending with the shields.
- `SpellHitSpell` fn_00720B70: cost = own strength × `costPerShieldCollide`. If the other one has strength 0 → 1. The
  other one pays forced and this one pays one event. If the other one is left without strength and this one with
  strength → `SetUpDestroyedReaction` and 1; otherwise `UpdateStruckReaction` and 0.
- `EffectValues` (0x40 bytes): +0x08 the 7 numbers (burn, crush, hit, heal, push, alignment, belief), +0x24 the radius,
  +0x28 who applies it (the spell's creator), +0x3C the player. `*=` (0x525720) only scales the 7 numbers.
  `IsDestructive` 0x5258C0: burn, crush, hit or push > 0.
- `ApplyEffectToMapPos` 0x525100: cells of pos ± R; every available object that accepts the effect, with
  `dist(pos, centro de fuego) ≤ R + radio de fuego` and `|alt(pos) + pos.y − (alt(obj) + obj.y)| ≤ altura + R`. No
  attenuation. In `Object` the fire centre is the position (0x639AA0) and the radius is `Get2DRadius` (0x639AC0 → vt
  0x64).
- `Object::ApplyEffect` 0x637980 (villagers do not override it):
  - damage = positive crush and hit × the defence multipliers (0x637D00; before that it passes the heat to the fire); healing = heal × its multiplier;
  - heal → `IncreaseLife`, damage → `ReduceLife`; if life drops to 0 → `DestroyedByEffect` (a villager dies);
  - crush > 0.01 on something that can be crushed and without its own reaction → REACT_TO_OBJECT_CRUSHED (18), started
    by whoever applies it (or the object itself) and with the player **of the object** (vt 0x1C; for a villager, the
    owner of its town);
  - returns `(1 − vida0)/curación + vida0/daño`.
- `FireEffect::ConvertTemperatureToDamage` 0x72EEC0: 0 below Tc; otherwise `(T − Tc)/Tc ×
  defenceMultiplierBurn × 0,1`.
- `GAlignment::Update` 0x414410, the alignment change left by the effects: in
  [Spell effects](magic.md#spell-effects-galignmentupdate-0x414410).
- The reactions (`CreateReaction` 0x6E3D70, `SpreadReaction` 0x6E3E10): in [Reactions](magic.md#reactions-ecseffectsreactions).
- `fn_00720AE0` builds the spell's EffectValues (effect info, applied by the creator, the creator's player). `fn_00720B20` scales them by `PayForOneEvent`'s strength and fails when that strength is 0.
- After a hit by a player's spell that is not a creature cast: `GPlayer::ConsiderMakingCreatureMimicPlayer` 0x4EA900 with the action from `fn_004E9DF0` (creature, not ported).
- Effect.cpp spans 0x524EF0..0x525910. `fn_005250A0` → `fn_005250D0` fill the 7 numbers and the radius from a `GEffectInfo` (the base of `GMagicEffectInfo`). `operator*=` 0x525720 is reached through `fn_00525670`, which is skipped for a factor of 1.
- `EffectValues::GetPlayer` 0x5254C0 looks at AppliedBy (+0x28) first; `GetCausedPlayer` 0x525910 looks at +0x3C first, otherwise AppliedBy's player. They only differ when `Abode::ApplyEffectsDueToPhysicalDestruction` sets +0x3C to the hand's player while AppliedBy is the hitter. `Town::UpdateAggressor` reads `GetCausedPlayer` (0x73C9BC).
- `Object::ApplyEffect` 0x637AFE..0x637B7D: with a destructive effect (`IsDestructive`), a town (GetTown vt +0x48), an AppliedBy and `ConvertTemperatureToDamage(burn) + damage != 0` → `Town::UpdateAggressor` 0x73C9B0 with `GetCausedPlayer`.
- Life reduction is vt 0x5B8 with the effect's player: `Abode::ReduceLife` 0x405D90, `Field::ReduceLife` 0x52A0A0 (no change, no building site), `Object::ReduceLife` 0x637810 for the rest, `Creature` 0x47DD00 (not ported).
- `Object::DestroyedByEffect` is vt 0x5F8: `Object` 0x6378E0 = ToBeDeleted (features too; `CleanupWhenDeleted` 0x6377F0 does RemoveMapObject vt +0x548); `Villager` 0x7502D0 → `VillagerDead(2 SPELL)`; `Animal` 0x41B1B0 → `Living::SetDying`; `Field` 0x52A010: fn_0052A030 sets crops, growth and food to 0 and `SetTemperature(0)`, then deletes its fire (vt 0xC, 0x52A024); `Abode::DestroyedByEffect` 0x403F80 (building site, ghost; not ported).
- `Object::FillInEffectDefenceMultiplier` (vt 0x5C8) takes the info's defenceMultiplier* (+0x90..); creatures override it (0x478C00). `GetDamageEffect` 0x637D00 first passes the burn to `FireEffect::ApplyEffectToFireEffectIfNecessary` 0x730670.
- `IsEffectReceiver` (vt 0x774): `Object` 0x4029E0 = 1. `Villager` 0x751D70 refuses a heal when the villager is dead; otherwise it uses vt 0x530 `Villager::IsReachable` 0x756460 (UNVERIFIED, taken as 1). `Pot` 0x66D650 receives a burn only with something in it (+0x70); `Field` 0x528900 only while ValidForPlaceInHand (vt 0x6FC; inferred: while it has food to give).
- A villager's `GetPlayer` (vt 0x1C) is its town's owner (`Villager::GetPlayer` 0x7502F0).

### Cast rules (`Magic/CastRules`)

- In the `GMagicInfo` vtable the symbols have swapped names:
  - **vt 0x30 is the check at a position**: base 0x5FB420 = 1; heal 0x5FBD20 = `FindTargets`; resources
    0x5FBA00 = land; creature 0x5FA7E0 = 0; forest 0x5FAE80; teleport 0x5FBE50;
  - **vt 0x2C is the one for an object**: base 0x5FB430 = vt 0x30 at its position; resources 0x5FAC00; forest
    0x42D8E0 = 0; creature 0x5FA7F0.
- The rule fn_005FB5D0: inside the map (10 m cell < size) and, depending on `castRuleType`: 0 always, 1 land, 2
  influence `> 0`, 3 both.
- `GMagicHealInfo::FindTargets` 0x5FBB00: R = `dummyVar` (10 / 35) and maximum `maxToHeal` (20 / 100), both × the
  tribal power if there is a spell. It walks `ceil(2R/10)²` cells in a spiral (GUtils::Spiral 0x74D7E0, table +x, +z,
  −x, −z) and counts their living mobile objects that accept the effect and `CanBeHealedByHealSpell`, closer than R.
  **It does not check whether they are missing life**: any living one counts. With a spell, each one becomes a target
  of its PSys.
- **`SPELL_AT_POS` checks nothing**: the creator is the neutral one and the check flag goes to 0. Moreover
  `SpellHeal::InitWithPos` 0x72D870 does not look at how many it found. So a script heal is cast even if there is
  nobody. Only the hand asks (`SpellSeed::CanCast` 0x729150: the rule and then vt 0x30).
- `GMagicCreatureSpellInfo` vt 0x2C 0x5FA7F0 accepts a Creature whose mind allows it (`fn_004F5230`).
- `GMagicHealInfo::FindTargets` makes each target a target of the spell's PSys through `fn_00720140` (psys->AddTarget, vt 0x114).
- `SpellSeed::CanCast` enters at `fn_00729120`: the cast rule for the seed's player, then vt 0x30 with the seed's creator.

### Seeds and one-off miracles (`SpellSeed`, `OneOffSpellSeed`)

- **fn_00729900 works the other way round from its first reading**: with 0 → `+0x90 = 1` (ready); with another value →
  `+0x90 = 0, +0x94 = 0`. The worship icons pass 1 (the seed waits `delayBeforeSeedActive` = 1.5 s) and
  `CreateSpellIntoHand` passes 0: **a one-off seed is ready as soon as it reaches the hand**. Moreover
  `InterfaceSetInMagicHand` 0x728810 already sets +0x90 = 1.
- `CreateSpellIntoHand` 0x72A730: with the hand free, it looks for the player's worship icon for that seed
  (`GPlayer::FindBestSpellIconForSpellSeed` 0x64BF40, which asks for an icon from which the spell can be requested **with
  chants available** at its site) and, if there is one, creates that icon's seed (fn_007282A0: its creator is the icon,
  `worship::icon::CreateSeed`); otherwise, the loose seed (fn_00728300). It marks it "ever enabled"; `+0x72 = 1`; it
  charges it for free with its whole cost; it puts it in the hand, ready. In Land1 there are no icons, so it always comes
  out loose.
- `InterfaceSetInMagicHand` 0x728810: `SetPowerUp` of the current level. Without chants to recast (or with bit 1 of
  +0x54) the seed is deleted (3); otherwise it clears +0x98, +0x70 and +0x94 and is left ready.
- `ProcessInHand` 0x729930 (every turn in the hand): +0x94++; ready when `turnos × 0.1 > 1.5`; if its spell has closed,
  the seed is deleted.
- Also ported: `StoreChantsAndAgeFromSpell` 0x728780, `ClearSpellLink` 0x728200 (if the spell is still tied to this
  seed, CloseDown of the spell; otherwise, only of its PSys), `ProcessFromSpell` 0x728F70 (always 1) and `Cast` 0x729520.
  Casting from the hand is below.
- Seed accessors: `SpellSeed::GetMagicInfo` 0x7290F0 (the magic at its power-up level, the base type for an empty level); `GetChantNeededForSpellSeed` 0x729860 = costToCreate(pu) − the store; `SetChantStore` 0x729A50 (symbol "GetChantStore": +0x74 = +0x78); `AddToChantStore` 0x729A30; `SpellSeed::GetMesh` 0x729850; `IsSpellCastInHand` 0x729820; kept in hand 0x729840; `DoPostCastThings` 0x729260 (`fn_007281A0` links the spell).
- `SpellSeed::GetPower` 0x7298B0 = min(store / costToCreate, 1). It divides with no test: store / 0 is inf or NaN, and the min against 1.0 [0x8AA390] then gives 1.
- Seed init: the common init `fn_00728140` zeroes +0x74/+0x78/+0x7C/+0x80/+0x84. The ctor 0x7280A0 then stores 0 in +0x7C/+0x84/+0x74/+0x78 and −1 in +0x80. Only `StoreChantsAndAgeFromSpell` 0x728780 writes +0x7C = −1 ("none").
- `SpellSeed::SetPowerUp` 0x729B30: a charge above the new level's cost goes back to the worship site. For the local interface, `fn_00729C40` plays SpellDialogue sample 10 / 11 / 12 for PU 0 / 1 / 2 (jump table 0x729C80; none for −1, 0x729C48): `GAudio::PlaySoundEffect` 0x429D60(NULL, sample, mode 2, loops 0, 0, is3D 0, AUDIO_SFX_BANK_TYPE 9).
- `DoPostCastThings` 0x729385..0x7293E6: if the seed is my interface's, `HelpProfile::Trigger` CastCreatureSpell (10) for a `GMagicCreatureSpellInfo` (vt +0x38 non-null), otherwise CastSpell (9).
- `InterfaceSetInMagicHand` 0x72881D..0x728859 (my interface): HelpProfile StopSpell (13) when the seed still has its spell (+0x60), otherwise GetSpell (12). `fn_005DCA20` stores the interface's last seed type, which the R gesture repeats; `fn_005DCA40` reads it.
- `ProcessInHand`, on the seed's first turn in the local interface's hand (`IsInterfacePowerUpWhenInHand` = 1): `SetupPowerUpGestures` `fn_005CEE30`. It also calls `Object::ProcessInHand` 0x639AD0 (not ported).
- `ClearSpellLink`: when the spell is no longer this seed's, `fn_00720190` closes only the spell's PSys.
- `fn_00728FC0` (the seed follows its spell): `SpellSeed::InsertMapObject` 0x728F30 is empty, so the "not in the map" test (vt 0x178) always passes. A NULL spell (+0x60) passes the "spell open" test (0x728FF6).
- `SpellSeed::GetPlayer` 0x729800 is the player of the interface status (+0x64), NULL without one.
- `ClearSpellIconLink` 0x7281D0 → `RemoveFromSpellIcon` 0x7281C0 (`fn_0077F7D0`).
- The seed's 2D radius is `Object::Get2DRadius` 0x638180 (vt +0x64, 0x6022D9) of the info's mesh (`GetMesh` 0x729850). The Game3DObject +0x40 keeps that mesh even while the seed is not drawn.
- `CreateSpellIntoHand` and `fireball::Catch` require `GInterfaceStatus::IsHandReadyForObject` 0x5DC890 (`IsSpaceInHands`: nothing held; the bit 0x20 of +0x24 that it also tests is not identified). `fn_005DC830` returns the seed in the player's hand.
- `OneOffSpellSeed::Create` 0x72A2F0: seed 0..29; `MobileObject(pos, info 0xD39F3C, 0, 0, scale 1)`. The orb is always
  drawn at scale 1 and +0x6C keeps the scale that will be passed to the seed.
  - Shared mesh `.\data\spells\meshes\O_Bibble_up.l3d`: a dome from 0 to 4.5 m above the ground, with UVs in 0..0.25
    (a 4×4 atlas).
  - `UpdateFrame` 0x72A570: `fase = fmod(fase + ms × 18 × 0.001, 16)`, frame = int(phase), offset
    `u = (cuadro % 4)/4`, `v = (cuadro / 4)/4` (vt 0xE8 receives (u, v)). In openblack the offset goes in
    `UvScroll {u, v}` and the shader adds `u` in quarters.
  - **The orb is additive (faithful, corrected on 2026-10-01 with the capture of the original).** The mesh has a
    physics submesh (`Smooth`, not drawn) and the visible one, the cap, an `AlphaTextured` primitive (type 4, byte
    +5 = 5: two-sided and repeat) whose skin 0xF49809BD ARGB4444 is a dark turquoise orb (51, 119, 136) with a white
    highlight at the top left, almost opaque (alpha 13-15 of 15, or 0 outside).
    - But the file is not what decides: `CallVirtualFunctionsForCreation` 0x72A450 loads it with
      `GJUtils::GetSharedMesh` 0x57DFB0 and `MaterialProperties` {1, 1, 0, 1, 1} (bytes at 0x72A474..0x72A485). Byte
      +3 = 1 makes `PGetSharedMesh` (0x57DF18) call fn_0057E1D0, which applies `GJUtils::SetMaterialProperties`
      0x57E120 to all the primitives when the mesh is loaded:
      - type 4 → 6; if +4 = 0 → 3; if +0 (additive) = 1 → 13; if +1 (writes Z) = 1: 6→5, 13→12, 8→3, 16→9; otherwise:
        5→6, 12→13, 2 or 3→8, 9→16;
      - +2 (two-sided) sets or clears bit 0 of byte +5.
      - For the orb: **mode 12** (`fn_0082EB50`: `SRCALPHA / ONE`, colour and alpha = texture × diffuse, writes Z) and
        **single-sided** (byte +5 = 4).
    - `Draw` 0x518E90 tints the object with `0x96FFFFFF` (byte from [0xBE8E8C]; fn_0080BF10 multiplies the diffuse:
      alpha 0xFF × 0x96 >> 8 = 0x95) and calls `SetGlobalAlpha(1)` (LH3DObject vt 0x48, bit 0x80 of the flags), which
      switches to the alternative mode table 0xC387C8. That table **leaves unchanged** the additive modes 10-13 (read
      from the executable).
    - Result: the orb **adds** its texture × light × (0.58 × texture alpha) to whatever is behind it. Over the sand by
      day it comes out almost white and pearly: the turquoise texture turns sky blue and the highlight, saturated
      white. The background shows through with green and pink tones.
    - Before, openblack blended it as mode 5 (`SRCALPHA / INVSRCALPHA`, two-sided). That covered half the background
      with the dark turquoise: a dark greenish orb. The earlier investigation (N·L light, ambient 90/256, alpha 0x95)
      was correct, but it missed this material change at load time.
    - openblack:
      - `graphics::MaterialProperties` and `L3DSubMesh::SetMaterialProperties` (the type change of 0x57E120, with the
        type stored in `Primitive::materialType`) and `L3DMesh::SetMaterialProperties` (fn_0057E1D0), in
        `src/3D/L3DSubMesh.*` and `L3DMesh.h`;
      - `Game.cpp` applies it to `O_Bibble_up` when loading it;
      - `Renderer::DrawSubMesh`: an object with `components::Alpha` (the 0xC387C8 table) keeps the additive blending
        of its additive primitives, and those in modes 11 and 13 without writing Z.
    - Differences remaining against the user's capture of the original, **pending**:
      - in the original the orb floats higher above the dispenser and looks bigger;
      - in openblack the effect of the FIRE seed is seen as a yellow core inside the orb, and in the original it is not
        seen (at its centre there is a sky-blue blotch);
      - the original's sand is lighter, and since the orb is additive the background changes its look a lot.
    - To sort it in the Z-sorter, `Draw` moves its position forward towards the camera by its radius (vt 0x60) and
      then restores it. That way the orb is painted after the seed inside. `DrawSpellGraphic` receives the high byte
      of the diffuse (0x95) as alpha. openblack: `components::Alpha` = 149/255 in `OneOffSpellSeedArchetype` (pass
      `MainBlended`). The forward shift by the radius is there: `one_off::UpdateFrames` stores
      `OneOffSpellSeed::sortPoint` = box centre + normalize(camera − centre) × radius (`Get2DRadius`: largest half
      extent in x/z × scale, 2.3 m) and `RenderingSystem` / `Renderer` sort the orb by that point
      (`RenderContext::sortPoints`).
  - **The orb always faces the camera.** `Draw` calls fn_00518720 every frame, active while the byte [0xBE8E8D] is 1
    (it is). This rotates the object's 3D matrix around the centre `c` of the mesh's box
    (`LH3DMesh::ComputeBoundingBox` 0x8081B0 at load time, all submeshes; here (0; 2.23; 0)):
    - `D = normalize(centro − cámara)` and `U = normalize(Y − (Y·D)·D)` (Gram-Schmidt with (0, 1, 0), static 0xCC62D0);
    - it builds the matrix with rows (U×D, −D, U), inverts it (fn_007FB3F0), scales it by +0x44 and sets the position
      to `centro − M·c`. That way the mesh's +Y points at the camera.
    - The visible submesh is only the top cap (y from 2.18 to 4.46, radius 2.28), so a round bubble is seen from any
      side. The physics one is a whole sphere from 0 to 4.37 and does not change when rotating.
    - Only the 3D object's matrix moves, not the object's position. The 4×4 animation does not depend on the rotation.
    - `Draw` draws nothing if +0x70 (the SpellSeedGraphic) is 0. `CallVirtualFunctionsForCreation` 0x72A450 creates it
      (except with object flag 0x100, which a new orb does not have): `SpellSeedGraphic::Create(pos, semilla, el
      jugador local, 1, pu)` and `SetAutoUpdate(0)`; `ToBeDeleted` deletes it. In openblack this is done by
      `OneOffSpellSeedArchetype` and `one_off::InterfaceTap`.
    - openblack: `one_off::UpdateFrames` computes `OneOffSpellSeed::facing` and `facingOffset`, and `RenderingSystem`
      draws the orb with them. `Transform` does not change: the dispenser compares its position and the physics uses
      the sphere.
- `InterfaceTap` 0x72A640: `CreateSpellIntoHand`, immersion 0xE, sample 0x6D (`G_SpellBubblePop_04`) and the orb is
  deleted (3).
- When it is created, the orb enters its map cell at once: MobileObject 0x607150+0xA9 → `Object::InsertMapObject` 0x636740. Its info (GMobileObjectInfo 25, 0xD39F3C) is type 20 MOBILE_OBJECT, so the orb goes at the head of the mobile list (0x636830).
- `OneOffSpellSeed::ToBeDeleted` 0x72A420 deletes the `SpellSeedGraphic` inside.
- `InterfaceTap` sound (0x72A6A5..0x72A6F4): bank InGame (GGlobal +0x3AC), owner the orb, sample 0x6D, is3D 1, track 0, at the interface status's +0xC8 (the hand's position), `GAudio::PlaySoundEffect` 0x429E30.
- **With the real hand** (faithful, `HandSystem.cpp` / `HandPlacement.cpp`):
  - The object under the cursor (`SendObjectDrawCollision` 0x5D56C0, exact triangle) reaches `ActionPressed`
    fn_005D1330 → `StartGrab` 0x5D1740 if `ValidForPlaceInHand` (vt 0x6FC) or `InterfaceValidToTap` (vt 0x740). The orb
    has both: it is a `MobileObject` (`Mobile::ValidForPlaceInHand` 0x425B00 = 1) and `InterfaceValidToTap` 0x72A630 = 1.
  - Pressing on it starts the grab (state 13). If it is released before 225 ms (`State_Grab` 0x5D5250, 0xE1) it is a
    **tap**: `Tap` 0x5D3930 → 0x5D38A0 → packet 0x20 → 0x5DA650 → `InterfaceTap`, and the charged seed goes to the hand.
  - If it is held down, **the orb itself is picked up**: `GenericPickup` 0x5D2800 (packet 0x13) → `PlaceObjectInMagicHand`
    → `InterfaceSetInMagicHand` 0x72A530 (it only marks the magic as enabled). It is carried as a `MobileObject`
    (`GetHoldType` 0x607120 = 6, `Object::GetHoldRadius` 0x638C00) and is dropped or thrown with physics: constants 9
    (`GetPhysicsConstantsType` 0x72A920) and the `GMobileObjectInfo` info 25 (0xD39F3C; its type 20 MOBILE_OBJECT confirms it, see
    above). The dispenser no longer sees it in its place and makes another one when
    recharging.
  - The tap and the grab require the hand to be inside the player's influence
    (`InterfaceMustBeInInfluenceForInteraction` 0x4028A0 = 1; `m_InInfluence` from fn_005D1120, type 1). Outside it
    nothing happens.
  - Selection volume: the mesh as it is drawn, rotated towards the camera (fn_00518720), so the dome that is seen from
    any side counts. **(inferred)**: if the ray hits the seed inside (`SpellSeedGraphic`, which is not an `Object`), it
    counts as if it hit its orb or icon.
  - The icons of the worship sites and of the town centres (`Object::ValidForPlaceInHand` 0x402870 = 0) are tapped on
    press (`StartGrab` → `Tap` immediately), with the same influence rule.
  - **Pending**: the hover tooltip text (fn_005D6D70: on an orb, `GetOverwritePickUpToolTip` 0x72AC50 = the text of
    its magic +0x110; the tap one is 0xEF7). Neither `GInterface::StartImmersion(0xE)` nor the `GameThingClicked`
    record of fn_005D36D0 are there.
- Map script (fn_00715150):
  - case 83, `CREATE_ONE_SHOT_SPELL(pos, semilla)` → Create(pos, the seed by name, −1, 1);
  - case 84, `CREATE_ONE_SHOT_SPELL_PU(pos, magia)` → the first seed of that magic and its level
    (`GetPowerUpFromMagicType`).

### CHL script (`Magic/Script/CHLSpells.cpp`)

- `SPELL_AT_POS` 0x70C190 pops curl, duration, radius, from, to and magic. `CastSpellAtPos` 0x70BD60 builds castData
  {radius, initialChants, duration, −1} and the PSysProcessInfo {+0x0C from, +0x18 to − from, +0x24 dir (0),
  power 1, +0x34 curl, active}. `SPELL_AT_THING` 0x70BFA0: with an Object, cast on the object.
- `SPELL_AT_POINT` 0x70C560 casts nothing: it returns the first spell of that magic closer than the radius
  (fn_007217A0). For the shields it calls fn_0072BA00.
- `SET_PLAYER_MAGIC` 0x70C6C0 (player, magic, enable) → `SetMagicTypeEnabled` (the counter of who has it).
  `HAS_PLAYER_MAGIC` 0x70C750 → "ever enabled", and **1 if the player does not exist**. The script's players are
  n − 1 (0 = the neutral one; `ConvertScriptPlayerToGamePlayer` 0x6EB9A0).
- `PLAYER_SPELL_CAST_TIME` 0x70C9A0: seconds since the last cast (FLT_MAX without a player).
  `PLAYER_SPELL_LAST_CAST` 0x70CA50: its magic. `GET_LAST_SPELL_CAST_POS` 0x70CAB0: its point. `GET_MANA_FOR_SPELL`
  0x70CD40: `costToCreate`.

### PSys linked to the spell (`Particles/SpellLink.h`)

How the spell owns its effect and advances it (`StrengthFloatProvider`, `EventConditionTrueWhenEnabled`, event 3 of
`LandscapeCollide`): in [PSys linked to the spell](particles.md#psys-linked-to-the-spell-particlesspelllinkh).

### Spell grid (`SpellGrid`)

`u8[64][64]` at 0xD9C370 (80 m cells). `MarkSpellGrid` fn_00721570 sets 0xFF where there is an open spell;
fn_007215C0 lowers it by 0x20 per turn when `g_game+0x205A28 == 1` (unidentified flag; here always).

- The decay step 0x20 is stored at 0xC22570 (u32).

### Order within the turn (`Magic/MagicLoop.cpp`)

`GGame::ProcessTurn` 0x54E5C0 calls, in this order: atmosphere (1), influence rings (2), players (3), dances (4),
forests (5), the living, fire (6), reactions (7), `Spell::ProcessSpells` (8), the particle containers (9), physics
(10), the PSys sounds (11), `GScript::Process`, weather (12), `CHand::GameTurnUpdate` (13) and the rewards (14). In
openblack `Game::GameLogicLoop` calls `magic::ProcessTurn` (1..8) after `livingActionSystem`, then
`psys::manager::ProcessTurn` (9) and the fireflies, the physics, `magic::ProcessSpellParticlesEndOfLoop` (11, was `ProcessPSysGameLoopEnd`), the scripts,
the weather things and the climate (12), `magic::ProcessHandTurn` (13), in the original's order
([engine-loop.md](engine-loop.md) §2); the rewards (14) are not ported. The spells' PSys are not advanced by the manager:
they are advanced by their spell in 8.

- Slot 11, `PSysGlobal::GameLoopEnd` 0x68F5B0: first the vortices (`fn_005FF330`, 0x68F5B9), then `fn_0069D340` (0x68F5BE, not identified, not ported).
- Fire is drawn per frame: `FireEffect::Draw` 0x730330 → `fn_00731560` with g_game_time_inc.
- The falling spell is drawn from `GGame::Process3dEngine` case 2 (0x54DDE0), with g_delta_time: `FallingSpell::Draw`, then its finish frame callback 0x526480.

### Hooks and traces

`OPENBLACK_TEST_SPELL`, `OPENBLACK_TEST_SEED`, `OPENBLACK_TEST_ONESHOT` and `OPENBLACK_SPELL_TRACE` are in
[openblack-internals.md](openblack-internals.md#debug-environment-variables). Checked with them: a heal at Land1's
store with 0 targets is cast anyway, closes after 20 s and is deleted on turn 200; a FIRE seed reaches the hand with
3500 chants, ready.

## Casting from the hand, gestures and hand effects (`src/Magic/Gestures`, `src/Magic/Hand`, `HandSpellSeed.cpp`)

What is below was read in the exe; what was not is stated.

### Gestures: the buffer and the recogniser (`GestureBuffer`, `GestureMatch`, `GestureTemplates`)

- **Input** (`GestureInput.cpp`): the sample is given by the mouse **with no button**. It is CMouse message 0, every
  28 ms of mouse events (`fn_005CEAD0`), with the terrain point under the cursor, or that of the last sample if it is
  outside. There are no samples while paused nor during the 0.4 s following a recognition. If the camera changed
  position in the frame (`GCamera::IsMoving`), **the buffer is cleared** on every `ProcessPowerUpSystem`.
- `GestureSystem::AddSample` 0x57BBC0: 80 samples in a ring.
  - A still sample is compared with the one from **two messages before** (0x57BC3A: head − 2, because the head has not
    advanced yet). 70 in a row like this clear the buffer, and that sample starts it again: it is the 72nd still
    sample.
  - `ProcessNewSample` 0x57C3F0 finds the corners on the fly:
    - corner = turn ≥ π/8·¾ (`FindCorner` 0x57BFE0);
    - merge or rejection by length (`MergeOrReject` 0x57C200; `LongEnough` fn_0057C630: 12 px, or between 4 and 12 if
      the recent box measures less than 50 px);
    - heading and octant of the exit (`UpdateHeading` 0x57C710). The octant rounds an exact .5 down (fn_00578700).
- `Gestures.jty` (`GestureSystemDataList::Load` 0x579AF0): 81 templates of 0x65C bytes.
- `MatchGesture` 0x579F10 → `Match` 0x57A050: first `MatchForward` 0x57A1A0 and, if the template allows it,
  `MatchMirror` 0x57A3E0 (negated turns, error without wrapping). Only three things are compared:
  - the sequence of turns (a turn smaller than T1 = 21π/128 can be absorbed; maximum error T2 = 3π/16);
  - the first direction;
  - the aspect class (0.15 / 4, fn_00579FA0).
- Packet (fn_0057A5E0), in mode 2 (the one of all the templates):
  - the point is the terrain point under the centre of the box of the matched samples;
  - the size is 1.05 × the half width of that box in the world, at that distance.

  It is the `size` of the circle of the storm and of the shields (the magnitude of the cast).
- The player's miracle gestures (SPIRAL selection) are **14**, not 12: FORK_DOWN, CYRILLIC_L, VERTICAL_SCRIBBLE,
  S_SHAPE, FORK_RIGHT, FORK_LEFT, FORK_UP, HEART, THREE, W_SHAPE, SQUARE_SPIRAL, INVERSE_SQUARE_SPIRAL, HOUSE and STAR.
  Each one recognises its stroke and none of the other 13 (`test_gestures`, `realData`).
- GestureSystem at g_game +0x25006C (0xC98 bytes); GestureSystemResult at +0x250070; the template list at +0x250064, read from ".\Data\Gestures.jty" (0xBEC8A4) in `GGame::LoadFiles`.
- T_corner [0xD064FC] = π/8·¾ (initialiser 0x57BB00); T1 [0xD064C0]; T2 [0xD064BC]; aspect classes [0x900110] / [0x900114].
- 70 still samples empty the buffer at 0x57BC90; the sample enters the emptied buffer at 0x57BCBF.
- fn_00578890: wrap(b − a) (> π → −2π, <= −π → +2π). fn_0057A150 is another wrap (|x| > π → x ∓ 2π).
- fn_00578730: octant of the heading + π/2 (0 = up on screen).
- fn_0057BF60: turn = wrap(Atan2P(c − b) − Atan2P(b − a)) on screen.
- fn_0057C590 / fn_0057C5C0: "far" = 4 px or more on either axis.
- fn_0057C820: the corner's turn relative to the previous one (the start keeps its own).
- fn_0057C8F0: sample with the land point of the most recent one if the cursor is off the land.
- Boxes: fn_005789D0 (samples s..b−1; those at (0,0,0) are skipped after the first) and fn_00578D70 (key points s..e).
- Key point indices (flags & 0xB, or the last one): fn_005788D0.
- Backward search: fn_0057BE10 (with flags), fn_0057C1A0 (corner), fn_0057BE70 (with flags or a far position).
- Aspect fn_00578E40 = (W + 1) / max(1, (H + 1)·ratio), with ratio = screen width/height (0xE85058 / 0xE8505A).
- ComputeAspect 0x578EA0: a template stores maxX / max(maxZ, 0.1). The template tool multiplied z by width/height (0x578F30).
- BuildFromSystem 0x578C20 takes the key points and the newest sample, and then calls ComputeAspect.
- GestureSystemData (vtable 0x8DF7E0), SetToZero 0x578BE0, Append fn_00578D30 (no bounds check).
- Template record (fn_005790A0, 0x65C bytes): 80 samples, then u32 count, gesture, positionMode, checkDirection, allowReverse, checkAspect (4 bytes; only the low byte is kept) and f32 aspect.
- fn_0057A0E0: the direction at idx against the template's first (mirrored 8 − d; 0 stays 0).
- fn_00579AB0: the gesture's first template gives the position mode.
- Centre: `LHRegionF::CentreCoord` 0x7DEBC0. Distance: fn_00442D50. Point on the edge: fn_0074CAF0, on the ray through (centre.x + width/2, centre.y).
- Mouse sampling: `GGame::MouseHandler` 0x54FFE0 adds the events' ms into [0xD019CC] (0x55001B) and sets it to 0, does not subtract (0x55006C); then `CMouse::ProcessPosition` 0x61A110. `GCamera::IsMoving` is +0x74, set every frame at 0x442660. Land under a pixel: fn_005E5620. Yaw: fn_00441E60 (cos, −sin) = the camera's right on the ground (inferred).
- Ideal shapes: fn_0068C650 / fn_0068C340 (per-gesture cache 0xD4E760); points (x + 100)/200, (100 − z)/200. GJPath 0x687BE0..0x687E60: fn_00687CB0 (re-measures), fn_00687C80 (length), fn_00687E30 / fn_00687E60 (point at t).

### What is looked for and when (`PowerUpSystem.cpp`, `GInterface::ProcessPowerUpSystem` 0x5CF300)

- It runs at the end of each `InterfaceActionProcess`: once per frame (`ProcessFrameInputs`) and again per turn
  (`GInterface::Process`, with the time of the last frame). Still not found: who sets bit 0x02 of
  m_Buttons and who reads the 40 s cap of the repeat.
- Order of each call:
  1. the clearing by the camera, the 0.4 s wait and the expiry of the pending circle (5 s);
  2. **the circle**: with the action pressed (m_Buttons 0x200) and a seed with `sizingGesture` (CIRCLE: storm,
     shield and physical shield), the circle stores position and size;
  3. with an **icon** seed that is charging (`HoldingChargingSeed` fn_005CEF50), the seed's power-up gestures
     (fn_005D0000); if it already has a power-up, SCRIBBLE removes it (packet 0x6A). If there is no such seed, the
     stage of the open selection (`SelectionStage` 0x5CFAE0, cap `selectionSystemTimeOut` = 30 s);
  4. **SCRIBBLE cancels**:
     - it shakes off whatever is in the hand, if it is in the influence and is `ValidToShakeFromHand`
       (`DoRemoveFromHandVisual` + `ForceDropHeld`; a seed goes back to its worship site or is deleted);
     - or, with the hand empty, it cancels the charge of the most charged icon (packet 0x1E);
  5. with the hand free, SPIRAL / INVERSE_SPIRAL open the selection if there is a requestable icon of that category
     (`OpenSelection` 0x5CF010);
  6. R_SHAPE repeats the last miracle (packet 0x26), if the player can.
- **One-off seeds** have no icon, so with them there are no power-up gestures (only with those of a worship icon).
  SCRIBBLE does shake them off.
- **API for worship** (`PowerUpSystem.h`): `gestures::SetIconProvider(IconProvider*)`. Without a provider the selection
  never opens; `Worship/GestureIconProvider.cpp` registers its own. The provider answers:
  - `AnyRequestableIconOfCategory` (fn_0064BE40), `ForEachRequestableIcon` (the walk of OpenSelection) and
    `IconValidForRequest` (fn_0064BEC0);
  - `RequestSpell` (packet 0x25), `CanRepeat` / `RepeatLast` (0x26) and `CancelMostChargedIcon` (0x1E);
  - `AnyIconChargingForHand` / `MaxChargeFraction` (the PHandFX charge bands);
  - `PowerUpAvailable` / `SetPowerUpCharge` (0x6A).
- Not ported:
  - the help (`HelpProfile::Trigger` 0xE..0x17): with `OPENBLACK_GESTURE_TRACE=1` its events go to the log;
  - the immersion (force feedback 3, 8, 9, 10);
  - the HUD gesture icons (`DisplayGesture` fn_0068ABA0, `S_Gesture0/1.raw`, not read). The `LookingFor` table is
    filled in.
- ProcessPowerUpSystem addresses:
  - wait [0xBF1AF8] = 0.4 s; fn_005CE420 = Success (sparks fn_00689790 and immersion 3); fn_005D2770 = empty and reseed;
  - fn_005CF220 = recognise; SetupPowerUpGestures 0x5CEE30 / fn_005CEE80; fn_005CF270 = the level's gesture;
  - fn_005CF1C0 = selection open only with the hand ready; fn_005D3880 = the seed in the hand; Object 0x636AA0 = ValidToShakeFromHand (1); `IsHandReadyForObject` 0x5DC890; fn_00729AC0 = sizingGesture; GInterfaceStatus fn_005DCA20 / fn_005DCA40 = R's seed.
  - Exits that empty the buffer: temple (g_game +0x205A28 == 1), status +0x24 & 0x20, !IsActive(), status +0x3C and the byte [0xC4CCEE]. The cameras that allow gesturing while moving are CameraModeNew3 in the arena, Dance and Follow.
  - Help events 14..22 with their call addresses (PowerUpSystem.cpp `Help`); 19 GestureCreatureSpecial (fn_005CFDE0 0x5CFF0B).
  - Icons: the walk fn_005CF040 (player +0xA48 → sites +0x34 → list +0xE0, link +0x110); `FindBestSpellIconForSpellSeed` 0x64BF40; `RequestSpell` 0x77FB40; CanRepeat fn_0064BD90; MaxChargeFraction fn_0064BB10 / fn_0077FE80; the 0x1E cancel fn_0064BCC0 (+0x138; which icon it cancels: see Pending); PowerUpAvailable fn_0077FBF0; SetPowerUpCharge 0x77FC30; handlers 0x25 0x5DABA0, 0x26 0x5DABF0.

### Casting from the hand (`HandSpellSeed.cpp`)

- `ActionPressedHolding` 0x5D1560 with a seed: over a valid object (in the influence) it applies to the object;
  otherwise, to the ground under the hand, which must be in the player's influence. Depending on the `castType`:
  - **HAND_GESTURE** (storm, fire, shields, flocks): it arms on press if it can be cast there
    (`ValidToApplyThisToMapCoord` 0x728720 = ready and `CanCast`); otherwise, `FailApply`. Arming
    (`BeginApplyOnRelease` fn_005D2730) resets the buffer with the current sample and starts the IN_GAME 3 loop
    `G_HandGesture_02`. It casts on release (states 8/9, 0x5D48D0).
  - **HAND_POSITION** (forest, heal, teleport, destroying lightning): it casts on press (`DropOnMapCoord` fn_005D1850).
  - **IN_HAND** (food, wood, water, lightning): state 10/11. While it is held, one apply per turn
    (0x5D4C10 / 0x5D4D00); on release, `ApplyUnlockProcess` 0x728EB0.
- `SendApplyToMapCoord` 0x5D3340:
  - one packet per turn (`m_ApplySentTurn`);
  - with a pending circle, the point and the gesture are those of the circle;
  - **fn_00729AF0: a seed with `sizingGesture` needs that gesture in the packet**; otherwise, `FailApply`;
  - a power-up level being charged goes with the cast;
  - then `SpellSeed::ApplyThisToMapCoord` 0x728E20 (the magnitude is the size of the gesture) and the result
    (fn_005DA100): the seed stays in the hand if the spell is kept in it; otherwise it leaves (0x16) with the visual
    SUCEED_CAST (3).
- `FailApply` fn_005D18F0: visual 4 (`SF_FailedApply`) at the point and `G_SpellCastFailure`.
- Hold parameters (0x728640..0x728680): MAGIC until the seed is ready (`Cwiggle` at half length) and then its
  `holdType`; radius `holdRadius × scale`, plus `holdLoweringMultiplier`. The seed's mesh is only drawn in the hand
  with `isSpellSeedDrawnInHand`: fire, lightning, heal and storm are only their effect in the hand.

### The hand (`HandMagicFX.cpp`: PHandFX and the effect in the hand)

- **Effect in the hand** (CHand fn_0046E7B0 / `DrawSpellInHand` 0x46E680):
  - it is the level's `particleTypeInHand`, and `SetPowerUp` creates it again;
  - it is advanced every frame by `max(1, g_game_time_inc)` ms, strength = that of the seed's PSys, magnitude = the
    hand's scale, and only with the seed ready;
  - `UR_FollowLocalHand` 0x69A6A0 and `UR_FollowCastPosn` 0x69FE30 (`Rules/HandFollow.cpp`) carry it to the hand;
  - effects that advance per frame are drawn where the last step left them (`manager::SetPerFrame`), without
    per-turn interpolation.
- **PHandFX** (ctor 0x68CB10, `Draw` 0x68D0C0, `Band::Draw` 0x68D6D0): `Power_Up_Band.L3d` bands at scale 10 on the
  root bone, at 10 + 40·index, spinning at (1 + 0.2·index)·12 rad/s.
  - **Matrix** (`Band::Draw` 0x68D8BB..0x68D9EA): the local one is 10·I with the translation (0, 0, +0x18 + index·+0x1C)
    (0x68D900..0x68D909), that is, along the **root bone's own Z axis** (the forearm). Only once it has arrived
    (f ≥ 1, 0x68D90D) each row rotates its (x, y) by the angle +0x20 around that Z (0x68D922..0x68D9DB:
    (x, y) → (c x + s y, c y − s x), c stored as a float at 0x68D929, s on the stack; `affine::TurnRows(2)` (was `lh_matrix`)).
    Afterwards fn_007FAFF0 0x68D9EA = local × bone (rows; in glm bone · local). The bone is the first 0x30 bytes of
    the matrix pointed to by CHand +0x47F0 (copied at 0x68D0F2..0x68D100; `PrepareForDrawing` 0x46CAE5 copies the
    same one into the matrix of the hand object). Result: a bracelet that goes around the wrist and spins about the
    forearm's axis. openblack had it along Y and spinning about Y (the ring hung below the hand and turned edge-on);
    corrected (`DrawBand`).
  - Permanent: `SetPULevel(pu + 1, 1)` from `SpellSeed::SetPowerUp` 0x729BFC..0x729BFE (pu = POWER_UP_TYPE: −1 no
    power-up, 0 = PU1, 1 = PU2), so 0 / 1 / 2 rings (maximum 5); they start at 2.4 s; alpha 20→130 in 0.85 s, with
    matrix lerp. They fly from in front of the camera to the root bone of the hand (the wrist).
  - Colour (`Band::Draw` 0x68D849..0x68D8B1, on every draw): +0x4C = `GetPlayerColour` 0x64D800 of the local player
    (g_game +0x205A59) with the band's alpha; +0x50 = per-channel lerp of the ctor's colours +0x34 / +0x38
    (fn_0068CA30, args 8 and 9), 0 in all callers. A single draw per band (0x68DD46 vt+0x104). User's recollection:
    a translucent red ring reaches the wrist when picking up a miracle (the exe confirms it: player 1's red).
    openblack: `components::ObjectColour`.
  - Temporary: 5 on gaining a level, 0.1 s between them; alpha 20→120, with slerp.
  - Charge ones: duration lerp(3.5; 1; c), one every lerp(6; 0.3; c) s.
  - They arrive flying from 4 m in front of the camera, at half scale (the matrix 0xEA1CF8 is the camera's, inf).
  - `AddSpellToHandVisuals` plays `G_SpellPowerUpBand`; the shake, `G_ShakeHand_01` and a band that leaves.
- **The hand glow** (a second pass with additive `S_Hand_Flow`, player colour, alpha 0.8, 8×4 atlas at
  −20 frames/s) is computed (`hand_fx::GetGlow`) but **not drawn**: it needs a skinned mesh shader with two textures
  (colour and `S_Hand_Flowa`).
- `PHandFX` loads the same `Power_Up_Band.L3d` as the icon bands (`fn_0068CC70` 0x68CC7D..0x68CC99), with MaterialProperties {additive 1, Z 0, two-sided 1, change 1, alpha 1}, so the band becomes mode 13 (SRCALPHA / ONE, no Z write, fn_0082ECD0).
- Band: 0x48 bytes, vtable 0x936B1C, ctor fn_0068CA30; only `LH3DObject::Create` 0x68CA98 and vt+0xF4 (no vt+0x78), with +4 = 0x10009 (0x816537): it neither receives nor casts a projected shadow.
- Bands: permanent fn_0068CCC0 (index count + 1, at the head); temporary fn_0068CD30 (index count); charge fn_0068CDA0 (alpha lerp(3, 5, c) → lerp(15, 50, c)).
- fn_0068D000 removes the newest; vt 0 RemoveAllPermBands 0x68D060; vt 8 AddSpellToHandVisuals 0x68DE20; vt 0xC SetPULevel 0x68DDA0; delayed start fn_0068DE90.
- DrawHandFX 0x68DD60 → Draw 0x68D0C0 every frame with g_game_time_inc × 0.001 (0 while paused). Interpolation: fn_0044CF90 / fn_0044E9F0.
- DoRemoveFromHandVisual: the band returns with alpha 5 → 50 reversed over 1 s (sound 0x77 is in audio.md).
- CreateTribalPowerColumn 0x68DEF0 → `PowerSpinRunner::Create` 0x66F730 (the tribe's name spinning): not drawn.
- In-hand effect: fn_007285E0 (the level's particleTypeInHand), `SpellSeed::GetPSysPower` 0x7298F0, end fn_0046E780, ReleaseInHandEffect CHand fn_0046E890. The trail: PSysUtilityPSys fn_00671DA0.

### Utility effects (`Particles/Utility.cpp`, PSysUtilityPSys 0xD4E0E8)

- **The trail** (PT 48 `SF_GestureChain`) is active when the game is waiting for a gesture: icon seed charging, seed
  with m_Held & 8, selection open with the hand free, or seed with a circle. It goes in the hand, with magnitude
  `hand scale × f(distance)` ({0, 50, 500, 1500} → {0.2; 1; 1; 1.5}). Its rules `ZR_ChainGesture` 0x68A080
  (emission fn_0068A330) and `CreateRuleMakeChain` 0x69FD10 are in `Particles/Rules/Gesture.cpp`, and the ribbon is drawn
  with the `ParticleChainCreator` (`Graphics/RendererChain.cpp`). Colour: fn_00671110 creates it with
  `PSysInterface::Create` and does `SetPlayer` (vt 0x20) on it with the local player (g_game +0x205A59,
  0x671172..0x671197); `ParticleChainCreator0` of `SF_GestureChain` has `UsePlayerColor 1` (white 255 × the player's
  colour, alpha 10), so the trail comes out in the player's colour (red for player 1). fn_00671260 does the same with
  PT 35 (0x6712CD..0x6712EA); the selection (fn_006711D0) gets no player.
- **The selection** (PT 28 `SF_SpellSelection`), while it is open.
- **The recognised gesture** (`fn_00689790` from `Success(1)`; PT 35 `SF_Gesture`; `UR_GesturingRecognised`
  0x6884F0 / 0x688910):
  - The record (0x48 bytes, list 0xD4EB10) carries the stroke (the terrain points of the whole buffer) and the ideal
    shape of the gesture (`PathSymbol<n>.cam`, or the circle's) placed over the pixel box of what was matched:
    - the box keeps the centre and divides its half sizes by those of the shape (fn_0068C140);
    - each point goes to the terrain under its pixel, at its altitude, or at 400 m along the ray (fn_00689F20);
    - if on the ground the shape comes out **more than twice as deep as it is wide** (camera axes in the horizontal),
      it is squashed vertically ×0.75 and this is repeated, 15 times at most;
    - the ideal one is resampled to as many points as the stroke has.
  - The rule takes one record per step: one atom (PCreator) and **IN_GAME 36 `G_SpellGestureRecognise`**. In its
    subcollection it places `NumAtoms` (234) sprites in the player's colour, with scale × (length of the ideal / 100):
    - the ideal approaches the camera until that scale rises (at most to half distance);
    - each sprite goes from the stroke to the ideal (t over `TimeToIdeal`, blended with smoothstep by `InterpGain`)
      and lights up from the ends (alpha `t × MaxAlpha`);
    - it jitters with shuffled-phase value noise, which fades out after `DispersalTime`. The noise is
      `Noise::VSNoise1To1` 0x590BB0: Ebert's lattice, permutation table 0xBEFDBC and Catmull-Rom spline 0x590010
      (`Particles/Noise.cpp`);
    - the collection pulses from `CollectionAlphaPulse` to 0 between 2.4 and 4.5 s, and the atom dies at `DieAge`
      (7 s).
  - Not ported:
    - the drawing of LH3D's `LightSheet` (50 points on the ideal, height scale × 9, alpha 1 − (2f − 1)²); the data
      are there;
    - the colour pulse of the hand (vt 0x2C of the hand object, unidentified).
  - The noise lattice: 256 × `1 − GameFloatRand(2)` (fn_00590DF0) from seed 0, the same values in every game
    (`Particles/Noise.cpp`, game_random). It is filled from `PSysGlobal::InitializeOneTimeOnly` 0x68F75C (fsubr [0x8AA390] at
    0x590E10); its 256 draws run before `GGame::Init` re-seeds (0x54F4AF), so they do not move Init's 0x88F89F stream.
    `Noise::VLattice` 0x590EB0 reads a value through the permutation table 0xBEFDBC.
- PSysUtilityPSys (0x40 bytes, made by `PSysGlobal::InitializeOneTimeOnly` 0x68F779): +0 the trail (48), +4 its "active", +8 the recognised sparkles (35), +0xC SF_OnFire (37), +0x10 the exploded meshes' SF_ExplodeObject, +0x14 SF_LightningStrike (61), +0x18 SF_ManaPathNew (22), +0x1C SF_BeliefSprite (24), +0x20 the selection (28), +0x24 its "active".
- fn_006721B0 (from `PSysGlobal::GameLoopEnd` 0x68F5C9, after the exploded meshes' slot +0x10), once a turn: the slots +0x0C, +0x18, +0x1C, +0x08 and +0x14, in that order, each made when missing (`PSysInterface::Create(NULL, type, 0, 0, 1.0, NET)`), stepped with an empty ProcessInfo (power 1, enabled) and the turn's ms ([0xD01A38], vt 0xFC `Process_`), and dropped when it returns 5 (made again next turn). The per-slot functions: fn_006717F0, fn_00671740, fn_00671B40, fn_00671CD0, fn_00672100, fn_006719E0.
- The slot gates [0xC029FC] (+0x0C), [0xC02A00] (+0x18, +0x1C) and [0xC02A04] (+0x08) are 1 in .data and never written; NET_GAME_TYPE is 1 for +0x0C (0x6716EE) and +0x14 (0x67198E), 0 for the others.
- The trail's magnitude table is 0xC0213C / 0xC0214C. Its condition is fn_00671DA0 (misnamed `GInterface::InterfaceActionHandObjectApplyToPos`, every frame): a seed in the hand with m_Held & 8 (fn_005CF200), the selection open with the hand ready (fn_005CF1C0), a circle-sized seed in the hand (fn_00729AC0 == 4). Not ported: `m_Buttons` bit 0x02 (no setter found) and byte [0xD17D10] (unknown).
- SF_Gesture (35) is stepped once a turn (fn_00672100) and drawn every frame with the turn's fraction by `Draw_(1)` (fn_00671DA0 0x671DD3); (pending) the original draws it only while MyInterface +0x3A0 != 0.
- The gesture records: list 0xD4EB10, count 0xD4EB18; the rule takes the newest one per step; the pixel box of the matched samples is 0xD4EB00; the gesture's shape is fn_0068C650.
- The camera's forward (0xEA1DD4) projected on the ground, (1, 0, 0) when it looks straight down; the 400 m point along the ray is fn_0074CAF0. The squash constants: 0.75 [0xC02650] at 0x689C20, 15 [0xC0264C] at 0x689C26, ratio 2 [0x8AB478] at 0x689C37.
- `UR_GesturingRecognised`: AtomData 0xC8 bytes (fn_00688140); the atom data is the channel owner of another interface's recognition sound (0x688643); [0xD4EB60] is never written, so the scale is always the ideal's length / 100. In the sparkle group (none in SF_Gesture) the atom runs along the ideal once every 2 s and fades out over its last 2 s (age / 2 [0xC02648] at 0x68872D, < 2 at 0x68877E, × 127.5 [0x92B6EC] at 0x68878B). The wiggle: x and z by the ideal's box, y upwards only, phase offsets +0.3 [0x8AB23C] (0x6892D3) and +0.7 [0x8AB238] (0x689308). The pulse start and length are fn_0068DE90 / fn_0068DEA0; the LightSheet is fn_0083E710 with +0x18 = 0.03.
- `ZR_ChainGesture` (AtomData 0x48, fn_0068A290): while the effect is enabled one head atom at the gesture position trails a chain whose NextGroups joints are a shift register, a new joint every MinEmitDist of movement (at most count / DieAge joints per second); when the effect stops being enabled the head stops emitting and goes 5 s later. `CreateRuleMakeChain` makes NumAtoms joints of PCreator, once.
- Not ported: feeding a fireball in flight with a fire seed in the hand (the start of `ProcessPowerUpSystem`); it needs
  the cursor to be able to point at the MagicFireBall.

### Hooks, tests and captures

- Hooks, in [openblack-internals.md](openblack-internals.md#debug-environment-variables):
  `OPENBLACK_TEST_CAST`, `OPENBLACK_TEST_CAST_PATH`, `OPENBLACK_TEST_THROW_VEL`, `OPENBLACK_TEST_SHOT_PATH`,
  `OPENBLACK_TEST_GESTURE` and `OPENBLACK_GESTURE_TRACE`.
- **For other systems**:
  - `OPENBLACK_TEST_SEED` + `OPENBLACK_TEST_CAST` casts through the real hand path;
  - `_CAST_PATH` drags the hand during the first press (food, wood, water);
  - `_THROW_VEL` is the hand velocity the spell receives (the fireball).
- `test_gestures`:
  - octants and rounding of .5; corners of a square; the clearing after the still samples;
  - synthetic templates (it recognises its own and not the others; the mirror, only with `allowReverse`);
  - the selection with a fake icon: SPIRAL opens and FORK_RIGHT requests seed 4;
  - with `OPENBLACK_GAME_PATH`, `Gestures.jty` (81 × 1628), the player's 14 gestures, and CIRCLE and STAR mirrored with
    `reversed`.

## Worship: where miracles come from (`src/Worship`, `ECS/Systems/Implementations/VillagerWorship`)

The original's chain is: a **town** stores magic types → its
**village centre** shows one icon per seed → the player's **citadel** has one **worship site** per tribe, with one icon
per seed → the **villagers** dance there and fill its **battery** of prayer power → when an icon is tapped it
**charges** and the seed appears in the hand. Separately there are the one-off miracle **dispensers** and the
**fireflies**.

### Structure and data

- `GWorshipSiteInfo[9]`, one per tribe (`GTribeInfo.worshipSiteInfo`): `chantsPerVillager` 3 (Celtic 4, Tibetan 5),
  `maxDancersVisible` 20, `chantsToFillBattery` 9000, `eachVillagerAddToFillBattery` 300, `prayerSiteDistance` 44,
  `radiusFromCitadel` 37.5, `artifactPowerupMultiplier` 1e-5, and the altar mesh per tribe (101
  `BuildingCitadelNorseAltar`, 93 Indian, 94 Aztec, 95 Celtic, 98 African/Egyptian, 99 Greek, 100 Japanese, 103
  Tibetan).
  - **Bug kept:** `chantsToReserveForMaintaining` is in the file as the integer **500** and the executable reads it
    with `fld` (`fn_0077A950`), so it is ~7e-43 ≈ 0: the reserve for maintaining spells does not exist in practice.
- `GSpellIconInfo[2]`: [0] "Spell Icon" (worship site), [1] "TownSpell Icon" (village centre). Both use mesh
  **203** `BuildingVillageCentreSpellHand` and `gatheringChantAddPerGameTurn` 61.
- **Special points** (the L3D's extra metrics; `Game3DObject::GetSpecialPos` 0x63B040 / 0x63B0B0 = the metric's
  matrix times the object's; `src/Worship/SpecialPoints.cpp`):
  - the worship site's mesh `b_worship.l3d` has **16**: 7 hiding place, 8 dance centre and totem, 9 arrival,
    **10..15 the six icon slots**;
  - the village centre's mesh (e.g. 179 `BuildingNorseVillageCentre`) has **14**: **0..5 the six icon slots** (all at
    y = 2.781, in a ring) and 6 the totem (y = 4.613, in the centre);
  - the icon's mesh 203 has 1: the point where the `SpellSeedGraphic` floats (+1 in y).
- Charge cost = `GMagicEffectInfo.costToCreate` (FIRE 3500 / PU1 7000 / PU2 10000, LIGHTNING 5000/7500/10000,
  HEAL 6000/9000, FOOD 7000/10000, WOOD 7000, WATER 5000/7000, NATURE 13000, LIGHTNING BEAM 16000/32000/60000...).
- `GSpellIconInfo` [0] "Spell Icon" is at 0xD9D3E8; [1] "TownSpell Icon" is at 0xD9D514.
- The altar mesh is `GWorshipSiteInfo` +0x124 (file +0x114 `meshType`), read by `WorshipTotem::GetMesh` 0x780A70.
- `B_WORSHIP.l3d` (Data\Citadel\OutsideMeshes) is loaded by `fn_008829C0` into the heart's 3D object +0xAC. The site's `CallVirtualFunctionsForCreation` 0x77B9D0 gives it to the site (vt 0xF4).
- **The site's texture** (**faithful**, `WorshipSite.cpp` `CitadelSiteMesh`). `B_WORSHIP.l3d` carries no skin
  (skin count 0); the three `Textured` materials of its drawn sub-mesh name skin **1**, and the `Smooth` one of its
  physics sub-mesh none (0xFFFFFFFF). `fn_008829C0` is called once, from `InitTemple` (0x8829A6, just before the first
  `ProcessAlignement` at 0x8829AF). It loads the file with `CreateFromHD(path, 1)` (0x8829DB..0x8829E2, "don't care
  about textures") when +0xAC is still null, takes the heart's mesh (`ResolveLoad` vt+0xF8, the morphed one) and its
  first skin (`LH3DMesh` +0x3C `skins`, element 0: 0x8829C6..0x8829D7), and writes that texture into
  `LH3DMaterial::texture` (+8) of **every primitive of every sub-mesh** (0x8829F0..0x882A2C), the physics one included.
  So the sites wear the temple's own texture, `B_FIRST_TEMPLE`'s skin 0xFC0B7D34, which the alignment blend rewrites in
  place (see "The temple's outside"): a site follows its temple from evil to good, and each player's set. One mesh per
  heart 3D object, shared by its sites (`CallVirtualFunctionsForCreation` 0x77B9D0 reads the citadel's heart
  (+0x30), its 3D object (+0x40) and its +0xAC at 0x77BA08..0x77BA14).
  - openblack: one copy of `B_WORSHIP` per heart mesh in the mesh cache (`temple/B_WORSHIP_l3d/<heart mesh id>`), made
    with the citadel's first site; its skin source is the heart's mesh and `L3DMesh::SetSkin` gives every primitive
    the heart mesh's first skin (`GetFirstSkin`, in the file's order). The partly built site (and temple) keeps the
    skins too: `PartialBuild::BuildMesh` sets the intact mesh as the generated one's skin source.
  - Before (bug): skin 1 was looked up in the texture manager, where it is the first texture of `AllMeshes.g3d`, so the
    site was drawn with an unrelated pack texture (black without one).
- Special points: `fn_0077CE70` = point 7 (where the villagers beyond `maxDancersVisible` wait); `GetArrivePos` 0x77CED0 = point 9; `WorshipSite::GetSpecialPos` 0x77CC90 (world point).
- The town belief constants: `GBeliefInfo` 0xC58640 +0x14..+0x24, with GPlayerInfo +0x48 and GTownInfo +0xB8.

### The citadel and its six slots (`Worship/Citadel.cpp`)

`CitadelWorship` goes in the temple's entity (`components::Temple`), which openblack creates in `CitadelArchetype`. Six
slots (`sites[6]`); the angle of slot *n* is **the heart's angle + n × 2π/7** (`Citadel::GetWorshipSiteAngle`
0x463610) and the site is placed at `radiusFromCitadel` from the citadel's origin.

- `Citadel::AddTown` 0x463130 → `FindOrCreateWorshipSite` 0x4631D0 / 0x463220 → `FindTribeWorshipSite` 0x463190 or
  `RequestANewWorshipSite` 0x4633F0 (the free slot closest to the nearest town of that tribe, otherwise to the
  citadel).
- `CitadelHeart::CreateBuiltWorshipSite` 0x465110 is the script's `CREATE_WORSHIP_SITE`: it creates the site of that
  tribe **without checking the town** and adds the player's towns of that tribe to it. The position and site number
  that the script brings **are not used**.
- `GPlayer::PostLoadCleanup` 0x64AB90 (right after the land's script; in openblack, on the first turn): for each player
  with a citadel, each of its towns without a worship site → `Citadel::AddTown`.
- `Town::IsAllowedToCreateWorshipSite` 0x740BB0: **never on land 1**, nor if the script forbids it
  (`SET_CAN_BUILD_WORSHIPSITE`), nor without population. That is why in Land1 there are only dispensers and fireflies.
- **What the audio reads** (`GGuidance::CheckWorshipSiteDesiresSFX` 0x71B270). It walks `GPlayer+0xA48` →
  `Citadel+0x34..+0x48` in slot order (`citadel::WorshipSitesOf`). It skips the sites without dancers: fn_0077B960
  jumps to 0x77CFB0, which gives `Dance+0x90` or 0 without a dance (`site::DancerCount`). Of the others it keeps the
  one closest to the camera, closer than 200 m (0x980130). Then it asks for its `CalculateDesireForFood` (vt+0x420 of
  `??_7WorshipSite` 0x8F2840 = 0x77C310; `site::CalculateDesireForFood`), which is
  `1 − min((comida + 0.0001) / (necesaria + 0.0001), 1)`.
  - The food is that of the site's pot (+0xB4, `Pot::JustGetResource` 0x66D390).
  - The needed amount comes from `Dance::CalculateFoodNeededByDancers` 0x50BF20: the sum, per dancer, of
    `(1 − comida en la barriga +0xE8) × foodReqiredForDinner` (+0x2D8).
  - It also reads `Citadel+0x70`, the fraction of the worship strain sound, limited to 1 at 0x71B31C
    (`citadel::StrainSoundFractionAtMostOne`). Only `SetWorshipStrainSoundFrac` 0x463850 writes it (from
    `ProcessSpellIcons` 0x46396C) and it is saved and loaded with the game (0x463D6A / 0x463FB9).
  - **(approximate)** openblack sums the dancers in the order in which they joined, not group by group; only the
    rounding changes.
  - **(inferred)** The initial value of +0x70 is 0: Citadel's constructor has not been read.
- The slot angle 2π/7 is the float 0x8C836C.
- `Citadel::FindNearestWorshipSitePosAngleAndSlotToPos` 0x463540: of the free slots, the one whose ring point is nearest (x, z) to the target point; best starts at 1e6; −1 when all six are taken. `RequestANewWorshipSite` 0x4633F0 gets that point from `Town::GetNearestTownToPos` 0x73B170 (the citadel's MapCoords, the tribe, 0x7FFF = any abode type, FLT_MAX) at 0x46345C: that town's MapCoords, otherwise the citadel's.
- `WorshipSite::Create` 0x77AC50 is called with (angle 0, scale 1.0, percent 0, underConstruction 0). The MultiMapFixed ctor sets +0x58 bit 3 and +0x5C = 0 (0x52E22D..0x52E234), so `IsBuilt` 0x77BDD0 (+0x5C ≥ 1) is false until the site is built. `WorshipSite::Draw` 0x5193D0 → `DrawBuilding` 0x517F90 then draws nothing at 0 %: **a site that the citadel requests is invisible until it is built.**
- `site::Create`: `fn_0077A960` = CitadelPart(pos = the citadel's origin, info, citadel, slot, tribe, angle, scale 1); `fn_00463770` stores it in sites[slot]. `WorshipSite::AssignTownsToWorshipSite` 0x77AF70 adds every town of the player's list whose `Town::GetTribe` 0x73C840 (Town +0x5B8) is the site's, oldest first.
- Only when it has a totem (0x77AD5D): `fn_0077AEE0` makes the Dance of `GDanceInfo[19 + slot]` (0xCC4B80 + (19 + slot) × 0xB0) at point 8, `fn_0077B8D0(0.5)` and `CreateFoodPot`. `AddResource` 0x77C638 makes the pot again when it is gone.
- `CitadelHeart::CreateBuiltWorshipSite` 0x465110: no site → returns 0. For the first town of the player of that tribe (Town +0x5B8), `AddTown` 0x77C800 if it is missing; if that town has a building site of the worship site, `BuildBy(1.0)` (vt +0x900, WorshipSite 0x77DC50) and `Town::RemoveBuildingSite` 0x73BA20. With no town of that tribe it still does `BuildBy(1.0)` and returns 0 (0x465163..0x465174).
- `Citadel::Process` 0x462D70 (from `GPlayer::Process` 0x649525, before the player's towns). The heart's 3D object takes `GetPercentBuilt` (vt +0x880) through `LH3DCitadel::SetPercent` 0x883120. When it reaches 1, the heart leaves its map cells and enters them again (RemoveMapObject vt +0x548 / InsertMapObject vt +0x544). Before and after that it gives the 3D object its two targets (0x462D8F..0x462DE4 and 0x462E55..0x462E65): the alignment target, from `GetAlignmentValue` 0x64D6A0, through `SetNasty` vt +0x218, and the size target, from the player's share of the influence, through `SetStage` vt +0x20C (an earlier note here called them the alignment colour). Then it steps both and blends the outside again when it has moved far enough (0x462E6B..0x46304A): [The temple's outside](#the-temples-outside-alignment-and-size). Not ported: the effects `fn_004630E0` / `fn_00454AA0` of that blend and `CitadelHeart::SetAlignmentFlock` 0x465270 (0x463050..0x463068).
- `CitadelHeart::Built` (0x46501B..0x4650F1): GetPlayer (0x468020: +0x80 ? the citadel's player : the town's), SetLife(1.0), `fn_00464F50(citadel, 0)`. `fn_00464F50` opens the worship sites: for every town not yet in a site, `AddTown`, then `Town::GetBuildingSiteInList` 0x73CE40 or else `Town::AddBuildingSite` 0x73B8E0, with the desire boost at +0x63C (0x464FCE). Then, only for the local player and outside a script's wide screen: `GAudio::StartScriptMusic(0x3D)` 0x428230 if no script music is playing and the land is not 1 (g_game +0x205A08 != 1); and `SaveGameRoom::InstantSaveGame(0x14)` 0x792FB0 if the game is neither a playground (g_game +0x205A0C) nor multiplayer. Save games are not ported.
- `WorshipSite::Built` 0x77AC10 (after `MultiMapFixed::Built`): the site's building site is removed from every town of its player.
- `Citadel::ToBeDeleted` sets the player's +0xA48 to 0 when the citadel is marked (0x462C5E..0x462C66), so a marked citadel belongs to nobody.
- `SET_INTERFACE_CITADEL` (414, 0x70B9A0) writes GScript +0xA0 = the popped value (0x70B9BD); `GScript::Reset` 0x6EB312 sets it to 1. `CitadelEntrance::InterfaceValidToTap` 0x468F50 = IsMultiplayerGame ? 1 : GScript +0xA0 != 0. openblack: `worship::citadel::ResetInterfaceCitadel` in `Game::LoadMap`'s script reset (since 57e7572d), so Land 1's `SET_INTERFACE_CITADEL(0)` does not carry over to the next land.
- Town side: `Town::CheckAddWorshipSite` 0x740BF0 requires that the town is allowed, that the player is not type 3, and that the player has a citadel → `FindOrCreateWorshipSite` and `AddTown`. `IsAllowedToCreateWorshipSite` 0x740BB0 reads the script flag at Town +0x5F0 and the population as Town +0x618 + +0x61C (the TownStats adults and children, `TownStats::Add` 0x7492E0).
- `GPlayer::PostLoadCleanup` 0x64AB90 is called from `GSetup::LoadMapFeatures` 0x7180B0.

### The temple's outside: alignment and size

**Faithful**, except where marked (`ECS/Systems/Implementations/TempleExteriorSystem.cpp`, `3D/TempleExteriorMorph.cpp`,
`components::TempleExterior`). The temple's outside is a blend of the fifteen meshes `B_TEMPLE<size><stage>` of
Data\Citadel\OutsideMeshes, three sizes by five stages from evil to good, with its texture blended from `Evil<set>.16B`
to `Neutral<set>.16B` to `Good<set>.16B`. The original keeps the look in the heart's 3D object (`LH3DCitadel`):

| Field | Last blended | Current | Target |
|---|---|---|---|
| Size, 0..1 | +0x84 | +0x88 | +0x8C |
| Alignment, 0..1 | +0x90 | +0x94 | +0x98 |

Also +0x9C the draw percent, +0xA4 a dirty flag and +0xA8 the player (& 7).

- **The start** (`InitTemple` 0x882730, from `CallVirtualFunctionsForCreation` 0x4675A0):
  - player & 7 into +0xA8 (0x88273F..0x882744);
  - alignment 0.5 for last, current and target (0x882789..0x88279C); size 0 for the three and the percent 0
    (0x8827A9..0x8827BB);
  - `SetDrawPercent` vt+0x200 with the percent built (0x8827C1), dirty = 1 (0x8827D0);
  - the land flattened (0x8827C7..0x882962; [openblack-internals.md](openblack-internals.md#render));
  - `ComputeBoundingBox` 0x8081B0 (0x882999), then `ProcessAlignement` vt+0x208 (0x8829AF): **the first blend is at
    the heart's creation**, due to the dirty flag, before the first turn. That is CREATE_CITADEL in the land script,
    and the plan's conversion for a planned citadel (Land 1: the CHL `BUILD_BUILDING`; the later lands: the AI).
- **The heart enters the map cells after the first blend** (InsertMapObject vt +0x544 at 0x46776C, after
  `InitTemple` 0x46767F and the entrance). The heart's vtable 0x8C8D00 gives `MultiMapFixed::InsertMapObject`
  0x52E650, which asks `CreatureMustAvoid(0)` (vt +0x7BC, 0x52F490, 1 with no creature) and then sends the footpaths
  round the heart, `SendFootpathsAroundObsticle` at 0x52E744, with `Object::Get2DRadius` 0x638180 of the 3D object's
  mesh (vt +0xF8): the blended one, whose box the blend has just made again (0x882E1F..0x882E27). On Land 1 that is
  B_TEMPLE02's 36.94 m, not B_FIRST_TEMPLE's 25.58 m, so the footpaths bend round a wider circle from the heart's
  creation on. Each later blend, between RemoveMapObject and InsertMapObject in `Citadel::Process`, sends them round
  again with the new box. `CitadelHeart::ShouldFootpathsGoRound` 0x464B70 is not on this path. In openblack the
  heart's map cells also come from that box (approximate: the original's own collide shape for the heart is not
  ported).
- **Every turn**, in `Citadel::Process` 0x462D70, first thing in `GPlayer::Process`. A citadel without a heart, or a
  heart without its 3D object, returns at 0x462D79..0x462D89: **a plan has nothing to morph**. The percent built
  (+0x9C) is never read by what follows, so **the look moves from 0 % built**, built or not.
  - Alignment target = (`GPlayer::GetAlignmentValue` 0x64D6A0 + 1) × 0.5 ([0x8AA390] 1.0, [0x8AA3B4] 0.5), given to
    `SetNasty` vt+0x218 = 0x80BB50: below 0 → 0; from 1 up → 0.9999 (0x3F7FF972); else as it is.
  - Size target = 2 ([0x8C7E2C]) × `fn_0064ACC0`, given to `SetStage` vt+0x20C = 0x80BAA0: below 0 → 0; above 1 → 1.
  - `fn_0064ACC0`, the player's share of the influence: 0.01 ([0x8C5840]) when the land number g_game+0x205A08 is 1;
    else the sum `fn_0064AEA0`, and 0.01 when it is 0 ([0x8AA398] 0.0); else the player's influence power +0x8C over
    the sum (0x64ACC0..0x64ACF6).
  - `fn_0064AEA0` adds +0x8C over the player slots from the first, power + sum in slot order. Its walk 0x550980 goes
    from g_game+0x18 by 0xA60 up to g_game+0x5318: **all eight slots, with no active test, so the neutral player
    counts**. `fn_0064B700` (the heartbeat's ratio, [audio.md](audio.md)) walks with 0x550930 instead, which skips the
    inactive slots.
  - Both targets are set twice, at 0x462DCF / 0x462DDE and at 0x462E5A / 0x462E65, around the percent part
    (`GetPercentBuilt` vt+0x880 at 0x462DF3, `SetDrawPercent` vt+0x200, and the map-cell remove and insert when it
    reaches 1).
  - The steps (alignment 0x462E6B..0x462F11, size 0x462F17..0x462FBD): |current − target| ≤ 0.001 ([0x8C7CB0], a
    double) → current = target; else current ± 0.016 ([0x8C8368]) toward the target, and the target when that would
    go past it.
  - `IsChangedSize` vt+0x1FC = 0x882AD0: |last − current| > 0.03 ([0x99A168], a double), for the alignment or the
    size. Only then (0x462FC7..0x46304A): RemoveMapObject vt+0x548, `ProcessAlignement` vt+0x208, then, when it
    returned non-zero and the 3D object's +0xE0 is set, an effect (`fn_004630E0` twice and `fn_00454AA0`, with 15.0
    and twice the mesh's +0x28: not decoded), and InsertMapObject vt+0x544. While a value moves a step a turn, that is
    a blend about every second turn.
  - Then, every turn, `CitadelHeart::SetAlignmentFlock` 0x465270 with the alignment and whether it changed
    (0x463050..0x463063).
- **The blend** (`ProcessAlignement` 0x882B10..0x88309A) returns 0 unless `IsChangedSize` or the dirty flag, and
  clears the flag.
  - The size index is vt+0x214(size) (ftol(2 × min(size, 0.9999))), clamped to 0..2, and the next one; the lower one's
    weight is clamp(next − 2 × size, 0, 1) (0x882B4C..0x882BB6).
  - The stage index is ftol(4 × alignment) ([0x8AB418] 4.0), clamped to 0..4, and the next one; the lower one's weight
    is next − 4 × alignment, not clamped (0x882BBE..0x882C0A).
  - `fn_008830E0(size, stage)` gives the mesh `B_TEMPLE<size><stage>`. `fn_00807970` blends two meshes into the
    heart's own mesh (vt+0xF8) by `fn_00855260` on every vertex: all eight floats (position, texture coordinates,
    normal) as w × a + (1 − w) × b, the normal not made unit again. Once along the size at the lower stage; when the
    stages differ, again at the upper stage, and then between the two (0x882C10..0x882CCF).
  - The bake (0x882CD4..0x882E15), every vertex of every primitive of every submesh, with the 3D object's matrix
    (rows at +0x14, +0x20 and +0x2C, position at +0x38): world x = ((z·[+0x2C] + y·[+0x20]) + x·[+0x14]) + [+0x38]
    and world z = ((x·[+0x1C] + z·[+0x34]) + y·[+0x28]) + [+0x40], the melting's order; H = `GetAltitude` 0x803090
    at ftol(x × 65536 × 0.1) and ftol(z × 65536 × 0.1) ([0x8AC408], [0x8AC404]); and **v.y = v.y − ([+0x3C] − H)**
    (0x882DC3..0x882DD7), with no 1 / scale. It runs at the creation (from 0x8829AF) and at every blend (from
    0x462FE2).
  - `ComputeBoundingBox` 0x8081B0 (0x882E27); then last size = size and last alignment = alignment
    (0x882E2C..0x882E42).
  - The texture (0x882E48..0x883081), into the mesh's first material's texture (locked by 0x838AF0, unlocked by
    0x838EB0). Alignment above 0.5: Neutral → Good with weight ftol((a − 0.5) × 510) ([0x9377F4] 510.0); else Evil →
    Neutral with ftol(a × 510); clamped to 0..255. The images come from the name tables 0xC39B28 (evil), 0xC39BA8
    (neutral) and 0xC39C28 (good), 0x180 bytes a set, set = +0xA8 & 3, read by `fn_00822190`. Each 4444 texel: for each
    of the three colour nibbles ((A & m) × (255 − w) + (B & m) × w) / 255 (the multiply by 0x80808081), & m; the alpha
    nibble is the first image's. It returns 1.
- **The draw.** The finished temple uses the static draw 0x80DB30: no land deltas when it is drawn, as its mesh is
  baked. While it is being built, `Draw` 0x882A40 uses `DrawPartialyBuilt` 0x816AD0 on the same morphed mesh.
- **openblack.** `CitadelArchetype::CreateHeart` calls `TempleExteriorSystem::Create` after the flattening (the start
  and the first blend), and `citadel::Process` takes the targets, steps them (`Step`) and, when `IsChangedSize` says
  so, takes the heart out of the map cells around `Blend`. The meshes and the images are read once through the byte
  cache; the first temple's file gives the own mesh's shape, skins and footprint. The own mesh is
  `temple/exterior/<the entity's index>` in the mesh cache, with skins that can take new texels. The bake is
  `land_morph::BakeAgainstY`. After a blend the partly built draw is cut again from the new mesh. `MorphWithTerrain`
  stays off.
  - (approximate) The four corners are summed at once; the original nests two-way blends. Only the float rounding
    differs.
  - (approximate) The influence powers are the last turn's: openblack works them all out after the citadels; the
    original works out each player's after its own citadel.
  - Not ported: the effect of the blend (`fn_004630E0` / `fn_00454AA0`) and `SetAlignmentFlock` 0x465270.

### The battery and the site's turn (`Worship/WorshipSite.cpp`)

`WorshipSite::ProcessSpellIcons` 0x77B4D0, once per turn from `Citadel::ProcessSpellIcons` 0x463920 (which comes from
`GPlayer::ProcessSpellIcons` 0x64AEE0, inside `Spell::ProcessSpells`):

1. **Strain** (+0x114) = `(pedido − capacity) / capacity`, with capacity = `N × chantsPerVillager × poder tribal[2]`
   (`fn_0077E060`). Without capacity, 1 if something was requested and 0 if not.
2. If the strain is **not** positive, the icons that are being charged share out what is left over:
   `min(available, necesitado) / cuántos` to each one (`fn_0077CBC0` subtracts the maintenance reserve, ~0 because of the
   bug above). What each icon accepts is charged to the site.
3. `WorshipSpellIcon::Process` of each icon.
4. **End of turn** `fn_0077B6A0`: `k = min(1, used/capacity + boost)` with
   `boost = max(0.2, 0.5 − battery/max × 0.5)` (0 if it comes out ≤ 0); produced = `capacity × k`;
   `chantDamage` = produced / N (what it costs each dancer in life); `battery -= used − produced` (never below 0);
   `available = battery + capacity`. That `k` is also the intensity of the dance (`fn_0077B8D0` →
   `fn_0050C340`).
   - Every 1000 turns the site's artifacts would give an extra; openblack has no artifacts.
- `UseChants` 0x77BBB0 records what was requested, charges at most what is available and adds to the player's
  statistic. `MaintainSpell` 0x77BC50 and `fn_0077CC50` are the variants for the cheats (infinite chants, free
  maintenance).
- `MaxBattery` = `chantsToFillBattery + N × eachVillagerAddToFillBattery` (9000 without dancers).
- **Visual strain** `fn_0077B3B0` (per frame): `fase = fmod(fase + (5 + 5·clamp(tensión,0.1))·dt, 2π)`,
  `pulso = (cos fase + 1)/2`.
- The real **dance** comes from its `.DAN` (`GDanceInfo[19 + hueco]`, `GroupBehaviour::CalculateDancePosition` 0x597F20).
  It is not ported: the dancers are spread over a 6 m ring around point 8, at 256/N each (the ring part of that
  function). **UNVERIFIED**: the exact shape of the dance.
- Battery helpers: `MaxBattery` = `fn_0077E780`; available `fn_0077CC10` = 1e6 with the infinite cheat, otherwise available − used; `WorshipSite::GetTotalChantsAvailable` 0x77CC30 = 1e6 with the cheat, otherwise +0xF8.
- Artifacts (not ported): every 1000 turns the site's artifacts (+0xAC) get [0xD1A298] × artifactPowerupMultiplier × N / the player's `fn_0064D0E0` (`fn_00426A80`).
- `GAME_SET_MANA` 0x6FE800: `fn_0077B060` empties every icon's store, then the battery (+0xF0) is set.
- The strain visual `fn_0077B3B0` is called per frame through `fn_00463980`.
- `Dance::CalculateFoodNeededByDancers` adds nothing for members that are not villagers (dynamic_cast 0x50BF51). `foodReqiredForDinner` is read as an integer (`fimul`). `WorshipSite::GetResource` 0x77BD80 (vt +0x98) gives 0 when the site has no pot.
- `WorshipSite::CreateFoodPot` 0x77AD90: pot info 2 (StoragePitFoodPile, 0xD4C8E8) at `GetFoodPosAndYAngle` 0x77CDB0 = the site's local point (9, 0, −38), at the site's angle + 1.5 (0x99C45C), scale 0.7 (0x99C460).
- `WorshipTotem::Create` 0x780930 (ctor 0x780840): the tribe's altar at special point 8, at the site's angle and scale.
- `WorshipSite::ToBeDeleted` 0x77AA60, in order (before the CitadelPart 0x469540 and MultiMapFixed 0x52E2B0 parts):
  1. Each icon in +0xE0 is deleted (`WorshipSpellIcon::ToBeDeleted` 0x77F230 → `RemoveSpellIcon` 0x77C450).
  2. The dance (+0xA0) is deleted: `Dance::ToBeDeleted` 0x50B970 runs `RemoveFromDance(1)` (vt +0xB08 0x751510) and `SetStateAfterFinishingDance` (vt +0xB0C 0x759B80) on every member that is still available.
  3. The object at +0x90 (not identified) is deleted.
  4. If the totem (+0xDC) is available, its +0x100 is set to 0 and it is deleted. +0xDC itself is not cleared.
  5. Every town of +0xA4 gets +0x98C (its worship site) = 0.
  6. `fn_004637C0` clears the citadel's sites[+0x110].
  7. The lists +0xA4 and +0xAC are emptied.
  8. The food pot (+0xB4, with no availability test) gets `SetMultiMapFixed(0)` (vt +0x868) and is deleted (`Pot::ToBeDeleted` 0x66D110, which runs `Pot::RemoveReaction` 0x66D6A0 first); then +0xB4 = 0.
  9. The object at +0xB8 is deleted (not identified).
- `MapCoords::FindWorshipSite` 0x602460 (off the map: null) does a single `FindTypeOnMap(8 CITADEL, 0)` on the cell's fixed list. A WorshipSite answers itself, a WorshipSpellIcon answers its site (vt +0x30C), and anything else (the heart, a town centre icon) gives null. The site's own collide shape is `CreateCollideData` 0x77E490.
- `WorshipSite::DeleteObjectAndTakeResource` 0x77E7B0 (vt +0x684, returns 1, makes no reaction, unlike the storage pit). If the local hand threw the object (Object +0x24 & 0x40, `SearchForPhysicsObject` 0x646950, its +0x24 == MyInterfaceStatus): `HelpProfile::Trigger(6)` 0x5C46E0. Then `DoDeleteObjectAndTakeResource` 0x63A940 → `WorshipSite::AddResource` 0x77C5F0 (type, n, IS, poisoned, pos, k): with a building site (+0x74) and WOOD (1) or ANY (−2) every argument goes to the site's `AddResource` (vt +0x9C, 0x77C5FE..0x77C625); else FOOD (0) only: no food pot (+0xB4) → `CreateFoodPot` 0x77AD90 (0x77C638), then the pot's `JustAddResource(FOOD, n, poisoned)` (vt +0x8C, 0x77C63D..0x77C651) and its result returned; any other type 0 (0x77C65B). The status, the position and the last argument are not used on the food path. openblack: `worship::site::AddResource` (the pure `AddResourceRouteOf`), reached from `object_resources::AddResource`, so an object a worship site takes (a thrown animal or pot of food, an object given) fills its food pot; the food pot's add is `pot_resource::AddToPotDirect` (the pile sound, the cap, the poison, the size).
- Lists and links: `Town::SetWorshipSite` 0x73D030. `WorshipSite::AddTown` 0x77C800 also makes the town's footpath link to the site (`fn_007412A0`, `GFootpathLink::GetNearestPathToQuick`); this is not ported, because the villagers use openblack's pathfinding. `fn_0077C950` removes a town. `GetSpellIconFromSeedType` 0x77B170 / `GetSpellIconFromMagicType` 0x77B1B0. A villager joins the dance in `StartWorshippingAtWorshipSite` 0x76C4C0 (GroupBehaviour, Dance +0x90). `Dance::SetState` 0x50BAF0. `GetNumVillagersRequestingToGoHome` 0x77E260.

### The icons and the charge (`Worship/WorshipSpellIcon.cpp`, `Worship/TownCentreSpellIcon.cpp`)

- `WorshipSpellIcon::Create` 0x77F2B0 places mesh 203 in slot 10..15 with the site's scale and angle, and its
  `SpellSeedGraphic` above it (`SpellIcon::Create3DSpellObject` 0x726210). `UpdateGraphicsWithPULevels` 0x77F320 shows
  the highest upgrade level the player has enabled and sets +0x58 = 0.5. **+0x58 is not an alpha**: it is only read by
  `DrawSpellGraphic` 0x51A712 as the band size (0.2 × +0x58 × scale). The icon's seed is painted opaque (the icon
  passes alpha 0xFF). Before, openblack painted it half transparent: corrected.

### SpellSeedGraphic: the seed that floats in the orb and in the icons (`Worship/SpellSeedGraphic.cpp`, faithful except where marked)

Object from `SpellIcon.cpp` (it is not an `Object`; list 0xD9D3D0). Fields: +0x14 MapCoords of the mesh, +0x2C the mesh
(Game3DObject), +0x30 the band, +0x34/+0x38 phases of the vials, +0x3C y angle, +0x40/+0x44 band angles, +0x48 seed,
+0x50 holder PSys, +0x54 scale, +0x58 band size, +0x5C auto-update, +0x60 PU, +0x64 the given point. Seed row =
0xD9D678 + type × 0x190 (memory offsets = file + 0x10).

- `Create` 0x726F60 → fn_00727190: the mesh `GSpellSeedInfo.mesh` (+0x130 of the file) and `ReplaceMeshGivenSeedType`
  0x728450 (table 0x72854C per seed − 3): FLYING_FLOCK sets mesh 1 (AnimalBat1) if the player's alignment
  (GPlayer+0x60 → +8) < `alignmentSwitch` (fn_00723140), otherwise 11 (AnimalSpellDove), and fn_00727440 redoes it
  every 30 turns (`g_game +0x205A40 % 0x1E` in fn_00727350; openblack uses `Game::GetTurn`, **(inferred)** that this
  field is the turn counter); FOOD and the creature vials carry envmap 0 (`envmap.raw`) and BEAM_EXPLOSION properties
  {1,0,1,1,0}: **not ported** (openblack has no per-object envmap). The holder PSys (+0x164 of the file) is created at
  the point + `unknown0x154` × scale with magnitude = scale; the band (`CreatePUBand` 0x727080) if pu ≠ −1.
- fn_007270E0: +0x64 = point, mesh at point + `unknown0x150` × scale (−1.5 almost always: the I_* meshes have their
  origin at the bottom and are ~3 m tall, so this centres them), effect at point + `unknown0x154` × scale.
- The orb (`OneOffSpellSeed::Draw` 0x518E90), every frame it is visible: `GetSpellGraphicPos` 0x72A840 = the drawn
  matrix applied to the mesh point `ResolveLoad()+0x18` (the box centre, **(inferred)** because it is the point
  fn_00518720 rotates about) and scale = scale of the 3D object × 0.6 ([0x8C7BDC]); `DrawUpdateAtPos` 0x727630 (+0x54 =
  scale, fn_007270E0, fn_007274D0: PSys to its point, magnitude = scale, `Process_` with the info zeroed, power 1,
  active) and `DrawSpellGraphic(bola, 0, 1, 0x95)`.
- The icons (`SpellIcon::Draw` 0x5198D2, `TownCentre::Draw` 0x5164D4 → `DrawSpellSeedGraphic` 0x726D30):
  `UpdateOnly(ms)` and `DrawSpellGraphic(icono, 0, 1, 0xFF)` (both tint the icon with 0xFFFFFFFF). The seed stays where
  `Create3DSpellObject` created it (special point 0 + 1, scale 1).
- `DrawSpellGraphic` 0x519AD0 (read in full in the part for the player's seeds):
  - only if `useMesh` (+0x168 of the file, fn_00727690) is 1. **STORM, FIRE, LIGHTNING_BOLT, WATER and TELEPORT have
    0**: in the orb and in the icon only their holder effect is seen (LIGHTNING_STORM / FIREBALL / LIGHTNING_BOLT / WATER /
    TELEPORT_ON_HOLDER). openblack painted their meshes (I_Lightning2, I_Blast, I_Lightning, the horn for water and
    the shield for teleport): those were the "wrong icons".
  - size = `GSpellSeedInfo.scale` (+0x134) × +0x54; angle +0x3C += 2 rad/s × dt ([0x8D8700]), fmod 2π (double
    [0x8D45D8]); `SetPosition` 0x423140: rows X = (cos, 0, sin), Z = (−sin, 0, cos). **No bounce or pulse** for the
    player's seeds: `AsMagicCreatureSpellInfo` (vt 0x38) of its base magic is NULL and it jumps to 0x51A0B3. The bounce
    (+0x38 at 0.35/0.5 per s, `0.5(1 + sin 2π f)`), the 8×4 UV frames at −15 per s (+0x34) and the squashes
    0.7/0.8/1.5 of the switch 0x519D76 (by GMagicCreatureSpellInfo+0x58) belong to the vials 12..27. Only the UV
    frames are ported (0x519B79..0x519C1B, `frame_anim::SpellIconFrame`, see
    [rendering-objects.md](rendering-objects.md#frame-animated-textures)); the bounce and the squashes are not.
  - diffuse alpha = the owner's (0x51A0B3..0x51A0E1) and `SetGlobalAlpha(alfa ≠ 0xFF)` (0x51A0EB), but with arg 2 = 0
    `GetAltitudeAndSetColorSpecular` (0x51A187) rewrites all of +0x4C with table[brightness] (0x803409..0x803413) or
    table[255] (0x803365 / 0x8033DA), with alpha 0xFF (all of `palette.raw` has alpha 0xFF): in the orb the seed goes
    through the 0xC387C8 table with alpha 0xFF ([0xC37D8C], 0x80DEF8), **opaque** (not 0x95). openblack:
    `components::Alpha` = 1.
  - with arg 2 = 0 (all the world calls) `GetAltitudeAndSetColorSpecular` 0x803340 (0x51A187, at +0x14) puts the
    cell's light on the mesh, with no haze afterwards: the `land_light::ObjectMode::Cell` mode of `SpellIcon::Draw`
    (`SpellSeedGraphic::landCellLight`, `LandLightOf` from `RenderingSystem.cpp`). The creature vials go through
    fn_00801C90 + fn_007FEB30 (0x519D90 / 0x519D9E), the model light.
  - the PSys receives the alpha: `GJPSysInterface::SetAlpha` 0x55ED50 (vt 0x12C) writes byte +0x6C of the manager;
    fn_00679860 0x679875 copies it into [0xC0215C] and fn_00679920 0x679BC2..0x679BDF does atom alpha × it >> 8 if it
    is not 0xFF. In the orb (0x95) the seed's additive effect adds 149/256 of its light: without that (before) the
    centre of the bubble came out burnt white and covered the icon. Then it is painted as the
    last step left it.
  - the band if pu ≠ −1: +0x44 += 10.3 × dt ([0xBE8E94]), +0x40 += dt; pu + 1 draws at +0x64 with size
    0.2 × +0x58 × +0x54, rows: identity with row 1 and row 2 swapped (the old 1 negated), rotation (x, z) by base
    + +0x44, (x, y) by 0.3, (x, z) by k, (x, y) by 0.2; base, k = 0, −1 for the first and 0.5, 1 for the others.
    Afterwards fn_0051A830 turns it towards the camera ([0xBE8E8E] = 1; `billboard::BandToEye`, see
    [rendering-objects.md](rendering-objects.md#objects-that-face-the-camera-billboards)).
  - **Band colour** (`SetColour` 0x7F9770 at 0x51A3BE: edx → +0x4C, the argument → +0x50): +0x4C =
    `GetPlayerColour` 0x64D800 (table 0xBFF0B8 by `GetRemapedPlayer`) of the owner (vt 0x1C), or of the local player
    (g_game +0x205A59) if the owner is the neutral one (g_game +0x205A5B) (0x51A322..0x51A36D); its rgb with alpha
    (+0x70 × the caller's alpha) >> 8 (0x51A397..0x51A3B9); +0x70 = 0x3C (fn_00726F10 0x726F4E, only writer), so in
    an icon (alpha 0xFF) the alpha is 59 and in the orb (0x95) 34. +0x50 (specular) = 0x141414 (byte [0xBE8EA0] = 20).
    Red for player 1. openblack: `components::ObjectColour` (new) + `Alpha`; **(approximate)**: the specular is not
    painted (the colour path of vs_object does not have it) and the model light (90 + 166 N·L) is that of the PSys
    mesh atoms. **(inferred)**: the local player is PLAYER_ONE.
  - **Each level is drawn twice** with the same matrix and colour: 0x51A780 vt+0x104 and then 0x51A7A3 vt+0x104 or, on
    the last level with arg 1 = 0 (all calls: icons and orbs), 0x51A796 vt+0x100. The object is an
    `LH3DStaticObject` (LH3DObject::Create(0) 0x80B4F8, vtable 0x9A2974). vt+0x104 = fn_00815980: screen test
    (CheckRegionOnScreen 0x868C80) and distance test, then it draws right away (vt+0x108 = fn_0080DB30). vt+0x100 =
    fn_00815A70: the same test, LOD by distance (vt+0x1D0), records g_last_distance / g_last_selected_box and, if the
    object has bit 0x10 of +4 (vt+0x44 = fn_007F97C0), puts it in the Z-sorter (`NewZObject` 0x83F310 with fn_007FA980
    → vt+0x108, key = distance² to the camera, 0x815F0F..0x815F53); otherwise, it draws right away. That bit is set by
    `SetMesh` (vt+0xF4 = fn_007F9E10 → vt+0x40 = fn_007F97A0) when the mesh has bit 0x200 in its flags (fn_007F9D40),
    and `Power_Up_Band.L3d` has it (flags 0xA2200). So: all draws are immediate except the second one of the last
    level, which goes sorted with the transparent ones (with the object's state when the sorter is flushed, which is
    that of the last level: nothing changes it afterwards). Same material and same face mode in both passes (both end
    in fn_0080DB30): there is no back-face pass and no half band. Additive, so each band adds its light twice.
    openblack: two entities per level (`k_DrawsPerBand`, `extraBands` = 2 (pu + 1) − 1). **(approximate)**: the order
    relative to the bubble (immediate ones before, the Z-sorter one among the transparent ones) is not reproduced: the
    2 (pu + 1) go in openblack's translucent pass.
- openblack: `seed_graphic::DrawUpdateAtPos` / `UpdateOnly` / `DrawSpellGraphic` / `UpdateIconGraphics`;
  `one_off::UpdateFrames` (orb) and `worship::Update` (icons) call them every frame. **(inferred)**: also when they are
  not on screen.
- Creation path: `Create` 0x726F60 → `CreateGraphic` 0x726F00 → the ctor `fn_00726E70` (not an Object: no creation index; `fn_00726F10` zeroes the angles and sets +0x58 = +0x54 = 1) → `fn_00727190`. `ToBeDeleted` 0x726FE0 takes it out of the list and deletes its 3D objects and effect. `SetAutoUpdate` 0x727680.
- The holder PSys (`fn_007276E0`): `PSysInterface::Create(NULL, pt, point + unknown0x154 × scale, 0, scale, NET 0)`, then `AddTarget(this)` and `SetPlayer`. The graphic owns it and steps it itself: every drawn frame (DrawUpdateAtPos / UpdateOnly), and also every turn when auto-updated (`fn_00727350` → `fn_007273A0`, vt 0x100 with the zeroed info). It is drawn as it was last stepped (time multiplier 1, 0x51A27E).
- `DrawSpellGraphic` with arg 2 = 0 (an orb or an icon) queues the whole holder effect as a single Z object (`AddDrawing(t, GetOrigin)` 0x51A2CA). The `CreatureRoom` / `WorldRoom` branch `Draw_(t, 0)` 0x51A291 is not ported.
- With useMesh = 0, 0x519B0D jumps past the whole mesh part, so the y angle +0x3C does not advance either.
- With +0x60 = −1, no band is drawn (the band object stays: `SetPowerUpType` 0x727060), and the band angles +0x40 / +0x44 do not advance (0x51A2D0 / 0x51A2E1 jump past 0x51A2EA..0x51A318).
- Band material: `CreatePUBand` 0x727097..0x7270B8 loads it with `GJUtils::GetSharedMesh` and MaterialProperties {additive 1, Z 0, two-sided 1, change 1, alpha 1}, so `SetMaterialProperties` 0x57E120 turns the band into mode 13 (SRCALPHA / ONE, no Z write, fn_0082ECD0).
- Band constants: +0x40 turns at [0xBE8E90] = 1 rad/s (0x51A305); the size factor 0.2 is [0xBE8E9C] (0x51A70C).
- A creature spell phial calls `SetAnimatedUV_2(1)` (0x519B83) before its 8 × 4 UV frames.
- `TownCentre::AddSpell` 0x744050 creates one icon per seed in the first free slot 0..5 of the village centre;
  `TownCentre::MakeFunctional` 0x743E80 does it for all the magic the town already had and then calls
  `WorshipSite::AddTownSpells`. Each village icon asks the worship site for an icon of its seed
  (`fn_0073D1C0` → `WorshipSite::AddSpellIconIfNecessary` 0x77C9E0); when it is removed, the site's one only
  disappears if no other town of the site has that seed (`fn_0077CAA0`).
- **Tapping** (`SpellIcon::InterfaceTap` 0x726430 → `WorshipSpellIcon::ActualInterfaceTap` 0x77F880): if it is already
  full, the seed goes to the hand; if it is being charged, it is cancelled; otherwise, it starts charging. A village
  centre icon forwards the tap to the worship site icon of its same seed (`TownSpellIcon::GetWorshipSpellIcon`
  0x748F30). The tap sound is `G_ClickOnSpell_01` at a pitch of {100, 115, 130, 145, 155, 175} % depending on the slot
  (`fn_00726490`). The tap pitch table on the stack of `fn_00726490` has 7 entries {100, 115, 130, 145, 155, 175, 190}, but the index is clamped to 0..5, so 190 is never read. The sound is bank InGame, sample 0x2A, not 3D, with the pitch at +0x48 (`GAudio::PlaySoundEffect` 0x429E30 at 0x726528). Sample 42's .sad flags (0x402) have no pitch bit, so the options' pitch is kept.
- **Charge** `StartCharge` 0x77FA00 / `ValidForStartCharge` 0x77FAB0 / `fn_0077FB40` (packet 0x25). When it fills
  (`GetChantNeeded` ≤ 0): if there is already a seed in the hand its upgrade level is raised; otherwise,
  `PutFullyChargedPowerUpSeedInHand` 0x77F8F0 puts it in the hand **already ready** (`fn_00729900(1)`, see
  above) and the miracle's voice plays (`PlayFullyChargedSoundFX` 0x77F4E0, bank `SpellDialogue.sad`).
- **Bug kept in `AddToChantStore` 0x77FDA0:** below the requirement it returns what it put in; above it, it leaves the
  store at the requirement and returns the **excess** `x − (requisito − almacén)`, and it is that excess that is
  charged to the worship site.
- Returning the seed: `CancelCharge` 0x77F9A0 and `ReturnAllChantsToWorshipSite` 0x77FD60 return the store to the
  battery; `SpellSeed::ApplyToWorshipSite` 0x7289C0 / 0x728B30 / 0x729A80 returns the seed's chants to the site of its
  icon (dropping it on the site's ground, giving it to the totem or to an icon, or shaking it off the hand). If it is
  given to an icon **of another seed** of the same player, that icon hands over its charged seed (the exchange). A
  dispenser, a `WorshipTotem` and any icon are "return points" (`IsSpellSeedReturnPoint`), so
  `SpellSeed::CanCast(objeto)` 0x729190 lets the seed be given to them even if the magic cannot be cast on objects.
- With the game flag 0x2000 (`OPENBLACK_INFLUENCE_EVERYWHERE`) the neutral icons charge by themselves at
  `gatheringChantAddPerGameTurn` (61) per turn.
- The **charge ring** (mesh 561 `MSH_S_PULSE_IN`, `TChargingData::Draw` 0x7267A0) uses the fraction
  `almacén/requisito` (1 with a seed in the hand), shown as `(f+0.2)/1.2`, and when it fills it pulses with
  `alfa = 255·(0.1 + 0.5·(sin(4π t)+1)/2)`.
- Icon creation: ctor 0x77F140 = SpellIcon(pos, info, seed, the site's scale, the site's angle); `SpellIcon::Create3DObject` 0x7261A0 = Game3DObject::Create(pos, 0, mesh 203, angle, scale); `WorshipSite::AddSpellIcon` 0x77C430 puts it at the head of the site's list; `CallVirtualFunctionsForCreation` 0x77F290.
- `WorshipSite::GetSpellIconPos` 0x77B080: for the rings 0, 15 and 30 m (step 15 at 0x77B100, cap at 0x77B10A), the first slot 10..15 whose candidate is not within 1.0 of an icon of the site (`MapCoords::IsCloseToEqual`); slot −1 when there is no room. `GetSpellIconPosFromSlot` 0x77AFC0: with ring > 0 the point is put on the ground (altitude 0) and pushed `ring` metres outward from the site's origin (0x77B00A..0x77B02F); with ring 0 the special point stays as it is.
- SpellIcon virtuals: vt 0x910 `GetSpellIconPlacementIndex` (WorshipSpellIcon 0x77FC90 = slot − 10; TownCentreSpellIcon 0x748E90 = its index, `fn_00743FF0`); vt 0x914 `GetWorshipSpellIcon` (a town icon asks its town's site, `fn_0077C2B0`); `SpellIcon::GetPlayer` 0x726540 (the site's player); `GetSpellSeedType` 0x726360; `IsSpellSeed` 0x726310; `GetMagicInfoFromPULevel` 0x7262A0.
- `WorshipSpellIcon::Process` 0x77F390 (every turn): the removal countdown, the neutral self-charge, and while charging the chants go into the held seed; when it is full, the held seed's power-up or the seed into the hand, the voice, and the charge ends. It returns 3 when the icon removed itself. The voice (0x77F520): a seed type above 0x1D gives 8 (0x77F5D6); `GAudio::PlaySoundEffect` 0x429D60(NULL, voice, mode 2, loops 0, 0, is3D 0, bank 9) at 0x77F5EE.
- Charge accessors: `GetHeldSpellSeed` 0x77F490; `GetChantRequiredForSpellSeed` 0x77F840 (the held seed's need at the charged level, otherwise costToCreate of that level); `GetChantNeededForSpellSeed` 0x77FE40; the charge fraction `fn_0077FE80`; `IsCharging` `fn_0077F6D0`; `RemoveFromChantStore` 0x77FE10 (takes out min(x, store)); `UseCreateChants` 0x77FCE0 (the store into the seed, at most its need), from `fn_0077FCD0`; `ValidForPutFullyChargedPowerUpSeedInHand` 0x77F950; `fn_0077FBF0` (a seed of this icon is held and that level's magic is enabled); `fn_0077F780` / `fn_0077F7D0` (a seed made from the icon joins / leaves its list, and bit 0 of the seed's flag); `StartCharge` records the game turn (the symbol given is the setter 0x77F680).
- `WorshipSpellIcon::ValidForRequestSpell` 0x77FBA6 returns 0 when `IsFunctional` (vt +0xD4, MultiMapFixed 0x52EF70) is not 1. In the vtable 0x99D878, `IsBuilt` (vt +0x890) is `WorshipSpellIcon::IsBuilt` 0x77FEE0 = the site's `IsBuilt`, or 1 without a site.
- The charge ring's mesh: `SpellIcon::TChargingData::Init` 0x7266E0 loads mesh 0x231 (561). Per frame, `SpellIcon::DrawMagicSystem` 0x726D20 → `TChargingData::Draw` 0x7267A0, while charging (a second reading of the ring's pulse is in Pending).
- Town centre icons: `TownCentre::AddSpell` 0x744050 does nothing if an icon of that seed exists. Otherwise it calls `fn_00748CB0(pos, &spellIcon[1], seed, TC, the point's angle, TC scale, 1, 0)` → `fn_00748BF0` → the TownSpellIcon ctor `fn_00748A70` (which calls `fn_0073D1C0`), then `CallVirtualFunctionsForCreation` 0x748D20. The slot comes from `fn_00743F60` (the first free slot, `GetSpecialPos` 0x63B0B0).
- **Bug kept (TownCentre::AddSpell):** when the first free slot has no special point, the loop goes on (0x7440A5), but `fn_00743F60` always tries the first free slot again (0x743F68). Every later try fails too and AddSpell returns 0 (0x7440AE).
- Town centre icon functions: `FindSpellIcon` 0x743FA0; `fn_00744120` counts the icons (`CanTownCentreHoldMoreSpells` 0x73D5F0: < 6); `fn_007442C0` removes a seed's icons (`TownSpellIcon::ToBeDeleted` 0x748AE0 → `Town::RemoveSpellIcon` 0x73D220 → the site's `fn_0077CAA0`); `TownCentre::AddPowerUp` 0x744010 / `fn_00744030` → `SetPULevel` 0x748EB0; `UpdateGraphicWithPULevels` 0x748ED0 shows the highest level the town holds; `SpellIcon::ToBeDeleted` 0x7260A0 takes the icon out of the map (vt +0x548, MultiMapFixed 0x52E7B0), then the graphic.
- `TownCentre::DeleteDependancys` 0x743BE0 (0x743C4E..0x743C80), for each of the 6 slots +0xD0: the slot is set to 0, the icon's +0x118 (its town centre) to 0, and the icon is deleted (`ToBeDeleted(0)`) unless its +0xA & 1 is set (flag not identified).
- Town magic: `Town::AddMagicTypesHeld` 0x73D380 (if not held yet: held, the owner's `SetMagicTypeEnabled(1)`, and `TownCentre::AddSpell` for a base magic or `AddPowerUp` for a power-up; returns 1 if newly added); `RemoveMagicTypesHeld` 0x73D450 (the mirror); `IsMagicTypeHeld` 0x73D630; `Town::GetNextSpellIcon` 0x73D360; `fn_0073D2E0` (the town has a TownSpellIcon of that seed). `WorshipSite::AddSpellIconIfNecessary` calls `WorshipSpellIcon::StopRemoveFromPlayer` 0x77FF40 when the icon was fading.
- Creature theft (not ported): `fn_0073D500` takes the seed's base magic and its held power-ups from a town (recording which ones); `fn_0073D5A0` gives them back or to another town.
- Player-wide icon functions (GPlayer): `fn_0064BB10` = the largest charge fraction of the icons charging for the player (per icon `fn_0077F690`); `fn_0064BDE0` = some icon is `ValidForRequestSpell(status, −1, 1)`; `fn_0064BDB0` = packet 0x25 → 0x5DABA0, which calls the best icon's `fn_0077FB40(status, −1, 1)`; `fn_0064BD90` / `fn_0064BD50` (packet 0x26 0x5DABF0) = the same with the last seed type; packet 0x6A 0x5DAC30 → `fn_0077FC30`; `GPlayer::CancelAllSpellsCharging` 0x64BC60 (CLEAR_PLAYER_SPELL_CHARGING); `IsAtLeastOneSpellBeginToBeCharged` 0x64BB90 / `IsSpellBeginToBeCharged` 0x64BBF0 (an icon of that magic, via `fn_00726380` → `fn_0072B230`, whose store is above 0); `GetNextInterfaceStatus` 0x64AAC0.
- `GPlayer::IsMagicTypeEnabled` 0x64C220 (the cheat, or a holder enables it). `SetMagicTypeEnabled` 0x64C300: on → one more holder and "ever enabled"; off → one less (never below 0); then every icon of the player's six sites redraws its levels (`fn_0077B8A0`). `SetMagicTypeEverBeenEnabled` 0x64C250 / `HasMagicTypeEverBeenEnabled` 0x64C260.
- Returning a seed: `SpellSeed::RemoveFromHand` 0x728F00 → `MapCoords::FindWorshipSite` → `ApplyToWorshipSite` 0x7289C0 (returns 3), otherwise 0x17 (0x728F1C). A forced throw (`SpellSeed::ThrowObjectFromHand` 0x72ACD0) uses `ApplyToWorshipSite` 0x729A80, and the seed goes in every case. `SpellSeed::InterfaceSetOutMagicHand` 0x728940: the seed's icon stops charging for that hand (`CancelCharge`), and the interface remembers the seed type (`fn_005DCA20`).

### The worship percentage and the villagers (`Worship/WorshipPercentage.cpp`, `VillagerWorship.cpp`)

- `Town::SetWorshipPercentage` 0x73C060 (dragging the totem, `TotemStatue::NetworkUnfriendlyLockedSelect` 0x7386A0:
  `pct = clamp(pct + dy × 0.1; 0; 1)`): 0 without a worship site; otherwise it is stored, passed to the totem
  (`TotemStatue::SetWorshipPercentage` 0x738270, which raises it 8 m with a *Zoomer* of |Δ|·5200 ms, which is in ms:
  [engine-math.md](engine-math.md#zoomer-lh3dlib)) and sent to the villagers
  that are missing.
- `Town::GetWorshipersNeeded` 0x73C860: `target = pct > 0 ? max(1, int(population × pct + 0.5)) : 0`;
  `result = target − (worshipping + on the way) + those asking to go home`.
- `Town::AdjustWorshipersWorshipping` 0x73C0F0: two passes (the second also accepts those flagged 0x200); to send, the
  available villagers **closest** to the dance centre first
  (`fn_0073C590` = `GetDistanceModifier(distance; distance del centro a la ciudad + 100) × vida³`); to withdraw,
  those who are at or going to the site, the **farthest** first (state 163).
  - `GetDistanceModifier` 0x74F290 is `SigmoidThreshold(0.5; 1 − min(d, max)/max)`, with the threshold in the **first**
    argument (`push 0x3F000000` at 0x74F2B7): it **decreases** with distance, from 0.99996 at d = 0 to 3.6e-5 at
    d ≥ max (see [engine-math.md](engine-math.md#gutils-distances)). openblack passed them the other way round and
    sent the farthest ones first; corrected.
  - It is **life³**, not life²: after `GetLife` (0x73C630) the loop 0x73C63A..0x73C644 (`mov eax, 2`, and two rounds
    of `dec eax; fmul life; jne`) multiplies the life twice more, and the modifier comes in at the end (0x73C646).
- Villager states (table in `LivingActionSystem.cpp`): **59** arrives at the site (0x76BE00; within 10 m of point 9 it
  joins the dance if `N < maxDancersVisible`, otherwise the hiding place), **60** dancing (0x76C680), **213** hidden
  (0x76C5E0) and **248** goes back home (0x761B70). Exits `ExitMoveToWorshipSite` 0x76C170 and `ExitAtWorshipSite`
  0x76C1F0. The original's 58 is the walk along the path (`SetupMoveToOnFootpath`); openblack walks with the WallHug
  inside 59, so 58 is not used. `Villager::CheckNeededForWorship` 0x76BA60 enters from `DECIDE_WHAT_TO_DO`.
  - **Watch out:** openblack's `k_VillagerStateStrings` is wrong at indices 248..254 (it says `RESTART_MEETING`...);
    the `VillagerStates` enum does match the original and is what indexes the table.
- `Villager::ProcessInWorship` 0x76C890 every turn: `CheckVillagerGoBackToTownFromWorship` 0x76BEC0,
  `CheckRequestGoHome` 0x76C8D0 (with life < `damageThresholdToGoHome` 0.3 it signs up in the queue, sorted by the
  life desire `GetLifeDesireFromLife` 0x75BBC0) and `ReduceVillagerLifeByChant` 0x76C800
  (`life -= chantDamage × chantLifeRate`, 5e-6; on reaching 0 it dies with reason 4 and is counted by
  `GET_TOWN_WORSHIP_DEATHS`).
- `Villager::CanIGetToTheWorshipSite` 0x76BC20: within `maxDistanceThatVillagersWillGoToWorship` (500).
- Not ported: eating at the site (state 241, needs the villager's stomach) and carrying supplies (states 42-46).
- The totem drag is packet 0x29. `TotemStatue::ValidForLockedSelectProcess` 0x738500 allows it only for the same player, a built statue, the player's citadel heart built and the town's worship site built. `Town::GetTotemStatue` 0x73E1D0.
- `Town::AdjustWorshipersWorshipping` 0x73C0F0 has the parameters (n, skipLifeCheck, requireReachable). Villagers sent to worship need a life above `damageThresholdToGoHome` (each villager's own GVillagerInfo +0x35C, 0x73C2DE), unless skipLifeCheck is set.
- `Town::AddVillagerOnWayToWorshipSite` 0x73E300 / `RemoveVillagerOnWayToWorshipSite` 0x73E360; `fn_0073E3E0` / `fn_0073E3F0` (the town's count of villagers at the site).
- The totem rise is advanced in two places, one float step at a time, with the engine ms since +0xD0: inline in `TotemStatue::Draw` 0x7389C9..0x738A78 (per frame) and in `TotemStatue::Process` 0x737FAF..0x738061 (vt +0x5FC, from `TownCentre::Process` 0x743E05). `Draw` 0x738960 stores +0xD0 every frame from the creation on.
- Totem sounds: `SetWorshipPercentage` sets +0xCC = 1, `ReleaseLoopOnSoundEffect(this, 0x1E, bank 1)` 0x42A330 and `SoundTag::Create(this, 0xB, track 0, mode 2, loops 0, 0, is3D 1, bank 1, delay 0)` 0x71E840 (the rising loop, 0x738434..0x73845B). In `Process`, when |value − destination| < 0.005 ([0x999A90], qword) while +0xCC is set: +0xCC = 0, `SoundTag::Remove(this, 0xB, bank 1, stop 1)` 0x71EC30, and the stop sound 0x1E at the totem's point (0x73808C..0x73810B).

### Dispensers and fireflies (`Worship/SpellDispenser.cpp`, `Worship/FireFlyReward.cpp`)

- `SpellDispenser` is an Abode with its magic and its period. `SpellDispenser::Process` 0x722A70: while its orb still
  exists and touches it, it waits; otherwise, every `periodo` turns it creates another (`CreateOneOffSpellSeed` 0x722B80
  → `OneOffSpellSeed::Create` at its position + 1.2 × its height, site visual 9). The default period is
  `timeEachMobileObjectTakesToProduce` = **300** turns; `SET_MAGIC_PROPERTIES` 0x70CC30 and `SET_TIMER_TIME` 0x711280
  change it in seconds (× turns per second) and a period of 0 disables it. Giving it an uncast seed turns it into an
  orb there, losing its chants (`fn_00728C50`, visual 0x1B).
- **Which lands have them.** `CREATE_SPELL_DISPENSER` only appears in Land3 (`WOOD`, 600 turns), Land5 (`FIRE_PU2`,
  600 turns) and the playgrounds. The challenge script's `GiveSpellDispenserReward` (`CREATE_WITH_ANGLE_AND_SCALE(SPELL_DISPENSER)`,
  `SET_MAGIC_PROPERTIES`, `SET_ACTIVE`, `SET_TIMER_TIME`) is the only `SPELL_DISPENSER` it creates, and it is run only by
  `SpellDispensors` (nine dispensers in a row, magic types 1..9, 250 s), which only `LandControlT`, the test land,
  runs. So **Land1 and Land2 have no dispenser** in a normal game.
- **The orb's seed.** `CreateOneOffSpellSeed` 0x722B80 takes its magic's `GMagicInfo` (`fn_00722B20`: the pointer
  table 0xD37D10 indexed by the magic type), its seed field +0x28 (`fn_005FB400`: 0xD9D678 + 400 × seed, back to the
  index by `fn_0072B200`) and the level of that magic in that seed (`fn_0072B010`). info.dat leaves the field at −1 in
  every row (+0x28 is the record's +0x18, after the 0x10-byte `GBaseInfo`), but it is **filled at load**: the end of
  `load_variables` calls `fn_0042B400` (0x42D547), which for magic types 1..41 writes
  +0x28 = `GSpellSeedInfo::GetFirstSpellSeedForMagicType(type)` when that is not −1 (0x42B40E..0x42B421), and
  +0x2C / +0x30 the gesture and level of `fn_0072B100` (0x42B432..0x42B44D) when that gesture is not 0.
  `fn_0072B100` 0x72B100 is `fn_0072B010` 0x72B010 on the first seed with the type (or gesture 0 and level −1 without
  one); `fn_0072B010` gives, for a type in `magicTypes[1..3]` at slot k, `powerUpGestures[k]` (+0x134 + 4k) and level
  k, and for the base type `magicTypes[0]` (+0x124) gesture 0 and level −1. So only the power-up types get a gesture
  and a level (FIRE_PU1 INVERSE_SPIRAL / 0, FIRE_PU2 SPIRAL / 1, …); the base types keep 0 and −1. Type 0 is not
  touched (the loop runs 0xD37D14..0xD37DB8). So a dispenser's
  orb is the first seed that has its magic type (Land3's WOOD → the WOOD seed, Land5's FIRE_PU2 → the FIRE seed),
  the same choice the firefly reward makes through the same field (`fn_0052B6F0`, 0x52B744). openblack:
  `magic::GetSpellSeedOfMagicInfo` (`Magic/MagicTables.cpp`), used by `dispenser::CreateOneOffSpellSeed`. Until
  2026-10-08 openblack read the raw −1 and its dispensers made no orb at all.
- **The other readers of the filled fields.** `SpellSeed::DoPreCastThings` 0x729460 tests the filled +0x28 of the
  magic being cast (0x729502: `== 2`, FIRE → magnitude 1; the magic is the seed's last cast one, +0x98, or its magic
  at its level, `Cast` 0x72953E / 0x729549). `SetupPowerUpGestures` 0x5CEE30 sets the interface's current power-up
  gesture (+0x178) to the filled +0x2C of the held seed's magic at its level (`GetMagicInfoFromPULevel` 0x72AFE0 with
  seed +0x6C, 0x5CEE57..0x5CEE6C). `PSysManager::GetPowerUpLevel` 0x673510 reads the filled +0x30. openblack:
  `GetSpellSeedOfMagicInfo` in `PrepareCast` (`SpellSeed.cpp`), `GetGestureOfMagicInfo` in
  `gestures::SetupPowerUpGestures` (`PowerUpSystem.cpp`), `GetPowerUpLevelOfMagicInfo` in the spell's PSys sink
  (`Spell.cpp`).
- **Fireflies** (`FireFly.cpp` 0x52B5A0..0x52B790): when picking up with the hand an object on which a firefly was
  sleeping (`fn_0052B600`, from `GInterface::PlaceObjectInMagicHand` 0x5DA6F0) a one-off miracle is drawn with the
  probabilities of `FIRE_FLY_SPELL_REWARD_PROB`, which **only Land1.txt uses** (HEAL 20; FIRE, LIGHTNING, NATURE,
  FOOD, WOOD and WATER 1 each): `r = GameFloatRand(total)`, the first miracle whose cumulative sum reaches `r`, its
  first seed and level, and an orb if that seed exists (`GSpellSeedInfo.exists`).
- `SpellDispenser::Create` 0x7228D0 (pos, abode info, town, y angle, scale); `fn_007227B0` zeroes its fields. `CallVirtualFunctionsForCreation` 0x7227D0 creates its effect, particle type 0x90, on the land under it.
- With no town, `fn_00723010` takes the head of `GetPlayer(0)`'s town list (+0xA50: PLAYER_ONE's oldest town, filled at the tail by `fn_0064C090`). A script town number goes through `GGame::FindTownWithID` 0x552FA0.
- `SpellDispenser::Process` (vt +0x5FC, after `Abode::Process`, 0x722A73) runs from the town's abode pass `fn_00747600` every processAbodeEvery turns, so the period counts those calls. The orb is at +0xCC: while it is available and touching the dispenser (vt +0x6B8 `Object::IsTouching` 0x637E00, 0.001), it waits; otherwise +0xCC = +0xC4 = 0 (0x722A78..0x722ABF). It also needs IsActive (vt 0x40C), a magic (+0xD4), IsBuilt (vt 0x890) and IsRepaired (vt 0x88C) (0x722AC0..0x722AF2). Then ++ +0xC4, and at ≥ +0xC8 (the period) `CreateOneOffSpellSeed` (0x722AF4..0x722B0F).
- `fn_00723030` sets the dispenser active; activating it makes an orb at once. `CREATE_SPELL_DISPENSER` writes +0xD4 = magic, `fn_00723030(1)`, +0xC8 = ftol(period) in turns; with 0 it calls `fn_00723030(0)`.
- Giving a seed to a dispenser (`fn_00728C50` / `fn_00728C80`) only works for a seed that is not cast yet (+0x98 == 0).
- Fireflies: the probabilities are at 0xCCFBAC and their running sums at 0xCCFB04 (the total at 0xCCFBA8), set by `fn_0052B630` (map command 88). `fn_0052B6F0` draws `GameFloatRand` 0x6DE530 of the total; nothing for a total of 0; the first magic whose running sum is ≥ r (a probability of 0 is never picked); its seed through `fn_005FB400` / `fn_0072B200`. In `fn_0052B600`, after the object's `InterfaceSetInMagicHand`, the firefly at exactly its MapCoords goes (`fn_0052B5A0`), then the reward is drawn.
- **Bug kept:** `FireFly::OnClearMap` 0x52A1E0 clears only the probabilities. The running sums, which are what `fn_0052B6F0` reads, keep the last land's values until the next `FIRE_FLY_SPELL_REWARD_PROB`.

### Script (`Magic/Script/CHLWorship.cpp`, the worship part of `MapScriptMagic.cpp`)

- Map commands: `CREATE_TOWN_SPELL` / `CREATE_TOWN_CENTRE_SPELL_ICON` (10 and 12, the same handler),
  `CREATE_NEW_TOWN_SPELL` (11), `CREATE_SPELL_ICON` (13, does nothing, not even in the original),
  `CREATE_PLANNED_SPELL_ICON` (14, only the town's magic type), `CREATE_WORSHIP_SITE` (19),
  `FIRE_FLY_SPELL_REWARD_PROB` (88) and `CREATE_SPELL_DISPENSER` (90).
- CHL natives: 330 `IS_SPELL_CHARGING` 0x70CB80, 331 `IS_THAT_SPELL_CHARGING` 0x70CBD0, 355 `GAME_SET_MANA` 0x6FE800,
  356 `SET_MAGIC_PROPERTIES` 0x70CC30, 376 `SET_CAN_BUILD_WORSHIPSITE` 0x6FEC40, 386 `SET_MAGIC_IN_OBJECT` 0x6FF0B0,
  410 `GET_TOWN_WORSHIP_DEATHS` 0x6FF640, 422 `GET_MANA` 0x6FE8C0, 423 `CLEAR_PLAYER_SPELL_CHARGING` 0x70CD80 and
  453 `GET_SPELL_ICON_IN_TEMPLE` 0x6F3590; plus the dispenser branches of `SET_ACTIVE` (255) and `SET_TIMER_TIME` (145)
  and the `CREATE` types 30 `ONE_SHOT_SPELL`, 31 `ONE_SHOT_SPELL_IN_HAND` and 36 `SPELL_DISPENSER` (`GScript`
  0x6F1010).
- `CREATE_WORSHIP_SITE` handler at 0x7160C8. `CREATE_SPELL_DISPENSER` arguments (case 90): town, pos, abode, magic, y angle, scale, period.
- `SET_ACTIVE` 0x6FD720 and `SET_TIMER_TIME` 0x711280 act on a dispenser through `fn_00723030`. `SET_TIMER_TIME` computes ftol(1000 / [0xD01A38] × seconds) inline (0x711338..0x711360); what it does with 0 is in Pending.
- `GET_SPELL_ICON_IN_TEMPLE` → `fn_00463AD0`: the site icon of that magic, in any of the sites.
- `GET_TOWN_WORSHIP_DEATHS` → `Town::GetDeathsFromWorshipping` 0x740D60 → `GetDeaths` 0x740D70: TownStats +0x7C[4 CHANT], written by `Villager::VillagerDead`.

### Selection by gesture

The icons seen by the hand's selection system are registered with `gestures::SetIconProvider`
(`Worship/GestureIconProvider.cpp`): those of the interface's player, in the order of the lists of its six sites
(`GPlayer` 0x64BAB0..0x64BF40). `GPlayer::FindBestSpellIconForSpellSeed` 0x64BF40 chooses, among the valid icons of that
seed, the one of the site with the most chants available.

- `FindBestSpellIconForSpellSeed` starts at best −1 (0x64BF64) and needs a strictly larger value (0x64BFBD), so the first site wins a tie.
- `fn_005CF040` walks the six sites' icons in list order and keeps the category's icons that are `ValidForRequestSpell(status, −1, 1)`. Packet 0x25 (0x5DABA0) checks `IconValidForRequest`, then requests from the best icon. Packet 0x26 (0x5DABF0 → `fn_0064BD50`) uses the interface's last seed type (`fn_005DCA40`).

### Checks in the game

Land2 with `-s Land2.txt`, Land1 with `-s Land1.txt`; the hooks are in
[openblack-internals.md](openblack-internals.md#debug-environment-variables):

- PLAYER_TWO's citadel with its two worship sites (the Norse one from `CREATE_WORSHIP_SITE` in
  slot 5 and the Greek one that `PostLoadCleanup` adds for town 2 in slot 0), both at the citadel's origin and rotated
  to their slot, with their food cauldron and their altar.
- The site's four spell icons (FIRE, NATURE, FOOD, WOOD from town 1) at points 10..13 of the
  `b_worship` mesh, with their `SpellSeedGraphic` and their effect above.
- With `OPENBLACK_TEST_WORSHIP="1.0.5"`: 11 of the 22 villagers of town 1 at the worship site. The
  log gives `site 149 icons 4 N 11 C 33.0 k 0.223 strain -1.000 battery 6824 / 12300 available 6857 damage 0.67`:
  capacity 11 × 3, maximum 9000 + 11 × 300 and the damage per dancer exactly as in the original.
- The same: town 1's totem raised on its plinth (8 × 0.5 = 4 m) and the village centre's icons
  around it.
- With `OPENBLACK_TEST_WORSHIP_SITE="NORSE,FIRE,HEAL,FOOD,WOOD"`,
  `OPENBLACK_TEST_TOWN_SPELL="0,FIRE;..."`, `OPENBLACK_TEST_MANA=40`, `OPENBLACK_TEST_TAP_ICON="FIRE,5"`: the charge
  ring lit over the FIRE icon while it fills at 40 chants per turn.
- With `OPENBLACK_TEST_MANA=20000`: with the battery full the icon fills in one
  turn and `Worship: seed 3093 of icon 3086 in the hand with 7000 chants` (FOOD costs 7000).
- With `OPENBLACK_TEST_DISPENSER="NORSE_ABODE_SPELL_DISPENSER,1826.2670,WOOD"`,
  `OPENBLACK_TEST_FIREFLY_REWARD="1846.2670.3"`: Land1's miracle dispenser with its WOOD orb and three firefly
  rewards. The total of the probabilities is 26 (HEAL 20 + six of 1), as in Land1.txt, and HEAL, WATER and FOOD came
  out.

The worship site's `b_worship` mesh wears its temple's texture, as the original (see "The site's texture" above;
it used to come out with a wrong texture).

### Differences from the original and what is missing

- openblack's worship sites are born **built**: there is no construction work and no `BuildingSite`, so
  `CREATE_PLANNED_WORSHIP_SITE` does nothing and a planned citadel gets its six slots anyway.
- **Dragging the totem with the hand is not wired up** (`percentage::TotemTown` is ready for it): the percentage is
  tested with `OPENBLACK_TEST_WORSHIP`.
- Not ported: the real dance of the `.DAN` files, the site's artifacts, the mana path sprite
  (`CreateManaPathSprite` 0x77B2C0, on the casting side), the supplies to the site, the reward chests and the
  stealing of spells by the creature (with the creature).
- **Unverified:** the villager's bit 0x200 that the second pass of
  `AdjustWorshipersWorshipping` accepts; `maxDistanceForVillagersToGoToTheWorshipsite` (1000) and
  `minLifeForVillagersToGoToTheWorshipsite` (0.4), which are not read in the ported functions; the counter
  `WorshipSpellIcon +0x114` (nobody activates it in the executable).

## Influence (`src/ECS/Influence`)

All distances are in x,z
(`GetDistanceInMetres` 0x74CD70).

- **Query.** `Influence::CalculatePlayerInfluence(pos, jugador, 0, tipo, aliados)` 0x5CD170 returns -1 to 1; "in the
  influence" is `> 0`.
  - Without a player it gives 0.
  - With the game flag 0x2000 (the registry value "GatheringFlag", `start_system` 0x6433B1) it gives 1 everywhere. In
    openblack it is `OPENBLACK_INFLUENCE_EVERYWHERE`.
  - Then it looks at the virtual influence (`SET_VIRTUAL_INFLUENCE`, not ported; it is the only one that reads `tipo`)
    and afterwards `CalculatePlayerRawInfluence`.
  - If that gives ≤ 0 and allies are requested, it returns that of the first ally with influence (`IsAllied` and
    +0x950 > 0.1). If there is no ally, 0.
  - Who calls and with which arguments:
    - cast rules 2 and 3 (fn_005FB5D0): allies = 1;
    - the hand, `m_InInfluence` (GInterface+0x48, fn_005D1120): with the hand's position, type 1 and allies = 1;
    - `GInterfaceStatus::Process` 0x5DC558: drops the locked object (picking up in batches) outside the influence. It
      is already in `HandResources.cpp`.
- **`CalculatePlayerRawInfluence`** 0x5CD230:
  - it sums the citadel, the player's towns (GPlayer+0xA50) and the rings, and clamps it between -1 and 1;
  - an anti ring of the same player covering the point returns 0;
  - a ring attached to an object that is in the hand does not count;
  - `CameraExclusion::InsideInclusion` is always true in a normal game (it is only used by the camera force field of a
    saved game).
- **Citadel and towns: all or nothing.** They contribute their radius if the point is inside, so the sum goes past 1
  and stays at 1. For the citadel, inside is `r > d` (fn_004630F0); for the town, `d < r` (fn_007479E0).
  - **Citadel.** `Citadel::GetInfluence` 0x464090 = `playerInfluenceMultiplier × Citadel+0x6C`.
    - +0x6C is set once, when the first CitadelHeart is created (0x4649B0): `M2 × (land ? storyInfluence[land-1] :
      influence)` from GCitadelHeartInfo, that is 125, or 750/450/250/450/450 on lands 1 to 5.
    - M2 is 1 with `CREATE_CITADEL` and the blueprint's scale with the planned citadel. In Land1 it is built by the
      challenge script: `BUILD_BUILDING(1915.05, 2508.89, 1.0)` → `ForceBuildingOfPlannedAtPos` →
      `CreatePlannedNoFixedCheck`.
    - Result: **750 m in Land1**, 450 in Land2 and 250 in Land3 (which is why in Land3 you start with so little).
  - **Town.** `Town::Process` 0x747380 recalculates the radius (+0x5C8) every turn:
    - the base is `Town::GetBaseInfluence` 0x73FD40: GTownInfo's `influence` (25), or its `storyInfluence[land-1]`
      (25/25/25/50/25);
    - to that is added, every `processAbodeEvery` (1) turns, the `GetInfluence` of each building in the town, except
      if Town+0x5F8 (last argument of the constructor, 0 in `CREATE_TOWN`; it is not `SET_TOWN_UNINHABITABLE`, which
      writes +0x5F4);
    - the total is multiplied by `townInfluenceMultiplier`;
    - only the player's own towns count: a NEUTRAL one only counts for the neutral player.
  - **Building.** `Abode::GetInfluence` 0x4072A0 =
    `% built × scale × life × GAbodeInfo::influence × (adults +0xB4 + children +0xB7 + 1)`
    (`MultiMapFixed::GetInfluence` 0x52ECA0 times that factor).
    - `influence` values: houses 5, totem and village centre 90, store 45, workshop and dispenser 25, graveyard
      30, crèche and football pitch 20, wonder 150, field 5.
    - Fields are also Abodes.
    - openblack's `GAbodeInfo::Find` would return the tribeless records at the end (ark 1, totem 120), so the record
      is looked up by the mesh.
  - **Land globals.**
    - The multipliers are 1 by default (GGame::Init). They are changed by `SET_TOWN_INFLUENCE_MULTIPLIER` (case 96:
      Land3 0.5, Land4 0.8, Land5 0.6) and `SET_PLAYER_INFLUENCE_MULTIPLIER` (case 97).
    - `SET_LAND_NUMBER` writes g_game+0x205A08.
    - Land 6 reads the float that comes after the story array.
- **Rings** (`InfluenceRing`, 0x44 bytes; list g_game+0x205C4C, newest first):
  - Fields: position, followed object (+0x28), player (+0x34), radius (+0x38) and anti (+0x3C).
  - They contribute `Influence::CalculateInfluenceOnRange(d, r)` 0x5CD560, with GInfluenceInfo 0.4 / 0.2 / 0.2:
    - 1 up to 0.4·r;
    - from 0.8 to 0 up to 0.6·r;
    - from **0.2** to 0 up to r. The 0.2 is a double at 0x8C7C68, and the jump from 0 to 0.2 at 0.6·r is kept.
  - `ProcessRings` 0x5CDB90: the ring follows its object, and if the object disappears it is deleted with it.
  - `IsInAntiInfluence` 0x5CD490: the point is inside an anti ring of that player (`d ≤ r`).
- **Scripts.**
  - `CREATE_INFLUENCE_RING(pos, jugador, radio, anti)` (case 59).
  - CHL `INFLUENCE_OBJECT` (60) and `INFLUENCE_POSITION` (61): on the stack go anti, player (game index, not
    converted), radius and object or position; they return the ring.
  - CHL `GET_INFLUENCE` (62): on the stack go position, `raw` and player (script one: 0 = the local one, n = n − 1).
    Allies = `raw == 0`.
  - LandT's script opens a 1000 m ring at (2185.6, 2409.5).
- Rings: `InfluenceRing::Create` 0x5CD9D0 → ctor 0x5CD760 (at the head of the list); a ring that follows an object is `fn_005CD990` → `fn_005CD800` (it takes the object's position and keeps a weak link); `InfluenceRing::ToBeDeleted` 0x5CD8A0; per ring in `ProcessRings` `fn_005CDC00`; a ring's contribution `InfluenceRing::CalculateInfluence` 0x5CD900; anti test `IsInInfluence` 0x5CDA60 (d ≤ r). The shields' anti rings are made with flag 1.
- `Influence::IsInPlayerRawInfluence` 0x5CD460.
- `GInfluenceInfo` (0.4 / 0.2 / 0.2) is at 0xD17CC0. The int third argument of `CalculateInfluenceOnRange` 0x5CD560 is not read.
- The single `GTownInfo` is at 0xDA2780 (`Town::GetBaseInfluence` 0x73FD40). The town total is multiplied by `townInfluenceMultiplier` only when the town has a player.
- "Object in the hand" for a ring is `fn_005CDBD0` plus the `obj+0x24 & 4` test.
- `CalculatePlayerInfluence` (not ported): `GPlayer+0x934` (only ever 0) and the virtual influence of each of the player's interface statuses (`fn_0076D330`). The ally search is `fn_005CD400` (`GPlayer::IsAllied` 0x64D5D0, +0x950[ally] > 0.1).
- Script natives: `INFLUENCE_OBJECT` = `GScript::InfluenceObject` 0x6F9AA0 (pushes the ring, via AddScriptGameThing, or 0); `INFLUENCE_POSITION` = 0x6F9B60; `GET_INFLUENCE` = 0x6F9C60. `GGame::GetPlayer` 0x5509B0 maps the game index 0..7 and returns NULL from 8 on.
- The player's influence power +0x8C (`GPlayer::CalculateInfluencePower` 0x64AD00) and `fn_0064B700` (the ratio) are in [audio.md](audio.md) (heartbeat, `beliefShare`).
- **Drawing: the border** (`InfluenceCircle`, `src/ECS/Influence/InfluenceCircles.cpp` and
  `src/Graphics/RendererInfluence.cpp`):
  - **The list.** `GGame::Update3DInfluence` 0x555280 (from `GGame::ProcessTurn` 0x54E738) rebuilds it only when the
    dirty byte g_game+0x250174 is set **and** `GameTurn % 10 == 0`: one circle per citadel and per town with influence,
    in the owner's colour (the rings, anti rings and shields are never drawn). The byte is set by fn_00555240 when a
    radius moved by more than 0.01 since the last rebuild (`Citadel::Process` 0x4630C6 against citadel +0x78,
    `Town::Process` 0x74759E against town +0xF24) and by `ForceNeedUpdateInfluence` 0x555270 (a deleted citadel or
    town, a town changing owner).
  - **Overlaps** (fn_00827040, at each `Add`): a circle fully inside another one **of the same player** (3D distance of
    the centres) is deleted; where two of them cross, the columns inside the other go transparent (fn_00827110). Quirk
    kept: a hidden closing column turns white (0x00FFFFFF), so its two segments fade towards white.
  - **The curtain** (`land_morph::InfluenceCurtain`, fn_008265F0): 40 units high, three rows at H, H + 20 and H + 40;
    only the middle row gets an alpha, so it is a soft band that peaks 20 above the land. `burn.raw` / `burna.raw`,
    scrolled +0.0001 u and −0.0002 v per game ms (global clock [0xEB9A40], `frame_anim::InfluenceScroll`), material
    [0xEB9A18] (mode 6, two-sided, tiled: `materials::k_InfluenceCircle`), colours `g_players_color` [0xEA9EFC]
    (`influence::k_CircleColours`, alpha 0; blue and white differ from the generic table 0xBFF0B8).
  - **When.** `InfluenceCircle::Draw(1)` 0x826C90 runs **every frame in the world view** (`GGame::Process3dEngine`
    0x54E3D2..0x54E3DE): there is no option, no hand-proximity test and no fade timer. `WorldRoom::ShowInfluence`
    [0xC2A478] only gates `Draw(0)`, the map in the temple's world room (0x54E3E0..0x54E412). Nothing is drawn while
    g_camera.y ≤ 100; the middle alpha is 120 from y = 200 up and ftol((y − 100) · 0.01 · 120) below. It is drawn at
    once, after everything else drawn at once and before the Z-sorter drain, so every Z-sorted blended thing is drawn
    over it; it writes no Z.
  - **The latch** [0xEB9A1C + 4p]: cleared on every land load (fn_00828A50 from `LH3DIsland::Create`); set by the
    citadel's 3D object when its fade reaches 1 (fn_00883120 0x8831AD). Until then the player's circles are drawn with
    alpha 0 and crossing them makes no ripple and no sound. (inferred) openblack has no temple fade, so it is set as
    soon as the player has a temple (`influence::ProcessCitadels`).
  - **The ripple** (fn_00827250..fn_00827500): crossing a border with the hand (fn_00827820) makes 7 growing rings of
    `smoke.raw` cell 63 in the player's colour, standing in the curtain's plane at the crossing point (bisection
    fn_00827670), for 2 s, Z-sorted; and sound 52.
  - The circle list [0xEB9A14] is newest first (the ctor links at the head, 0x826608..0x82661B); each circle is new(0x38) at 0x826FB6. Fields: +0x18 the hidden flags ((3N + 3) × 4 bytes allocated, N + 1 used), +0x2C the colour index & 7 (0x826632), +0x30 dead, +0x34 the alpha cache. `InfluenceCircle::Add` 0x826FA0 = the ctor `fn_008265F0`, then every dead circle is deleted (0x826FFA..0x82701F).
  - `g_players_color` [0xEA9EFC] is written by `fn_00826510` (0x826520..0x8265E5). Colour indices 5 and 7 are the ones that differ from the generic table 0xBFF0B8 (there 0x4777FF and black).
  - `Update3DInfluence` starts with `InfluenceCircle::Reset` 0x826C50 and walks the players with `GetNextPlayer` 0x5508A0, which stops at the neutral one (0x5508C6): the neutral player gets no circle. A citadel circle needs `Citadel::GetInfluence` ≠ 0 (0x55530E), a town circle +0x5C8 ≠ 0 (0x555354); the dirty byte is cleared at 0x555384.
  - `ForceNeedUpdateInfluence` 0x555270 is called from `Citadel::ToBeDeleted` 0x462C9B, a town changing owner (`fn_00649810` 0x6499E2), `Town::ToBeDeleted` 0x739993 and `TownCentre::Process` 0x743E63.
  - `SetCurtainAlpha` (0x826F0D..0x826F57): only when the alpha differs from +0x34 and the player's border is shown; only the middle vertex (3j + 1) of each column that is not hidden takes the alpha.
  - Overlaps (`fn_00827040`, for each older circle, newest first): nothing if either circle is dead or their players differ. d is the 3D distance between the centres: d + r < r_older kills the new one, else d + r_older < r kills the older one, else each hides its columns inside the other, the new one first (0x8270F9..0x827104). Quirk kept: a circle killed later in the same ctor may already have hidden columns of others.
  - Point inside a circle: `fn_00827210`, dx² + dz² < r² (strict, 0x82723C).
  - The ripple: `fn_00827250` (new(0x68)), life 2000 ms at +0x0C (0x827273), colour at +0x34. 7 sprites from `LH3DSprite::Create(7, 1)` 0x8404A0 (stride 0x34), smoke material [0xEA1ABC], cell 63, size max(2i, 0.0001), angle `Random(0, 2π)` 0x81D180. Matrix RotX(π/2), then the yaw. The ripple list [0xEB9A44] is newest first (linked through +0x10).
  - The ripple's yaw is `GetYAngle(centre, point)` 0x841260 + π/2 ([0x8C78D8]). The inside point is the previous hand point if it is inside, otherwise the current one, both at y = 0; `fn_00827670` finds the edge between them; y = max(GetAltitude(point), hand.y).
  - Per frame, `fn_008274A0` on each ripple (from `fn_005E5CD0` 0x5E6264..0x5E628D): life −= g_game_time_inc; below 0, `fn_00827450` unlinks it and frees its sprites. The Z callback `fn_00827500`: every sprite grows by g_game_time_inc × 0.001 × 10, wraps past 14, alpha ftol((1 − s/14) × fade × 255), with fade = life × 0.001 under 1000 ms, otherwise 1. The sizes are kept, so the next draw grows them again.
  - **Quirk:** the ripple ctor never writes ripple +0..+8, so its Z-sorter key reads whatever the heap held there.
  - The hand-crossing statics [0xEB9A48] (inside on the previous frame), [0xEB9A68] (filled) and [0xEA9EF0] (previous point) are never cleared in the original; `InfluenceCircle::Reset` only empties the list.
  - (inferred) The dirty byte g_game +0x250174 is 0 after `GGame::ClearVariables` 0x54BF22, so a land's first circles appear only when a radius first moves by more than 0.01 from the 0 of Citadel +0x78 / Town +0xF24.
  - The latch is set from vt +0x200 of the citadel's 3D object at 0x462E33 / 0x462E4F.
- **Not ported:**
  - the virtual influence;
  - the allies (openblack has no alliances);
  - the multiplayer rule (without a citadel, 0);
  - the border in the temple's world room (`Draw(0)`, `WorldRoom::ShowInfluence`; openblack has no temple world room)
    and the per-land colour remap `GetRemapedPlayer` 0x64D790 (every openblack user of the player colours takes the
    identity);
  - `CalculateMostInfluentialPlayer` and its helpers 0x5CD4F0 / 0x5CD600 / 0x5CD6C0.
- **Inherited difference.** openblack creates the temple of `CREATE_PLANNED_CITADEL` already built. The original gives
  it the influence when Land1's script builds it, a few seconds in.

## Player alignment (`GAlignment`, GPlayer +0x60; `src/ECS/Effects/Alignment.*`, `components::Alignment`)

- Value from −1 (evil) to +1 (good) at +0x08 and a pending change at +0x0C. New game: 0 (`GGame::Init` 0x54FEA0
  takes the one from the profile, 0 without one). It lives with the player, not with the land (it is not cleared when
  loading a map): in openblack, one `components::Alignment` per `PlayerNames` outside the land's registry
  (`Magic/Core/Players`, `AlignmentOf`), the same one the miracles use with `GAlignment::Update` 0x414410.
- **Acts** (`GAlignment::Update` 0x4145A0 for trees): ±`GPlayerInfo::treePullPutAlignmentChange` (0.005), weighted by
  the current alignment (fn_00414660): towards where it already leans it counts `v·(1 − |a|/2)`, against it
  `v·(1 + |a|/2)`; it is added to the pending change. Uprooting with the hand (`Tree::InterfaceSetInMagicHand`) is
  evil; replanting (`Tree::EndPhysics`) and the tree that water plants (`Tree::ApplyWaterSpell`) are good.
- **Every turn** (`GPlayer::Process` → `ProcessForPlayer` 0x4141A0 → `Process` 0x414140; in openblack slot 3 of
  `Magic/MagicLoop.cpp`, `GPlayer::ProcessPlayers`): the pending change, clamped to −1..1,
  times `maxAlignmentChangePerGameTurn` (0.0019444 = 0.7 per game hour) is added (`CrudeUpdate`, clamped to −1..1) and
  the pending change goes back to 0. In other words, the pending change is a **fraction of the maximum rate** of that
  turn: an uprooted tree moves the alignment by about 10⁻⁵ (−0.005 × 0.0019444). That is what the code says; other acts
  (effects, miracles, deaths) contribute much more.
- Script: `GET_ALIGNMENT(player)` (`GScript::GetAlignment` 0x6F9A60) returns the value (`GPlayer::GetAlignmentValue` 0x64D6A0); `SET_ALIGNMENT(jugador, v)` **adds** v (`CrudeUpdate`, despite the
  name) and outside −1..1 gives the error "Alignment out of range" without doing anything (`GScript::SetAlignment`
  0x6F99C0).
- Not ported: the history (`CAlignmentHistory`, one global at 0xC4CD40; `AddTotal` 0x415480 and the `Add*` wrappers
  0x414D40..0x4153C0, e.g. `Add(GPlayer*, Tree*, float)` 0x415260). **Retail never reads it**: its only reader is the
  debug overlay 0x414840, which has no caller, and its node lists are never recorded (byte +0x85 is always 0). The
  advisors (`GGuidance::HelpSpritesAlignmentProcess` 0x71CEB0, ported as `audio::guidance::UpdateAlignmentRemarks`)
  read the raw per-turn change straight from `GAlignment::ProcessForPlayer` 0x4141A0, not the history.
  The **terrain** alignment (`MapCoords::GetAlignment`, the one for growth
  and the fields) is something else, from the influence of each cell, and is still not ported. Trace:
  `OPENBLACK_ALIGNMENT_TRACE=1`.
- `GPlayer::LoadPlayerAlignment` 0x64D355 reads the value saved in the registry, clamped to −1..1 (`GGame::Init` takes the profile's from esi+0xF4 → +0x1C).
- `GAlignment::CrudeSet` 0x4146F0 (+8 = the value clamped to −1..1); `CrudeUpdate` 0x4146B0 (+8 += change, clamped at once; used by SET_ALIGNMENT and the network packets).
- Trees: uprooting is called from `Tree::InterfaceSetInMagicHand` 0x74B730, planting from `Tree::EndPhysics` 0x74BBB6.
- **Deaths** (`GAlignment::Update(GPlayer*, Object*, DEATH_REASON)` 0x4143B0, from `Villager::VillagerDead` 0x750818 on the owner's GAlignment). The change is v = GPlayerInfo +0x20 + 4r (`dealthReason[r]`); a child (vt +0x458) doubles it (`fadd st0, st0`); an animal (vt +0x454) halves it (× 0.5). There is no ScaleChange and no clamp: the pending change += v (0x414400), and `ProcessForPlayer` folds it in.
- `GAlignmentInfo` at runtime is at 0xC4CE30 (stride 0x48), rows 0 burn .. 4 fly away. The column comes from `GObjectInfo::GetAlignmentType` 0x4012A0 (vt 0x34 of the info at +0x28).

### The player system's list of player entities

- The original has no list of player objects to keep: its players are the game's fixed table of `GPlayer`
  (`GGame::GetPlayer` by index, no range check), all there from the game's start. A lookup always gives the player as
  it is on the land being played.
- `TOGGLE_COMPUTER_PLAYER(player, on)` (feature-script case 77, 0x717842) creates nothing: unless the GSetup flag
  [0xC20D5C] is 0 (1 at start; named `CreateMultiplayerCreatures` in the symbols), it takes
  `GPlayer::GetPlayerFromText` 0x64B5E0 and calls fn_0064C400(on) on that player (0x717860): with a computer-player
  object at +0x944, its +0x1B8 = on, and with on != 0 the player's +0x8E0 = 2. Land 2 uses it for PLAYER_TWO and
  PLAYER_THREE, Land 3, Land 5 and comp.txt for PLAYER_TWO; Land 1 never does.
- openblack makes a player entity per land (LOAD_LANDSCAPE makes PLAYER_ONE, TOGGLE_COMPUTER_PLAYER the computer
  players) and `PlayerSystem` lists them by name (`AddPlayer`, `RegisterPlayers`, `GetPlayer`). Since 2026-10-08 the
  list is emptied on every land load (`magic::players::Reset`, before the registry is reset), and the computer players
  a script makes are listed as they are made. Before, the list was never emptied and only kept the first entity
  listed for a name, so from the second land on it named the last land's destroyed entities, and the script's
  computer players were missing. Nothing in the game reads the list yet, so neither change moves a measured check.

### Spell effects (`GAlignment::Update` 0x414410)

- `GAlignment::Update` 0x414410: nothing if the life did not change. `K = |Δlife| +
  GPlayerInfo.applyEffectAlignmentChangeAddition`: player 0 stores at +0x64 the pointer to `GPlayerInfo` 0xD47988,
  and +0x1C in memory is file +0x0C. For crush, hit, heal and push, `pending += f(v ×
  GAlignmentInfo[i][col] × K)`; for burn, the same with `ConvertTemperatureToDamage`. **fn_00414660 compares the sign
  of the change with that of A** (0 counts as positive): same sign `v(1 − |A|/2)`, opposite sign `v(1 + |A|/2)`
  (read again at 0x414660..0x4146AD: `je 0x414696` if A ≥ 0; in each branch `jne` if v < 0; corrected on
  2026-09-30, the first reading said it only looked at the sign of v).

### The sky alignment (`alignment::GetInterfaceAlignment`)

`fn_0064AC30`, once per turn at the end of `GPlayer::ProcessPlayers` (0x64A697; here in slot 3 of the turn, after
`alignment::ProcessPlayers`): the player with the most influence (`Influence::CalculateMostInfluentialPlayer` 0x5CD630:
the first, in player order, whose influence exceeds that of the previous ones and 0; if none, the neutral one) at the
interface's position, **GInterfaceStatus +0xB0 = the camera's position** (as shown by `UpdateSpellInfo` 0x5DC948,
which computes the camera's front as +0xBC − +0xB0), and `x = clamp((alineación + 1)/2, 0, 1)` is what
`fn_005E2240` receives. It starts at 0.5; `DoCitadelMultiplayer` fixes it at 0.5 (there is no multiplayer).
`Clouds::InfluentialPlayerAlignment` (map one) returns `2x − 1`, except with the `OPENBLACK_TEST_SKY_ALIGNMENT` hook
or the debug slider moved off 0.

## Reactions (`ECS/Effects/Reactions`)

- Reactions (`ECS/Effects/Reactions`, the only module, merged with the animals' one): `CreateReaction` 0x6E3D70 creates
  the 0x44-byte object (radius of the ctor 0x6E39D0: 1 if the reaction grows, otherwise `maxReactionDistance`) and
  spreads it once (`SpreadReaction` 0x6E3E10, the spiral from [animals.md](animals.md#reactions)): each living thing
  in the cell, in the cell's order, goes to the handler of its class (`SetLivingReactionHandler`: animals in
  `ECS/AnimalFlee.cpp`, villagers in `VillagerReactions.cpp`, which dispatches fire and teleport). What is common to the
  living is also there: the records (+0x98, `components::ReactionRecords`), the score fn_006E4620 and the switch rule.
  Before, fire always spread with `maxReactionDistance` (inf) and sorted the cell's villagers by entity; now it uses the
  ctor's radius (in info.dat REACT_TO_FIRE does not grow: 35 m, the same) and the cell's order, like the animals
  (approximate: that of openblack's grid, which is rebuilt once per turn and before an out-of-turn spread, not the
  original's lists). There is one clock, the game turn fixed at the start of the turn (`BeginTurn`, which also removes
  the reactions whose initiator no longer exists); the animals' records also use it.
- `SpreadReaction` 0x6E3E10 walks (max(1, trunc(radius × 0.2)))² × GetReactionPower cells. `GetReactionPower` (vt +0x4F4) is 1.0 in `GameThingWithPos` 0x4024D0; `Spell` 0x55CF10 overrides it with `GetSpellStrength` 0x720750, and `Tree` 0x55D8D0 with its life.
- The stealth branch of 0x6E3E10: when +0x20 is set and the cell's first object is another Living, `GameRand(1000) > stealthRandomChance` skips the cell; otherwise the stealth flag is cleared. No creator sets +0x20.
- In `ApplyReactionToLivingObjectsAtSquare`, the shield test (0x6E4031, `fn_0072B990`) comes after the class's vt +0x984 test and before the distance; it applies to villagers and animals alike.
- `Reaction::GetInfo` 0x6E4700 = 0xD4F6B0 + 100 × the type, + 0x24. The info's fields start at +0x10: the ctor 0x6E39D0 reads `whetherReactionGrows` at +0x34 and `maxReactionDistance` at +0x2C; `whetherReactionFinishesIfInitiatorInHand` is at +0x28.
- Turns to react: `Living::StandardNumGameTurnsToReactFunction` 0x5F18C0 = ftol((1 + 0.5 × (R − d)/R × howImportantIsDistance) × numGameTurnsForNormalThingsToReact), R = 10 + maxReactionDistance, each step in float, with `fimul` for the turns (info +0x14, 0xD4F6C4). `StandardNumGameTurnsBeforeReactingAgainFunction` 0x5F1920 is the same with numGameTurnsForNormalThingsBeforeReactingAgain (info +0x18, 0xD4F6C8).
- Field +0x04 of the reaction type table 0xC09CC0 + 0xC0 × t (read at 0x6E41BE): a Living may switch to another reaction of the same type. It is 1 only for 10 REACT_TO_FIRE, 28 and 35 (initialiser 0x6E0E80). (See Pending.)
- `fn_006E47C0(obj, v)` sets +0x30 = v (the "available" flag) on every reaction the object started (+0x14, list g_game +0x205BDC, next +0x40); `HandleApplyResult` calls it with 1 (0x5DA136). `fn_006E47F0` (from the put-in-hand `fn_005DC330`, 0x5DC38C) sets +0x30 = 0 on the object's reactions whose info asks `whetherReactionFinishesIfInitiatorInHand`.
- `fn_006E4830` moves a reaction to another initiator (from `FireEffect` `fn_00730960`: a tree's fire passes to its DeadTree). `Reaction::GetReactionInitiatedByObject` 0x6E4870 returns the first one's id. `fn_005F0FB0` returns a record's turn.

## Object life (`src/ECS/Life`)

- Life in 0..1 (Object+0x48). Villagers now store it as a float (`Villager::life`, before an integer percentage that
  lost small changes from fire or chanting); rocks and animals in `components::Life`.
- `Living::Living` 0x5EBEC0 starts with `SetLife(GLivingInfo::life)`; `Object::Object` 0x636520 with 1.0.
- `Object::ReduceLife` 0x637810: if the life is less than the amount, 0; otherwise life − amount. Returns the new one.
  Villagers and animals do not override it. It does not kill: the death states are not ported, so the physical impact
  (`HurtByImpact`) kills on reaching 0 as before.
- `Object::IncreaseLife` 0x637870 (and `Villager::IncreaseLife` 0x753460, which calls it): up to 1.
- `Villager::SetLife` 0x756B40 counts in the town (Town+0x714) the villagers below 0.7 life
  (fn_00756BC0 / fn_00756BD0); openblack's `Town` does not have that count yet. `Object::SetLife` 0x63A140 does not let
  objects with flag 0x40 (or 0x200 in a certain interface state) drop below 0.01: which ones is unverified, not ported.

## Fire (`src/ECS/Fire`)

Everything below was read in the exe (W120) except what is marked UNVERIFIED or "(inf)".

The fireball and the lightning, which are what start most fires, are in
[Fireball and lightning](miracles.md#fireball-and-lightning-magicobjectsmagicfireball-particlesrulesfireballlightning).

### The heat model (`FireEffect`, `SpreadEffect.cpp` 0x72E940-0x7310F0)

Every object hotter than the air carries a `FireEffect` (0x50 bytes, save type 0x29). There is a global list, newest
first, and `FireEffect::ProcessList` 0x730760 walks it once per turn (0.1 s; slot 6 of `GGame::ProcessTurn`).

- **Object values** (`GObjectInfo` +0xB0 heatCapacity, +0xB4 combustionTemperature, +0x80.. defence multipliers;
  `FireObjectTraits.cpp`): `Tc = max(combustionTemperature, 40)` (fn_00730180), `Tmax = 2·Tc` (fn_007301B0),
  capacity `max(heatCapacity, 1)` (fn_007301D0), ambient `MapCoords::GetTemperature` 0x605CC0 = **24.7 across the whole
  map** (fld 0x930080).
- It **burns** when `T >= Tc` (`IsOnFire` 0x730360); it reacts with `T >= 100` or `T >= Tc`
  (`IsAboveReactionTemperature` 0x730380). The fire fraction (0x7303E0) is `(T - 0.8·Tc)/(2·Tc - 0.8·Tc)` limited to
  `2·life` and to 0..1; the fire radius is `1.25 ·` the object's radius `·` the fraction (0x72FF10) and the flame height
  `1.25 · altura · (T - Tamb)/(2·Tc - Tamb)` (fn_0072FF70).
- **Per turn** (`fn_0072F5B0`, inside ProcessList):
  - in water it cools 50 times faster; with rain or snow the multiplier is `rainCoolingMultiplier·lluvia + 1`;
  - if it was given heat this turn: `T += 0.1·T/(2·Tc)` with a ceiling of `2·Tc`;
  - otherwise: `T -= (T + 10 - Tamb)·(4·altura·radio)·0.1·multiplicador/capacity` (the "area" 4·H·r);
  - while burning: damage `(T - Tc)/(2·Tc - Tc) · defenceMultiplierBurn · 0.1` to the life (0x72EEC0), and **charring**
    +0.04 per turn while life < 0.6, capped at `(0.6 - life)/0.6`; when cooling it drops 0.02 per turn;
  - on death: `DestroyedByEffect` (a creature is not destroyed);
  - **spreading**: spiral of 10 m cells while the cell is within `radio del fuego + 10 m` of the fire centre; each object in
    those cells receives heat (`fn_0072F980`). The wind (fn_00771B10) **is computed and discarded**: it does not move
    the search. An object in the hand only spreads inside the influence of whoever holds it (0x730860);
  - the reaction `REACT_TO_FIRE` (10) is created when passing the reaction temperature and deleted when dropping below
    it; in the hand it is `REACT_TO_BURNING_OBJECT_IN_HAND` (33), `FireEffect::StartedMoving` 0x730A60;
  - **groups**: each fire is born as the root of its group (+0x40 root, +0x44 next); `AddToMyFireGroup` 0x72FBE0 chains
    the new fire right behind the one that lit it, and a root that stops burning passes the group to the first member
    that is burning. The firefighters list (+0x48) is kept only by the root.
- `FireEffect` vtable 0x9996D4 ("SpreadEffect:"); the list is g_game +0x205C14. Fields: +0x20 the player responsible (`GetPlayer` 0x72EAB0), +0x30 the process tag copied from 0xDA09E5 at creation (processed while equal to 0xDA09E4; both 0 every turn), +0x40 root (`GetFirstCaused` 0x732AE0), +0x44 next (fn_00732AD0).
- `FireEffect::Create` fn_0072ECF0: no fire if the object refuses a burn (`IsEffectReceiver` of the static EffectValues 0xDA0980 = BURN 100), cannot be set on fire (+0x0A bit 3), has Tc 0 or is being deleted. T starts at the object's temperature; the fire goes to the head of the list as the root of its own group, gets its graphic (`CreateSprites` 0x730AD0 → fn_00731160) and the object `StartOnFire`.
- Object entry points: `Object::GetTemperature` 0x639A10 (the fire's T, else ambient), `Object::IsOnFire` 0x637CC0; `FireEffect::SetTemperature` 0x72EF10 (`Object::SetTemperature` 0x639A60: a new fire when hotter than the object, then T = t; an existing fire just takes t, also lower); `FireEffect::SetOnFire` 0x72EF60 (`Object::SetOnFire` 0x639A40: T = 2·Tc·speed + Tc).
- `FireEffect::ApplyEffectToFireEffectIfNecessary` 0x730670 (the start of `Object::GetDamageEffect` 0x637D00): a burn drives T towards ambient + burn with T += min(10·dT / capacity, dT); a negative one (water, beating) cools it; a villager not yet on fire runs (`SetupOnFire`).
- Helpers: ambient fn_007301C0; `GetMaxFireRadius` 0x730000 = 1.25 × the fire radius; `GetSafeFireRadius` 0x730020 = min(fire radius, max) + 1; heat content fn_00730600 = (T − Tamb) × capacity; fn_00730630: T += q / capacity, at most dTmax by magnitude; fn_0072F960 = 0.1 × 100 × dT.
- Object values: `Object::GetCombustionTemperature` 0x639A30 (info +0xB4), `Object::GetHeatCapacity` 0x637CE0 (info +0xB0, vt 0x5E4), defence multiplier burn info +0x90 (fn_00730230). `MagicFireBall::GetHeatCapacity` 0x682D40 = r² × 0.0625 × info.heatCapacity × the effect's strength (manager +0x54).
- Rain cooling multiplier (vt 0x5EC): 0.01 for an Object (0x639AB0); `MagicFireBall` 0x682DB0 gives +0x58 ? 0.01 : 0 (0 when the script cast it).
- Fire centre (vt 0x5F0): the object's position (0x639AA0); `DeadTree` 0x510CE0: the world matrix × the mesh centre (L3D +0x18) in x, z, with its own height above the land (+0x1C) in y.
- Burning damage constants: 0.1 [0x999630] (0x72F418), 0.6 [0x999658] (0x72F428), +0.04 [0x999654] (0x72F435), 1 / 0.6 [0xDA09C0] (made at 0x72EA16, used at 0x72F467); charring −0.02 [0x999650] (0x72F558).
- On death the fire calls vt 0x5F8 `DestroyedByEffect` with the fire's `GetPlayer` (0x72F509) and 0 (0x72F506 push ebp, ebp = 0 since 0x72F216): not the burn damage.
- The spread visits every object of each spiral cell except the fire's own; there is no "done" set, so an object in several cells of the spiral is heated once per cell. A cell's objects: the fixed list, then the mobile one only when the last fixed object's type counts as fixed (`MapCell::FindTypeOnMap` 0x6015E0, 0x601621); the held object is off the map.
- An object in the hand: `Object +0x24` bit 2 (`GetPlayerHoldingThis` 0x63A190); it spreads only where `CalculatePlayerInfluence` 0x5CD170 (with 1, 0, 0) of the holder is > 0 (0x72F61C). `FireEffect::CheckToSeeIfObjectIsNearOnFireObject` 0x730860 (from `Object::ProcessInHand` 0x639AD0): every fire in the map cell of the held object's fire centre heats it (0x7308B8 / 0x7308DB).
- Groups: `IsInSameFireGroupAs` 0x72FEA0, `RemoveFromFireGroup` 0x72FDD0, fn_0072FD20 (a root hands the group to a member, which goes first), the firemen list of the old root appended to the new one (0x72FC30 / 0x72FD36), fn_0072F910 / fn_0072F930 (the group's firemen stop: all, or those fighting this fire).
- `CopyFire` fn_007308F0 (`Rock::SplitInTwo`): the new object's fire is in the same group, T = Tprev = the max of both. `MoveFire`: the reaction's initiator becomes the new object (fn_006E4830). `SetOutMagicHand` 0x730AB0; clear fn_0072ED80.
- `ReduceLifeDueToBurning` (vt 0x5C4): Object 0x637C20 = vt +0x5B8 ReduceLife unless +0x0A bit 2 (`Abode::ReduceLife` 0x405D90 for abodes, `Object::ReduceLife` 0x637810 for the rest); Field 0x52A050 = RemoveFood(damage × GFieldTypeInfo +0x130 totalFoodInField) and returns 1 (the 6 rows are identical). Not ported: the town's aggressor (`Town::UpdateAggressor` with EffectValues(BURN, T, 0, 1, player)).
- `StartOnFire` (vt 0x6BC): `MultiMapFixed` 0x52EC60 (also Rock, MobileStatic, AnimatedStatic, Fragment, Field, FishFarm, BigForest, the citadel and worship classes, spell icons) and `DeadTree` 0x510E20 drop their reactions, `Pot` 0x66D6C0 its own (RemoveReaction); Tree and the living do not override it. `EndOnFire` (vt 0x6C0): `DeadTree` 0x510E60 creates REACT_TO_WOOD (0xC) with its GetPlayer (vt 0x1C) and 0 while available; `MagicTree` 0x5FD0E0 creates REACT_TO_MAGIC_TREE again (no availability test); `Pot::EndOnFire` 0x66D6D0 re-creates its reaction (fn_0066D660, info +0x128; not ported).
- `Field::Field` 0x527DD0 hands its GAbodeInfo to `Abode::Abode` (0x527DFA), which keeps it as the object's info.
- Not ported: a creature gets 3 flames (`Creature3D::AddFlame` fn_00486390, random points in its box).
- **Sound** (`FireSound.cpp`): only the **2** fires closest to the camera with fraction > 0.1 make sound (2-slot table
  0xDA09CC), a looping `G_Fire` on the object.
  - The farthest slot's distance is 0xDA09C8. At the end of fn_0072EFB0 (0x72F7C6..0x72F875) a fire nearer than the farthest slot (or with no slot yet) takes the first slot that is empty or not nearer, then the farthest distance is recomputed; a fire already in a slot can take the other one too. The original also stops the slot's sound when it is the same fire (fn_0072EDC0 at 0x72F82C); openblack keeps it playing (approximate). `ProcessList` 0x730823..0x73083E calls fn_0072EDE0 for each slot every turn.
- **Visual** (`FireGraphic.cpp`, `PSysBase` 0xD0, fn_00731160/1560/2200): `S_Fire.raw` flames in mode 13 orange
  0xFF713C with cell `int(fmod(-25·age, 32) + 32)`, additive white steam and grey smoke `S_SpriteSheet3` (cell
  `int(fmod(25·age, 32))`) in bursts of 30 turns; tint of the burning tree (fn_0074B3A0: grey 50, or
  `max(50, 255 − (1 − life)·2550)` with life > 0.9, **capped unsigned at the frame's tree brightness
  [0xC22FA0]** (0x74B47B, `ecs::TreeBrightness`, the same one that multiplies a tree without fire at 0x74B077; before,
  the port used 255, and at night the burnt tree came out lighter), grey of the charring (fn_00730570:
  `k = ftol(c·255) & 0xFF` and each channel `((unsigned)(−175k) >> 8) − 1` (0x730585..0x7305D7) =
  **`255 − ceil(175k/256)`**: 255 with k = 0, **80** with k = 255; before, the port truncated and gave 81; `test_fire`
  compares it with the exe's integer code for all 256 k) and brightness `GetFireEffectCharingColor` 0x730480. The light
  map `S_LMFireBall` of the burning object (bit 4 of +0xB5) **only exists under the
  MultiMapFixed objects**: the flag `Object +0x24 & 2` (0x7312D5) is set only by the `MultiMapFixed` ctor (0x52E207:
  houses, BigForest, Feature...), not by a standalone tree.
  - **Pending (render)**: while it draws the burning tree, 0x74B4D6..0x74B51E set `OverrideMaterial` [0xECA658] = 1
    and `OverrideRenderMode` [0xECA65C] = `ftol(min(254, 230 + calor·25/255))` (heat 255 if T > 1.5·Tc, otherwise
    `ftol((T − Tc)·255/(0.5·Tc))`; cap 254 [0x99A17C]), and remove them after `AddForDrawing` (0x74B5D8). It is not a
    glow material: the mode functions 0x82E080.. read it as the **ALPHAREF** of the alpha-tested primitives
    (`render_modes::AlphaRef` `forced`), so the burning tree's foliage gets cut out (only the almost opaque texels pass).
    Not ported: the trees are instanced and `fs_object` takes the ALPHAREF per draw (`u_skyAlphaThreshold.y`), so a
    separate draw is needed for each burning tree (`FireGraphic.cpp`, TODO).
  - FireGraphic vtable 0x9997D4, made only for an object with a 3D object; `~FireGraphic` 0x7313C0; fn_00731560 updates it with the frame time (g_game_time_inc × 0.001). The original updates it only when drawn (`FireEffect::Draw` 0x730330) with a catch-up after 10 turns; openblack updates every fire (approximate).
  - `SpritePos` 0x48 bytes (vtable 0x9998D4); flames: life 4.3 s [0x999664], fade-in 1 s [0x999660], scale (fraction + 1) × 0.5 × localScale (fn_007317F0 0x7319D0), alpha clamp(…, 0, 1) × 250 [0x8C7B2C] (0x731A34).
  - Steam and smoke puffs: life 3 s [0x999680 / 0x99969C], scale ((2.6 − 0.2)·t + 0.2) × baseScale (steam [0x999688] / [0x999684], smoke [0x9996A8] / [0x9996A4]), pulled by the wind (wind × 0.5 − v) × 0.1 × dt (fn_00731AB0, [0x999674] / [0x999678]); emitted 4 per second (steam [0x999670], smoke [0x9996A0]); steam alpha 100 [0x999690] and rise 1 [0x99967C], smoke alpha 180 [0x9996B0] and rise 2 [0x9996B8]; the burst constant 30 turns is [0x999694 / 0x9996B4]. Steam only while Cooling and T > 75 [0x999638] (0x731AD9). The smoke is grey with or 0x707070 (0x732593).
  - Materials: 0xDA09EC (S_Fire, mode 13), 0xDA09F0 (S_SpriteSheet3, 13), 0xDA09F4 (S_SpriteSheet3, 6); the shared LH3DSprite 0xDA09E8 has size 1 and height 2 (0x73130F / 0x731316). The cell is the sprite flags' low 6 bits (0x7323B7..0x7323CA) on 8 cells a row (LH3DSprite +0x30 = 8 from SetToZero 0x8404F0; inferred: S_Fire is 8 × 8 like S_SpriteSheet3).
  - The steam and smoke never write the shared sprite's +0x18 / +0x1C (fn_007323F0 / fn_007324E0), so they keep the origin −2 × size of the last flame drawn with it (fn_00732200 draws a fire's flames newest first, then its steam, then its smoke; a fire with no flame takes the last flame of the fire drawn before it): in the original they sit 2 × that size higher on the screen. openblack draws them centred (approximate).
  - `GetPSysFireMaxFlames` 0x732A30: trees 2; otherwise max(2D radius, height) < 3 ? 2 : 7. `GetPSysFireLocalFlameScale` 0x732950: trees 0.2 × height, the rest 0.5 × height (impressive objects and citadel parts 0.3, not ported).
  - Flame positions: `Object::GetPSysFireLocalRndFlamePos` 0x732770 (fn_00590460) a random point of a random triangle of the mesh in mesh coordinates, trees × 0.5 (inferred: the sampling steps); `GetPSysFireWorldFlamePos` 0x732660 = the object's matrix × that point; `GetPSysFireFlameMatrix` 0x732630 copies the object's G3D matrix (vt 0x570), whose translation goes to +0x98 (0x7316FD..0x731713).
  - With flag bit 0 (+0xB5 & 1) the flames follow the land like the morphed object: H0 at +0x98 / +0xA0, y = (H(flame) − H0) + y (0x73230A..0x732366).
  - The light map `S_LMFireBall` (pitch 6, 3 bytes, 1 frame, loaded once into 0xDA0960, +0x30) is given only when the object is a MultiMapFixed and its Get2DRadius > 2 (0x7312B6..0x7312EF). With flag bit 4 its alpha = 0.6 [0x8C7BDC] × GetFireFraction × (1 + 0.2 [0x99964C] × VLNoise(0.6 [0x999644] × (turn + fraction) + (this & 0xFFFF))) × (1 − charring +0x34), only above 0, at most 1 (0x731633..0x7317B2); openblack uses two sines for VLNoise 0x590C30, leaves out the turn fraction and uses the fire's id for the pointer (approximate).
- **Villagers** (`VillagerFire.cpp`, `VillagerFireman.cpp` 0x75A3D0-0x75B460 and `ReactToFire` 0x765870): states 215
  `REACT_TO_FIRE`, 216 `PUT_OUT_FIRE_BY_BEATING`, 219 `ON_FIRE` and 220 `MOVE_AROUND_FIRE`. The water ones (217, 218)
  **in W120 give up on the spot** (`DECIDE_WHAT_TO_DO`), so nobody carries water. A villager who is putting out a fire
  receives no heat (fn_0072F980). `SetupOnFire` 0x75B170 stores the previous state and destination and switches to
  `ON_FIRE` with the fire that heats it. The decision at 0x765870 is read in `ReactToFire`: the villager looks
  for the fire of the group closest to it that is above the reaction temperature (`fn_00730070`).
  - Firefighter's spot (`GetFireFightingPos` 0x75AA90): on the line from the fire to the villager, at `max(radio seguro, radio
    del objeto)` (0x75AAF2..0x75AB16; before, the port took the minimum, and the villager went back and forth
    216 ⇄ 220 every turn) + the villager's radius (0x75AB23) + `GameFloatRand(1)`. The arrival of `MOVE_AROUND_FIRE` and
    of `GO_TOWARDS_TELEPORT` is `MobileWallHug::AreWeThere` 0x60AD60: strict `d² < (paso +0x5A + extra)²`, the step of
    `RebuildMoveByStep` 0x609D10 = `WallHug::speed` (before, 1 m). Checked: Land1,
    `OPENBLACK_TEST_FIRE="1785.2.2652.6.450,abode,20"`, `OPENBLACK_VILLAGER_TRACE=1`:
    13 changes 216 → 220 and 25 220 → 216 over the whole life of the fire (before, 3213 in 650 turns), each villager
    dozens of turns in each state.
  - `Villager::ReactionValidate` 0x756A00 (`villager_reactions::ReactionValidate`): the "validate" column (+0x80) of the
    state table 0xD09198 in the reaction rows (201, 202, 251, 215-218, 220, 6-30, 140-146), which
    `Villager::ProcessState` 0x74FF91/0x74FFD9 runs every turn for the top state (+0x8C) and the saved one (+0x8D)
    before the state: `PopFromPrevious` 0x751E50 if the reaction's object (+0xBC) does not exist or is not available
    (`GameThing::IsAvailable` 0x401810, vt 0x2C), or if the `ReactionInfo` row (0xD4F6B0, `Reaction::GetInfo`
    0x6E4709) asks for `whetherReactionFinishesIfInitiatorInHand` (+0x28) and the object is in the hand (+0x24 & 4).
    `ReactToFire` 0x765870 and `GoToTeleportReaction` 0x7662F0 check nothing more (the first only returns 0 if the
    object is not an `Object` or has no fire, without changing state). Wired up:
    `LivingActionSystem::VillagerCallValidate` calls it on every row without its own validate whose original validate
    is 0x756A00 (`VillagerOriginalFns.h`); the custom (inferred) exits of `ReactToFire` and `GoToTeleportReaction` are
    gone (details in [villagers.md](villagers.md)).
  - **What takes the villager out of 215 when the object stops burning** (2026-10-02): it is not the state. The fire
    removes its `REACT_TO_FIRE` when dropping below the reaction temperature (fn_0072EFB0 0x72F781), when being deleted
    (`FireEffect::ToBeDeleted` 0x72EC4C) or when moving, with `RemoveAllReactionsOfTypeInitiatedByObject` 0x6E4780,
    which calls `Reaction::ShutDown` 0x6E4720 on each one: +0x34 = 1 and, while there is any follower left (+0x1C),
    `StopReactingAndSetState` (vt +0x99C, 0x5F11C0: `ResetStateAfterReacting` 0x751E10 = `PopFromPrevious` and
    `DECIDE_WHAT_TO_DO` if the final state is a reaction one; then `StopReacting`) of the first in list +0x18
    (0x6E4731..0x6E4743). That way the villager goes back to what it was doing in the same turn, whether it is in 215,
    fleeing towards 215 or putting out the fire. Ported: `villager_fire::ShutDownReaction`, called by `RemoveReactions`
    in `FireEffect.cpp` before removing the reaction (the order of the followers is (inferred): by entity). For a
    `REACT_TO_FIRE` removed some other way (`Pot::RemoveReaction` 0x66D6A0 removes all those of an object; openblack's
    reactions do not keep the list of followers), `ReactToFire` does the same `StopReactingAndSetState` when it sees
    that its reaction is gone (approximate: one turn later). Still not ported: `Living::ProcessReaction` 0x5F1270
    (every turn: reaction not available → `StopReacting`; object +0xBC null or not available, or past the turns of table
    0xC09CF0 for its type → `StopReactingAndSetState`), `TODO` in `VillagerCore.cpp`.

### CHL natives (`Magic/Script/CHLFire.cpp`)

170 `IS_ON_FIRE` 0x6FB4C0, 171 `IS_FIRE_NEAR` 0x6F7910 (`FindNearForScript` with the predicate 0x6F7100; a fireball is
not in the cells, so it does not find it), 174 `SET_TEMPERATURE` 0x6FB840, 175 `SET_ON_FIRE` 0x6FB780, 321
`SET_HURT_BY_FIRE` 0x6FDF40 and 426 `SET_SET_ON_FIRE` 0x6FDEE0 (the last two, bits 2 and 3 of Object +0x0A).

- `SET_TEMPERATURE` → `Object::SetTemperature` 0x639A60 (no source) → 0x72EF10.

## Time and weather

It is in [day-night-weather.md](day-night-weather.md#weather-and-climate-srcecsweather) (LH3DAtmos, GClimate, storms, rain and `Weather.h` queries).

## Miracles one by one

Each miracle has its own section in [miracles.md](miracles.md):

- [Food and wood](miracles.md#food-and-wood-magicspellsspellresource-magicobjectsmagicfoodwood-ecspotresource)
- [Water](miracles.md#water-magicspellsspellwater-particlescreatorsmist)
- [Heal](miracles.md#heal-magicspellsspellhealcpp-particlesruleshealcpp)
- [Forest](miracles.md#forest-magicspellsspellforest-magicobjectsmagictree-ecstrees)
- [Flocks](miracles.md#flocks-magicspellsspellflock-particlesrulesflockcpp)
- [Fireball and lightning](miracles.md#fireball-and-lightning-magicobjectsmagicfireball-particlesrulesfireballlightning)
- [Shields](miracles.md#shields-magicspellsspellshield-magicobjectsmapshield-particlesrulesshield)
- [Teleport](miracles.md#teleport-srcmagicobjectsmagicteleport-srcecssystemsimplementationsvillagerteleport)
- [Storm, lightning storm and tornado](miracles.md#storm-electric-storm-and-tornado-magicspellsspellstormandtornado-particlesrulesstorm-ecsweatherlightningflashstormclouds)
- [Lightning explosion and missing PSys classes](miracles.md#lightning-explosion-and-missing-psys-classes-particlesrulesexplosionkeypointsorientforestcpp)
- [Creature miracles](miracles.md#creature-miracles-pending)

The particle engine (particle types, class registry, creators, sound and rule index) is in
[particles.md](particles.md).

## Review of the whole chains

Checked against the executable and with the complete chains in the game (hand casting, food and wood, fire, weather and worship together).

### Formulas re-read in the exe (they match)

- Gestures: `MatchForward` 0x57A1A0 (turns aligned from any starting point, absorption of small corners, error wrapped
  with fn_0057A150, the two constants of `crt_xc` 0x579DC0/0x579DF0 = 3π/32 × 7/4 and × 2).
- Casting: `SpellSeed::ApplyThisToMapCoord` 0x728E20, `Cast` 0x729520, `DoPreCastThings` 0x729460 (the branch "type 2
  seed → magnitude 1" looks at `GMagicInfo +0x28`, −1 in every row of the file but filled at load: live for the
  fireball and its power-ups) and `SendApplyToMapCoord` 0x5D3340.
- `Pot::AddResourceToPos` 0x66F270: the 9-cell spiral, first list +4 and then +0, `IsCloseToEqual` with
  `Get2DRadius × GetRadiusMultiplierForApplyingPotToPos`, poisoned in arg5 and acceleration in arg6.
- Heat: fn_0072F980 (the firefighter immune, radius, the height check only if either of the two is 3 m or more above
  the ground, `min(10·ΔT, 0.5·calor de la fuente)`, the source loses heat if it is not burning, the group,
  `SetupOnFire` if the villager is not in state 219).
- `UpdateRuleGravityWithFloor` 0x6A1880 (gravity in the air `clamp(v.y + MaxSpeed, 0, 1) × g × gravedad del átomo
  × dt` and the return to the ground).
- `GWeather::CalcAtmos` 0x8400E0 (box, radius², falloff between the two radii, `ftol(f × fundido × 256)`, temperature with
  a wrapping byte add and the other five bytes with saturation).
- The worship site's battery (fn_0077B6A0: intensity `used/capacity + max(0.2, 0.5 − battery/max × 0.5)` up to
  1, `battery − (used − produced)` with no upper cap, available = battery + capacity) and the tap on an icon
  (`SpellIcon::InterfaceTap` 0x726430 → `ActualInterfaceTap` 0x77F880).

### Fixed in the review

- `Pot::AddResourceToPos` returns `amount − what was left` on every path (0x66F511), also when it makes a new pile;
  before it returned the whole amount. Only the hand's log used it.
- `MapCoords::IsWater` 0x6035B0 answers **1** outside the map and where there is no land block (0x603617); the copy in
  `ECS/PotResource.cpp` answered 0, so food or wood dropped on open sea without a block made a pile.
- The one-off seed is tied to the player's best icon (`CreateSpellIntoHand` 0x72A730 → fn_007282A0), as above.
- The one-off orb has its `SpellSeedGraphic` inside (0x72A450) and is deleted with it.

### The size of the fireball cast with the hand

`Spell::InitWithPos` 0x71FE50 gives the PSys the magnitude `SpellCastData[0]` without checking whether it is 0
(`PSysInterface::Create` 0x68E910 → `GJPSysInterface::Create` 0x68F3DA stores it in the manager +0xA0, which
`MagnitudeFloatProvider` 0x69DA90 reads). In `SpellSeed::Cast` 0x729520 that value comes from the gesture packet (+0x14,
fn_0071FA10), which is `GInterface` +0x1B8 copied whole into packet 0x12 (`SendApplyToMapCoord` 0x5D362D → fn_00550E90 →
format 15 of `SendPacketCompressed`, a 0x18-byte block, not quantised) and which **only the circle writes** (0x5CF57A
and 0x5D33BA; the `GInterface` is born zeroed). But right afterwards, `SpellSeed::DoPreCastThings` 0x729460 does
`if (magicInfo.spellSeedType == FIRE) castData.magnitude = 1.0` (0x729502..0x72950B): the programmers fixed the
fireball at magnitude 1 whatever the gesture. info.dat leaves `GMagicInfo` +0x28 (`spellSeedType`) at −1 in every
row, but the game fills it at load with the first seed of each magic type (`fn_0042B400`, see
[Dispensers and fireflies](#dispensers-and-fireflies-worshipspelldispensercpp-worshipfireflyrewardcpp)), so the branch
is live for FIREBALL, FIRE_PU1 and FIRE_PU2 (their first seed is FIRE, and only the FIRE seed has them). Without the
branch the fireball would come out with the size of the last circle, or 0 → 0.01 (4 cm, it barely heats) if one was
never drawn; the user remembers (2026-10-01) that a fireball cast from the hand **always came out big**, with any
gesture, which the fill explains. openblack: `PrepareCast` (`SpellSeed.cpp`, was `DoPreCastThings`) tests
`GetSpellSeedOfMagicInfo` of the magic cast. From 2026-10-01 to 2026-10-08 it tested the seed's own type when the row
left the field at −1 (inferred then); with the vanilla data the two give the same seeds. Result: atom scale 1 × 4.0168 of the root sprite, the fireball is
visible in flight and sets fire to the house and the tree where it lands. With `SPELL_AT_POS` the magnitude is still
the script's radius (10 in the test),
because it does not go through the seed.

### Chains tested in the game

- Land1, dispenser → orb → seed → cast: `OPENBLACK_TEST_DISPENSER="NORSE_ABODE_SPELL_DISPENSER,1812.2652.1"`,
  `OPENBLACK_TEST_TAP="1812.2652.200"`, `OPENBLACK_TEST_CAST="press@30,release@31,shot@33"`,
  `OPENBLACK_TEST_THROW_VEL`: the orb gives the FIRE seed ready (3500 chants), it arms (state 8) and on release the
  spell comes out with its `MagicFireBall` (T 6000). With the earlier 0.01 fireball the barn did not catch fire; with
  the FIRE seed's magnitude 1 the house, a tree and the villagers next to it burn (with `OPENBLACK_CAMERA_FLY=1800.75.2600.1826.30.2641`, `OPENBLACK_MOUSE_AT=0.5.0.55` and
  `OPENBLACK_TEST_THROW_VEL=0.2,6`).
- Land1, food next to the store: the same dispenser with `FOOD`: inside the store's radius (18.5 m) everything goes into
  it; a little further away it makes a `MagicFood` of 200 that
  grows 18 per grain, with the hand raised 16 m and the 4 s stream.
- Land2, icon → charge → seed → cast: `OPENBLACK_TEST_WORSHIP_SITE="NORSE,FIRE,HEAL,FOOD,WOOD"`,
  `OPENBLACK_TEST_TOWN_SPELL="0,FIRE"`, `OPENBLACK_TEST_MANA=20000`, `OPENBLACK_TEST_TAP_ICON="FIRE,200"` and
  `OPENBLACK_TEST_CAST`: `seed 3120 of icon 3082 in the hand with 3500 chants`, armed and cast
  . With 3000 chants the icon stays charging with the battery at 0. A one-off orb tapped with chants at the site comes
  out tied to icon 3081.

## Audited assumptions (2026-10-01)

Audit of every assumption the magic code adds: 245 findings, 41 corrected to match the original, 52 with the source
added, 139 marked in the code and 13 unchanged (already faithful or from another part of the code). What remains
marked, by topic:

- **Corrected to match the original:**
  - Spells: a spell without a PSys is cast anyway and ends on the next turn (0x71FE50, step 8).
  - Seeds and casters: `ProcessSpellSeed` always returns 1 (0x721370); a creator without an object is not functional
    (0x405240); the miracle selection sets the power-up gesture to 0 (0x5CF010).
  - Hand: the glow frame is rounded (`fistp` 0x68D323, not `__ftol`) and the wrap is "> 64" (0x68D0C0).
  - Teleport: the SPOT_VISUAL 14 flashes last as long as their entry.
  - Script player: the byte g_game+0x205A5B is the slot of the **neutral player** (7; GGame::SetupPlayers 0x550458,
    GPlayer::IsNeutral 0x64AC00). That is why the script's player 0 and an ownerless magic pile are neutral.
  - Fireball: the fireball bounces off the shields (DoAnyShieldDeflections 0x6A1FA0 from GravityWithFloor 0x6A1F48);
    the non-human cast is resolved again if v² > **0.01** (the double [0x8C7620] of `fcomp qword` at 0x69EC60; read as
    a float it looked like 89129, corrected on 2026-10-02) and the climb exceeds 30° (the double [0x9375F0] =
    0.52370351552963257, `fptan` 0x69EC77).
  - Lightning: the modes go in order (hand, manager, parent; 0x690F88) and the parent's uses a circle, without a cone;
    the forks are only updated with the effect active.
  - UR_WillowWisp: the age of each atom is fraction·dt (0x6A70CC).
  - Fire:
    - The fire reaction does not come out either in the hand or in flight (+0x24 & 0x44, 0x72F729).
    - A burnt Feature is deleted (0x6378E0); a burnt field sets T = 0 and deletes its fire (0x52A010).
    - StartOnFire includes field, mobile static, animated static and fragment (0x52EC60).
    - Bit 0 of the fire graphic is IsMorphWithLand.
    - The rain is cut off only below 0 (fn_008341B0).
  - Villagers:
    - The exit functions receive the next state (ExitPutOutFire 0x752530, ExitReaction 0x7527A0,
      ExitMoveToWorshipSite, ExitAtWorshipSite 0x76C1F0).
    - The shield blocks the fire reaction (fn_0072B990).
    - A villager going to worship does not fight the fire (0x765A6A).
    - Worshippers are no longer counted twice: the worship site keeps the list of its villagers (+0xD4, fn_0077D040).
    - `SetupMoveToWithHug` (0x5F2890) sets TOP and then FINAL (0x752440) in a single shared function
      (`VillagerMove.cpp`).
  - Worship: the seed goes into the hand only if `InterfaceSetInMagicHand` returns 1 (0x5DA77C); the citadel
    influences with factor 1 (0x463240).
- **(approximate):**
  - Reactions: openblack's cell grid and its order; `InBounds` uses the land's extent, not
    MapCoords::InBounds 0x6042C0.
  - EffectValues: Object's `ReduceLife` for all classes.
  - Lightning: the second fork instead of the tree fn_00691F30.
  - Light maps: alpha = RGB maximum, without the ×190 level.
  - Fire graphic: the charring noise is made of two sines (not VLNoise 0x590C30); all fires are updated.
  - Random numbers: storms, rain, fire and the miracles now go through `game_random` (GRand, the PSys and the
    original's CRT); the fireflies and other systems still use openblack's.
  - Villagers: MOVE_AROUND_FIRE goes straight (GetViaPoint 0x75A440 not ported); the decision to fight the fire
    (0x765870: formula read, without a random term) takes fn_00730290 / fn_007302E0 untraced; FLYING / LANDED are not
    run when landing after a teleport.
  - Gestures: frames with the mouse act as mouse messages.
  - Worship: the count of those going back home (vt 0x8C8 unidentified).
- **(inferred):**
  - Reactions: GetReactionPower = 1 for all (Spell 0x55CF10 and Tree 0x55D8D0 not ported).
  - EffectValues: one hit per object in ApplyEffectToMapPos 0x525100.
  - Seeds: the right hand for the one-off seed; the local player to check the influence of what is carried in the
    hand.
  - Default values of the PSys rules whose ctor was not read (Gravity 10, turns, trail, mesh, gesture 5 s,
    chain of 0.5·scale).
  - Shield: the default castData (40).
  - Worship: the 6 m dance ring and the fallback icon point.
  - Many of openblack's defensive values: out-of-range indices, caps, 0.0001.
- **Pending (TODO with an address in the code):**
  - Lightning: the single-target branch (0x691CF5), the cut-off by shield fn_006D0BC0 and the per-state sound.
  - Reactions: the stealthy branch of 0x6E3E10.
  - Creature: death counter and alignment (+0x11C0, +0x168).
  - Seeds: the MagicFireBall target (0x728A20).
  - Worship: the path along the footpath (58).
  - Fire: the route around the fire.
  - Spell class not ported (creature), which runs as a plain Spell (storm, water and flocks already have theirs).

### Second audit (water, flocks, storm, lightning explosion)

25 findings: 4 corrected, 1 comment, 7
marked, 11 checked against the disassembly and 2 unchanged.

- **Corrected to match the original:**
  - Flocks: at the end of `SpellFlock::Process` the leader's position goes to the **flock** (+0x14, the centre of the
    domain), not to the spell (0x7234F2..0x723519).
  - Tornado: the pick-up spiral is GUtils::Spiral 0x74D7E0 started with direction 1 (0x6D22E9); the port walked the
    mirror-image one.
  - Storm: the fire-extinguishing reaction is forgotten when it is not available (vt 0x2C, 0x72DBA2), not only when it
    disappears.
  - Loop: fn_0064AC30 (sky alignment) goes after the teleport travellers, at the end of
    GPlayer::ProcessPlayers (0x64A697).
- **(approximate):** the subcollection added mid-step is updated in that step (PSys.cpp); `MoveToBaseGroup` without a
  root collection deletes the atom; the tornado's per-cell vessels come from the registry; the base colour of the mists
  is that of the previous frame; the output of `UR_ForestPath` without keys is 0.
- **(inferred):** the +0x80 of `EventConditionAtomNearVillagers` in metres; the wolf leader's destination before its
  first turn.
- **Unchanged:** `SetDeathCallback` for animals has a single slot (only the flocks use it).

## Test hooks

All the `OPENBLACK_*` are in [openblack-internals.md](openblack-internals.md#debug-environment-variables).
By topic:

- Spell core: [Hooks and traces](#hooks-and-traces)
- Casting from the hand, gestures and hand effects: [Hooks, tests and captures](#hooks-tests-and-captures)
- Worship: where miracles come from: [Checks in the game](#checks-in-the-game)

## Pending

What is missing is in each topic, at the end of its section:

- Worship: where miracles come from: [Differences from the original and what is missing](#differences-from-the-original-and-what-is-missing)
- The temple's outside: the effect of a blend (`fn_004630E0` / `fn_00454AA0`) and `SetAlignmentFlock` 0x465270 are not ported. Open: whether `DrawPartialyBuilt` 0x816AD0 also adds the `UpdateMelting` deltas while the temple is built, which would count the land offset twice over the baked mesh; the player's alignment on the first turn of each land ([The temple's outside](magic.md#the-temples-outside-alignment-and-size)).
- Influence: the virtual influence, the allies, the multiplayer rule, the border in the temple's world room, the colour remap and `CalculateMostInfluentialPlayer` ([Influence](magic.md#influence-srcecsinfluence)).
- Casting from the hand: the help, the immersion, the HUD gesture icons, the hand glow and feeding a fireball in flight ([Casting from the hand, gestures and hand effects](magic.md#casting-from-the-hand-gestures-and-hand-effects-srcmagicgestures-srcmagichand-handspellseedcpp)).
- Alignment: the terrain alignment; the history (`CAlignmentHistory`) only feeds a dead debug overlay in retail ([Player alignment](magic.md#player-alignment-galignment-gplayer-0x60-srcecseffectsalignment-componentsalignment)).
- Life: the town's count of injured villagers (Town+0x714) and the 0x40 flag of `Object::SetLife` 0x63A140 ([Object life](magic.md#object-life-srcecslife)).
- Dispenser orb: the user accepts the size of the seeds and the height of the bubble (2026-10-01). The reference capture of the original is a WATER orb, not a fire one: its sky-blue blotch is the effect of the water seed. What remains (approximate) is that the terrain light and the haze are taken at `position + facingOffset` and not at the point moved forward towards the camera (Draw 0x518FCD..0x518FF2) ([Seeds and one-off miracles](#seeds-and-one-off-miracles-spellseed-oneoffspellseed)).
- Dispenser broken by a thrown rock: openblack breaks it into pieces like a house; the original draws it with `MultiMapFixed::Draw` (`SpellDispenser::Draw` 0x722940 -> 0x518090). `Abode::ReactToPhysicsImpact` 0x406240 and what happens to its orb still have to be read.
- FOOD and BEAM_EXPLOSION seeds: they are also loaded with material properties (`{1.0,1.1,0}`); apply `L3DMesh::SetMaterialProperties` as for the bubble.
- Vortex between lands (`MagicVortex`, CREATE VORTEX): not ported; on release, fn_005FE3B0 marks `thing+0x25 |= 0x40` at 0x5FE5DD (`script_held::SetCannotBeEaten`).
- Rain in the growth of the trees (`GrowTree`, formula in [trees.md](trees.md)).
- Burning tree: the forced ALPHAREF 230..254 (`OverrideRenderMode`, 0x74B4D6..0x74B51E) needs a separate draw
  per tree ([Fire](magic.md#fire-srcecsfire)). The villagers' `Living::ProcessReaction` 0x5F1270.
- `OPENBLACK_TIME_OF_DAY` is no longer applied in Land 1 (the script controls the clock).
- Fire: the light map `S_LMFireBall` of the burning object, only under the MultiMapFixed objects
  ([Fire](magic.md#fire-srcecsfire)).
- What the audit marked in the code: [Audited assumptions](magic.md#audited-assumptions-2026-10-01). The size of the fireball, settled by the load-time fill: [The size of the fireball cast with the hand](magic.md#the-size-of-the-fireball-cast-with-the-hand).
- `player::CancelMostRecentCharge` / `CancelMostRecentCharge` (PlayerSpellIcons): "fn_0064BCC0 (packet 0x1E, the scribble): the charge of the player's that started last is cancelled" and "(inferred): fn_0064BCC0 is not read". "What is looked for and when" step 4 says "the most charged icon (packet 0x1E)". UNVERIFIED.
- `UpdatePhase` (SpellSeedGraphic.h): "The PSys global phase [0xD4EBF8] (PSysGlobal::DrawLoop 0x68F680: + ms x 0.001 / 3.33, 0..1), per frame". miracles.md (Lightning, DrawOffsetLT) names 0x68F680 `DrawOffsetDecay::ProcessList`. UNVERIFIED which one it is.
- `HeartRingPoint` (WorshipSite.cpp): "fn_00467890(heart, 9, angle) (called at 0x463587): the heart's B_WORSHIP special point 9 turned to the angle. UNVERIFIED: fn_00467890 is not read; its radius may be info.radiusFromCitadel (37.5)".
- `icon::ChargeFraction` (WorshipSpellIcon.cpp): "fn_00729890: the seed's need at that level over its cost (inf: the fraction still to charge)". UNVERIFIED.
- `seed::ProcessInHand` (SpellSeed.cpp): "fn_005D1260 (GInterface::EndAction) for the local interface". The condition under which it is called is not given. UNVERIFIED.
- `ProcessCitadels` (InfluenceSources.cpp): "(inferred) that value is the temple's appearance fade (the virtual call at 0x462DF3 that gives it was not decoded)". The value is settled: the call at 0x462DF3 is the heart's `GetPercentBuilt` (vt +0x880), passed to the 3D object's `SetDrawPercent` (vt +0x200), not a fade. So, if the latch is set when that value reaches 1 as the note says, it waits for the temple to be built, while openblack sets it as soon as the player has a temple. UNVERIFIED: where the 3D object sets the latch. See [The temple's outside](#the-temples-outside-alignment-and-size).
- `SameTypeSwitch` (Reactions.h) reads field +0x04 of the table 0xC09CC0 + 0xC0 × t as "may switch to another reaction of the same type: 1 only for 10, 28 and 35". villagers.md (reactions 7 / 12) calls 0xC09CC0 "the villager type table: only 7 and 12 registered". These may be two different fields of one table. UNVERIFIED.
- `dispenser::SetTimerTime`: "ftol(1000 / [0xD01A38] * seconds) inline (NumGameTicksPerSecond 0x711630), kept when > 0 unsigned (jbe)". "Dispensers" says "a period of 0 disables it". With this note, SET_TIMER_TIME 0 would leave the period unchanged instead. UNVERIFIED.
- `icon::UpdateChargingVisual`: the pulse ring (`TChargingData::Draw` 0x7267A0, while charging: t = (the PSys phase − 0.1, wrapped) × 3.33 (`fn_0068F720(0.1)`); for 0 ≤ t < 1, the ring is drawn on the icon's matrix, scaled 3(1 − t), alpha 255(1 − t), raised 0.1 (0x981A38) × twice the mesh's height, in the player's colour) does not match the charge ring text in "The icons and the charge" (fraction (f + 0.2)/1.2; when full, alpha = 255·(0.1 + 0.5·(sin(4πt) + 1)/2)). They may be two parts of `TChargingData::Draw` 0x7267A0. UNVERIFIED.
- `effects::IsEffectReceiver`: "Villager::IsReachable 0x756460, UNVERIFIED: taken as 1".

What is pending for each miracle is in [Pending](miracles.md#pending).
