# Animals: AI, states and clips

All the animal AI ported from runblack.exe: herbivores, predators and hunting, birds, fleeing and the other reactions,
flocks, age, smoke, the hand, flight and death, the detour, the lairs and the script animals. bw1-decomp only has empty
stubs of Animal*.cpp: everything comes from the executable.

In openblack: `ECS/AnimalAI.*` (turn, herbivores, hand, death, movement states), `ECS/AnimalPredators.cpp`
(predators and hunting), `ECS/AnimalLairs.cpp` (lairs), `ECS/AnimalBirds.cpp` (birds), `ECS/AnimalFlee.cpp` (the
reactions), `ECS/AnimalAIDetail.h` (what
they share), `ECS/AnimalAnimations.*` (clip per state and species), `ECS/AnimalApi.cpp` (functions for spells and
scripts, in `AnimalAI.h`), `ECS/AnimalDebugHooks.cpp` (test hooks), `ECS/DisappearSmoke.*` (was `ECS/SmokyStuff.*`; smoke from the carcass),
`components::AnimalBrain` (the Living / MobileWallHug / Animal fields that the AI uses), `components::Flock`,
`ECS/ScriptHeld.*` + `components::ScriptHeld` / `CannotBeEaten` (what the scripts hold and their flags) and
`ECS/AnimalScript.cpp` (the animal that a script releases).

> **Code rules.** Animal state lives in ECS components (`components::AnimalBrain`, `components::Flock`) and the
> systems are reached through Locator services, never globals; the AI's formulas stay pure functions, unit tested with
> fakes in `test/`; comments describe behaviour in plain English, with no decompiled names or addresses (those
> belong here). See [openblack-internals.md](openblack-internals.md).

**Land1 sheep:** `Land1.txt` creates none. The 9 sheep are created by the challenge "The Lost Flock" (`challenge.chl`,
script `TheLostFlock`), which `LandControl1` launches only after the `FollowUs` and `CitadelGuide` intro; in openblack the intro
does not finish yet, so they do not appear. Land2 does create them in the map.

## Turn (`Animal::ProcessState` 0x417EE0)

Every turn, after the villagers: TurnsSinceStateChange + 1; if the state has the needs flag
(info.dat `animalStateTable.field0xa4`) `ProcessNeeds` (hunger, sleep, breeding +1 up to their info.dat maximum; the leader
counts `leaderTurns`); then the state's function (`g_AnimalStateTable` 0xD12108). There are no entry / exit clips
nor speed changes per state (`Animal::SetStateSpeed` empty). `SetTopState`: exit filter (in the hand only
FLYING, LANDED or death; flying only IN_HAND, LANDED or death), state, counter to 0 and the state's clip.
`PlayAnimThenSetState`: WAIT_FOR_ANIMATION with the clip unchanged until turns × 100 ms ≥ its duration.

- `Animal::SetStateSpeed` is at 0x41A2B0 (empty). `Animal::GetFinalState` 0x41A240: the top state if the table marks it final, otherwise the destination.
- The exit filters of the hand and of flight are `Living::ExitInHand` 0x5ED500 (only FLYING, LANDED or dying) and `Living::ExitInFlying` 0x5ED540 (only IN_HAND, LANDED or dying).
- `Animal::ProcessNeeds` 0x417DC0. `Animal::CheckNeeds` 0x418450 returns 3 ready to breed (otherwise the breed counter restarts when the flock is full), 1 hungry, 2 sleepy, 0 none.
- At the start of a turn a target that is no longer available is dropped (fn_00417E90); a young one grows four times a year, every 1500 / 4 turns (fn_004179F0).
- `Animal::InteractDecideWhatToDo` 0x417D80 (vt +0xB88): with a flock (+0xB8) `LookForFlocksInSpiral(Pos, info, 2 × domainRadius (+0x25C), merge 1)` 0x41A690 (0x417D83..0x417DA2), then `StartWander` (vt +0xB48, 0x417DAB): the Dove class has its own, `Animal::StartWander` 0x417C90 serves the grazers, the hunters and the SpellWolf.
- `Living::GetNumTurnsToDieOver` 0x5EC3E0 = 600 (the corpse time).
- `Animal::Animal` 0x416EB0 with `Living::Living` 0x5EBEC0: counters 0, speedDefault, DECIDE_WHAT_TO_DO, the info.dat life; the sleep place is the flock's domain-centre cell at creation (fn_005E18E0).
- The initial heading comes from `MobileWallHug::SetToZero` 0x60F760: game angle 0, facing +x; nothing at creation sets another.

## Herbivores (sheep, tortoise, cow, horse, pig)

A single class (constructor 0x41D0B0, vtable "Cow"); only the clips change. Cycle:

- **DECIDE_WHAT_TO_DO** (`Cow::DecideWhatToDo` 0x41D1B0): breed if it is time; the leader (first member of the flock) takes
  the herd to a random point of the domain every `stayTime` turns (cow 200, the rest 1000) or if it left the domain; a
  member more than `flockDistance` from the leader goes back to its side; otherwise, the needs; otherwise, START_WANDER.
- **WANDER** (`Cow::Wander` 0x41D280): in a straight line at `step` per turn and it only changes heading on entering another
  10 m cell (no wall-hug nor water). The new heading (`SetNewWander` 0x41A3F0) adds, with a speed budget
  along the major axis (`fn_0041A5B0`), 0.9 × speed towards the leader if it is far (the distance in whole metres,
`__ftol` 0x41A421, compared as int with the int radii), the flock (`fn_0041AD70`: 1/5 towards the
  centre of the others, the nearest neighbour by axes, the cohesion vector **again**, 3/5 of the neighbour's step; the
  distance is compared with raw MapCoords, so it almost always attracts) and a random turn of ±turnAngle/2.
- **Hunger** (50 turns, horse 100): `LookForGrazePos` walks in a spiral (domain/10)² cells from its own, in front
  (±viewAngle/2 = ±90°), neither its own nor that of another member (or its destination), without water nor a fixed object;
  MOVE_TO_POS there → **START_TO_EAT** (head-lowering clip once) → **EAT** 20..34 times (GameRand(15) + 20), each time one of
  the two eating clips at random → **FINISH_EATING** (raise the head) → DECIDE.
- **Sleep**: a pure counter, without day / night. Full (1000) → SEEK_SLEEP to the sleeping spot (the cell at the centre
  of the domain at birth) → SLEEPS standing (−2 per turn, +1 from ProcessNeeds: ~100 s).
- **Breeding** (3000 turns): only if the flock has fewer members than it had (`maxMembers`): GIVES_BIRTH creates one
  of age 1 in the same flock.
- Speed: always `speedDefault` (cow, sheep, pig 0.75 m/s; horse 1.5; tortoise 0.25), so always the walking
  clip. Turning in MOVE_TO_POS: `Animal::SetTowardsAngle` 0x418560, at most turnAngle per turn (cow 34 = 6°);
  with the destination inside its turning circle (R = 2 × speed / turnAngle in radians) it turns |diff| − turnAngle × d / R,
  almost fully when close (0x4186B4..0x4186FC), so it does not keep circling the destination.
- `Living::KeepLeaderWithinDomain` 0x41AAD0: the leader takes the herd to a new place every stayTime turns.
- `Cow::ReactToAnimalNeeds` 0x41D310: breeding, then hunger, then sleep.
- The wander's straight move is fn_0060BD00. In `SetNewWander` the random turn is `GameRand(turn)` (0x41A509; 0 for 0, without a draw); the heading follows the step at once (fn_0060C000).
- The speed budget's scale is fn_0041A590 = v × num / den, truncated.
- The flock steering's neighbour distance is fn_0074D090 [inferred |dx| + |dz|].
- `SetTowardsAngle`: the side and the size of the turn come from `GetAngleSign` 0x74D6F0 and `GetAngleDifference` 0x74D740 (0x4185E9).
- `Cow::LookForFoodPos` 0x41D440 → `Animal::LookForGrazePos` 0x41A8B0 / `FindGrazingPosition` 0x41A980; fn_00419980 refuses a cell where another member of its flock stands or is going. The spiral is `GUtils::Spiral` over a copy of its own MapCoords (0x41A8C5..0x41A8D5): PosWithinDomain 0x41A906, InBounds 0x41A913, FindGrazingPosition 0x41A924, then += 0x41A945 (whole cells on the high words).
- `Animal::MoveToPos` 0x41BAF0 → `Living::MoveToPos` 0x5EC270 (`MobileWallHug::MoveTo`; arrived (0xA) → SetTopStateToFinal 0x5ECA80).
- Eating: `Animal::StartToEat` 0x418280 gives 15..24 eat clips (GameRand(10) + 15, 0x41828F); the grazers' `Cow::StartToEat` 0x41D4A0 then makes it 20..34 (GameRand(15) + 20, 0x41D4B4). `Animal::Eat` 0x4182D0.
- Sleep: `Animal::SeekSleep` 0x4180D0 goes to a spot in a square around its sleep cell, 2 m per flock member (fn_0074F310); `Animal::Sleeps` 0x418330.
- Breeding: `Animal::GivesBirth` 0x418230.
- Landing: `Animal::Landed` 0x417D50 (CalculeLairPos, then the flock centres where it landed; the predators: their lair).

## Predators (lion, tiger, leopard, wolf)

Tiger and leopard use the lion's code (the tiger puts its lair in a forest); the wolf has its own. They wander, sleep,
breed and land with the herbivores' functions.

- **Felines** (`Lion::DecideWhatToDo` 0x41FE70): breed; after 22:00 (visual time) to sleep with the counter
  full; the leader moves the herd (stayTime lion 200); otherwise, hunger (lion 12000 turns, tiger 9000, leopard 1200),
  then sleep; otherwise, wander at 1.5 m/s.
- **Lairs** (`ECS/AnimalLairs.cpp`). The forest list (g_game +0x205BB4) goes from the
  newest to the oldest; the first is taken without scoring and the others score with `fn_0053AD00` =
  `SigmoidThreshold(-0.9, -(MapCoords distance / 1000))` (the sigmoid of the number of trees is computed and discarded),
  changing only if they score strictly more. Every forest more than 1000 MapCoords (0.15 m) away scores 0.1144, so in
  practice **the lair is the second newest forest** (with only one, that one), at its grown tree nearest to the centre.
  - Tiger (`Tiger::CalculeLairPos` 0x421470): that rule; without forests or without a grown tree, where it is. Land5: the tiger at
    (2841, 2918), next to forest 19, puts its lair in forest 18 (1358, 3577), 1600 m away.
  - Wolf (`Wolf::CalculeLairPos` 0x421730): the nearest big forest; if there is none, the tiger's rule; if the chosen
    forest has no grown tree or there are no forests, the nearest tree (all of them); otherwise, where it is.
  - Lion and leopard (`Lion::CalculeLairPos` 0x420010): where it is. Land2 has no forests: the tiger stays where it is.
- **Wolves** (0x4216B0): to the lair (a square the size of the pack around the centre of the domain, see Lairs)
  and lying in it (HIDE_IN_LAIR, sleeping clip). When hungry (120 turns) they only
  go out after 23:00: towards the nearest flock that is not stronger at 1.2 m (in practice their own: the leader hunts
  where it is) or towards the nearest village.
- **Prey** (`fn_004196D0`): the first in a spiral of 64 cells (±40 m): another species, on the ground, alive, with meat
  (all except the tortoise), outside their turning circles; a cub only a downed prey. It is chased from the
  next check, at 8 m/s. **Villagers are also prey** (see [Hunted villagers](#hunted-villagers)). What cannot be eaten (+0x25 & 0x40) is never prey, nor what is
  controlled by a script (+0x24 & 0x400) except for a hunter that is in a script (see Scripts and flags).
- **Chase** (HUNTING_MOVE_TO_POS 41, 0x418DB0): stalks at 1.25 m/s (stalking clip) from 100 to 50 m, sprints at
  10 m/s below 50 m; pounces when it is at pounce stride × scale × 0.5 (~2.3 m) and the prey is within ±22.5° of
  its heading; gives up after 20 s (chaseTime) or beyond 100 m.
- **Pounce** (TARGET_POUNCE 40, 0x419010; clip POUNCE_HI, the wolf POUNCE): at 1 m or less the prey falls (life 0.05,
  DOWNED); when the pounce stride ends: if it fell, `FinishPouncing` (hunger 0, it places itself next to the prey to eat);
  otherwise, it goes back to chasing it.
- **The prey**: DOWNED (falling clip) → BEING_EATEN (lying down) 300 turns → DEAD, carcass 50 turns; the one that cannot be
  eaten (+0x25 & 0x40, 0x5EC4E0) goes to LANDED and stays alive.
- **Eating**: START_TO_EAT → EAT 15..24 cycles; if the prey disappears it goes back to deciding (`Lion::Eat` 0x41FE40; also
  at the end of the meal, so the getting-up clip is never seen).
- `Lion::ReactToAnimalFoodNeeds` 0x41FF40, used by every predator: chase the target seen last turn, otherwise look for one.
- `Animal::ReactToAnimalNeeds` 0x4183C0: hunger (no food: on to sleep), then sleep. The wolf's is `Wolf::ReactToAnimalNeeds` 0x421950.
- `IsHuntingTargetValid` (vt +0xBB4) of the Animal class, 0x418DA0, only checks that the target is not null.
- Prey search (`FindPrey`): `GUtils::Spiral` at 0x41953E over a copy of its own MapCoords (0x41949A..0x4194B6), InBounds 0x4194D6; in each cell `GetFirstIterator` 0x6034D0 (at 0x4194E9) walks the fixed list, then the mobile one (0x41951B..0x419531), each from its head (GetMapChild 0x41950D); the first object that passes fn_004196D0 (0x4194F9) wins.
- fn_004196D0 refuses all but villagers and animals (info type 2 / 4, 0x41975A); the "cannot be eaten" test is at 0x4196F4.
- `Living::IsSkeleton` (status bit 0x40) is never set for animals, so their corpses are prey too.
- fn_004196D0's "nearer than the stored candidate" compares 2 × Chebyshev(me, prey) in raw MapCoords against the Chebyshev of my cell indices and the stored raw MapCoords (fn_0074CF30): a unit mix-up, so in practice the new one is always nearer.
- `Animal::SetupMoveToTarget` 0x418D20: a downed prey is eaten, any other is chased (the state only, the clip stays); the walk is `MobileWallHug::SetupMobileMoveToObject` 0x60ACD0 (STEP_THROUGH at it), then `Living::SetState` 0x5F2A80.
- fn_005EC240 is the current move plus SetTopStateToFinal on arrival (used by the pounce).
- `Animal::HuntingMoveToPosAbandon` 0x418FD0. `Animal::FinishPouncing` 0x419120: fed, then it walks to the prey and stops one scale-metre short of it to eat.
- `Living::Downed` 0x5EC4B0 plays the dying clip, then BEING_EATEN; `Living::StateBeingEaten` 0x5EC4D0 counts the 300 turns.
- `Wolf::HideInLair` 0x4219E0 (the in-script leader's test is at 0x421A0E).
- `SpellWolf::Wander` 0x420A10. The SpellWolf's vt +0xB48 is `Animal::StartWander` 0x417C90 too: after a hunt it wanders, and its Wander runs on.
- Lairs: the forest list (g_game +0x205BB4) is linked by +0x44; the tiger's loop is at 0x4214D2 and the wolf's at 0x42181A; the chosen forest gives its first grown tree (Forest +0x48: the one nearest its centre). The wolf's nearest big forest comes from the BigForest list g_game +0x205CE4.

## Birds (crow, dove, swallow, rock dove, seagull, bat)

All of them are the Dove class (constructor 0x41DCF0); only the clips and the info.dat values change. They are born at
`altitudeNormal` above the ground (20 or 40 m), at 8-10 m/s.

- **Leader** (`Dove::DecideWhatToDo` 0x41DE40 → `StartWander` 0x41DF50 → SPECIAL_MOVE_TO_POS 44): a leg to a
  random point 80 m away (domainRadius) **from where it is** (the flock drifts across the island), at its height ± altitudeVariance
  within altitudeNormal + [altitudeMin, altitudeMax]; another leg on arrival or every stayTime (100 turns).
- **Followers** (FOLLOW_FLOCK 45): a point 10 m from the leader, then a formation slot (`fn_0041E890`, read
  literally) and MOVE_TO_POS in 2D at a constant height above the ground; start over.
- **Height** (`Animal::MoveTo3D` 0x418AA0): in flight they keep their absolute height (they do not follow the hills) and rise or
  descend at most altitudeMovementChange per turn (0.2-0.6 m), never less than 2 m from the ground.
- **Banking**: when turning they tilt ±0.5 rad over 2 s (the Zoomer, `Animal::SetTowardsAngle`) and level out again over 2 s;
  `Dove::Draw` 0x41F680 rotates the drawn matrix around its forward axis (in openblack, in `MobileDrawing`).
- **They never land** in the original game: info.sleep is 0 in all of them, so LAND_AT_POS / SLEEPS are never reached. They
  have no hunger, breeding nor day / night (the bats fly by day).
- **Clips**: the moving one (and the deciding one) is a coin toss between flapping and gliding on each state change and on each
  SetSpeed (the swallow among three, the bat always flaps); its SetAnim never restarts the clip. Table in
  `ECS/AnimalAnimations.cpp` (`BirdClip`).
- They cannot be picked up nor hit (playerCanPickUp 0) and are not prey (more than 2 m up). When dead they fall with physics at
  the flight speed (`Dove::Dying` 0x41F1B0) and stay on the ground as a carcass.
- The flock's altitude is fn_0041DF20: the flock's own, otherwise the info's altitudeNormal.
- A leg's height (`StartWander`, 0x41DFFD): (altitude + variance) − GameFloatRand(2 × variance).
- `Dove::ReactToAnimalNeeds` 0x41E600: no hunger, no breeding.
- `Dove::SpecialMoveToPos` 0x41E160: the leader re-steers every stayTime turns and starts a new leg on arrival; it ends through fn_005EC2A0 (SetTopStateToFinal, then SetState(0, flock +0x7C)).
- `Dove::FollowFlock` 0x41E0B0; fn_0041E130: once the clip has played it tosses flap or glide again, every turn until the state changes (the counter is not restarted).
- `Dove::Dying` 0x41F1B0 starts the physics with its flight velocity along its heading: s = ConvertGameAngleToScawenAngle(+0x5C) (0x41F1BB), velocity (sin(s)·v, 0, −cos(s)·v) (0x41F1D5..0x41F220, the trigonometry in extended precision, rounded once by the fmul).
- The formation slot (fn_0041E890, 0x41E98F..0x41EA05) does not use AddDistanceFromAngle: x = ftol(((cos(a) × row) × 10 + leader.x × 10 × 2^-16) × 65536 / 10), z the same with sin and column, with two roundings (fimul then fmul 10 [0x8AB470]).

## Moving (MobileWallHug)

`Living::SetupMoveToPos` (0x5F2830) uses the one-argument version of `SetupMobileMoveToPos`: **STEP_THROUGH**, a straight
walk without going around obstacles that in the animals re-aims every turn with their `SetTowardsAngle` (turn limited by
turnAngle). At the step that is one stride away it goes to FINAL_STEP and arrives (0xA) the next turn. Going around obstacles
(LINEAR → ORBIT → EXIT_CIRCLE) is only used by `SetupMoveToWithHug` (0x5F2890): going to the food (19) and fleeing again (6);
ported in `ECS/AnimalWallHug.*`:

- **Circles** (`ObjectCircleIterator`, always those of the 10 m cell of the point being swept): those of each fixed object
  in the cell with collision data (not the fields, which the iterator skips; forests have none; a tree only its 0.3 m
  trunk and only in its cell; a multi-cell object in each cell its 7.1 m circle touches). That of a building or
  feature is the one of its box (centre, the largest half-axis × scale, minimum 1 m); if the box is more than 1.4 times longer than
  wide, a row of int(length / width) + 1 circles of the short half-axis. Then, the water cells (or outside the map) of
  the cell and its 8 neighbours, each a 7.2 m circle at its centre.
- **LINEAR** (`MoveToCircleHugLinearSquareSweep` 0x60CA50): the nearest circle that the ray of the step enters
  (the real entry, dot − √disc; behind −0.2 m it does not count); TurnsToObj = distance / step length (0xFF: none or more
  than 255 turns away). It walks straight, and **only re-aims** (`InitStepsXZ`, with the species' limited turn) and sweeps again
  when changing cell. When the count runs out it starts orbiting on the side the step passes (CW / CCW).
- **ORBIT** (0x60B0E4 / 0x60B40E): turns speed / radius × 0.0497 + 1 every turn. `MoveToCircleHugCircleSquareSweep`
  (0x6159F0 CW, 0x614C40 CCW) searches along the orbit for the first intersection with another circle of the cell (or the destination if
  it is inside the circle, 0.1° earlier), TurnsToObj = the arc / (1.5 × speed), and sets the tangent heading (towards the
  centre ± 90°, minus 64 per radius beyond 0.9 radii). Less than one turn away: the destination → direct STEP_THROUGH; another
  circle → switches to it and sweeps again (3 levels). It exits (EXIT_CIRCLE, straight outwards from the centre) when it is
  closer to the destination than when it started orbiting, with the destination on the inner side and ahead; outside the circle it goes back to
  LINEAR_CW / CCW.
- Consequence of the original: since LINEAR only re-aims when changing cell and with turnAngle (6° the cow), an animal whose
  heading no longer points at the destination (or that leaves an orbit outwards) moves away and does not arrive [the code; not seen in the
  original game]; if the destination lies inside the circled circle, it arrives via the final STEP_THROUGH. In numbers: the sheep
  turns at most 64 (11°) per re-aim (`Animal::SetTowardsAngle` 0x418560; the almost full turn only inside its
  turning circle, R = 2 × 0.075 m / 0.196 = 0.77 m) and walks 0.075 m per turn, so it re-aims once every ~133 turns;
  in Land2 with a pile next to them (`OPENBLACK_TEST_FOOD_PILE`) only 2 of 25 hungry sheep get to eat, and the
  food reaction lasts 2000 turns. Checked against the code 2026-10-01 (MoveTo 0x60B095, MoveToCircleHug
  0x60D800, InitStepsXZ 0x60BFA0, SetupMobileMoveToPos 0x60ABC0, AreWeThere 0x60AD60, Living::MoveToPos 0x5EC270,
  ProcessReaction 0x5F1270: nothing else re-aims).
- The handlers of `MobileWallHug::MoveTo`: LINEAR 0x60B095, ORBIT_CW / CCW 0x60B0E4 / 0x60B40E, EXIT_CIRCLE 0x60B78F. Each returns 6 / 7 when it stepped (same / new map cell) and 1 when it only changed state.
- Constants: a water cell's circle 7.2 m (`ObjectCircleIterator::Init` 0x60D21C); a tree's trunk 0.3 m (`Tree::CreateCollideData` 0x74C622); a box more than 1.4 times longer than wide becomes a row of circles (`NewCollide::NewCollide` 0x829470); the "behind" bound −0.2 is the double at 0x930668; arc length (MapCoords) / radius (m) → game angle is 0.0497359186 = 2048 / (2π × 6553.6) (0x930660); the goal point is taken 0.1° before the goal (cos / sin at 0x930680 / 0x93067C); the orbit sweep's recursion budget is 3 (0xBF42C8 / 0xBF42CC).
- `CircleHugInfo::GetObjectPtr` 0x60A660 / `SetObjectPtr` 0x60A770; the rest of SetObjectPtr is the bookkeeping of `g_CircleHugStateInfo`.
- A hug handler that reaches FINAL_STEP (4) (0x60B232, 0x60B801..0x60B81E) sets the step to Pos − goal; it is unused, since FINAL_STEP snaps to the goal the next turn (0x60AF6C).
- The water test of the iterator is `LH3DIsland::IsWater` 0x60D3A0 (the land cell under a cell index is water, or there is none); the neighbours' test in `ObjectCircleIterator::Init(int)` 0x60D0A0 is the same hasWater bit.
- `ObjectCircleIterator` over a map cell: Init 0x60D280 / 0x60D0A0, GetMapChild 0x638560. It takes the cell's fixed list in its order (`GetFirstObjectFixed` 0x6034B0); Init skips an object without `GetCollideData` (vt +0x858, 0x60D29D) and a field by its RTTI (0x60D2B1..0x60D2C8). A multi-cell object is in the cells of its NewCollideDescriptor.
- The LINEAR sweep: the entry distance is fn_0060CEE0 (dot − √disc; behind −0.2 it is FLT_MAX), the sweep record fn_0060CF20, "nearer than the best" fn_0060CFF0; the step is normalised in metres (0x60CAB9) and turns = distance / |step| (0x60CE5E: above 255 none, not above 0 or unordered 0).
- The orbit sweep orders points along the orbit starting from the animal (g = its position − the centre, 0xD3EE70 / 0xD3EE68): `Point2DCompare::operator<` 0x610180 / 0x6101F0, `Resolve__Point2DCompare` 0x6101C0 / 0x610230 (the half the orbit reaches first), the exact cut `IntersectIntervalCircle_1::Resolve` 0x6169F0 / fn_00616C70, the interval fn_00616D00 / fn_00616A80 (hi = d, lo = d + perp(d), × the radii's ratio when the other circle is not bigger; resolved at once when the bounds come out in the wrong order), `Point2D::Cross` 0x611240.
- The end of the sweep sets the heading along the orbit (0x6166E3 / 0x61593B).
- ORBIT uses the step as it is now, which the sweeps may have re-aimed, not the one it moved by (0x60B2C5).

`GUtils::Spiral` (dir 1, count 1: (−1, 0), (0, −1), (+1, 0) × 2, (0, +1) × 2...)
in all the spirals; `Collide(1)` is only water (the `hasWater` bit of the terrain cell) or outside the map,
`Collide(0)` nothing; `CalcRandomPos` (0x5ED080): two random points, each with a spiral of 25 cells, accepting if
it is on the map, without collision, outside the turning circles and (birds) over a terrain block; otherwise, the centre if it is
outside its turning circles, and otherwise, its own position. Angles with the trunc(65536 cos) tables and `LHArcTan`.
Initial heading 0 (+x). The flock list is in reverse order of creation: the leader is the first one that entered; the
birds' formation counts from the newest. The animals are processed from the newest to the oldest.

## Reactions

`Reaction::ProcessReactions`, which would distribute them every turn, depends on a debug switch that the game never
enables, so **each reaction is offered only once**, when created (`Reaction::CreateReaction` → `SpreadReaction`
0x6E3E10: (maxReactionDistance × 0.2)² cells of `GUtils::Spiral` within the radius; `ApplyReactionToLivingObjectsAtSquare`
0x6E3F90 for each animal: available (`IsAvailableForReaction`), distance = half Manhattan distance, score
min(255, priority × (1 + 0.5 × howImportantIsDistance × (max − d) / max)), and its **records** (3 at most, {type,
turn}: it does not take a type again before NumGameTurnsBeforeReactingAgain; they expire after 1800 turns). An animal that is already
reacting only changes if the new one scores more and the current one has already lasted 10 s (1 s if it was being picked up by the hand). Each type ends
after its own number of turns (`Living::ProcessReaction` 0x5F1270), if the initiator disappears (and then it goes back to the
saved state: DECIDE, or state 0 if it came from moving, and stays there like the original) or if the
reaction disappears (it stays in its state). Leaving to a state that is not a reaction one (the hand, dying, a need) drops the
reaction without changing the state (`Animal::ExitReaction` 0x41B170). The animals react to object (0), look (1),
spell (3), creature (6), food (7), fire (10), falling tree (27, which nobody creates) and predator (28); the
predators also to flying object (9). Tortoise and birds do not react. **No animal reacts to the hand**.
In openblack the distribution, the records, the scoring and the change rule are common to all living beings
(`ECS/Effects/Reactions`, `components::ReactionRecords`); what is specific to the animals (priorities 28/7/9, StartReacting,
turns of 28, states 49/6/30/19/20) is their handler in `ECS/AnimalFlee.cpp`. The villagers receive the same
reactions in the same distribution (fire and teleport ported).

- **Predator (28):** each predator creates it at birth (fn_0041FD30). Those with `isFleeingFromPredator` flee within
  25 m, if the predator is not stalking (at its speed2 or less it goes unnoticed); a predator only from a stronger one. Fleeing:
  state 49 at its flee speed (cow, sheep, pig 4 m/s; horse 9) ~10 m to one side of its trajectory; then
  6 (again if it is coming towards it or is closer than 30 m) or 30 (still, looking at it); ~80-88 turns or beyond 75 m.
- **Food (7, `Pot::SetupReaction` 0x66D660):** the map's food piles (CREATE_POT) on load, and a pot that the
  hand puts down; it is removed when it is picked up or emptied. **Hungry** herbivores within 35 m go to the edge of the pile (the sum of
  both radii), take 50 from it (the pile shrinks; when empty it disappears) and their hunger is set to 0. The piles the hand makes
  (MagicFood) have foodType 0: no animal goes to them, not in the original either.
- **Flying object (9, `Object::InitialisePhysicsFromHand` 0x637412):** what the hand throws makes the
  predators within 25 m flee if 2 × its speed exceeds its distance; the reaction is removed when the object lands.
- Not ported (they need systems that openblack does not have): spells (0, 3), village artefacts (1), creature (6),
  fire (10).
- `Living::IsAvailableForReaction` 0x5F11F0 (vt +0x984 of every Animal class) checks these conditions in order: IsFunctional (vt +0xD4, 0x5F1202; an animal always is); not controlled by a script (0x5F120C); not in the dance editor (0x5F121E); the final state (vt +0xB04) not one of the list 0x5F1227..0x5F1248; not dying (+0xB4 & 1, 0x5F124A); not on a structure (+0x24 & 0x80, 0x5F1258, set only by `Living::MoveOnStructure`).
- Priorities: `Living::FleeFromPredatorPriority` 0x5F15D0 (at its speed2 or slower a predator goes unnoticed); `Lion::FleeFromPredatorPriority` 0x4203C0 (a predator only flees from a stronger one).
- More priorities: `ReactToFoodPriority` 0x5F1710 with `Animal::IsInterestedInFoodObject` 0x419BC0 (some food in it, not in the physics, near enough, and a hungry animal whose needsFoodTypes takes that food); `ReactToFlyingObjectPriority` 0x5F1800 (not while it flies or lands itself).
- Turn functions: `NumGameTurnsToReactToPredatorFunction` 0x5F1980 (0 unless the initiator is an animal; 0 while it looks at a predator that runs at it faster than 5 m/s), `NumGameTurnsBeforeReactingAgainToPredatorFunction` 0x5F1A00.
- "Coming towards me" is fn_005F1E60: the object's movement within acos 0.8 of the line to it.
- `Reaction::CreateReaction` 0x6E3D70 gets its player (+0x38) from the predator's GetPlayer (fn_0041FD30, 0x41FD5C), the pot's GetPlayer (`Pot::SetupReaction` 0x66D660), or the thrower's `GInterfaceStatus::GetPlayer` (0x637405).
- `Animal::SetupFleeFromPredator` 0x420410; `SetupReactToFood` 0x5F14C0.
- `StopReactingAndSetState` 0x5F11C0 → `Animal::ResetStateAfterReacting` 0x41A280: the stored state's return state (animalStateTable field0x1c).
- `Animal::GetFleeingPositionFromMovingObject` 0x420550: about 10 m sideways out of its path, on the side it stands on, ±4 m ((GameFloatRand(8) + side × distance) − 4, x first at 0x420688, then z at 0x4206B0).
- `Living::GetFleeingPositionFromStationaryObject` 0x5F2010: straight away from a still object; on it, it stays.
- The flee states: `Animal::FleeingFromPredatorReaction` 0x4201F0; `Living::FleeingFromObjectReaction` 0x5F1D10 with `FleeFromObjectIfComingTowardsMe(obj, 30, 30)` 0x5F1D90 (the 30s are states, FLEEING_AND_LOOKING; the distance is the reaction's minDistanceToRunAwayFromObject); `SetupMoveToWithHug` is called at 0x5F1E3F.
- `Living::LookingAtObjectReaction` 0x5F23A0.
- Food: `Living::GotoFoodReaction` 0x5F2550 walks to the food's working point (SetupMoveToWithHug at 0x5F259A), then ARRIVES_AT_FOOD; `Animal::ArrivesAtFoodReaction` 0x41A0A0 sets hunger 0 and does RemoveResource(FOOD, min(50, amount)).

## Hand, flight and death

- Picking up: `GAnimalInfo.playerCanPickUp` (all the land ones except those of the puzzles). When picked up it leaves its flock for
  one of its own (`SeperateLivingIntoNewFlock`) → IN_HAND. Dropping or throwing: physics → FLYING (clip THROWN).
- At rest (`Animal::EndPhysics` 0x5F0D80, `ECS/LivingPhysics.cpp`): landType by the turn-start matrix's right row
  (po+0xD8: y > 0.5 → 1, < −0.5 or NaN → 2, otherwise 0; the villager's mapping reversed; a NaN gives 2 because
  `test ah, 1` at 0x5F0DBB takes the unordered compare as "<"), heading =
  Wrap(float(GetYAngle(current fwd row po+0xBC) + pi)) into SetYAngle (+0x4C and GameAngle), drawn afterwards at the GetYXZ
  yaw of the current matrix (RemoveObject 0x646B72). The rows are the original's (`living::BodyRows`: as they are from the
  hand, OriginalRows for the world path; (inferred, not verified in game), trace
  `OPENBLACK_ANIMAL_TRACE` `landing:` lines). No body (Living 0x5EFDF8 → EndPhysics(NULL), `living::EndPhysicsWithoutBody`;
  `animal_ai::PutDown` calls it): heading unchanged, landType 3. A put-down goes IN_HAND → LANDED without FLYING; alive → LANDED (getting-up clip according to landType; the predators, the waking one) → the
  flock centres itself where it fell → INTERACT_DECIDE → off wandering. **There is no
  drowning state** in the animals (`Animal::EndPhysics` has no water branch): in the sea the animal
  never stops, its density rises and after **~75 turns (7.5 s)** it exceeds 1 and `Living::HasSunk` 0x5ED370 kills it and
  deletes it (`SetDying`, state 15, `ToBeDeleted(0)`) — alive or carcass. In a shallow cell with water of altitude ≥ 2
  it lands **alive** (unlike the villager, who drowns); see
  [water.md](water.md#sinking-drowning-and-being-deleted). Dropping gently
  puts it into physics like the original, over the sea and on land (see [Pending](#pending)).
- Death (`Living::SetDying` 0x5EC390, nothing while it is flying): DYING (falling clip) → DEAD (lying according to landType; the
  predators with the sleeping clip) 600 turns (never if a script controls it) → its smoke (`CreateSmokyStuff`, below) and it disappears. A thrown carcass
  goes back to DEAD with another 600.
- `Animal::InterfaceSetInMagicHand` is 0x419B60; `Living::InitialisePhysicsFromHand` 0x5EFD80 (every drop and throw is physics, FLYING).
- `Animal::EndPhysics` writes the landType into +0xB4 bits 4-5 (0x5F0E06..0x5F0E21) and altitude +0x1C = 0 (0x5F0E28).
- `Living::IsDead` 0x417270 also counts an animal that is not functional: `Living::IsFunctional` 0x4172D0 = IsAvailable (vt +0x2C) == 1 and GetLife (vt +0x11C) != 0.
- `Living::Dead` deletes the vanished animal in its own turn (vt +0xC ToBeDeleted(0), 0x5EC462), off the list for the livings after it (the loop has already read its next).
- `Animal::DeleteDependancys` 0x417BA0 unlinks it from its town's list (+0xE0's list +0x984, 0x417BA4..0x417BF6, −−+0x988), sets +0xE0 = 0 (0x417BF7), then leaves its flock (0x417C02..0x417C18). No prey / hunter link is cleared: the readers ask IsAvailable.
- The death reason is `Animal::GetDeathReason` 0x417890 (+0x10C).

## Clips per species (AnimalAnimation.cpp 0x41C0E0..)

| species | walk | idle | eat | lower / raise head | hand | thrown | lands lt 0/1/2 | dead lt 1/other | falls |
|---|---|---|---|---|---|---|---|---|---|
| cow | 45 (41 runs) | 42 | 36/35 | 37 / 44 | 38 | 43 | 42/40/39 | 30/29 | 31 |
| sheep | 145 (141) | 142 | 133/132 | 136 / 140 | 137 | 143 | 142/139/138 | 131/130 | 134 |
| pig | 128 (125) | 126 | 117/116 | 120 / 124 | 121 | 127 | 126/123/122 | 115/114 | 118 |
| horse | 61 / trot 59 / 56 | 57 | 49/48 | 52 / 60 | 53 | 58 | 57/55/54 | 47/46 | 46 |
| tortoise | 172 | 171 for everything | | | | | | | |

Thresholds: `speedThreshold` entry 2 (cow, sheep and pig) and 3 (horse). Predators (lion, tiger, leopard, wolf):
stalking below speedDefault, walking, running above entry 6..9; in the table of `ECS/AnimalAnimations.cpp`. The clip
advances with the terrain covered while it moves (`Object::IsMoving`) and with time otherwise.

- `Animal::GetAnimId` 0x417FA0 always asks the species' function of the state (g_AnimalStateTable slot 0x60), with no info.dat fallback; −1 keeps the clip playing (`Animal::StandardAnimation` 0x4177D0). Animals have no into / out-of clips.
- The species' clip functions span 0x41C0E0..0x41CF10: `Cow::MoveAnimation` 0x41C720 (run above its walk speed; sheep and pig use the cow's threshold, entry 2), `Horse::MoveAnimation` 0x41CA70 (entry 3: walk, trot, run), `Cow::EatAnimation` 0x41C740 (GameRand(2) at 0x41C74C picks one of the two), `Cow::LandedAnimation` 0x41C760, `Cow::DeadAnimation` 0x41C790 ((status & 0x30) == 0x10 ? DEAD_ON_RHS : DEAD_ON_LHS).
- The birds' slots span 0x41BD20..0x41C080. Decide is `Animal::DecideAnimation` 0x41BD20, the Move function. The coin is GameRand(2) == 0 ? first : second (Dove 0x41BD39, Crow 0x41BEC9, Pigeon / Seagull 0x41BF39 / 0x41BFA9); the swallow's three-way draw is GameRand(3) at 0x41C019.
- The birds' Stand, Dead, Eat, Sleep and Thrown slots are fixed ids: the odd ones (e.g. the crow's stand = TAKEOFF) are what the code returns. Every other slot is −1.
- The birds' SetAnim (the one that never restarts the clip) is fn_0041E7E0.
- The clip's advance is `Animal::Draw` 0x51C310 → fn_0051AF00 (by the ground covered while `Object::IsMoving`, otherwise by time).

## Flocks, age and smoke

- **Picking up an animal** (`Flock::SeperateLivingIntoNewFlock` 0x52FE10): it always goes to a flock of its own where it was
  picked up, with the radius and distance of the old one, without a village and maximum 0; the old one, if it is left empty, is deleted, except
  a script flock (+0x25 & 4: it releases the animal's reference and stays even if empty, 0x419B75).
- **Merging** (`Animal::LookForFlocksInSpiral` 0x41A690): only after landing (±80 m), because `flocksCanMerge` is 0 in
  all of them; the biggest one keeps all of them if they are of the same species and the sum does not exceed `maxFlockSize`; a flock
  with a village does not merge; two script flocks do not merge and a foreign script one always keeps all of them
  (0x41A837, fn_005302A0 0x5302B4). Oddity: the maximum of the one that stays adds the other's once per member, clamped.
- **Age** (`Living::GetAge` 0x5ECAF0): 1500 turns per year since its birth turn. The young grow every 375
  turns (`scale += random(0.75 × (ageToScale[age + 1] − scale))`) until `grownUpAge`; an adult is born at 0.9 and two
  rolls leave it in (0.95, 1.05]. **Animals do not die of old age**: checked across the whole executable:
  `oldAge` / `retirementAge` are only read by `Villager::CheckDeathFromOldAge`
  (0x760CA0) and the death reason OLD_AGE (9) is only for villagers.
- **Being born** (GIVES_BIRTH): the newborn decides immediately, before the mother goes back to wandering.
- **Animal thrown into a food store:** it becomes food (its foodValue: cow and horse 1200, sheep 800,
  lion and tiger 900, wolf 700, pig 290) and disappears (`Animal::ReactToPhysicsImpact` 0x41BC10): the hit
  (`GetGameObjectWhoHitMe`) and the animal available, and the hit `IsResourceStore(FOOD)` (a storage pit, a worship
  site) → the hit's `DeleteObjectAndTakeResource(this, po +0x24)`: the thrower's interface, so the pit's town gets the
  desire, alignment, belief and the creature deed, the Supply help, reaction 22, the ghost; when it took, nothing
  more, else `Living`'s damage. The food is `ftol(GetFoodValue(3))` 0x41BC80 = `(GetFoodType() & 3) ? foodValue : 0`
  (0x4026D0): an animal whose foodType has neither bit is still taken and deleted, worth nothing. Given from the
  hand on the press: valid only on a **StoragePit** (RTDynamicCast, 0x41B320; else `Living`'s totem test, the
  sacrifice, not ported); apply 0x41B360: `IsResourceStore(FOOD)` and not INDESTRUCTIBLE (+0x24 bit 0x4000) → taken,
  3, else 0 (still held). openblack: `AnimalReactToPhysicsImpact` (`LivingPhysics.cpp`) and `ecs::held_apply` (the
  Animal rows), through `ecs::resource_stores`; the deletion is `ToBeDeleted` (out of its town's list and its
  flock, out of the physics and the cells). A worship site puts the food in its food pot (`WorshipSite::AddResource`
  0x77C5F0, [magic.md](magic.md)).
- **Smoke from the carcass** (`Object::CreateSmokyStuff` 0x63A810 → `SmokyStuff::Create` 0x823C90 mode 0, size 1): 15
  sprites of `Data\Textures\smoke.raw` with a random direction at 0.3..1 × size, grey 0x808080 with opacity life × 100,
  3 s, from 0.5 to 1.5 × size. The engine is `ecs::disappear_smoke` (`ECS/DisappearSmoke.*`, the same as the dust
  and the boat's splashes; it is drawn with the boat sprites); the animals only call `disappear_smoke::Create`.
- **In the hand** the animals have the villagers' grip (2D radius, drop 0.65). The landType is read from the body's matrix
  at the start of the turn.
- `ageToScale` is at GAnimalInfo +0x27C, read at +0x280 + 4 × age by `SetScaleForAge` 0x417A5C; index −1 is the dword before the table (`InitialiseScale` 0x417B30 at age 0): +0x278, altitudeNormal.
- `fn_00417C50(town)` (SetTown) inserts the animal at the head of the town's list (a new node, ++ +0x988, 0x417C66..0x417C85) but does not take it off the list of the town it had (only `Animal::DeleteDependancys` unlinks), so a second call can list it twice.
- Flock merging (`LookForFlocksAtPos` 0x41A790): FindTypeOnMap(4, 0) 0x41A7A7 .. FindTypeOnMap(4, obj) 0x41A876 walks the ANIMAL objects of the cell's mobile list from its head (type 4 does not count as fixed); the merge itself is fn_00530210.
- `Living::SetFlock` 0x5EE5F0: an old flock → RemoveLivingFromFlock(living, 1) (0x5EE5F3..0x5EE600), then +0xB8 = flock (0x5EE609 / 0x5EE617).
- AddMember fn_0052FA50: the old flock's +0x30 is cleared when it is this living (0x52FA55..0x52FA6B); fn_005ECFE0 takes it out of its flock, even this one, with deleteWhenEmpty 1 (0x52FA74); already in the list → 0 (0x52FA79..0x52FA8F); Living::SetFlock at 0x52FB2E.
- AddLeader (0x52FC75..0x52FDF4): if it is already the tail, nothing (0x52FC97); otherwise it is unlinked and inserted again by +0xD4 (0x52FCDA..0x52FDEE, no SetFlock); a non-member goes through AddMember (0x52FDF4).
- `Flock::RemoveLivingFromFlock` 0x52FB50(living, deleteWhenEmpty): a non-member still gets +0xB8 = 0 and returns false (0x52FB6D); otherwise it is unlinked (0x52FB93..0x52FBF7, −−count), +0xB8 = 0 (0x52FBFA) and, with deleteWhenEmpty and no member left, the flock's ToBeDeleted(0) (0x52FC00..0x52FC10).
- `SeperateLivingIntoNewFlock` 0x52FE10 calls RemoveLiving at 0x52FE1E, creates its own flock at 0x52FE41, and copies the old flock's domainRadius and flockDistance at 0x52FE4A..0x52FE57.
- `Flock::Flock(Living*)` 0x52F950 does not write +0x88 (maxMembers).
- Merge fn_00530210(keeper, other) walks the other's cursor (+0x44) from its head while it has a member (0x53021B..0x53022A); each step does keeper +0x88 += other +0x88 (0x53022C..0x530232), clamped to the maxFlockSize (info +0x260) of the animal at the keeper's cursor +0x44 (0x530238..0x530272); then AddMember(keeper, head) (0x530282 / 0x530291) empties and deletes the other.
- Merging a flock into itself (attaching a flock to itself) never empties `other` and hangs the game.
- fn_00530180(exclude) picks a random member: count 0 → null (0x53018A); r = GameRand(count − 1) when there is an exclude and more than one member, otherwise GameRand(count) (0x530190..0x5301B1, "Flock.cpp" 0x182 / 0x186); then, from the head, the first member at an index ≥ r that is not the excluded one (0x5301BE..0x5301EF).
- `Flock::FindLiving` 0x530510: from the head (the cursor +0x44, 0x53051E..0x530557), the first member the callback accepts.
- `Living::PosWithinDomain` 0x5ED010(pos, factor): GetDistanceInMetres from the domain centre (0x5ED025..0x5ED037, `Living::GetFlockPos` 0x5ECF60 = flock +0x14); outside when radius × factor < distance (0x5ED04E..0x5ED05F).
- FLOCK_ATTACH also makes its flock with the script id 0xABA52 at +0x8C (0x6EF6EA; FLOCK_CREATE at 0x6F2271).
- The Flock class spans Flock.cpp 0x52F780..0x530590.

## Hunted villagers

Villagers are prey like the animals (type 2, with meat) if they are outside their home. When downed, their health stays at 5 %
and they go to DOWNED (clip `P_ATTACKED_BY_LION`), then BEING_EATEN 300 turns (`P_DYING`) and they die (`Villager::BeingEaten`
0x76B380: l = GetLife(); SetLife(0); `VillagerDead(ANIMAL 3, its player (the owner of its town), l, 1)`, in
openblack `ecs::villager::VillagerDead`, which kills it at the end of the turn). They are driven by the animal
AI (`components::DownedVillager`). The one that cannot be eaten (+0x25 & 0x40)
gets up (LANDED) instead of dying; openblack puts it in LANDED when the 300 turns end, without waiting for the clip (approximate).

## Scripts and flags

- **Held by a script** (`ECS/ScriptHeld.*`, faithful): the original's script objects are slots of
  `ScriptManage` (511, 0xD967F8) with their reference count; in openblack the slot is the entity's `components::ScriptHeld`.
  Each LHVM variable that stores an object gives it a reference (callbacks of `LHVM::Initialise` in
  `Game.cpp`, `ADD_REFERENCE` / `REMOVE_REFERENCE`; the original ScriptLibraryR.dll calls those two natives on each POP
  of an object and when stopping a task); with the first one it becomes `IsInScript` (+0x24 & 0x200) and, if a
  script created it (`CREATE`, `CREATE_WITH_ANGLE_AND_SCALE`: `AddScriptGameThing(thing, 1)`), **controlled by the script** (+0x24
  & 0x400). After each `LookIn` (GScript::Process 0x6EB6DB → fn_0070D480) whatever no longer has references is released:
  it loses both flags and an animal goes through fn_0041AA00 (own flock if it has none; dead → SetDying, which gives the carcass another
  600 turns; alive → INTERACT_DECIDE_WHAT_TO_DO, in openblack one turn later: approximate).
- **Controlled by the script**: it is not prey except for a hunter that is in a script (HuntingMoveToPos 0x418DD7,
  fn_00419340, fn_004196D0), it does not take reactions (IsAvailableForReaction 0x5F120C), **its carcass does not expire** (Living::Dead
  0x5EC41E) and its flock is a script one (hand and merging, above).
- **Wolf in a script**: the leader puts the lair where it is (Wolf::CalculeLairPos 0x421774) and hunts from it at
  any time without looking at hunger (Wolf::HideInLair 0x421A2E).
- **Cannot be eaten** (+0x25 & 0x40, `components::CannotBeEaten`): it is set by the land-to-land vortex on everything that
  comes out of it (fn_005FE3B0 0x5FE5DD) and on the puzzle objects; it is not prey and, if it was already downed, it survives.
- Releasing (fn_0041AA00): it does nothing while +0x24 & 0x44 (in the physics or in the hand) or g_game +0x14 & 0x8000.
  It creates its own flock at 0x41AA47 (`Flock::Flock(Living*)`: radius info +0x25C, distance (int) info +0x21C). A dead
  animal released this way goes to SetDying at 0x41AA89; one that was already dying (status & 1) goes straight to DEAD.
  A living one calls vt +0xB88 `Animal::InteractDecideWhatToDo` 0x417D80 at once (0x41AAB7).

## API for other systems (`ECS/AnimalAI.h`)

- **Spells**: `CreateAnimal`, `MoveTo` (Living::SetupMoveToPos), `SetState` / `SetStateRaw` (vt+0x938),
  `Destination`, `SetFinalDestination` (SpellWolf +0x148), `Kill` / `DestroyedByEffect` (0x41B1B0), `Remove`, `SetAlpha`.
- **Death**: `AddDeathListener(fn)` → id / `RemoveDeathListener(id)`: all of them are notified at the start of Living::SetDying
  (vt+0x6A4), once per death and not while it is flying; `SetDeathCallback` is a single listener that replaces its own.
  `SetSpeciesDying(type, fn)` is a species' own SetDying and replaces the whole of Living::SetDying. It is used by
  SpellDove 0x41F5C0, SpellBat (shares SpellDove's slot) and SpellWolf 0x420CF0: a fade over
  GetNumTurnsToDieOver = 20 turns, without a carcass; the spells register it.
- **Script**: `ScriptMoveTo(e, xz)` = the Living branch of MOVE_GAME_THING (GScript 0x6F8F6C): nothing in the hand; already
  there (AreWeThere vt+0x85C) → `SetScriptState(IN_SCRIPT 4)`, otherwise `SetupMoveToPos(pos, IN_SCRIPT 4)`.
  `SetScriptState(e, s)` = GScript::SetScriptState 0x6F82E0: StorePreviousState (0x417040: its final state),
  CallExitStateFunction (0x41A2C0, without looking at its answer), SetState(0, s) (no animal script state has
  an entry function, 0x41A310), its clip (SetAnim vt+0x8FC) and the counter +0x58 to 0. "On the map" for an animal = not
  in the hand [inferred].
- **Villagers**: the eaten one dies via `ecs::villager::VillagerDead`; `villager::IsAtHome` is awaited (excluding
  from the prey those who are at home) and the list of shepherds.
- **Hand and physics**: `PlaceInHand`, `InitialisePhysics`, `EndPhysics`, `PutDown` (only for openblack's
  bodiless case, `physics::from_hand::PlaceWithoutBody`; the original ends the physics immediately, 0x5EFDF8). The
  physics and the hand reach them through `ECS/LivingPhysics` (the Animal class's physics handlers and
  `living::InterfaceSetInMagicHand`).

## Blobs and mesh of the animals

(Moved from rendering.md.)

### Blobs (done)

- In `fn_00812170`: if it is not human, with `IsHumanShadowed` (flag 0x4000000, `SetHumanShadowed(1)` in each species'
  Create; 0 while the creature holds it), and > 0.2 and a mesh with `ContainsEBone` → `fn_0081FFF0(obj, normal, ebone)`.
- EBone block (836 bytes) after the footprint ones (size at +8), UV2, name and extra metrics: `u32 size; float m[16][12];
  int32 bone[16]`. The positions of m[0..3] in their bone's space are used: P = object × bone × pos, y = ground + 0.2.
  Pair (0, 1) always, pair (2, 3) if bone[2] ≠ −1 (all the quadrupeds: 4 quads). Birds and bats have no EBone.
- **Oddity of the original**: the first quad of each pair receives V = D (it builds D + (P1 − P0)/2 but passes &D); the second
  D + (P0 − P1)/2.
- openblack: `L3DFile::GetEBone`, `L3DMesh::GetBlobPoints`, animal loop in `Renderer::DrawHumanShadows`.

### Creation: mesh and scale

- `CREATE_ANIMAL` (24, "ANNN": type, herd, village) and `CREATE_NEW_ANIMAL` (25, "ANNNN": + age) → `fn_00419D10`
  (herds and classes in [map-loading.md](map-loading.md#animals-and-flocks-create_flock-create_new_animal)). Mesh: `Object::CallVirtualFunctionsForCreation` 0x636BE0 gives the
  LH3DObject `GetDetailMesh(2, 1, 0)` (info +0x1FC + 4k: high, std, low) and the LOD is always 1: **the std one** (also
  `GetMesh`); openblack used the high one. Scale (`InitialiseScale` 0x417B20, then `SetScaleForAge` 0x417A40):
  young ones (age < grownUpAge) ageToScale[age − 1] + FloatRand(0.75·(ageToScale[age + 1] − s)), at age 0 the
  dword before the table (+0x278, altitudeNormal); adults 0.9, then t = (0.05 − FloatRand(0.1)) + 1 and, if 0.9 < t, a
  second roll. No initial angle. openblack: `animal_ai::InitialScaleForAge` / `ScaleForAge`, shared by the creation,
  the growth tick and the script's `SET_PROPERTY` Age (`animal_ai::SetAge`, Animal::SetAge 0x4179C0).
- Script properties: Speed `SetSpeedInMetres` (MobileWallHug 0x60C080 → Animal::SetSpeed 0x417FE0, the clip for the
  new speed), `GetSpeedInMetres` 0x60C070; Living::IsDead 0x417270 (`animal_ai::IsDead`); `SET_FOCUS` →
  `living::SetFocus` → `animal_ai::SetYAngle` (see [villagers.md](villagers.md)).
- Flocks (the script opcodes): the animal AI's flock code and `ECS/Flocks.h` (the scripts' flocks) share one
  convention: `members` front() is the leader (the original's tail). `LeaderOf`, `SetDomainCentre` and
  `PosWithinDomain` now call `ecs::flocks`, and `CalcRandomPos` calls `living::CalcRandomPos` with the animal's vt
  tests. Living::PosWithinDomain 0x5ED010 is **0 without a flock** (0x5ED01E); openblack's copy answered 1.
- Land1 creates 116 (doves 40, seagulls 22, swallows 14, horses 12, cows 10, pigs 7, tortoises 6, bats 5);
  openblack creates them all, with the AI and the clips of this page.
- openblack: `components::Animal`, `AnimalArchetype`.

## Test hooks

`OPENBLACK_ANIMAL_TRACE=1` (state changes, landType and every 50 turns how many there are in each state),
`OPENBLACK_TEST_VIEW_ANIMAL="n[,distance[,angle[,every]]]"`, `OPENBLACK_TEST_THROW_ANIMAL="n,turn[,vx,vy,vz]"`,
`OPENBLACK_TEST_KILL_ANIMAL="n,turn"`, `OPENBLACK_TEST_ANIMAL_SPECIES=<AnimalInfo>` (n only counts that species),
`OPENBLACK_TEST_HUNGRY=<AnimalInfo>` (that species hungry on turn 1), `OPENBLACK_TEST_SPREAD_REACTIONS=<turn>`
(the predators spread their flee reaction again), `OPENBLACK_TEST_HUNT_VILLAGER="<species>,<turn>"`,
`OPENBLACK_TEST_FOOD_PILE="<species>,<turn>"`, `OPENBLACK_TEST_FOOD_BEHIND="<species>,<turn>[,<type>[,<m>]]"` (the
first of that species, hungry, 8 m in front of the nearest building (0; not a field, which the circle iterator skips) or
feature (2) of 2..8 m radius or tree (1)
and a food pile m (6) behind it; its route every 2 turns; with the trace, each change of the detour),
`OPENBLACK_TEST_SMOKE=<n>`, `OPENBLACK_TEST_CORPSE_TURNS=<n>`, `OPENBLACK_TEST_LAIRS=<turn>` (lists the forests and
each predator leader recomputes its lair; with the trace, the chosen lair and the rule),
`OPENBLACK_TEST_SCRIPT_HELD="n,turn[,release]"` (the n-th animal is held by a script as if it had been
created by a CREATE; at `release` it loses the reference; every 25 turns its state, counter and flags),
`OPENBLACK_TEST_CANNOT_BE_EATEN=<turn>` (on that turn all the animals and villagers receive the vortex flag; with
`HUNT_VILLAGER` before the downing there is no hunt, after it the villager gets up),
`OPENBLACK_TEST_VIEW_LOCK=1` (the camera is placed every turn
next to the animal, for the birds). Land2 (`-s Land2.txt`) has lions, tigers and wolves.

## Pending

What openblack still does differently from the original, and what is left:

- The detour (`ECS/AnimalWallHug.*`): the order of the objects in a cell is that of openblack's registry, not the cell's
  list (the orbit sweep decides by bounds, so a tie or a different order can choose another intersection); the
  membership of the cell is computed (7.1 m circle; tree in its cell) instead of reading the map's list; a circle
  whose object is deleted is released [inferred]; the shared bookkeeping `g_CircleHugStateInfo` / `DoWallHuggerLookahead`
  (0x609A50, a ghost villager that simulates the path) is not ported; arcs and headings in metres, not in integer MapCoords.
  Two openblack checks that the original does not do (it reads `GetObjectPtr()` without looking): no circle at the start of the
  orbit or the sweep, only possible if its object was deleted [inferred]. `+0x76` is always set to 0 when preparing the walk
  (the original only with an entry in `g_CircleHugStateInfo`; nobody reads it before rewriting it) and the field `+0x78`
  (1 / 0x10) is not carried [inferred: unidentified, it is not read on the animals' path].
  openblack's port for villagers (`PathfindingSystem`) has its own bugs.
- Lairs (`ECS/AnimalLairs.cpp`): `GUtils::GetDistance` is ported as is (`hypotenuse` 0x74F680 with the approximate
  inverse root of the 1024-entry table 0xDA5A10, ~0.1 %), but over openblack's float positions
  truncated to MapCoords. The tiger's water search is not ported (it does not change the result). `flock +0x5C`
  (without a lair) is not ported because the original only sets it to 0. Distance ties between big forests
  and trees choose the newest object by creation index, like the original's head-insertion lists.
  The order of a forest's grown trees (`GrownTreesByDistance`) uses the 3D distance to the
  centre; the original (`DistanceToForest` 0x53A890 = `GetDistanceInMetres`) measures it only in x / z: on slopes it can
  change which tree is the lair.
- Eaten villager: dies via `VillagerDead` (3 ANIMAL, its owner) and lies as a normal corpse (600 turns, the
  owner's alignment, the town's counters and help sprites, the neighbours' REACT_TO_DEATH: [villagers.md](villagers.md), Death); the
  villagers do not flee from predators nor take other reactions. A corpse is reachable, so a predator may pounce on it:
  fn_005EC480 gives it the downed mark and life 0.05 as to any Living, and its SetTopState(17) (Villager::SetTopState) is
  refused by DEAD's exit (to check in the game). The corpse
  counter of animals and villagers is the shared `living::DeadTick` (Living::Dead 0x5EC400).
- Scripts: the vortex does not exist in openblack, so nothing has the "cannot be eaten" flag yet
  (it needs the vortex: `script_held::SetCannotBeEaten` on what comes out of it); there are no script flocks
  (FLOCK_CREATE / FLOCK_ATTACH not implemented: neither DisbandId nor its references); when releasing a villager
  `Villager::ReleaseFromScript` is not ported; `SetScriptState(0x20)` of the released animal is not ported (fn_0041AA00 overwrites it
  immediately); the death reason SACRIFICE (7), which deletes a carcass even if it is controlled, is not carried. The finders
  of openblack's CHL (CALL, GET_...) do not call `AddScriptGameThing`: the slot is created with the first reference
  [approximated]. When releasing an animal, "in physics or in the hand" (+0x24 & 0x44) is in openblack the physics
  component or the IN_HAND / FLYING states [approximated], and the flag g_game +0x14 & 0x8000 (scripts stopped: with it
  `GScript::Process` does not do `LookIn`, 0x6EB6C7) does not exist in openblack. The LHVM references are faithful: the
  original ScriptLibraryR.dll calls the natives ADD_REFERENCE / REMOVE_REFERENCE from POP (0x10008BC0: the new object
  and the old one of the variable) and when stopping a task (0x10006604); a task's parameters receive them through
  the POP of its prologue.
- Flock merging (0x41A690 / 0x41A790): the original does not merge a flock with a shepherd (Flock +0x30, the villager of
  `VillagerBecomesShepherd` 0x768C1C: its own or the other) nor two of different players (`GetPlayer`); openblack has no
  shepherds nor flock player, so it only looks at its own flock's village [approximated].
- Shepherds, the leader priority of FLOCK_ATTACH, the birds' landing (unreachable in the game: info.sleep 0), the exact
  order of the lists of each map cell and the turn interleaved with the villagers.
- The random numbers go through `game_random` (GameRand over the synchronised seed); the sequence is still
  not that of an original game while other systems drawing from the stream are not ported ([engine-math.md](engine-math.md),
  "Random numbers") (approximate).
- Putting an animal down gently goes through physics like the original (`Object::InitialisePhysicsFromHand` 0x636F00,
  [physics.md](physics.md)): on land it leaves physics on the spot through `Animal::EndPhysics` 0x5F0D80. The three
  landing poses of `Villager::EndPhysics` are (pending).
- `Dove::IsPosValidForMapCellExistance (0x41F840) [the tie rules not read]: a land block under the point`
- `StepTowards`: `fn_0060ADC0: SetupMobileMoveToObject's STEP_THROUGH walk at the object, whose position is the goal each turn [inferred: the goal follows the object]`
- `Flock::info`: `+0x28 / +0x2C: the GFlockInfo and the player Flock::Flock 0x52F780 hands to its Container base (fn_0046B8A0, 0x46B8DC / 0x46B8F4); none for Flock::Flock(Living*). (pending) their readers`
- `Flock::calm`: `+0x5C: CHANGE_INNER_OUTER_PROPERTIES' calm, ftol (0x70F53C). (pending) its reader`. This disagrees
  with the Lairs point above, which says the original only sets flock +0x5C to 0; one of the two readings needs checking.
- `Living::MoveToPos` is 0x5EC270 on this page, while a test note gives "MobileWallHug::MoveTo 0x60AF20 for this
  villager (its state function's call, Living::MoveToPos 0x5EC280)"; 0x5EC280 may be the call inside the function
  (unverified).
