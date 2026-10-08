# Villagers: data, state machine and speed

The runblack.exe (W120) villager and its port. The clips (which clip per state, transitions, size) remain in
[animation.md](animation.md); here go the villager's data, the turn, the state changes and the speed rules.

In openblack:
- `ECS/Villager/VillagerCore.{h,cpp}`: the turn (ProcessState, CheckEveryTime), the state changes (SetTopState,
  SetCurrentAndDestinationState, SetState, exits and entries), creation (constructor), CREATED (85), the pause (239),
  SetupMoveToWithHug;
- `ECS/Villager/VillagerStateInfo.h`: names of the fields of the info.dat state table;
- `ECS/Villager/VillagerStateTable.h`: the row of the function table (state, entry, exit, validate...);
- `ECS/Villager/VillagerOriginalFns.h`: an id for each row's entry, exit and validate function in the original (the map
  is in "The original's function ids" below);
- `ECS/Villager/VillagerDebugHooks.cpp`: the trace and the test hooks;
- `ECS/Systems/Implementations/LivingActionSystem.cpp`: the table (`k_VillagerStateTable`) and the turn loop;
- `ECS/Components/Villager.h` (the fields), `LivingAction.h` (the three states and the counters), `Town.h`
  (`TownDesire`), `ECS/Villager/VillagerAge.h` (age), `ECS/Archetypes/VillagerArchetype.cpp` (creation),
  `ECS/VillagerSpeed.*` (speed), `ECS/VillagerAnimations.*` (clips of the state changes);
- `test/test_villager_core.cpp`: 20 cases (see [Tests](#tests));
- `ECS/Villager/VillagerHome.*`, `VillagerFood.*`, `VillagerAge.*`, `VillagerResources.*`, `ECS/Town/AbodeVillagers.*`,
  `ECS/Town/TownVillagers.*`; `test/test_villager_food.cpp`, `test_villager_home.cpp`, `test_villager_age.cpp` (see
  [Home, food, sleep, homeless and age](#home-food-sleep-homeless-and-age));
- the other parts list their files at the start of their section.

> **Code rules.** The villager's data lives in ECS components (`ECS/Components/Villager.h`, `LivingAction.h`, `Town.h`)
> and its behaviour in systems reached through the Locator, with no globals. The state functions stay free of global
> state, so they are unit tested with fakes in `test/`, never through `Locator::X::value()`. Comments describe the
> behaviour in plain English, with no decompiled names or addresses (those belong here). See
> [the conventions](../refactor/README.md).

## Fields (Villager.h)

| offset | field | notes |
|---|---|---|
| Object +0x48 | `life` | 0..1 (`ecs::life`) |
| Living +0xA0 | `birthTurn` | Living::SetAge 0x5ED2C0: `turn − age·1500`; the age is Living::GetAge 0x5ECAF0 = `(turn − birthTurn) / 1500` unsigned (`villager::GetAge`). 1500 = GGameInfo +0xC (0xD01A04) |
| +0xE0 | `flags` | see [Flags](#flags) |
| +0xE8 | `food` | what it has in its belly; the constructor leaves it in [0.5; 1.1) |
| +0xEC | `lastCheckTurn` | turn of the last periodic check (reset by CheckHungry 0x75BEDF) |
| +0xF0 | `foodSpeedUp` | IsFoodSpeedUp 0x55C980; ProcessFoodSpeedup 0x753430 |
| +0xF2 | `discipleType` | g_DiscipleInfos 0x99A1F8 |
| +0xF4 / +0xF6 | `resourceHeld` | food and wood it carries |
| +0xF8 | `pregnancy` | turns remaining (0 = no) |
| +0xFC | `buildingSite` | the building site it builds ([Builders](#builders)) |
| +0x100 | `mother` | |
| +0x118 | `targetThing` | TargetThing (bw1-decomp `Villager.h`) |
| +0x118 (int) | `buildPosIndex` | while building: the site's ring index 0..127 (the original's union with TargetThing, kept apart) |
| +0x11C | — | union Football* / TradeTown / WanderArea (bw1-decomp `Villager.h`). openblack keeps no union: the farmer's work point is its own `workPos` (assumption 32) |
| +0x128 / +0x12C | `abode` / `town` | |

`lifeStage` is a mirror of `flags & 0x8` (read by DetailMeshes, drawing and sounds) and `sex` is a mirror of
GVillagerInfo +0x1F8 (`IsWoman` 0x752620 and SetSpeed read the info one). `task` is not from the original.

Fields of the original that openblack does not keep as such: `next` +0xE4 (home list), `LastPlayerToInteract`
+0x104, +0x108 / +0x10C / +0x110, `fire_effect` +0x114 (fire), +0x124.

- +0x118 also holds the death reason as a byte (VillagerDead writes `(u8) reason` after SetDying, 0x750929; GetDeathReason
  0x55CB10): the same union as TargetThing and the builder's ring index. Writers from the building side:
  fields::RemoveFarmer 0x528362 / 0x5283B8 / 0x5283C8 and Field::DeleteDependancys 0x52814E (null),
  fish_farms::AddFisherman 0x52D281 (the farm).
- +0x11C / +0x120 under a script: SET_SCRIPT_ULONG on a villager (GScript::SetScriptUlong 0x6F8770, 0x6F884D..0x6F8855)
  writes +0x11C = the clip and +0x120 = the number of plays. A farmer uses +0x11C as its work point.
- +0x28 is the GVillagerInfo pointer (ChangeInfo 0x761A0B). The records are `_VillagerInfos` 0xDA6BE8, stride 0x3A4.
  GVillagerInfo::Find 0x752650 returns the FIRST record whose +0x1F4 is the tribe and whose +0x1FC is the villager
  number, or null.
- Living +0xB4 status: bit 0x1 = dead, bits 4-5 = landType, bit 0x40 = skeleton (Living::IsSkeleton 0x416FF0;
  Villager::SetSkeleton 0x7562F6..0x756315), bit 0x80 = downed.
- GameThingWithPos +0x24: 0x1 in the map (Object::IsObjectInMap 0x6392B0, vt +0x178); 0x2 the MultiMapFixed bit
  (MultiMapFixed ctor 0x52E1F0); 0x4 in the hand (PlaceObjectInMagicHand 0x5FB014); 0x40 in the physics (set 0x636F2F,
  cleared 0x6375A0 / 0x646B56); 0x80 on a structure (only Living::MoveOnStructure sets it).
- Living +0x60 is compared with the abode in HomeDeleted 0x7611F3..0x7611FD and set to 0 there. Its meaning is unknown
  (see [Pending](#pending)).
- Accessors: Villager::GetTown 0x751F00 (vt +0x48) = +0x12C; GetAbode 0x752160 = +0x128; SetAbode 0x750DE0 (+0x128 =
  abode, SetTown(0), then SetTown(abode's town) when there is an abode); SetTown 0x756530; IsChild 0x55C970 (vt +0xAF8) =
  flags & 8; Villager::GetPlayer 0x7502F0 = its town's +0x2C, or none without a town; GetTribe 0x751EE0 =
  GGame::GetTribe(info +0x1F4), whose +0x10 is the tribe type; Town::GetTribe 0x73C840 = the GTribeInfo of Town +0x5B8.
- IsSexuallyActive 0x761090: StartHavingSexAge (info +0x228) <= age < StopHavingSexAge (+0x22C).
- IsPregnant 0x752210: info sex (+0x1F8) == 1 and +0xF8 != 0.
- IsVillager is vt +0x2C8 (Villager 0x55CAB0 returns 1). IsTree is vt +0x338 (Tree::IsTree 0x55D9D0 returns 1,
  GameThingWithPos 0x402320 returns 0).

### Flags

Villager +0xE0: 0x1 after a tap on its home
(SetupAfterTapOnAbode), 0x2 at the worship site (AddVillagerToWorshipSite / RemoveVillagerFromWorshipSite), 0x4 inside
the home (ArriveHome), 0x8 child (SetAge; CheckChildGrownUp clears it), 0x10 on the way to worship or to a fire, 0x20 in
the hand (InterfaceSetInMagicHand; EndPhysics / Landed clear it), 0x80 football / script, 0x200 / 0x400 disciple /
follower, 0x800 / 0x1000 entry / exit clip (in openblack `SkeletalAnimation::transitionFlags`), 0x2000 going to
sleep (CheckWhenGoingToBed). In openblack the worship 0x2 lives in `WorshipVillager::atSite` (AddVillagerToWorshipSite
0x76C3F0 / RemoveVillagerFromWorshipSite 0x76C440); CheckEveryTime (0x7504DC) reads `flags & 2` **or** `atSite` until it
moves to `flags`. The disciple bits 0x200 / 0x400 are set by nobody (disciples are not ported).

Also: +0xE0 0x40 = counted out of the world population (SetDying 0x76A53E, ~Villager 0x74FBC0); bits 14-15 = the
carried tree type (see [Carrying resources](#carrying-resources-and-the-storehouse)).

## info.dat state table (VillagerStateInfo.h)

`GVillagerStateTableInfo::Infos` 0xDB9E68, 0x114 bytes per state; the disassembly reads each field at the file
offset + 0x10. The fields of `InfoConstants.h` keep their `field0x..` names (other code uses them);
`state_info::` gives them names: `Clip` (0x00), `ServedDesire` / `ServedDesireAmount` (0x04 / 0x08, AdjustTownModifier),
`IsFinal` (0x0C), `NotStoredAsPrevious` (0x10), `IsMoving` (0x14), `ResumeState` (0x20), `SpeedGroup` (0x24),
`CanPauseForASecond` (0xC4), `NoGoHomeWhenHurt` (0xD8), `DoPeriodicChecks` (0xE4), `GoHomeWhenHurt` (0xE8),
`NoOutOfClip` (0xF0), `LifeDrainPerTurn` (0xF8)... (full list in the file).

Fields not named above (file offset, with the memory address the code reads):
- 0x18 IsScriptState (0xDB9E90, vt +0x960, read by ExitInScript 0x5ED9E7); 0x1C script-interruptable (0xDB9E94, vt
  +0x964, read by ExitNoChangeState).
- 0xA8 AvailableState, values 1 / 3 / 4: GetVillagerAvailableState 0x751F40 reads it from GetFinalState's row, and
  IsVillagerAvailable tests `& 1`.
- 0xC8 food interest (0xDB9F40) and 0xCC wood interest (0xDB9F44): the state factor of IsInterestedInFoodObject /
  IsInterestedInWoodObject.
- The carried-object column read by SetStateCarriedObject is at 0xDB9F54. The reaction-availability column read by
  IsAvailableForReaction is at 0xDB9F64 (both called "+0xEC", see [Pending](#pending)).
- NoGoHomeWhenHurt (0xD8) is also read by ExitGetFoodAtWorship.

## The function table (VillagerStateTable.h)

The original's is at 0xD09198, 0x90 bytes per state: +0x00 state, +0x10 entry, +0x20 exit, +0x30/+0x40
save/load, +0x50, +0x60 (clip function), +0x70 (transition clip), +0x80 validate. In openblack:
- the **exit** returns **1 = can exit** and receives the next state; the **entry** receives (previous final,
  new state) and returns **1** (accepted), **0x23** (accepted; the function set the states) or another value (rejected);
- an empty slot is "no function": it counts as 1, as in the original;
- an unported state (`k_TodoEntry`) does nothing (returns 0) and warns **once per state** with the original's
  address; if the original has an entry, exit or validate in a row that openblack leaves empty, it warns once
  (`TODO: Unimplemented entry function of ... (0x...): taken as 1`);
- the worship exits (58, 59, 60, 213) keep their old convention (false = can exit) and the row
  adapts them;
- the fire ones are in their rows, as `_$E32` fills them (see [Fire in the table](#fire-in-the-table));
- 16 DROWNING: EnterDrowning 0x767410 and ExitDrowning 0x767420 only accept (`mov eax, 1; ret`); the state function
  is Villager::Drowning 0x76A780 (`ECS/VillagerDrowning.*`, see [water.md](water.md)).

- The table is filled at run time by its static initialiser `_$E32` 0x59D030. 0xD09198 lies past the raw end of .data,
  so the static image holds only zeros. Column +0x50 (0xD091E8 + 0x90·s) is the town-emergency answer, and its
  this-adjustment +0x54 is 0 in all 255 rows. Entry column = 0xD091A8, exit column = 0xD091B8, validate column = 0xD09218.
- Villager::IsStateEntryFunctionSameAs 0x7524D0: compares the entry members of both rows (all four dwords; a first dword
  of 0 counts as equal at once, 0x7524F4). Rows sharing an entry: EnterInScript, EnterPlayAnim, EnterBuilding,
  EnterFarming, EnterFishing.
- The hand's own entry and exit are EnterInHand 0x76AFE0 and ExitInHand 0x76B000.
- Other rows and clip functions:

| state | function | entry / exit / clip |
|---|---|---|
| 13 SET_DYING | StateSetDying 0x5AFF40 = jmp [vt +0x6A4] = SetDying 0x76A4C0 | |
| 14 DYING | StateDying 0x5AFE30 = jmp [vt +0x89C] = Villager::Dying 0x76A570 | clip DyingAnimation 0x423770 |
| 15 DEAD | StateDead 0x5AFE90 = jmp [vt +0x8A0] = Villager::Dead 0x76A5E0 | clip DeadAnimation 0x4237A0 |
| 19 / 20 / 21 / 22 | Living thunks 0x5B0050 / 0x5AFE20 / 0x5B00F0 / 0x5AFE80 -> vt +0x8AC / +0x8BC / +0x8B0 / +0x8C0; GotoFoodReaction 0x7646A0, GotoWoodReaction 0x7646D0 | exit thunk 0x5B0100, validate 0x756A00 |
| 23 WAIT_FOR_ANIMATION | IsReadyForNewAnimation(1) -> SetTopStateToFinal 0x5ECA80 and 0, else 1 (0x5EC995) | |
| 27 MOVE_IN_FLOCK | thunk 0x5B0190 -> vt +0x8B4 = Living::MoveInFlock 0x5ECDB0 | none |
| 28 MOVE_ALONG_PATH | StateAlongPath 0x5B0040 -> vt +0x52C = Living::MoveAlongPath 0x5EE230 | EnterInScript 0x5ED7E0 / ExitInScript 0x5ED9C0 |
| 33 / 34 | GotoStoragePitForFood 0x769830 / ArrivesAtStoragePitForFood 0x7698B0 | |
| 39-41, 184 | | SaveBuilding 0x754A00 / LoadBuilding 0x754A60 |
| 47-50, 52 | | exit ExitForesting 0x75FE20 (+0x118 = 0; 1); 47's validate WallhugValidate 0x756990 |
| 48 FORESTER_GOTO_FOREST | ForesterGotoForest 0x75F710 = jmp CheckSatisfyWoodDesire 0x75F4A0 | |
| 51 / 54 | ForesterChopsTreeForBuilding 0x75FB40 (SetTopState(163), 1) / ArrivesAtBigForestForBuilding 0x75FAB0 (`return 1`) | |
| 52 | ForesterFinishedForestering 0x75FB60: (int16) wood > 0 -> GotWoodDecideWhatToDo, else SetTopState(163) | |
| 55 / 56 | FishermanArrivesAtFishing 0x75B5D0 / Fishing 0x75B6A0 | EnterFishing 0x75B820 / ExitFishing 0x75B880 |
| 67 / 68 / 69 | FarmerArrivesAtFarm 0x759D20 (clip 259 P_FARMER_SOWING_SEEDS) / FarmerPlantsCrop 0x759EC0 / FarmerDigsUpCrop 0x759E40 (clip 257 P_FARMER_HARVESTING) | EnterFarming 0x75A250 / ExitFarming 0x75A2A0 |
| 110 / 111 / 112 | HousewifeStartsGivingBirth 0x7621A0 / HousewifeGivingBirth 0x762430 / HousewifeGivenBirth 0x7624A0 | none |
| 113 CHILD_AT_CRECHE | ChildAtCreche 0x757C90 | none |
| 117 EAT_FOOD | EatFood 0x75C000 | clip 254 EatDinner |
| 118 EAT_FOOD_AT_HOME | EatFoodAtHome 0x75C090 | clip -4 |
| 131 MORN_DEATH | MornDeath 0x76AA60 = jmp GoHome 0x760270 | into / out-of clip MournIntoOutofAnimation 0x424300 |
| 186 / 187 | TakeWoodFromTree 0x75FBC0 (clip ForesteringAnimation 0x423AC0) / TakeWoodFromPot 0x75FBB0 (`return 1`) | |
| 197 AFTER_TAP_ON_ABODE | AfterTapOnAbode 0x761440 | clip 273, YawnAnimation 0x424050 |
| 200 SCRIPT_PLAY_ANIM | ScriptPlayAnim (0x768973..0x7689A9) | EnterPlayAnim; exit vt +0x914 = ExitInScript 0x5ED9C0 |
| 205-208 | 0x766680 / 0x766700 / 0x766810 / 0x766850 | 208: MournIntoOutofAnimation 0x424300 |
| 209 NOTHING_TO_DO | 0x760000 (`mov eax, 1; ret`) | |
| 221 DISCIPLE_NOTHING_TO_DO | 0x7540D0 | entry 0x754140; save / load 0x755760 / 0x755780 |
| 238 SLEEP_IN_TENT | 0x761AE0 | clip 381, SleepInTentIntoOutofAnimation 0x424290 |
| 239 PAUSE_FOR_A_SECOND | | clip function PauseForASecondAnimation 0x424080 |
| 243 | | TownEmergencyAnimation 0x424100 |
| 246 | | SitDownAnimation 0x424210 (+0x60), SitDownIntoOutOfAnimation 0x4243A0 (+0x70) |
| 249 ARRIVES_HOME_FROM_WORSHIP | 0x76B7E0 = jmp ArrivesHome 0x760930 | exit ExitAtHome |
| 250 SLEEP_IN_TENT_FROM_WORSHIP | 0x76B7F0 = jmp SleepInTent 0x761AE0 | |

### The original's function ids (VillagerOriginalFns.h)

`k_OriginalStateFns` keeps, for each of the 255 rows, an id for its entry, exit and validate functions instead of
their addresses: the distinct addresses of each column are numbered in table order, 0 = no function there. Only
equalities within a column are used (IsStateEntryFunctionSameAs, IsStateExitFunctionSameAs, the reaction validate
and the building exit), so the ids keep the original's behaviour. The ids and the addresses they stand for:

- entry: 1 = 0x5ED7E0, 2 = 0x5EDA50, 3 = 0x767410, 4 = 0x76AFE0, 5 = 0x5EDDD0, 6 = 0x759750, 7 = 0x75B820, 8 = 0x75A250, 9 = 0x76ACE0, 10 = 0x7687D0, 11 = 0x768840, 12 = 0x75ADC0, 13 = 0x75AF30, 14 = 0x754140, 15 = 0x76A2A0, 16 = 0x76B570, 17 = 0x761800.
- exit: 1 = 0x5EDDA0, 2 = 0x5ED9C0, 3 = 0x5EDB10, 4 = 0x7527A0, 5 = 0x76ACB0, 6 = 0x5ED580, 7 = 0x768640, 8 = 0x767420, 9 = 0x5EDDC0, 10 = 0x76B000, 11 = 0x5EE090, 12 = 0x761B40, 13 = 0x7597B0, 14 = 0x76C120, 15 = 0x75FE20, 16 = 0x75B880, 17 = 0x76C170, 18 = 0x76C1F0, 19 = 0x76C280, 20 = 0x75A2A0, 21 = 0x763280, 22 = 0x7527E0, 23 = 0x768F50, 24 = 0x76AD80, 25 = 0x768780, 26 = 0x768830, 27 = 0x7689C0, 28 = 0x766390, 29 = 0x759A90, 30 = 0x75AE80, 31 = 0x75AF80, 32 = 0x76A2D0, 33 = 0x761980.
- validate: 1 = 0x756990, 2 = 0x7569A0, 3 = 0x7569D0, 4 = 0x756A00, 5 = 0x756A50, 6 = 0x756A80, 7 = 0x756AA0.

Each row's state function (the column openblack does not keep), by row:

0 0x5EC1D0; 1 0x5EC270; 2 0x5EC2C0; 3 0x5ECD00; 4 0x5ED9A0; 5 0x5EDAD0; 6 0x5AFFE0; 7 0x5B0060; 8 0x5B01B0; 9 0x5B0240; 10 0x5EC330; 11 0x7606E0; 12 0x5B0230; 13 0x5AFF40; 14 0x5AFE30; 15 0x5AFE90; 16 0x76A780; 17 0x5AFF30; 18 0x5AFFD0; 19 0x5B0050; 20 0x5AFE20; 21 0x5B00F0; 22 0x5AFE80; 23 0x5EC990; 24 0x5AFF20; 25 0x5F2640; 26 0x765320; 27 0x5B0190; 28 0x5B0040; 29 0x5EDDE0; 30 0x5B00E0; 31 0x769620; 32 0x7696D0; 33 0x769830; 34 0x7698B0; 35 0x769B30; 36 0x760270; 37 0x760930; 38 0x760B10; 39 0x758990; 40 0x758AF0; 41 0x758C40; 42 0x76BFA0; 43 0x76C080; 44 0x76C100; 45 0x76C110; 46 0x76C2D0; 47 0x75F7D0; 48 0x75F710; 49 0x75F930; 50 0x75FAC0; 51 0x75FB40; 52 0x75FB60; 53 0x75F9E0; 54 0x75FAB0; 55 0x75B5D0; 56 0x75B6A0; 57 0x5EC310; 58 0x76BCC0; 59 0x76BE00; 60 0x76C680; 61 0x76CB00; 62 0x76CB80; 63 0x76CBB0; 64 0x76CBE0; 65 0x76C390; 66 0x76C3C0; 67 0x759D20; 68 0x759EC0; 69 0x759E40; 70 0x763110; 71 0x763120; 72 0x763140; 73 0x75D230; 74 0x75EA10; 75 0x75E170; 76 0x5319C0; 77 0x5319D0; 78 0x75F270; 79 0x75F2C0; 80 0x75F2F0; 81 0x75F300; 82 0x763170; 83 0x7631F0; 84 0x7631B0; 85 0x753DD0; 86 0x76B990; 87 0x76B980; 88 0x76A7E0; 89 0x76A8B0; 90 0x76C7C0; 91 0x768C30; 92 0x768CC0; 93 0x7690D0; 94 0x769070; 95 0x768FB0; 96 0x768DD0; 97 0x769390; 98 0x769460; 99 0x768E30; 100 0x761C10; 101 0x761CE0; 102 0x761D60; 103 0x761EA0; 104 0x761F10; 105 0x761F60; 106 0x761FA0; 107 0x761FC0; 108 0x761FF0; 109 0x762020; 110 0x7621A0; 111 0x762430; 112 0x7624A0; 113 0x757C90; 114 0x7578C0; 115 0x757F10; 116 0x760B20; 117 0x75C000; 118 0x75C090; 119 0x760B30; 120 0x760D70; 121 0x760E50; 122 0x760E60; 123 0x760EE0; 124 0x760F50; 125 0x760F80; 126 0x761010; 127 0x761020; 128 0x761030; 129 0x761320; 130 0x76A8D0; 131 0x76AA60; 132 0x764410; 133 0x764490; 134 0x764610; 135 0x764650; 136 0x764680; 137 0x764690; 138 0x764660; 139 0x764670; 140 0x766C60; 141 0x766D00; 142 0x766D60; 143 0x766E50; 144 0x766FB0; 145 0x75C0F0; 146 0x7678A0; 147 0x767970; 148 0x767A00; 149 0x764AA0; 150 0x764B50; 151 0x764D10; 152 0x764D70; 153 0x759930; 154 0x759990; 155 0x767BA0; 156 0x767CE0; 157 0x767DC0; 158 0x765140; 159 0x767E00; 160 0x767C80; 161 0x7653F0; 162 0x765450; 163 0x7515C0; 164 0x756E10; 165 0x76AA70; 166 0x758390; 167 0x767F70; 168 0x765E00; 169 0x7613F0; 170 0x757260; 171 0x757270; 172 0x757420; 173 0x757590; 174 0x757610; 175 0x757690; 176 0x757720; 177 0x757730; 178 0x757750; 179 0x757760; 180 0x757850; 181 0x757740; 182 0x758EF0; 183 0x758F00; 184 0x758F60; 185 0x7592E0; 186 0x75FBC0; 187 0x75FBB0; 188 0x75FB90; 189 0x75FBA0; 190 0x7690E0; 191 0x768EC0; 192 0x768F20; 193 0x759960; 194 0x7668E0; 195 0x766910; 196 0x766930; 197 0x761440; 198 0x768A10; 199 0x7687F0; 200 0x768970; 201 0x7662F0; 202 0x7663F0; 203 0x766130; 204 0x759A00; 205 0x766680; 206 0x766700; 207 0x766810; 208 0x766850; 209 0x760000; 210 0x759520; 211 0x759450; 212 0x75B940; 213 0x76C5E0; 214 0x766A90; 215 0x765870; 216 0x75AC50; 217 0x75AFE0; 218 0x75B000; 219 0x75B1E0; 220 0x75A7E0; 221 0x7540D0; 222 0x75F380; 223 0x769D20; 224 0x769DC0; 225 0x76A1B0; 226 0x76A220; 227 0x767280; 228 0x7691A0; 229 0x756D30; 230 0x761510; 231 0x763B40; 232 0x7586B0; 233 0x76AC40; 234 0x761810; 235 0x7673A0; 236 0x763D90; 237 0x763F80; 238 0x761AE0; 239 0x76B0B0; 240 0x76B1C0; 241 0x76CA30; 242 0x76B200; 243 0x76B300; 244 0x756540; 245 0x76B3F0; 246 0x76B4E0; 247 0x768A20; 248 0x761B70; 249 0x76B7E0; 250 0x76B7F0; 251 0x766380; 252 0x76B590; 253 0x7615C0; 254 0x76B800.

### Fire in the table

The static initialiser `_$E32` (0x5AA2D9..0x5AA767) sets +0x10 / +0x20 of each row:

| row | state | entry (+0x10) | exit (+0x20) | where in `_$E32` |
|---|---|---|---|---|
| 215 | REACT_TO_FIRE | — | ExitReaction 0x7527A0 (the thunk 0x5B0100 = `jmp [vt +0x910]`) | — |
| 216 | PUT_OUT_FIRE_BY_BEATING | EnterPutOutFire 0x75ADC0 | ExitPutOutFire 0x75AE80 | 0x5AA2D9 / 0x5AA2EC |
| 217 | PUT_OUT_FIRE_WITH_WATER | EnterPutOutFire 0x75ADC0 | ExitPutOutFire 0x75AE80 | 0x5AA421 / 0x5AA434 |
| 218 | GET_WATER_TO_PUT_OUT_FIRE | EnterPutOutFire 0x75ADC0 | ExitPutOutFire 0x75AE80 | 0x5AA4BE / 0x5AA4CB |
| 219 | ON_FIRE | EnterOnFire 0x75AF30 | ExitOnFire 0x75AF80 | 0x5AA611 / 0x5AA642 |
| 220 | MOVE_AROUND_FIRE | EnterPutOutFire 0x75ADC0 | ExitPutOutFire 0x75AE80 | 0x5AA75D / 0x5AA767 |

What they return (read in the disassembly):
- **EnterPutOutFire(final, s)**: 1 if `IsStateEntryFunctionSameAs(final, s)` 0x7524D0 (both rows have the same
  entry: from 216/217/218/220 to another of them); otherwise, with a live fire (+0x114) (fn_0075AD90; if it no longer
  exists, +0x114 = 0), its vt +0x2C and a reaction (+0x94) not shut down (Reaction +0x34, set by ShutDown 0x6E4723):
  **0** if it is already in the root's firemen list (0x75AE75), and if not it adds it (AddFireman 0x7309A0) and gives
  **1**. In any other case it gives **0** and, if the previous final is reactive (table +0xB8, 0xDB9F30), StopReacting
  (vt +0x998). A 0 is 0x2F and Villager::SetTopState enters 163.
- **ExitPutOutFire(s)**: always **1** (0x75AECB and 0x75AF20). If the exit is not "the same" (vt +0x96C,
  0x752530: another row with ExitPutOutFire, or a non-final state) it leaves the firemen list and the town's
  on-the-way-to-worship list (0x73E360); if it was not in the list, only +0x114 = 0 and it returns without ExitReaction;
  otherwise, ExitReaction 0x7527A0 (ends the reaction unless `s` is reactive).
- **EnterOnFire(final, s)**: **1** without fire or with its vt +0x2C at 0; **0** if it is already in the list
  (0x75AF78); otherwise it adds it and **1**.
- **ExitOnFire(s)**: always **1**; it leaves the list if it is there and **+0x114 = 0 always** (it does not look at `s`).

The state changes of `VillagerFire.cpp` and `VillagerTeleport.cpp` are now those of the core: `villager::SetTopState`
(with the pause roll, the exit of TOP and of the final and the entry, once each, and the codes 1 / 0x2E / 0x2F;
both go through `villager_reactions::SetTopState`, which also ends openblack's walk),
`villager::SetState(2, s)` for the saved state (vt +0x938: skips those of table +0x10 and adjusts the town) and
`villager::SetupMoveToWithHug`. There is no longer a local SetTopState, nor `CallEntry` / `CallExit`, nor
`villager_fire::CallFinalStateExit`; the worship exits (58, 59, 60, 213) run through their rows. In addition:
- SetupMoveAroundFire 0x75A770 only stores destination and next state if SetTopState(220) gives 1 (0x75A783).
- PopFromPrevious 0x751E50 (`villager_reactions::PopFromPrevious`, a single one for fire and teleport):
  SetTopState of the resume state of what was saved (Infos +0x30 0xDB9E98 = file 0x20); if it gives 0x2E, TOP = 163
  raw (LivingAction::SetState 0x5ECC90) and afterwards PREVIOUS = 0 raw. **With nothing saved** it is row 0, whose
  resume state is **0** in info.dat: SetTopState(0) = INVALID_STATE, as in the original (Living::InvalidState 0x5EC1D0
  returns 0 each turn). Previously 163 was invented.
- MoveAroundFire 0x75A7E0 on arriving: PopFromPrevious (0x75A815) and PREVIOUS = 163 **raw** (0x75A81A..0x75A827,
  LivingAction::SetState 0x5ECC90 with ecx = +0x8C: without the table +0x10 skip nor the town), not Villager::SetState.
- ResetStateAfterReacting 0x751E10 (vt +0x9A0): PopFromPrevious and, if the final state is reactive (file 0xB8),
  SetTopState(163). StopReactingAndSetState 0x5F11C0 (vt +0x99C): that and then StopReacting if it is still reacting. It
  is used by ReactToFire (0x765A48, already a fireman) and TeleportReaction (0x76642F, after the jump; before it was the
  other way round: first StopReacting and then PopFromPrevious).
- **(approximate)** The exit of MOVE_TO_POS (ExitMoveToPos 0x5EDDA0) is not ported: these SetTopState remove
  openblack's walk marks (except with 0x2E), as the local SetTopState did.

Differences from before (checked in a game, see below): the entries and exits also run when the final state
does not change, as in the original. When replanning or arriving (SetupMoveToWithHug / SetTopStateToFinal) a fireman in
220 goes through ExitPutOutFire(220) and EnterPutOutFire(220, 220), which change nothing; a villager in 219 goes through
ExitOnFire, which **clears +0x114**: after its first walk it stops fleeing from other fires and only stays in 219 while it
is itself burning (the same in the original: OnFire 0x75B1E0 calls SetupMoveToWithHug 0x5F2890 at 0x75B39E).

**The pause 239 does not come out in fire nor teleport.** CanPauseForASecond 0x752120 looks at the row of the
**destination** state (Infos +0xD4 in memory, 0xDB9F3C = file 0xC4), which is 0 in 163, 201, 202 and 215..220: those
SetTopState do not roll the pause. It can only come out when PopFromPrevious returns to a state that pauses (a job with
0xC4 = 1) or in 248 (worship).

### Exits of the reactions (ExitReaction, ExitReactToTeleport)

- **ExitReaction 0x7527A0** (vt +0x910; the rows store the thunk 0x5B0100 = `jmp [vt +0x910]`, 45 rows in `_$E32`):
  CircleHugInfo::Reset(+0x70) 0x60A9F0 (in openblack it removes `WallHugObjectReference`, **approximate**), and if `s` is
  not reactive (IsReactiveState inline, Infos +0xC8 0xDB9F30 = file 0xB8) StopReacting (vt +0x998). Returns 1.
  In the table: 215 and the unported rows 6-9, 203 and 214 (`TodoWithExitReaction`); the other rows with that thunk
  (12, 19-22, 25, 26, 30, 140-168, 194-196, 205-208, 227, 231, 235-237) remain empty because nobody enters them.
  ExitPutOutFire calls it at the end (0x75AF19).
- **Villager::StopReacting 0x7637D0** → **Living::StopReacting 0x5F1140**: with TOP 203 and dancing, RemoveFromDance(1)
  (unported: 203 has no state function); with a reaction (+0x94): out of the reaction's list,
  `fn_005F0FE0(tipo)` = the type's record receives the turn (`reactions::RefreshRecord`), +0x94 = 0; +0xBC = 0 always.
  In openblack `villager_reactions::StopReacting` calls `villager_fire::StopReacting` (reaction 0, null object,
  RefreshRecord of REACT_TO_FIRE), `villager_teleport::StopReacting` (clears its state, RefreshRecord of
  REACT_TO_TELEPORT), `villager_shield::StopReacting` and `villager_mourning::StopReacting` (one taker fewer: 0x5F1186
  `dec` reaction +0x1C); `villager_reactions::IsReacting` is true for any of the four. EnterPutOutFire (0x75AE55) also
  uses it.
- **ExitReactToTeleport 0x766390** (exit of 201, 202 and 251): if not `IsStateExitFunctionSameAs(s)` (vt +0x96C
  0x752530), it leaves its town's on-the-way-to-worship list (GetTown vt +0x48 → 0x73E360) and +0xE0 &= ~0x10 (also
  without a town); then ExitReaction(s) and its result.
- **IsStateExitFunctionSameAs 0x752530** (`villager::IsStateExitFunctionSameAs`): the exit of the GetFinalState row and
  that of `s` are the same (the whole 16-byte pointers; here the ids of `VillagerOriginalFns.h`, two empty ones
  count as equal) → 1; otherwise, `s` final (0xDB9E84) → 0, otherwise 1. ExitPutOutFire (0x75AE95) now also uses it,
  instead of the hand-written list.

## Creation (Villager::Create 0x74FBE0 and the constructor 0x74F950)

`Villager::Create` first rolls `GameRand(10) <= 1` (0x74FBF0) to try a SpecialVillager (none is ported, so a normal
one always comes out). Afterwards, the constructor (`villager::Construct`):
1. Living::Living (life = info.life) and SetToZero 0x74FB20 (all new fields to 0).
2. SetAge 0x7528C0: child if `age < grownUpAge` (`flags |= 8`), otherwise `age = max(age, 18)` and `flags &= ~8`;
   meshes and scale (consumes FloatRand); `birthTurn = turn − age·1500`.
3. `foodSpeedUp = 0` (0x74FA02); woman (info +0x1F8 == 1): `pregnancy = 0` (0x74FA08).
4. food (0x74FA18..0x74FA67): `min(1, GameFloatRand(0.6) + hungryForFood)` is a macro that evaluates twice: if the
   first roll gives < 1 it rolls again and stores the second one without clamping.
5. lastCheckTurn (0x74FA6D..0x74FACD): `turn − (GameRand(processChecksEvery) < turn ? GameRand(...) : turn)`. The
   second roll can exceed the turn: on turn 6 with a roll of 7 it ends up 0xFFFFFFFF (literally; the unsigned
   subtraction of GetGameTurnsSinceLastChecked leaves it correct).
6. State counter Object +0x58 (`turnsUntilStateChange`) = GameRand(500) + 1 (0x74FAB0).
7. **Water rule** (0x74FADC..0x74FAF5): `SetState(TOP, IsWater(pos) ? 16 DROWNING : 85 CREATED)`, the exact
   SetState, without entry, clips or speed, and without calling anything of the water. `IsWater` = MapCoords::IsWater
   0x6035B0 (`pot_resource::IsWater`; with the water merge, `sea_cells::IsWater`).
8. `++g_game+0x205A54` and SetSkeleton 0x7562C0 (openblack does not call SetSkeleton here, see [Pending](#pending)).

Afterwards the archetype sets the home and the town (in the original, CallVirtualFunctionsForCreation and
AddVillagerToAbode) and the speed of 85 **(approximate)**.

In Land2 the script creates three villagers in open sea (1578, 2220) on turn 6: they are born in 16 DROWNING, as in the
original.

- The special-villager roll calls SpecialVillager::Create 0x71F1A0. If the special villager comes back without +0xA & 1,
  it is the result (0x74FC1F).
- Villager::InitialiseScale 0x74FB80 (0x74FB88..0x74FBAB): a child (age < grownUpAge) takes ageToScale[age − 1], where
  ageToScale is GVillagerInfo +0x2E8. At age 0 this reads the dword before the table (+0x2E4, dancingSpeed) as a float.
  An adult takes 0.9 (0x3F666666).
- Villager::SetScaleForAge 0x752A90, adult (0x752AF5..0x752B2F): t = (0.05 − GameFloatRand(0.1)) + 1. If the current
  scale < t, the scale becomes a SECOND draw (0.05 − GameFloatRand(0.1)) + 1; otherwise it stays. An adult coming from
  InitialiseScale (0.9) therefore always uses two draws. Child: current + GameFloatRand((ageToScale[age + 1] − current)
  × 0.75) (0x752AA4..0x752AE9).
- SetAge (0x752919..0x752A5C) changes the meshes only when the age crosses grownUpAge, comparing against the old birth
  turn. A new child gets +0x20C / +0x208 / +0x204; a new adult gets GetDetailMesh(2 / 1 / 0). InitialiseScale runs at
  0x752A62 and SetScaleForAge at 0x752A6D.
- The constructor's fourth argument is SetSkeleton (0x74FB0C, Villager::SetSkeleton 0x7562C0): status bit 0x40, the
  skeleton mesh or the villager's own meshes, then a SetScaleForAge draw (0x756436).
- The world population g_game +0x205A54 goes down in SetDying and in ~Villager 0x74FBC0, only for a villager not yet
  counted out (+0xE0 & 0x40).
- MobileObject's CallVirtualFunctionsForCreation (0x607150 + 0xA9) inserts the villager at the head of its cell's mobile
  list (Object 0x636740) (inferred: after the town and the house).

## The turn (Villager::ProcessState 0x74FF70)

`LivingActionSystem::Update`: first the test hooks, then for each villager (registry order)
`ProcessReaction` 0x5F1270 (see [reactions 7 and 12](#food-and-wood-jobs-reactions-7-and-12)) and `ProcessState`; at the end the souls (once a turn, see [Death](#death)).
**(approximate)** the original keeps villagers and animals in a single list (g_game +0x205BBC) and the path step
(Living::MoveToPos 0x5EC270) goes inside the state function; openblack moves everyone first (PathfindingSystem) and
processes the animals afterwards.

ProcessState:
1. `++turnsSinceStateChange` (+0x90) and ProcessFoodSpeedup 0x753430 (`foodSpeedUp != 0 && turn % 10 == 0` → −1).
2. validate (+0x80) of the TOP (+0x8C, 0x74FF91) and of the raw FINAL (+0x8D, 0x74FFD9), only if the row has one; the
   result is not used. The reaction rows (201, 202, 251, 214-218, 220, 6-30, 140-196... all those with the original
   validate 0x756A00, id `k_ReactionValidate` in `VillagerOriginalFns.h`) call `villager_reactions::ReactionValidate` 0x756A00 from
   `LivingActionSystem::VillagerCallValidate`: with no reaction object (+0xBC), or not
   available, or in the hand if the reaction asks for it → `PopFromPrevious` 0x751E50. That is why `ReactToFire` 0x765870
   only returns 0 (without changing state) when the object is not an `Object` or has no fire, and `GoToTeleportReaction`
   0x7662F0 checks nothing (openblack only returns 0 if it does not store a stone, instead of reading a null); the
   custom exits (inferred) that did that work are gone.
3. If an entry / exit clip is playing (flag 0x800): it waits for it to finish (IsReadyForNewAnimation 0x5EC960 →
   FinishedIntoOutOfAnimation 0x750060) and does nothing else that turn.
4. CheckEveryTime 0x750410 and CallState 0x7521D0 (the TOP's function).

CheckEveryTime:
- controlled by a script (+0x25 & 4; written by GameThingWithPos::SetControlledByScript 0x402240, in openblack
  `ecs::script_held`) → nothing;
- wear: if the TOP is moving, life drops by the TOP's `LifeDrainPerTurn` and from there the row of the **raw
  FINAL** is looked at; otherwise, it drops by that of the final state (GetFinalState) and the TOP's row is looked at
  (literal quirk);
- if the row has checks (`DoPeriodicChecks`): life == 0 exactly → VillagerDead(CHANT if the final is 248-250 or
  flags & 2 / `WorshipVillager::atSite`, otherwise EXHAUSTION); if **more than** `processChecksEvery` (8) turns have
  passed → the periodic check, every 9
  turns: CheckDeathFromOldAge when `(turn + UniqueId) % 800 < t`; hurt (`life < 0.3`, outside the home, the row
  allows going home, not knocked down, and in 19/20 only with `food > hungryForFood`) → SetTopState(36 GO_HOME) (0x7505C3),
  which walks it home (GoHome 0x760270 → DoGoingHome 0x760280: home, path, ARRIVES_HOME);
  CheckChildGrownUp, WomanSpecial and CheckHungry (which resets `lastCheckTurn`);
- otherwise: the disciple check (flags & 0x200, a type that ignores needs according to g_DiscipleInfos +0xC, raw FINAL
  221, town with +0x5E8, the town's "something changed" pulse) → 163.

- The living list: head g_game +0x205BBC, count +0x205BC0, next = Living +0xA4. Living::Living 0x5EBEC0 inserts every
  new Living at the head (0x5EBFBD..0x5EBFD4), so the list runs newest first with villagers, animals and creatures mixed
  (the Creature ctor 0x473B90 calls Living::Living at 0x473B9F). Living::ToBeDeleted 0x5EC0A0 unlinks a Living and zeroes
  its next (0x5EC11A). Living::RemoveFromGame 0x5EE560 (vt +0x7DC) also unlinks, but nothing in W120 calls that slot.
- Living::ProcessLiving 0x5EC810 reads next (+0xA4) before each turn (0x5EC8A8). As a result: a Living unlinked by an
  earlier turn is skipped; one unlinked during its own turn does not stop the loop; if the next one is unlinked during
  the current turn, it still takes its turn (it is still in memory) and the loop then ends (its next is 0); a Living
  created during the loop sits at the head and first runs on the next turn.
- Per Living: Pos (+0x14) is copied to +0x2C (0x5EC8AE..0x5EC8C1). Then a villager runs ProcessReaction (0x5EC8C6) and
  ProcessState (vt +0x620, 0x5EC8CF); an animal runs Animal::ProcessState 0x417EE0. After the loop, 0x5EC8E1 writes
  g_game +0x2502CC = 0x14 (reader unknown, see [Pending](#pending)). Before the loop, 0x5EC811..0x5EC898 sets the
  creatures' Creature3D +0x6405C from [0xBE026C] = (CreatureList count [0xC5FCFC] < 3) + 1.
- The 18 villager rows whose state function reaches MobileWallHug::MoveTo 0x60AF20 (directly, or through
  Living::MoveToPos / MoveToObject / PerformDance 0x5EF9F0): 1 MOVE_TO_POS 0x5EC270, 2 MOVE_TO_OBJECT 0x5EC2C0, 3
  MOVE_ON_STRUCTURE 0x5ECD00, 5 IN_DANCE 0x5EDAD0, 29 MOVE_ON_PATH 0x5EDDE0, 47 FORESTER_MOVE_TO_FOREST 0x75F7D0, 60
  0x76C680, 66 RESTART_WORSHIPPING_CREATURE 0x76C3C0, 71 FOOTBALL_WALK_TO_POSITION 0x763120, 90 WORSHIPPING_CREATURE
  0x76C7C0, 140 MOVE_TOWARDS_OBJECT_TO_LOOK_AT 0x766C60, 153 DANCE_FOR_EDITING_PURPOSES 0x759930, 154 MOVE_TO_DANCE_POS
  0x759990, 167 MOVE_TOWARDS_CREATURE_REACTION 0x767F70, 193 DANCE_BUT_NOT_WORSHIP 0x759960, 203 DANCE_WHILE_REACTING
  0x766130, 222 FOOTBALL_MOVE_TO_BALL 0x75F380, 230 ARTIFACT_DANCE 0x761510. No other villager state walks. Direct
  callers of MoveTo: Living::MoveToPos 0x5EC280, Living::MoveOnFootpath 0x5EDDE9, FootballWalkToPosition 0x763120,
  FootballMoveToBall 0x75F415, MoveTowardsCreatureReaction 0x767F88.
- Living::MoveToPos 0x5EC270 does nothing (returns 0, no MoveTo) while the villager is in a hand (+0x24 & 4,
  0x5EC273..0x5EC27C).
- A villager marked for deletion during CheckEveryTime still runs its state function that turn: there is no test between
  0x750049 and CallState 0x750050.
- CheckEveryTime info offsets: processChecksEvery = GVillagerInfo +0x2DC; the hurt threshold damageThresholdToGoHome =
  +0x35C (life < it, strict); hungryForFood = +0x2C0. The "reacting to food" exception reads rows 19 / 20 (0xDBB2E4 /
  0xDBB3F8). The disciple check is at 0x750643.
- Town +0x5E8, the town's "something changed" pulse, is read by CheckEveryTime's disciple check, Town::Process and
  DiscipleNothingToDo 0x7540D8; its writers are under [Town lists and queries](#town-lists-and-queries).
- Villager::IsHungry 0x752600: food <= hungryForFood (+0x2C0). GetGameTurnsSinceLastChecked 0x750670 = turn − +0xEC
  (unsigned); SetGameTurnLastChecked 0x7506A0 sets +0xEC = the turn.
- POWER 0x75BB60 = 1 − min(x, 1)³, the cube computed by a loop of two fmul (0x75BB81..0x75BB8B). GetDesireForFood
  0x75BB50 = POWER(food).

## State changes

Codes: 1 done, 0x2E the exit rejected (nothing changes), 0x2F the entry rejected.

- **Villager::SetTopState(s)** 0x752010: if `CanPauseForASecond(s)` (TOP ≠ 239, row with pause, no script):
  `x = 1 − life` (life·0.5 if poisoned), and if `GameFloatRand(1) − 0.5·x³ < pauseForASecondChance (0.01)` →
  SetupPauseForASecond(s) 0x76B090 = SetCurrentAndDestinationState(239, s). Otherwise, Living::SetTopState; if it gives
  0x2F, CallEntryStateFunction(163).
- **Living::SetTopState(s)** 0x5F28E0: exit(s) → 0x2E; `out` = CallOutofAnimationFunction(s) (0x5F2900); entry(s)
  → 0x2F; SetStateSpeed (0x5F291B, unconditionally); if `out ≠ −1`, SetAnim(out); otherwise, SetStateAnim 0x5ECB10 and
  CallIntoAnimationFunction(s) (0x5F2947).
- **Living::SetCurrentAndDestinationState(c, d)** 0x5F2980: the same, but the exit (0x5F298B), the exit clip
  (0x5F299C) and the entry clip (0x5F29EB) receive **`d`**; only the double entry (vt +0x908) receives both.
- **CallExitStateFunction(s)** 0x752320: the TOP's exit and, if the final state is another one, also its own; 1 only if
  both give 1.
- **CallEntryStateFunction(s)** 0x7523D0: `entry[s](final, s)`; 1 → SetState(0, s). The double one 0x752440: that of
  `c`, then `entry[d](previous final, d)`; 1 → SetState(1, d).
- **Villager::SetState(i, s)** 0x753690: PREVIOUS does not store a state with 0x10; the old state of any index,
  if it is final, leaves the town modifiers, and the new one enters (also in PREVIOUS: literal quirk); setting TOP
  first clears FINAL (with its adjustment) and sets +0x90 to 0. It does not touch the counter +0x58, nor clips, nor speed.
- **AdjustTownModifier** 0x753560: `town.desire.doingNow[d] ±= amount` and `doingNowCount[d] ±= 1` (TownDesire
  floats, town +0x510 / +0x554).
- **SetupMoveToWithHug(pos, final)** 0x5F2890: SetCurrentAndDestinationState(GLivingInfo +0x124 `moveState`, final)
  (0x5F2894..0x5F28AF; the 63 villager rows of info.dat have 1 MOVE_TO_POS) and, only if it gives 1, the walk
  (WallHug). Used by fire and teleport (`VillagerMove.h` forwards to VillagerCore).
- **Arrival of MOVE_TO_POS** (Living::MoveToPos 0x5EC270): MobileWallHug::MoveTo == 0xA → SetTopStateToFinal
  (0x5EC28E) = Villager::SetTopState(FINAL): pause roll, exit of MOVE_TO_POS (ExitMoveToPos 0x5EDDA0,
  CircleHugInfo::Reset, unported: counts as 1) and exit / entry of the FINAL. MoveTo 0x60AF20 only gives 0xA in ARRIVED
  (0x60AFC0, if AreWeThere(0)) and in FINAL_STEP (0x60AF6C), and both first put the object at the destination (Pos =
  destination, MoveMapObject vt +0x55C); otherwise it returns 0 / 1 / 6 / 7 and Living::MoveToPos does nothing: **there is
  no "abandoned" walk**. In openblack (`WallHugMoveToResult`): it arrives when it has the FinalStep (or Arrived) mark and is
  already at its destination (PathfindingSystem sets it with AreWeThere and places it at the destination on the next turn,
  as the original gives 0xA one turn after STEP_THROUGH sets FINAL_STEP). Previously a walk that the PathfindingSystem
  abandoned (without marks) counted as an arrival wherever it was: that is how the teleport villagers went round in
  circles 1 ⇄ 201 without ever reaching the stone.
- **The unported cases of the PathfindingSystem** (destination inside the circle being hugged, TODO #864, and the step
  from one circle to another, TODO #865; in the original MoveToCircleHugCircleSquareSweep<0/1> 0x614C40 / 0x6159F0):
  `AbandonMove` no longer drops the walk, it stays in STEP_THROUGH (0x60B02A: straight to the destination, without
  obstacles) **(approximate)** and the villager arrives through AreWeThere. In addition the PathfindingSystem's ARRIVED
  was inverted (it left ARRIVED just when it had arrived; 0x60AFC0 leaves when it has **not** arrived).
- The invented idle walk (radius 40, FINAL 209) **no longer exists**: 163 is `Villager::DecideWhatToDo` 0x7515C0
  (see [Deciding what to do and leisure](#deciding-what-to-do-and-leisure)). Only the debugging tools ("Move To Point") prepare walks with
  FINAL 0, which on arrival return to 163 via the compatibility path (the original would do SetTopState(0)).
- CallOutofAnimationFunction 0x756620 / CallIntoAnimationFunction 0x756590: see [animation.md](animation.md)
  (`VillagerCallOutOfAnimation`, `VillagerApplyStateClips`).

- Villager::GetFinalState 0x751DD0 (vt +0xB04): the TOP if its row is final (0xDB9E84, file 0x0C), else the FINAL.
- Living::SetTopState: when the entry is refused (0x2F), the transition flags and the out-of clip already set stay as
  they are (literal).
- Villager::SetCurrentAndDestinationState is 0x7520E0 (vt +0x8DC); a 0x2F there also runs CallEntryStateFunction(163)
  (0x752105).
- PlayAnimThenSetState(s) 0x5ECAC0: if CallExitStateFunction(s) is 0, nothing happens (0x5ECAD3). Otherwise
  CallEntryStateFunction(23, s) runs (the double entry, vt +0x908): TOP = 23 WAIT_FOR_ANIMATION, FINAL = s, with no clip
  of its own.
- SetupWaitForCounter (0x76B060; 0x76B06E..0x76B086): SetCurrentAndDestinationState(57 WAIT_FOR_COUNTER, final). Only
  when that gives 1 is the state counter +0x58 set to the count; otherwise it returns 0 and leaves the counter alone.
- Living::SetupMoveToPos 0x5F2830 (also Villager::SetupMoveToPos 0x763800): the walking state is the byte at GLivingInfo
  +0x124, or 3 MOVE_ON_STRUCTURE when +0x24 & 0x80. It calls SetCurrentAndDestinationState(walk, final), and if that
  gives 1, SetupMobileMoveToPos.
- MobileWallHug::SetupMobileMoveToPos 0x60AAD0: +0x80 = the goal, InitStepsXZ, the object leaves the circle-hug lists,
  then AreWeThere(0) -> +0x5E = 1 ARRIVED (0x60AB91); otherwise CircleHugInfo::Reset, +0x78 = 1 and +0x5E = 0xB
  STEP_THROUGH.
- MobileWallHug::InitStepsXZ 0x60BFA0: a = GUtils::GetAngleFromXZ(Pos, goal) 0x74D240; SetTowardsAngle(a) (vt +0x868,
  a villager's 0x473E40 = SetGameAngle 0x60DA90, with no turn limit); step x = (COS[a] 0xC31E14 × (+0x5A >> 4)) >> 12,
  z the same with SIN 0xC31614, y = 0.
- AreWeThere (0x60AD64..0x60ADB0): R = speed (+0x5A as a float) + r; arrived when dx² + dz² < R² (strict).
  AreWeThereAtDestination uses GetDestPos (vt +0x860).
- Living::LookAtPos 0x5EC550, one turning step per call: at most 0x40 / 0x80 / 0x100 (modes 0 / 1 / 2) of the
  2048-unit circle, or the mode value itself for any other mode. The short way: d > 0 turns + if d < 0x400, else −; the
  result is & 0x7FF. It returns 1 and snaps to the target when |d| < the step. LookAtObject 0x5EC520 = LookAtPos(the
  object's position), 1 for a null object.
- MobileWallHug::SetGameAngle 0x60DA90 stores +0x5C and calls Object::SetYAngle 0x639260(ConvertGameAngleTo3D
  0x74DC50(a)). SetYAngle 0x60DAC0 (Villager vt +0x524) does the reverse with ConvertAngle3DToGame 0x74DC30.
  Object::GetYAngle 0x402500 (vt +0x508) reads +0x4C.
- Villager::StorePreviousState 0x763470 writes PREVIOUS raw (LivingAction::SetState 0x5ECC90, no town modifiers). A
  passing final state (one never kept in PREVIOUS, file 0x10, or a reactive one, 0xB8) keeps what PREVIOUS had.
- Living::ExitNoChangeState 0x768780 (vt +0x928): 1 when next is script-interruptable (file 0x1C), is IN_HAND, or has
  the same exit; else 0.
- Villager::IsAvailable 0x751D50 (vt +0x2C): 0 while being deleted (+0xA & 1) or when GetFinalState is 14 DYING; else
  1. Villager::IsFunctional 0x751CF0 (vt +0xD4): IsAvailable and TOP not 13 / 14.
- Villager::IsVillagerAvailable 0x752290: not controlled by a script (+0x25 & 4), IsAvailableForStateChange
  (GameThingWithPos 0x401A30: not in the hand) and GetVillagerAvailableState & 1.
- Living::IsDead 0x417270 is also true when the villager is not functional (vt +0xD4, 0x417287), not only for status &
  1 or TOP 15.
- Living::IsDeathState 0x5ECAA0 (vt +0x970): 13..17, unsigned.

### Compatibility path (`LivingActionSystem::VillagerSetState`)

| call | what it does |
|---|---|
| TOP, `skipTransition = false` (worship) | exact `villager::SetTopState` |
| TOP, `skipTransition = true` (hand, physics, animals, the arrival of a walk with FINAL 0, LANDED, Gui) | if it is the same state, nothing; otherwise, exact `villager::SetState(0, s)` + clips and speed. **(approximate)**: the original goes through SetTopState with EnterInHand / ExitInHand..., not ported |
| FINAL / PREVIOUS | exact `villager::SetState(i, s)` |

## States 85 and 239

- **85 CREATED** (Villager::VillagerCreated 0x753DD0): `v = +0x58; +0x58 = v − 1; if v == 0 → +0x58 = 0 and
  SetTopState(163)`. It moves to 163 on call number counter + 1 (1..501 turns).
- **239 PAUSE_FOR_A_SECOND** (0x76B0B0): when the clip ends (IsReadyForNewAnimation(1)), SetTopStateToFinal 0x5ECA80
  = SetTopState(FINAL): the exit and the entry of `s` run again; it does not pause again (TOP == 239). While it lasts,
  GetFinalState() = s.

## Speed (Villager::SetStateSpeed 0x753760)

It is called unconditionally in both SetTopState; the skips are its own: nothing changes if the villager is controlled by
a script (GameThingWithPos +0x25 & 4, 0x753766) or if it is dancing (Living::IsDancing 0x5ECC10, 0x753772: the DanceGroup
of Living +0xD8; in openblack `WorshipVillager::dancing` or TOP == IN_DANCE, **(approximate)**). Thus the villager that
enters 60 WORSHIPPING_AT_WORSHIP_SITE after FindDanceGroup keeps its speed. Afterwards it takes the final state and SetSpeed
0x750ED0 (formulas in [animation.md](animation.md)). The age is GetAge (unsigned) and is compared **unsigned** with
grownUpAge (0x750F26, `jae`) and oldAge (0x750F87, `jbe`); the differences are loaded as an unsigned qword, ×0.2×0.1,
maximum 0.4. The adult subtracts
`GetDesireForFood()·0.1` (0x750FDB, POWER 0x75BB60 = `1 − min(food, 1)³`), `life·0.1` and 0.2 if it is a woman.

- The four speed branches (0x7538D3..0x753B40): the two wounded ones are (GameFloatRand(0.2) + 0.4) × speed4 and
  (GameFloatRand(0.25) + 0.5) × speedDefault. Emergency: when the villager's town is in a state of emergency
  (Town::IsInStateOfEmergency 0x747970), (GameFloatRand(0.5) + 0.75) × speedFleeing (GVillagerInfo +0x108). Then the
  normal branch. Every branch is multiplied by the map scale m.
- The food speed-up (× info +0x39C) applies to the emergency and normal branches only. The two wounded branches jump past
  it (0x7538EB / 0x753934 -> 0x753B36).
- Info offsets of the normal branch: BaseForTownNeedsSpeedMod +0x36C; DivisorForTownNeedsSpeedMod +0x370 (2.0 in
  info.dat); SpeedModWhenFullLoadOfWood +0x374; SpeedModWhenFullLoadOfFood +0x378. Without a town the town-needs term is
  1. Order of the product (0x753AFD..0x753B0D): speed group entry × foodF × woodF × T × m, each rounded to float.
- A TRADER disciple (type 9, 0x7539FE) uses MaxTraderFoodCarried / MaxTraderWoodCarried (+0x274 / +0x278) in the load
  factors. Both load factors clamp at 0.75 from below (0x753AA1, 0x753AD1), food too (not +0x378).
- SetSpeed 0x750ED0 is then called with the factor flag 1 (0x753B36..0x753B40).

### Script properties and facing

- `SET_PROPERTY` Speed (022, 0x70E9F6) → `Villager::SetSpeedInMetres` 0x753110: `whole = ftol(m / 10 × 65536)`
  (0x74DCE0), then `SetSpeed(whole, 0)`: with a second argument of 0 the factor stays 1 (0x750ED4..0x750EE3), so the
  script's speed is the speed (clamped to 0..0xFFFF). It lasts: the state speed skips a villager controlled by a script.
  openblack: `ecs::SetVillagerSpeed(e, whole, applyFactor)` / `SetVillagerSpeedInMetres` (`ECS/VillagerSpeed.h`).
- `GET_PROPERTY` Speed (0x70DE4E): in the physics the length of the body's velocity (po +0x104), a dead Living 0, else
  `GetSpeedInMetres` 0x60C070. Scale (8) is the Transform's uniform scale (Object +0x50). Age (16): GET `GetAge` as a
  float, SET `Villager::SetAge` 0x7528C0 (`villager::SetAgeAndScale`: the meshes when grownUpAge is crossed, then
  InitialiseScale and SetScaleForAge with their GameFloatRand draws). On a thing that is not a villager or an animal
  these stay **(pending)**.
- `SET_FOCUS` (034, GScript::SetFocus 0x6F90B0) → `Object::SetFocus` 0x6393A0: `SetYAngle(LH3DMath::GetYAngle
  0x841260(Pos, target))` = atan2(dz, dx), + 2π when negative (`affine::GetYAngleBetween`, was `lh_matrix`). An instant snap: no
  focus is kept, the next step or LookAtPos overwrites it. openblack: `living::SetFocus` (`ECS/LivingAngle.h`).
  A container (a flock or a town) gets it on every member (SetFocusLoopFn 0x6F9090); a creature **(pending)**.
- OVERRIDE_STATE_ANIMATION 068 (GScript::OverrideStateAnimation 0x6F9F50): a clip <= 0 or >= 441 logs "Invalid
  animation forced" (0xC0D69C) and carries on (0x6F9F80..0x6F9F93). The clip is AnimPack[clip], with entry 0 outside
  [0, count) (0x6F9FD3..0x6F9FE7). The thing is read at 0x6F9F78; no thing → nothing said (0x6F9F9D); not a Living →
  0x6F9FC0. When the 3D object's anim (vt +0x184) differs it is set (vt +0x180) with cycle time 0 (vt +0x188)
  (0x6F9FEA..0x6FA00D); DataForScriptRemind (Living +0xB0, 0x6FA013..) gets +0x3C = clip, +0x44 = 0, read only by
  EnterPlayAnim 0x768914's resume. The clip lasts until the next `Living::SetStateAnim` 0x5ECB10.
- GET_PROPERTY Speed (0x70DE4E..0x70DF5F), the other cases: a Living in the physics WITHOUT a PhysicsObject returns
  GetSpeedInMetres with no IsDead test (0x70DEB4); any other thing outside the physics has no speed. The velocity length
  is computed as (z·z + y·y) + x·x, then fsqrt (0x70DEB6..0x70DEE0).
- GET_PROPERTY Age (0x70DFFD): GetAge loaded as an unsigned qword and stored as a float. On a thing that is not a
  Living, the caller logs "Not used on non living objects". SET_PROPERTY age: a Living (the cast at 0x70EB72) →
  `SetAge(ftol(val))` (vt +0x8D4, 0x70EB65): Villager 0x7528C0, Animal 0x4179C0; else "Cannot Set Property".
- GET_PROPERTY scale: 0x70DD4C, `GetScale` (vt +0x120). SET_PROPERTY scale: 0x70EA76, `SetScale` (vt +0x124,
  `Object::SetScale` 0x639200: +0x50 and the matrix when it changes).
- GScript::SetScriptState 0x6F82E0 on a villager (0x6F831E..0x6F836F): only when the villager is available and
  IsObjectInMap. StorePreviousState (vt +0x8EC), CallExitStateFunction(s), then CallEntryStateFunction(s), both results
  ignored; SetAnim(1); +0x58 = 0.
- 200 SCRIPT_PLAY_ANIM: with +0x120 == 0 nothing happens; otherwise +0x120 −1, then PlayAnimThenSetState(plays left ?
  200 : 4 IN_SCRIPT). IsScriptAnimationComplete (0x7689D3..0x7689F3): TOP 23 -> 0; TOP 200 -> +0x120 == 0; else 1.
  GetTopState is 0x5F27F0.
- EnterInScript (0x5ED7F7..0x5ED989) / EnterPlayAnim (0x768858..0x768957): the same entry -> 1. Without a
  DataForScriptRemind (Living +0xB0), or when its +0x44 is not next, also 1. Only with matching remind data do they
  resume the remembered walk or clip and return 0x23.
- ExitInScript 0x5ED9C0: CircleHugInfo::Reset; IsDancing is read and unused; IsScriptState(next) -> 1. Otherwise it
  creates the DataForScriptRemind if missing, sets its +0x44 = GetFinalState, calls KeepThatInMind 0x5EF1D0, and returns
  ExitNoChangeState(next).
- `SET_FOCUS` pops z, y, x (0x6F90C1..0x6F90EB), then the thing (0x6F910B). A container (0x6F912F): the point at g_game
  +0x250090 +0x40, the loop with SetFocusLoopFn 0x6F9090, no SetControlledByScript. A creature: SetControlledByScript(1)
  (vt +0x440) first (0x6F91A9); a Living at 0x6F91BC.
- `SET_TOWN_DESIRE_BOOST` 0x6FE650 pops the boost (0x6FE660), the desire (0x6FE674), the thing (0x6FE686); the re-sort
  of order 1 is fn_746140 (0x6FE73E).

### Script flocks and containers

- `ECS/Flocks.h` is the original's Flock class for any Living. `components::Flock::members` is the original's member
  list **reversed**: front() is the tail, the leader (IAmFlockLeader 0x5ECF70); back() is the head. AddMember
  fn_0052FA50 first takes the living out of its old flock (which is deleted when emptied), then inserts it before the
  first node from the head whose Living +0xD4 is >= its own. +0xD4 is 0 (Living::SetToZero) except after AddLeader
  0x52FC70 (the tail's + 1, 5 for an empty flock), so a normal member becomes the new head and a leader the tail.
  Living +0xB8 / +0xD4 are `Villager::flock` / `flockOrder` (and `Animal::flock` / `flockOrder`).
- A villager leaves its flock when it dies (Living::Dead 0x5EC403) and when it is deleted (Living::ToBeDeleted
  0x5EC166).
- State 27 MOVE_IN_FLOCK (Living::MoveInFlock 0x5ECDB0, `ECS/Villager/VillagerFlock.h`): inside the domain nothing
  happens. Outside it, the leader goes to a random point of the domain; another member that is farther than
  flockDistance from the leader goes to a random point near the leader, unless that point is outside the domain while
  the leader is inside. The random point is Living::CalcRandomPos 0x5ED080 (`ECS/LivingPos.h`, shared with the
  animals; a villager's two vt tests are 1).
- `ECS/ScriptContainers.h`: towns and flocks are the containers (a dance **(pending)**: openblack has no Dance class).
  FLOCK_CREATE 036, FLOCK_ATTACH 037 (a flock target: merge, living + flock, or a new flock for two livings; a town
  target: RemoveVillager / AddVillagerToTown, pushes the town), FLOCK_DETACH 038 (an animal gets a flock of its own,
  info +0x10 == 4 = OBJECT_TYPE ANIMAL; a villager is only removed; no obj: a random member, not the leader; the
  member's state is not changed), FLOCK_DISBAND 039 (a controlled member goes to IN_SCRIPT), ID_SIZE 040, CALL_IN 055
  (a town: its abodes newest first, each one's inhabitants, then the homeless; a flock from the head), CHANGE_INNER_OUTER
  056, and the container loops of SET_SCRIPT_STATE 017, SET_SCRIPT_ULONG 020 and SET_FOCUS 034.
- Living::GetFlockPos 0x5ECF60 = the flock's +0x14 (the domain centre); Flock::GetFlockPos 0x530570 = the tail's
  (leader's) Pos.
- MOVE_IN_FLOCK details (0x5ECDB0..0x5ECF19): inside the domain (PosWithinDomain(Pos, 1.0)) it returns 1. The leader:
  CalcRandomPos(domain centre, 0, radius), SetupMoveToPos(p, 27), 0x23. Another member: if its distance to the leader is
  at most flockDistance it returns 0; otherwise CalcRandomPos(leader's Pos, 0, flockDistance). If p is outside the domain
  while the leader is inside it returns 0, else it walks there and returns 0x23.
- Living::CalcRandomPos 0x5ED080(centre, rMin, rMax): two tries. Each try draws a = GameFloatRand(2π) and r =
  GameFloatRand(rMax − rMin) + rMin (always called), then walks a 25-cell spiral from AddDistanceFromAngle(centre, a, r)
  (0x74D510 inline). The first cell that is in bounds, does not collide with the info's collide type (+0x11C) and passes
  both class tests wins. Failing that, the centre if it passes the turn test (vt +0xB3C, 0x5ED1E4..0x5ED218), else the
  Living's own position (fn_0074D650's step is 0 after `sar 4`). For a villager both tests (IsPosValidForTurnAngle
  0x473EE0, IsPosValidForMapCellExistance 0x417260) return 1.
- `IsScriptContainer` (vt +0x3F8) is 1 for Town, Dance and Flock (0x52F880).
- The loop table 0xC0C73C (row = script type × 24): TownLoop 0x6F7850 (`Town::FindVillager` 0x73E600), DanceLoop
  0x6F78E0, FlockLoop 0x6F78B0 (`Flock::FindLiving` 0x530510). Each member goes through 0x6F7800
  (SetControlledByScript(1) first when g_game +0x250090 +0x3C); a type without one: "No Loop function for type".
- `Town::FindVillager` 0x73E600's order: the abodes of +0x754 (next +0x9C, newest first), each one's +0xA0 list (next
  +0xE4), then the homeless +0x768 (next +0xE4).
- `FLOCK_CREATE` (`GScript::CreateFlock` 0x6F21F0): `MapCoords(pos)` (0x6F2243), `Flock::Flock(pos, GFlockInfo
  0xC5E624, a player, 0xABA52)` (0x6F2287..0x6F2293), `AddScriptGameThing(f, 1)` (0x6F22C2).
- `FLOCK_ATTACH` (`GScript::FlockAttach` 0x6EF3C0): pops the leader flag, the target, the obj; "Id deleted to attach"
  (0x6EF4B4) / "Id deleted to attach to" (0x6EF4AD); a dance target (0x6EF426): SetControlledByScript(1), fn_006F0320; a
  flock target: SetControlledByScript(target, 1) (0x6EF468) then fn_006EF4E0; a town target: fn_006EF8A0 (0x6EF496);
  else "Thing not added to id" (0x6EF4A6).
- fn_006EF4E0(obj, target, leader): the obj a flock or a Living (0x6EF518..0x6EF56B); the target a flock: the two merge
  (`fn_00530210(target, obj)` 0x6EF601, the target pushed) (0x6EF570..0x6EF5DF); a Living + a flock
  (0x6EF5E3..0x6EF697): AddLeader (0x6EF5F6) or AddMember (0x6EF621, "Living already in Flock"), then
  `SetScriptState(living, 27 MOVE_IN_FLOCK)` either way (0x6EF63A), the flock's id pushed and the living's reference
  incremented.
- fn_006EF4E0, two livings (0x6EF698..): a new flock at the target's Pos (+0x14) (0x6EF700..0x6EF70B),
  `AddScriptGameThing(f, 1)` (0x6EF71D), `IncrementScriptReference(target)` (0x6EF729); the id is pushed before the
  members are added (0x6EF742); AddLeader + Increment(obj), no state 27 (0x6EF752..0x6EF764); the target too, with a
  second reference (0x6EF793..0x6EF7C4); failure "Thing not added to Flock" (0x6EF7E3).
- fn_006EF8A0(obj, target, town) pushes the target (the town's id): a container's members through fn_006EF810
  (0x6EF8C6..0x6EF912); a single villager (0x6EF9A0..0x6EF9BF); a SpellDispenser vt +0x8F0 with the town (0x6EF938); exit
  0x6EF98F. fn_006EF810: `RemoveVillager` 0x73E210 and `AddVillagerToTown` 0x73A090 (the town at g_game +0x250090
  +0x50); a non-villager logs "JONTY-Trying to add non villager to town" and pushes a 0 (0x6EF860).
- `FLOCK_DETACH` (`GScript::FlockDetach` 0x6EF9E0): pops the container, then the obj id; a flock → fn_006EFB80
  (0x6EFA2A); a dance → `DetachFromDance` 0x6F0430 (0x6EFA3C); a town → fn_006EFAF0 (0x6EFA66: `RemoveVillager` at
  0x6EFB3C, pushes 0 either way); a Living stands for its flock (0x6EFA8F..0x6EFAA9); a dead container: "From thing
  dead!" and no push at all (0x6EFAD2).
- fn_006EFB80(obj, flock): an animal gets a flock of its own, a villager is only removed (0x6EFBC0..0x6EFC0B),
  Decrement, push; no obj: a random member, not the tail when there are two or more (0x6EFC16..0x6EFC31):
  `FindScriptGameThing`, fn_0070D2F0 (its id, a slot made when it has none), Decrement, push the id (0x6EFC56..0x6EFC9B).
- `FLOCK_DISBAND` (`DisbandId` 0x6EFDC0): from the head, the next one read before each removal (0x6EFE35..0x6EFE90); an
  animal (info +0x10 == 4) with g_game +0x14 & 0x8000 clear (0x6EFE45..0x6EFE5A); DecrementScriptReference (0x6EFE73);
  a controlled one SetScriptState(4) (0x6EFE84); a dance 0x6EFE9B; a town or an abode: nothing (0x6EFF1F / 0x6EFF2D); the
  container is not deleted.
- `ID_SIZE` (`GScript::IdSize` 0x6EFF50): a flock's +0x48 (`fild qword` of the dword, 0x6EFF88..0x6EFF99), a dance's
  +0x90, a town's TownStats +0x8 + +0xC (Town +0x618 / +0x61C, 0x6EFFFD..0x6F0017), a football's; else "Cannot Find
  Flock/Dance/Town Size" (0x6F0073) and 0.
- `CALL_IN` 055 (`GScript::CallIn` 0x6F22F0): pops excluding, the container, the sub-type, the type; table 0xC0C730:
  FindInTown 0x6F7410, FindInDance 0x6F7520, FindInFlock 0x6F7500; the filter 0x6F79F0 (!IsInScript && 0x6F6FA0) when
  excluding, else 0x6F6FA0 (0x6F2364..0x6F2376). Found → `AddScriptGameThing(found, 0)` (0x6F2383); none → warning,
  push 0 (0x6F23AE); not a container → "Cannot look in object" (0x6F23A1).
- CALL_IN in a town: villagers (types 4 / 5, `Town::FindVillager`), animals (6, `Town::FindAnimal` 0x73E6E0: the town's
  +0x984), its storage pit (16, `Town::GetStoragePit` 0x73B5B0, no filter, 0x6F7442); another type logs "Looking for
  strange type in Town".
- `CHANGE_INNER_OUTER_PROPERTIES` 056 (0x70F480): pops calm, outer, inner, the obj; on a flock outer ≠ 0 → +0x50 =
  ftol(outer), inner ≠ 0 → +0x52 = ftol(inner) (0 keeps the field: `fcomp, test ah 0x40`, 0x70F4FF..0x70F537), +0x5C =
  ftol(calm) always (0x70F49F / 0x70F53C); a WeatherThing → fn_00774340 (0x70F546).

### Walking a camera track

- WALK_PATH 177 on a villager -> Living 0x5EE100 (`ECS/LivingWalkPath.h`): its own DataPath (Living +0xAC,
  `components::LivingWalkPath`), step = duration / (focus.length / (speed / 655 x 0.1)), then state 28
  MOVE_ALONG_PATH (final 4 IN_SCRIPT) and SetAnim(1). Each turn Living::MoveAlongPath 0x5EE230: a speed change
  retimes the step (speed 0 gives step 0: the walker stops in state 28), the point is the MobileObject path's
  (`SampleWalkPath`), below `to` it advances one step, turns towards the point from its Pos at the turn's start
  and moves there (relative y 0); at `to`, SetTopStateToFinal. GET_WALK_PATH_PERCENTAGE 179 is current / duration.
  An animal's or a creature's state 28: **(pending)**.
- `WALK_PATH`: IsLiving at 0x6FBBD9 → 0x5EE100(track, 4 IN_SCRIPT, from, to, forward); else "Thing is invalid for
  move path" (0x6FBC2E).
- The walk turns towards the point only when it is more than sqrt(0.001) from the turn-start position
  (0x5EE423..0x5EE489). The retimed step is 0 when the length per turn is not above 0.01 (0x5EE245..0x5EE30D).
- 0x5EE4F0(v), the "walk path reached" test: 1 without a DataPath (+0xAC) or when GetWalkPathPercentage >= v.
- The MobileObject path: the start sets current = duration × from (0x60776B, `fild` of the duration); the sample is
  ftol of current (forward) or of duration − current, clamped to 0..duration (0x6077A2 / 0x6077F4); the position goes
  through MapCoords, fixed point ftol(x × 6553.6) (0x6078A3), read back as x / 6553.6 by Game3DObject::SetPosition
  0x63B6BD; a finished walk leaves the list without moving that turn (0x607900).

### Released from a script

- RELEASE_FROM_SCRIPT 159 and the task's end (`script_held::ReleaseFromScript` / `Process`, GScript 0x70D540 /
  0x70F600): a villager gets DECIDE_WHAT_TO_DO (in the physics as its previous state), then
  Villager::ReleaseFromScript 0x7531D0: out of its flock; in the physics or the hand only a villager without a town
  joins the vagrants; a dead one VillagerDead(0, .., 0, 1) and DEAD unless already in a death state (13..17); with a
  town DecideWhatToDo; else the vagrants, VAGRANT_START and VagrantStart. An animal gets WANDER and fn_0041AA00.
  A controlled container: at the task's end DisbandId and, when the script created it, its deletion; with the
  command its controlled members are released first. A container not controlled gives back its members' references.

## Deciding what to do and leisure

Code: `Villager/VillagerDecide.{h,cpp}`, `Villager/VillagerHome.{h,cpp}`,
`Town/TownQueries.{h,cpp}`, `Town/AbodeQueries.{h,cpp}`; tests `test/test_villager_decide.cpp` (13 cases).

- **163 DECIDE_WHAT_TO_DO** (0x7515C0, returns 1): town emergency (Town::IsInStateOfEmergency 0x747970, +0xF1C
  `Town::emergencyStartTurn`, written by SetInStateOfEmergency, see [the town emergency](#repairs-the-tap-on-a-home-and-the-town-emergency))
  → 242; disciple / follower (DiscipleDecideWhatToDo neutral: disciples are not ported); `SetTopState(163)`; child →
  ChildDecideWhatToDo (CheckChild, town distribution, creche neutral, → 114); CheckNeededForSomething (homeless:
  CheckHomelessMoveIntoAbode first → CheckNeededForSpecial: **worship**, civic (computes the trigger and clears
  `flags & 1`; the distribution in [Town desires and distribution](#town-desires-and-distribution)), own desires with
  threshold 0.3) → CheckTakeResourcesToStoragePit
  (→ 31) → SetupNothingToDo. Worship no longer goes at the start of 163: it is in its place (0x760013), before the idle
  branch, and is also checked from 246.
- Desires: food = 1 − min(food, 1)³, life = 1 − ((life − min(0.3, life)) / 0.7)²; the largest first, strict
  comparisons with 0. CheckSatisfySleep 0x761490 does not look at the time of day (with a home → 36).
- **SetupNothingToDo** 0x753B50: GameRand(9), table 0x753C64 = 0,1,1,1,2,2,2,2,2. Branch 0: functional home → 36;
  otherwise GameRand(100) < 10 → 36, otherwise falls through to 1. Branch 1: with a home → 245; otherwise falls through
  to 2. Branch 2: with a town, walks to GetChillOutPos (meeting point + R..10R, ±22.5° on its side, R = 0.1·GTownInfo
  +0x140) with FINAL 246; otherwise 36. It always returns 1.
- **209** returns 1 (only scripts set it). **245** GoAndChilloutOutsideHome 0x76B3F0 and **252** GoAndChilloutInTown
  0x76B590 → GetMeToMyChillOutPos 0x76B610 (far: walks to GetPosOutside(3, R/2, R/2) of the door; near and clear
  (CheckForClearArea with 1.2·radius): LookAtPos one step and 246; occupied: FindClearArea(5, 1)). **246** SitAndChillout
  0x76B4E0: entry 500 turns (+0x394), then a check every 101 calls (+0x396 = 100): emergency, CheckNeededFor
  Something, GameRand(10) == 0 → SetupNothingToDo without going through 163. SitDown clip: the 0x800 bit before the
  current clip.
- **36 GO_HOME** = DoGoingHome(37, 238): with a home, walks to the door with FINAL 37 (37 makes it go in). Without a
  home: the tent or 130. The "hurt → 36" rule of CheckEveryTime is switched on in the game.
- **114 CHILD_FOLLOWS_MOTHER** 0x7578C0: CheckChild, distribution, creche; otherwise, walks to the mother (or to the home)
  + 5 m at a random angle (GameFloatRand(2π), VillagerChild.cpp 0x39) if the point is navigable; without mother or home,
  CheckNeedNewAbode (0 for a child, so it stays in 114). Row 114 carries +0x50 AlwaysReactToTownEmergency (0xD0D208 = 0x5AC990), like 36 and
  209.
- Town: GetCongregationPos 0x7408B0 with cache `Town::congregationPos` (+0xF10; also written by
  SET_TOWN_CONGREGATION_POS); mean of the homes that are not fields (with < 3, plus the plans) and FindClearArea(130, 3, 10,
  BlocksTownClearArea); otherwise, base + 10..20 m. The list of homes (+0x754) goes from newest to oldest
  (AddStructureToTown inserts at the head, 0x7399C3..0x7399CF); that of plans (+0x9A8) from oldest to newest
  (AddPlanned appends at the end, 0x73D08A..0x73D0AD). The height of the result is that of the last one read
  (0x7409C1..0x7409DB). SET_TOWN_CONGREGATION_POS: GetScriptPos 0x718250 → MapCoords::Set 0x603280 (x, z; height 0, or
  the third field unscaled if present, 0x6032E4); the offset 0xD99724 is null when loading a land (LoadMapFeatures
  0x7180FE) and only the vortex sets it (fn_0076FA50).

- 163's disciple branch (0x7515F4..0x751680): for a disciple (0x200) or a disciple follower (0x400), a follower loses its
  disciple type (0x751618). If g_DiscipleInfos[type] +4 is set (types 1, 2, 3, 4, 6, 8), the villager creates Reaction
  0x18 (Reaction::CreateReaction(this, 0x18, GetPlayer(), 1) 0x6E3D70) and returns 1. A follower with nothing to do
  stops following (0x751666..0x751680).
- SetupNothingToDo always returns 1, so the SetTopState(36) after it in 163 (0x7516C4..0x7516D3) is dead code. A table
  entry above 8 would also give 36.
- Addresses: CheckNeededForSomething 0x75FF80 (homeless: CheckHomelessMoveIntoAbode 0x761360 first);
  CheckNeededForSpecial 0x760010 (worship CheckNeededForWorship 0x76BA60, then civic, then own desires);
  CheckNeededForCivic 0x758180 = a town and fn_7581A0 == 1; Villager::CheckNeededForTownDesire 0x757C80 = jmp fn_7581A0.
  fn_7581A0 always clears flags & 1 when there is a town (0x7581CF).
- Villager::GetOwnDesiresTrigger 0x7581E0: 0 after a tap on the home (flags & 1). Otherwise f = IsHungry ?
  GetDesireForFood : 0; l = GetDesireForLife − ownDesireThreshold (info +0x38C), when that is positive, else 0; t =
  max(f, l) + 0.5 × min(f, l); a child gets at least 0.11 (0x8CA280); the result is min(t, 1).
- GetDesireForLife 0x75BBA0 = GetLifeDesireFromLife(life) 0x75BBE0.
- Villager::CheckSatisfyOwnDesire 0x760050(t): dF = food desire − t, dL = life desire − t. Food first when dF > dL and
  dF > 0 (CheckSatisfyOwnFoodDesire 0x75BF00 = IsHungry ? ChangeStateToFindFoodToEat : 0); otherwise life first, which
  is CheckSatisfySleep when dL > 0.
- CheckSatisfySleep 0x761490: after a tap (flags & 1) with life >= the threshold -> 0. Inside its home:
  CheckWhenGoingToBed, then 119. With a home -> 36. With TOP 238 SLEEP_IN_TENT -> 1. Else 0.
- CheckTakeResourcesToStoragePit 0x7516E0: the thresholds are info +0x26C (wood) and +0x270 (food), compared signed.
- Villager::CheckChild 0x757E80: not a child -> GoHome; mother not alive -> mother = 0; hungry -> GoHome.
  ChildDecideWhatToDo 0x757EC0: CheckChild, CheckNeededForTownDesire, ChildGotoCreche, else 114; always 1.
- 114's step: 5 m = the constant 0x99A934 (10, read only here) × 0.5. The point must be IsNavigable
  (MapCoords::IsNavigable 0x603840: Collide & 2 and not & 8).
- 245 GoAndChilloutOutsideHome: R = GTownInfo +0x144 (maxDistanceFromHouseThatPeopleChillOut). The look-at point is
  door + GetPosFromAngle(angle from the home to the door, 10 R), i.e. out of the door. Villager::GetPosOutsideMyHouse
  0x753D50 = Abode::GetPosOutside(3, 0.5 R, 0.5 R); 0 without a town or a home.
- Villager::GetChillOutPos 0x753C70: angle GameFloatRand(π/4) − π/8 + the angle from the meeting point to me; distance
  GameFloatRand(9 R) + R.
- GetMeToMyChillOutPos 0x76B610: near means distance <= R. The clear-area radius is Get2DRadius × 1.2, tested with
  IsObject (FUN_00761BB0 -> vt +0x460). Its "R + 5 < distance: step 5 m towards A" branch (0x76B6FD..0x76B741) is dead
  code, because it only runs when distance <= R.
- 246's entry EnterSitAndChillOut 0x76B570: +0x58 = initialChillOutTime (info +0x394).
- SitDownAnimation 0x424210: while an into / out-of clip plays (flag 0x800), the current clip decides (367 -> 369, 370 ->
  372); otherwise GameRand(2): 0 -> 369, 1 -> 372.

### Test hooks and checks

- Hooks: `OPENBLACK_TEST_VILLAGER_FOOD="<food>[,<n>]"`, `OPENBLACK_TEST_VILLAGER_NOTHING="<r>[,<n>]"` (forces
  the next GameRand(9)), `OPENBLACK_TEST_VILLAGER_SHOT="<turn>,<png>[;...]"` (capture on that turn). The trace adds
  `decide: …`, `chill 245/252: …`, `sit 246: check -> …`, `home 36: …`, `child 114: …` and `congregation town …`.

Checked (2026-10-01): Land1, 2532 turns, all villagers: 62 SetupNothingToDo (r = 0..8: 11, 3, 10, 8, 6, 4, 7, 5,
8) → 36 × 11, 245 × 18, 246 × 33; 245: 20 "far", 17 "near and clear", 11 "occupied" (by its home, see assumption 21);
246: 274 "again" and 26 "nothing"; a single warning of 37 ARRIVES_HOME; no 163/209/245/246 without function. Meeting point
of towns 0 (1789.3, 2681.3) and 4 (2479.1, 2542.7) by the mean. Land2 with `OPENBLACK_TEST_WORSHIP="1,0.5"`, 3335
turns: r = 0..8 spread out (30..45 each), 1734 checks of 246 "again" and 185 "nothing", 10 towns with a meeting
point, worship continues (11 worshippers in 59/60).

## Town desires and distribution

Code: `Town/TownDesire.{h,cpp}`
(table, functions, Process, orders, distribution, read API, scripts), `Town/TownProcess.{h,cpp}` (Town::Process and the
player loop), `Town/TownStats.{h,cpp}`, `Villager/VillagerSatisfy.{h,cpp}` (the CheckSatisfy), `Components/Town.h`
(`TownDesire`, `DesireSort`, `TownStats` and the new town fields); tests `test/test_town_desire.cpp` (16 cases) and
a new case in `test/test_villager_decide.cpp`.

- **Order in the turn**: `town_process::ProcessPlayers` is called once per turn
  from `magic::ProcessTurnStart` (Magic/MagicLoop.cpp, slot 3 of GGame::ProcessTurn). It goes after
  `InfluenceRing::ProcessRings` (0x54E63C), so Town::Process sees this turn's rings. Afterwards come the teleport
  travellers (fn_005FCC70, 0x6496BC) and the alignment (0x6496C5), and then the dances, GlobalGameLists, the
  forests and, at the end, Living (0x54E65B). **(approximate)**: GPlayer::Process 0x6494E0 does towns, teleport and
  alignment player by player; here each step goes through all players before moving on to the next.

- **Table** (faithful): 0xDA32C8 + d·0x68, filled by crt_xc 0x744BD0: name, function (+0x10), Amount/Desired (+0x20/+0x30,
  only 5, 6, 7; only the trace 0x745EC0 reads them), the villager's CheckSatisfy (+0x40), modification (+0x50), children
  (+0x60: 2, 3, 4, 15, 16) and +0x64 (no reader). Per-desire info 0xDA2930 + d·0x90 (+0x18 trigger, +0x58 TribeMultiplier[9]).
  Names for `TOWN_DESIRE_BOOST`: fn_747270 (`_stricmp`).
- **Functions** (faithful, x87 float steps, see assumptions): Food = `Town::CalculateDesireForFood` 0x747F00 (thunk 0x747340):
  `1 − (food + 1e-4)/(5·Σ foodReqiredForDinner + 1e-4)`, with the warning `HelpSpritesLowOnFood(min(v,2) − 0.9)`
  (0x747FA0) if v ≥ 0.95 and the town belongs to the local player; Wood 0x747FF0 with S = min(R5+R6+R9+R12, 3), a =
  (craftsmen + 0.001)/(adults + 0.001) + S, **k = max(abodes/10, 1)**, B = 500k, C = 5000k and the LowOnWood warning
  (0x7481BC, min(v,2) − 1); Abodes 0x748210 (max(a, c)⁴·(1 − R9)(1 − D6)); Civic 0x748330 (PopulationWhenNeeded of
  GAbodeInfo::Find 0x405B30, the first match); For_Children 0x748430 (the player's alignment and TribalPower[4] of
  `PlayerMagic`, 1.0); To_Build 0x748640 and Repair_Town 0x7486B0 (Abode::GetDesireToBeRepaired 0x406970 with the life
  `ecs::life`); Playtime 0x7487B0 (0.1 if D0, D1, D5, D6, D9 < trigger and turn > 4000); Relaxation 0x7488C0 and Sleep
  0x748960 (`sky_type::At` and `EveningRamp` over the visual time of day of `Game`'s clock; Sleep reaches 6.25 at night).
  Protection 0x7488A0 / Mercy 0x7488B0 read Town +0xEC0 / +0xEBC, which are 0 until the aggressions (**pending**);
  For_Wonder 0x748740 = 0 without `GetBeliefInPlayer` (**pending**); Supply_Worship, For_Rain, For_Sun,
  Suppy_Workshop = 0 (literal).
- **TownDesire::Process** 0x745AE0 (faithful): +0x164, the 17 in order 0..16 (a desire that reads another of higher index
  sees the previous turn's), `CallDesireFunction` 0x745D80 (raw +0x168 = f·TribeMultiplier unclamped; desire +0x118 =
  clamp(raw·modification, −1, 1)), modifications 0x746490 / 0x7462A0 (150 = GVillagerInfo[10] maxFoodCarried) /
  0x746350 (250) / 0x746400, the two orders and, every 50 turns, `(2R0 + R1 + max(R3,R4) + max(R5,R6))/5` with
  `HelpSpritesVillagerUnhappy` (0x745C8A) if it exceeds 0.6 and belongs to the local player. The player's statistic
  (GPlayer +0xA44) goes to GameStats: `game_stats::TownDesire`, the count and the sum of 1 − average.
- **Orders** (faithful): order 1 (+0x278, value GetDesire, +0 = boosts A + script) and order 2 (+0x344, GetRawDesire,
  +0 = boost A) with the VC6 CRT `_qsort` 0x7C7E64 ported literally (CUTOFF 8, `_shortsort` 0x7C7FB8, pivot in the
  middle; it is not stable: with everything at 0 it ends up `8 1 2 3 4 5 6 7 0 9 … 16`). The tests compare with the
  exe's `_qsort` run under an emulator.
- **Distribution** `CheckVillagerNeededForTownDesire` 0x745FF0 (faithful): trigger 0 → 0.001; t = min(trigger + info
  +0x18, 1); the entries without CheckSatisfy and, for a child, those without +0x60 are skipped without cutting off; it
  cuts off (0) at the first eligible one with `TempMod(k)·valor ≤ t` and returns 1 when a CheckSatisfy gives 1. **Quirk
  kept**: `TempMod` is requested with the loop index k, not with the desire's. It is called by fn_7581A0
  (`villager::CheckNeededForTownDesire`, VillagerDecide.cpp).
- **CheckSatisfy**: Sleep as in [Deciding what to do and leisure](#deciding-what-to-do-and-leisure); Playtime 0 and
  Relaxation 0 (literal: no football, creature or artefacts); Food and Wood in
  [Food and wood jobs](#food-and-wood-jobs-reactions-7-and-12), Abodes / Civic and To_Build in [Builders](#builders),
  Repair_Town in [Repairs](#repairs-the-tap-on-a-home-and-the-town-emergency); Supply_Worship and Supply_Workshop
  return 0 (**pending**).
- **Town::Process** 0x747380 (`town_process::ProcessTown`): the turn's TownStats, +0x5E4 = 0, TownDesire::Process, every
  10 turns `worship::percentage::GetWorshipersNeeded(1, 0)` / `AdjustWorshipersWorshipping(n, 1, 0)` of the worship code
  (fn_7489F0), the pulse +0x5E8/+0x5EC and the countdown +0xF20; the original's steps are listed in
  [Town::Process step by step](#townprocess-step-by-step). `town_process::ProcessPlayers` iterates
  over `map_cells::ForEachTown` and is called from `magic::ProcessTurnStart` (see "Order in the turn" above), before
  the villagers (GPlayer::ProcessPlayers 0x54E641 goes before Living::ProcessLiving 0x54E65B). The influence (+0x5C8,
  steps 3-5 of the original: 0x7473A0 / 0x7473AD / 0x7473BD) is not called here: it is computed every turn by
  `influence::ProcessTowns` (`influence::ProcessTurn`, inside `magic::ProcessTurn`).
- **Script** (faithful): CHL `SET_TOWN_DESIRE_BOOST` (341) = GScript::SetTownDesireBoost 0x6FE650 (town, d < 17,
  −1 ≤ v ≤ 1 → +0xD4[d] = v and re-sorts only order 1; "Thing not valid!" / "Invalid Params"); CHL `GET_DESIRE` (234) =
  0x6FCCA0 (invalid d → "Invalid desire" and 0 **without popping the object**; otherwise, GetRawDesire); the map command
  `TOWN_DESIRE_BOOST` 0x7179EC writes +0xD4 without re-sorting or checking the range (Land2.txt: "Abodes" /
  "Civic_Buildings" −0.75).
- **API for other systems** (`ECS/Town/TownDesire.h`): `GetDesire` / `GetRawDesire`, `GetSortedDesires` (+0x278),
  `GetSortedRawDesires` (+0x344 = Town +0x37C value / +0x380 type, what CheckTownDesiresSFX 0x71B130 reads), `GetField`
  (+0x90 / +0xD4 / +0x118 / +0x168), `GetDesireSignificanceToVillager` 0x746660, `GetMostDesired` 0x745E50,
  `GetMostSignificantRawDesire` 0x745EA0, `CalculateDesireForFood` (with warning) / `FoodDesireValue` (without warning),
  `SetBoost`, `AlignmentTurns` (+0x410, only the alignment code). Audio's `desireTowns` / `townResourceNeeds` are
  filled from it (`ECS/AudioQueries.cpp`).
- The villagers' CheckSatisfy functions not yet addressed above: Playtime = CheckSatisfyPlaytimeDesire 0x763130 (`xor
  eax, eax; ret`); Supply_Worship = CheckSatisfySuppyWorship 0x76CC00 -> GotoStoragePitForWorshipSupplies 0x76BFA0;
  Supply_Workshop = CheckSatisfySupplyWorkshop 0x7593A0 (Town::GetBestWorkshop 0x740250); Relaxation =
  CheckSatisfyRelaxation 0x761460 = Town::SetVillagerActivity 0x73FF10, which takes the highest
  GetVillagerActivityDesire (vt +0x4C) among the football (+0xEA4), the player's creature (+0xA4C) and the town's
  artifacts (+0x994, next +0x20); best 0 -> 0, else that object's vt +0x50. The Suppy_Workshop (sic) desire function is
  0x748730; For_Rain 0x748690 and For_Sun 0x7486A0 have no CheckSatisfy; these all use ModificationGeneral 0x746490.

### Desire functions

- Town::GetDesire = +0x118 + +0xD4 + +0x90 of TownDesire, at Town +0x14C / +0x108 / +0xC4 [0x73E400]; GetRawDesire
  [0x73E420].
- Food: the available food is ftol(TownStats foodCarried) + the storage pit's food (GetStoragePit → GetResource(FOOD)) +
  that of the temporary pot +0x600, the latter with no IsAvailable test [fn_747A90, 0x747A94..0x747AC6].
- Food: the warning is evaluated on the already stored float (fst), which is why v ≥ 0.95 compares the rounded value
  [0x747F29..0x747FA0]; the 0.95 threshold is at [0x8CF000].
- Wood: the available wood is ftol(wood in building sites + wood carried) + the storage pit's + that of the temporary
  pot +0x604 [fn_747B00, 0x747B03..0x747B3C].
- Wood: B and C come from GTownInfo +0xE0 minimumWoodForDesire and +0xE4 maximumWoodForDesire multiplied by k =
  max(abodes (+0x758) / info +0xE8 numOfBuildingsForDesiredWood, 1) [fn_747BE0, fn_747B60].
- Wood: v = (1 − min(w / C, 1)) × (1 − min(w / B, 1) + a), with S = R12 + R9 + R6 + R5 summing first the raws, then the
  boosts and then the A boosts, and S < 3 or 3 (NaN keeps the sum) [0x747FF7..0x748152].
- Abodes: a = adults / (adult places +0x644 + 1e-5) capped at 1.5; c = children / (child places +0x650 + 1e-5) capped
  at 1.5; m = the larger; result clamp((1 − D6) × m⁴ × (1 − R9), 0, 1) [0x748216..0x74831B]; 1e-5 at [0x99A100].
- Civic_Buildings: for each abode number i = 0..15 the town does not have (+0x718[i] == 0), p = PopulationWhenNeeded
  (+0x1B4) of the tribe's GAbodeInfo (+0x5B8); if p > −1, (p == 0 or homeless / population < 0.2) and p ≤ population: s
  += 0.5 × (pop − p + 0.001) / (p + 0.001) + 0.5; result min(s, 1) [0x748338..0x748422].
- Civic_Buildings: with population 0 and no homeless the comparison is "unordered" and the condition passes
  [0x74837F..0x7483BA].
- For_Children: A = R0 ≥ 1 ? 0 : (R0 > 0 ? 1 − R0 : 1); B = children < child places ? 1 : 0; C = adults < adult places ?
  1 : 0.5; Q = (1 − max(D4, 0)) × (1 − max(D3, 0)); al = 0.5 × alignment³ (0 with no player); X = the player's
  TribalPower[4] (1 with no player); v = X × (1 + al) × Q × C × B × A, ×0.5 with no functional creche (+0x744), clamp
  [0, 1] [0x748436..0x7485F5].
- Playtime: the cut-off turn 4000 is `cmp 0xFA0; jbe` (strictly greater) [0x748877].
- To_Build: sum of BuildingSite::GetDesireForVillagers over the sites +0x790, capped at 1 [0x748640..0x748689].
- Repair_Town: sum of Abode::GetDesireToBeRepaired of the abodes +0x754 (no site or +0x638 filter, 0x7486CB) plus the
  GetDesireToBeRepaired of each plan +0x9A8, capped at 1 [0x7486B5..0x748726].
- Abode::GetDesireToBeRepaired [0x406970]: 0 if life > GTownInfo +0x10C thresholdToStartRepairing; 0 if it is a
  dwelling (type & 2) with nobody in it (+0xA4 == 0); otherwise MultiMapFixed [0x52ECE0]: 0 with life ≥ 1, else
  min(((1 − life) × 0.5 + 0.5) × info +0x118 DesireToBeRepaired, 1).
- For_Wonder: b × (1 − min(0.5 × (D0 + D1 + D5), 0.5)), with b = the owner's GetBeliefInPlayer [0x748743..0x7487A4;
  Town::GetBeliefInPlayer 0x73BAB0].
- Relaxation: sky = Time2SkyType(GetVisualTime); ramp = fn_557AE0(0.5 × info +0xEC relaxationMod, the same); x = max(1 −
  sky, 0); R = ramp × x with minimum 0.1 (also NaN) and maximum 1 [0x7488C4..0x74895A].
- Sleep: s = fn_557AE0(1, 0) + sky − info +0xD8 bedTimeMod; s > 0 or 0; returns s² unclipped (hence 6.25 at night)
  [0x748964..0x7489B9].
- GetVisualTime is GGameInfo::GetVisualTime = [0xBF3380] [0x5575A0].
- Supply_Worship: amount = (storage pit and worship site) ? the pit's food : 0 [0x747D70]; desired fn_73C980 via the
  worship site (fn_77B920) [0x747DD0]; the desire function is `fld 0; ret` [0x748320].
- Amount / Desired for Abodes: amount = Town +0x620 [0x747CF0]; desired = ftol(((adults + children) / +0x10 if +0x10,
  else 0) + +0x10 + 0.001) / (+0x30 + 0.001)) with signed fild [0x747D00, fn_749BE0].
- Amount / Desired for Civic: Town +0x62C [0x747D50] / +0x634 [0x747D60].

### Modifications

- GetDesireVillagerModification [0x746270] calls the entry's +0x50 / +0x54 function with d.
- ModificationGeneral: 1 − min(doingNow +0x510[d] / ((u64)(adults + children) + 1e-5 in double), 1)
  [0x746493..0x7464E2]; 1e-5 double at [0x99A0E8].
- ModificationFood: n = 150 × doingNow[d] + 1e-4 + (the storage pit's food, the pot is not read); 1 − min(n /
  (DesiredFood + 1e-4), 1) [0x7462A3..0x74633D].
- ModificationWood: the same with 250 and the pit's wood, divided by C (fn_747B60) + 1e-4 [0x746353..0x7463ED].
- 150 / 250 are read from a fixed row of the exe: GVillagerInfo[10] ("African Farmer Male", 0xDA6BE8 + 10 × 0x3A4)
  +0x264 / +0x268, into g[0xDA92B4] / g[0xDA92B8] (load_variables 0x42C9AF).
- ModificationToBuild: a = 1e-4 + Σ builders of the site (+0x634), b = 1e-4 + Σ fn_43BBD0 (maximum builders); 1 − min(a
  / b, 1); with no sites it gives 0 [0x746404..0x746480].
- GetTemporaryDesireVillagerModification(k) [0x7464F0]: 1 − min(max(+0x4DC[k] − +0x454[k], 0) / (Town +0x618 + +0x61C +
  1e-5), 1).

### Process, sorting and queries

- ProcessDesire (fn_745CA0): doingNow +0x4DC[d] < 0 (or NaN) → 0; copies to +0x454 / +0x498; Amount / Desired read as
  unsigned qword; +0x118[d] = CallDesireFunction(d) [0x745CAC..0x745D56].
- ProcessDesire adds each desire to the network checksum [0xDA2770] (GNetwork::ResetStateDebug) [0x745D5D..0x745D6E].
- CallDesireFunction: the tribe index of the TribeMultiplier is (GetTribe 0x73C840 − 0xDA57A8) / 0x1C
  [0x745DC6..0x745DFF].
- TownDesire::Process: +0x164 = (u64)(adults + children − on the way +0x5CC − worshipping +0x5C4) in u32 arithmetic
  [0x745AE7..0x745B1E].
- Average every 50 turns: (2·R0 + raw1 + boost1 + boostA1 + max(R3, R4) + max(R5, R6)) / info +0x158
  divisorForAverageDesires; the warning uses info +0x15C thresholdForAverageDesiresHelpSprites and passes avg − 0.6
  [0x745BF5..0x745C8A].
- The player's GameStats: +0xA44 +0x70 += 1 and +0x6C += 1 − average [0x745C2A..0x745C46].
- Sort comparator [0x746110]: a < b or unordered → 1, equal → 0, otherwise −1 (descending, NaN first).
- Sort 1 (fn_746140): elements {+0xD4 + +0x90, +0x118 + +0xD4 + +0x90, d}; sort 2 (fn_746190): {+0x90, +0x168 + +0xD4 +
  +0x90, d}; both with _qsort of 17 elements of 12 bytes [0x74614F..0x7461D3].
- VC6 _qsort [0x7C7E64]: after the pivot, recursion goes to the shorter part (test `higuy − 1 − lo >= hi − loguy` in
  bytes, 0x7C7F43..0x7C7F51); _shortsort [0x7C7FB8] is a selection of the maximum towards the end.
- CheckVillagerNeededForTownDesire: t = trigger + info +0x18 capped at 1, stored as float [0x746031..0x74605F]; the
  caller's IsChild is vt +0xAF8 [0x74601F].
- GetDesireSignificanceToVillager [0x746660]: max(D(d) − info +0x18, 0).
- GetMostDesired [0x745E50]: best = 0, index −1; desire[d] > best wins.
- GetMostSignificantRawDesire [0x745EA0]: if order2[0].value (+0x348) < m (or NaN) → −1; otherwise its index.
- TownNeedsSum [fn_00747150]: sums in this order the raws 13, 12, 9, 7, 6, 5, 4, 3, 1, 0 (+0x19C ... +0x168), × 0.2
  ([0x8AA3AC]), clamp [0, 1].
- GetSortedDesire [0x7465D0] = &order1[k].
- GET_DESIRE: an object that is not a town → PUSH 0 with no message [0x6FCD3C..0x6FCD48]; no object → "Object no longer
  valid" (0xC0D428) [0x6FCCF2..0x6FCD3B]; "Invalid desire" is at 0xC0CFC0.
- SET_TOWN_DESIRE_BOOST: "Thing not valid!" at 0xC0DA54; the message goes out through ScriptErrorMessage [0x6F62B0].
- The map command TOWN_DESIRE_BOOST finds the town with FindTownWithID [0x552FA0] and the desire by name (fn_747270),
  writes at [0x717A26].

### TownStats (Town +0x610)

- The original keeps them incrementally: Add(Villager) [0x7492E0] / Remove [0x7493C0], Add(Abode) [0x7498C0] / Remove
  [0x749990], Add(plan) [0x749A60] / Remove [fn_749B10], Add(site) [0x749AA0] / Remove [fn_749B50].
- Add(Villager): child → +0x0C (and +0x3C), adult → +0x08; +0x54 / +0x58 [sex] (children too); +0xE4 += info +0x2D8
  foodReqiredForDinner; +0xF8 / +0xFC += food / wood carried; disciple (flags & 0x200) → byte +0xC8[+0xF2]++
  [0x7492EF..0x7493A4].
- Add(Abode) runs only once, in MakeFunctional (+0x7C bit 1), so an abode under construction does not count
  [0x404818..0x40483C].
- Add(Abode): +0x4C += MaxVillagers, +0x50 += MaxChildren, +0x30 += both, +0x44++, wonder → +0x48++; with places
  +0x10++, +0x34 += MaxVillagers, +0x40 += MaxChildren; civic → +0x1C++; byte +0x108[abode number]++
  [0x7498C3..0x749988].
- +0x4C (free adult places) is moved later in MoveIntoAbode / MoveOutOfAbode [0x7494C0] / ChildToAdult [0x749490].
- Add(plan): +0x14, civic +0x24, wonder +0x28; Add(site): +0x18, civic +0x20, wonder +0x2C, +0x100 += GetWoodForStats
  (only if the site's GetTown is this town).
- Abode::IsCivic [0x405FF0] (vt +0x8C0): ABODE_TYPE Totem 0x14, StoragePit 0x24, Creche 0x44, Workshop 0x84, Wonder
  0x100, Graveyard 0x204, TownCentre 0x404, FootballPitch 0x1004, SpellDispenser 0x2004 (tables 0x406044 / 0x40604C);
  not Field 0x4004 nor Citadel 0x804.
- GAbodeInfo::Find [0x405B30]: walks _AbodeInfos 0xC3C690 (stride 0x1C8), tribe at +0x158, number at +0x124; the first
  that matches.

### Town::Process step by step

Town::Process 0x747380:
- 1 +0x5E4 = 0 [0x747390]; 2 pruning of building sites fn_43BD00(&+0x790) [0x747396]; 3 +0x5C8 = GetBaseInfluence
  [0x73FD40, 0x7473A0].
- 4 fn_747600 [0x7473AD]: if turn % GTownInfo +0x4C processAbodeEvery == 0, for each structure of +0x754 (newest first)
  its Process (vt +0x5FC) and, except +0x5F8, +0x5C8 += GetInfluence (vt +0x868); 5 with a player +0x5C8 ×= g_game
  +0x250078 [0x7473BD].
- 6 TownDesire::Process [0x7473D7]; 7 TownArtifact::Process [0x425FB0] of the artifacts +0x994 (fn_747780, 0x7473DE).
- 8 every 10 turns fn_7489F0: n = GetWorshipersNeeded(1, 0, null) [0x73C860]; n > 0 → AdjustWorshipersWorshipping(n, 1,
  0) [0x73C0F0] [0x7473E3..0x7473FE].
- 9 fn_747660 [0x747405]: the list +0x770 (CallState == 5 → removed and deleted; not available → removed); only the
  ctor, fn_73C710 and Town::Load write it.
- 10 football +0xEA4 if FootballEnabled [0x74740A..0x74741E]; 11 the town's spell icons +0x778 (next +0x110),
  fn_747750 [0x747428].
- 12 fn_73D850 [0x74742F]: each available desire flag +0x9B0[i]: Process and +0x58 = R(+0x5C); otherwise null.
- 13 Town::ProcessPlayerInteract [0x73DEC0, 0x747436]: Protection +0xEC0 / Mercy +0xEBC from the aggression slots.
- 14 ProcessTownRepairs [0x74743D]; 15 ProcessTownEmergency [0x747444]; 16 UpdateAttitudeToCreature [0x7437F0,
  0x74744B]; 17 temporary pots [0x747450..0x7474A0].
- 18 missionaries +0x99C: MissionaryControl::Process [0x7567E0], the unavailable ones removed, --+0x9A0
  [0x7474A2..0x7474FD]; 19 belief fold fn_4383D0(+0x798, town), unconditional [0x7474FF..0x747506].
- 20 with a player fn_4141F0(player +0x60, town): alignment from desires (fn_7466D0 with GetDesire, info +0x4C / +0x50 /
  +0x54 and AlignmentTurns +0x410; fn_414660; CAlignmentHistory::Add 0x414D40) [0x74750B..0x747523].
- 21 pulse: +0x5EC != 0 → +0x5E8 = 0; then +0x5EC = +0x5E8 [0x747528..0x747544].
- 22 +0xF20 != 0 → --; at 0 SetTownEmpty [0x741080]; with people → 0 [0x747536..0x747574]; the one that sets 50 is
  Town::RemoveVillager [0x73E2CD].
- 23 not neutral: fn_555240(+0x5C8, +0xF24): |a − b| > 0.01 → g_game +0x250174 = 1 (redraw influence)
  [0x747574..0x74759E].
- 24 ShuffleDue: ftol((u64) id +0x5B4 × 20 ([0x8C7658]) + turn) % GTownInfo +0x168 shuffleVillagersEvery == 0
  (unsigned div) [0x7475A3..0x7475E8].
- GPlayer::ProcessPlayers [0x649A20] → GPlayer::Process [0x6494E0]: first Citadel::Process [0x462D70] (+0xA48,
  0x649525), then each town of +0xA50 (next +0x75C) → Town::Process [0x649551].
- A town converted in its fold (step 19) moves to the tail of the new owner's list with next = 0 (fn_0064C090,
  0x64C0C6): the old owner's walk ends there (0x649545) and its following towns wait until the next turn; the town is
  processed again this turn if the new owner's slot comes later (always for the neutral).
- TownCentre::Process [0x743DF0]: the villager part of Abode::Process [0x743DF4], the totem's Process +0xCC
  (TotemStatue::Process 0x737F40), and Town +0x5F8 = 0 with GGame::ForceNeedUpdateInfluence [0x555270] unless the head
  scaffold of the centre's building site is still adjustable (ValidForPlaceInHand(0) vt +0x6FC == 1)
  [0x743E0B..0x743E63].
- Overrides of vt +0x5FC: Field 0x529020, TownCentre 0x743DF0, Workshop 0x7797F0 (Abode::Process at the end),
  SpellDispenser 0x722A70 (Abode::Process first, 0x722A73); Abode, StoragePit, Creche, Wonder and Graveyard use
  Abode::Process [0x404440].
- Abode::Process starts with MultiMapFixed::Process [0x52F700]: with a building site (+0x74) its Process (vt +0x100)
  [0x404443].

### Town lists and queries

- Global town list g_game +0x205C84: the ctor inserts at the head (0x739637, 0x73964D..0x739656), so the newest first.
- GGame::FindTownWithID [0x552FA0]: players in the order of GetNextPlayerAndNeutral [0x550980] (slots 0..7), each one's
  towns from +0xA50; the first with +0x5B4 == id.
- Town::GetStoragePit [0x73B5B0]: +0x30 if IsAvailable (vt +0x2C) [0x73B5BC].
- Town::GetTribe [0x73C840] (+0x10) (inferred: the same TRIBE_TYPE as +0x5B8).
- "Something changed" pulse (+0x5E8 = 1, +0x5EC = 0), written inline by: StoragePit::AddResource 0x73316D,
  InsertBuildingSite 0x73B96D, the field 0x52964E, fn_0073E440 (a villager's death), Workshop MakeFunctional 0x7797CA /
  RemoveScaffold 0x779D0E / AddResource 0x779E37, Scaffold::BuildBuilding 0x6E93AF.
- Object::GetTown (vt +0x48) by class: Villager 0x751F00 (+0x12C), Field 0x528960 (+0x118), FishFarm 0x52C450 (+0x8C),
  Abode 0x401730 (+0x98; storage pits, centres, workshops, creches, wonders, dispensers and graveyards without
  override), Scaffold 0x55E120 (+0x8C), TotemStatue 0x738480 (that of its centre, +0x7C), PotStructure 0x66EF60 (the
  structure from IsPartOfStructure 0x66DA00 = +0x78 if available), TownArtifact 0x425D60 (+0x18); the rest 0: Object
  0x419950, MultiMapFixed 0x4220A0, GameThing 0x56FF10.
- What a town's HelpSprites read (GGuidance 0x71CA60 / 0x71CAF0 / 0x71CC40): adults + children (+0x618 + +0x61C),
  GetStoragePit and its IsFunctional, the position +0x14.
- Town ctor: GBelief::Init at 0x739523 while +0x5D8 is still 0 (the neutral belief is 0 until the first fold); then
  +0x5D8 = GTownInfo +0xB8 beliefInNeutralPlayer [0x73966C] and +0x5DC = 1.0 [0x739672]; SetPlayer fn_0073A8F0
  [0x739597] puts it at the end of the owner's list (fn_0064C090).
- CREATE_ABODE / CREATE_TOWN_CENTRE with no town (cases 8 and 9: 0x7156A6, 0x7157FF) take the nearest town with
  fn_00552FF0: the first of the global list always, then distance < best, only with x / z.

### Congregation point and clear areas

- The cache +0xF10 is valid unless x == 0 && z == 0 && y == 0.0 [0x7408B6..0x7408EF].
- It uses a ring of 100 elements: the abodes that are not fields and, with fewer than 3, the plans
  [0x7408F5..0x740983]; 32-bit x / z sums read as unsigned qword [0x74099C..0x7409E3].
- With a single unread element in the ring that one is used, otherwise the town's position (+0x14), copying its three
  dwords [0x740A71..0x740ABC].
- In the fallback the distance is drawn before the angle: d = GameFloatRand(10) + 10, then angle = GameFloatRand(2π)
  (Town.cpp 0x11EC) [0x740AA6..0x740AFD].
- BlocksTownClearArea (Object vt +0x534, thunk 0x743690): Object 1 (houses, Fixed, Feature, Field, stores, centre…);
  Mobile 0 (villagers, animals, piles, pots), MobileStatic 0 (fallen tree, rock, bonfire…), Tree 0.
- Town::CheckWhenNewBuildingCreated(b) [0x741500] (from PostCreatePlanned 0x648C50): if distance(b, +0xF10) −
  Get2DRadius(b) < 7.5 ([0x8C7798]) the congregation point cache goes back to (0, 0, 0).
- Town::CheckForClearArea [0x7413D0]: walks max(ftol(0.2 r), 1)² cells in a GUtils::Spiral [0x74D7E0] from pos's cell
  (only those on the map, MapCoords::InBounds 0x6042C0); not clear if an object, except the excluded one and if it
  passes the filter, has distance − Get2DRadius < r [0x74144A..0x741475].
- Town::FindClearArea [0x7412F0]: GetIncrementSpiralSizeFromRadius(a, b) points from the start in steps of b metres;
  the first clear one goes to the result; if none, the result is left untouched (0x741390 only resets the local).

### Deviations and visible effects

- At night Sleep is the first one (raw up to 6.25, desire 1) and CheckSatisfySleep sends to 36, then into the home
  (37 → 38 → 119 → 120, see [Home, food, sleep, homeless and age](#home-food-sleep-homeless-and-age)). By day the
  distribution cuts off at Relaxation/Playtime (CheckSatisfy 0) and the villager goes on with its own desires and leisure.
- The worship code receives, every 10 turns, the worshipper adjustment from Town::Process (fn_7489F0).
- **(approximate)** TownStats is recomputed at the start of Town::Process from the entities (the original adds when
  adding and removing); same counts, the float sums in a different order. **(approximate)** all script homes
  count as functional, and `Town::storagePit` (+0x30) / `Town::creche` (+0x744) are set by script creation
  (AbodeArchetype) instead of StoragePit / Creche::MakeFunctional (the last storehouse wins; the first creche).
- The x87 chains are float steps: the game logic runs with the control word at 24 bits (fn_007DEE00, `and cw, 0xFCFF`
  at 0x7DEE0D), so each fadd / fmul / fdiv rounds to float.
- **(approximate)** the turn's influence (`magic::ProcessTurn`) is computed after the desires and not inside
  each Town::Process (the desires do not read it).
- **(approximate)** `SET_TOWN_DESIRE_BOOST` with negative d does not write (the original writes outside the array).
- **(inferred)** +0x90 is 0 in a new game (only Load writes it); TRIBE_TYPE = the `Tribe` enum for
  TribeMultiplier; "local player" = PLAYER_ONE; the list +0x770 is empty; fn_555240 (step 23) needs no call.
- Not ported (no behaviour): the network checksum [0xDA2770], the debug trace 0x7457C0 (replaced by
  `OPENBLACK_TOWN_TRACE`) and the functions without calls (0x745E80, 0x745FA0, 0x7461E0, 0x746220, 0x7465F0, 0x7466B0,
  0x7468E0).

<a id="home-food-sleep-homeless-and-age-v4"></a>

## Home, food, sleep, homeless and age

Code: `Villager/VillagerHome.{h,cpp}` (36/37/38, 119/120/121, 129, 130, 234, 238, the tent, the moves),
`Villager/VillagerFood.{h,cpp}` (CheckHungry, amounts, 117/118/212, 33/34/35), `Villager/VillagerAge.{h,cpp}` (growing up,
scale, old age, pregnancy), `Villager/VillagerResources.{h,cpp}` (what it carries and takes: the food half of the carrying),
`Town/AbodeVillagers.{h,cpp}` (the home's list, PresentAtHome, the score, Abode::Process, the Shuffle's
moves), `Town/TownVillagers.{h,cpp}` (homeless, vagrants, AddVillagerToTown, FindAbodeWithSpaceInTown, UseFood,
Shuffle); tests `test/test_villager_food.cpp`, `test_villager_home.cpp`, `test_villager_age.cpp`.

- **The night** (faithful): Sleep (16) on top → distribution → CheckSatisfySleep 0x761490 → 36 → door → **37** ArrivesHome
  0x760930 → `Villager::ArriveHome` 0x751FA0 (bit 4 of +0xE0, `Abode::presentAtHome` +0xB6 `inc`, mesh hidden by the
  −4 clip) → **38** AtHome 0x760B10 = HomeDecideWhatToDo 0x75FEA0 → CheckSatisfySleep inside → CheckWhenGoingToBed
  0x760B60 (returns 1 unless it dies of old age; once per stay, bit 0x2000) → **119** GotoBedAtHome 0x760B30 → **120**
  SleepingAtHome 0x760D70 (counter RestAtHomeTime 100; without a town it does not count) → DoSleeping 0x760DB0 every 100
  turns (+0.05 life unless poisoned; continues while Sleep is the first of order 1 or life < 0.7: from 0.4 to 0.70000005
  in 6 cycles). By day DoSleeping gives 0 → 38 → distribution or leisure → the exit **ExitAtHome** 0x761B40 of 35..38 and
  118..121 does LeaveHome (0x751FD0: bits 4 and 0x2000 off, `presentAtHome` `dec`) if the next state does not stay at
  home (info.dat row, file 0xC0). 121 WakeUpAtHome 0x760E50 = GoHome (no code sets it).
- **37** (faithful): has not arrived (AreWeThere(door, 0)) → again to the door with FINAL 37 (literal, also from 249);
  built and repaired (life ≥ 1, IsRepaired 0x4016A0) → inside; damaged (< 0.3): functional home → inside, otherwise tent
  (238); hungry (food < 0.5 strict): `SetTopState(163)` if it is not functional and inside in the same turn (literal);
  otherwise SetupBuildingObject 0x758530 (see [Repairs](#repairs-the-tap-on-a-home-and-the-town-emergency)) and inside. Without a home → 129 and 0.
- **38** (faithful): emergency → 119; CheckNeedsAtHome 0x760110 (the pregnant woman stays; threshold
  `0.9·max(GetLifeDesireFromLife(0.7), POWER(0.5))` = 0.7875, the larger, not the smaller; 0.9 for the disciple that
  ignores needs; the child goes through CheckChildActivity = ChildDecideWhatToDo, always 1); the disciple;
  CheckNeededForSomething (also the worship every turn); HomeNothingToDo 0x75FFB0 (inside, GameRand(4) == 0 → 119
  with counter 0).
- **Homeless** (faithful): DoGoingHome 0x760280 without a town → 130 (0x7604CD); more than 100 m from its town → walk to 10..35 m
  from it, on its side (FINAL the TOP); near → GetTentPos 0x7604F0 → 238, or a walk of 10..30 m. The tent: the nearest
  tree within 50 m (fn_00604AF0 with IsTree) if fn_0074C650 finds room for it (2 m from the tree, on the other side of the
  occupant; a villager in 238 or a MultiMapFixed within 4 m counts; two = full, and no other tree is tried); otherwise, 3
  attempts: free cell (`Collide & 0x19 == 0`) and no villager in 238 within 5 m in the 9 cells of a spiral that **moves
  the point**: the tent ends up at (−20 m, +10 m) from the tested spot (literal quirk). **238** SleepInTent 0x761AE0,
  **129** HomelessStart 0x761320, **130** VagrantStart 0x76A8D0 (a town of its tribe within 200 m → AddVillagerToTown and
  163; hurt → tent; otherwise a walk of 10..30 m forward).
- **Eating** (faithful): **CheckHungry** 0x75BCC0 (batch = turns·9e-5 divided by
  TribalPower[3] (player +0x74) and by the speed if it exceeds 1 and it is moving; damage 0.001 with hunger (food < 0.5,
  strict: IsHungry uses ≤) or poison: the `max(…, 1)` leaves the factor at 1; interruptions 0xD0 / 0xD4 of the final
  state's row; life 0 → STARVING, or CHANT from worship); the amount GetAmountOfFoodToEat 0x75BC20 =
  `ftol((1 − 0.3·clamp(the town's Food desire))·(float)(POWER(food)·85))` (74 with food 0.5);
  **ChangeStateToFindFoodToEat** 0x75B990 (needs 0 → 117, or 118 inside; its functional home with enough → 36 / 118; the
  storehouse —the town's or, if there is none, its home— functional with enough → 33; without a functional storehouse →
  to the delivery point with FINAL 34; if it carries something, it eats it; otherwise 0); **117 / 118** EatFoodHeld
  0x75BF20 (`eaten/toEat·1.2 + food`, clamped to [0, 1], NaN → 0; Town::UseFood 0x73B5E0 adds to `Town::foodUsed`
  +0x6F8); **GetFoodFromHome 0x75C040 takes twice** (GetResourceFrom already does PickupResource: the home loses n and
  the villager gains 2n, literal quirk); **34** ArrivesAtStoragePitForResource 0x7698D0 (takes min(what it needs, what
  there is) and returns to the door with FINAL 163; then it eats what it carries); **212** ShowPoisoned 0x75B940; **35**
  ArrivesAtHomeWithFood 0x769B30 (the housewife's, not ported). The home subtracts its food with DoResourceRemoving 0x404F60
  (the town's CallDesireFunction first, `town_desire::CallDesireFunctionNow`).
- **Age** (faithful): CheckChildGrownUp 0x751050 at 13 → bit 8 off, age 18, ChildToAdult of the home (or of the town) and
  ChildBecomesAdult 0x757F10 (mother 0, CheckNeedNewAbode, **234** GoHomeAndChange 0x761810); the adult mesh arrives at
  the exit of 234 (ExitGoHomeAndChange 0x761980 → ChangeTribeIfRequired 0x7618C0 → ChangeInfo 0x761A00), not in SetAge.
  Otherwise, it rescales every 375 turns (only the children whose check falls on those turns: gcd(9, 375) = 3,
  **(inferred)**). Old age CheckDeathFromOldAge 0x760CA0 (in the periodic check, ≈ every 800 turns, and in
  CheckWhenGoingToBed): age > 60, `n = ftol(r³·40)` (the cube, not the square), `GameRand(n)`, dies if age + d > 100:
  nobody before 63. WomanSpecial 0x752240 (the pregnancy countdown) is literal; childbirth is not ported (see
  [Pregnancy and births](#pregnancy-and-births)). The scale of the constructor and of SetScaleForAge uses the
  synchronised GameFloatRand.
- **Home and town** (faithful): `Abode::inhabitants` is the ordered list +0xA0 (the head, the most recent: decides who
  moves in the Shuffle and the partner at bedtime), `maleFemale` +0xA8 / +0xAC, `adultCount` / `adultMaleCount` /
  `childCount` +0xB4 / +0xB5 / +0xB7, `emptyTimer` +0xB0; `Town::homelessVillagers` +0x768, ordered. AddVillagerToAbode
  0x404060, RemoveAliveVillagerFromAbode 0x404340 (inside → 163; its exit does the LeaveHome; the partner is not
  touched), RemoveDeletedVillagerFromAbode 0x404220 (clears both partners), RemoveAllVillagersFromAbode 0x404560 (the
  home destroyed, Buildings.cpp → HomeDeleted → MakeHomeless), the score 0x404B40, FindAbodeWithSpaceInTown 0x73B370 (the
  newest wins ties), AddVillagerToTown 0x73A090 (the worship code's CheckAddWorshipSite with the first villager),
  CheckNeedNewAbode 0x757F90 (with percentTooCrowded 0.5, an adult alone in a home for 2 is already «too many»: it moves
  if there is something better or becomes homeless, literal). Town::Process step 4 (Abode::Process 0x404440: an empty
  built home loses 0.0001 life every 1001 processed turns, in float) and step 24 (ShuffleVillagersAroundAbodes 0x741540
  with VC6's `_qsort`, one move per call).
- **Creation by script**: 0x715A4C is CREATE_TOWN_VILLAGER (command 16, 0x715AA8..0x715AE6): it creates the villager
  at the second argument, looks for the town with FindTownWithID of integer slot 0 (N0) and calls AddVillagerToTown, which
  chooses the home. LHScriptX::ScanLine 0x7E7540 only writes the slot of an 'N' argument (atol), so it holds the id of
  the last command with an 'N' first (in the lands, the CREATE_ABODE or CREATE_TOWN just before:
  `lhscriptx::Script::IntSlot`); without that town, the one nearest to the villager's position (fn_00552FF0).
  CREATE_VILLAGER / CREATE_VILLAGER_POS (command 18, `cmp eax, 0x12` at 0x715B9B; 0x715D84..0x715DF1) look the abode up
  at A0, otherwise FindAbodeWithSpaceInTown / AddVillagerToTown, otherwise the vagrants (0x715DC0). Villager::Create
  0x74FBE0 houses nobody: the script handlers do it. openblack's rule «the home within 1 m² of the script position»
  was removed. (Which of the two openblack's slot rule is applied to: see [Pending](#pending).)
- **APIs** for other systems: `villager::IsAtHome`, `IsReachable` (0x756460: available, not at home, TOP ≠ 236; used by
  AnimalPredators instead of its test of states 13..18), `LeaveHome`; `abode_villagers::VillagersOf`,
  `PresentAtHome`, `RemoveAllVillagersFromAbode`; `town_villagers::Homeless`, `AddVillagerToTown`;
  `town_desire::CallDesireFunctionNow`.
- **(inferred)**: TribalPower[3] is 1.0 (nobody writes it); the non-villager occupant of fn_0074C650 (+0x24 & 2) is a MultiMapFixed (+0x24 & 4 of IsReachable and
  IsAvailableForStateChange is «in the hand», PlaceObjectInMagicHand 0x5FB014: `fire::traits::InHand`); 115 only by
  script; IsInScript of a home (+0x24 & 0x200) is 0; IsTree = the Tree component.
- **(approximate)**: the speed +0x5A comes from `WallHug::speed`; IsMoving = the WallHug's last step is not zero;
  TownStats recomputed in each Town::Process with `males` / `females`, and AddVillagerToTown, Town::RemoveVillager
  and ChildToAdult update adults, children and sexes immediately; the homes' influence goes in the influence code's
  hook, not in step 4; Abode::ReduceLife 0x405D90 has no entry point (Object::ReduceLife of `ecs::life` is used); the
  types of openblack's signatures stand in for the exe's type strings for the integer slots.
- DoGoingHome 0x760280 (arrive, tent): a villager already inside -> SetTopState(38). GetFinalState already equal to
  `arrive` -> 1 (already on the way). A dancing villager first leaves its dance (IsDancing vt +0x978 ->
  RemoveFromDance((flags >> 1) & 1) vt +0xB08). Homeless and near the town, the tent search starts from me +
  GetPosFromAngle(GameFloatRand(2π), GameFloatRand(8) + 2). The far walk is town + GetPosFromAngle(GameFloatRand(π/2) −
  π/4 + the angle from the town to me, GameFloatRand(25) + 10).
- SetupMoveToOnFootpath 0x5EDD20: when I stand on the object's arrive point and the target is elsewhere, a direct walk.
  Otherwise object.UseFootpathIfNecessary (vt +0x80: MultiMapFixed 0x52EEC0 / GameThingWithPos 0x570350): with a
  footpath link GFootpathLink::UseFootpathIfNecessary 0x5362E0, without one SetupMoveToWithHug (0x57037E).
- ArrivesHome 0x760930: IsRepaired is Abode 0x4016A0 (life not below 1, vt +0x884). For a damaged non-functional home,
  the tent search starts from me + GetPosFromAngle(angle from the home to me + (π/8 − GameFloatRand(π/4)),
  GameFloatRand(5) + 5).
- Villager::ArriveHome 0x751FA0 calls Abode::ArriveHome 0x405FA0; LeaveHome 0x751FD0 (only when inside: flags &=
  0xDFFB) calls Abode::LeaveHome 0x405FB0.
- HomeDecideWhatToDo 0x75FEA0: in the town's emergency the villager hides in bed (119). A disciple whose type ignores
  needs: a BREEDER (5) with Sleep first in the town's order 1 and CheckSatisfySleep -> 1; otherwise DecideWhatToDo (vt
  +0x8C8) and its result.
- CheckNeedsAtHome 0x760110: a woman with WomanSpecial == 1 -> 1; a pregnant woman -> 1 (she stays home doing nothing).
  The thresholds (D, F) are (+0x360, +0x2C0), or (+0x35C, +0x2C4) for a disciple that ignores needs. A child goes
  through CheckChildActivity 0x757F00.
- GotoBedAtHome: SetTopState(120), then +0x58 = RestAtHomeTime (info +0x24C). SleepingAtHome without a town never counts
  down: it sleeps forever (literal).
- DoSleeping 0x760DB0: poisoned -> 0 (no sleep, no healing). Life below info.life (+0x128) -> IncreaseLife(f ×
  RestAtHomeRestoresLifeBy +0x250) (Villager 0x753460 -> Object 0x637870). It goes on while Sleep is first in order 1,
  or life < DamageThresholdToSleepUntil (+0x360); then +0x58 = RestAtHomeTime.
- SleepInTent 0x761AE0: a counter that is not 0 -> −1, 1. Then DoSleeping(1) != 0 -> 1. A homeless villager tries
  CheckHomelessMoveIntoAbode. Then r = HomeDecideWhatToDo; when r == 0 or the TOP is still 238, the counter is reset to
  RestAtHomeTime and decremented.
- GetTentPos 0x7604F0: Collide(0x19) means 0x10 off the game map, 8 a fixed object, 1 water (MapCoords::Collide 0x6033B0
  = Collide 0x6033C0 & type). Between the three tries, tmp += GetPosFromAngle(GameFloatRand(2π), GameFloatRand(5) + 3).
  The 5 is drawn first because it is the last argument pushed (0x760654..0x7606A0).
- HomelessStart 0x761320: calls CheckHungry directly, which resets LastCheckTurn and so moves the phase of the periodic
  check. Then CheckNeededForSomething, then CheckHomelessMoveIntoAbode; else SetupNothingToDo.
- VagrantStart 0x76A8D0: MapCoords::GetNearestTown 0x6020E0 within 200 m, and of my tribe. A hurt villager's tent search
  starts from me + GetPosFromAngle(GameFloatRand(2π), GameFloatRand(5)), drawn d first, then a. The forward walk is a =
  yaw + (GameFloatRand(π/4) − π/8), d = GameFloatRand(20) + 10, only InBounds, FINAL 130.
- CheckHomelessMoveIntoAbode 0x761360: FindAbodeWithSpaceInTown(me, 0) -> out of the homeless list, AddVillagerToAbode,
  SetTopState(36); 1.
- MakeHomeless 0x761220 = MakeHomelessNoStateChange 0x761240 + SetTopState(129). MakeHomelessNoStateChange: out of its
  abode (SetAbode(0), SetTown(town)); no town or already in the list -> 0; out of the vagrants; at the head of Town
  +0x768, ++ +0x76C; 1.
- HomeDeleted 0x7611F0: with an abode -> MakeHomeless; else TownDeleted 0x750B50.
- CheckNeedNewAbode 0x757F90: a child -> 0 (so a child in 114 with no mother and no home stays there). A home that is not
  too crowded -> 0. No town -> VagrantStart, 1. Otherwise s = the score of the current home for me (0 without one) and
  FindAbodeWithSpaceInTown(me, s). On a find, MoveVillagerToAbode == 1 -> SetTopState(36) if IsVillagerAvailable, 1.
  Otherwise MakeHomeless unless the villager is already in the town's homeless list; 1.
- MoveVillagerToAbode 0x758080: room left (GetRoomLeftForChildren / ForAdults, signed) > 0 -> ForceMoveVillagerToAbode,
  1. ForceMoveVillagerToAbode 0x756240: same town -> AddVillagerToAbode. Otherwise the old town's RemoveVillager, then
  AddVillagerToAbode below 100 % full (GetPercentAbodeFullWithChildren vt +0x8A0 / WithAdults vt +0x89C), else the new
  town's AddVillagerToTown.
- FindPosOutsideAbode 0x753470(abode; null = its own): door + GetPosFromAngle(angle from the home to the door + (π/8 −
  GameFloatRand(π/4)), GameFloatRand(1.5) + 1.5). The 1.5 is drawn first.
- GoHomeAndChange (234) 0x761810: not at the door -> walk to the door with FINAL 234. A scale below 0.95 is reset with
  SetScaleForAge (0x76187D..0x7618A1). For the order of its state changes see [Pending](#pending).
- ExitGoHomeAndChange 0x761980: the same exit -> nothing. Otherwise ChangeTribeIfRequired(the town's tribe +0x5B8, or the
  info's +0x1F4 without a town; leaving = next's row 0xC0 == 0), then a CHANGE_HOUSE disciple (+0xF2 == 10, 0x7619D9) ->
  SetVillagerDisciple(0).
- ChangeTribeIfRequired 0x7618C0: KeepMeshWhenChangeTown (+0x388) != 0 -> nothing. Otherwise GVillagerInfo::Find(tribe,
  my number +0x1FC). With a town, Town +0x6F4 += new +0x2D8 − old +0x2D8; then ChangeInfo. When leaving the home, a smoke
  puff CreateSmokyStuff(1, 1.0, −1) 0x63A810 half the villager's height up.
- ChangeInfo 0x761A00: +0x28 = the new info. A child's three meshes all become +0x204 ChildMeshHigh (SetAge uses +0x20C
  / +0x208 / +0x204: an oddity of the original). An adult gets GetDetailMesh(2 / 1 / 0) (vt +0x60C).

### The house and its inhabitants

- Abode::IsBuilt [0x4016C0]: !(+0x58 & 2) && GetPercentBuilt (+0x5C) ≥ 1.
- Abode::IsFunctional [0x406200] (vt +0xD4): MultiMapFixed::IsFunctional [0x52EF70] (IsAvailable, IsBuilt vt +0x890
  and GetPercentRepairedForNonFunctional 0x407290 = info +0x1B8 thresholdForStopBeingFunctional < GetPercentRepaired
  0x401500 = the life) and IsBuilt again [0x406211].
- Abode::GetArrivePos [0x401770] = MultiMapFixed::GetDoorPos [0x52E370] (vt +0x864): the world door × 6553.6 with ftol
  (0x63AFF2..0x63B017, and 0); with no door point, or with x == 0 or z == 0 (0x52E3A4 / 0x52E3AC), the abode's position.
- Abode::GetPosOutside(p1, p2, p3) [0x4072E0]: door + GetPosFromAngle(Get3DAngleFromXZ(pos, door) + GameFloatRand(2π /
  p1) − π / p1, GameFloatRand(p3) + p2); the two draws in that order (Abode.cpp 0x94A and 0x94B).
- Inhabitant list +0xA0 (next = villager +0xE4) / +0xA4; AddVillagerToAbode inserts at the head [0x40415A].
- GetRoomLeftForAdults [0x404660] = info +0x174 − (u8) +0xB4; GetRoomLeftForChildren [0x404680] = info +0x178 − (u8)
  +0xB7; signed.
- Abode::IsTooCrowded [0x4046C0]: MaxVillagers 0 → 1; otherwise adults / MaxVillagers ≥ percentTooCrowded (info +0x1A0).
- GetPercentAbodeFullWithAdults [0x407050] (vt +0x89C): adults / MaxVillagers (fidiv); MaxVillagers 0 → 1.
- GetPercentAbodeFullWithChildren [0x407090] (vt +0x8A0): children / MaxChildren with INTEGER division (`div`,
  0x4070B3): it only gives 0 or 1; MaxChildren 0 → 1.
- AddVillagerToAbode score [0x404B40]: f = min(count / max, 1) (adults +0x174 / +0xB4 or children +0x178 / +0xB7
  depending on IsChild); room = 1 − f; sex = empty list ? 1 : ((1 − same_sex / list_size) + 1) × 0.5 (the list includes
  children and the villager himself if already there); result (GetDistanceModifier(d, 500) + 1) × 0.5 × sex × room; max
  0 → 0 [0x404B4F..0x404CA7]; 500 at 0x43FA0000 and 0.001 at [0x8AA3B0].
- Villager::IsMaleVillager [0x55CAC0] (vt +0x44C): info +0x1F8 == 0 with no child test; IsFemaleVillager [0x55CAE0]: ==
  1.
- AddVillagerToAbode [0x404060]: removes him from the town's homeless list (--+0x76C) [0x404079], from his old abode
  [0x4040DE..0x4040F8] and from the vagrants [0x4040FA..0x404151]; head and ++count; SetAbode [0x750DE0] (the villager's
  town = the abode's or 0); if the abode's town is not his, AddVillagerToTown and VillagerMoveIntoAbode; child
  ++ChildCount, adult MaleFemale[sex] if empty, ++AdultCount, AdultMaleCount += IsMale [0x40417C..0x404210].
- RemoveAliveVillagerFromAbode [0x404340]: counts (never below 0) [0x404360..0x4043B6], list [0x4043BC..0x404406],
  SetAbode(0), with a town TownStats::VillagerMoveOutOfAbode [0x7494C0].
- RemoveDeletedVillagerFromAbode [0x404220]: if MaleFemale[sex] is him, MaleFemale[0] and [1] to 0
  [0x404227..0x40424D]; with a town Town::RemoveVillager [0x73E210] [0x4042FC..0x404326].
- RemoveAllVillagersFromAbode [0x404560]: from the head, reads the next (+0xE4) before Villager::HomeDeleted
  [0x7611F0].
- Abode::ChildToAdult [0x404CC0]: --ChildCount if not 0, ++AdultCount, AdultMaleCount += IsMale; with a town
  Town::ChildToAdult [0x73AF50] → TownStats::ChildToAdult [0x749490] (with abode +0x4C − 1, +0x50 + 1; children − 1,
  adults + 1).
- Abode::Process: the abode has to be without adults or children, built, not in a script (GameThingWithPos::IsInScript
  0x402280: +0x24 & 0x200) and, with a town, the town not uninhabitable (+0x5F4) [0x404448..0x4044B4].
- Abode::Process: +0xB0 += 0.001 per processed turn; on reaching 1, +0x7C |= 0x40, ReduceLife(info +0x1B0
  emptyAbodeLifeReducer, no player), +0x7C &= ~0x40, +0xB0 = 0 [0x4044B6..0x404503]; the 0x40 bit only lives during the
  call (the statistic in FUN_004073F0 reads it).
- Abode::Process: the byte +0xB9 counts up to 200 (no reader found) [0x404503..0x40450F].

### Moving villagers between houses (Shuffle, step 24)

- Abode::CalculateDesireToGainMale [0x4074A0]: MaxVillagers 0 or no town → 0; otherwise (men + 0.001) / (women + 0.001)
  − (adultMales + 0.001) / ((adults − adultMales) + 0.001), with men / women = Town +0x664 / +0x668 = TownStats +0x54 /
  +0x58 [0x4074A6..0x407531].
- Abode::CalculateDesireToGainVillager [0x407540]: MaxVillagers 0 or no town → 0; otherwise (town adults +0x618 + 0.001)
  / (adult places +0x644 + 0.001) − GetPercentAbodeFullWithAdults [0x407566..0x40759E].
- ShuffleVillagersAroundAbodes [0x741540]: entries = the functional structures of +0x754 with MaxVillagers + MaxChildren
  != 0, each one {abode, m = DesireToGainMale, v = 0.5 × DesireToGainVillager} [0x741549..0x74161E]; with no entries
  nothing [0x74158B].
- They are sorted with _qsort(list, n, 12, 0x7417C0); the comparator gives −1 if |b.m² + b.v²| < |a.m² + a.v²|,
  otherwise 1 (never 0: the largest first).
- Loop of pairs i = 0 .. n − 2; the first move that returns 1 ends the call [0x74163D..0x741798].
- For each a it looks for the next c with |(c.m + a.m)² + (c.v + a.v)²| strictly less than |a.m² + a.v²|
  [0x741685..0x7416B9].
- moreV = a.v > b.v; with opposite signs of v a swap is done unless the winner (moreV ? a : b) is below 100 % adults;
  moreM = a.m > b.m [0x7416C3..0x74173A].
- Swap: moreM ? a.SwapMaleForFemaleFrom(b) : b.SwapMaleForFemaleFrom(a); otherwise: moreV ? a.TakeVillagerFrom(b,
  moreM) : b.TakeVillagerFrom(a, !moreM) [0x74173E..0x741777].
- Abode::SwapMaleForFemaleFrom(y) [0x407620] on x: the first man in y's list and the first woman in x's who are not
  inside (+0xE0 & 4); ForceMoveVillagerToAbode(man → x) and (woman → y); 1, or 0 with no pair.
- Abode::TakeVillagerFrom(y, male) [0x4075B0] on x: the first of y of that sex who is not inside →
  ForceMoveVillagerToAbode(→ x) [0x407603]; 1, or 0.

### Town, homeless and vagrants

- Homeless list +0x768 (next = villager +0xE4) / +0x76C, at the head (MakeHomelessNoStateChange 0x7612F9..0x761312);
  Town::IsVillagerInHomelessList [0x73B580].
- The unlinking from the list is inline in AddVillagerToAbode 0x404085, CheckHomelessMoveIntoAbode 0x76137D and
  RemoveVillager 0x73E259.
- Global vagrants g_game +0x205BFC / +0x205C00, at the head and without duplicates (ReleaseFromScript
  0x7532F8..0x75331A / 0x7532A2..0x7532CA); Villager::IsVagrant [0x7531A0].
- Writers of the vagrant list: CREATE_VILLAGER / _POS with no abode or town (0x715DC0), ChildBorn 0x762396, TownDeleted
  0x750B58 and ReleaseFromScript 0x75329C; removed from it by AddVillagerToAbode 0x4040FA, MakeHomelessNoStateChange
  0x761290 and DeleteDependancys 0x74FE4B.
- Town::AddVillagerToTown [0x73A090]: uninhabitable (+0x5F4) → 0 [0x73A095]; TownStats::Add and SetTown; abode of this
  town → done; of another → out of it and SetAbode(0), which also clears the villager's town (+0x12C = 0, 0x750DE8,
  literal); FindAbodeWithSpaceInTown(v, 0) → AddVillagerToAbode and 1, without checking the worship site; otherwise
  MakeHomelessNoStateChange [0x73A105].
- FindAbodeWithSpaceInTown [0x73B370]: only functional abodes (vt +0xD4) and a score strictly greater than the best,
  which starts at the minimum passed in [0x73B371..0x73B3B6].
- Town::RemoveVillager [0x73E210]: FindChildrenAndOrphanThem [0x73E21C]; TownStats::Remove [0x73E231]; with an abode
  RemoveAliveVillagerFromAbode and SetAbode(0), otherwise out of the homeless; RemoveVillagerOnWayToWorshipSite
  [0x73E360, 0x73E29F]; SetTown(0) [0x73E2B7]; adults + children == 0 → +0xF20 = 50 [0x73E2BF..0x73E2CD]; mother
  (+0x100) = 0 [0x73E2D8].
- Town::UseFood: +0x6F8 += n (unsigned qword fild) and the owner's GameStats +0xA44 +0xA4 += n [0x73B5EA..0x73B60F].

## Pregnancy and births

- How a woman gets pregnant: CheckWhenGoingToBed 0x760B60 runs once per stay (bit 0x2000) and returns 1 unless she dies
  of old age. With no town, raw Sleep < 1 or not sexually active, it returns 1 without anything else. Otherwise
  CheckGetPregnantAtHome 0x760C80 runs for the partners in the home (see [Pending](#pending)).
- CheckGetPregnantAtHome 0x760C80: WillHousewifeGetPregnant 0x7624C0 -> HousewifeGetsPregnant 0x762570; 0 when she will
  not.
- WillHousewifeGetPregnant 0x7624C0 (the argument is not read): needs a home, not already pregnant, a town and
  IsSexuallyActive. s = the town's GetDesireSignificanceToVillager(8 FOR_CHILDREN) 0x746660; n = the home's ChildCount
  (+0xB7) + its pregnant inhabitants. True when s > 0 and MaxChildrenInAbode (abode info +0x178) > n (unsigned).
- HousewifeGetsPregnant 0x762570: +0xF8 = TimePregnantFor (info +0x240, u16); not at home -> GoHome (its result). At home
  it returns the info pointer, never 0; the callers ignore it.
- WomanSpecial 0x752240: only for a pregnant woman not controlled by a script. +0xF8 −= GetGameTurnsSinceLastChecked (a
  16-bit sub); still > 0 (signed) -> 0; otherwise HousewifeStartsGivingBirth and its result.
- HousewifeStartsGivingBirth 0x7621A0: +0xF8 = 0; +0x58 = ftol(r + T × 0.25 + 1) with r = GameRand(ftol(T)), where T =
  GGameInfo +0x14 (0xD01A0C) = GGameInfo +0x10 (36000.0f, ctor 0x55775E) / 365.25 (fn_00557810) = 98.5626. So r =
  GameRand(98) and the counter is r + 25. Then SetTopState(111) and HousewifeGivingBirth. SET_TURNS_PER_YEAR (GSetup
  0x714DAA) also writes GGameInfo +0x10. No code sets 110 (no `push 0x6E` before a SetTopState): WomanSpecial calls the
  function itself (inferred).
- HousewifeGivingBirth 0x762430: −1 on +0x58 (u16); not yet 0 -> 1. At 0: ChildBorn; if there is a child,
  SoundTag::Create(child pos, GetRandomSample(0x14, 0xA) 0x71ED40) 0x71EB60 (not tracked, mode 2, no loops, 3D, bank 1
  IN_GAME, no delay); SetTopState(112); 1.
- HousewifeGivenBirth 0x7624A0: +0xF8 = 0; jmp GoHome 0x760270.
- ChildBorn(mother) 0x762220 (cdecl): r = GameRand(100). When BoyGirlChance (info +0x25C) >= r, the child's info is
  GVillagerInfo::Find(the tribe type, GameRand(6) + 1), villager numbers 1 FORESTER .. 6 TRADER (null -> the mother's);
  otherwise the mother's info. Villager::Create(mother pos, info, age 1, mother.IsSkeleton). Then the mother's home
  (AddVillagerToAbode 0x404060), else her town (AddVillagerToTown 0x73A090), else the head of the vagrants (g_game
  +0x205BFC, ++ +0x205C00) unless the child is there already. The child's +0x100 = the mother. The player's GameStats
  (+0xA44) +0x48 "Total Nacimientos" ++ (0x7623CD..0x7623E5). A poisoned mother gives a poisoned child (vt +0x4A4 -> vt
  +0x69C). Finally ChildDecideWhatToDo(child). In a multiplayer game with a player there is also a step on player +0xF8
  (fn_0056F8D0, fn_00775720(1)) (0x7622D3..0x762354).
- The housewife's day (states 100..109) and the meal call (ShallIWaitForDinner 0x7611B0, HousewifeAskForMeal 0x762600,
  HousewifeCalledToMakeDinner 0x7625A0) are dead code in W120.

## Children and the creche

- IsMotherAlive 0x757F40: the mother (+0x100) is set, IsAvailable, of the child's tribe (info +0x1F4), IsAMother 0x751110
  (info +0x1F8 == 1 FEMALE), and her status bit 0 (dead) is clear -> 1; else 0.
- ChildGotoCreche 0x7579F0: the town's creche (+0x744) functional -> SetupMoveToOnFootpath(creche, its door
  (MultiMapFixed::GetDoorPos 0x52E370, vt +0x864), 113) 0x5EDD20; 1. Else 0.
- 113 CHILD_AT_CRECHE (ChildAtCreche 0x757C90): CheckChild == 1 -> 1; no town -> 0; CheckNeededForTownDesire == 1 -> 1;
  no creche -> 0. By day (LH3DSky::Time2SkyType(GetVisualTime()) 0x86A1B0 <= 1.2, i.e. not GGameInfo::IsVisualNight
  0x5575E0) with a functional creche: SamplePlayAnimEffect 0x42A4B0 on the creche with the key {0, 0, 0x13, 0, 0x52}
  (editor.sad, GAudio +0x3B0) and the next promenade point (SetupMoveToPos(p, 113)); 1. Otherwise: a home whose first
  inhabitant (+0xA0) is at home -> GoHome; no home and touching the creche (IsTouching 0.001, vt +0x6B8, Object
  0x637E00) -> the next point; 0.
- GetNextDstPromemade(creche, index, from) 0x757A50: n = the creche mesh's extra metrics / 5 (signed). index = (k << 16)
  | p. At the door (MapCoords::operator== 0x605660): p = GameRand(n), k = 0. Elsewhere: p clamped to [0, n − 1], ++k, and
  k > 9 -> k = 0, p = GameRand(n). For k 5..9 the path is walked back (j = 9 − k). n == 0 -> the door. Else the extra
  metric 5p + j in the world (LH3D vt +0x1CC), plus two jitters GameFloatRand(1) − 0.5 in x and z. Both jitters are
  drawn whenever n != 0, so the RNG order does not depend on finding the point.

## Disciples

- g_DiscipleInfos 0x99A1F8: 13 records of 0x1C bytes. +0x0 the start state, a byte (Villager::Landed 0x760845 enters it;
  0 = inspect); +0x4 != 0 creates Reaction 0x18 in 163 (types 1, 2, 3, 4, 6, 8); +0x8 the 3D object's vt +0xCC argument
  (SetVillagerDisciple 0x756087 / ResolveLoad 0x75488F; what it shows is unknown); +0xC "held at the job", ignores needs
  (`cmp .., 1` at its 9 readers, e.g. CheckHungry 0x75BE3A, CheckEveryTime 0x75061E); +0x10 fetches wood (FORESTER,
  BUILDER, CRAFTSMAN; IsInterestedInWoodObject 0x76504F); +0x14 the TOWN_DESIRE_INFO it serves, −1 none (FOOD 0, WOOD 1,
  8, 9; fn_00741220 0x74123E); +0x18 moves into another town when dropped on its object (Landed 0x7607F0,
  CheckMoveIntoTown 0x757210).
- Types as the jump table orders them: 1 FARMER, 2 FORESTER, 3 FISHERMAN, 4 BUILDER, 5 BREEDER, 6 PROTECTION, 7
  MISSIONARY, 8 CRAFTSMAN, 9 TRADER, 10 CHANGE_HOUSE, 11 WORSHIP, 12 FROM_VORTEX.
- Villager::SetVillagerDisciple(thing, disciple, h) 0x756000 (ret 0xC; `thing` and `h` are not read): outside 0..12 ->
  0, nothing done. With a town and a new type: TownStats (Town +0x610) DecrementNumOfDisciples(old) 0x749C80 /
  IncrementNumOfDisciples(new) 0x749C60 (the bytes +0xC8 + type; type 0 skipped). A disciple: +0xE0 = (+0xE0 & ~0x400) |
  0x200, vt +0xCC(record +0x8), +0xF2 = type. Type 0: +0xE0 &= ~0x600, vt +0xCC(0), +0xF2 = 0. Returns 1.
- DiscipleDecideWhatToDo 0x751720 (the jump table 0x75193C on +0xF2 − 1; 0 and 13..255 go to the fallback). FARMER:
  Town::FindBestField 0x73E870; no town or no field -> fallback. If the field's GetFieldActivity is 2 (harvest) and the
  food carried > minFoodToShowGraphic (+0x270, signed) -> 31; else VillagerBecomesFarmer 0x759C00. FORESTER:
  CheckSatisfyWoodDesire. FISHERMAN: FishermanLookForWater 0x75B4C0. BUILDER: CheckNeededForBuilding 0x758340. BREEDER:
  SetupBreederDisciple 0x769EE0. PROTECTION: the fallback. MISSIONARY: 0 at once. CRAFTSMAN: CheckSatisfySupplyWorkshop
  0x7593A0 != 0. TRADER: CheckTrader 0x769B80. CHANGE_HOUSE: SetVillagerDisciple(0) first, then CheckMoveHouse
  0x757180(GUtils::FindClosestAbode 0x74DD70) == 1 -> SetTopState(234), and with another player FUN_0064DA80(0xD, 1); 1.
  WORSHIP: SetTopState(58); 1. FROM_VORTEX: SetTopState(record 12 +0x0 = 244 SCRIPT_IN_CROWD), then
  SetVillagerDisciple(0); 1. Any job result other than 1 goes to the fallback.
- The fallback 0x7518E7: FINAL already 221 -> 0. A disciple held at the job -> SetDiscipleNothingToDo 0x754070 != 0 ->
  1; else 0.
- SetDiscipleNothingToDo 0x754070: FindDisciplePrayerPos -> +0x58 = 0, SetupMoveToWithHug(p, 221); 1. 0 when there is
  no point.
- FindDisciplePrayerPos 0x754180: no town -> 0. No town centre (+0x9A4) -> the town's position, 1. Else the centre +
  GetPosFromAngle(the angle from the centre to me + GameFloatRand(π/2) − π/4, GameFloatRand(4) + the centre's
  Get2DRadius); 1.
- 221 DiscipleNothingToDo 0x7540D0: the town's pulse (+0x5E8) -> +0x58 = GameRand(10). Then −1 on +0x58; still > 0 (s16)
  -> 1. At 0 with the clip not done -> +0x58 = 1. Otherwise DiscipleDecideWhatToDo == 0 -> +0x58 = 300. Always 1. Its
  entry EnterDiscipleNothingToDo 0x754140: with a town centre, LookAtObject(centre, 2); always 1.
- A disciple that dies is cleared first: VillagerDead calls SetVillagerDisciple(0, 0, 0) (0x7507F3..0x750800).

## Carrying resources and the storehouse

Code: `Villager/VillagerResources.{h,cpp}`,
`Villager/VillagerSatisfy.cpp`, `VillagerSpeed.cpp`, `VillagerAnimations.cpp` (the carried object), `Town/TownDesire`
(`TownNeedsSum`), `LivingPhysics.cpp` (the villager's InitialisePhysics); tests `test/test_villager_resources.cpp`.

- **Load**: food +0xF4 and wood +0xF6 (`resourceHeld`), the carried tree type in `flags` bits 14-15 (`k_FlagTreeTypeMask`,
  overwritten on each wood pick-up, never cleared by a drop). PickupResource 0x7513F0 / DropFood 0x7511E0 / DropWood
  0x751240 / DropResource 0x7511B0 keep the town's **carried** totals (Town +0x708 / +0x70C, `TownStats::foodCarried /
  woodCarried`; the stock is the storage pit's). Capacities MaxFoodCarried 150 / MaxWoodCarried 250 (GVillagerInfo +0x264
  / +0x268, the same in all 63 records): GetFoodCapacity 0x7514D0 / GetWoodCapacity 0x7514F0 are a 16-bit `max − held`
  (negative above the maximum). Villager::AddResource 0x7564D0 always returns 0 (literal).
- **Only the larger load per trip**: GetResourceHeld 0x751570 (food if food > wood, else wood if any: a tie goes to the
  wood). With food 120 and wood 60, the food is dropped and CheckTakeResourcesToStoragePit (wood > MinWoodToShowGraphic
  50 or food > MinFoodToShowGraphic 100) sends it back for the wood.
- **31 GOTO_STORAGE_PIT_FOR_DROP_OFF** 0x769620: to the "storage pit" (Villager::GetStoragePit 0x751F10 = the town's, or
  else **its own home**) when it is functional; otherwise to GetResourceDropoffPos; nothing held → 163.
  **32 ARRIVES_AT_STORAGE_PIT_FOR_DROP_OFF** 0x7696D0 (clip 347 P_PUT_DOWN_BAG): AtStructureAddResource 0x76A3B0 (within
  the villager's *speed* of the structure's resource edge, `IsCloseToEqual`) adds it with the structure's AddResource and
  drops it; then it walks back to the pit's arrive point with FINAL 163 (not 163 at once). Without pit or functional home:
  the town's **temporary pot** (MagicFood / MagicWood, `town_stores::GetTemporaryResourceStorePotOrPos` 0x73E900,
  [buildings.md](buildings.md)); asking for the drop-off point creates it.
- **The pot branch of 34** (ArrivesAtStoragePitForResource 0x7698D0): the pot's RemoveResource result is discarded
  (0x769AA3), so a villager takes the whole meal even from an empty temporary pot, and no poison or food speed-up from it
  (literal quirk, (not verified) in the real game).
- **CheckSatisfyFoodDesire** 0x759F30 (desire Food): the jobs and the drop-off against each other (see
  [Food and wood jobs](#food-and-wood-jobs-reactions-7-and-12)).
- **The carried object** (SetStateCarriedObject 0x7501A0): wood > 50 → the log of its tree type (WOOD / TREE_1..3); else
  food > 100 → BAG unless the final state's exit is ExitBuilding 0x7597B0; nothing at life ≤ LifeWhenCrawlsWounded; the
  state rows' object (+0xEC) wins, final then TOP; IN_SCRIPT (4) and SCRIPT_PLAY_ANIM (200) keep the previous one.
- **Speed** (SetStateSpeed 0x753760, 0x75397B..0x753B2A): load factors `clamp(1 + SpeedModWhenFullLoad − held / max, 0.75,
  1)` (wood 0.75, food 0.85: slower only above 187.5 wood / 127.5 food); the **town-needs term** `0.85 + clamp(S / 2, 0,
  0.5)`, S = clamp(0.2 × the sum of ten raw desires (13, 12, 9, 7, 6, 5, 4, 3, 1, 0), 0, 1) (fn_00747150); the food
  speed-up ×4 (+0x39C) when IsFoodSpeedUp 0x55C980. Villagers of towns with positive desires walk up to 1.35 / 0.85 =
  1.59 times faster than with the base 0.85 alone.
- **Dropped log** (CreateDroppedResource 0x750940): a villager with wood > 50 put into physics by anything but the hand
  (Villager::InitialisePhysics 0x5EFEF0), thrown by the hand without landing (0x6373FA) or dying (VillagerDead 0x7507D0)
  lets the log fall as a Pine DeadTree with the carried mesh (`ecs::CreateDroppedLog` in Trees, DeadTree::Create
  0x510BB0 with the mesh override 0xCC5F10 and +0x9C = wood / Pine's woodValue; the physics'
  `PhysicsObjects::AddDroppedObject`). 51 wood gives back a 51 log (DeadTree::GetDefaultResource 0x511330: 350 ×
  float(51/350) = 50.99999905 rounds to 51.0f, the x87 runs at 24 bits, 0x7DEE0D).
  **(approximate)** from a physics release other than the hand the handler runs after the body was added and without
  the angular velocity. VillagerDead also drops food and wood to nothing (0x7507D9 / 0x7507E2).
- Reaction 9 (REACT_TO_FLYING_OBJECT) is not about picking up resources: it makes villagers flee (6) or point (162) at a
  flying object. Picking up dropped food is reaction 7, wood reaction 12.
- CARRIED_OBJECT values (CarriedObject::Init, table 0xC5E19C): 1 NONE, 6 BAG, 12 WOOD, 13..15 TREE_1..3; 16 and above
  are not carried (CreateDroppedResource 0x75095F). Villager::GetWoodCarriedObject 0x7502A0: +0xE0 >> 14 = 1 -> 13, 2
  -> 14, 3 -> 15, else 12.
- SetStateCarriedObject 0x7501A0 is called from GetAnimId 0x750133. It first sets +0xF1 = 1 NONE (0x7501C8). "The final
  state's exit is ExitBuilding" is FUN_00753140 0x753140, comparing row 0xD091B8 + 0x90·s with 0x7597B0.
- Villager::GetResourceFrom 0x753390(object, type, n): c = object.RemoveResource(type, n) (vt +0xA0). A negative n is a
  huge unsigned: the whole store (literal). c != 0 -> PickupResource(type, c, object.GetCarriedTreeType()) (Object
  0x402AF0 returns 0); object.IsSpeedUp (vt +0x4A8; 0 for homes and storage pits, GameThingWithPos 0x402410) ->
  SetFoodSpeedup; object.IsPoisoned (vt +0x4A4) -> SetPoisoned(1). Returns c.
- Villager::AddResource 0x7564D0: PickupFood(n) and, with the poisoned argument, SetPoisoned(1); wood is PickupWood(n,
  0). PickupFood 0x751490 = PickupResource(FOOD, n, 0); PickupWood 0x7514B0 = PickupResource(WOOD, n, tree).
  DropFood(0) / DropWood(0) drop everything (0x7511EB).
- GetResourceDropoffPos 0x753E20(type): GetStoragePit functional -> its arrive point; else the town's storage pit
  functional -> its arrive point; else with a town, the point of Town::GetTemporaryResourceStorePotOrPos 0x73E900 (asking
  creates the pot); without a town, the villager's position. Town::GetStoragePit is 0x73B5B0.
- GetResourceNearestEdge (vt +0x8D4): StoragePit 0x733400 = its arrive point; MultiMapFixed 0x401590 -> GetResourcePos
  0x401560 = its position.
- Villager::AtStructureRemoveResource 0x76A2F0: within the villager's 2D radius of the edge (IsCloseToEqual 0x6053C0), c
  = GetResourceFrom: 0 -> 0, c < n -> 0x24, else 1. Not there -> SetupMoveToWithHug(edge, GetFinalState), 0x24.
  (AtStructureAddResource uses the speed instead of the radius.) 0x24 is the value of GO_HOME reused as "on the way".
- AtStructureAddResource 0x76A3B0: when the structure belongs to another player, the AddResource status comes from
  fn_0064A9C0 (the player's first non-null interface slot of +0x14[0..17] -> +0x39C); for its own structure the status
  is 0.
- ArrivesAtStoragePitForResource 0x7698D0(type, n, ok, fail): n == 0 -> SetTopState(fail), 1. m = min(n, the pit's
  stock); m == 0 -> SetTopState(fail), 0. AtStructureRemoveResource(m): 0x24 -> 0x24; 1 with ok != 0 ->
  SetupMoveToOnFootpath(pit, arrive, ok), 1; else SetTopState(fail), 0. No functional pit and no town ->
  SetTopState(fail), 1. In the pot branch, the walk target is the pot's GetNearestEdgeOfObject (vt +0x834, Object
  0x636CD0 -> GetNearestPosOfObject 0x636D30, both 2D radii), with AreWeThere(p, 0).
- GotoStoragePitForDropOff 0x769620 is also called as a function by CheckSatisfyFoodDesire (0x75A094): holding neither
  food nor wood -> SetTopState(163), 0.
- 32 (0x7696D0): after the structure adds or refuses the load, the walk back always goes to the pit's arrive point with
  FINAL 163. At a temporary pot (PotStructure::AddResource 0x66ED70), the AddResource result is ignored: DropFood /
  DropWood of the whole load, then SetTopState(163).
- CreateDroppedResource 0x750940: nothing (and no DropWood) unless the carried object c0 = +0xF1 is 2..15 and wood >
  MinWoodToShowGraphic (+0x26C). The log is DeadTree::Create fn_00510BB0(my position, GTreeInfo Pine 0xDA49D8, 0, scale
  1.0, angle π/2, 0, 0, mesh CarriedObject[c0]) with life 1. +0x9C = wood / DeadTree::GetWoodValue (0x511AD0 = life ×
  woodValue × scale³). Then InitialisePhysics (vt +0x784), po +0x90 = c when given, po +0x1D8 |= 0x10,
  PhysOb::AdjustToGroundLevel 0x7FCB80 and PhysicsObject::RaiseUntilNotIntersecting 0x644800; finally DropWood(0).
- Test hook `OPENBLACK_TEST_VILLAGER_CARRY="<food|wood>,<amount>[,<n>[,<tree 0-3>]]"` (turn 2, through PickupResource);
  the trace adds `carry:`, `drop 31:` / `drop 32:`, `carried:`, `speed:`, `food-desire:`, `pot:` and `dropped log:` lines;
  `OPENBLACK_TOWN_TRACE` adds `pots:` / `pit:`.

## Builders

Code: `Villager/VillagerBuild.{h,cpp}` (VillagerCivic.cpp 0x758340..0x75980F), the rows 39 / 40 / 41 / 184 of
`LivingActionSystem.cpp`, `Town/TownDesire.cpp` (the site and plan inputs); tests `test/test_villager_build.cpp`. The
building side (sites, the 128-point ring, the pile, BuildBy, Built) is `ecs::building_sites` / `ecs::abodes`
([buildings.md](buildings.md)); the villager reaches it through `villager::BuildingSiteOps` (mocks in the tests).

- **From 163 to a site**: the town desires Abodes (5), Civic_Buildings (6), To_Build (9) and Repair_Town (12).
  CheckSatisfyAbodesDesire 0x758E30 / CheckSatisfyCivicBuildings 0x758E90 first try **any** site (CheckNeededForBuilding
  0x758340: IsBuildingHappening, GetBestBuildingSite with includeFull = BUILDER disciple), then, once a turn per town, set
  Town +0x5E4 (`requestedPlanThisTurn`, before the request, also when it fails), RequestANewAbode / RequestBestPlanned
  and try again. To_Build 0x759330 takes the best site, Repair_Town 0x759370 GetBestRepairBuildingSite. SetupBuildingObject
  0x7584B0 refuses a built-and-repaired building; CheckForClearArea 0x7590A0 is always 0 (no Object class is pushable in
  W120), so it goes on to SetupGetBuildingSupplies 0x7586E0. A villager arriving at a damaged or unbuilt home
  (ArrivesHome) calls SetupBuildingObject(abode) 0x758530, which makes the home's (repair) site.
- **Wood**: ShouldIGetWood (building side) false → straight to the ring (GotoBuildingSite); true → DecideHowToGetWood(1)
  0x75F510: the store (storage pit, else the home, else the town's temporary wood pot) scores
  `GDM(d, 250) × (stock > capacity ? 1 : 0)`, the forest `GDM(d, 250) × 0.5`; the store wins only when strictly higher.
  Store → GotoStoragePitForBuildingMaterials 0x7587D0 (the pit's wood edge, FINAL 39) → 39 0x758990 (clip 340): up to
  its capacity from the pit (ArrivesAtStoragePitForResource) and on to 184. The forest results (2 BigForest / 3 forest)
  walk to the forest (see [Food and wood jobs](#food-and-wood-jobs-reactions-7-and-12)); **(approximate)** the town's
  forest list is not filled on load yet, so the global FindForest scores the forest.
- **The ring**: GotoBuildingSite 0x758A00 = SetTopState(163) (the builder leaves and re-enters the list), +0xFC = site,
  GetRandomBuildPos (one GameFloatRand), a walk with FINAL 40 (footpath beyond 40 m). 184 0x758F60 (also where a
  builder resumes after a reaction: the "after" state of the building rows): ShouldIGetWood → supplies; touching the
  building (0.001) → 40; else the walk. 40 0x758AF0 (clip 348): within 0.2 m of its ring point it turns to the building
  (LookAtPos mode 1), puts **all** its wood on the site's pile (whatever the pile took), then PlayAnimThenSetState(41)
  keeping the state counter.
- **The build cycle** 41 0x758C40: once per play of the BuildingAnimation clip (hammer 276 / saw 354 / mallet 380):
  `f = 1 − 0.2·alignment` (LandAlignmentAt), 1 on good land, at most 1.2; `u = min(ftol(WoodUsedPerBuildCycle ×
  f), pile)` (50 for the men, 40 for the housewives); `x = u / GetWoodValue` (read before the removal), RemoveResource(u),
  BuildBy(x), the town's wood-used stat. Then: the site gone (Built) or built and repaired → +0xFC = 0 and 163; wood
  left → the next ring point (GameFloatRand, GameRand(2)) and 40 again; none → SetupGetBuildingSupplies (0 keeps it in 41
  hammering an empty site, literal). A hut (1400) takes 29 cycles of 50 / 35 of 40; the temple (6500) would take 130
  **(pending)**: `abodes::BuildBy` skips a CitadelHeart (its plan is not ported), so the temple site is refused; the
  in-game check was done with the Aztec houses.
- **The builder count**: EnterBuilding 0x759750 (entry of 39 / 40 / 41 / 184) refuses when +0xFC is not a valid site and
  calls AddBuilder when the previous final state had another entry; ExitBuilding 0x7597B0 does nothing towards another
  building state, else RemoveBuilder (valid site) and +0xFC = 0. So hunger, the hand, death and reactions take the
  villager out of the list; after a reaction 184 finds +0xFC = 0 and the builder goes to 163 (inferred).
- Not ported, never entered in W120: 51, 54, 185, 188, 189, 232 (no state change pushes them; WAIT_FOR_WOOD needs
  g_game +0x14 & 0x40000, never set). SaveBuilding / LoadBuilding not ported.
- **Town desires**: To_Build sums the sites' GetDesireForVillagers (newest first), its modification the sites' builders and
  places; Repair_Town adds the plans' GetDesireToBeRepaired after the abodes (`DesireInputsOf` of the building side).
- A BUILDER disciple is +0xF2 == 4. WoodUsedPerBuildCycle is info +0x27C (GetWoodUsedPerBuild 0x758E20). The walk to a
  ring point takes the footpath when strictly farther than 40 m ([0xC23704]): SetupMoveToOnFootpath(the site's building,
  pos, 40), else SetupMoveToWithHug(pos, 40).
- Addresses: Town::IsBuildingHappening 0x73E2F0 (+0x794 != 0); BuildingSite::GetBuilding 0x43BC70; GetClearAreaRadius
  0x43BDE0; Town::GetBuildingSiteInList 0x73CE40; Town::AddBuildingSite 0x73B8E0 (SetupBuildingObject(abode) loops on it
  until a site appears); Town::RequestANewAbode(2) 0x73B330; Town::RequestBestPlanned 0x73A650;
  Town::GetBestRepairBuildingSite 0x747EA0; BuildingSite::ShouldIGetWood 0x43C680 (it asks for GetResourceDropoffPos
  itself); NeedsBuilders fn_0043BC60; AddBuilder 0x43BE40; RemoveBuilder 0x43BE90; GetWoodValue 0x43C0C0; BuildBy
  fn_0043D080 -> the building's vt +0x900; the wood-used stat fn_0073B620 -> TownStats +0xEC, and the player's GameStats
  +0xA44 +0xA8; Town::IsBuildingSiteValid 0x73CF00; GetWoodNeededToBuild 0x43C5F0; SetupWaitForWood 0x7585A0. GGame init
  writes g_game +0x14 = 0x20.
- CheckForClearArea 0x7590A0 runs fn_0074E4C0(point, radius, ClearAreaPoint::ProcessPoint 0x7591E0, 10.0, 1), keeping the
  nearest IsPushable object with CalculateForceAppliedBy > 0.05. Every Object vtable points +0x814 at Object::IsPushable
  0x402AE0 (`xor eax, eax`; 135 vtables), and the visit draws no GameRand.
- SetupBuildingObject(site) is also called by the worship side, CheckNeededForWorshipSiteBuilding 0x76C98A.
  SetupBuildingObject(abode) 0x758530 is called by ArrivesHome 0x760AB3 and CheckInteractWithAbode 0x7574C1.
- DecideHowToGetWood 0x75F510: D = GTownInfo maxDistanceForTownForest [0xDA28E4] (250). The forest is
  Town::FindNearestForestToPos 0x73EC10, else FindForest(me, D, 0) fn_0053A1A0; result 2 when the forest's +0x38 (its
  BigForest) is set, else 3. Mode 0 weights (the wood desire): frac = (float)(1 − (capacity + 1e-5) / (MaxWoodCarried +
  1e-5)); the store scores frac, the forest (float)(1 − frac). In builder mode, a negative capacity (more than
  MaxWoodCarried held) is huge unsigned, so the store weight is 0.
- GotoStoragePitForBuildingMaterials 0x7587D0: wood capacity <= 0 -> GotoBuildingSite. Not a builder of the site, the
  site needs none and not a BUILDER disciple -> 0. The pit (or home) is walked to with the footpath even when it is not
  functional (literal); without one, SetupMoveToWithHug(GetResourceDropoffPos(WOOD), 39).
- 39 (0x758990): capacity 0 -> GotoBuildingSite == 1 ? 1 : SetTopState(163). A negative capacity is passed on as a huge
  unsigned amount (literal).
- 40: the ring point is (ftol(ring[i].x × 6553.6), ftol(ring[i].z × 6553.6)). The wood goes on the pile through the
  site's AddResource (vt +0x9C, result ignored), then SetStateCarriedObject.
- 41's release sets +0xFC = 0 BEFORE SetTopState(163), so ExitBuilding skips RemoveBuilder. When the site was deleted by
  Built, this 163 replaces the walk that ToBeDeleted gave the villager (literal). The next ring point is
  GetNextPosFromIndex (vt +0x124: GameFloatRand then GameRand(2)); GetRandomBuildPos is vt +0x128.
- 184's touch test is IsTouching (vt +0x6B8, Villager 0x55C9A0 -> Object 0x637E00), and GetRandomBuildPos is drawn also
  when touching.

## Repairs, the tap on a home and the town emergency

Code: `Villager/VillagerEmergency.{h,cpp}` (197, 242, 243, the tap, the emergency call), the +0x50 column
`k_TownEmergencyReaction` in `Villager/VillagerOriginalFns.h`, ArrivesHome's repair
branch (`VillagerHome.cpp`), CheckSatisfyToRepair (`VillagerBuild.cpp`); tests `test/test_villager_repair.cpp`. The
damage, the sites, ProcessTownRepairs, GetBestRepairBuildingSite, BuildBy / Repaired and SetInStateOfEmergency are the
building side's (`ecs::abodes`, `ecs::building_sites`, `ecs::town_emergency`, [buildings.md](buildings.md)).

- **A repairer is a builder** ([Builders](#builders)): no state of its own. On a built building SetupBuildingObject 0x7584B0 goes on as for
  a build; the wood fetched is `GetWoodNeededToBuild` = (1 − life) × WoodValue − pile; each cycle 41 adds
  `x = u / GetWoodValue` with BuildBy → IncreaseLife (capped at 1) and at life 1 Repaired deletes the site (the builders
  are released as at Built). Cycles from life 0.6: Norse hut 12 of 50 / 14 of 40, storage pit 33 / 41; from 0.1 the
  Citadel heart 117 / 147 (float model). RepairAmount (GVillagerInfo +0x280) has no reader in W120 (inferred).
- **Who comes**: a rock / spell / fire hit goes through Abode::ReduceLife 0x405D90, whose site is **ordinary**
  (+0x638 = bit 2 of +0x58, 0 on a finished house). So `CheckSatisfyToRepair` 0x759370 (desire 12, only +0x638 sites
  with a desire > 0: ProcessTownRepairs' ones) does not take it; the ordinary builders do (To_Build 9, Abodes 5,
  Civic 6 through GetBestBuildingSite), while `GetBuildersNeeded` > 0, i.e. life <= 0.9 and, for a house, someone lives
  there. Literal: Repair_Town still rises for every damaged abode (no site filter); a house at life in (0.9, 1) keeps
  an unserved site (only BUILDER disciples, not ported, repair above 0.9). A rebuild plan's site has +0x638 = 1 but its new
  building starts at life 1, so it is built through To_Build too.
- **Its own inhabitants** (ArrivesHome 0x760930): arriving at a home that is not built-and-at-full-life, with life >= 0.3
  and food >= 0.5 (strict), SetupBuildingObject(abode) 0x758530 (the site made if missing); 1 → it repairs, else in.
  Hungry: in (with a SetTopState(163) overwritten when the home is not functional, literal).
- **The tap** (`SetStateWhenTappedOnAbode` 0x752B80, called by Abode::ReduceLife 0x405DF3 for each inhabitant when the
  new life is below 1 and by Abode::InterfaceTap 0x406855): only a villager available and **inside** (+0xE0 & 4):
  FindPosOutsideAbode (GameFloatRand(1.5) then GameFloatRand(pi/4)), SetupAfterTapOnAbode 0x761400: raw PREVIOUS = 163,
  a walk with FINAL 197, +0xE0 |= 1 (no own desires, no sleep until the next share-out clears it). 197 0x761440:
  PlayAnimThenSetState(PREVIOUS): the yawn, then 163.
- **The emergency call** (`CallToTownEmergency`, the body of CallAllVillagersToTownEmergency 0x747890, run
  at once from SetInStateOfEmergency when +0xF1C was 0): every housed villager whose **GetFinalState** row has a +0x50
  answer goes to SetTopState(242) (its exit runs: a builder leaves the site). 206 rows always answer (walks, jobs,
  carrying, going home 35..37, 163, reactions), 220 MOVE_AROUND_FIRE answers when its PREVIOUS is not 0, 48 never (at
  home 38 / 100 / 118..120, in the hand, script, dance, dying / dead, fainting, the fire rows 215..219, 242 / 243,
  chilling out 245 / 246 / 252). So a villager walking home (FINAL 37 ARRIVES_HOME) answers, as does a walk to a site
  (FINAL 40); only one already inside (TOP 38 / 100 / 118..120) stays. No villager walk has FINAL 38 in W120 (the only
  `push 0x26` before SetupMoveToPos / SetupMoveToWithHug is the cow's 0x41D3D7, an animal enum). Homeless
  villagers join when they next run 163 or 246.
- **242** 0x76B200: no town → 0 (stays); else the congregation position + GetPosFromAngle(GameFloatRand(2 pi), d) with
  d = min(n × 0.025, 1) × 20 + 10 (n = adults + children: 10 m empty, 20 m at 20, 30 m from 40) → walk FINAL 243.
  **243** 0x76B300 (TownEmergencyAnimation): during the emergency GameRand(12): 0 → 36 GO_HOME, else 242 again; after
  it GameRand(5): 0 → 163, else 242; one clip per stop. During the 1200 turns DecideWhatToDo sends every villager that
  reaches 163 to 242, so a damaged pit is only repaired after it (inferred, not verified in the original).
- Town::SetInStateOfEmergency is 0x7479A0. Villager::LookAtPreviousStateReactToTownEmergency 0x756520 returns the byte
  +0x8E (PREVIOUS); only row 220 has it.
- SetupAfterTapOnAbode walks with Villager::SetupMoveToPos 0x763800 (= Living 0x5F2830); the walk leaving the at-home
  state runs ExitAtHome, which takes the villager out of the home. SetStateWhenTappedOnAbode's 1 / 0 result is
  discarded by both callers.
- The congregation constants: 0.025 [0x8D150C], 20 [0x8C7658], 10 [0x8AB414].
- Traces (`OPENBLACK_VILLAGER_TRACE`): `tap: inside -> out (x, z) prev 163` / `tap: not inside`, `197: yawn -> s`,
  `emergency: row f always|previous(p)|none -> 242|stay`, `242: congregation (x, z) n d a -> (x, z)` / `242: no town`,
  `243: emergency b r r -> s`, `home 37: repair abode e built b life l -> 0|1`, `repair: site … needs builders …
  builders … life …` (SetupBuildingObject on a built building), `repair desire: best repair site e -> r`.

## Food and wood jobs, reactions 7 and 12

Code: `Villager/VillagerFarmer.{h,cpp}`, `Villager/VillagerFisherman.{h,cpp}`, `Villager/VillagerForester.{h,cpp}`,
`Villager/VillagerSatisfy.cpp` (the Food desire), `Systems/Implementations/VillagerResourceReactions.{h,cpp}` (7 / 12),
`Systems/Implementations/VillagerReactions.{h,cpp}` and `Effects/Reactions.{h,cpp}` (the slot framework),
`Components/VillagerReaction.h`, the rows 19-22, 47-50, 52, 53, 55, 56, 67-69, 186, 187 of `LivingActionSystem.cpp`, the
three wood reactions of `Trees.cpp`; tests `test/test_villager_farming.cpp`, `test_villager_reactions.cpp`,
`test_villager_forester.cpp`. The field / fish-farm side is `ecs::fields` / `ecs::fish_farms` (reached
through `villager::FieldOps` / `FishFarmOps`, mocks in the tests).

- **The Food desire** (CheckSatisfyFoodDesire 0x759F30): three candidates in k order, fish farms (fn_0073E750:
  `Score × GDM(d, 500)`), fields (FindBestField 0x73E870: `GDM(d, 300) × GetDesireToBeFarmed`), flocks without a
  shepherd (fn_0073E7F0: `GDM(d, 300)`), each the strictly best of its list (newest first); sorted high to low, ties keep
  k's order. The drop-off (`GDM(d, 500) × frac`, see [Carrying](#carrying-resources-and-the-storehouse)) beats the head only when strictly higher; then the head's job. A fish
  farm scores 1 **only with no fisherman** (`ftol(1 − min(1, n / 4))`): through the desires a farm gets one fisherman.
  **(approximate)** a free flock at the head returns 0 (VillagerBecomesShepherd 0x768BE0 is not ported).
- **Farmers**: SetFarmerGotoField 0x759C40: activity 1 (sow) or 2 (harvest) → 163, +0x118 = the field, the walk to its
  centre with FINAL 67, **then** the work point +0x11C (`workPos`) = RandomFarmPoint (2 GameFloatRand(10)). 67 0x759D20
  (clip 259): sowing at the point → a new point and 68 (one crop per clip, PlantCrop; 30 crops a field, shared); else the
  walk to it. Harvesting: RipeFarmPoint draws 2 and discards them, unripe → 163; at the point a new point and 69 (clip 257:
  RemoveFood(capacity), the EAX result, then 67; a negative capacity, > 150 held, → 31). **Quirk kept**: with a full load
  (capacity 0) the farmer digs for 0 until something else takes it out. EnterFarming / ExitFarming (rows 67-69) keep the
  field's farmer list: added once on the first farming state, kept across 67 ↔ 68 / 69 and the walks (same entry / exit
  functions), removed (and +0x118 cleared) on any other state.
- **Fishermen**: VillagerBecomesFisherman 0x75B560 → 163, +0x118, the walk to the farm with FINAL 55. 55 0x75B5D0: out of
  the farm's map cell → walk again; on the arrive point → the fishing spot (2 GameFloatRand(5) − 2.5); else 56. 56
  0x75B6A0 (clip 262), once a clip: GameRand(fishermen) must be 0; the catch `ftol(TribalPower[7] × min(37.5 × season,
  capacity))` (37 / 33 / 26 / 22 by season; tribal power index 7 for every tribe, literal); then 31 when full. The farm's
  stock is never touched.
- **Foresters**: CheckSatisfyWoodDesire 0x75F4A0 = DecideHowToGetWood(0) (see [Builders](#builders)): 1 → 31, 2 → the BigForest's arrive
  point (53), 3 → VillagerGotoForest 0x75F720 (the centre tree's working point, else the forest centre; FINAL 49 and the
  **raw** TOP 47, no exit / entry / clips). 49 0x75F930 (clip 217): touching the nearest tree (IsTouching: the speed
  strictly above the distance) → turn −0x124 and 50; a tree → walk to it; none → an empty forest within 50 m is deleted,
  52. 50: FellTree (FelledTree::Create + ToBeDeleted) and +0x118 = the felled tree **after** it (its reactions 12 spread
  inside); while it is in the physics it waits; then 52 → GotWoodDecideWhatToDo 0x75FC30 (no wood 163; the builder's site or
  CheckNeededForBuilding; else 31). **The forester does not pick the log up**: reaction 12 does. 53 0x75F9E0: at the
  BigForest's arrive point RemoveResource(capacity) and PickupWood (BigForest carried type 3), then 163 **even after**
  GotWoodDecideWhatToDo (literal). 47's look-ahead needs MobileWallHug +0x5E == 2, never set: off (literal).
  SetupGetBuildingSupplies' forest results walk to the forest / BigForest with +0xFC = the site.
- **Reactions 7 / 12 and the slot**: one `components::VillagerReactionSlot` (Living +0x94 /
  +0xBC) for the types of the villager type table (0xC09CC0: only 7 and 12 registered). The spread's per-villager body
  0x6E3F90: the whole Villager::IsAvailableForReaction 0x763390 (worship site, football, death states, life ≤ 0.15 except
  for 7, Living 0x5F11F0); no reaction held → score > 0 (fn_006E4620: the type's priority only within its distance) and
  the records (again 0 for both: 47-50 / 52 make 12's 0 anyway) → MarkStarted and the setup (AddReaction 0x763440:
  StorePreviousState only without a reaction, SetTopState, then +0x94); a reaction held → the switch rule (never to the
  same type, +0x04 = 0; food ↔ wood after max(10, cur / new × 20 − 10) s; SetReactionDoneWhen). ProcessReaction 0x5F1270
  ends it (reaction gone → StopReacting; object gone → StopReactingAndSetState; past the standard 2000..3000 turns).
  Reaction::ShutDown 0x6E4720 (RemoveAll* / Prune: a handler per Living class, before the erase) stops its followers newest
  first. **(approximate)** it runs at BeginTurn's Prune (or an owner's RemoveAll*), not at the initiator's ToBeDeleted:
  a log or pile destroyed mid-turn reaches ProcessReaction's first test (StopReacting, no state reset) first. **The
  miracle reaction types (fire, teleport, shield, death) keep their maps and code**: the slot is only consulted when a
  slot type meets them (their priority functions score them as the current reaction).
  - 7: ReactToFoodPriority 0x5F1710 (not in the hand / physics, has food, within 100 m, IsInterestedInFoodObject
    0x764DF0: `boost × capacity share × POWER(food) × state interest × GDM(d, 35)` > 0.25, else the town's GetDesire(FOOD)
    term > 0.01) → 19 (walk to its working point) → 20 0x764920: food taken **only from a PileFood** (the pit's pile, the
    hand's, magic food: a plain Food Pot attracts and gives nothing, literal), hungry → 117, else `CalculateDesireForFood ×
    load share × GDM(d pit, 100)` > 0.1 → 31, else 163.
  - 12: ReactToWoodPriority 0x5F17A0 (not in the hand, IsInterestedInWoodObject 0x764F60: capacity, town, life > 0.3; a
    **dead tree with less than 125 held** is always wanted; else the town's raw wood desire term) → 21 → 22 0x764720:
    while the log falls (in the physics) it waits beside it (0.5 m, LookAt, 57 for 10 turns), giving up after 1000 turns;
    landed → RemoveResource(capacity), PickupWood(its tree type), GotWoodDecideWhatToDo.
  - The wood reactions (Trees.cpp): a felled tree (FellTree, the local player) makes one at once (the DeadTree ctor
    0x510880, 0x510957) and a second one after its physics set-up (0x511889). A dropped log (CreateDroppedLog,
    fn_00510BB0 -> ctor 0x510A30, 0x510BF4) makes none until it lands: `DeadTreeEndPhysicsReaction` (0x511413; not a
    felled tree) **(pending, see [Pending](#pending))**.
- Farmers: Field::GetFieldActivity 0x529350 (1 sow, 2 harvest, else 0); RandomFarmPoint fn_00528970; RipeFarmPoint
  fn_00529240; PlantCrop 0x5291A0 (the position is not read); GetPlantCropPos 0x529210 (still sowing); Field::RemoveFood
  0x5295A0; AddFarmer 0x5283E0; RemoveFarmer fn_00528340. VillagerBecomesFarmer 0x759C00: no field -> the town's
  FindBestField (none -> 0), then SetFarmerGotoField(field, 0).
- 68 FarmerPlantsCrop: PlantCrop false -> 163; else still sowing ? 67 : 163. 67 with activity 0 -> 163. Arriving at the
  field writes +0xF1 = 1 NONE (0x759DE3), overwritten by the state change that follows. 69's "overfull" test compares
  (float)(int16) capacity < [0xD394DC], a 0.0 that nothing writes; then GotoStoragePitForDropOff.
- ExitFarming removes the farmer only while the field is still in the global field list g_game +0x205C04 (next +0xC4).
  For a deleted field +0x118 is kept.
- Fishermen: the catch base is (float)MaxFoodCarried × 0.25 (37.5 for 150). The season multiplier is 1.0, 0.9, 0.7, 0.6
  for GetSeason 0..3 (0x75B70F..0x75B72D); then min with the capacity, × TribalPower[7], ftol. The farm's score function
  is fn_0052D2F0; FishFarm::GetArrivePos 0x52C490; FishingSpot fn_0052C870; AddFisherman fn_0052D250 (writes +0x118);
  RemoveFisherman 0x52D290; the roll is GameRand(farm +0x84). Puzzle fish farms have no town, so no desire reaches them.
- IsAtValidFishingPos 0x75B670: the high words of x and z (+0x16, +0x1A: the map cell) equal the farm's. The "on the
  arrive point" test is an exact MapCoords::operator== 0x605660 (x, z and the altitude).
- EnterFishing 0x75B820: the same entry -> 1 (55 -> 56 adds nothing); no town -> 1; not in the farm's list (+0x80) ->
  AddFisherman. ExitFishing 0x75B880: the same exit -> 1; +0x118 available -> RemoveFisherman; +0x118 = 0.
- Foresters: Villager::IsTouching(pos) 0x753040 (vt +0x6B4): GetSpeedInMetres (vt +0x130) > GetDistanceFromObject(pos)
  (vt +0x13C = distance − radius; equal is not touching). FindTreeNearVillager 0x75FD00 (the nearest tree in 9 cells)
  returns 10 when touching its working point, else 1 (`neg; sbb; and 9; inc`, 0x75FDF3..0x75FDFD). VillagerGotoForest:
  Forest::GetForestCentreTree 0x53ABF0 and Tree::GetWorkingPos 0x74C040.
- 47's look-ahead (0x75F7EE..0x75F913): probes me + 10 m at the game angle + {0xFE00, 0, 0x200}, and from every
  FindType(6) tree with a forest (+0x68) in the probe's cell takes the one whose working point is nearest the probe
  (first best 10000), then walks there with FINAL 49. It never runs in the game (+0x5E == 2 is never set).
- FelledTree::Create 0x5116A0 returns 0 at once for a tree without its 3D object (+0x40), and 0x75FAF8 still deletes the
  tree: the tree goes and +0x118 = 0. Forest delete is Forest vt +0xC.
- BigForest: RemoveResource 0x4390D0; GetCarriedTreeType 0x438DE0; BigForest::GetArrivePos 0x439360. 53 needs a town
  and a capacity != 0 (a negative one goes on); not there -> SetupMoveToWithHug(p, GetFinalState).
- 186 TakeWoodFromTree: no tree near -> 163; else the creature empathises
  (GPlayer::MakeCreatureEmpathiseWithPlayerTownDesire(1, 0.5, Pos) 0x4C80F0), then SetTopState(49) and
  ForesterArrivesAtForest's result.
- GotWoodDecideWhatToDo also sends a disciple held at the job (g_DiscipleInfos +0xC) straight to 163.
- The reaction slot table: setup vt +0x9BC Living 0x5F14C0 (food) / vt +0x9C0 Villager 0x765B70 (wood); turnsToReact vt
  +0xAC0 Living 0x5F18C0; turnsBeforeAgain vt +0xAC4 Living 0x5F1920, and for wood vt +0xAD8 Villager 0x7648D0 (the
  final state 47..50 or 52 -> 0, else the standard). Initialiser 0x6E0E80. Priorities: food 60 [0xD4F97C], wood 55
  [0xD4FB70]; maxReactionDistance 35 for both ([0xD4F998] / [0xD4FB8C]).
- The score fn_006E4620 counts only when isReacting[t] (GLivingInfo +0x144 + 4t) is set and d <= maxReactionDistance. A
  switch between types counts seconds as (now − RecordTurn(current type)) / (1000 / [0xD01A38]), both unsigned
  divisions. Villager::IsAvailableForBeliefButNotReaction is 0x763410 (vt +0x988).
- ReactionValidate reads ReactionInfo (Reaction::GetInfo 0x6E4709, table 0xD4F6B0) +0x28
  whetherReactionFinishesIfInitiatorInHand (0xD4F6D8). CircleHugInfo::Reset 0x60A9F0 sets TurnsToObj to 0xFF.
- ReactToFoodPriority 0x5F1710: not in the hand or physics; the object has food (GetResource vt +0x98); within
  ReactionInfo[7] +0x44 (100 m). ReactToWoodPriority 0x5F17A0: only the hand test (no physics, resource or distance
  test).
- IsInterestedInWoodObject 0x764F60, the other branches: wood capacity == 0 -> 0; no town -> 0; the villager's building
  site (+0xFC) valid with GetWoodNeededToBuild <= 0 -> 0; a building-site pile (IsAPotFromABuildingSite) or a
  wood-fetching disciple (+0x10), when IsVillagerAvailable -> 1. The town term uses GetRawDesire(WOOD) and its trigger
  GetInfo(1) +0x18; the food term GetDesire(FOOD) and GetInfo(0) +0x18. The capacity fractions are capacity / Max (int16
  / int).
- 20 ArrivesAtFoodReaction 0x764920: a disciple held at the job -> 163 before the drop-off test. The drop-off distance
  modifier is GDM(d, MaxDistCarryFoodPit, info +0x350, 100) and uses the LOAD fraction (held / Max), not the capacity.
- 21 / 22 also stop when the log is on fire (IsOnFire 0x637CC0). 22's falling wait uses SetupWaitForCounter(10,
  GetFinalState) 0x76B060.
- SET_INTERACT_DESIRE (feature script case 76, 0x71782F) writes g_game +0x205A50, the "interact" term of both interest
  scores; StartPlaygroundGame resets it.
- Traces (`OPENBLACK_VILLAGER_TRACE`): `food-desire: farm <id> <s> field <id> <s> flock <id> <s> drop <s> -> 31|fisher|
  farmer|shepherd TODO|0`, `farm: goto|add|remove`, `farm 67|68|69:`, `fish: goto|add|remove`, `fish 55|56:`,
  `wood-desire:`, `forest: goto`, `forest 49|50|52|53:`, `gotwood:`, `react 7|12: <obj> ... -> setup|skip`, `react
  19|20|21|22:`, `AddReaction`, `ProcessReaction:`. The existing hooks start the jobs: `OPENBLACK_TEST_TOWN_DESIRE="0,1"`
  (Food) / `"1,1"` (Wood), `OPENBLACK_TEST_FIELD_GROWTH=1200`, `OPENBLACK_TEST_FELL="x,z"`, `OPENBLACK_TEST_VILLAGER_CARRY`.

## Worship: return home

CheckVillagerGoBackToTownFromWorship 0x76BEC0 (worship code) returns the code of SetTopState(248) == 1
(0x76BF59..0x76BF6D), not "TOP == 248": if it pauses first (239 with FINAL 248) it also returns 1 and the villager has
already left. Previously, after a pause, ProcessInWorship carried on with a villager that was no longer at the site
(again in the return queue, one more worshipper requested, extra chant damage and a stale entry at the front of the
queue).

## Soft drop and landing

It uses `affine::ArcTanOctant` / `GetYAngle` / `WrapAngle`, `PhysicsObjects::SyncTurnStart` and the late FLYING of
[physics.md](physics.md). Code: `LivingPhysics.{h,cpp}` (EndPhysics,
the pure pose functions, InterfaceSet{In,Out}MagicHand), `VillagerCore` (`villager::SetYAngle`), `VillagerAnimations`
(`VillagerLandedClip`), LANDED's state function (`LivingActionSystem.cpp`), `Villager/VillagerInteract.{h,cpp}` (173 /
174); tests `test/test_living_landing.cpp`.

- **Axes**. From the hand (put down or thrown) the original builds the body from the held object's drawn
  matrix (Object::ThrowObjectFromHand 0x638669 from HandAngles, SetUpPos 0x7FC779), as openblack does: the body's
  columns are the rows as they are (`living::BodyRows`). Only the world path (Object::InitialisePhysics 0x6374D9) uses
  `Object::GetWorldMatrix` 0x638200 = AngleY(+0x4C) while drawing at +0x4C + pi / 2 (fn_0051AF00); there openblack's
  body is the drawn rotation, so right = -column 2, up = column 1, fwd = column 0 (`living::OriginalRows`, until
  the physics builds those bodies as the original); every formula below reads the rows. (inferred, not verified in
  game: `OPENBLACK_VILLAGER_TRACE` / `OPENBLACK_ANIMAL_TRACE` print
  `landing: … (original right.y a, openblack column 0 y b) …`: a put-down on flat ground gives 0 / 0; a villager thrown
  so it lands on its side tells which of the two the clip 306 / 307 agrees with.)
- **Pose** (Villager::EndPhysics 0x5F0A60, from the turn-start rows, a = right.y = po+0xD8): a < −0.5 (or NaN) → landType
  1, yaw = GetYAngle(up) as it is; a > 0.5 → 2, yaw = Wrap(float(GetYAngle(up) + pi)); else 0, yaw =
  Wrap(float(GetYAngle(fwd) + pi)). The yaw goes to `villager::SetYAngle` (MobileWallHug 0x60DAC0: +0x4C and GameAngle
  +0x5C = ConvertAngle3DToGame); the caller's second SetXYZAngles (0x646B72 / 0x645F08) then draws it at the GetYXZ yaw
  of the current matrix, so GameAngle (the walking heading) and the drawn yaw differ for poses 1 / 2. Empathy (ANGER 0.5 /
  COMPASSION 0.1 when po.GetPlayer()): not ported (no creature). Not a gentle put-down (po LANDED clear) → +0xE0 &= ~0x20. Then
  Object::EndPhysics (BackInMap), +0xB4 bits 4-5, the water / dead branches ([Death](#death)), LANDED.
- **Soft drop.** A villager put down gently (not thrown, on dry land, normal.y >= 0.7) is taken
  out of the physics at once through EndPhysics and never enters FLYING (Living 0x5EFDD7..0x5EFDEB): IN_HAND → LANDED,
  +0xE0 & 0x20 kept (set by Villager::InterfaceSetInMagicHand 0x753083). On flat ground the pose is 0 and the heading
  the one it had in the hand.
- **LANDED** (Villager::Landed 0x7606E0): a disciple (+0xE0 & 0x200) still flagged 0x20 waits for the clip, drops the
  0x20 reaction and the flag, and (pending: disciples are not ported) looks for something to work at (FindCloseObjectsForInteract 0x756E80 →
  begin state or SetupInspectObject 0x76AAB0 → 233 → 171-175 CHECK_INTERACT_WITH_*); otherwise (or nothing found) the
  0x20 reaction goes and PlayAnimThenSetState(163). Its clip (LandedAnimation 0x423730): landType 0 → 308 / 309 (+0xF1),
  1 → 306 P_LANDED, 2 / 3 → 307 P_LANDED_FROM_BACK.
- **CHECK_INTERACT_WITH_FIELD 173** (0x757590): the target (+0x118) field's player (its town's) is the villager's and
  SetFarmerGotoField(field, 1) → 1 (creature empathy TownDesire 0, 0.5: not ported, no creature); else 0. **174 FISH_FARM**
  (0x757610): the same with VillagerBecomesFisherman. **172 ABODE** (0x757420, pending): built-and-not-needing-repair
  or another player's → CheckMoveHouse 0x757180, another player's (+0x11C = its town, 163), workshop (163), its own home
  (GoHome); else (same player; another town only for a town centre → ForceMoveVillagerToAbode) SetupBuildingObject
  0x758530 (`SetupBuildingObjectForBuilding`) == 1 → 1, else DecideWhatToDo. These states are only reached from the
  disciple's inspection, which is not ported.
- **+0x4C** (`living::ObjectYAngle`): the y angle Object::SetYAngle 0x639260 keeps and GetWorldMatrix 0x638200 reads
  (AngleY, no pi / 2). openblack keeps only the drawn Transform = AngleY(+0x4C + pi / 2): exact when it is still the one
  drawn from the game angle (WallHug::yAngle = ConvertGameAngleTo3D(a) for villagers, ConvertGameAngleTo3D(AnimalBrain::
  angle) for animals: the SetGameAngle writer), else (inferred) the GetYXZ yaw of the drawn matrix's original rows.
  The physics is to build the bodies from AngleY(ObjectYAngle) (a TODO on OriginalRows).
- **No body** (`living::EndPhysicsWithoutBody`, Living::InitialisePhysicsFromHand 0x5EFDF8 -> EndPhysics(NULL)): the
  villager jumps to 0x5F0B79 (no pose / empathy / SetYAngle, +0xE0 & 0x20 kept), the animal to 0x5F0DF9 (no SetYAngle);
  then the same code as with a body (Object::EndPhysics, landType 3, the water / death branches without a player,
  LANDED). Drawn from +0x4C (the hand's tilt goes).
- **Out of the hand**: Villager / Living / Animal vt +0x704 is Object::InterfaceSetOutMagicHand 0x637670 (the FireEffect's
  SetOutMagicHand): `living::InterfaceSetOutMagicHand`.
- Living::MoveToPos 0x5EC270, state 1: MobileWallHug::MoveTo (0x5EC280) and, only on 0xA, SetTopStateToFinal
  (0x5EC287..0x5EC28E); it returns MoveTo's result (0x5EC293).
- Landed 0x7606E0, the creature part (0x760707..0x760756, on the first turn, +0x90 == 1):
  GetInterfaceStatusWhoLastDroppedMe 0x63A710, then for its player ConsiderMakingCreatureMimicPlayer(status, disciple +
  0x15, this, 0) 0x760744 when disciple + 0x15 < 0x2E, and fn_00414600(player + 0x60, player, this) 0x760756 when +0x10C
  != disciple.
- Landed's disciple branch, when FindCloseObjectsForInteract finds something: first the town change (g_DiscipleInfos
  +0x18, CheckMoveIntoTown 0x757210, ChangeTribeIfRequired 0x7618C0). Then the record's start state if != 0, else
  SetupInspectObject 0x76AAB0 (+0x118 = the object, SetupMoveToWithHug(…, 233) or SetTopState(233)); 1. The reaction
  removed is RemoveAllReactionsOfTypeInitiatedByObject(this, 0x20). Villager::InHand (0x76AE27) also calls
  FindCloseObjectsForInteract.
- 173 / 174: Field::GetPlayer 0x528940 and FishFarm::GetPlayer 0x52C850 are their town's (two "none" count as the
  same). The creature empathy is MakeCreatureEmpathiseWithPlayerTownDesire(0, 0.5, Pos) 0x4C80F0; 172's is (5, 0.5,
  Pos). 172 also needs Abode::GetPlayer 0x405F70 (town-less GameThing::GetPlayer 0x570130) and the other player's town
  pointer at +0x11C (0x757527).

## Death

Code: `Villager/VillagerDeath.{h,cpp}`
(VillagerDead, SetDying, the states 13 / 14 / 15, IsDead, GetPlayerOf, DeleteDependants (was DeleteDependancys), Delete, SetSkeleton, the shared
`living::DeadTick`), `Villager/VillagerMourning.{h,cpp}` (REACT_TO_DEATH, 205-208, the orphans and 131),
`Villager/VillagerSoul.{h,cpp}` (the soul), `Components/TownDeaths.h` (the town's death counters); the dead branches of
`LivingPhysics.cpp` (land) and `VillagerDrowning.cpp` (water); test `test/test_villager_death.cpp`.

- **VillagerDead 0x7506C0** (reason, killer, amount, drop): nothing while the villager flies (+0x24 & 0x40; openblack: a
  flying body, `PhysicsObjects::IsFlying`) or once dead (`Living::IsDead` 0x417270: status & 1 or TOP 15). Then: the
  help sprites (table 0x99A368, A / B / C per reason: KillingPeople when the killer is the local player, else
  DeathInVillageSFX when the owner is; with a town WorshippersDying for CHANT, LosingVillagers above GTownInfo +0x150
  adults, else LowOnPeople), the drops (CreateDroppedResource only with `drop`; DropWood / DropFood always), the
  **owner's** alignment (`effects::alignment::UpdateForDeath`: GPlayerInfo +0x20 + 4r, twice for a child, no
  ScaleChange; a villager without a town changes none), the town's counters (`TownDeaths`: +0x38, +0x5C[0],
  +0x7C[reason], +0xA4[killer, none = neutral], +0xD8 = the turn, +0xDC), the town's pulse (+0x5EC = 0, +0x5E8 = 1),
  SetDying and then the reason (+0x118). The killer and owner are `std::optional<PlayerNames>` (none = neutral); the
  local player is PLAYER_ONE **(inferred)**.
- **SetDying 0x76A4C0** (also row 13): life 0, SetTopState(14) (the exits of the state left run; the dying clip is chosen
  with the landType of the last landing), status |= 1, DeleteDependancys (out of the abode and town, a mother's orphans),
  status |= 0x30, the counter = 600 (120 with a functional graveyard in the town), out of the world population (+0xE0 &
  0x40, which `magic::players::WorldPopulation` skips).
- **14 DYING 0x76A570**: SACRIFICE -> 15 at once; else PlayAnimThenSetState(15): 23 WAIT_FOR_ANIMATION plays the dying
  clip (253 P_DYING, 246 P_DEAD2 after landing on the left side, 283 in the water). Not at home: REACT_TO_DEATH. It runs
  the same turn as the death (ProcessState calls CallState after CheckEveryTime).
- **15 DEAD 0x76A5E0**, then Living::Dead 0x5EC400: a fire on it goes; not script-controlled: on the first DEAD turn a
  smoke puff (`SmokyStuff`, half its height up), out of the water a soul (`villager_soul`: a child's ChildMeshHigh forced
  to heaven, else StdDetail; Random(0, 100) < 50 heaven; the clip pair 244 / 245, or 247 / 248 when the corpse's clip is
  named "M_P_DEAD1", literal) and the mesh becomes the skeleton 0x1FF PersonSkeletonMale (every turn). The counter runs
  down one per turn (`living::DeadTick`, shared with the animals); at 0 (counter + 1 turns) or for SACRIFICE a second puff
  and Villager::ToBeDeleted. A script-controlled corpse never smokes, never turns skeleton and never times out (except
  7). The dead clip is 243 P_DEAD1 (landType 3 after SetDying), 246 for a corpse thrown back on its left side, 249 in the
  water. DEAD's exit is CannotExitState 0x768640 (only IN_HAND 24, FLYING 10 or a state with the same exit).
- **Corpses, the hand and the physics**: a corpse in 15 (or playing its dying clip, FINAL 15) is available and reachable
  (IsAvailable is false only with FINAL 14). In the hand or flying it does not count down. Landing on land
  (Villager::EndPhysics 0x5F0A60): the landType (the turn-start matrix's right row y: < -0.5 -> 1, > 0.5 -> 2, else 0)
  into status bits 4-5; life <= 0: already dead -> 15 with its remaining counter, else VillagerDead(5 PLAYER_INTERACTION,
  the hand's player); then the landType again. In the water: dead -> 14 with 600 (never the graveyard's time), else
  VillagerDead(6). A villager killed in flight (impact, spell, fire) dies at rest with reason 5 / 6 (literal).
  Villager::HasSunk 0x750AB0: not available (FINAL 14) -> not sunk; the status bit alone (+0xB4 & 1, not IsDead) -> 14,
  600; else DROWNING.
- **A predator's pounce** (fn_005EC480): every Living, a corpse too, gets the downed mark, life 0.05 and SetTopState(17)
  (Villager::SetTopState); DEAD's exit refuses the state of a corpse (to check in the game).
- **Mourning**: REACT_TO_DEATH (23) spreads once over 60 m. Priority (0x766440) 100 unless my town has a functional
  graveyard, I am the dead one or 10 follow it now (reaction +0x1C: AddReaction `inc`, StopReacting `dec`). Setup (0x7665B0): GameRand(2) 0 -> 205 with the counter 0, 1 ->
  206. 205 points (clip 395) once facing it, for ftol((GameFloatRand(20) + 2) x (1000 / ms per turn = 10)) facing turns
(x87 precision, one truncation); 206 walks to 4 m
  (maxDistanceToRunAwayFromObject) from it when farther than 4.8 m; 207 turns to face it; 208 mourns (285 -> 313 -> 321)
  for ftol((GameFloatRand(3) + 4) x 10) turns, then StopReactingAndSetState. The exit is ExitReaction, the validate
  ReactionValidate (the corpse gone, unavailable or in the hand -> PopFromPrevious).
- **Orphans**: FindChildrenAndOrphanThem 0x756BE0 (DeleteDependancys of a woman, Town::RemoveVillager): every villager of
  the town's abodes and homeless list whose mother she is -> 131 MORN_DEATH (GoHome with the mourning clips) if
  available, and the mother link cleared (grown-up children too, literal).
- **Deletion**: `villager::Delete` = `ecs::ToBeDeleted` = Villager::ToBeDeleted 0x7521B0 (no death: no reason, counters
  or corpse): its villager branch calls `villager::ToBeDeletedOverride` when it is marked: DeleteDependancys 0x74FD60
  (SET_DYING through the real exits unless already 13-15, the orphans, out of the abode / town / vagrants), then
  Living::ToBeDeleted 0x5EC0A0's StopReacting (any reaction, the mourning too). Town::RemoveVillager 0x73E210 calls
  RemoveVillagerFromWorshipSite 0x76C440 when the villager is at the site (+0xE0 & 2), before SetTown(0). The beam
  (Explosion) deletes. A new map clears the mourning in MagicLoop's OnLoadMap.
- **Callers**: CheckEveryTime (EXHAUSTION / CHANT, owner), CheckHungry (STARVING / CHANT, owner), old age (9, none),
  Drowning and EndPhysics in the water (6), EndPhysics on land (5), BeingEaten (3, owner: then a normal corpse),
  ReduceVillagerLifeByChant (4, owner; GET_TOWN_WORSHIP_DEATHS reads `TownDeaths::byReason[4]`), DestroyedByEffect
  (2: spells with the effect's player and damage, fire with its player and 0 (0x72F506), tornado (player, 1.0),
  impacts).
- The graveyard: Town +0x748 is `graveyard::GetGraveyard(town)` and its IsFunctional `abode_queries::IsFunctional`
  (SetDying's 120 / 600, ReactToDeathPriority); fn_0073E440 calls `graveyard::AddDead` when +0x748 is set (0x73E48B;
  AddDead tests IsFunctional and the 50 itself).
- VillagerDead: the multiplayer call FUN_0064DA80(0, 1) on the killer runs only for a reason not in {0, 1, 3, 4, 8, 9}
  and only when IsMultiplayerGame 0x552F80. The owner falls back to the neutral player g_game +0x18 + 0xA60 ×
  [+0x205A5B], and so does a missing killer. The town's adults (+0x618) are read before SetDying, so the dying villager
  still counts. GAlignment::Update 0x4143B0 does nothing without a player. fn_0073E0A0 adds the amount to Town
  +0xA08[killer × 32 + reason]. The TownStats counters are fn_00749780 (via fn_0073E440). For SACRIFICE the counter
  +0x58 = 0.
- HelpSpritesLowOnPeople is 0x71CBE0. The on-screen test of the help is fn_0071CE70 (IS + 0x30) on the villager's
  bounding box (fn_0081F1A0).
- DestroyedByEffect 0x7502D0 is vt +0x5F8, called from Object::ApplyEffect when the life reaches 0. Villager::SetLife is
  0x756B40 (vt +0x5B0).
- SetDying: DyingTimeWithoutGraveyard = info +0x290 (600), DyingTimeWithGraveyard = +0x294 (120), low words. The counter
  is set also when the villager was already dead.
- Dying 0x76A570: REACT_TO_DEATH is created only "not at home and (no town or no graveyard)", but SetDying has already
  removed the town, so the graveyard test never stops it (literal). The call is CreateReaction(me, 23, no player, 0).
- Dead 0x76A5E0: the soul is fn_00828790, made only out of the water. The child mesh is +0x204 ChildMeshHigh (heaven
  forced), the adult mesh GVillagerInfo::GetMesh 0x74F880 (+0x214 StdDetail). The skeleton mesh is the static 0xDCB164 =
  MeshPack[0x1FF], set every turn through vt +0xF4, with a vt +0xC4 call whose purpose is unknown (see
  [Pending](#pending)).
- Graveyard::MakeFunctional 0x595E00 sets Town +0x748 and Graveyard::DeleteDependancys 0x595CE0 clears it.
- DeleteDependancys 0x74FD60: a villager walking a footpath (TOP != 29 MOVE_ON_PATH and +0xCC set) leaves the footpath's
  walker list (+0x28, count +0x2C), +0xCC = 0. With a home it calls Abode::RemoveDeletedVillagerFromAbode 0x404220,
  which itself calls Town::RemoveVillager when the home has a town; else Town::RemoveVillager 0x73E210; else out of the
  vagrants (g_game +0x205BFC). SET_DYING runs ExitAtHome (row 13's 0xC0 is 0: LeaveHome).
- Living::ToBeDeleted 0x5EC0A0 also frees the data path (+0xAC) and the script remind (+0xB0) and leaves a dance (vt
  +0x978 / +0xB08) before the flock. Object::ToBeDeleted is 0x636670. Living::Dead's tail: CreateSmokyStuff(0, 1.0,
  0xFFFFFFFF), ToBeDeleted(0), returns 5.
- The soul (fn_00828790 / fn_00828900 / fn_00828950 / fn_00828990): a 12-byte record {next, LH3DObject*, elapsed ms} at
  the head of the list 0xEB9A7C, LH3DObject::Create(2) 0x80B4D0. It takes the source's drawn position and orientation
  with scale 1. Random(0, 100) (0x81D180, the CRT stream) is always drawn, before the clip name is tested. Alpha 105
  (colour alpha << 24 | 0xFFFFFF, white). In the last 500 ms: ftol((1 − (elapsed − (duration − 500)) × 0.002) × 105).
  Freed when elapsed + 110 > the clip's duration (anim +0x20). Updated every frame by fn_00828950 from fn_005E5CD0
  (0x5E6171) with g_game_time_inc. The clip statics are 0xEB9A80 / 0xEB9A88 (244 / 245) and 0xEB9A84 / 0xEB9A8C (247 /
  248), and the name "M_P_DEAD1" is 0xC38664.
- Mourning: a creature as the dead initiator gets REACT_TO_DEATH's priority (byte 0xD4FFBC) without the graveyard /
  self / 10-mourner tests, and in the setup the villager takes reaction 7 LOOKING_AT_OBJECT_REACTION instead of 205 /
  206. 206 walks only when 1.2 R < d and d − R > 1, to me + the step of the whole metres of d − R towards the dead
  (fn_0074E1D0 / fn_0074D3E0). R = ReactionInfo +0x44 (4.0 for row 23). The point and mourn rolls are drawn every facing
  / mourning turn.
- MakeChildOrphaned 0x7580D0: the child's mother is not her -> 0; IsVillagerAvailable -> SetTopState(131); mother = 0;
  1. FindChildrenAndOrphanThem walks the structures (+0x754, next +0x9C), their inhabitants (+0xA0, next +0xE4) and the
  homeless list (+0x768).
- The soul's alpha goes through `components::Alpha`, not the object colour (inferred). What is not ported is in
  [Pending](#pending).

## Routes and footpaths

- Footpath queries (Footpath.cpp): GetNextNode 0x5351A0, GetEndNonHiddenNode 0x535120, GetNearestPos 0x5352C0,
  fn_005351F0 / fn_00535270, `GFootpathLink::GetNearestPathTo` 0x536110 / Quick 0x5361F0, GFootpathLinkSave's resolve
  0x536FA0.
- fn_005351F0: the nearest node within maxDist, then the next one; 0x2D at the end.
- Loading a link (0x536FA0): no MultiMapFixed at the point → a planned abode within 0.05 m (+0x38); none → the link is
  deleted.
- `UseFootpathIfNecessary` 0x5362E0: GetNearestPathTo(+0x14, pos, 40) and SetupMoveOnFootpath ≠ 0 → 1
  (0x5362F5..0x536331).
- The walk to the footpath: SetupMobileMoveToPos(the node, 12) 0x5EDCE2, the goal being that node.
- Nothing within 40 m, no owner and no town → SetupMoveToWithHug(pos, final) (0x536417..0x53642F); the footpath fields
  are not touched.
- `SendFootpathsAroundObsticle` 0x537290 re-plans an original stretch through the planner and leaves a hidden node at
  the obstacle (bits 8 and 2), or bends a stretch of visible nodes round it in 18-degree steps; `StopGoingRoundObsticle`
  0x537DF0 takes the hidden obstacle node away again.
- A stretch that passes inside the obstacle's circle and whose head has no bit 1 is pending: AttemptRerender(head, tail,
  &pos) 0x5377E6; when it is found, the hidden node goes at pos (bits 8 and 2).
- Nodes with bit 1 (made by the planner) have no pending rerender; with both ends outside the circle the stretch takes
  the arc (0x5377F6..).
- `StopGoingRoundObsticle` 0x537DF0, pass 2: the node with bits 8 and 2 near pos is deleted and the stretch is
  re-planned without it.
- `ConvertCreaturePlanToFootpath` 0x538340 makes the footpath between two nodes from a route plan.
- `EraseNodesBetween` 0x538B90: the nodes between are deleted; a hidden node is kept and chained after the new ones.
- `GenerateRoutesBetween` 0x535850 drives the planner as SetStart(start, 0.5, holder, −1, −1, 0), SetDest(dest, 0, 0, 0,
  −1, 0, (range + 1) × 5, 1), then GameTurnUpdate(0) while it searches, at most 128 times.
- An RPFollow is 0x640C0 bytes (allocated on the heap).

### The route planner (RPlan)

- The planner lives in RoutePlan.cpp of runblack.exe: RPHolder 0x83B280..0x83C990, RPFollow 0x8639E0..0x864FB1, Route /
  RouteNode 0x869100..0x869451, RPAvoid 0x869020..0x8690FB, RPlan 0x86E0D0..0x86FC00, Point2D 0x86FD00..0x86FEB0.
- Everything runs with the x87 at 24 bits: every add / sub / mul / div is a float operation in the exe's order; fpatan /
  fsin / fcos keep full precision until the next store.
- Point2D (metres on x / z): length fn_0086FD50 = sqrt(x·x + z·z); `GetNormSq` 0x86FDA0; `GetRange` 0x86FDC0 = sqrt((z −
  b.z)² + (x − b.x)²); `SetSize` 0x86FD70 (f = len / length); `Normalize` fn_0086FE80 (f = 1 / len, returns len).
- `Point2D::GetHeading` 0x86FD00: 0 when x² + z² ≤ 1e-6 ([0x9A2BAC]), else fn_0086FC30(−z, x).
- fn_0086FC30(a, b) = atan2(b, a) by octants: a ≥ b and −b ≤ a → atan(b/a) (0x86FC30..0x86FC5C); b ≥ a and −a ≤ b → π/2
  − atan(a/b) (0x86FC5D..0x86FC91); −b ≥ a and a < b → atan(b/a) + π, or − π when b < 0 (0x86FC92..0x86FCDC); else −π/2
  − atan(a/b) (0x86FCDD..0x86FCF3).
- Constants: π [0x9A3C08], π/2 [0xFA51D4] = [0x9A3C08] × 0.5 (fn_0086FC10). The planner's float π is also stored at
  [0x9A3BCC] / [0x9A3BB8].

#### RPHolder (the obstacles)

- A 64 × 64 grid of 80 m squares ([0x9A3AE8]) over the 512 × 512 cell map. Each square keeps a chain of the obstacle
  circles that touch it.
- A square is filled the first time a search reads it, so the chain order follows the search. `FillSquare` fn_0083B380
  marks the square filled, then calls the fill callback for its 8 × 8 cells (0x83B3A5..0x83B3D3).
- `RPHolder::InitialiseSystem(cb1, cb2)` 0x83B300 (static) installs [0xEDDD14] / [0xEDDD18]: the world's
  `CheckSquareFunction` 0x54AFB0 for each map cell of a square, and `AddSpecialRPObjects` 0x54AF60 (the creature's).
- Layout: +0x50000 object count, +0x50004 / +0x50008 the plan's dest and start (kept outside every obstacle), +0x50020
  the margin `Object::AddToRoutePlan` adds to every circle, +0x50024 the follower (fn_0083B320; null for the footpaths'
  holders), +0x50028 the squares, +0x54028 the chains, +0x64028 the chain count.
- Ctor 0x83B2E0: +0x50024 = 0, then `Empty` 0x83B330 (no objects or chains, no dest / start, margin 0, every square
  unfilled).
- RPAvoid (0x14 bytes, fn_00869020): +0 object id (−1 anonymous), +4 active (0 once a later circle swallows it), +8
  centre, +0x10 radius.
- `RPAvoid::PointIsTotallyInside(p, r)` 0x8690A0: k = (this.r − r) − 0.001, below 0 → 0 (0x8690A4..0x8690C7); true when
  k² > dz² + dx² (0x8690D4..0x8690EF).
- `AddObject(id, c, r, notify)` 0x83B450: an obstacle with the plan's dest or start strictly inside it is not added
  (0x83B45B..0x83B4C7, dest checked first).
- `AddObject` with a follower (0x83B4DD..0x83B517): an obstacle within 0.1 ([0x8AB22C]) of the follower is not added.
  Otherwise its radius is capped at that distance, and the exe overwrites the argument, so every later use sees the
  capped radius.
- `AddObject` then checks the squares it covers (inside the grid, room for chain entries; 0x83B519..0x83B5AF), tests
  every entry already in those squares (0x83B5B5..0x83B6D8), and adds the entry and its chain entries
  (0x83B6DE..0x83B719).
- With notify == 1 it calls the follower's fn_00864D30 (0x83B716..0x83B728), with no null check.
- fn_0083B770 links the entry into every square of its circle's box, z outer, x inner.
- `SquareDoesNotContain(id, cellX, cellZ)` 0x83B3E0: true when no chain entry of that cell's square is `id`.
- `GetSidePointOfStartObject(obj, p, out, side)` 0x83B860: the tangent point from p to the circle. p on the centre → the
  circle's top (0x83B894..0x83B8BA); p inside or on the circle (r² + 0.001 > n²) → pushed out to it
  (0x83B8C4..0x83B920); tangent points at 0x83B923..0x83B9D8.
- fn_0083B9E0(objA, sideA, outA, objB, sideB, outB): the tangent between two circles. No tangent when h ≤ 0
  (0x83BA87..0x83BA9F).
- `GetFirstObject(from, to, ignore, hit, side, r)` 0x83BB60: the first obstacle on from → to, or −1. With r > 0 the
  segment is cut short (0x83BBAF..0x83BC1D). Square 0 counts as off the grid (0x83BCE4 / 0x83C1FB). Jump tables 0x83C284
  (the last square) and 0x83C2A8 (the next square). Nothing hit → hit = from (0x83C23C / 0x83C263).
- fn_0083C2D0(obj, side, end, start, hit): 1 when another circle cuts the arc start → end around obj. Circles apart are
  skipped, concentric ones ([0x9A2BAC] = 1e-6) give 0 (0x83C40C..0x83C45D). The cut shrink is 0.005 ([0x8C7674]).
- The search's obstacle hook [0xFA51D0] / [0xFA51C4] is called as hook(context, id) for an id != −1 (from
  `RPlan::SetDest` 0x86E4BA and `GameTurnUpdate`).

#### RPlan (the search)

- A best-first search over routes that go straight between the obstacle circles and around them along their arcs.
- RouteNode is 0x2C bytes (ctor 0x869150(parent, a, from, to, obj, side)). The copy ctor 0x869100 copies +0..+0x20 and
  sets +0x18 / +0x24 / +0x28 = 0.
- Route is 0x10 bytes (ctor 0x869380: +0 = +4 = +8 = 0, +0xC = 1). +0xC = shortcut done: fn_0086F560 skips the route
  while it is set. The copy `Route(Route*)` 0x8693A0 copies the nodes in order, linked both ways (0x8693A3..0x86940A).
  Dtor 0x869430.
- `RouteNode::GetArcLength` 0x8691C0 uses the PREVIOUS node's circle and side (+0x28). arc == 1 → from this.from to
  this.to; otherwise from the parent's `to` to this.from (0x8691F7..0x869225).
- Node length fn_008691A0: type 1 → the arc, else `GetRange(from, to)`. Cost fn_00869330: the previous node's +0x20 (0
  without one) + the node's own length.
- fn_00869460 deletes every node after `node` and makes it the tail.
- RPlan ctor 0x86E0D0 (+0x50..+0x70 = 0), dtor 0x86E0F0 → `FreeRoutes` 0x86E100. `SetStart(start, r, holder, objA, objB,
  sideB)` 0x86E200. `SetDest(dest, r, near, startCost, destObj, destSide, maxLength, linkHolder)` 0x86E250.
  `GameTurnUpdate(depth)` 0x86E9A0 = one search step.
- `SetDest`: a dest inside an obstacle → DestInside (0x86E319..0x86E35E). Found at once at 0x86E438..0x86E4B5. Otherwise
  it goes around the obstacle one way in this route and the other way in a second route (0x86E4EF..0x86E627). Starting
  on an obstacle is at 0x86E62C..0x86E79F.
- `GameTurnUpdate`: the best route by estimate, where a later route must beat it by 0.001 (0x86E9C8..0x86EA2C). Arc from
  the tip around the tip's object (0x86EB48..0x86EC31), then the leg to the target (0x86EC36..0x86EC8A).
- `GameTurnUpdate`: if obj2 overlaps the tip's circle, go around obj2 the same way (0x86EE64..0x86EF38). If they are
  apart, first check whether another circle cuts the tip's whole way around (0x86EF3D..0x86F07D), then go around obj2
  one way here and the other way in a copy (0x86F07D..0x86F1BB).
- fn_0086E7C0: the best route's tip within `near` of the dest → `Finish` fn_0086E7F0 (frees every route but the best;
  Found).
- fn_0086F390 removes a route from the list; with none left the plan has Failed (0x86F3D2..0x86F413).
- Visited points fn_0086E890(p, cost, limit): 1 rejects (too long, or the point already seen), else it is added. Blocks
  of 256 entries are searched from the newest (0x86E8F4..0x86E942); fn_0086FB80 adds a new block when the newest is
  full.
- Shortcut fn_0086F560(route, depth): plans the route's last stretch again from its tip back and splices the result in
  when found.
- Shortcut details: B is the tip (or its parent when the tip is not an arc) and E goes back to the first arc or the
  route's first node (0x86F5B6..0x86F61E). The nested plan's dest is a zeroed local (0x86F62C) with a dest object. At
  most 16 steps while it searches (0x86F72D, 0x86F726..0x86F761), and 4 tries with an earlier E (0x86F777,
  0x86F770..0x86F7BB).
- Splicing: the splice point is at 0x86F7C0..0x86F8F1, the nested route is reversed onto this one (0x86F8F1..0x86FAC6),
  and the route's last node is set at 0x86FAC8..0x86FB0B. A plan's +0x1C is written only by a parent plan's fn_0086F560.
- Fields RPFollow reads from a plan: +0x3C / +0x40 / +0x4C / +0x5C / +0x60 (in fn_00864420, fn_00864040, fn_00864D30).

#### The temple in the route plan (`CitadelHeart::AddToRoutePlan`)

- `CitadelHeart::AddToRoutePlan` 0x4680D0 (vt +0x7C0 of the heart; in openblack `CitadelHeartAddToRoutePlan`,
  `src/ECS/RoutePlanWorld.cpp`): a centre circle of radius 21.5 ([0x8C8428]) × 0.64 ([0x8CA28C]) + 1.1 ([0x8AB230])
  at the heart's MapCoords in metres (× 0.000152588, 0x4680E5..0x46810E), then seven arms from the heart's Y angle +
  3.83 ([0x9CE958]), 2π/7 apart, with circles of radius 21.5 × 0.11 ([0x8CA280]).
- Arms 0 and 1 have six circles each (three straight, then three turned by ±0.06, ±0.16, ±0.27). The other five arms
  have `count` circles: the heart's player (`CitadelHeart::GetPlayer` 0x468020 through vt +0x1C at 0x46815B: the
  citadel's player when +0x80 is set, else the player of the town at +0x94) is read at +0x60 → +8, its `GAlignment`
  value (0x46815E..0x468161), the same field `GPlayer::GetAlignmentValue` 0x64D6A0 returns. Then `fcom` with −0.7
  ([0x8CA288]): below → 2 (0x468173); else `fcomp` with −0.3 ([0x8CA284]): below → 3, else 4 (0x46817D..0x468192).
  Both tests are strictly below (C0 of `fnstsw`), so −0.7 gives 3 and −0.3 gives 4. There is no null check on the
  player.
- openblack reads the owner's alignment with `ecs::effects::alignment::Get(Temple::owner)`, the per-player value kept
  for the whole game (see [Player alignment](magic.md#player-alignment-galignment-gplayer-0x60-srcecseffectsalignment-componentsalignment)).
  Until 2026-10-08 it read an `Alignment` component on the player entity, which no player entity has, so every temple
  had 4 circles per arm. A difference against the old count only shows while the owner is below −0.3.

#### RPFollow (the creature's route follower)

- RPFollow is the creature's: Init from its ctors 0x47F527 / 0x47F80C, Update from fn_0048D250, MoveAlongRoute from
  fn_004907C0. It is its own RPHolder (+0..+0x6402B) and plans with up to 5 RPlans (the main one and the re-plans of
  parts of the route ahead).
- Ctor 0x8639E0: RPHolder, then the fields (+0x6404C = 1.0, +0x6403C = 5.0, +0x6405C = 1, the rest 0), then fn_0083B320
  sets the holder's follower to itself (0x863A5F).
- Layout: +0x6402C position, +0x64034 speed, +0x64038 distance along the current node, +0x6403C (5.0, reader not known),
  +0x64040 heading, +0x64044 next plan's start (the route's end while following), +0x6404C dest radius.
- Layout (continued): +0x64050 last step, +0x64054 state, +0x64058 dest pending, +0x6405C turns per update, +0x64060
  context, +0x64064 / +0x64068 / +0x6406C callbacks, +0x64070 current node, +0x64074 dest, +0x6407C / +0x64080 /
  +0x64084 SetDest's a / b / c.
- Layout (end): +0x64088 found once (1 at a found end, 0 at a new plan), +0x6408C the adopted main plan, +0x64090..
  plans, +0x640A4.. the route node each re-plan starts from, +0x640B8 plan count, +0x640BC best plan (−1 after case 1).
- Callbacks: +0x64064 done(context, code), with code 2 = re-planning after a found / dest-inside end, 1 = after a failed
  one, 0 = no pending dest and found, 3 = no pending dest and failed. +0x64068 first step(context, 0, the first node's
  length ≥ 0.01). +0x6406C the radius ahead (the creature's nav radius).
- `Init(context, done, step, radius, turns)` 0x863AC0 (0x863AC0..0x863AF2).
- The creature's fn_0048D250 (0x48D3BF) writes +0x64054 = 1 before its first `SetDest`.
- `SetDest(dest, a, b, r, c)` 0x863B70: +0x64084 = c first (0x863B7E..0x863B8E), then r → +0x6404C (the creature passes
  `GetNavRadius`, 0x4841BE). c is the plans' max length.
- `SetDest` by state: case 1 makes a new main plan (0x863C94..0x863D57). Cases 2 / 4 re-plan when d1 × 0.5 < d2, with d1
  = |plan[0] dest − follower| and d2 = |plan[0] dest − new dest| (0x863BB8..0x863C77, 0x863BE0..0x863C29): the dest
  moved by more than half the remaining way.
- `SetDest` case 3 re-plans the parts of the route ahead, every quarter of what is left (0x863D5C..0x863ED1): always the
  tail, otherwise while fewer than 4 re-plans (0x863DE6) and the node's cost is at least the threshold
  (0x863DE2..0x863E00).
- `Update` fn_00864040(hook, turns): up to `turns` (or Init's) search turns of the best plan. The hook is installed for
  these turns only (0x864053, with the creature as context) and cleared at the end (0x86415D / 0x86416D).
- `Update`: the best plan is the lowest estimate, a later one only when strictly lower (0x86408C..0x8640CB); state jump
  table 0x864178. With a hook and the dest farther than the plan's worst rejected cost, the plan is given up
  (0x8640FF..0x864134). While re-planning it waits for the next turn (0x86413F).
- `OnFailed` fn_008641F0: states 0, 1 and 3 do nothing. A pending dest is planned now; done(2), or done(1) for a failed
  plan (0x86423D..0x864284). Without a pending dest the code is ((failed != 4) − 1) & 3: 0, or 3 for a failed plan
  (0x864290..0x8642A6).
- `OnFound` fn_008642C0: +0x64088 = 1, then by state (jump table 0x864404; 0, 1 and 3 return at once, 0x8643FF). The
  main plan becomes the route (0x8642E4..0x864396). The next plan's start = the route's end (0x86431F..0x86432E). The
  first node's length is at least 0.01 (0x864361..0x86438D). Then the splice (0x864398..0x8643AD). With no plan left and
  a pending dest, it plans now (0x8643B7..0x8643FA).
- The exe moves the plans' RouteNodes into its route by pointer and keeps the adopted plan (+0x6408C) only as their
  owner.
- `FreePlans` fn_00864190: ~RPlan and delete of every plan, count = 0.
- Splice and shortcut fn_00864420(at), steps 1-6: delete every node after `at` (0x86445F..0x86446F); `at` → the
  sub-plan's route, the route's tail = its tail (0x864478..0x86448C); the main plan's end = the sub-plan's
  (0x864495..0x8644D1); r = the radius ahead (0 without the callback) and L = the current node's length
  (0x864517..0x864557); the look-ahead point P (0x864561..0x8645EE); the current node starts at the follower, and so
  does the previous one at both ends (0x8645F3..0x86463F).
- Splice and shortcut, steps 7-8: a local plan from P (0x864642..0x864668) to the target's end, cost from the current
  node + L (0x86468E..0x8646BE), up to 4 candidates (0x864683) and up to 32 turns while searching (0x8646CA,
  0x8646C3..0x8646F4; jump table 0x864894: 2 → a turn, 3..5 → stop). SetDest's near distance is 0.001 (0x3A83126F). If
  found and the plan's tip is the target's circle (or none), that is the shortcut (0x8646F6..0x864719).
- Splice and shortcut, steps 9-12: the new tip takes the target's side, circle and place (0x86471F..0x86476D); the route
  starts with the new nodes and +0x64044 = its tail's end (0x864770..0x8647A7); with the look-ahead, a straight node
  from the follower to P goes first (0x8647AF..0x86480C); the current node is the head again and the old nodes up to the
  target go (0x86480F..0x86484D); then the next candidate (0x86484F..0x86485E).
- `MoveAlongRoute` 0x864990: step = 0.1 ([0x9A3BBC]) × speed (0x864990..0x8649B0).
- `MoveAlongRoute`: still on this node → `PositionAt` (0x864B08..0x864B27). The first re-plan's node, once behind, is
  dropped (0x8649F3..0x864A62). Moving to the next node deletes the previous one; the new head gets +0x1C = 0 and its
  start = its end (0x864AAE..0x864AEA). The route's end is at 0x864B31..0x864B9B.
- `PositionAt` fn_00864BA0(&+0x6402C, &+0x64040, t): an arc with no circle before it is treated as straight
  (0x864BAC..0x864BBF).
- `PositionAt` on an arc: θ = ta + (tb − ta) × t / length (0x864C6D..0x864C79); the heading is θ + π when going the
  other way around (side 2, 0x864C81..0x864C99); position = c + (cos θ, sin θ) × r with fcos / fsin at full precision
  (0x864C9B..0x864CB4).
- `PositionAt` on a straight node: only when longer than 0.001 (0x864CBD..0x864D24); heading from `Point2D::GetHeading`
  0x86FD00.
- fn_008694A0(list, entry, from, holder, &pos): does the route from `from` meet the new obstacle? Each straight node
  from its start (the follower's position for `from`) to its end; the first obstacle on it, ignoring the circle before
  it (0x8694CB..0x86951E). The hit must be this entry (holder + index × 0x14, 0x869514..0x86951C).
- On a hit, n ends there, the route is cut after it (0x869582..0x8695A9). An arc on the previous node's circle means the
  new circle overlaps it (0x869520..0x86956B). The first node ends at the follower (0x8695AC..0x8695D5), then the cut
  (0x8695D8..0x8695F1).
- `OnObjectAdded` fn_00864D30 (from `RPHolder::AddObject` with notify 1, 0x83B728): state 1 → nothing. State 4 → the
  main route, then the re-plans; 2 / 6 → only the re-plans; 3 → the route (0x864DD8..0x864DEE).
- `OnObjectAdded`: if the main route meets it, a fresh plan from its tail (0x864DF4..0x864E80). Each plan's routes
  (+0x60, next +8) are tested from their first node (0x864EA9..0x864ECE); all plans are freed on a hit. In state 3, the
  route's end becomes the next plan's start and SetDest re-plans ahead (0x864F3F..0x864FA7).

## Test hooks

- `OPENBLACK_VILLAGER_TRACE=1` (or `=<n>`, creation index): `Villager trace:` lines with the creation (position,
  age, food, lastCheckTurn, counter, 85 or 16), each `SetState`, `SetTopState a → b = code`,
  `SetCurrentAndDestinationState`, `pause 239 → s (rand, threshold)`, `AdjustTownModifier`, each periodic check, a
  summary every 100 turns and each call to the fire entries and exits (`EnterPutOutFire(final, s) = r`,
  `ExitPutOutFire(final, s) = 1`, `EnterOnFire`, `ExitOnFire`), `ExitReaction(s) reactive r`,
  `ExitReactToTeleport(s) same r`, `StopReacting`, `PopFromPrevious stored a -> resume b = code`; the summary carries
  the position, the destination and the walk mark (L / O / E / S / F / A / -).
- `OPENBLACK_TEST_VILLAGER_LIFE="<life>[,<n>]"`: on turn 2 sets the life of all villagers or of villager n.
- `OPENBLACK_TEST_VILLAGER_STATE="<state>[,<n>]"`: on turn 2 calls `villager::SetTopState` and writes the code.
- `OPENBLACK_TEST_VILLAGER_BORN_IN_WATER="x,z"`: on turn 2 creates a Celtic villager (Housewife, 25 years old) there.
- `OPENBLACK_TEST_VILLAGER_POISONED=<n>`: on turn 2 poisons villager n.
- `OPENBLACK_TEST_ABODE_LIFE="<index>,<life>[,<pit>]"`: on turn 2 the index-th abode that is not a storage pit
  (the order of `OPENBLACK_TEST_HIT_ABODE`; with pit 1 the index-th storage pit) down to `life` through
  `abodes::ReduceLife` (the taps, the site and, for a pit or a town centre crossing 0.75, the emergency).
- `OPENBLACK_TEST_TOWN_EMERGENCY="<town id>"`: on turn 2 `town_emergency::SetInStateOfEmergency` of that town.
- `OPENBLACK_TEST_VILLAGER_KILL="<reason 0-9>[,<n>[,<turn>]]"`: at turn 2 (or `<turn>`) VillagerDead(reason,
  GetPlayerOf, its life, 1) of villager n (or all); with `OPENBLACK_TEST_CORPSE_TURNS=<n>` the corpse's counter is n.
  The trace adds `death: <reason> killer <p> owner <p> amount <a> drop <d> help <kvwlp>`, `death: town <id> deaths[<r>]
  = <n> total <n>`, `setdying: counter <c> (graveyard <0|1>)`, `dying: -> 15 (reaction ...)`, `dead: smoke, soul <clip>
  mesh <m>, skeleton` / `dead: smoke, water, skeleton`, `dead: <counter>` every 100 turns, `dead: vanish`, `mourn: <state>
  of <dead>`, `orphan: -> 131`; the alignment trace adds `Alignment: player <p> death <r> <v>`.
- `OPENBLACK_TOWN_TRACE=1[,<every>][,raw]`: per town, every `<every>` turns (50) and whenever the first of order 1
  changes: `town <id> turn <t> pop <p>: [16 Sleep 1.000 raw 6.250] [15 Relaxation 0.100] …` (the 17, order 1) and
  `avg <a>` on the 50-turn turns; with `,raw` also order 2.
- `OPENBLACK_VILLAGER_TRACE`: the distribution writes `civic: t=<t> k=<k> d=<d> v=<v> tmp=<m> -> skip(child)|skip(nocs)|
  cut|cs=0|cs=1`.
- `OPENBLACK_TEST_TOWN_DESIRE="<d>,<boost>[,<town>]"`: on turn 2, SetBoost like SET_TOWN_DESIRE_BOOST (re-sorts
  order 1) in all towns or in the one with that id.
- `OPENBLACK_TEST_BUILD_AT="<x>,<z>[,<desire>]"`: on turn 2, the building side's ForceBuildingOfPlannedAtPos(MapCoords(x,
  0, z), desire × 5) (what BUILD_BUILDING does; the Land 1 temple `"1915.05,2508.89"` waits for the CitadelHeart, see
  above). The trace
  adds `build: need|supplies|pit for materials|abodes desire|civic desire`, `build goto|184:` (ring, distance,
  footpath), `build 39:`, `build 40:` (ring, distance, look, wood dropped / added), `build 41:` (a, f, u, pile, value,
  x, % built, next / supplies / release), `build enter|exit <state> site <id> builders <n>`; `OPENBLACK_TOWN_TRACE` adds
  `sites:` (every 50 turns: each site's building, builders / max, pile, % built, repair).
- `OPENBLACK_TEST_VILLAGER_AGE="<age>[,<n>]"`: on turn 2, only Living::SetAge (the birth turn), without
  meshes or bits (12.99 → 13 and old age). `OPENBLACK_TEST_HOMELESS=<n>`: on turn 2, MakeHomeless of villager n.
- `OPENBLACK_VILLAGER_TRACE` adds `home 36: …` (to the door / no abode -> far / tent / wander / vagrant 130),
  `home 37: not there|arrive (present <n>)|tent|hungry 163+arrive|builds its home`, `home 38: emergency|needs(t=…)|
  disciple|something|nothing r4=<r>`, `exit-home <s> -> <next> (stay|leave, present <n>)`, `sleep 120: life <l> ->
  keep|wake`, `tent: tree|spiral try|fail`, `food: …`, `eat: …`, `home-food: took <m> held <h>`, `age: grown|rescale|
  old age r= n= d= -> die|live`, `homeless: into abode|list`, `abode: moves|too crowded`, `vagrant 130: …` and, every 100
  turns, `home: town <id> inside <n> asleep <m> tents <k> homeless <h> vagrants <v>`. `OPENBLACK_TOWN_TRACE`:
  `shuffle: <abode> -> <abode> (swap|take …) = <r>`.

Checked (2026-10-01): Land1, 58 villagers, all 85 → 163 with code 1, checks every 9 turns, wear 2e-6 per turn
when walking; with `LIFE=0.2` and `STATE=246`, 14 of 55 pause (239 → 246; ~27 % was expected) and on the next check they
go to 36; with `LIFE=0`, `died (EXHAUSTION)` without a hang; `OPENBLACK_TEST_THROW_VILLAGER`:
85 → 10 → 11 → 163. Land2, `OPENBLACK_TEST_WORSHIP="1,0.5"`: they reach 59 and 60; three villagers are born in the sea in 16.

After the audit (2026-10-01): normal Land1, 338 turns, no changes. Land2, `WORSHIP="1,0.5"` and `LIFE=0.25`, 2452 turns: 25 direct exits 60 → 248 and 8 after a pause (239 → 248), spread
among 21 villagers until the end (before, from turn 2333 only one came out). Land2,
`OPENBLACK_TEST_HUNT_VILLAGER="0,3"` (turn 3, before the script creates the sea villagers, so the first one in the
registry is on land): 85 → 17 DOWNED → 18 BEING_EATEN → eaten.

Fire and teleport through the core (2026-10-01): Land1, `OPENBLACK_TEST_FIRE="1785.2,2652.6,450,abode,20"`, 650
turns: the same states as before (85/1 → 215 → 220 ⇄ 216, 163/1 → 219) and one entry and one exit per change (3213
`EnterPutOutFire(216, 220) = 1` for 3213 changes 216 → 220, 3221 for 220 → 216, 11 `(215, 220) = 1`), plus those of
replanning and arriving (22 `ExitPutOutFire(220, 220)` / `EnterPutOutFire(220, 220)`, 6 `ExitOnFire(219, 219)` /
`EnterOnFire(219, 219)`), no 0x2E nor 0x2F; before, those last ones were missing and ExitOnFire was never called. The
216 ⇄ 220 back-and-forth every turn was already there before. Teleport (`OPENBLACK_TEST_TELEPORT="1785,2655,1830,2660,7,walk"`,
`_TURN=300`): 1 ⇄ 201 as before and, in one of the passes, 201 → 202 → jump (saving 26 m) → `SetTopState 202 → 163 =
0x1`.

A second review (2026-10-01): teleport as
above, 1365 turns: the 6 villagers that react make **one** walk to the stone (1 with FINAL 201, about 70 turns),
arrive (1 → 201 → 202), jump (saving 44-45 m) and `PopFromPrevious stored 209 -> resume 163`; the back-and-forth
1 ⇄ 201 is over. The villagers keep walking and arriving (208 walks finished, median 170 turns; none in MOVE_TO_POS
without moving between two summaries), 18 unported cases of the PathfindingSystem remain in STEP_THROUGH. Fire
(`OPENBLACK_TEST_FIRE="1785.2,2652.6,450,abode,20"`, 747 turns): the same states (85 → 215 → 220 ⇄ 216 → 163,
163 → 219), `ExitReaction` on each exit of 215..220 and 12 `StopReacting` when it goes out; no `pause 239` nor
`Stuck in an invalid state`. `OPENBLACK_TEST_MAP_CYCLE` over Land1-5 without hangs.

With `ReactionValidate` connected (2026-10-01, with a temporary trace in `VillagerCallValidate` that does not stay in the code): fire (`OPENBLACK_TEST_FIRE=
"1785.2,2652.6,450,abode,60"`, 785 turns) 12 villagers 85/1/114 → 215 → 220 ⇄ 216 and back to 163 with
`StopReacting` when it goes out; `ReactionValidate` runs every turn for the TOP (1080 times in 216, 54 in 220, 13 in 215)
and for the FINAL (410 in 220, 3 in 215), without any pop (the object, the home, is still available; when it goes out
`ExitPutOutFire` → 163 does `StopReacting` before a reaction state without an object is validated). Teleport
(`OPENBLACK_TEST_TELEPORT="1715,2595,1760,2640,7,walk"`, `_TURN=550`): 1 → 201 → (20 turns walking, FINAL 201
validated every turn) → 202 → jump of 63.6 m → 163 and `PopFromPrevious stored 245 -> resume 163`. **Beware of the
`walk` hook**: its walk with FINAL 163 does not admit reactions (`IsAvailableForReaction` 0x763390: the +0xEC
of 163 is 0), so the hook's villager does not react; the test was done with FINAL 245 changed by hand in
`TeleportDebugHooks.cpp` (not saved). `OPENBLACK_TEST_MAP_CYCLE` over Land1-5, Greek God, TwoGods and Kapa's Land1
without hangs.

## Tests

`test/test_villager_core.cpp` (fake table in the Locator and scripted rolls with `villager::SetRandForTests`):
food with one and two rolls, lastCheckTurn, counter and water rule, random order of the constructor, 85, 0x2E by TOP and
by FINAL, 0x2F (→ 163), 0x23, setting TOP clears FINAL with the town, the PREVIOUS rule, AdjustTownModifier, the
pause (with and without poison, without a roll in 239 or without the mark), 239 → FINAL, check every 9 turns, wear,
EXHAUSTION / CHANT (also with `WorshipVillager::atSite`), hurt → 36 (switched off in one case; 19
with food, knocked down), SetupMoveToWithHug with `moveState` keeps FINAL (and 0x2F without a walk), POWER.

`test/test_villager_food.cpp`: the hunger batch (0.79919), the strict damage, the interruptions (0xD0 / 0xD4,
the disciple, STARVING / CHANT), the reference amounts (74, 65, 63, 54, 83, 85), ChangeStateToFindFoodToEat
(117 / 118 / 36 / 33 / what it carries / 0), EatFoodHeld (1.0 and 0.9864865, NaN → 0), the double take of
GetFoodFromHome, 117 / 118 / 212 and 34. `test/test_villager_home.cpp`: 37, ExitAtHome with PresentAtHome,
HomeDecideWhatToDo (0.7875, GameRand(4)), pregnancy and child, sleeping (6 cycles from 0.4 to 0.70000005),
CheckWhenGoingToBed once per stay, the tent (tree, the other side, full → (−20 m, +10 m)), DoGoingHome without a home,
the score and FindAbodeWithSpaceInTown, the home's list, CheckNeedNewAbode → 129 → 36, 130, 238, 234, the Shuffle and
Abode::Process (1001 turns). `test/test_villager_age.cpp`: the pure layer (63 years, r³), SetScaleForAge with scripted
GameFloatRand, the 13-year-old child (18 years, counts, 234), old age and UpdatePregnancy (was WomanSpecial). In `test_villager_decide.cpp`
a hungry villager eats immediately (117) and one without a home or town goes to 130.
`test/test_villager_death.cpp`: the help table, the dying time and clips, DeadTick, the soul's clip / alpha /
expiry, the death alignment and the mourning turns; VillagerDead's guards, fields and order, the owner / killer / town
counters / pulse, the help sprites, the world population, Dying, Dead (smoke, skeleton, vanish, script-controlled,
SACRIFICE), CannotExitState, DeleteDependants, the orphans, the mourning priority, setup and states 205-208 (no
landscape in the tests: everything is water there, so no soul).

`test/test_villager_build.cpp` (the fixture of test_villager_resources plus a mock building side): the build
factor and amount at 24-bit float steps, the cycles (29 / 35 / 130 / 82), the ring index, the wood source scores, 41
(the call order value → RemoveResource → BuildBy → stats, the next ring point, the pile cap, the release without
RemoveBuilder), 40 (the walk, the index range, the drop of all the wood, the kept counter), GotoBuildingSite, 184,
GotoStoragePitForBuildingMaterials (+0xFC only from a non-building TOP), 39, EnterBuilding / ExitBuilding, the four
CheckSatisfy functions (+0x5E4 before the request), SetupBuildingObject(abode) and Repair_Town's plans.

`test/test_villager_farming.cpp` (mock field / fish-farm sides): the field and fish-farm scores (GDM 0.5 at half the
limit), the catch by season / capacity / tribal power, FindBestField's strict order, CheckSatisfyFoodDesire (the field
when the farm has a fisherman, a tie keeps k's order, the drop-off, the flock TODO), SetFarmerGotoField (the point after
the walk), 67 / 68 / 69 (the digging for 0, the unsigned cost at 170 held), Enter/ExitFarming, 55, the trip 37 / 74 /
111 / 148 / 150 → 31, Enter/ExitFishing. `test/test_villager_reactions.cpp`: the pure scores, the standard
turns (3000 / 2222), the same-type flags, SetReactionDoneWhen, IsAvailableForReaction(type), a wood reaction's life
(score 78, the setup, the same-type refusal, 21's walk, ProcessReaction's end, ShutDown, the initiator gone), the wood
gates (124 / 125 held, life 0.3), a miracle reaction type with no slot, 20 / 21 / 22. `test/test_villager_forester.cpp`:
FindTreeNearVillager's code, IsTouching, the raw 47, GotWoodDecideWhatToDo, 52, 50 after the fall, 53 without a town.

`test/test_villager_repair.cpp` (the fixture of test_villager_home plus a mock building side): the +0x50 column
(206 / 1 / 48, spot rows), ReactsToTownEmergency by GetFinalState (the walk home FINAL 37 yes, at home TOP 38 no,
FINAL 40 yes, 220 by PREVIOUS), CallToTownEmergency (a builder: its exit told 242 is the real ExitBuilding, RemoveBuilder
and no site; no draw; at home unchanged), the tap (not inside: nothing; inside: the two draws first, FINAL 197,
PREVIOUS 163, flag 1, out of the home; DYING: nothing), 197, 242 (no town; the 2 pi draw, the ring point at d = 20 for
20 people), CongregationDistance (10 / 15 / 20 / 29.5 / 30), 243 (GameRand(12) / GameRand(5) branches, no town),
CheckSatisfyToRepair (a mock GetBestRepairBuildingSite with the +0x638 and desire > 0 filter: the ordinary site of a
rock-damaged house and a +0x638 site of desire 0 are not taken; the real filter is the building side's, tested there),
ArrivesHome's repair branch (repairs; site full → in; hungry, hurt, full life → in) and the repair cycles (the real
life::IncreaseLife).

`test/test_living_landing.cpp`: OriginalRows (the drawn pi / 2), the villager's three poses and their yaws
(reference values: flat 0, on the right / left side, the strict thresholds and NaN), the animal's reversed landType and
heading, YawFromRotation (was GetYXZYaw), VillagerLandedClip, villager::SetYAngle (+0x4C drawn, GameAngle), InterfaceSetInMagicHand's +0xE0 0x20,
ObjectYAngle (exact from the game angle, else the GetYXZ yaw), EndPhysicsWithoutBody (landType 3, nothing for a
non-Living).

## Assumptions (inferred / approximate)

1. **(approximate)** "Dancing" (Living +0xD8, the DanceGroup) is approximated with `WorshipVillager::dancing` (set by
   AddDancer / FindDanceGroup and removed by ExitAtWorshipSite / RemoveVillagerFromWorshipSite) or `TOP == IN_DANCE`
   (VillagerSpeed.cpp).
2. GRand: `villager::GameRand/GameFloatRand` forward to `game_random` (LHRand over the synchronised seed,
   engine-math.md «Random numbers»). **(approximate)** the sequence is not that of an original game: other
   systems that draw from the same stream (animals, trees…) still use openblack's generator or are not ported.
3. **(approximate)** The archetype sets the speed of 85 CREATED when creating the villager; in the original the first
   SetTopState sets it. It is not visible: CREATED does not walk.
4. **(approximate)** UniqueId (UniqueKeyHeap::GetUniqueIdFromAddress 0x7E19A0) is the creation index (`object_index`,
   +0x3C): it decides in which periodic check old age is looked at, and the SetSpeed factor.
5. **(inferred)** +0x11C: the type of the union comes from bw1-decomp; it is not ported until a job reads it.
6. The game turn is `Game::GetTurn` (g_game +0x205A40); without Game (tests) it is 0 or that of `SetTurnForTests`.
7. **(approximate)** Turn order: villagers and animals in two passes, everyone's movement before the logic, and
   the villagers in registry order (not that of the list g_game +0x205BBC).
8. (No longer an assumption.) VillagerDead keeps the villager as a corpse (SetDying, 14, 15) and CallState runs
   after a death; the player of VillagerDead is GetPlayer (the town's owner).
9. **(approximate)** The TOP changes of the hand, physics, animals, LANDED and the Gui do not go through exits or
   entries (their Enter/Exit of the original are not ported). Those of fire and teleport now do (core).
10. (The invented idle walk has been removed.) A walk with FINAL 0 (only debugging tools) returns to 163
    via the compatibility path (the original would do SetTopState(0)). With a FINAL other than 0 the arrival is the
    exact one (SetTopStateToFinal, 0x5EC28E).
11. **(approximate)** The exit of MOVE_TO_POS / MOVE_TO_OBJECT (ExitMoveToPos 0x5EDDA0: CircleHugInfo::Reset and
    +0x60 = 0) is not ported: it counts as 1 (what it returns) and warns once.
12. **(approximate)** AdjustTownModifier ignores a desire outside 0..16 (info.dat has none).
13. **(approximate)** A disciple type outside the g_DiscipleInfos table (13 rows) does not ignore needs.
14. **(approximate)** Without clip resources (the tests), IsReadyForNewAnimation gives "finished".
15. **(approximate)** Bit 0x2 of +0xE0 (at the worship site) is read from `flags` or from `WorshipVillager::atSite`
    (the worship code) until it moves to `flags`.
16. (No longer an assumption.) The "hurt → 36 GO_HOME" rule of CheckEveryTime (0x7505C3) is switched on: 36 walks to
    the door, and 37 ARRIVES_HOME is ported.
17. Neutral (they do not invent behaviour): SpecialVillager. ProcessReaction, the Town +0x5E8 pulse, DROWNING 16, the
    world population and the skeleton are in; the constructor does not call SetSkeleton yet. CheckHungry,
    CheckChildGrownUp, WomanSpecial (`UpdatePregnancy` in openblack) and CheckDeathFromOldAge are in.
18. Neutrals (return 0 / do nothing, with TODO and address): DiscipleDecideWhatToDo 0x751720, IsMotherAlive 0x757F40
    (keeps the mother), ChildGotoCreche 0x7579F0, RemoveFromDance. Now in: CheckHomelessMoveIntoAbode,
    ChangeStateToFindFoodToEat, CheckWhenGoingToBed, CheckNeedNewAbode, the homeless branch of DoGoingHome (tent 238 /
    130), ExitAtHome 0x761B40, and TownDesire::CheckVillagerNeededForTownDesire 0x745FF0 (leaves 0 or 1 in eax, 0x7460EB
    / 0x7460F7); Town +0xF1C is written by SetInStateOfEmergency.
19. **(approximate)** The door (Game3DObject::GetDoorPosition 0x63AFE0 via LH3D vt +0x1C4, without symbols): the
    L3D's door point (`L3DMesh::GetDoorPos`) by rotation·scale of the home's Transform; without a door, the
    home's position (literal, 0x52E3A4).
20. **(inferred)** Abode IsAvailable (+0xA & 1) = the entity is valid; IsBuilt (+0x58 & 2, +0x5C ≥ 1) = yes for all
    homes **(approximate, see [Pending](#pending))**; the home's GAbodeInfo is that of its
    number and mesh.
21. **(approximate)** CheckForClearArea: the entities of each cell are taken from the map cells
    (`TownCellObjectsInterface::ObjectsInCell`: the fixed list, then the mobile one) and their radius `Object2DRadius`: same set, different order (the
    result is yes / no). With the radius of openblack's homes (≈5.4 m) and the door at ≈1.5 m from the centre, many 245
    spots fall "occupied by its home" and FindClearArea moves them aside: the villagers sit somewhat further from the door
    than in the original (effect of 19).
22. **(approximate)** The town's list of homes (+0x754) goes from newest to oldest (AddStructureToTown
    0x7399C3..0x7399CF inserts at the head); openblack sorts it by creation index (+0x3C) from highest to lowest, as
    if each home entered its town when created. It decides the height of GetCongregationPos (that of the last one read:
    the oldest), the fallback base with one home and which homes remain in the ring with more than 100. The plans
    (+0x9A8) go in order of arrival (AddPlanned 0x73D08A..0x73D0AD appends at the end), like `plannedAbodes`.
23. **(approximate)** SetupMoveToOnFootpath 0x5EDD20: GFootpathLink::UseFootpathIfNecessary 0x5362E0 is not
    ported: always the direct walk (SetupMoveToWithHug), which is the literal behaviour without a footpath link.
24. **(approximate)** LookAtPos 0x5EC550: the angle Living +0x5C is `WallHug::yAngle` (radians, rounded to 2048ths).
    It takes two arguments (ret 8); with a mode other than 0 / 1 / 2 the step is the mode itself (0x5EC57A).
25. **(approximate)** 114: the "available" mother (vt +0x2C) = valid entity; IsNavigable (Collide & 2 and not & 8):
    openblack's Collide only knows land / water (bit 8 is never set).
26. **(approximate)** SET_TOWN_CONGREGATION_POS: openblack's script parser (Script.cpp GetParameter) only makes a
    vector from a two-field string (and its y is the terrain height, not a third field): the cache height is 0,
    the literal behaviour for two fields. A three-field string (MapCoords::Set 0x6032E4 would read it unscaled) does not
    reach the command in openblack. None of the game's scripts uses three fields.

27. **(approximate)** The build factor and amount are computed in float steps (the x87 runs at 24 bits in the game
    logic, GameDistance.h): alignment −0.1 gives u = 51, −0.3 gives 52, and the housewives' u at −0.25 / −0.75 is
    42 / 46 (the earlier 50 / 53 / 41 / 45 came from an extended product). (not verified) in the game.
28. **(openblack)** `buildPosIndex` is its own member instead of the +0x118 union; SetupBuildingObject(abode) tries
    AddBuildingSite once (the original loops); null-town guards where the original would read null.
29. DecideHowToGetWood's forest results walk to the forest / BigForest (SetupGetBuildingSupplies 0x75876F /
    0x75878B). Until Town::AsssignTownFeature 0x73EAC0 fills the town forest lists, the global FindForest gives the
    forest.
30. FPU at 24 bits (fn_007DEE00, `and cw, 0xFCFF` at 0x7DEE0D): the x87 intermediates of the villager code
    are float precision even on the x87 stack; the double models are superseded: LoadFactors / TownNeedsSum /
    TownNeedsFactor / DropOffScore / DropOffFraction / DroppedLogValue / the speed product, PointTurns /
    MournTurns / SoulAlpha and the desire chains of TownDesire.cpp are float steps now.
31. **(not ported)** The rows 51 FORESTER_CHOPS_TREE_FOR_BUILDING, 54 ARRIVES_AT_BIG_FOREST_FOR_BUILDING, 185
    ARRIVE_AT_PUSH_OBJECT, 188 / 189 (wood from a tree / pot for building) and 232 WAIT_FOR_WOOD stay `k_TodoEntry`:
    none is entered in W120 (no `push` of those states before a state change; CheckForClearArea finds no pushable
    object; SetupWaitForWood needs g_game +0x14 & 0x40000, never set).

32. **(approximate)** `workPos` (+0x11C) is its own member instead of the union with the script's clip / loops.
    The field / fish-farm lists of the town (+0x780 / +0x788) are the building side's component walks, newest first. Fishing's
    "on the arrive point" compares x / z of the villager with the arrive point through the metres its walk's goal went
    through (openblack keeps no walking altitude).
33. **(inferred)** "+0x24 & 0x40" (in the physics) = `PhysicsObjects::IsFlying` (in the physics and not a
    resting proxy: a resting proxy is in the list without the bit; confirmed by the physics code); "+0x24 & 4" = in the hand
    (`fire::traits::InHand`); a pot's IsAPotFromABuildingSite (+0x74 bit 3) = a building site's pile
    (`building_sites::SiteOfPile`); the reaction a miracle reaction map follows = the one of its type its object started
    (their maps keep no public id); PhysicsObject::GetPlayer = the local player when the hand threw it.
34. **(approximate)** A miracle reaction type meeting a slot holder runs its own Records(again) check after the switch
    rule (the original's switch path does not); two miracle reaction maps at once are scored in ReactionValidate's order. Its
    availability is the original's: the whole IsAvailableForReaction(type) 0x763390 first (0x6E401D), then the switch.

No longer assumptions: the writer of +0x24 & 0x400 (controlled by script) is GameThingWithPos::SetControlledByScript
0x402240 (`ecs::script_held`; also the vortex, fn_005FE3B0 0x5FE474); the walking state of SetupMoveToWithHug is
GLivingInfo +0x124 (`moveState`).

## Addresses of the original, moved out of the code

The code's comments describe what it does in plain words; the original's addresses and function names they used to quote are kept here, next to the openblack symbol each one corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `MobileWallHug::SetYAngle vt+0x524 (0x639260), Object::GetYAngle vt+0x508 (+0x4C)` | the heading setter and getter used by THING_JC_SPECIAL | `SetYAngle / GetYAngle (super_villager)` |
| `+0x39A / +0x38A` | GVillagerInfo::drowningTime offset: +0x39A in memory, +0x38A in info.dat (600 turns for every villager) | `ecs::DrowningTime (VillagerDrowning.cpp)` |
| `0x76A7A0..0x76A7CB` | Villager::Drowning's death call: VillagerDead(reason 6, player, 0.01f = 0x3C23D70A, 1), player = GetPlayerWhoLastDroppedMe (vt +0x6C)->GetPlayer (vt +0x1C), else lastPlayerToInteract (+0x104), else neutral | `ecs::VillagerDeadDrowned` |
| `dec word ptr [esi + 0x58]` | the state counter (Villager +0x58, u16) is decremented without a check, so it wraps when already 0 | `ecs::VillagerDrowningState` |

## Pending

Not ported, or to check in openblack:
- Script flocks: who writes g_game +0x250090 +0x3C (the loops' SetControlledByScript), the g_game +0x14 & 0x8000 flag
  of FLOCK_DISBAND, the flock's +0x30 and +0x5C (calm) readers, a town's animals for CALL_IN, DANCE_CREATE 054.
- Released from a script: the g_game +0x14 & 0x8000 flag, the dropper's player of the death, RemoveThingMusic.
- Home: CheckGetPregnantAtHome neutral and no births (a pregnant woman without a birth would stay at home forever); the
  dance in DoGoingHome; SetVillagerDisciple in 234 and HousewifeStartsGivingBirth; Town::RemoveVillager only with lists
  and counts; the worship rows 248-250 (`DoGoingHome(249, 250)`, ArrivesHome, SleepInTent, ExitAtHome) await the
  worship code; the hand that picks up a villager from inside; the night windows still use `inhabitants` instead of
  `presentAtHome` (Abode::Draw 0x515F78); the in-game captures.
- Death: GameStats (+0x4C, +0x106C, +0x1124), Town +0xA08 (no reader), the multiplayer FUN_0064DA80, disciples, the
  footpath walker list, the soul's per-frame update (openblack: once a turn, the clip runs per frame), SetSkeleton in the
  constructor (draw order), the sacrifice, ReleaseFromScript's and the creature's VillagerDead.
- Reactions: UpdateHowImpressed 0x7634C0 (AddReaction), the interact desire (g_game +0x205A50, 0), the disciple columns,
  MakeCreatureEmpathiseWithPlayerTownDesire (186), the belief-only branch of the spread (0x763410), dancing
  (RemoveFromDance); WallhugValidate 0x756990 (47's validate) is not ported.
- `DeadTreeEndPhysicsReaction` (0x511413) has no caller yet: the DeadTree endPhysics handler, and Tree::EndPhysics
  0x74BBE2 → `CreateDeadTreeReaction` for a tree thrown by the hand.
- Creation by script: this page used to give the integer-slot rule (FindTownWithID of slot 0) to CREATE_VILLAGER_POS
  ("AALN", 0x715A4C); in the original it is CREATE_TOWN_VILLAGER's. Check which command openblack's
  `lhscriptx::Script::IntSlot` rule is applied to, and whose signature "AALN" is.
- Whether openblack's vagrant list is still always empty (an earlier note said so, when its writers were not ported).
- Assumption 20: whether IsBuilt is still "yes for all homes" now that the building sites are ported.

Unverified (kept as the notes say them):
- `GoHomeAndChange` (VillagerHome.cpp): "0x76185A..0x761877: SetTopState(inside ? 38 : 37)" and "0x76186D:
  SetTopState(0xA3 163)". It is not clear which condition picks 163 against 37 / 38.
- `CheckWhenGoingToBed` (VillagerHome.cpp): "0x760BDD..0x760C1F: the first man of the abode's list that is inside ->
  CheckGetPregnantAtHome (hers); 1" and "0x760C26..0x760C6D: each woman of the list that is inside -> her
  CheckGetPregnantAtHome". Read as: a woman going to bed checks herself when a man is inside, and a man going to bed
  checks every woman inside. Not confirmed.
- Two "turns per year" fields: VillagerAge.h "GGameInfo +0xC (0xD01A04): turns in a year, ftol(1500.0f)" and
  VillagerBirth.cpp "GGameInfo +0x14 (0xD01A0C): +0x10 (36000.0f, the ctor 0x55775E) / 365.25". The note on
  BirthCounterRange calls +0x10 "turns per year" too. Which one is the calendar year was not checked.
- State-table column "+0xEC": VillagerResources.cpp "Infos[GetFinalState()] +0xEC (0xDB9F54) != 0 -> it" (carried
  object) and VillagerCore.cpp "Infos[final] +0xEC (0xDB9F64) == 0 -> 0" (reaction availability). Two addresses for one
  label; given the page's "memory = file + 0x10", one of the labels is a file offset and the other a memory offset.
- `HomeDeleted`: "+0x60 == GetAbode -> +0x60 = 0. TODO: Living +0x60 is not identified".
- `SetVillagerDisciple`: "the 3D object (+0x40) vt +0xCC, edx = g_DiscipleInfos[disciple] +0x8 … what that LH3DObject
  call shows (a disciple marker, inferred)".
- `Dead`: "vt +0xC4 (edx 0) (not ported: (inferred) a reset of the 3D object)".
- `ProcessLiving`: "0x5EC8E1 g_game +0x2502CC = 0x14: (pending) its reader".
- `CreateVillagerAtAbode`: "[0xD99384] = it, null too ((not ported) [0xD99380] the same, no reader known)". The role
  of [0xD99380] is unknown.
- `k_NoVillagerInfo`: "0x54, one past the 84 records of [0xDA6BE8]. (pending) what the exe reads there".
- `SameTypeSwitch` (Reactions.h) reads field +0x04 of the table 0xC09CC0 + 0xC0 × t as "may switch to another reaction
  of the same type: 1 only for 10, 28 and 35". This page (reactions 7 / 12) calls 0xC09CC0 "the villager type table:
  only 7 and 12 registered". These may be two different fields of one table.
