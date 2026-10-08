# Buildings, building sites and towns (the building side)

The buildings of a town as the original runs them (Abode.cpp, MultiMapFixed.cpp, StoragePit.cpp, BuildingSite.cpp,
Workshop.cpp, Town.cpp of `runblack.exe` W120) and how openblack does it. The villagers that use these buildings (going
home, builders, carrying) are in [villagers.md](villagers.md); the physics of a building that breaks is in
[physics.md](physics.md).

> **Code rules.** A building's and a town's state lives in ECS components (`components::Town`, `components::TownBelief`,
> the abode components) or Locator services, never in globals; logic such as the desire, the belief fold or the
> builder ring is kept free of global state so it can be unit tested with fakes in `test/`; comments describe
> behaviour in plain English, with no decompiled names or addresses (those belong here). See
> [the conventions](../refactor/README.md).

- [Resources held by objects](#resources-held-by-objects)
  - [The town's belief](#the-towns-belief)
- [The town's temporary pots](#the-towns-temporary-pots)
- [Life and damage](#life-and-damage)
- [Plans and building sites](#plans-and-building-sites)
  - [Plans](#plans) · [Building sites](#building-sites-buildingsite) ·
    [Citadel building site](#citadel-building-site-six-piles) · [Workshop](#workshop-workshopcpp-0x7791900x77a720) ·
    [Scaffolds and the wonder](#scaffolds-choosing-the-building-and-the-wonders-power) ·
    [Citadel plan](#citadel-plan) · [Placement and town area](#placement-and-town-area) ·
    [Script buildings](#script-buildings) · [Graveyard](#graveyard)
- [Pending](#pending)
- [Sources](#sources)

## Resources held by objects

`ecs::object_resources` (`src/ECS/ObjectResources.h`) is the original's Object::GetResource (vt +0x98),
RemoveResource (vt +0xA0), AddResource (vt +0x9C) and IsPoisoned (vt +0x4A4) for every object that holds food or wood:

- **Abode**: +0xBC[type] (`Abode::foodAmount` / `woodAmount`). GetResource 0x404D30; RemoveResource 0x404F10 ->
  DoResourceRemoving 0x404F60 (the town's CallDesireFunction before the removal, then JustRemoveResource 0x404D60 =
  min(amount, held)); AddResource 0x404D90 -> DoResourceAdding 0x404DF0 -> JustAddResource 0x404D40 (no cap).
- **Storage pit**: the total of its piles (PotStructure::GetResource 0x66EF00, StoragePit::RemoveResource 0x7332A0,
  `StoragePitStore`).
- **IsPoisoned**: StoragePit 0x7336B0 (any available pile poisoned), Pot 0x55D4E0, an abode 0 (GameThingWithPos
  0x402400), a villager its own flag.

- **Pots and piles**: a pile of a storage pit answers the pit's total (PotStructure::GetResource 0x66EF00); any other
  pot its own amount when the resource is its own (Pot::JustGetResource 0x66D390). RemoveResource of a pit's pile
  (PotStructure 0x66EE10): the touched pile gives n - min(over, n) (over = CalulateAmountOverMaximum 0x733260: wood
  above 25000, food above 15000), then the pit pile 5 -> 1 the rest. A pot that empties (Pot::JustRemoveResource
  0x66D410) loses its reaction, its poison and its fire; a loose pile, a temporary pot, MagicFood or MagicWood is then
  deleted (PotStructure::JustRemoveResource 0x66D9B0), a pit's pile stays. AddResource of a pile: PotStructure 0x66ED70
  (`pot_resource`).
- **Storage pit** (`StoragePitStore`, StoragePit::AddResource 0x732F60 / RemoveResource 0x7332A0): only FOOD and WOOD;
  each pile's JustAddResource (the pile sound with the amount still asked for, the cap, the poison), the town pulse
  +0x5E8 = 1 / +0x5EC = 0 when the pit held none of it and something went in, then DoResourceAdding; removing goes
  pile 5 -> 1 and then DoResourceRemoving (the town's desire before the change, also for villagers). The full
  original sequence is in [objects-and-resources.md](objects-and-resources.md#store-storagepit).

**The hand's branch.** DoResourceAdding 0x404DF0 and DoResourceRemoving 0x404F60 do more only with a GInterfaceStatus
(`pot_resource::Dropper`); villagers and scripts pass none (0x404E01 `je 0x404EE3`).
- Removing: Town::SetGameTurnResourceLastRemoved 0x7400D0 (Town +0xEC8[player][type] = the turn), the desire after,
  and GAlignment::Update(this, type, -n, before - after) 0x414520 of the **town's owner** (`alignment::UpdateForResource`:
  k = n > 0 ? giveResourceAligmnetChangeMultiplier : take..., 0.5 each; pending += ScaleChange(delta x k)).
- Adding: delta = before - after x Town::GetGameTurnResourceLastRemovedModifier 0x740030 (never taken 1, else
  min((turn - last) / 1000, 1)^3), GAlignment::Update of the dropper's player, and GBelief::AddToBelief 0x437EB0 with
  delta x (not the owner ? multiplierForNonOwnerAddingResource 1.0 : 1) x multiplierForAddingResourceToTown 0.25:
  Town +0x798 GBelief +0xC8 (pending, folded each turn) and +0x28 (recent, decays) += f, +0x48 = the turn
  (`components::TownBelief`). DoCreatureMimicAfterAddingResource (vt +0x68C) is (not ported): no creature.
- GetGameTurnResourceLastRemovedModifier (0x740030) and SetGameTurnResourceLastRemoved (0x7400D0): player ≥ 8
  (unsigned) or type outside FOOD / WOOD → 0 / nothing (0x740037..0x740049, 0x7400D6..0x7400E4); the 1000 is GTownInfo
  +0x100 maxGameturnsForBeliefAfterRemovingFromStoragePit; only Abode::DoResourceRemoving (0x404FD7) calls Set.
- GBelief::AddToBelief indexes with GPlayer::GetPlayerNumber (0x64A790) without checking (0x437EBB); with a thing:
  DrawBelief if draw (0x437F19) and GGuidance::BeliefSFX (0x437F40), which only sounds for a player below the strongest
  belief (+0x8) (0x437F0A..0x437F2A).
- CallDesireFunction is called from Abode::DoResourceRemoving (0x404FA0) and DoResourceAdding (0x404E22) before the
  change; Villager::ArrivesAtFoodReaction (0x764A45) also calls CalculateDesireForFood.

### The town's belief

The town's per-turn belief fold, ProcessOncePerTurn and DrawBelief (openblack: `src/ECS/Town/TownBelief.*`).

- GBelief at Town +0x798: belief +0x8, recent +0x28, lastAddedTurn +0x48, addedThisPeriod +0x88, pending +0xC8,
  boredom +0xE8 (41 reactions), desire thresholds +0x18C (17); 8 slots per player (GPlayer +0xB5), neutral = 7
  (g_game +0x205A5B, set by GGame::SetupPlayers 0x550458 and GGame::Load 0x554988).
- GBelief::Init (0x437DD0): belief, pending, reduction accumulator and lastAddedTurn to 0, cap 10.0 (0x437DE1);
  belief[neutral] = Town +0x5D8 (fn_0073E4B0, no cap) (0x437DF6..0x437E2C); boredom[0..40] = 1.0 (0x437E30..0x437E40);
  thresholds = GTownDesireInfo[d] +0x3C desireAffectsBeliefAfter (fn_00437E50); recent and addedThisPeriod are NOT
  reset; called by the ctor (0x739523) and SetTownEmpty (0x74108E).
- GBelief::SetBelief (0x4387D0): belief[n] = min(cap[n], v) with no lower limit; SetBeliefInPlayerCap (0x438A00) /
  GetCap (0x438A20).
- Town::SetBeliefInPlayer (0x73BA70) (SET_TOWN_BELIEF 0x71542B): for the neutral Town +0x5D8 = f (0x73BA87), then
  SetBelief.
- GBelief::ReduceBelief (0x437FD0): belief[n] −= r with no limit (it can go below 0); with a centre (+0x9A4) acc[n] += r
  and, above 0.005 (double 0x8C5890), DrawBelief(−acc, centre, P) and acc = 0; that drawing is negative and never
  shows anything (0x438817 jle).
- AddToBoredomMultiplier (0x438790): boredom[r] + f, < 0 → 0; GameThingWithPos::GetBoredomMultiplier (0x56FE70): with
  no town 1.0.
- GetMaxBeliefMeNotIncluded (0x4389B0): the largest belief[i], i ≠ n, from 0, strict.
- fn_00438910 (P, thing): if the thing's player is P, returns belief[i] (not the difference) of the slot i ≠ P with
  the largest belief[i] − belief[P] (from −9999, strict); otherwise belief[the thing's player] (0x438970..0x438983).
- fn_00438A40 (P) (CPU player 0x66ABD8): in slots 0..5 that are not P nor allies, the largest recent[i] > 0.008; none
  or belief[P] == 0 → 0; otherwise min((2·belief[i] / belief[P])², 1).
- fn_00438B20 (P) (Shuffle 0x741945): the largest belief of the active non-allied players ≠ P (GetNextActivePlayer
  0x5508D0: slots 0..6 with +0x8E0 != 0) / belief[P]; 0 with belief[P] == 0.
- GetAddedThisPeriod (0x438060) (and 0x437E80 with no caller); GetAddedThisPeriodRatio (0x438070) = added == 0 ? 0 :
  added / (belief × 10), with no direct caller.
- Fold fn_004383D0 (step 19 of Town::Process): 1) for each player d = Town +0x5DC × pending; d == 0 or NaN → skipped
  and pending is NOT cleared; otherwise added += d, with a centre a belief sprite ftol(d × 10000) in the player's
  colour, ResetDesireThresholds, SetBelief(belief + d), pending = 0 (0x4383FA..0x4384B4).
- Fold 2) boredom: x = LostTownScale (0xBF33F0) × ReactionInfo[k] +0x4C + boredom[k]; stored only if x < 1
  (0x4384BA..0x4384EE).
- Fold 3) if the owner is not neutral, for each desire s = GetDesire (x87 sum in the original): if s > threshold,
  ReduceBelief(owner, (s − threshold) × GTownDesireInfo[j] +0x38 desireToBeliefScale) and, if the threshold >
  GBeliefInfo +0x1C minimumThreshold (0.25), threshold −= +0x48 (it can go below once) (0x4384F0..0x4385BE).
- Fold 4) belief[neutral] fixed to +0x5D8 via SetBelief (0x4385CA..0x4385F7); 5) the strongest player: slots 0..7 in
  order, only < skips (a tie goes to the higher slot, owner included) and recent ×= decay (0x4385FC..0x43865F).
- Fold 6) if the strongest is not the owner: SetBelief(new, GBeliefInfo +0x14 claimedTownBeliefMultiplier 1.5 ×
  belief[new]); if the old one is not neutral, each of its towns (this one included) SetBelief(P, +0x18
  lostATownBeliefInPlayerMultiplier 0.9 × belief[P] × LostTownScale); HelpSpritesGeneralBad if the old one is local,
  otherwise GeneralGood if the new one is local; GPlayer::TakeOverTown fn_00649810 (0x438661..0x43875D).
- LostTownScale (0xBF33F0): SET_LOST_TOWN_SCALE (case 104 of the map script, 0x717E85); 1.0 in GLandBalance::Init
  (0x5E28A9).
- GPlayer::TakeOverTown fn_00649810: fn_0073A7D0 (+0xF20 = 0, 0x73A7F7; leaves the old one's list, fn_0064C0E0),
  SetPlayer fn_0073A8F0 (+0x2C) → tail of the new one's list; the new one's GameStats +0xA44 +0x68 += adults + children
  (0x6498F7..0x649910).
- Town ctor: GBelief::Init at 0x739523 while +0x5D8 is still 0 (the neutral belief is 0 until the first fold); then
  +0x5D8 = GTownInfo +0xB8 beliefInNeutralPlayer (0x73966C) and +0x5DC = 1.0 (0x739672); SetPlayer fn_0073A8F0
  (0x739597) puts it at the end of the owner's list (fn_0064C090).
- Town::SetTownEmpty (0x741080), belief part: Init, SetBelief(i, +0x5D8) for the 8 slots, owner not neutral →
  TakeOverTown(neutral).
- GBelief::ProcessOncePerTurn (0x4380B0) (row 27 of GGame::ProcessTurn, 0x54E6DF): only with turn % 10 == 0; walks the
  towns from newest to oldest, sets addedThisPeriod to 0 and sums each player's belief (into their GameStats: maximum
  +0x1070 / minimum +0x1074, fn_0056A3B0) (0x43810C..0x438187).
- If the local player's added belief > 0.001: ToolTips::ForceToolTips(0xEE1 «Conseguidos %3.0f Creyentes», "%3.0f
  believers gained", sum × 1000) (0x4381C9) and a ValueSpinner "%4.0f" at the point under the hand (MyInterface +0x3B8,
  altitude + 8), colours 00 FF FF FF, Init 0x833A00 (+0x18 = 5.0) (0x4381D0..0x438281); then fn_00438340 for the local
  player (0x4382B9).
- fn_00438340: the "losing belief" help sprite (list 10, fn_0071CCC0, with adults + children) when another player has
  more belief; nothing on land 1 (g_game +0x205A08) (0x438345..0x4383C4).
- GBelief::DrawBelief (0x438800): v = ftol(f × 10000) (truncated); v ≤ 0 → nothing; point = GetLHPoint + GetHeight
  (vt +0x42C) (0x438827..0x438864); the player's colour or 0xFFFFFFFF; with g_game +0x14 & 0x4000 a debug ValueSpinner
  "B n" (Belief.cpp 0x15B).
- Belief sprite queue at 0xD4ED00 (24 bytes per entry; grows 10 at a time, fn_0069B6B0): fn_0069B550 does not add if
  [0xC02A08] != 1 or there are already 400 (0x69B550..0x69B56B); UR_BeliefSprite::ModifyAtomCollection (0x69B750)
  takes the LAST one (LIFO) (0x69B781).
- Belief help sprites: LosingBelief fn_0071CCC0 (list 10), GeneralBad fn_0071CD70 (list 13), GeneralGood fn_0071CDF0
  (list 14).

## The town's temporary pots

`ecs::town_stores::GetTemporaryResourceStorePotOrPos(town, from, type)` is Town::GetTemporaryResourceStorePotOrPos
0x73E900: where villagers drop resources while the town has no working storage pit.

- The pot of Town +0x600[type] (FOOD +0x600, WOOD +0x604; `components::Town::temporaryPots`) if it is available
  (vt +0x2C). The slot is considered alive if it is not NULL and IsAvailable (GameThing 0x401810)
  (0x73E90D..0x73E91F).
- Otherwise a new one: GetCongregationPos 0x7408B0 + GetPosFromAngle 0x74D580(0, WOOD ? 5 : 0) (0x73E96E), moved by
  FindClearArea 0x7412F0(45, 1.5, 2) away from MultiMapFixed objects (the filter 0x73EA50 is GameThingWithPos::Flags
  +0x24 bit 1, the MultiMapFixed bit; its result is not tested), then Pot::Create 0x66CF10 with GPotInfo 10 MagicFood /
  9 MagicWood and **amount 0**. For those two infos Pot::Create calls the MagicFood 0x5FA9F0 / MagicWood 0x600E20
  constructors (player NULL; the town is not passed on) and CallVirtualFunctionsForCreation (vt +0x658): openblack's
  `magic::objects::CreateMagicResourcePile(..., allowEmpty = true)`.
- Pot::Create receives GPotInfo 0xD4C660 + (FOOD ? 10 : 9) × 0x144 and the town as `this` (0x73E9D9..0x73EA11).
- Either way the point returned is the pot's GetNearestEdgeToPos(from) (vt +0x83C, Object 0x636DA0).
- There is always a pot: Pot::Create fails only when its allocation does (0x66CF81), and 0x73EA18 uses the result
  unchecked.
- **(approximate)** `town_queries::GetCongregationPos` returns x and z only; the cache's altitude is dropped (the pile
  stands on the land anyway).
- openblack's `PotArchetype::Create` makes no pile of amount 0 unless `allowEmpty`: an openblack rule, not the
  original's (Pot::Create has no such check).
- Town::SetStoragePit (0x73EA60) (from StoragePit::MakeFunctional 0x732F30): +0x30 = pit; each available temporary pot
  with its resource → Pot::SetupReaction (0x66D660); otherwise ToBeDeleted; the slot = 0 (0x73EA79).
- Step 17 of Town::Process (0x747450..0x7474A0): pot not available → slot = 0; available, empty (+0x70 == 0) and with a
  functional GetStoragePit → ToBeDeleted(0) (PileFood::ToBeDeleted 0x66E100 closes the speed-up; Pot::ToBeDeleted
  0x66D110 → RemoveReaction 0x66D6A0) and slot = 0; otherwise it stays.
- `StoragePit::DeleteDependancys` 0x732CD0 → SetStoragePit(the other pit) 0x73EA60: its empty temporary pot goes.
- Town::AssignForestsToTown (0x73EB00) takes as reference the position (+0x14) of the storage pit or, with no pit,
  GetTemporaryResourceStorePotOrPos(+0x14, WOOD) (0x73EB4D), which creates the town's MagicWood pot if it has none.

## Life and damage

The building's side of physical damage (Abode::ApplyEffectsDueToPhysicalDestruction 0x406640, StopBeingFunctional
0x4073C0, DestroyedByEffect 0x403F80; openblack `abodes::OnPhysicalDamage`; the hit itself is in
[physics.md](physics.md)). The original lowers the life through the effect system (EffectValues(3) 0x524FE0, damage =
max(0, life - remaining) / GetDefenseMultiplier 0x637930, Object::ApplyEffect vt +0x5CC -> Abode::ReduceLife
0x405D90), not by setting it, and shows HelpSpritesDestroyBuilding 0x71D070 when less than 0.4 is left (0x406753) and
the player is the local one.

- A storage pit under construction whose percent reaches 0 drops its life from 1 to 0 (0x52F659), crossing the 0.75
  threshold: StopBeingFunctional and `SetInStateOfEmergency` (0x405E07..0x405E3E).
- A house does not cause a town emergency: `CausesTownEmergencyIfDamaged` of Abode (0x4016F0) returns 0.
- `TownCentre::StopBeingFunctional` 0x744A00 sets the town's worship percentage to 0.
- `TownCentre::ToBeDeleted` 0x743B40 runs DeleteDependancys 0x743BE0 twice (0x743B4E, then `Abode::ToBeDeleted`
  0x402C6F): on the second pass the town's +0x9A4 is not this centre, so 0 is written (0x743BFD) and +0x9A4 always ends
  at 0. The totem +0xCC is deleted with it (0x743B54..0x743B65).
- `Creche::DeleteDependancys` 0x50AA50 writes the town's creche +0x744 = the creche found, unconditionally (0x50AAB3);
  found is 0 when +0x744 is another creche.
- `RemoveStructureFromTown` (0x739A8A) sets +0x98 = 0; `MultiMapFixed::ToBeDeleted` calls
  RemoveAllReactionsInitiatedByObject (0x52E2B7).
- `Town::ProcessTownRepairs` skips an abode that already has a site (0x747E39).
- Emergency: started by Abode::ReduceLife (0x405E3E) when a storage pit or centre drops below its functional
  threshold, or by ProcessTownEmergency; it lasts GTownInfo +0x110 (1200 turns).
- Town::SetInStateOfEmergency (0x7479A0): if +0xF1C == 0 it calls CallAllVillagersToTownEmergency; in both cases +0xF1C
  = turn (on turn 0 it stays "no emergency", literal) (0x7479A3..0x7479D5).
- CallAllVillagersToTownEmergency (0x747890): abodes of +0x754 and inhabitants of +0xA0 in list order; row = 0xD091E8 +
  0x90 × (GetFinalState & 0xFF), if its function gives != 0 → SetTopState(0xF2); the next villager (+0xE4) is read after
  SetTopState (0x747898..0x747949); a debug hook via [0xC22E8C] if [0xC22E84] == 0 (0x747951).
- Town::ProcessTownEmergency (0x7477A0) (step 15): in an emergency, if +0xEB0 == turn it is refreshed; a worship
  percentage +0x5C0 != 0 is saved in +0xEC4 and set to 0 (SetWorshipPercentage 0x73C060) (0x7477AE..0x7477F0).
- Outside an emergency: storage pit or centre (+0x9A4, no IsAvailable test) on fire (Object::IsOnFire 0x637CC0) →
  starts it (0x7477F9..0x747826); otherwise, with +0xEC4 != 0 and +0x5C0 == 0 the saved percentage comes back; then
  +0xEC4 = +0xF1C = 0 every turn (0x74782F..0x747873).
- Town::UpdateAggressor (0x73C9B0) (from Object::ApplyEffect 0x637B7D): +0xEAC = the effect's GetCausedPlayer
  (0x525910) or the neutral (0x73C9C7), +0xEB0 = turn (0x73CA82 / 0x73CA98); also the per-player aggression slot
  fn_0073E0F0 (town + n × 0x80 + 0x9F4, GTownInfo +0xAC if 0, × +0xEB8 for the player itself or +0xEB4, then × 0.9),
  TownAttackSFX (0x71B7C0) and a creature part (0x73CAA3..).

## Plans and building sites

`ecs::plans` and `ecs::building_sites` (`src/ECS/Town/BuildingSites.h`), the building's own state in `ecs::abodes`
(`src/ECS/Abodes.h`).

- **Plans** (PlannedMultiMapFixed, a GameThingWithPos: not drawn, Draw 0x648930 = `ret`; not in the map cells) live in
  the town's list, oldest first (Town::AddPlanned 0x73D080). The town never invents them: the script
  (CREATE_PLANNED_ABODE), destroyed buildings (repairs), scaffolds and the rival AI make them.
- **Choosing**: GetDesireToBeBuilt 0x73A1A0 per type (houses by free adult places, civic buildings once, wonder by the
  For_Wonder desire...), GetBestPlanned 0x73A140 (strictly better, the first on ties), RequestBestPlanned 0x73A650
  (mask 4, no fixed check; fields pass too), RequestANewAbode 0x73B330 (mask 2, with the fixed check
  IsSuitableForFixedAbodeInTown 0x603860: inside the nearest other town's area widened by 4 cells it is accepted with
  no other test).
- **Converting** a plan (CreatePlannedNoFixedCheck 0x405770): the abode is created under construction (+0x58 bit 1,
  percent 0, life 1.0 from the Object ctor), then its building site (BuildingSite 0x43B7E0: the 128 builder positions of
  PosBuilder 0x43AE10 around the mesh, the repair base 1.1 x life - 0.1 for a damaged built building). CHL
  BUILD_BUILDING 130 (GScript::BuildBuilding 0x6FAB30 -> ForceBuildingOfPlannedAtPos 0x73E560) converts the plan at the
  point within the same call, with the desire boost x 5.
- **Building**: BuildBy 0x52ED40 adds to the percent built; at 1 Built 0x52EBB0 / Abode::Built 0x404720 deletes the
  site (builders back to state 163, the wood pile released) and MakeFunctional 0x4047E0 runs (the town's statistics count
  the abode from then on: Add(Abode) 0x7498C0). On a built but damaged building BuildBy repairs (IncreaseLife 0x405ED0,
  Repaired 0x4047B0).
- **Drawing**: DrawBuilding 0x517F90 draws the partial model with GetPercentForDrawBuilding 0x52EFD0
  (`components::DrawMesh`, the mesh of `physics::PartialBuild::BuildMesh`), nothing at exactly 0 %
  (`components::NotDrawn`) but the ground footprint stays (SetFootPrintOnTexture 0x52EA33), no static shadow until
  built (SetShadowOnTexture 0x1000, `abodes::CastsShadowOnTexture`), and no land haze (only fn_00801C90). The inner
  walls of the partly built draw are pushed along the L3D vertex normal (+0x14) (fn_0085C0E0 0x85C0EB..0x85C111); the
  rest of the partial draw is in [physics.md](physics.md#sounds-dust-and-the-look-of-impacts).
- **Graveyard** (`src/ECS/Town/Graveyard.h`): Town +0x748; AddDead fn_00595E50 counts the dead below 50 and sets the
  graves stage max(1, ftol(n x 0.18)) & 7 on the 3D object (details under [Graveyard](#graveyard)).

**(approximate)** the town areas recomputed at the query; a deleted site is freed one turn later.

### Plans

- PlannedMultiMapFixed ctor: +0x3C = creation turn (g_game +0x205A40) (0x6487F6); Town::AddPlanned puts them at the
  end, +0x9AC++ (0x73D080).
- Town::RemovePlanned (0x73D0D0) (PlannedAbode::ToBeDeleted 0x4056B0): unlinks and +0x9AC--.
- GetAbodeType: PlannedAbode (0x4061E0) = info +0x120 (PlannedTownCentre the same); PlannedTownCitadelHeart (0x467E30)
  = 0x804 (bit 2 makes it eligible with mask 4).
- IsCivic (vt +0x50C): PlannedAbode (0x4060C0) = the type is in the civic list (table 0x406118 / 0x406120);
  PlannedTownCentre (0x55DBE0) = 1; PlannedTownCitadelHeart (0x467E10) = 0 (its type 0x804 has the civic bit but
  TownStats +0x24 does not count it).
- GetDesireToBeRepaired of a plan (vt +0x514) (0x648910) = +0x30 (was built) ? info +0x118 : 0.
- PlannedAbode::CreatePlanned(life) (0x405710) (vt +0x500): GAbodeInfo::IsOkToCreateAtPos(info, pos, angle +0x28,
  GetScale, town +0x48) (0x404B10) or null; otherwise CreatePlannedNoFixedCheck.
- CreatePlannedNoFixedCheck (0x405770): Abode::Create(pos, info, town, angle, scale, food 0, wood 0, life, 1, 1)
  (0x402E20, 0x4057AC) → MultiMapFixed ctor (0x52E1E0) with underConstruction 1 → Abode::Init (food and wood 0, no
  MakeFunctional) → CreateAbodeSurroundingObjects (0x403E00); the life argument is ignored.
- PlannedTownCentre::CreatePlannedNoFixedCheck (0x744550) calls TownCentre::Create (0x743C90) directly and does not do
  the +0x30 step.
- MultiMapFixed ctor (0x52E1E0): underConstruction 1 → +0x58 bit 1 and +0x5C = 0 (bit 3 clear); 0 → bit 3 and +0x5C =
  life (0x52E20B..0x52E234).
- After creation: PostCreatePlanned (0x648C50) moves the footpath link (+0x64 ← plan +0x38) and, with the plan's town,
  CheckWhenNewBuildingCreated (0x741500); PlannedAbode only: +0x30 → building +0x58 |= 4 (0x4057C5..0x4057CC); then the
  plan is deleted.
- PlannedAbode::Create(Abode*) (0x405660) (from Abode::MoveAbodeToPlannedAbodes 0x40453E): a plan where the building is
  (PlannedMultiMapFixed(MultiMapFixed*) 0x648820): +0x14 pos, +0x28 GetYAngle, +0x2C GetScale, +0x40 info, +0x30 = bit
  3 of +0x58 (built) (0x6488B4..0x6488CB); always PlannedAbode (also from a centre, 0x405681); the footpath link moves
  to the plan (+0x38 = b +0x64, b +0x64 = 0); Init(GetTown) (0x4055A0) → AddPlanned; it only fails when allocating
  memory (`new 0x4C`, Abode.cpp 0x560).
- GetDesireToBeBuilt (0x73A1A0): b = info +0x114; type = the info's GetAbodeType (vt +0x40); s = building sites of
  +0x790 whose building is of that type; b == 0 → 0 (0x73A1AC..0x73A1FD).
- GetDesireToBeBuilt constants: 0.8 (0x8C4A04), −10 (0x8C7670), 0.2 (0x8AB244), 0.6 (0x8C7BDC), 10 (0x8AB414), 0.3
  (0x8AB23C); a wonder with 7 or more scaffolds is worth 1.0 (0x73A446).
- Cases by type: LIVING_QUARTERS 0x73A2EF (u = (adults + children) / 10 + 1 unsigned; the comparison of free places > u
  is signed, and only with no scaffolds); TOTEM 0x73A279; STORAGE_PIT 0x73A2BE (GetStoragePit); CRECHE 0x73A2E4
  (+0x744); WORKSHOP 0x73A23C (byte +0x721 = stats +0x111 = abodes of number 9); WONDER 0x73A446 (GetDesire(14) against
  GTownDesireInfo(14) +0x18); FOOTBALL 0x73A502 returns 0 at once with football off.
- Correction by abode number: m = byte stats +0x108[number]; b −= b / max(m + 1, 10) × m (0x73A3F4..0x73A43D).
- The "undivided" exits only skip the common division r / s (0x73A557); with scaffolds c = (float)(u32)(n − info
  +0x10C ScaffoldsRequired) × 0.3 × r and r −= min(c, r): with n below ScaffoldsRequired the subtraction wraps around
  and r ends up 0 (literal) (0x73A571).
- GGame::FootballEnabled (0xD0196C) is 0 in .data (single-player game).
- GetPlannedAtPos: best = r; with no plans (+0x9AC == 0) → 0 (0x73E4C6); the radius is fn_636E30(info, scale) = scale
  × max(mesh +0x24, mesh +0x2C) (MeshPack 0xE9FE34; the citadel heart uses info +0x124, GetMesh 0x464370).

### Building sites (BuildingSite)

- MultiMapFixed::CreateBuildingSite (0x52F590) (`new 0x648`, Fixed.cpp 0x50C) → BuildingSite(MultiMapFixed*)
  (0x43B7E0): +0x638 = bit 2 of the building's +0x58 (repair site), root, building +0x74 = the site (fn_52E3F0), +0x640
  = life or 1.1 × life − 0.1 (0x8AB230, 0x8AB22C) if it has no DestructionMesh (+0x90) and is built; ring with
  fn_43CDB0 → PosBuilder::Process.
- Site types: a workshop creates WorkshopBuildingSite (0x77A6D0, vtable 0x8C6F24); CitadelHeart::CreateBuildingSite
  (0x468DC0) (`new 0x65C`) creates CitadelBuildingSite (ctor 0x43D1E0, then CreatePilesOfWood 0x43D2A0); the rest,
  worship site included, StandardBuildingSite.
- PosBuilder::Process (0x43AE10): 1) the 128 entries = the translation and the static array of best distances
  0xC58CD4[128] = 0; 3) with no mesh, x = sin a × R + c.x, z = cos a × R + c.z (a accumulated), y = GetAltitude
  (0x43AEE8..0x43AF74).
- PosBuilder with a mesh: each triangle of each primitive of each submesh in file order; the two vertices with the
  lowest LOCAL y (strict < 2.5, 0x8C581C); to world; u = normalize(S − L) (0x43AF77..0x43B3EA).
- PosBuilder: ray c → P2 = (c.x + R cos a, c.y, c.z + R sin a) with a = i × 2π/128 (0x8C6B64 = 0x3D490FDB) against the
  edge L → S in XZ (fn_43B630, fn_43B4F0; inclusive XZ box fn_43B5A0); the hit's y is interpolated along the edge
  (0x43B2A2..0x43B316).
- PosBuilder 5): each entry is pushed 1 m outwards along its 3D direction from c (with no hit, from the translation)
  (0x43B3F0..0x43B4D2); ComputeRing fn_43CDB0 passes IsFootball (vt +0x464).
- GetNearestEdge (0x43CE40): angle < −6π (0x8C6CB4) → index 0; > 6π → π; normalises to [0, 2π] (2π stays); 0 → 0;
  otherwise ftol(angle × 1/2π (0x8C6CAC) × 128) & 0x7F.
- GetRandomBuildPos (0x43CDE0) (vt +0x128): a = Get3DAngleFromXZ(building, villager) + GameFloatRand(π/2) − π/4
  (BuildingSite.cpp 0x37C; ±45°).
- GetNextPosFromIndex (0x43CF40) (vt +0x124): with no building (0, 0, 0); step = 2.0 / (Get2DRadius × 2π × 0.0078125);
  k = ftol(GameFloatRand(step × 0.5) + step) (line 0x3B7), then GameRand(2).
- GetBuildPos: ArrivesAtBuildingSite reads the entry directly as (ftol(x × 6553.6), ftol(z × 6553.6), 0) only for 0 ≤
  index < 128 (0x758B12..0x758B6A).
- GetClearAreaRadius (0x43BDE0) = Get2DRadius × 1.2 (0x8C6C98); GetRadius (0x43D050) (vt +0x60).
- ShouldIGetWood (0x43C680): "near the building" = 50.0 (0x8C6CA4); GetDistanceModifier maximum 5000.0; m =
  GetResource(WOOD); GetResourceDropoffPos(WOOD) (0x753E20) is only requested at 0x43C78E.
- Wood pile position (GetResourcePosAndYAngle 0x43C220, vt +0x114): type not WOOD → angle 0 and the root's position;
  worship site → the local point (9, 0, −50) through the 3D object's matrix and its Y angle (0x43C24D..0x43C2CF).
- Pile with no worship site: a = Get3DAngleFromXZ(door, centre) + π/8 (0x8C6CA0) − GameFloatRand(π/4) (line 0x2B8);
  d = distance(centre, door) + 4; centre + GetPosFromAngle(a, d); Y angle = a − π (0x8C36A0): the pile ends up on the
  other side of the door (0x43C361..0x43C426).
- CreatePileWood (Standard) (0x43D760): only an available Standard site with no pile; Pot::Create(pos, GPotInfo
  0xD4D1C4 (9 "Magic Wood"), 0, building, town 0, 0, angle, 1.0, 1); the MagicWood ctor (0x600E20) ignores angle and
  scale (0x600E30..0x600E48), although the angle draw is consumed.
- GetResource (Standard) (0x43C5B0): the pile's JustGetResource or 0; GetWoodForStats (0x43C5E0) = GetResource(WOOD);
  WorkshopBuildingSite::GetWoodForStats (0x43DB90) = 0.
- AddResource (Standard) (0x43C490): WOOD only; with no pile it creates one; if available JustAddResource; returns what
  was added. RemoveResource (Standard) (0x43C530): WOOD only, JustRemoveResource(WOOD, n, 0) of GetPileWood(status ?
  status->vt +0x100 : 0).
- Process (Standard) (0x43D8D0): pile +0x644 not available → +0x644 = 0; CitadelBuildingSite::Process (0x43D660) the
  same for its six slots (inferred: not reached, CitadelHeart::Process 0x4665A0 does not call MultiMapFixed::Process).
- BuildingSite::ToBeDeleted (0x43B960): 2) out of g_game +0x205CAC; 3) out of each town of +0x205C84 that lists it
  (fn_73B990); the available builders (Villager::IsAvailable 0x751D50) to 163 moving out to Get2DRadius + 2
  (0x8AB478); 5) list emptied, +0x634 is NOT reset; 6) each scaffold of +0x20 → ToBeDeleted(0) (on a copy); 9)
  GameThing::ToBeDeleted (0x56FB70, 0x43BB97).
- CitadelBuildingSite::ToBeDeleted (0x43D220): for each slot SetMultiMapFixed(0) and, with wood and without g_game +0x14
  & 0x8000, Pot::SetupReaction (0x66D660), otherwise ToBeDeleted; SetPileWood(0) is a no-op (0x43D180) (the slots keep
  the piles).
- AddBuilder (0x43BE40): the duplicate search is useless (dead loop): node at the head, +0x1C++, +0x634++.
  RemoveBuilder (0x43BE90): each of the villager's nodes removed (+0x1C-- each) and +0x634-- once even if he was not
  there.
- AddScaffold (0x43BEF0): node at the head of +0x20, +0x24++ (the scaffold's +0x68 = site is written by the caller,
  0x43BF18); RemoveScaffold (0x43BF20): each node removed, without clearing the scaffold's +0x68.
- GetBuilding (0x43BC70): +0x14 available ? GetBuildingObject (vt +0x8BC, MultiMapFixed 0x401520 = this) : 0;
  GetRootBuilding (0x43BCA0) = +0x14; GetTown (0x43C0B0): the root's GetTown (0 for the citadel heart and a worship
  site).
- GetDesireForVillagers (0x43BD70): clamp(GetBuildersNeeded / max (fn_43BBD0 = info +0x110 MaxVillagerNeededToBuild)
  + boost +0x63C, 0, 1); a maximum of 0 divides by 0 (literal). GetBuildersNeeded (0x43BC00) is signed.
- GetDesireToBeRepaired (site) (0x43BE00): with a building, GetDesireForVillagers × the ROOT's GetDesireToBeRepaired
  (vt +0x8D8); otherwise 0.
- GetPercentBuilt (site) (0x43BCB0) = GetPercentRepaired × the building's GetPercentBuilt.
- GetWoodValue (0x43C0C0) = (float)(u32) info +0x6C × GetScale / TribalPower[5] of the site's GetPlayer, which is
  GameThing::GetPlayer (0x570130) = the neutral player for every site.
- GetWoodNeededToBuild (0x43C5F0) = (root built ? 1 − life : 1 − percentage) × GetWoodValue − the pile's wood (caller
  IsInterestedInWoodObject 0x76501C).
- SetDesireBoost: +0x63C (fn_464F50 0x464FCE, ForceBuildingOfPlannedAtPos 0x73E560); SetRepairBase: +0x640
  (Abode::ReduceLife 0x405EA7).
- Town::AddBuildingSite(BuildingSite*) (0x73B910): already in the list → nothing (no pulse); otherwise node at the
  head, +0x794++ and pulse (0x73B96D..0x73B977). Town::IsBuildingHappening (0x73E2F0) = +0x794 != 0.
- Town::AddBuildingSite(MultiMapFixed*) (0x73B8E0): CreateBuildingSite (vt +0x4D4) and insertion; used by
  Abode::ReduceLife 0x405E7A, ProcessTownRepairs 0x747E75 and SetupBuildingObject 0x758565.
- Town::AddBuildingSite(PlannedMultiMapFixed*) (0x73B860): CreatePlanned(0.0) with the fixed check, then the site.
  0x73B8A0 (from Scaffold::TryToBuildPlannedBuilding 0x6E914C, plan from PlannedAbode::CreateNoInit 0x4055C0 in no
  list): CreatePlannedNoFixedCheck and the site.
- Town::RemoveBuildingSite (0x73BA20): the first site whose GetBuilding is b → ToBeDeleted(0). GetBuildingSiteInList
  (0x73CE40). IsBuildingSiteValid (0x73CF00): in the list, building != 0 and !(IsBuilt && IsRepaired).
- Town::GetBestBuildingSite (0x73CF60): from the head, score = distance(pos, the building's GetNearestEdgeToPos) ×
  (GetDesireForVillagers × 0.9 + 0.1) below 99999 (strict); the LOWEST wins (literal: it favours sites that need FEWER
  builders); a GetBuilding of 0 midway aborts with 0 (0x73CFB6).
- Town::GetBestRepairBuildingSite (0x747EA0): among the sites with +0x638, the largest GetDesireToBeRepaired > 0
  (strict; the first on a tie).
- Town::ProcessTownRepairs (0x747DE0): best = 0; plans with +0x30 by GetDesireToBeRepaired (strict, NaN never wins),
  then abodes of +0x754 with no site (+0x74) nor bit 2 of +0x58 against the SAME best; abode → +0x58 |= 4 (0x747E6E)
  and AddBuildingSite (repair site +0x638 = 1); otherwise plan → AddBuildingSite(plan) with the fixed check; at most one
  site per turn (0x747DE6..0x747E87).
- Pruning fn_43BD00 (step 2): each node (reading the next one first) with a null root, not available, or built and
  repaired → ToBeDeleted if the site is available.
- fn_73B620 (0x73B620) (from Villager::Building 0x758D71): TownStats +0xEC += wood used; also the player's GameStats
  +0xA44 +0xA8.
- BuildBy (site) fn_43D080: GetBuilding()->BuildBy (vt +0x900).

### Citadel building site (six piles)

- CitadelBuildingSite::GetResourcePosAndYAngle (0x43D470): GetWorshipSiteAngle(index) − 1.1424 (0x8C6DEC) and
  GetPosFromAngle(that, 22.0) from the root; it does not write the output angle.
- Citadel::GetWorshipSiteAngle (0x463610): the heart's Y angle (+0x30, vt +0x508) + slot × 2π/7 (0x8C836C = 0.897598).
- GetResource (0x43D320): sum of the six JustGetResource with no IsAvailable test. AddResource (0x43D360): pos NULL →
  nothing; otherwise the nearest slot (with none, CreatePilesOfWood and again), JustAddResource of any type, and
  GetTown (0) +0x710 += what was added.
- RemoveResource (0x43D3F0): with an interface, BuildingSite::RemoveResource (0x43C530) on the pile nearest to
  IS->GetPos; without it the slots in order while any remains (0x43D410..0x43D422).
- GetPileWood (0x43D500): pos NULL → 0; otherwise the nearest slot with (float)(u32) distance strictly less than FLT_MAX
  (0x8C6B50); IsLinkedToThisBuildingSite (0x43D580) = one of +0x644[0..5]; Standard (0x43D830) = +0x644; BuildingSite
  (0x43D0A0) = 0.
- CreatePilesOfWood (0x43D2A0): an empty "Magic Wood" in each empty or unavailable slot (Pot::Create 0x43D303).
- Citadel heart: SetLife(StartLife) at 0x469452 and heart +0x94 = the plan's town; InsertMapObject (0x46776C) puts it in
  the map cells.
- `CitadelHeart::ToBeDeleted` 0x464C50: the CitadelEntrance +0xDC ToBeDeleted(now) (0x464CCF), then the building site
  (MultiMapFixed::ToBeDeleted, through `CitadelPart::ToBeDeleted` 0x464D62).

### Workshop (Workshop.cpp 0x779190..0x77A720)

- Converts 2500 wood (GScaffoldInfo +0x6C WoodValue, global 0xD959D0, GetInfo 0x779A50) into a scaffold of value 1
  every 200 town turns (info +0x1AC TimeEachMobileObjectTakesToProduce); at most 3 waiting (literal `mov edx, 3` at
  0x779B05; info +0x1A8 is not read).
- Special points of its mesh: 4 the wood pile, 6 / 7 / 8 the slots, 9 the ShowNeeds icon (GetShowNeedsPos 0x77A638), 10
  the centre of the drop-off area.
- Workshop::Create (0x779590) (`new 0xE8`, line 0x82): ctor (0x779350) all to 0 (fn_779450);
  CallVirtualFunctionsForCreation (0x7793E0): +0xCC = Game3DObject::Create(0) (0x63ABB0) and a ShowNeedsVisuals
  (0x719D60) (GShowNeedsInfo 0xD99A74, an Object with a creation index); then Abode::CallVirtualFunctionsForCreation
  (0x403200); CreatePileWood (0x7795EA) after InsertMapObject (0x7795E2), so the pile enters the cells after the
  workshop.
- Workshop::CreatePileWood (0x779630): with +0xD0 null, Pot::Create at point 4 with 0 wood and last argument 0, so
  PotStructure +0x78 = workshop and +0x80 = 0 (no SetupReaction on creation; the Standard site passes 1).
- fn_779690 (0x779690): WOOD with a 3D object and point 4 → that point (altitude above the ground kept) and its angle;
  otherwise the workshop's position and angle 0.
- Workshop::MakeFunctional (0x7797B0): after Abode::MakeFunctional, with a town the pulse and Town::AddWorkshop
  (0x740180) (at the head if not there: +0xD8, +0x74C, +0x750++). Town::RemoveWorkshop (0x7401D0) (+0x750--, +0xD8 = 0)
  from DeleteDependancys 0x779509 and RemoveStructureFromTown 0x739B12.
- Workshop::DeleteDependancys (0x7794F0): RemoveWorkshop; each of its own scaffolds loses its owner (+0x58 = 0 if
  available) and all its nodes, without touching +0xC8 or the slots (literal).
- Workshop::ToBeDeleted (0x779480): DeleteDependancys; the pile SetMultiMapFixed(0) (Pot 0x55D570) and ToBeDeleted: its
  wood is lost (no pick-up reaction); the ShowNeedsVisuals ToBeDeleted and +0xD4 = 0.
- Workshop::Process (0x7797F0): functional → --(+0xC4) and at 0 FinishScaffold (fn_779910); otherwise, with room and
  wood ≥ WoodValue → StartScaffold (fn_7798A0); then only the head scaffold (Scaffold +0x98 is only written by the
  ctor, to 0) is released if it is no longer adjustable (fn_6EAA40 → RemoveFromWorkshop 0x6E9200); ShowNeeds +0x5C =
  GetVisualWoodDesire with no null test (fn_719E80); at the end Abode::Process (the workshop's site has an empty
  Process 0x43DBA0).
- StartScaffold fn_7798A0: RemoveResource(WOOD, WoodValue) (from the pile and the mirror), +0xC4 = ftol(200.0),
  SoundTag::Create(this, 0x4A, 0, 2, −1, 0, 1, 1, 0) (0x71E840) (work loop of the InGame bank).
- FinishScaffold fn_779910: first free slot (fn_779610); Scaffold::Create(pos of the point, GScaffoldInfo, GetTown,
  this, the point's Y angle, 1.0) (fn_6E8460); SetScaffoldValue(1) (0x6E9D40); AddScaffold; the scaffold's slot bits;
  slot byte = 2; G_ScaffoldReady 150 (0x779A03) at the workshop; always +0xC4 = 0 and SoundTag::Remove(this, 0x4A, 1)
  (0x71EBE0).
- GetSpaceInStore (0x779AF0) = 3 − (+0xC4 != 0) − +0xC8 (signed).
- GetVisualWoodDesire (0x779B90) = 1 − min((wood + 0.0001) / (WoodValue + 0.0001), 1) (0x8BF518 = 0.0001);
  GetDesireToBeSupplied (0x779B60) = room != 0 or GetVisualWoodDesire > 0 ? 1 : 0.
- fn_779B20 (tooltip over the workshop, GInterface::CalculateOverObject 0x5CA1E8): room > 0 ? WoodValue × room − wood
  (uint32 that wraps around, literal) : 0.
- Workshop::AddResource (0x779E00): n == 0 → 0; pulse if it had none of that type; WOOD: with no pile it creates one,
  available pile → JustAddResource; then DoResourceAdding with status 0 (only JustAddResource 0x404D40: +0xBC[type] +=
  added; no desire, alignment, belief or mimicry).
- Workshop::RemoveResource (0x779EC0): WOOD with a pile → JustRemoveResource and DoResourceRemoving(WOOD, r, status);
  otherwise DoResourceRemoving(type, n); there is no redirection to a site (Abode's 0x404F1A is not called).
- PotStructure::RemoveResource (0x66EE10) on the workshop's pile: over = CalulateAmountOverMaximum (MultiMapFixed
  0x4015D0 = 0); r = JustRemoveResource (the pile stays even if emptied, 0x66D9D3); r != 0 → the workshop's
  DoResourceRemoving; r < n → r += the workshop's RemoveResource(n − r).
- Workshop::IsResourceStore (0x77A650) = MultiMapFixed 0x52F1F0 (WOOD with a site) || WOOD || ANY (−2).
  Workshop::RemovePotFromStructure (0x779750): DoResourceRemoving(the pile's type and amount) and +0xD0 = 0, then
  MultiMapFixed::RemovePotFromStructure (0x52F160).
- WorkshopBuildingSite: AddResource (0x43DB20) and RemoveResource (0x43DB60) go to the root (Workshop 0x779E00 /
  0x779EC0); GetPileWood (0x43D9B0) = root workshop ? +0xD0 : 0; GetResourcePosAndYAngle (0x43DA80); CreatePileWood
  (0x43D9F0) creates a pile whose +0x78 is the workshop and then SetPileWood (no-op 0x43D9E0) leaves +0xD0 null.
- Town::GetBestWorkshop(pos, useDesire, onlyFunctional) (0x740250): best = 0; v = GetDistanceModifier(distance, 500) ×
  (useDesire ? GetDesireToBeSupplied : 1), strictly greater and (not onlyFunctional or functional); callers
  CheckSatisfySupplyWorkshop 0x7593BE, ArrivesAtStoragePitForWorkshopMaterials 0x7594A0 (state 211) and
  ArrivesAtWorkshopForDropOff 0x759542 (state 210), all (pos, 1, 1).
- IsPosWithinScaffoldAreas (0x77A3A0): distance ≤ 50 (0x8C6CA4) and (IsCloseToEP(10, p, 10) | IsCloseToEP(6 / 7 / 8, p,
  5)), all four evaluated. IsCloseToEP (0x77A420): distance to the point < r (with no point, (0, 0, 0)).
- Town::IsScaffoldAwayFromWorkshops (0x740300) (ProcessInHand 0x6E874C, ChoosePlan 0x6E8D34, TryToBuildPlannedBuilding
  0x6E909B); Town::CheckScaffoldSnapToPoint (0x740340) (Scaffold::EndPhysics 0x6E87D1): CheckSnapToPoint of all
  workshops, results ORed.
- CheckSnapToPoint (0x77A480): within 2 m (0x40000000) of point 6, 7 or 8 (all three evaluated); from another owner
  with no room → nothing; otherwise RemoveFromWorkshop, AddScaffold, owner = this, slot = first free & 3; then snapping
  fn_77A530.
- Snapping fn_77A530: the slot's point (0 → 6, 1 → 7, 2 and 3 → 8); RemoveMapObject, altitude 0, SetPos, SetXYZAngles
  with the point's GetYXZ angles (the Transform's scale is kept), InsertMapObject, slot byte = 2; with no point (0, 0)
  and angles 0.
- AddScaffold fn_779C10: node at the head, +0xE4++, +0xC8++ always (no duplicate test). RemoveScaffold fn_779C60 (only
  from Scaffold::RemoveFromWorkshop 0x6E923E): if it is there, nodes removed, +0xC8--, slot = 0; with +0xC8 == 2 and a
  town, the pulse.
- fn_779610: first slot with byte 0, otherwise 0 (literal: 0 even if occupied); fn_779A60: point 6 + slot (any other
  value is passed as-is), x / z and altitude 0; fn_779D30: slot < 3 → 0; ScaffoldMoved (0x77A610)
  (InterfaceSetInMagicHand 0x6E975A, InitialisePhysics 0x6EA87D): slot = 1 (reserved);
  Scaffold::CanBecomeAPhysicsObject (0x6EA910) reads +0xDC + slot == 2.
- The byte +0xDF (padding after the three slots) receives the write of a slot value of 3 (0x779CEB / 0x77A5FD /
  0x77A622).
- **The needs sign** (ShowNeeds.cpp: ShowNeedsVisuals 0x719D60..0x71A230, GShowNeedsInfo 0xD99738, 4 rows of 0x114
  bytes; the workshop's row is 3, 0xD99A74, pushed by Workshop::CallVirtualFunctionsForCreation 0x779420): the
  ShowNeedsVisuals is an Object (the next creation index) that copies its owner's MapCoords as they are (Object::Object
  0x636547..0x63658D); CallVirtualFunctionsForCreation 0x719E00 gives it the info's mesh, +0x54 = 0 and SetScale(1);
  SetDesire fn_00719E80 writes +0x5C; ShowNeedsVisuals::ToBeDeleted 0x719DD0 runs from its owner's ToBeDeleted.
- The sign is drawn by Workshop::Draw (0x51CC41..0x51CC67) only while the workshop is functional and not on fire
  (FireEffect::IsOnFire 0x730360) → fn_00719E90: with the desire not above ShowNeedGreater (+0x10C) the height Zoomer is
  set to the desire and nothing is drawn (0x719EB0..0x719EC1, 0x71A16C..0x71A19F); otherwise, every frame,
  SetDestinationWithSpeedAndTime(min(desire, MaxNeedValue) / MaxNeedValue, 0, 1.0) (fn_0071A200, 0x719ED0..0x719EEB),
  the Zoomer advanced inline by the frame ms × 0.001 (0x719EF0..0x719F77, its own sum order, not Zoomer::Update
  0x442720's), height = value × MaxHieght (+0x110) (0x719F7E..0x719F90).
- Where the sign stands: the owner's GetShowNeedsPos (vt +0xE4; Workshop 0x77A630 = GetSpecialPos(9) 0x63B060) with
  the height as its altitude (GetAltitude 0x803090 is computed and dropped, 0x719FBF), turned to face the camera
  (atan2(dz, dx) + π/2, 0x8C78D8, 0x719FC1..0x71A02A), LH3DObject::SetPosition(point, yaw, 1.0) 0x423140 (0x71A051).
  Below L = (MaxNeedValue − ShowNeedGreater) × 0.2 the 3×3 is scaled for the draw only by (value − ShowNeedGreater) /
  (L − ShowNeedGreater) (0x71A056..0x71A0E4, restored at 0x71A116).

### Scaffolds: choosing the building and the wonder's power

- Town::GetNewPlannedBuilding (0x73D980): best = 0; abode numbers 0..15 (or only `limit`); the scale is reloaded each
  iteration; tribe = GetTribe +0x10, for the WONDER (10) the scaffold's if not −1 and scale WonderScale(pos)
  (fn_73DC40); info GAbodeInfo::Find; skipped if ScaffoldsRequired is 0 or greater than n (unsigned) or, without force,
  if the position does not suit its mesh (IsSuitableForFixed 0x603DC0, 0x73DA34); the strictly greater
  GetDesireToBeBuilt wins.
- fn_73D8D0 uses GameFloatRand(2π) (Town.cpp 0xB31) and π/4 (0x8C6C9C).
- Town::GetWonderPower (0x73DAF0): raw = GetRawDesire(14); r = 15 × raw if > 15, otherwise 15 (also NaN); sums
  GetTownArtifactValue (vt +0xC8) + GetImpressiveValue (vt +0x66C) × 0.1 of the objects (fixed and then mobile) at
  distance < r; result ≥ 0.25 (0x8AB3D4); constants 15.0 (0x8C2C40) and 0.1 (0x8AC404).
- Literal in GetWonderPower: GetFirstIterator (0x6034D0) is always called on `pos` (0x73DB67) and not on the copy the
  spiral moves, so the same cell is walked n times (0x73DBF7..0x73DC04).
- Wonder::Create (0x778E80) (`new 0xC8`): ctor 0x778E00 (SetToZero 0x779060), SetPower(fifth argument, inferred: the
  scale) (0x778ECB..0x778ED0), CallVirtualFunctionsForCreation (0x779160), built → AddToPlayer (0x778EE1..0x778EF3): a
  script wonder counts at once, a plan one from Built.
- Wonder::SetPower (0x779070) = +0xC4 (TryToBuildPlannedBuilding passes it GetWonderPower). AddToPlayer (0x778FC0):
  with a player, tribe (GetTribe 0x779040 = &GTribeInfo[info +0x158]) and built, GPlayer +0xB8[tribe] += power;
  RemoveFromPlayer (0x778F50) subtracts it.
- Wonder::Built (0x778F30) = Abode::Built and then AddToPlayer; DeleteDependancys (0x778E60) (ToBeDeleted 0x778E40) =
  RemoveFromPlayer and Abode::DeleteDependancys; MakeFunctional 0x778F20 = `jmp Abode::MakeFunctional`.
- `SET_SCAFFOLD_PROPERTIES` (`GScript::SetScaffoldProperties` 0x6FEAB0): POP destroy (ebx), size (a float), type
  (stored raw), the object; none → "Object no longer valid!" (0xC0DA90); not IsScaffold (vt +0x49C) → "Thing must be
  scaffold" (0xC0DA78) and nothing (the next test 0x6FEB4A fails). Then (0x6FEB54..0x6FEB82): +0x90 = type,
  `SetScaffoldValue(ftol(size))`, flags bit 0x800 = destroy & 1.
- `SET_ACTIVE` on a scaffold (vt +0x49C, 0x6FD7A1..0x6FD7B7): active → `ForceBuildBuilding(0)` 0x6E8860; inactive:
  nothing.

### Citadel plan

- CREATE_PLANNED_CITADEL (handler 0x715E91) → PlannedTownCitadelHeart ctor (0x467DD0): PlannedMultiMapFixed ctor
  (0x648780) (+0x14 pos, +0x28 angle, +0x2C scale, +0x30 = 0, +0x38 = 0, +0x3C turn, +0x40 info, +0x44 = 0), vtable
  0x8C9D4C, +0x48 = town and AddPlanned; it is not drawn, is not in cells, does not flatten land and does not consume a
  creation index; the info is 0xC5E270 + N3 × 0x158.
- CreatePlanned (0x467EA0): fn_4695E0 = MapCoords::IsSuitableForFixed(info +0x124, angle, scale) (0x6038B0) != 0, then
  vt +0x504 (0x467EDA).
- CreatePlannedNoFixedCheck (0x467EF0): the town's player +0x48 (no NULL test; with no player 0) (0x467EFB); citadel =
  player +0xA48 or a new one (`new 0x80`, Citadel ctor 0x462B10 with GCitadelInfo 0xC5E1E8) (0x467F08);
  CitadelHeart::Create(..., life, 1) (0x464E20, 0x467F72); heart +0x94 = town (0x467F83); +0x30 → heart +0x58 |= 4
  (0x467F90); PostCreatePlanned without CheckWhenNewBuildingCreated (the plan's GetTown is GameThing 0x56FF10 = 0)
  (0x467F99); plan ToBeDeleted(0) (0x467E80, 0x467FA5).
- CitadelHeart::Create with life ≥ 1 calls fn_464F50(citadel, 0) (CREATE_CITADEL); never in the conversion of a plan.

### Placement and town area

- Town::SetTownArea (0x73AAF0): min = 0x7FFFFFFF, max = 0 (0x73AB00..0x73AB1B); fn_73AC90 for each abode (+0x754) and
  each field (+0x780); only called by AddStructureToTown (0x739A0A) and RemoveStructureFromTown (0x739BB6).
- fn_73AC90 (0x73AC90): the object's GetRadius (vt +0x60: Object 0x638110 → Get2DRadius) around its +0x14 in metres (x
  × 10 × 2⁻¹⁶, no height); below the minimum → min = ftol(v × 65536 / 10); z with +0x72C / +0x738.
- The Abode ctor calls AddStructureToTown (0x4013BB → 0x7399A0) before its 3D object exists (+0x40 = 0, 0x63657B), so
  Get2DRadius (Object 0x638180) gives 0 (0x6381E9): the newcomer counts as a point.
- The Field ctor puts the field at the head of +0x780 and calls fn_0073AC90 with its own radius 5.0 (Get2DRadius
  0x528E80) (0x527E64..0x527E82); it stores the GFieldTypeInfo in +0x120 (0x527E23) and GameRand(10) in +0x11C
  (0x527E90).
- SetTownArea also computes a camera point at +0x598 from +0x740 (or the storage pit, or the centre of the rectangle)
  with GCamera::SetPointFromPointDistanceHeadingAndPitch (0x442810); only Town::Save / Load read it (0x73ED70 /
  0x73F490) (0x73AB5C..0x73AC7F).
- fn_0073AE10 (0x73AE10): centre of the rectangle x = ftol((maxX × 10 / 65536 + minX × 10 / 65536) × 0.5 × 65536 /
  10), z with +0x738 / +0x72C, altitude 0 (constants 0x999AA0, 0x8AC41C, 0x8AA3B4, 0x8AC408).
- Town::AsssignTownFeature (0x73EAC0) (at the end of GSetup::LoadMapFeatures 0x71813D): MakeScenicForest (0x741B40)
  for each town (newest first) at the centre of the rectangle (0x741B8C), and in a second pass AssignForestsToTown
  (0x73EB00).
- MapCoords::IsSuitableForFixed (0x6038B0) → fn_604020: uses the static Game3DObject g_tmp (0xD38330) with mesh, angle
  and scale; with no mesh 0 (0x603B10); walks the cells of the NewCollideDescriptor (GetNext 0x46AD80 stops at a cell
  outside the map and ends with 1).
- In each cell: the POSITION's cell in 0..0x1FF, with a block and not water (lc +6 & 0x10); no MultiMapFixed of the
  fixed list (+0x24 & 2 and IsSolidToNewAbode vt +0x7D8) with d < R_obj + scale × max(+0x24, +0x2C) (strict)
  (0x6040A9..0x604170).
- The variant (Game3DObject*, angle, scale, under) (0x603DC0) also rejects the coastline (lc +6 & 0x20,
  MapCoords::IsCoastal 0x6036A0) (0x603F16..0x603F75) and does not count `under` as an obstacle (0x603F96); it is used
  by the scaffolds (0x6E8E37, 0x6E873C, 0x6E9118) and GetNewPlannedBuilding (0x73DA34).
- IsSuitableForFixedAbodeInTown only acts with a town (0x603865); GetNearestTown(&near, &dist, excluded = town, tribe =
  −1) by octagonal cell distance (0x60387B); code 1 → 0xE, which IsOkToCreateAtPos (0x404B10) turns into "accepted"
  (`neg; sbb; neg` 0x404B2E..0x404B32).

### Script buildings

Complements "Converting" above.

- Abode::Init (0x403130) does the MakeFunctional of a whole abode (the script one); a plan one does it in Built.
- StoragePit::MakeFunctional (0x732F30) → SetStoragePit: the last one wins; Creche::MakeFunctional (0x50AB50): +0x744 =
  this only if null (the first one wins, 0x50AB72).
- Abode::Create (0x402E20) calls CreateAbodeSurroundingObjects (0x403E00) last (0x40308E, after Init 0x403087): the
  "did you know?" sign and the lantern.
- Abode::GetNewEp 0x403590 takes the first entrance point of a type from the NewEP block (flag ContainsNewEP; ABODE_EPP
  type and position, in block order).
- TownCentre::MakeFunctional (0x743E80): the town's +0x9A4 = this if still null (like CREATE_TOWN_CENTRE 0x71577C)
  (0x743EAB), and CreateTotemIfNecessary (0x743DA0) → TotemStatue::Create (0x737CC0).
- Totem: the tribe's pedestal (GTotemStatueInfo, table 0xDA1D18 by Abode::GetTribeType) at the centre's special point 6
  (TownCentre::GetTotemPos 0x743F20, through its full matrix), with its Y angle and scale (0x737B20) and the icon on
  top; it rises with the morphed centre: (H(p) − H(pos)) + p.y (GetExtraPos 0x80FF20, IsStaticMorphable 0x81002E,
  0x8100B7..0x8100D0); it faces the worship site (AddToPlayer 0x738130) and carries the creature's icon
  (SetPlayersCreature 0x7381C0).

### Graveyard

- Graveyard.cpp 0x595CB0..0x595FB0; the dead in +0xC4 are saved by Graveyard::Save (0x595EE0).
- Town::SetGraveyard fn_0073D690: +0x748 = g only if +0x748 is null or g is null: the first functional one stays
  (0x73D69C..0x73D6A2).
- AddDead fn_00595E50: requires a town and IsFunctional and (float) +0xC4 < 50 (0x8C6CA4); the 0.18 factor is at
  0x9003E0; the stage goes to LH3DObject +4 bits 21..23 through vt +0x1D8 (fn_007F9C40), so stage 8 shows as 0
  (0x595E58..0x595EC5).
- Graveyard::MakeFunctional (0x595E00): after Abode::MakeFunctional, with a town without a graveyard SetGraveyard(this)
  and AddDead (literal: it counts one dead) (0x595E0C..0x595E3A).
- Graveyard::DeleteDependancys (0x595CE0) (from ToBeDeleted 0x595CB0): if the town has it as graveyard, it searches
  +0x754 for the first other functional abode whose ABODE_TYPE has bit 2 or bit 9 (`test 0x204`, so any civic one,
  e.g. the storage pit, passes) and does SetGraveyard(0), SetGraveyard(found or 0); then Abode::DeleteDependancys
  (0x403F00) (0x595D05..0x595D59).

## Pending

- CREATE_TOWN_TEMPORARY_POTS (0x716B65..0x716BF0).
- The building-site branches (+0x74) of Abode / StoragePit AddResource and RemoveResource (the original's side is in
  [objects-and-resources.md](objects-and-resources.md#store-storagepit)).
- A missing pit pile is created by StoragePit::AddResource (Pot::Create 0x66CF10); openblack makes all six with the
  pit and never deletes them **(approximate)**. How CREATE_ABODE fills a new pit is not read: today it goes through
  StoragePit::AddResource (pile sounds and the pulse at load) **(approximate)**.
- BeliefSFX 0x437F40 needs the interface's position IS +0x14.
- The citadel as a plan (see [Citadel plan](#citadel-plan)).
- Picking a 0 % (invisible) house; Town +0x740 totem / +0x750 workshops / +0xEA4 football; the graves stage's reader;
  feature_build should hide with NotDrawn.
- **(not verified)** freeAdultPlaces = MaxVillagers - adults housed.
- What a broken building does to its town: the alignment and the anger of the town, and the inhabitants leaving when
  the building stops being functional (StopBeingFunctional 0x4073C0, see [Life and damage](#life-and-damage)). The
  villager side of a hit house is in [villagers.md](villagers.md) (the tap from Abode::ReduceLife); whether the building
  side is ported is **(not verified)**.
- `workshops::SpecialPointOf`: «(not verified) that SpecialPoint::yAngle is fn_779910's Y angle».
- `workshops::FinishScaffold`: «the angle when the point is missing: fn_779910's local is not initialised
  (0x779922..0x779939)».
- `wonders::Create`: «edi = Create's fifth argument [esp + 0x20] (0x778EA3). (inferred) the scale: Create(pos, info,
  town, yAngle, scale, ...) as Workshop::Create's argument order».
- `wonders::Create`: «(+0x40)->vt +0x1F4() ? ... : (+0x40)->vt +0x1E8() ((pending) both names)».
- `workshops::Create`: «(+0x40)->vt +0x1E8() 0x779445 ((pending) its name)».
- `k_WorkingLoopSample` 0x4A: «(pending) the sample's name in the .sad».
- `PlayerWonderPower` (GPlayer +0xB8[tribe]): «(pending) who reads it (inferred: a wonder bonus per tribe) and who
  clears it».
- `gravesStage`: «(pending) what the draw shows for it».
- `ProcessAbode` +0xB9: «+0xB9 < 200 -> ++ (no reader found)».
- `QueueBeliefSprite`: «[0xC02A08] != 1 (a constant 1)».
- `GetAddedThisPeriodRatio`: «0x438070 (no direct caller)».
- `PowerSlot`: «(inferred) an abode's GetPlayer is its town's (Town +0x2C, Town::owner)».
- `building_sites::Process` (citadel): «(inferred) not reached in the original: CitadelHeart::Process 0x4665A0 does not
  call MultiMapFixed::Process 0x52F700, the only caller of a site's Process found».
- `TownStats AddAbode` +0x4C: «(not verified) that it equals that book-keeping».
- ShowNeeds: «0x719E98..0x719EAD: the 3D object's +0x60 and +0x5C = 0x2D. (pending) the two fields of the
  Game3DObject».

## Sources

- Disassembly of `runblack.exe` W120.
- bw1-decomp `src/Black/Abode.h`, `GameThingWithPos.h` (Flags +0x24), `Town.h`.
