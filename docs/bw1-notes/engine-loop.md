# The engine loop: game turns and frames (original and openblack)

What runs once per game turn and what runs once per frame in the original, who may change the game state, how the
random streams are shared, and how openblack's loop compares; the plan to put the drawing on its own thread; the
tools to prove that two runs play the same game. Everything is **faithful** (read in
runblack.exe W120 with `bwdis.py` / `callers.py`, or in the decomp `bw1-decomp/src/Black/Game.cpp`)
except what is marked **(inferred)**, **(approximate)**, **(pending)** or **(not ported)**.

- [1. The original loop](#1-the-original-loop)
- [2. The turn, call by call](#2-the-turn-call-by-call)
- [3. The interface: frames send packets, turns apply them](#3-the-interface-frames-send-packets-turns-apply-them)
- [4. Random streams between turn and frame](#4-random-streams-between-turn-and-frame)
- [5. openblack's loop](#5-openblacks-loop)
- [6. Threads (plan)](#6-threads-plan)
- [7. Proving two runs are the same game](#7-proving-two-runs-are-the-same-game)
- [8. Floating point](#8-floating-point)
- [9. Game statistics (GameStats)](#9-game-statistics-gamestats)
- [Pending](#pending)
- [Test hooks](#test-hooks)
- [Sources](#sources)

## 1. The original loop

`GGame::Loop` 0x54CF20, each iteration (decomp Game.cpp:1902-1984):

1. `control_map->ProcessActionsPerformed`, `ProcessBufferedKeys`.
2. `ProcessNetworkPackets` 0x54CC30 (called at 0x54D28A): while `LocalTimerSaysDoATurn` 0x54C4A0 and fewer than
   1 turn this frame (10 in a network game), `ProcessOneGameTurn` 0x54D620. In a network game a turn runs only when
   the next packet is a superpacket: one superpacket is one turn (lockstep). Then fn_005525E0 (called at 0x54D291)
   hands the packets sent up to that point to the session; a later turn applies them (§3).
3. The frame clock (0x54D2A8..0x54D3A6: remainder, visual clock, `g_game_time_inc`, the fraction of the turn;
   `game_clock`, [engine-math.md](engine-math.md)).
4. `ProcessGraphicsEngine` 0x54D850: mouse delta, `GCamera::Update` 0x441F80 (at 0x54D879), `GInterface::PreDrawProcess`
   0x5CE9E0, `Process3dEngine` 0x54DA80 (the draw, [original-frame.md](original-frame.md)), `PostDrawProcess` 0x5CEAB0,
   `HelpSystem::PostDrawProcess`.
5. `NetworkTurnsThisFrame = 0` (0x54D3C3); `ProcessFrameInputs` 0x54C340 (`Mouse.ProcessButtons`,
   `GInterface::ProcessFrameUpdates` 0x5CEDB0), screenshot, `FlipScreen`.

`ProcessOneGameTurn` 0x54D620: `GameTimeMilliseconds += MsPerTurn`, **`ProcessGameInputs` 0x54C3D0**
(`ProcessBufferedKeys`, `ProcessOneSuperpacket` → `GPacket::ProcessPacket` 0x63C420, `GInterface::Process`
0x5CEC10), `ProcessGameCode` 0x54D820 (`StartTurn` 0x54E4F0 → `ProcessTurn` 0x54E5C0 unless paused → `EndTurn`
0x54E960), then `fn_005557D0`, `DoWallHuggerLookahead` 0x609A50, `RepairMissingMothers`.

Call sites: `ProcessOneGameTurn` → `ProcessGameInputs` [0x54D662]; `ProcessGameInputs` → `GInterface::Process`
[0x54C40D]; `ProcessGraphicsEngine` → `Process3dEngine` [0x54D88E]; `Process3dEngine` → `HelpSystem::Draw3D` 0x5C59A0
[0x54E2ED]; frame pass `GGame::Loop` [0x54D3E9] → `ProcessFrameUpdates` 0x5CEDB0 → fn_005D9A20.

The script's speed opcodes: `START_GAME_SPEED` 0x6FA9E0, `END_GAME_SPEED` 0x6FAAB0, `SET_GAME_SPEED` 0x6FAAE0 (the
speed itself is in [engine-math.md](engine-math.md#game-clock)).

The game runs on one thread; the window's input reaches it through critical sections (`LHKeyboard`, `LHScreen`,
decomp Game.cpp:1604-1618).

## 2. The turn, call by call

`GGame::ProcessTurn` 0x54E5C0 calls, in this order (call site → callee): 0x54E5C7 Whale::ProcessAll 0x775140;
0x54E5D7 LH3DAtmos::UpdateGame 0x8356E0; 0x54E62D PSysGlobal::GameLoopStart 0x68F590; 0x54E637 GGameInfo::Process
0x557B60; 0x54E63C InfluenceRing::ProcessRings 0x5CDB90; 0x54E641 GPlayer::ProcessPlayers 0x649A20; 0x54E646
Dance::ProcessDances 0x50BB60; 0x54E651 GlobalGameLists::Process 0x591370; 0x54E656 Forest::ProcessForests 0x539D70;
0x54E65B Living::ProcessLiving 0x5EC810; 0x54E660 FireEffect::ProcessList 0x730760; 0x54E665 Ball::ProcessBalls
0x435F30; 0x54E66A Reaction::ProcessReactions 0x6E3B50; 0x54E66F Spell::ProcessSpells 0x720300; 0x54E674
GParticleContainer::ProcessParticleContainers 0x63E090; 0x54E679 FireFly::ProcessAll 0x52B7A0; 0x54E67E
PhysicsObject::GameTurnUpdate 0x644FC0; 0x54E683 PSysEditorInterface::ProcessGameTurn 0x67D630; 0x54E688
PSysGlobal::GameLoopEnd 0x68F5B0; 0x54E693 GScript::Process 0x6EB6B0; 0x54E69E HelpSystem::Process 0x5C8FE0;
0x54E6A9 HelpProfile::Process 0x5C4660; 0x54E6C3 GLandAlignement::UpdateTime 0x5E1FE0; 0x54E6CB
WeatherThing::ProcessWeatherThings 0x7741A0; 0x54E6D0 Bookmark::ProcessAll 0x439DD0; 0x54E6D5
ScriptHighlight::ProcessHighlights 0x70A460; 0x54E6DA GClimate::ProcessAll 0x771BE0; 0x54E6DF
GBelief::ProcessOncePerTurn 0x4380B0; 0x54E6F1 CHand::GameTurnUpdate 0x46E4E0; 0x54E6F8 GGame::AddPlayerSparkles
0x552640; 0x54E6FD MobileObject::AddMobileObjectCheckSum 0x606FC0; 0x54E704 GameThing::ProcessDeadList 0x56FB10
(argument 0); 0x54E70C Reward::ProcessList 0x6E6890; 0x54E711 GSpookyVoices::Process 0x72E310; 0x54E716
GGuidance::HelpSpritesCheckMoonPhase 0x71D1C0; 0x54E729 GGuidance::ProcessTownDesireSFX 0x71B020; 0x54E731
GConfirmation::Process 0x71A650; 0x54E738 GGame::Update3DInfluence 0x555280; 0x54E743
GCamera::CheckStackedModesForValidity 0x441D40; 0x54E74E GCamera::Validate 0x441F50; 0x54E768
Fragment::ProcessTimer 0x76EAF0 (each fragment, the next taken first); 0x54E77A MusicMoodController::UpdateOnGameTurn
0x633EF0; then the memory packets and `ScriptRebootRequested`.

- `GlobalGameLists::Process` 0x591370 calls, in this order: `Field::Process` [0x591379], `FishFarm::Process`
  [0x591398], `MoveAlongPath` of the WALK_PATH list [0x5913ED], fn_00606880 [0x59140B], fn_0066E1C0 [0x591429] and the
  PuzzleGame fn_006D7480 [0x591449].
- `GScript::Process` 0x6EB6B0 starts with fn_006EB930 [0x6EB6BA], the countdown timer's turn, before running the
  scripts.
- `GGame::EndTurn` 0x54E960 keeps the statistics: every `UpdateInterval` turns ([0x8FFDB8] = 10; 5 while bit 2 of
  g_game+0x14 is set) it calls `GameStats::AddToTotalLinesOfCodeExecuted` 0x565110 [0x54EAE9..0x54EB07], which draws a
  number from the local stream (`LocalRand` 0x6DE570), and then the frames-per-second count [0x54EB0C..0x54EB39]
  (see [§9](#9-game-statistics-gamestats)).

The dead list (`GameThing::ToBeDeleted` 0x56FB70 marks, `ProcessDeadList(0)` frees on the second pass):
- `GameThing::ToBeDeleted` 0x56FB70: already unavailable, nothing; with its `now` argument, deleted at once; else
  marked UNAVAILABLE (+0xA bit 0, bit 1 clear) and put at the head of the dead list (GameLists.GameThings, head
  g_game+0x205D1C, count +0x205D20). The physics body stays: `PhysicsObject::GameTurnUpdate` drops the unavailable
  ones at the start of the next turn (0x645018).
- `ProcessDeadList` 0x56FB10 / `ProcessDead` 0x56FAA0: from the head (the last marked first), each thing marked before
  this pass; the first pass only notes it (+0xA bit 1), the next one frees it (`Delete`, vt +8). The turn calls it
  with 0 at 0x54E704; with 1 (`GGame::Close` 0x54EC80, `ClearMap` 0x552E58, `LoadAllGame` 0x558A9B) it frees them
  all, again until the list is empty.
- The classes' own parts (vt +0xC): `Villager::ToBeDeleted` 0x7521B0 (`DeleteDependancys` 0x74FD60, then
  `Living::ToBeDeleted` 0x5EC0A0's StopReacting); `Animal::ToBeDeleted` 0x417B60 → `DeleteDependancys` 0x417BA0 (out
  of its town's list, town = 0, then out of its flock; prey and hunter links stay, their readers ask IsAvailable);
  `Tree::ToBeDeleted` 0x74A210 (out of its forest, fn_0053A220, and of the tree list g_game+0x205CDC);
  `ScriptHighlight::ToBeDeleted` 0x709980 (out of the highlights' list, its two effects closed); `Object::ToBeDeleted`
  0x636670, whose fire (+0x44) is deleted after the mark (0x63667D..0x63668B); then `CleanupWhenDeleted` 0x6377F0
  takes the object out of its cells (vt +0x548).
- openblack has the dead list (`ecs::ToBeDeleted` / `ecs::ProcessDeadList`, through the `ToBeDeletedSystem` service),
  but the deferral stays off until every owner's readers ask `IsAvailable`, so an entity is still destroyed at once
  **(pending)**.

## 3. The interface: frames send packets, turns apply them

- The interface's message pump and action state machine (fn_005D9A20 → fn_005D9BC0 → fn_005D1120
  InterfaceActionProcess) run **once per frame** (`ProcessFrameUpdates` 0x5CEDB0 is `jmp 0x5D9A20`, from
  `ProcessFrameInputs` at 0x54C3B6) **and once per turn** (`GInterface::Process` at 0x5CEC1F). The queue (+0x43E) is
  emptied after each pump, so a mouse message is handled once, almost always in the frame pass.
- They do not change the game: they send packets (0x13 pick up, 0x12 / 0x4D throw, 0x20 tap, 0x1B / 0x1C locked
  select, 0x1F give, 0x6A power-up; 0x15 hand position, 0x16 / 0x17 camera from fn_005D2250 per turn), applied by
  `ProcessOneSuperpacket` → `GPacket::ProcessPacket` 0x63C420 at the start of a later turn.
- The hand's per-turn game writers: `GInterfaceStatus::Process` 0x5DC4E0 (the multi pick-up transfer,
  `ProcessInInteract` 0x66E520) and `CHand::GameTurnUpdate` 0x46E4E0.
  - `CHand::GameTurnUpdate` 0x46E4E0 = the HandStateGrain raise (`fn_005B2D70`), `CHand::ThrowObject` when the held
    object is no longer available, +0x490C = GetHoldType, the +0x48FC clean-up (+0x49B4 / +0x4950 not read), then the
    hold (0x46E505..0x46E557). The held object's `ProcessInHand` runs at the start of the turn instead
    (`GInterfaceStatus::Process` 0x5DC4E0 → `ProcessHands` 0x5DC6A0).
- The packets: `GGame::SendPacketCompressed` 0x551690 writes into the buffer g_game +0x5318, which is only flushed at
  6 packets (0x551E0A). The loop flushes it once per iteration after the turns (fn_005525E0 at 0x54D291 →
  `LHSession::Write`). The next turn applies it first (ProcessGameInputs 0x54C3D0 → ProcessOneSuperpacket 0x63C3D0 →
  0x63C420): nothing is applied in the turn that sent it.
- Per frame without game writes: hand placement and spring (HandStateHolding::Update 0x5B3C70), the held object's
  pose (CHand::UpdateHeldObject 0x46E490), PreDrawProcess (UpdateHandRenderCollide 0x5D0610, disciple icon), and
  PostDrawProcess (leashes 0x5D9130, UpdateInterfaceCollide 0x5D5A70).
- The hand demos (fn_005DAEE0) play inside the pump, in both passes.
- During a hand demo, `GCamera::Update` skips its own update [0x442614]: the recording sets the camera with
  `GCamera::SetPositionAndFocus` 0x4438C0 and `LH3DTech::UpdateCamera` 0x819920, and it is the one in charge. The demo
  runs in the turn (via `ProcessGameInputs` → `GInterface::Process`), before the frame's graphics, so the spirits are
  placed (fn_005C3A30) and drawn with that camera.
- No synced random number is drawn in the hand and interface code (CHand 0x46B000-0x46F000, hand states
  0x5B0000-0x5BA000, GInterface 0x5CE000-0x5DD000, gestures 0x578000-0x57D000).

The draw writes a few game fields itself: the whale's heading (fn_00774E30, `mov [ebx+0x6c]` at 0x774FF2) and the
fish bait (fn_00824B90: +0x20 / +0x24 / +0x18 / +0x8C).

## 4. Random streams between turn and frame

| Stream ([engine-math.md](engine-math.md) "Random numbers") | Turn side | Frame / draw side |
|---|---|---|
| Synced GRand (GameRand 0x6DE510) | the game logic | none found in the draw |
| Local GRand (LocalRand 0x6DE570: 115 call sites; LocalFloatRand 0x6DE590: 49) | PhysicsObject::AttemptToAddSoundEvent 0x6467D1, GGuidance, GSpookyVoices, PSys local fn_00672AA0, ParticleMistCreator 0x6AA5E0 | Tree::Draw 0x74B135, Abode::Draw 0x51615A, light maps fn_006CA7D0, ParticleLightMap::DrawAt 0x67B26A, HelpSystem draw, fire flames fn_00731AB0 |
| CRT (rand 0x7C8837: 48; Random 0x81D180: 167) | SmokyStuff::Create 0x823D39, LH3DMist, Reward and TempleLeash creation | GWeather::DrawClouds 0x83FD05, Tree::PreDraw 0x74A805, camera shake fn_008210C0, fish fn_00824DA0, PetitNavire::PostDraw |
| PSys selection ([0xD4E0C0] / [0xD4E0BC], per effect step fn_00673340) | effects stepped in the turn | effects stepped in the draw |

So all streams but the synced one are shared by the turn and the draw on the one game thread: running turns and
frames at the same time would change their sequence (and race on the PSys selection).

## 5. openblack's loop

`Game::Update` (src/Game.cpp) per frame: input and events, ImGui, the camera **before** the turns (the original
updates it after: **(approximate)**), `while (game_clock::TurnDue()) GameLogicLoop();`, the frame clock, the
per-frame updaters (physics interpolation, fields, trees, fireflies, rings, scenery, mobiles, animations, sharks,
fish), the hand, `RenderingSystem::PrepareDraw`, audio; then `Game::Run` draws (`DrawScene`) and calls `bgfx::frame`.
bgfx is built multithreaded (vcpkg feature `multithreaded` → `BGFX_CONFIG_MULTITHREADED`): its render thread is
already separate; openblack's main thread is the bgfx API thread.

> **Code rules.** The turn's steps reach their systems through Locator services and one-way notifications go
> through `Locator::events`, never globals; the clock and the random streams are pure modules that tests drive with
> fakes in `test/`; comments describe behaviour in plain English, with no decompiled names or addresses (those
> belong here). See [openblack-internals.md](openblack-internals.md).

Differences with §1-§3:
- The hand acts on the game directly every frame (pick-up, release, taps, tug, seeds) instead of packets applied in
  the turn **(approximate, pending)**.
- The first animation clip of a new villager or animal is chosen per frame with synced GameRand; the original does
  it at creation in the turn (Living::CallVirtualFunctionsForCreation 0x5EC9B0 → SetStateAnim 0x5ECB10)
  **(pending)**.
- Bullet was stepped every frame with the wall clock (openblack-only); it only answers ray casts **(not original)**.
- The turn steps follow §2 since the turn-order commit (Game::GameLogicLoop names each call site); what is not
  ported is marked there.
- In openblack the hand demo runs once per frame, before `Draw3D`; the original runs it in both passes, and the frame
  pass's one reaches the screen one frame later **(approximate)**.
- The intro light (fn_005DF640, and the call of its Z object 0x8283D0) draws its numbers from the CRT `Random` when the
  Z queue is flushed, after the clouds, the night lights and the smoke; openblack draws them after updating the ship
  **(approximate)**.

## 6. Threads (plan)

User decision (2026-10-03): the game logic stays exactly the original's (turns, order, random numbers); the drawing
may run on its own thread, and **one frame of display latency is accepted by default** — a presentation difference,
not a logic one. A single-thread mode without latency stays for comparison and for the replay test.

Design "level 1": one logic thread runs every frame's game code in today's order (input, turns,
frame updaters, hand, the draw-side writers moved out of DrawScene); it publishes an immutable snapshot of what the
draw reads; the main thread (window, bgfx API) encodes the previous frame's snapshot. Turns in parallel with frames
("level 2") cannot be identical (§4) **(pending)**.

## 7. Proving two runs are the same game

- `OPENBLACK_STATE_HASH=<file>` (src/Debug/StateHash.*): at the end of each turn, one line `turn <n> <total>
  random=… crt=… clock=… pools=… transform=…` (FNV-1a over the GRand seeds and the PSys stream; the CRT seed; the
  turn, pause, speed, ms per turn; every registry storage's size and entity order, folded by storage id; every
  Transform's bits). Owners add their parts with `state_hash::Register`. Not the original's network checksum (`AddMobileObjectCheckSum`
  0x606FC0 / `SendNetworkChecksum` 0x635210, **not ported**).
- `OPENBLACK_FIXED_FRAME_MS=<ms>` (src/Debug/FixedClock.*): every frame is that many ms (game timer, engine timer,
  audio ticks, the frame's delta), so a run repeats (not covered: the 10 s wall-clock timeout of Particles/Rules/Lightning.cpp).
- `test_replay_determinism`: the mock land played twice, headless, with the fixed clock; the per-turn hashes must be
  equal (later: one thread against two).
- `OPENBLACK_PROFILE=<s>` now also logs the stages "Frame Updaters", "Turn: Map Rebuild", "Turn: Magic", "Turn:
  Physics", "Turn: Particles" and the memory (RAM working set, peak and private bytes; bgfx texture and render-target
  memory, GPU memory when the backend says, transient buffers).

## 8. Floating point

- The original sets the x87 FPU to 24-bit precision for the game logic (fn_007DEE00, `and 0xFCFF` at 0x7DEE0D; also at
  the start of GGame::EndTurn): each + − × ÷ and sqrt rounds to a float. openblack is x64: SSE2 scalar float, one
  rounding per operation, MSVC's default `/fp:precise`, no FMA. For those operations the results are the same,
  **(inferred)** except where an intermediate would overflow, underflow or be denormal in float (the x87 keeps its
  extended exponent range).
- **(not verified)** The transcendental functions (`pow`, `sin`, `cos`, `atan2`, `exp`, `log`…) of the x64 CRT are not
  the original's x87 instructions (`fsin`, `fpatan`, `fyl2x`…) or its CRT's x87 code: their last bits can differ. To
  check case by case where it matters to the game's results (game_random already writes its arithmetic one operation
  per statement).
- The top CMakeLists.txt stops the configure on `/fp:fast`, `/fp:contract`, `/arch:AVX2` / `AVX512`, `-ffast-math`,
  `-mfma`, `-ffp-contract=fast` (they would fuse or reassociate float math).
- Threads: a new thread starts with the default MXCSR (0x1F80: round to nearest, no flush-to-zero); nothing in src
  changes it. The game logic always runs on one thread; the logic thread will check its MXCSR in debug builds.

## 9. Game statistics (GameStats)

`EndTurn` feeds it (§2).

- One GameStats per player: GPlayer +0xA44, a GameThing of 0x1128 bytes, vtable 0x8FFDC0. It holds the data of the
  Statistics page of the in-game menu (StatsBox; a snapshot taken by fn_00566BC0).
- `GameStats::Init` 0x564B40(player): +0x1068 = `GPlayer::GetAlignmentValue` [0x564B50]; +0x107C = creature (+0xA4C)
  +0x168 +8, or 0 without a creature [0x564B5B..0x564B7B]; [0xD0604C] = `_time()` [0x564B76..0x564B81] ("Time
  Played"); [0xD06050] = how many towns there are in GameLists.TownList (g_game+0x205C88, the `inc [esi+4]` of the Town
  ctor 0x739656) [0x564B86..0x564B96].
- `GameStats::ClearAll` 0x564D90 (from `GGame::ClearMap`): fn_00564BA0 for each player [0x564C56..0x564C6E] (sets
  +0x5C = −1) and the statics: [0xD06050] = 0 [0x564D60], [0xD06054] [0x564BA6].
- Histories: two adaptive histories of 0x7DC bytes, influence at +0xAC (`GPlayer::CalculateInfluencePower` 0x64ADA3)
  and population at +0x888 (`GPlayer::Process` 0x649599); ctor 0x564A40. Each has 500 buckets, and each bucket is the
  mean of `period` samples. When the 500 are full, neighbouring buckets are averaged in pairs into the first half
  [0x64960D..0x649641] and the period doubles [0x6495A5..0x64966D].
- Original bug that is kept: when emptying, `count` **bytes** are cleared starting at bucket[count] (`rep stosd`
  count/4 and `rep stosb` count&3), not `count` buckets [0x649643..0x64965A].
- +0x20..+0x44: sums over the player's towns stored by fn_00564DF0; the exe recalculates them every turn. Among them,
  the wonders = Town::stats.abodesByNumber[10] (`TownStats::Add` [0x74997C..0x749988]; on removal, fn_00749990
  [0x749A45..0x749A56]).
- `AddToTotalLinesOfCodeExecuted` 0x565110: [0xD0603C] += ftol(LocalRand(ftol(v·10130·0.2)) + v·10130), with v =
  [0xD06050] and the doubles 10130 [0x8FFEC0] and 0.2 [0x8C7C68] [0x565113..0x56515C]. It is called by `EndTurn` and
  fn_00566890 (the Statistics page, once).
- Frames per second (`EndTurn` [0x54EB0C..0x54EB39]): g_frame_rate_stats [0xC386E0] goes into the maximum [0xD06040]
  or, otherwise, into the minimum [0xD06044]. A new maximum skips the minimum, and the minimum starts at 0, so it never
  changes.
- Objects created [0xD06048] += 1 in fn_00436870 (the objects' `new`) ("Total Objects Created"). Version [0xBEE568] =
  113, a constant nobody writes.
- Final places: [0xD06054] = places counted; +0x18 place and +0x1C value (2nd argument), both from fn_0056A6D0;
  +0x1064 [0x56A724].
- Births +0x48 (`ChildBorn` [0x7623E5]). Deaths +0x4C and villagers killed +0x106C: fn_0073E440 [0x73E46A..0x73E479]
  adds one to the town's player and another to the killer (3rd argument, not checked for null).
- Population: `GPlayer::Process` [0x649594] → fn_0056A320(men, women), summed over the player's towns (TownStats +0x54
  / +0x58). Keeps maximum and minimum of the sum (+0x50 / +0x5C, fn_0056A350), of the men (+0x54, fn_0056A370, town
  +0x664) and of the women (+0x58, fn_0056A390, town +0x668). Uses `ja`, and otherwise goes to the minimum: a new
  maximum skips the minimum.
- Town taken over: `GPlayer::TakeOverTown` fn_00649810 adds to the new owner's +0x68 the town's adults +0x618 plus
  children +0x61C [0x6498F7..0x649910].
- Desire: +0x6C sum (`TownDesire::Process` [0x745C43]) and +0x70 count [0x745C30]; the row shows +0x6C / +0x70
  [0x566C60].
- Buildings: `IncrementAllBuildingsBuilt` 0x56A3F0 adds one to +0x74 [0x56A3F3..0x56A3FC] and another to +0x78 / +0x7C /
  +0x80 according to the first set type bit of the abode, 2 / 4 / 0x100 [0x56A409, 0x56A423, 0x56A43D]. The type is
  given by `Abode::GetAbodeType` 0x4061F0 (vt +0x8C4, the info's ABODE_TYPE).
- Disciples: fn_0056A450 (from `Villager::Landed` [0x7607DF]), table 0x56A4AC: VILLAGER_DISCIPLE 1..5 → +0x84..+0x94,
  6 is not counted, 7 → +0x9C, 8 → +0xA0; nobody writes +0x98.
- Food eaten +0xA4 (`Town::UseFood` [0x73B60F]); wood used +0xA8 (fn_0073B620 [0x73B64F]).
- World belief: fn_0056A3B0 (from TownBelief [0x438177..0x438187], the player's summed belief). If it exceeds the
  maximum +0x1070, it is the new maximum and the minimum is skipped (`test ah, 0x41` / `jne 0x56A3CE`)
  [0x56A3B4..0x56A3BF]. Otherwise, if it is below the minimum +0x1074 (starts at FLT_MAX [0x8FFDA8]), it is the new
  minimum [0x56A3D2..0x56A3DD].
- Creature value +0x1078 (`Creature::ProcessState` [0x472E49]).
- Buildings that stop working +0x1080: `Abode::StopBeingFunctional` [0x4073D1..0x4073E0]; the caller checks the player
  and that +0xB9 ≥ 200.
- +0x1124: written by `Animal::ApplyThisToObject` [0x41B3F4] and `VillagerDead` [0x75091D].
- Miracles cast: fn_0056A4D0 (from `Spell::InitWithPos` [0x71FE9B]). MagicType 1..35 → +0x1090 + 4·(type − 1), 41 →
  +0x111C; for the rest the jump table 0x56A62C counts nothing.
- Saving: `GameStats::Save` 0x565E70 comes after the GameThing header and the player (0x56FBE0, 0x561E10). It writes
  the fields as 4-byte values, +0x20 as 0x28 bytes, each history as 0x7DC bytes (fn_0056A890), and the GameStats
  statics in place, 4 bytes each (`GameOSFile::WriteIt(long&)` 0x4E94F0 included).

## Addresses of the original, moved out of the code

The code's comments describe what it does in plain words; the original's addresses and function names they used to quote are kept here, next to the openblack symbol each one corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `[0x1001F140]` | the script DLL's float constant the product is multiplied by before the fstp | `LHVM (float rounding)` |
| `ScriptDLL::LookIn 0x6F6840` | the exe's entry into the script DLL, called by GScript::Process on the game thread | `LHVM (script step)` |
| `ScriptLibraryR.dll TaskNumber 0x100025A0` | the script DLL's export that returns the id of the task running now | `LHVM::GetCurrentTaskNumber` |
| `GetCurrentScriptType 0x10002630` | the script DLL's export that returns the type of the task running now | `LHVM::GetCurrentTaskScriptType` |
| `GetScriptType 0x10002650` | the script DLL's export that returns the type of a task by its number | `LHVM::GetTaskScriptType` |
| `0x10008217` | the end of the script DLL's LookIn, where the tick count goes up by one | `LHVM::GetTicksCount` |
| `DLL_GETTIME handler 0x1000ABD0` | the script DLL's elapsed time: the tick count [0x1003BDB4] times 0.1f, pushed as type 2 | `LHVM::PushElaspedTime` |
| `TaskNumber -> 0x10008320, ScriptDLL::TaskNumber 0x6F69F0` | the DLL routine behind TaskNumber (reads the task's +0x14) and the exe's wrapper | `LHVM::GetCurrentTaskNumber` |
| `ScriptDLL::GetCurrentTaskScriptType 0x6F6A90` | the exe's wrapper of GetCurrentScriptType (reads the task's +0x158) | `LHVM::GetCurrentTaskScriptType` |
| `GetScriptType -> 0x100051F0, ScriptDLL::GetScriptType 0x6F6C50` | the DLL routine behind GetScriptType (the task's +0x158) and the exe's wrapper | `LHVM::GetTaskScriptType` |
| `GScript::Process 0x6EB6B0, fn_007DEE00` | the exe's script step on the game thread, and the routine that sets the FPU to 24 bits | `LHVM::GetTicksCount` |
| `INTCAST opcode 23 = 0x10008EE0, switch table 0x100090E4` | the script DLL's cast, a switch on type - 1 | `LHVM (Mode::CAST)` |
| `0x10008F0A, __ftol 0x1001568C` | the cast to int: fistp qword with truncation, the low dword | `LHVM (Mode::CAST, Int)` |
| `0x10008F6F` | the cast to float: fild qword with a zero high dword | `LHVM (Mode::CAST, Float)` |
| `0x100090DE` | the cast's case for type 5: nothing | `LHVM (Mode::CAST, Unk5)` |
| `0x10008F32, 0x10008FA2, 0x10008F57` | the cast's cases for vector, object and boolean: retag only | `LHVM (Mode::CAST, default)` |

## Pending

- The hand as packets applied in the turn, the first clip at creation, the rest of the turn order (§2), the
  deferred dead list (§2), the threads (§6), the per-frame registry entities of carried props and scenery.
- `FillOverlayFrame` (src/Game.cpp), in `helptext::Get(0x1016)};`: literal note «// 0x5C8579..0x5C8609». It is not
  clear whether the original uses help text 0x1016 in that range or whether it is only where the line is built.
- `Stats::killed` (+0x1124): (unknown) what it counts. The old note only says who writes it: an animal applying
  itself to an object and a villager's death.
- `finishPlace`, `finishValue`, `playersOutBefore`, `maxPopulation`, `u1084`: (unknown). Their old notes only gave an
  offset.
- `AddToTotalLinesOfCodeExecuted`: the interval is 5 instead of 10 while «bit 2» of the game flags (g_game+0x14) is
  set. (unknown) what that flag means. Unverified: if «bit 2» means the value 4, it matches the pause bit
  (`PauseGame`, g+0x14 & 4 in engine-math.md).

## Test hooks

`OPENBLACK_STATE_HASH`, `OPENBLACK_FIXED_FRAME_MS`, `OPENBLACK_PROFILE` (with memory), `OPENBLACK_TRACE_GAME_RAND`
([engine-math.md](engine-math.md)); headless runs with `-b Noop -n <frames>`.

## Sources

runblack.exe W120 (`bwdis.py 0x54E5C0:0x200`, `callers.py`); `bw1-decomp/src/Black/Game.cpp`
1585-1620, 1704-1788, 1902-1984, 2072-2101, 2105-2120, 2438-2527; `bw1-decomp/src/Black/GameThing.cpp:55-131`.
