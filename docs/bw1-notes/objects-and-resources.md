# Objects and resources

Pots and piles of food and wood, taking in batches, the store, the static objects and the rocks, the fields and the
sounds (picking up, LHAudio and QMixer, channels, hand in the water, ambience). Everything **faithful** except what is marked **(inferred)**.
The trees are in [trees.md](trees.md), the map loading in [map-loading.md](map-loading.md) and the buildings' side of
the stores (abodes, building sites, workshops) in [buildings.md](buildings.md).

> **Code rules.** An object's state (amounts, poison, the field's growth, a channel's owner) lives in ECS components or
> Locator services, never in globals; the sounds and meshes load once through the resource caches; formulas such as
> the pile proportion, the field growth or the QMixer gain are pure functions tested with fakes in `test/`;
> comments describe behaviour in plain English, with no decompiled names or addresses (those belong here). See
> [the conventions](../refactor/README.md).

- [Pots and piles](#pots-and-piles-pot--pileresource)
- [Taking in batches](#taking-in-batches-multi-pick-up)
- [Store](#store-storagepit)
- [Static objects](#static-objects-mobilestatic-rocks)
- [Fields](#fields-field)
- [Sounds](#sounds)
- [Moved to other pages](#moved-to-other-pages)
- [Pending](#pending) · [Test hooks](#test-hooks) · [Sources](#sources)

## Pots and piles (Pot / PileResource)

- `Pot::Create` (0x66CF10): potType 0 = simple Pot, 1 = PileFood, 2 = PileWood. The hand pots (HandWood 11,
  HandFood 12) are also piles.
- **Giving a pot** (any pot or pile in the hand, the hand's HandWood / HandFood included):
  `Pot::ValidToApplyThisToObject` 0x66DD50 is 1 when the target `IsResourceStore` of the pot's type, or is a Pot
  (RTDynamicCast) of the same type. `Pot::ApplyThisToObject` 0x66DDD0: a store → `DeleteObjectAndTakeResource` → 3
  (0 if it took nothing); otherwise `Pot::AddResourceToPos(own MapCoords +0x14, IS, type, GetResource, IsPoisoned, 0)`,
  `GoolooGooloo`, `ToBeDeleted(0)`, 3. So a hand pot pressed over a pit, its rim or one of its piles of that type joins
  the pit's single total. openblack: `ecs::held_apply` (the Pot rows); the release still puts a hand pot down where it
  lands (`PutDownHandPot`, `AddResourceToPos`), as the original's release does.
- **Simple pot**: scale `min(5, amount/scaleEvery + 0.25)` (`Pot::GetScaleFromAmount` 0x66D4A0). `scaleEvery` is not
  used for anything else.
- **Pile**: it does not scale; it sinks. `PileResource::SetSize` (0x66E900): target `(GetProportionRaised − 1)·altura`,
  animated over 1 s with the Zoomer; it is drawn at `GetAltitude(pos) + desplazamiento` and is not visible if it is fully buried.
- `GetProportionRaised` (0x66F1B0 wood / 0x66EB60 food): x = amount/maxInPot in [0.1];
  p = x > 0 ? 0.05 + 0.95x : 0; food: 1 − (1 − p)². A single copy, `ecs::object::GetProportionRaised`
  ([engine-math.md](engine-math.md#object-size)); the 2D radius of a food pile is
  `GetProportionRaised × Object::Get2DRadius` (0x66F180), so when empty it measures 0.
- When created, every pile starts buried (`−altura`) and rises over 1 s (`CallVirtualFunctionsForCreation` 0x66E300).
- Creation scales: **MagicFood 0.3**, **MagicWood 0.7** (constructors 0x5FA9F0 / 0x600E20); the rest 1.
- `PileFood::Draw` (0x51BF80): the store's food pile (info 2) and the magic food (info 10) offset the
  texture in V by `0.25·(1 − clamp(desplazamiento/altura + 1))` (LH3DObject vfunc 0xE8, **inferred** as a UV
  offset). The grain seems still and the pile "shrinks".
- `FoodPile` (info 8, MSH_B_WORSHIPGRAIN) is the pile of the worship sites; its mesh is offset.
- Pot flags at +0x74: bit 0 poisoned (`Pot::IsPoisoned` 0x55D4E0 / `SetPoisoned` 0x55D510), bit 3 from a building site
  (`IsAPotFromABuildingSite` vt +0x830 = Pot 0x55D590), bit 4 speed-up (`IsSpeedUp` vt +0x4A8 = Pot 0x55D4F0).
  `PotStructure::SetSpeedUp` 0x55D530 only keeps the flag. `PileFood::SetSpeedUp` 0x66E220 also shows it: switching it
  on starts the PILEFOOD_SPEEDUP spot visual (46, scale 1, forever, on the pile); switching it off closes it.
- Resource virtuals: `GetResourceType` vt +0x690 = DeadTree 0x5110C0 / Tree 0x74B820 WOOD, PileFood 0x66EC50 FOOD,
  PileWood 0x66EC60 WOOD, Pot 0x55D4C0 its +0x68, Object 0x402750 −1; `RemoveResource` vt +0xA0 = DeadTree 0x511370,
  PotStructure 0x66EE10, Pot 0x66D3F0; `GetCarriedTreeType` vt +0x820 = DeadTree 0x511A20, Tree 0x55D900, Object
  0x402AF0 (0); `IsDeadTree` vt +0x47C = DeadTree 0x510990.
- **Adding** (`JustAddResource` vt 0x8C): `PileResource::JustAddResource` 0x66D330 plays the pile sound with the amount
  asked for (not for the hand pots, infos 11 and 12). Then `Pot::JustAddResource` 0x66D2B0 caps at maxAmountInPot
  **only when nextPotForResource < 19** (so not for magic or loose piles), sets poisoned = poisoned || IsPoisoned, and
  calls SetSize (vt 0x85C).
- **Removing** (`Pot::JustRemoveResource` 0x66D410, the type is not tested): n ≥ the amount → all of it, and the emptied
  pot loses its reaction (`RemoveReaction` 0x66D6A0), its poison and its fire (+0x44, deleted, 0x66D431..0x66D43F);
  then SetSize (0x66D451). `PotStructure::JustRemoveResource` 0x66D9B0 deletes an emptied pile that is not part of a
  structure (0x66D9E0). A pile of a storage pit, building site or workshop stays (0x66D9D3; `Workshop::CreatePileWood`
  0x779630; a site's pile until `BuildingSite::ToBeDeleted` 0x43B960 releases it). The plain Pot class has no such
  step and stays (`Pot::RemoveResource` 0x66D3F0).
- `PotStructure::GetResource` 0x66EF00: a pile linked to its building site (`IsPartOfStructure` vt +0x860, 0x66DA00;
  `IsLinkedToThisBuildingSite` vt +0x11C, 0x43D830) reports its own amount (0x66EF12..0x66EF47); a workshop's pile
  reports the workshop's mirror (`Abode::GetResource` 0x404D30, 0x66EF2F). `Object::GetResource` 0x639520: the type
  must equal `GetResourceType` (vt +0x690), then `GetDefaultResource` (vt +0x694; Scaffold 0x6E9D30 = value ×
  WoodValue). `Scaffold::GetResourceType` 0x55E0F0 = WOOD.
- Add/remove redirects: `PotStructure::AddResource` 0x66ED70 / `RemoveResource` 0x66EE10 send everything to the
  building site when the pile is linked to it (0x66ED7E..0x66EDB9 / 0x66EE1E..0x66EE4F), or to the structure (for
  example `Workshop::AddResource` 0x779E00 / `RemoveResource` 0x779EC0) when it is part of one but not linked; without a
  structure they use `JustRemoveResource` (0x66EEAB). `Pot::AddResource` is 0x66D290.
- Deletion: `PileFood::ToBeDeleted` 0x66E100 closes its speed-up visual first (wood piles: `PotStructure` 0x66D960,
  nothing); both end in `Pot::ToBeDeleted` 0x66D110 → `RemoveReaction` 0x66D6A0.
- `IsResourceStore` (vt +0x680): `StoragePit` 0x55CD20 = 1 for any type; `PotStructure` 0x66DA30 asks its structure
  (and the type must be the pile's own or ANY −2); `Pot` 0x66F560 = 0; `MultiMapFixed` 0x52F1F0 (any building) = WOOD
  with a building site (+0x74); `Workshop` 0x77A650 = MultiMapFixed's, or WOOD, or ANY; `WorshipSite` 0x77DEC0 =
  MultiMapFixed's, or FOOD, or ANY; `Object` 0x4192D0, `Rock` 0x439710 and `Tree` 0x55D8F0 = 0. What each class is
  worth when given: an animal FOOD (0x417530), `ftol(GetFoodValue(3))` 0x41BC80 = `(GetFoodType() & 3) ? foodValue :
  0` (0x4026D0); a fence (`MobileStatic::IsFence` 0x609110: meshes 0x38, 0x51, 0x52) WOOD (0x6096B0),
  `ftol(Object::GetWoodValue)` 0x6096D0 = `woodValue × life × scale³` 0x6395C0; any other MobileStatic −1. info.dat
  gives the Short and Tall Celtic Fence woodValue 25; no MobileStatic row of info.dat uses the American fence's mesh
  (0x38), so no fence of that mesh is ever made from a row. openblack: `ecs::resource_stores` (`IsResourceStore`,
  `DeleteObjectAndTakeResource`, the pure `IsStoreForType`, `PileIsStoreForType`, `AnimalFood`, `FenceWood`), and
  `object_delivery`'s resource of an animal or a fence. A worship site's food pot and a building site's piles are not
  linked to their structure in openblack, so they answer as loose pots (**approximate**).
- **Putting down** (`Pot::AddResourceToPos`): the search radius is multiplied by
  `GetRadiusMultiplierForApplyingPotToPos`: Pot 0x66F520 = 2, Object 0x63AAD0 and WorshipSite 0x77E480 = 1.2. The cells
  are walked fixed list first, then the mobile list (0x66F2CA); pots (type 21) are at the tail of the fixed list.
  Every new pile is in the cells at once (`CallVirtualFunctionsForCreation` 0x607150 → `InsertMapObject` at 0x6071F9).
- Script type: `Pot` 0x66F530 = 16 for every pile (PileFood, PileWood, MagicFood, MagicWood, PotStructure).
- `ADD_RESOURCE` (`GScript::AddResource` 0x6FAD10): POP the thing, the amount (ftol 0x6FAD49), the RESOURCE_TYPE
  (raw). No thing → "No thing for resource", not an Object → "Not object for resource", both push 0; else
  `Object::AddResource` (vt +0x9C)(type, amount, IS 0, poisoned 0, pos 0, 0) and PUSH (float)(uint64) what it took.

## Taking in batches (multi pick-up)

- Press on a pile: 25 immediately into a hand pot. Every 0.1 s turn: `(int)(8 + 62·t²)`, t = n/60,
  up to **20000** per batch (`maxAmountCanBePickedUp`). Verified: 413 at 3 s, 1746 at 6 s.
- The hand stays fixed in x,z over the pile, y = ground + height of the pile.
- Releasing the button leaves the pot in the hand; another press + release puts it down or throws it.
- Putting down: it joins a nearby pile or store of the same resource (approximate radius 15 m) or creates MagicWood/MagicFood.
- Particles (`SF_MultiPickUpWood/Food`, `ER_MultiPickup::ModifyAtomCollection` 0x6A77C0): 8 per second, each one goes
  in a straight line over 1 s from the ground under the hand to the current position of the hand; they are destroyed when picking up stops.
  Wood = mesh MSH_I_OFFERING_WOOD at 0.35 with `AppearanceRuleTumble`; food = grains from S_SpriteSheet1 (32 frames,
  20 fps); fish = S_Spangle_A (not implemented).

## Store (StoragePit)

- A single total for 5 piles of wood + 1 of food (`StoragePit::AddResource` 0x732F60 /
  `RemoveResource` 0x7332A0).
- Adding wood: piles 1→5, each up to 5000 except the last one (no cap). Taking out: **5→1**, regardless of which
  pile it is taken from. A pile at 0 stays buried and is not visible.
- `PotStructure::GetResource` (0x66EF00): a store pile reports the store's total.
- Layout: food pile at +0xC4, wood piles 1..5 at +0xC8..+0xD8.
- `StoragePit::AddResource` 0x732F60 (type, n, IS, poisoned, pos, k): when the pit has a building site that is not
  built yet (+0x74), WOOD and ANY (−2) go to the site (0x732F67..0x732F99). Only FOOD and WOOD fill piles (0x732FA2 /
  0x733083). A missing pile is made first (`Pot::Create` at `GetResourcePos`, 0x732FF8 / 0x7330D2), and only an
  available pile is filled (0x73302C..0x733037).
- Pulse: if the store held none of that resource before and something went in, Town +0x5E8 = 1 and +0x5EC = 0
  (0x73316D..0x73318D). Then `DoResourceAdding` (0x7331B1) with what the piles took; it returns the amount stored
  (0x7331B9). The mirror (`GetResource`, 0x73315E) is read after the piles but before `DoResourceAdding`, so it still
  holds the old total.
- `StoragePit::RemoveResource` 0x7332A0: only FOOD and WOOD (0x7332E9; any other type 0, with no
  `DoResourceRemoving`). Wood piles 5 → 1 (0x7332EF..0x73331B), each through `PotStructure::JustRemoveResource` (an
  empty pit pile stays). If something came off, `DoResourceRemoving` gets the removed total (0x73331D..0x733337).
- `PotStructure::RemoveResource` 0x66EE10 on a pit pile: over = `CalulateAmountOverMaximum` (vt +0x8EC,
  0x66EE71..0x66EE9A). The touched pile gives n − min(over, n) when over > 0, otherwise n, and an empty pile is still
  asked. If r ≠ 0, the pit's `DoResourceRemoving(r)` runs (0x66EE9C..0x66EED3). If r < n, the pit's `RemoveResource(n −
  r)` takes the rest (0x66EED9..0x66EEF5).
- `StoragePit::CalulateAmountOverMaximum` 0x733260 (signed): WOOD = total − 5 × GPotInfo[Wood Pile 1].maxAmountInPot
  (0xD4CB48); any other type = total − GPotInfo[Storage Pit Food Pile].maxAmountInPot (0xD4CA04).
- Poison: `StoragePit::IsPoisoned` 0x7336B0 = `IsPoisonedResource(FOOD) || (WOOD)` 0x733550 (an available pile whose
  `Pot::IsPoisoned` is set); `StoragePit::SetPoisoned` 0x7335D0 ORs each pile's flag, so it is never cleared. An
  abode's `IsPoisoned` is 0 (GameThingWithPos 0x402400).
- **Abode/store resource economy** (`Abode::DoResourceAdding` 0x404DF0, vt +0x8E4): with no interface or no town, only
  the add itself (`JustAddResource` 0x404D40: +0xBC[type] += n, no cap). Otherwise, the town desire
  `CallDesireFunction(type != FOOD)` 0x745D80 is read before and after. The difference is multiplied by
  `Town::GetGameTurnResourceLastRemovedModifier` 0x740030 (0x404E70). It then calls `GAlignment::Update` 0x414520 for
  the interface's player (0x404E87), and `GBelief::AddToBelief` 0x437EB0 of the town with delta × (not the owner ?
  multiplierForNonOwnerAddingResource : 1) × multiplierForAddingResourceToTown (0x404E8C..0x404EC3). Last comes
  `DoCreatureMimicAfterAddingResource` (vt +0x68C: StoragePit 0x733810, MultiMapFixed 0x52F210). The town side is in
  [buildings.md](buildings.md#resources-held-by-objects).
- **The storage pit's creature deed** (`StoragePit::DoCreatureMimicAfterAddingResource` 0x733810, disassembled
  from W120; found by the creature mimicry RE). `MultiMapFixed`'s test runs first (0x52F210: WOOD on a pit that is
  still a building site gives deed 7 and stops). Then the status's +0x128, the player who owned what the hand picked
  up (set at every pick-up, see the next point), is compared with the status's own player (vt +0x1C), pointer to
  pointer: a NULL +0x128 is never the player's own. The pit's owner is not read. FOOD (0x73382C): the same player gives 2 (PUT_FOOD_IN_STORAGE_PIT, 0x733864); another one, or
  none, draws `GameRand(2)` at 0x733844 (synced stream) and gives 43 + (draw != 0) (STEAL_FOOD_FROM_FARM /
  _FROM_STORAGE_PIT, pushed at 0x733861, reported at 0x7338B6). The draw comes **before** any creature check
  (`ConsiderMakingCreatureMimicPlayer` 0x4EA900), so it happens even when the player has no creature. Any other type
  draws nothing: 4 (PUT_WOOD_IN_STORAGE_PIT) for the same player, 45 (STEAL_WOOD_FROM_STORAGE_PIT, 0x73389A)
  otherwise. Workshops (0x77A680) and pots (`Object` 0x63AAE0, vt +0x68C of Pot, PotStructure and PileResource) never
  draw.
- It is asked up to twice for one deposit: at the end of `Abode::DoResourceAdding` (0x404ED2, with a status and a
  town, whatever was added; `StoragePit::AddResource` reaches it at 0x7331B1), then by the deposit itself:
  `Object::DoDeleteObjectAndTakeResource` (0x63A9C1, when something was taken) and `Pot::AddResourceToPos` (0x66F3BB,
  for every store or pot that was offered, whatever it took). **Matched** in openblack
  (`StoragePitStore::DoCreatureMimicAfterAddingResource`, called from `object_resources::DoResourceAdding`,
  `object_delivery::DoDeleteObjectAndTakeResource` and `pot_resource::AddResourceToPos`); the deed is only returned
  until the creature's mimicry is ported.
- **Putting down into a pit passes the status** (disassembled from W120). `Pot::AddResourceToPos` 0x66F270 calls every
  offered object's `AddResource` (vt +0x9C, 0x66F3A9) with (type, what is left, the status, poisoned, the position,
  0): a storage pit gets the status in `StoragePit::AddResource`, so with a town its `DoResourceAdding` side runs
  (desire, alignment, belief and the first deed), then the put-down's own deed (0x66F3BB). A pile of a store
  forwards every argument to its store: `PotStructure::AddResource` 0x66ED70 calls the structure's `AddResource`
  (vt +0x860 gives the structure; the call at 0x66EDE6), or the structure's building site's when the pile is linked
  to it (0x66EDB9), and its own `JustAddResource` only without a structure (0x66EE04); the pile itself tells the
  creature nothing (`Object` 0x63AAE0). The poison goes with it: `Pot::AddResourceToPos` passes its poisoned argument
  to every store or pot offered (0x66F3A9) and `PotStructure::AddResource` forwards it to the structure (0x66EDE6).
  **Matched** in openblack: `pot_resource::AddResourceToPos` gives a pit the dropper and the poison through
  `pot_resource::StoragePitTakesPutDownResource` (the pit's `AddResource`, then the deed) and a pit's pile through
  `pot_resource::PotStructureAddResource`; `object_resources::AddResource` on a pit's pile passes the dropper on too
  (an object given to a pile: its pit's town side and deed).
- **Who owned what the hand picked up** (`GInterfaceStatus` +0x128, a `GPlayer*`; disassembled from W120, every
  write to +0x128 of a status in the exe listed). `GInterfaceStatus::SetToZero` 0x5DBA00 clears it to NULL (0x5DBAE1;
  called by the status's constructor 0x5DB8AD and `Init` 0x5DD1A9, and by `GInterface::SetToZero` from
  `GInterface::Init` 0x5CE676, which `GPlayer::Init` 0x64929B calls). Then:
  - every object put in the hand: `GInterface::PlaceObjectInMagicHand` 0x5DA6F0 calls the status's add-to-hand
    step `fn_005DC330` (0x5DA7C1) when the object's `InterfaceSetInMagicHand` returned 1; on success it sets
    LastPickedUpObject (+0x120) and +0x128 = `MapCoords::CalculateMostInfluentialPlayer` 0x603830 of the object's
    position (0x5DC3E9..0x5DC3F7). `Influence::CalculateMostInfluentialPlayer` 0x5CD630 starts from the neutral
    player and keeps a player only with an influence above the best so far (start 0), so it is never NULL.
  - a locked select's start overwrites it after its hand pot went in the hand: `PileResource` 0x66E7EC = the pile's
    `GetPlayer` (vt +0x1C, read after the first scoop); `Field` 0x5299B6 = the most influential player at the
    field's position; `FishFarm` 0x52D853 = `FishFarm::GetPlayer` 0x52C850: the town's player (`Container` +0x2C,
    0x462A50), **NULL without a town**.
  - A pile's `GetPlayer`: `PotStructure::GetPlayer` 0x66F230 asks the structure it is part of (+0x78, a storage
    pit's piles: `Abode::GetPlayer` 0x405F70, the town's player, else `GameThing::GetPlayer` 0x570130, the neutral
    player), else its own +0x7C, else the neutral player; +0x7C is 0 in the constructor (0x66D94D) and set to
    `GetPlayer()` by `PotStructure::CallVirtualFunctionsForCreation` (0x66DBA4), so a loose plain pile is the neutral
    player's. `MagicFood::GetPlayer` 0x5FA980 (+0xBC) and `MagicWood::GetPlayer` 0x600DA0 (+0xB4) answer their own
    player (the neutral one when made with none, 0x600E64..0x600E8A); the workshops' and building sites' piles are
    Magic Wood, so they are the neutral player's unless a player made them.
  - Every deposit by that status carries it: the hand's (objects given or thrown, pots put down) and the food and
    wood miracles, whose IS is the leader interface of the spell's player ([miracles.md](miracles.md)).
  - **Matched** in openblack: `pot_resource::Dropper::sourceOwner` (`std::optional`, none = NULL) is the hand's
    record (`HandSystem`, read by others through `HandSystemInterface::GetSourceOwner`): `HandSystem::PickUp` records
    `alignment::MostInfluentialPlayer` of the object's position; the locked-select start then records
    `object_resources::PlayerOfPile` (read before the first scoop), the field's most influential player or
    `fish_farms::PlayerOf` (none without a town). `Pot::owner` is the neutral player unless a miracle or a hand pile
    set it. Every `Dropper` the hand builds (`HandSystem::InterfaceStatus`) and the miracles' carry it.
- `Abode::DoResourceRemoving` 0x404F60 (vt +0x8E8): the desire before (0x404FA0); with an interface and a town,
  `Town::SetGameTurnResourceLastRemoved` 0x7400D0, the desire after, and `GAlignment::Update(this, type, −n, before −
  after)` for the **town's owner** (0x404FEF..0x405005; −n is the amount asked for).
- `Abode::AddResource` 0x404D90 / `RemoveResource` 0x404F10 send WOOD or ANY to the abode's building site (+0x74) when
  it has one (0x404D9C..0x404DE9 / 0x404F1A..0x404F4E); `Abode::JustRemoveResource` 0x404D60 = min(n, what it has).
- **Giving an object to a store** (`Object::DeleteObjectAndTakeResource`, vt +0x684; callers first ask
  `IsResourceStore` vt +0x680): `Tree::ApplyThisToMapCoord` 0x74C008 (put down on a store;
  `MapCoords::IsWithinBuilingSite` 0x605250) and `Tree::ReactToPhysicsImpact` 0x74B6F9 (it hit one;
  `GetGameObjectWhoHitMe` 0x644F00). `Object` 0x63A930 = 0. StoragePit 0x733750 and WorshipSite 0x77E7B0 wrap
  `Object::DoDeleteObjectAndTakeResource` 0x63A940.
- `StoragePit::DeleteObjectAndTakeResource` 0x733750: it reads the player of IS first (0x73375A..0x733767). If the
  object is in physics and its PhysicsObject's interface is the local one (`GGame::MyInterfaceStatus` 0x555880), it
  calls `HelpProfile::Trigger(6 SUPPLY)` 0x5C46E0 (0x73376D..0x7337A6; the same code in WorshipSite at
  0x77E7B6..0x77E7EC). Then comes `DoDeleteObjectAndTakeResource` (0x63A940), then `Reaction::CreateReaction(this, 22
  REACT_TO_HAND_PUTTING_STUFF_IN_STORAGE_PIT, player, 1)` 0x6E3D70 (0x7337B4..0x7337BA; its result unused). It returns
  1 (0x7337C5).
- `DoDeleteObjectAndTakeResource` 0x63A940: `this->AddResource(object's type, object's amount, IS, object's poison,
  &object->pos, 0)` (0x63A965..0x63A99C). If something was taken and the interface is the local one, `ResourceDropSFX`
  (a store's guidance resource type is 0, so it is silent). The physics list is 0xD47814, 0x1DC bytes per entry. Then
  `GoolooGooloo` 0x5E6540 (a 500 ms ghost of the object's 3D model) and `ToBeDeleted(0)` (0x63AAA3 / 0x63AAB1).
  **Matched** in openblack (`object_delivery`, the ghost through `ecs::object_ghosts`, rendering-objects.md).
- Pit life cycle: `StoragePit::StopBeingFunctional` 0x733960 → `Pot::SetupReaction` 0x66D660 on each pile that holds
  something; `RestartBeingFunctional` 0x7339D0 → `RemoveReaction` on each available pile. `StoragePit::ToBeDeleted`
  0x732C30 (before `Abode::ToBeDeleted`, 0x732CB8) runs `DeleteDependancys`, then for each available pile
  `SetMultiMapFixed(0)` and its `ToBeDeleted(now)`, and clears the slot (0x732C3E..0x732CB3). An unavailable pile keeps
  its slot. `StoragePit::MakeFunctional` 0x732F30 → `Town::SetStoragePit` 0x73EA60.

## Static objects (MobileStatic, rocks)

- Position `GetAltitude(pos) + altitud del script`, rotation `SetYXZMatrixOnly(y, x, z)`, uniform scale
  (`Game3DObject::SetPosition` 0x63B680, `MobileStatic::GetWorldMatrix` 0x608DE0).
- The original does **not** settle them on the ground: `GetAltitudeFondation` is only used for buildings.
- **Fences are wood.** A fence (`MobileStatic::IsFence` 0x609110) is WOOD worth `ftol(woodValue × life × scale³)`
  (0x6096B0 / 0x6096D0 / 0x6395C0; info.dat Short and Tall Celtic Fence 25). In the hand:
  `MobileStatic::ValidToApplyThisToObject` 0x608BB0 = fence and `target->IsResourceStore(WOOD)`; `ApplyThisToObject`
  0x608C30: fence, store, not INDESTRUCTIBLE (+0x24 bit 0x4000) → `DeleteObjectAndTakeResource` → 3. Thrown:
  `MobileStatic::ReactToPhysicsImpact` 0x608FC0: fence, not INDESTRUCTIBLE, the hit (`GetGameObjectWhoHitMe`, no
  availability test) `IsResourceStore(WOOD)` → the hit's `DeleteObjectAndTakeResource(this, po +0x24)`. Both also
  have a branch for the static of info +0x128 == 0x31 dropped on an AnimatedStatic (`AddGateStone` 0x422D50, the gate
  stones), not ported. openblack: `ecs::held_apply` (the Fence rows), the fence branch of the physics' impact
  (`PhysicsObjects.cpp`), `object_delivery` (its wood and the mulch sound, WOOD that is not a pot).
- `GObjectInfo::IsOkToCreateAtPos` 0x638C40: `MapCoords::CollideWithFixed` 0x604FE0 on the live map cells, x and z
  only; bit 8 clear → yes. Off the map the value is 0xFFFFFFFF, which has the bit set.
- Script types: CREATE_MOBILE_STATIC (0x716DC1 → fn_00608840 / fn_00608770) makes a GBaseOnly for info 6 (type 0), a
  Bonfire for info 8 (type 8), a Rock for mobileType 2 (`Rock` 0x6E79E0 = 33), otherwise a MobileStatic (0x609330 =
  8). A Fragment is 8 (0x76F7C0 jumps to MobileStatic's).

## Trees

Moved to [trees.md](trees.md) (uprooting, dropping, forests, growth, drawing).


## Fields (Field)

- The 6 GFieldTypeInfo are identical: ageGrowth 80, ageRecolt 1200 (ripe), timesToSow 30, foodValueTakenWithHand 25,
  totalFoodInField 350, maxFarmerInFarm 10, sun 0.5/1.5, rain 1.5/1.5, ratioBeforeRipe 0.2. The `IsUnripe` symbol
  (0x5298D0) returns **ripe** (growth ≥ 1200). The field rows of info.dat are at file offset 0x49898 + 0x144·i.
- Layout: +0xD4 / +0xD8 the farmers list (8-byte nodes, new ones inserted at the head) and its count, +0x118 the town
  (`Field::GetTown` 0x528960), +0x11C the turn offset, +0x120 the GFieldTypeInfo (set in the ctor 0x527E23). Info
  offsets: +0x120 ageGrowth, +0x124 ageRecolt, +0x128 timesToSow, +0x130 totalFoodInField, +0x134 maxFarmerInFarm,
  +0x138/+0x13C sun growing/ripening, +0x140/+0x144 rain growing/ripening, +0x14C ratioBeforeRipe, +0x150
  effectOfWaterSpell.
- Order: `GlobalGameLists::Process` 0x591370 calls `Field::Process` (vt +0x5FC) for every field every turn (list g_game
  +0x205C04, newest first). The town's abode pass fn_747600 (0x74762A) also calls it every processAbodeEvery turns, so
  on those turns it runs twice. A town's fields (+0x780) are newest first (head insertion in the ctor
  0x527E64..0x527E75).
- `Process` 0x529020 every 10 turns (+ an offset 0..9), with the 30 crops sown and not ripe:
  d = 2·(0.5·alignment + 1)·(0.5 growing | 1.5 ripening or with rain); growth += d, food += d·350/1200.
- `Field::Process` 0x529020 first calls `Abode::Process` 0x404440 (0x529026: the site's `MultiMapFixed::Process`
  0x52F700 and the empty-abode counters +0xB0 / +0xB9). Then: (turn + offset) % 10 (0x52902B..0x529047); **no growth
  while on fire** (`Object::IsOnFire` 0x637CC0, 0x52904F); the field must be fully sown ((float)crops < timesToSow →
  nothing, 0x52905D) and not past ripe (0x529084).
- Growth inputs: `MapCoords::GetAlignment` 0x6057B0 at the field (0x5290A1) and `GClimate::IsRaining` 0x7714B0 at its
  point (0x5290BF); it is "growing" while growth < ageGrowth, read before the step (0x5290CA). Then growth += d
  (0x52911C) and food += d × totalFoodInField / ageRecolt (0x529133..0x52914F).
- The town pulse after growth (0x52912C / 0x529159, `IsUnripe` called twice on the same value, first true and then
  false) can never fire.
- `Field::ApplyWaterSpell` 0x528F78..0x528FEA: a field past ripe gets nothing; if crops ≤ timesToSow, crops =
  ftol(timesToSow + 1).
- `RemoveFood(n)` 0x5295A0: 0 without food or unsown; cost = n ripe, (int)(1.2·n) unripe; if it is not enough:
  unripe it is emptied and gives (int)(0.2·n); ripe it gives what is left and is deleted entirely (it has to be sown again).
  In detail: unripe cost = ftol(amount × ratioBeforeRipe + ftol(amount)) (0x5295F0..0x529624); the cost is compared as
  unsigned with food (0x529626). When it is enough, food −= cost and it returns ftol(amount) (0x5296CF..0x5296E8). When
  it is not enough: `SetTemperature(0)` 0x639A60 (0x529643), the town pulse +0x5E8 = 1 / +0x5EC = 0
  (0x52964E..0x52966A); unripe → food = 0 and it returns ftol(amount × ratioBeforeRipe); ripe → it returns ftol(food),
  and food, crops and growth are set to 0 (0x5296A8..0x5296CC).
- `Field::GetPercentFull` 0x529500 = crops / timesToSow. `GetFieldActivity` 0x529350: < 1 full → 1 (sow); growth ≥
  ageGrowth → 2 (harvest); otherwise 0.
- `Field::GetDesireToBeFarmed` 0x5293A0: a fire effect at +0x44 (the pointer, not `IsOnFire`) or not functional (vt
  +0xD4) → 0. Otherwise a = 1 − min(1, farmers / maxFarmerInFarm); activity 2 → growth ≥ ageRecolt ? a : 0; activity 1
  → (1 − min(full, 1))·a³; otherwise 0.
- `Field::PlantCrop` 0x5291A0: if crops < timesToSow, crops++ and true (the position is not read). `GetPlantCropPos`
  0x529210 is misnamed: it answers "still sowing" (crops < timesToSow).
- Farm points: `RandomFarmPoint` fn_00528970 = the position + (5 − GameFloatRand(10)) metres on x, then on z
  ("Field.cpp" lines 0x164 / 0x165, 5.0 at 0x8AB6E4); `RipeFarmPoint` fn_00529240 draws nothing unless growth ≥
  ageRecolt. `Field::IsTouching` 0x529290 (vt +0x6B4): fx − 5 ≤ px < fx + 5, and the same on z. `GetArrivePos`
  0x529330 = the position. `GetFoodValue` 0x529700 = growth < ageRecolt ? 0 : food.
- Farmers: `Field::AddFarmer` 0x5283E0 adds at the head, with no maximum and no duplicates (maxFarmerInFarm is only used
  in the desire). `RemoveFarmer` fn_00528340 unlinks every node of the villager and **always** clears the villager's
  target (+0x118), even when it was not in the list.
- `Field::DeleteDependancys` 0x528100 (from `Field::ToBeDeleted` 0x5280F0 → `Abode::ToBeDeleted` 0x402C6F, vt +0x910):
  for each farmer, head first and with the next one taken before the call, `SetTopState(163 DECIDE_WHAT_TO_DO)` (its
  exit `ExitFarming` 0x75A2A0 unlinks it) and target = 0 (0x52814E). Then it leaves the town's list +0x780
  (`Town::SetTownArea`, 0x5281C3), the map (0x5281CA..0x5281DC) and the global list (0x5281F1..0x52826C). The fence
  (+0xC8, fn_00528CA0) has a builder 0x528B80 with no caller.
- Hand: action button over the field (locked selection; needs growth > 0 and food > 1). It starts with
  (int)min(25, food), **half if it is ripe**, removed from the field; per turn (int)min(8 + 62t², food), t =
  min(turns/60, 1), ≤ 20000 − what is in the hand, half if ripe; the hand receives n (oddity). Grain particles
  (`SF_MultiPickUpFood`), sound G_PICKUPFOOD. It cannot be given back.
- Drawing `Draw` 0x528570: one mesh (MSH_T_WHEAT); only with growth ≥ 20 and food ≥ 25; it sinks v = food/350 − 1
  over 1 s (y += 2·v·scale·height) and fades out below v = −0.8; colour from olive to light green while growing, to
  white while ripening; the ripe ones sway with the trees' wind.
- The original's hand leaves the last unit of food of a ripe field forever: its halved and truncated amounts reach 0,
  and `RemoveFood` only deletes the field if it is asked for more than it has.
- openblack: `ecs/Fields`, `HandFish.cpp` (`TryPickUpField`, `UpdateFieldPickUp`), hook `OPENBLACK_HAND_TEST_FIELD=1`.
  The engine is faithful: the field is born empty and only the farmers sow it. The alignment/rain in the growth is
  missing.
- **Colour and sway of the mesh** (`Field::Draw` 0x528570):
  `BlendColor` 0x5284C0 (k = 0 gives a, 255 gives b, `(a(255−k) + b·k)/255` truncated): growing, olive (121,145,25) →
  light green (170,212,67) with k = 255·(1 − food/350); ripening, olive → white with k = 255·(growth − 80)/1120;
  ripe, white. It multiplies byte by byte the object's terrain light, `(c·tinte) >> 8` (fn_0080BF10), before the
  haze and the N·L: in openblack it goes in the x of the instance's fifth column, negative
  (`−1 − r·65536 − g·256 − b`, `argb_colour::PackInstanceTint`, [rendering-objects.md](rendering-objects.md#the-object-colour-fields-in-the-instance)). The
  ripe ones sway: column 1 (up axis) is sheared in world z with `1.75 × scale × T0[i]`, `T0 = −0.03·cos(fase)`
  of 16 phases (`Tree::PreDraw` 0x74A7C0: speed Random(1, 2) every 2 s, phase += ms·speed·0.00106061; the wind
  angle is always 0), `i` fixed per field (in the original, bits of its address); only the drawn matrix.
  `ecs::FieldDrawColour`, `ecs::WindSway`. The trees use the same table ([trees.md](trees.md#drawing)).
- Sway timer: the 16 speeds are at 0xDA3A4C, past the raw end of .data, so they are 0 until the first draw (**no sway
  for the first 2 s**); the 2 s counter [0xDA579C] += the frame ms (0x74A7D6) and is reset above 2000 (0x74A879). The
  field colours are startup constants (0x528440 / 0x528470 / 0x5284A0).


## Sounds

- LHAudio's pitch is a **percentage** of the wav's frequency (100 = normal). On start (0x1001278B, unsigned
  integers): d = deviation·p/100, p = p − d + rand·2d/32767 (0 → 100), frequency = rate·p/100 (integer division; the same
  in `LHSampleSetPitch` 0x10013520, which does nothing if the channel already has that p). The pitch, the volume, the loops
  (+0x248) and the .sad mode only count if their bit is in the flags at +0x244 (0x1, 0x20, 0x40, 0x400) and the
  caller has not set them (mask +0x1C of the options); otherwise, 100, 127, 0 and 3. openblack passed the raw number.
  In the sample header (read at 0x10011420) the pitch is at +0x260 (flag 0x1) and the volume at +0x25C (flag 0x20).
- **Volume** (verified with Unicorn): LHaudio sends to `QSWaveMixSetVolume`
  floor(master·v/127)·258 (0x100133C1; master = BWSetup's `AudioSampleMasterVolume` = 127 → v·258, 0..32766) and
  QMixer stores it as vol/32767 (0x18007AE5) and **multiplies** it by the distance gain (0x1800AE20):
  linear gain v·258/32767 (`sample_play::QMixerGain`, `Sound::volume`). The .sad's "user param"
  (`LHSampleGetUserParam` 0x10014230) is the high half of that same u32 (+0x25C >> 16).
- Picking up from a pile, field or fish farm: **a single looping channel** (G_PICKUPWOOD 98 for wood; G_PICKUPFOOD 44
  for the rest) whose pitch rises to ftol(60 + 180·t²) % per turn; it stops on release or when it runs out. Putting down on a pile:
  G_PileFood/Wood(Small) depending on the amount (< 200 small ones). openblack: `HandSystem::UpdatePickupSound` with
  `audio::PlaySoundEffect` (.sad mode 2, owner 0, `audio::SetPitch`, `audio::StopSoundEffect`); traced with `OPENBLACK_HAND_TEST_FISH=1`: a single start and
  pitch 0.60 → 1.02 over the hook's 3 s. The original makes it 3D (+0x0C 0, it does not follow anyone) at the point +0xC8 of the
  interface state, which is **the hand**: `GInterface::Process` → `fn_005D2250` sends in packet 0x15 the
  hand's position (`CHand`+0x78, `Morphable::position`, 0x5D2350); `GPacket` 0x63CA9E → 0x5DBFB0 stores it at
  +0xA4 (and the camera at +0xB0/+0xBC); `GInterfaceStatus::Process` 0x5DC4E7 → `fn_005DBC60` computes the hand's velocity
  with +0xA4 − +0xC8 and copies +0xA4 to +0xC8 (0x5DBF1F) **before** `ProcessInInteract` (0x5DC574), which reaches
  `UpdateMultiPickup`. So it sounds where the hand is at the start (mode 2 does not move it afterwards) and does not start with the
  camera more than 180 from the hand. openblack: the position of the hand's `Transform`.
- The player **no longer sets AL_PITCH = 1 every frame** (`AudioPlayer::UpdateSource` did so and erased the .sad's pitch
  and that of `SetEmitterPitch`): the pitch is set when the source is created and with `SetSourcePitch`.
- **Distances** (`QSWaveMixSetDistanceMapping {min, max, scale}`, LHaudiodllR 0x10012159): .sad +0x268 / +0x26C /
  +0x270 if the flags 0x80 / 0x100 / 0x200 are set; otherwise, 1 / 9999 / 0.3 (`LH_SamplePlayOptions` 0x10010E90).
  QMixer (0x1800ACDF, 0x1802CE50; channel flags 0x103/0x111 from 0x10012065: neither 0x800 "cap at max" nor 0x1000
  "linear"; verified with Unicorn): gain 1 up to min (or with scale 0), `min / (min + scale·(d − min))` up to max
  (min/d with scale 1), **0 beyond max** (the channel keeps playing, muted). The scale is per channel because LHaudio does not
  use the hardware mixer (option `UseHardware` of `HKCU\Software\Lionhead Studios Ltd\Audio\Override`, which does not
  exist; with it it would call `QSWaveMixSetListenerRolloff(4)`). In openblack: `AL_INVERSE_DISTANCE_CLAMPED` with reference = min and rolloff =
  scale (`AudioPlayer::SetSourceDistance`), and the channel beyond max is muted in `AlSampleOutput` (the 16
  channels of `audio::sample_play`; there are no `AudioEmitter` emitters any more). `Sound` also stores `cloneGroup` (+0x118), `playMode` (+0x274 with 0x400, otherwise 3),
  `atmosGroup` (+0x11A) and `atmosFrequency` (i32 at +0x27C, −1 = not an atmosphere one; checked with `offsetof`).
- `GAudio::PlaySoundEffect` 0x429E30 with a position: **does not start** if the camera is farther than the .sad's max
  (raw +0x26C, `LHSampleGetMaxDistance` 0x10014170; the mapping's max if it is 0) → `audio::PlaySoundEffect` (`Audio.h`).
  In addition (0x429F36..0x429FD9, and the same in `SamplePlayAnimEffect` 0x42A4B0): with the widescreen (panoramic) view set by a script
  (+0x45E8 and +0x45EC) no sample with user param 1 plays (InGame has 60, editor 82, e.g. `G_WaterFlow`);
  inside the citadel (`g_game+0x205A28 == 1`) only those with user param 2; after `SET_GAME_SOUND false` (GScript+0x90,
  0x7100B0, which also does `LHSampleStopAll`) only the dialogue banks HelpSprites and Villagers. The collisions of
  physics go through `SamplePlayAnimEffect` → `LHSamplePlayAnimEffect` 0x100146F0: they do not start beyond the .sad's max nor beyond
  800 (0x426E6B) and the channel's owner is the object. All in `Audio/SamplePlay` (`PlaySoundEffect`, `PlayAnimEffect`).
- **Channels and modes** (`LHSamplePlay` 0x100113B0: allocation 0x10011020, start 0x10011420; `Audio/SamplePlay`): 16
  channels (+0xCC, constructor 0x1001535E). Each channel remembers bank, owner (+0x20 of the options: 0 none, −1 the
  ambience's, or the object), sample, clone group (.sad +0x118) and priority (.sad +0x240). Mode 1: a free channel.
  Mode 2: **nothing** (not even a new position) if a channel of the same bank and owner plays the same sample or one of the same group
  (> 0); otherwise, a free one. Mode 3: the channel of the same bank, owner and sample (it restarts), otherwise that of the same group
  (> 0), otherwise a free one. With no free one: the one with the lowest priority if it is lower than the sample's (it restarts); otherwise, it does not play.
  `G_BigSplash` (mode 2) does not play again while the same sample of the same object is playing; the ten `G_HandInWater`
  (group 4, mode 3, no owner) restart one another; the ambience one-shots (mode 3, owner −1) restart their
  channel if they come up again while playing. Only the samples that go through `sample_play` count towards the 16: the
  physics hits, the hand's water and dust, the pick-up loop, the ambience, the `SoundTags` (waterfall) and those
  of the boat. Still on their own: `AnimationSounds`, the rocks, the camera whistle,
  `G_RockPast` and the hand's pick-up/plant/break `PlaySample` calls.
- **Tracking and listener, once per turn** (`fn_004270D0` from `ProcessAudioGameTurn`): `LHSampleUpdate3DChannels`
  0x10014310 moves each 3D channel with +0x0C (1 by default) and without AtmosInfo to its owner's position
  (`Get3DSoundPos`, game function 0x427200): without an owner, **the camera**; owner −1 or no longer present, it stops; beyond
  its max (+0x6C) from the camera, it stops. The collisions set +0x0C = (code A of the table ≠ 0x16, 0x64689F); the hand in
  the water and the pick-up one, 0. Then `LHListenerUpdate`: the QMixer listener = the camera (position, forward and up,
  velocity 0) **only once per turn** (and not while paused nor in the first 5 turns); openblack the same
  (`AudioManager::UpdateListener`).
- The .sad files are RIFF wavs (`QSWaveMixOpenWaveEx`); openblack used to try MPEG first and dr_mp3 found frames inside
  some of them (`G_BigSplash_03` lasted a fraction of a second): now a RIFF goes straight to the wav reader.
- **Listener axes**: openblack is left-handed and OpenAL right-handed; positions and velocities already went with x ↔ z, the
  orientation (at, up) did not, and left and right came out reversed. Now it goes through the same swap.
- **Hand in the water / grabbing land** (`StartLandscapeGrip` fn_005D1AB0): a single branch chosen by the **water bit
  of the cell** (`InBounds && IsLand` 0x5D1F94; outside the map = water), not by the height. Water: ring,
  `G_HandInWater_01..10` = InGame 99 + counter (0xD18228, 0..9, advances even if discarded), **3D at (x; 0.2; z)**,
  vol 127, pitch 100 ±5 %, min 40 / max 150 / scale 4, and the scare of the fish. Land: dust and
  `G_HandGrabLand_01..06` = InGame 4 + LocalRand(6) (0x5D1FC4, `SoundTag` without an object, 2D: is3D 0, vol 10, pitch
  60 ±15 %). The ten water ones are in **clone group 4** and are played in mode 3 without an object: `LHSamplePlay`
  (0x10011146..0x100111BC) restarts the previous one's channel → **one at a time** (`sample_play`, HandFish.cpp).
  Trace: `OPENBLACK_AUDIO_TRACE=1`.
  - Conditions (resolved): the land branch (0x5D1FA8) makes no dust nor sound if `g_game+0x25005C` (the
    **HelpSystem**) has the **widescreen** set (+0x45E8) **by a script** (+0x45EC = the script's task number,
    `GScript::SetWideScreen` 0x6F7BF0 → `HelpSystem::SetWideScreen` 0x5C6AD0; the videos and the playback
    pass 0); the water one (0x5D1FF0) does nothing with the **game paused** (`g_game+0x14` bit 4, toggled by
    `PauseGame` 0x54AE20) nor if the hand's 3D object (`CHand+0x482C`) draws something held (+0x8C, set by its
    `SetHeldG3D` vt+0x234 = 0x816830; empty also with a `SpellSeed` that is not drawn in the hand). In openblack:
    `ScreenFade::IsWideScreenOn` (only `SET_WIDESCREEN`), `Game::IsPaused` and `!_held` (`HandPlacement.cpp`).
- **Ambience (atmos)** (`Audio/SoundMap`, `Audio/AtmosBanks`):
  - Cell zone: `Terrain::GetAtmosType` 0x7352B0 = `(flags >> 2) & 0xF` (bits 2..5 of byte 7; **1 = SEA** outside
    the map or without a block). Table 0x9CB048 of 14 types {name, bank, daytime}: 1 SEA `ocean.sad`, 2 STILL_FRESH_WATER
    `lake.sad`, 3 COASTAL `shore.sad`, 4 JUNGLE*, 5 ARCTIC, 6 DESERT*, 7 COUNTRYSIDE*, 8 SWAMP*, 9 RUNNING_WATER
    `stream.sad` (no base map uses it), 10 STRATOSPHERE `high.sad`, 11 NIGHT* `night.sad`, 12 RAIN, 13 WIND
    (* = daytime). The codes of Daniels118's editor are `tipo << 1` (bit 1 is the water cell that is not drawn).
  - `GSoundMap::Update` 0x71D6F0, every turn from `GGame::EndTurn` (before `ProcessSoundTags`): receiver = camera;
    11 × 11 cells around (radius 50); per type, number of cells and the nearest corner. Volume = radial (1 up to
    20, 0 at 50) × fade by the camera's height above the ground at that corner (1 below 120, 0 at 250); the daytime ones
    also × weatherFade × (1 − night), night = max(0, sky type − 1). The coast is computed without that and the **sea =
    min(radial, 1 − coast)**: within 20 of a COASTAL cell the sea goes quiet (the point `1464, 2016` of Land1 gives SEA 0,
    COASTAL 1; `1300, 2016` gives SEA 1). STRATOSPHERE with the absolute height: 0 below 200, rises up to 1500, falls up to
    44444. NIGHT = fade(camera) × (1 − fraction of sea cells) × weatherFade × night. RAIN = rain/70, WIND =
    (|wind| − 15)/30. `CameraWeather()` reads the weather with `ecs::weather::atmos::GetWeatherSmooth(cámara, true)`
    (`LH3DAtmos::GetWeatherSmooth` 0x835180; byte 3 is the cloudiness).
  - `GAudio`: targets = the map's volumes, **all 0 inside the citadel**: the symbol
    `HelpSystem::GetWideScreenControl` 0x4282F0 is misnamed, it is `g_game+0x205A28 == 1` (`GoInsideCitadel` 0x554004,
    2 during the falling spell video); the widescreen does not touch the ambience (without a citadel interior: never). The
    `fn_00429100` copies 15 floats, so the 15th (the camera's x) falls into `current[0]` (NONE, without a bank: only visible
    in the trace). `ProcessAtmosBanks` 0x428FE0: step 0.04 between 0.1 and 0.8 and 0.02 outside (0 → 1 in 33 turns, 3.3 s),
    group 1 if GAudio+0x190 > −0.6 (double at 0x8C4A08) otherwise 2, `LHAtmosSetBankVolume(trunc(cur·127))`.
    GAudio+0x190 is set by `fn_005E2240(a)` = 2a − 1 with a = clamp(a, 0, 1), from `fn_0064AC30` in
    `GPlayer::ProcessPlayers` every turn: a = (alignment of the **player with the most influence at the camera's
    position**, `MapCoords::CalculateMostInfluentialPlayer` + `GPlayer::GetAlignmentValue`, + 1) / 2; `GAudio::Reset` (on
    clearing the map) leaves it at 0. openblack: `atmos_banks::Alignment()` reads `GameQueries::cameraAlignment`, which
    `ecs::audio_queries` takes from `ecs::effects::alignment::GetInterfaceAlignment()` (fn_0064AC30, once
    per turn) with the formula of fn_005E2240; the same x as the target of the sky alignment, so the
    sky test hook and the debug slider also reach it (see [audio.md](audio.md)).
    Only from turn 6 and not paused (`g_game+0x14 & 4`); otherwise, `AtmosProcess(0)`: the loops stop and **only the
    channels with AtmosInfo** (0x10001EBF: loops = 1, one-shots = their entry), not the other samples. With a video
    (`g_game+0x250188`) `LHAtmosProcess(1)` does not run (openblack has no in-game videos). On changing map
    (`GAudio::Reset`) ambience and samples stop, but the bank volumes are kept.
  - The DLL's mixer (`fn_10001610`, `LHAtmosProcess` 0x100018B0): samples with +0x27C = 0 are 2D **loops** (vol of the
    .sad or 127, fade-in +5 per turn without a cap until reaching its volume, gain bank·fade/127; they are cut
    abruptly when the bank reaches 0); +0x27C = f > 0 are **one-shots** in **a single queue** for all the banks,
    next = counter + 4f + rand·12f/32767, and when registering a bank counter = head − 20. **At most one per
    turn** (the head, if due): it plays if its bank is not at 0, relative to the listener in QMixer (x, y, 0) with x, y =
    2 − rand·4/32767; if |x| + |y| ≤ 1 they are multiplied by 4 and if they end up at (0, 0) they go to 5·(a, b), with a, b = ±1 that
    rotate (a' = −a, b' = −a·b). It is put back in the queue even if it has not played. The one-shots of a group other than
    their bank's go down 5 per turn. In Land1 there are 15 loops and 400 one-shots.
  - **Axes of the relative mode** (verified with Unicorn): LHaudio converts the relative position (+0x14) to
    polar (0x10012269): azimuth = atan2(x, y) in degrees (with π taken as 1/0.318471 and |ftol| of the negatives),
    elevation = atan(z/|xy|), range |xyz|; QMixer (0x1800AA85) goes back to right = r·cos(el)·sin(az), up =
    r·sin(el), forward = r·cos(el)·cos(az). That is, **x right, y forward, z up**: the one-shots (x, y, 0) lie in the
    horizontal plane around the listener, at 2..7.1 (e.g. (2, 2) ahead to the right, (0, −4) behind), attenuated with
    their mapping (min 1, scale 2 or 4: at (2, 2) with scale 2, 0.215). In openblack: `sample_play::PolarRelative` and the relative
    emitter in OpenAL's listener space (right, up, −forward). Traces: `OPENBLACK_AUDIO_TRACE=1` (channels)
    and `OPENBLACK_ATMOS_TRACE=<n>`. Checked in Land1 (camera at 2120, 40, 2400): the lake waves (lake.sad, pitch
    160 of the .sad) play at AL_PITCH 1.600 at (−1, 1) and (1, 2).
- All the world's sounds (picking up, uprooting, planting, crushing, rocks, whistles, piles) go through the 16
  channels as in the original, 2D or 3D according to their place
  ([audio.md](audio.md#the-worlds-callers-on-the-channels)).

## Moved to other pages

- Trees: rules, fire and sacrifice → [trees.md](trees.md) ([tug](trees.md#tug-handstatetug-enter-0x5b7df0--update-0x5b8070-in-handtreescpp),
  [pick-up rules](trees.md#pick-up-rules-and-bigforest), [fire](trees.md#fire), [sacrifice](trees.md#sacrifice)).
- Creation from CHL → [map-loading.md](map-loading.md#creation-from-chl-create-27--create_with_angle_and_scale-252).
- Map mist (CREATE_MIST) → [map-loading.md](map-loading.md#map-mist-create_mist).
- Animals and herds → [map-loading.md](map-loading.md#animals-and-flocks-create_flock-create_new_animal).
- Map simulation data → [map-loading.md](map-loading.md#map-simulation-data-data-only-nothing-is-drawn).
- Fish farms → [map-loading.md](map-loading.md#fish-farms-create_fish_farm--create_town_fish_farm).
- The missionaries' boat (`PetitNavire`) → [water.md](water.md#the-missionaries-boat-petitnavire).
- Construction percentage of a Feature →
  [map-loading.md](map-loading.md#build-percentage-of-a-feature-built_percentage-chl-property-22).
- Fish puzzle: the script side → [water.md](water.md#fish-puzzle).
- Map script objects →
  [map-loading.md](map-loading.md#map-script-objects-street-lanterns-bonfires-dead-trees-gates).

## Addresses of the original, moved out of the code

The code's comments describe what it does in plain words; the original's addresses and function names they used to quote are kept here, next to the openblack symbol each one corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `GetSizeEBone 0x4038E0` | the original's size of an extra-bone block of an L3D file; the footprint block keeps its size at +8 | `l3d::L3DFile (extra bones block)` |
| `GetSize*Data 0x403730..0x403A30` | the original's per-block size readers that Abode::GetNewEp sums to reach the entrance points | `l3d::L3DFile (additional data blocks)` |
| `jle 0x40367F` | in Abode::GetNewEp, the branch that reads no entrance point when the count is 0 or less | `l3d::L3DFile (NewEP block)` |
| `LH3DStaticObject::GetChimneyPos 0x7F9F10` | reads the L3D extra point [1], the chimney | `l3d::L3DMeshFlags::HasChimney` |
| `fn_0081FFF0` | the animal ground blobs, which use the first 2 or 4 EBone points | `l3d::L3DFile (EBone block)` |
| `Abode::GetNewEp 0x403590` | reads a building's entrance points from the NewEP block | `l3d::L3DFile (NewEP block)` |
| `Terrain::GetAtmosType 0x7352B0` | the cell's ATMOS_TYPE, (flags >> 2) & 0xF | `lnd::LNDCell` |

## Pending

- Taking in batches: the fish particles (`S_Spangle_A`).
- `PileFood::Draw`: confirm that LH3DObject's vfunc 0xE8 is the UV offset (**inferred**).
- Fields: the alignment and the rain in the growth; the villager jobs that sow them (without them the fields
  stay empty).
- The status's +0x128 (who owned what the hand picked up): openblack's record starts empty with the hand system and
  is not cleared again when a land is set up (`GPlayer::Init` → `GInterface::Init` → `GInterfaceStatus::SetToZero`);
  whether a land change reaches that path is not traced. A town's player can be NULL in the original (`Container`
  +0x2C); openblack's `Town::owner` is always a player (the neutral one when none), so a farm or pit of such a town
  records the neutral player instead of NULL; whether a land makes such a town is not checked.

## Test hooks

- `OPENBLACK_HAND_TEST_FIELD=1`: taking from a field.
- `OPENBLACK_HAND_TEST_FISH=1`: taking from a fish farm; traces the pick-up loop (one start, pitch 0.60 → 1.02).
- `OPENBLACK_AUDIO_TRACE=1`: `sample_play` channels (hand in the water, collisions, ambience).
- `OPENBLACK_ATMOS_TRACE=<n>`: the ambience (sound map and banks).
- Trees and map loading: the hooks of [trees.md](trees.md#test-hooks) and
  [map-loading.md](map-loading.md#test-hooks).

## Sources

- Disassembly of `runblack.exe`, `LHaudiodllR` and QMixer; the QMixer volume, distances and axes were checked by
  running the original code under an x86 emulator (Unicorn).
