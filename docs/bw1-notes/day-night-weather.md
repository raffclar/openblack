# Day, night and weather in the original

The game weather (climates, storms, rain) is at the end, in «Weather and climate».

- [Clock](#clock-done-src3ddaynightclock)
- [Sky type](#sky-type-src3dskytype)
- [Night lights](#night-lights-report-night_visualstxt)
- [Climate](#climate)
- [Weather and climate](#weather-and-climate-srcecsweather)
- [Pending](#pending)

## Clock (done: `src/3D/DayNightClock.*`)

There are **two clocks** in hours 0..24:

- **Visual time** (`GLandAlignement::VisualTime` 0xBF3380). It advances linearly, one day every `duration` seconds of
  game time (1700 s by default, about 28 min). It is moved by `GLandAlignement::UpdateTime` 0x5E1FE0 once per
  turn, called from `GGame::ProcessTurn` 0x54E6AE with `(scale · 0.1, 0.1)` (`GGame::ProcessTurn` passes the turn length [0xD01A3C] × 0.1, 0x54E6AE..0x54E6C3). With the game paused it does not advance and with
  the game sped up it goes faster (it goes by turns).
- **Script time**: the visual time passed through a piecewise linear function (`fn_00869FD0`; `fn_0086A160` visual →
  script, `fn_0086A110` script → visual). It maps the cycle thresholds to the fixed hours 3.5 / 7.5 / 8 / 8.5
  (0xC395B0..BC), mirrored around 12. It is used by `GET_GAME_TIME` / `SET_GAME_TIME`, the sun (`fn_0086C020`), the moon
  (`LH3DAtmos::UpdateGame`) and the street lantern lights (`fn_0086C220`). openblack passes this time to the sun and the moon
  (`Sky::SetTime` / `GetTime`). It is **not** the time of the sky type: that one runs on the visual time (see
  [Sky type](#sky-type-src3dskytype)).

**Cycle** (`GGameInfo::SetVisualTimeCycle` 0x557620, `(duration, night, change)`; fractions of the whole day):

- Speed: `n = ftol(duration · 0.41666666f)` ([0x8DF8F0] = 0x3ED55555) and `10/n` hours per second, the same by day
  and by night (0xBF338C and 0xBF3390 hold the same value; 0 for n = 0, 0x557645..0x557685). The product stays in the x87 register before `__ftol`: with the
  FPU at 24 bits (`fn_007DEE00`, `and cw, 0xFCFF` at 0x7DEE0D) it is the float product that openblack computes; with 53 bits
  durations that are multiples of 2.4 (1200, 2400) would give one `n` less.
- Thresholds (`LH3DSky::SetDayNightTimes` 0x869FA0 → 0xFA26A0..94): N = 12·night, E = 12·change + N (stored as
  float), c = min((E − N)·0.25, N), and `SetDayNightTimes(N − c, c + N, E − c, E + c)` (0x55768F..0x5576E4; **faithful**,
  openblack does it in this order from `DayNightClock::SetCycle`, which also calls `sky_type::SetThresholds`).
  By default (1700; 0.083; 0.07) they are **0.786 / 1.206 / 1.626 / 2.046 h**
  from midnight. That is, there is full night only between 23.2 and 0.8 visual hours and it is day between 2 and
  22 h: the night lasts about 2 real minutes and each transition another 30 s.
- `LH3DSky::Time2SkyType` 0x86A1B0 over the visual time: 2 at night, 1 at dusk, 0 by day. `IsVisualNight` is > 1.2
  (the double). All the detail, in [Sky type](#sky-type-src3dskytype).
- `fn_0086A3B0` sets (4.5; 7; 7.5; 8.25) when opening the sky, but `GLandAlignement::Open` overwrites it immediately with the
  default cycle.
- `ResetGameTimeProperties` 0x711520: 1700 s, 8.3 % night, 7 % change. `SetVisualTimeScale` 0x557610
  (GAME_TIME_ON_OFF). Target hour 0xBF3384 (12), step 0xBF3388 (2.5 h per second of game time), MOVE_GAME_TIME in
  progress 0xD1A268.
- In the campaign the thresholds are **always** the default ones: `challenge.chl` never calls 407 or 408, Land1,
  Land4, Land5 and LandT have no `SET_NIGHTTIME` and Land2/Land3 repeat (1700; 0.083; 0.07). The playgrounds do
  change them: SandBox Creature Training and ThreeGods (1000; 0.23; 0.17) → 2.25 / 3.27 / 4.29 / 5.31; Demon God
  (1143; 1; 0) → 12 / 12 / 12 / 12, type 2 always except at exactly 12 o'clock (0); Ultimate Sandbox (3400; 0.4; 0.01) →
  4.77 / 4.83 / 4.89 / 4.95.

**Advance per turn** (`UpdateTime(a, b)`, a = scale · 0.1 and b = 0.1):

1. If there is no `MOVE_GAME_TIME` in progress: step 2.5 h/s and target = visual + speed · a, where speed =
   lerp(day, night, sky type / 2).
2. The visual time approaches the target by the short way (±12 h), at most step · b per turn, and wraps around at 24.
3. `SetTime` 0x5E22E0 normalises the target and sets it to 0 outside ±1000. With seconds ≠ 0, step = |difference| /
   seconds. `MOVE_GAME_TIME` finishes on the turn after arriving.

Checked: 20 000 turns plus a `MOVE_GAME_TIME(3, 30)` give in the port the same values as the emulation of the
original, down to the sixth decimal.

**Startup and scripts**

- `GLandAlignement::Open`: scale 1, default cycle and time 12. openblack calls `DayNightClock::Reset` at the start of
  `Game::LoadMap`, before the island script.
- Island script: `SET_NIGHTTIME(1700, 0.083, 0.07)` in Land2/Land3 reaches `SetVisualTimeCycleFromMapEditor` 0x557BB0,
  which limits night ≤ 1 and change ≤ 1 − night.
- CHL: `SET_GAME_TIME` 112, `GET_GAME_TIME` 113, `GAME_TIME_ON_OFF` 288 (scale 1/0), `MOVE_GAME_TIME` 289 (hour,
  seconds), `SET_GAME_TIME_PROPERTIES` 407, `RESET_GAME_TIME_PROPERTIES` 408.
- `SET_GAME_TIME` 0x710E20: ForceVisualTime (script → visual time); `GET_GAME_TIME` 0x710E60: the visual time mapped
  back to script time. `GAME_TIME_ON_OFF` 0x710E90: `SetVisualTimeScale(on ? 1 : 0)`; `MOVE_GAME_TIME` 0x710EC0: the
  visual time slides to the hour in `duration` seconds of game time. `SET_GAME_TIME_PROPERTIES` 0x7114B0:
  `SetVisualTimeCycle(duration, percentage night, percentage change)`.
- Land1 starts with `FollowUs`: `SET_GAME_TIME(7.3)` and `GAME_TIME_ON_OFF(0)`, a dawn with the clock stopped; later
  it does `MOVE_GAME_TIME(12, 460)`. `LandControl1` sets 15.4 h and turns the clock on. Other sequences set their
  time, for example `VillageWavingSequence` 12/16 h and the Land4 meteorite one 16.5 h.
- In openblack the intro does not advance (CHL functions are missing), so Land1 stays for now at 7.3 h with the clock
  stopped. To see the cycle you have to use the World menu or the hooks below.
- **VM fix**: unimplemented CHL functions left their arguments on the stack, and those of the following ones were
  shifted. For example, `SET_PROPERTY` left a 1.0 that `SET_GAME_TIME` then read. Now `LHVM::Opcode05Sys`
  removes `stackIn` values when the function has not popped any, below whatever it has pushed.

**Test hooks**

- `OPENBLACK_TIME_OF_DAY=<script hour>` fixes the time on every turn (`SetScriptTime`, with the sky jump).
- `OPENBLACK_TEST_MOVE_TIME="hour,seconds"` does a `MOVE_GAME_TIME` on load; the script may overwrite it.
- `OPENBLACK_CLOCK_TRACE=1` writes the clock to the log every 50 turns: visual time, script time, sky type computed
  now, the frame's one (`sky_type::Frame()`) and the dome's one (`Dome().Built()`).
- The World menu shows the script time (slider), the visual time, the frame's sky type and the dome's.

## Sky type (`src/3D/SkyType.*`)

A single API, `openblack::sky_type::` (`src/3D/SkyType.h`), with the original's convention: **2 night, 1 dusk, 0 day**,
continuous. All **faithful** except what is marked.

| Function | Original | What it does |
|---|---|---|
| `At(hour)` | `Time2SkyType` 0x86A1B0 | Folds around 12 only if hour > 12 (`test ah,0x41`); then strict `<` against A..D: 2, 2 − (h − A)/(B − A), 1, 1 − (h − C)/(D − C), 0. There is no division by zero: with A = B the ramp is unreachable. `DayNightClock::SkyType(hour)` (was `Time2SkyType`) forwards here with its thresholds |
| `SetThresholds(A,B,C,D)` | `SetDayNightTimes` 0x869FA0 | A → 0xFA26A0, B → 0xFA269C, C → 0xFA2698, D → 0xFA2694; called by `DayNightClock::SetCycle` (0x557620) |
| `SampleFrame(visual)`, `Frame()`, `FrameHour()` | `fn_0086A2C0`, [0xFA26BC], [0xFA26C4] | Normalises to [0, 24) with the loops at 0x86A2C4..0x86A308 and stores hour and type. `DrawSky` 0x5E2226 calls it once per frame with the visual time [0xBF3380]: in openblack, at the start of `Renderer::DrawScene` (once per frame, not on every pass) |
| `Jump(visual)` | `fn_0086A270` | `SampleFrame` and the whole dome at once. Called by `fn_005E22A0` 0x5E22CB (`ForceVisualTime` 0x5575D0): in openblack, `DayNightClock::SetScriptTime` (Reset/LoadMap, `SET_GAME_TIME`, the slider, `OPENBLACK_TIME_OF_DAY`, `config.timeOfDay`). `MOVE_GAME_TIME` does not jump |
| `IsVisualNight(T)` | 0x5575E0 | T > the double 1.2 ([0x8D8758] = 33 33 33 33 33 33 F3 3F): T = 1.2f is already night. Used by `DayNightClock::IsVisualNight`; `ChildAtCreche` 0x757CFF has the same comparison |
| `EveningRamp(visual, w, o)` | `fn_00557AE0` | u = 24 − visual, s = D + o; u < s → 1; !((D + w) + o > u) → 0; otherwise, 1 − (u − s)/w. For the Relaxation 0x7488C0 / Sleep 0x748960 desires (no caller yet, **pending**) |
| `LightColumn(T)` | 0x86985E..0x8698AD | (2 − T)·6.0f·2.5f = (2 − T)·15, column of the light table |
| `HazeFactor(T)` | 0x869D5F..0x869D7A | v = T > 1 ? 2 − T : T; v² |
| `DomeWeightOf`, `BlendTexel555`, `DomeBlend` | `fn_0086B7F0`, `fn_0086B9A0`, `fn_0086A330` | The dome, below |

Who calls what:

- They compute the type on the spot with the visual time, like the original: the model lighting
  (`model_light::UpdateFrameLight(…, sky_type::At(visual))`, like `fn_005E5830` 0x5E58D1..0x5E58DF; it gives the same as
  `Frame()` because `DrawSky` samples the same time right afterwards), `DayNightClock::ProcessTurn` (0x5E202B), the
  fireflies (`SkyType`, 0x52B7CA / 0x52B820) and `IsVisualNight` (night lights).
- They read the frame sample: the dome and `LandLightTable::Build(sky_type::Frame(), …)` (column
  `LightColumn`, haze `HazeFactor`; see [rendering.md](rendering.md)). Sound (`GSoundMap`, 0x71DDF1) must read
  `Frame()`, the T of the previous frame (ProcessTurn 0x54D830 and EndTurn 0x54D837 run back to back, without `DrawSky`):
  `audio::ProcessTurn` reads it this way (done).
- Constants: 12 [0x8CF1C0], 24 [0x8CA26C], 2.5 [0x8C581C], 255 [0x8AB270] (0x86B8A6 / 0x86B8C3); the ramp's fdivp is
  (h − A)/(B − A) (0x86A206..0x86A21A). The set-up thresholds 4.5 / 7 / 7.5 / 8.25 are pushed as 0x40900000 /
  0x40E00000 / 0x40F00000 / 0x41040000 (0x86A3B7..0x86A3E0).
- Removed: `Sky::GetCurrentSkyType` with script time and invented thresholds 3.5 / 7.5 / 8 / 8.5 with `<=` (and later
  its forwarder 2 − `Frame()`), `Sky::SetDayNightTimes`, `u_skyAlphaThreshold.x` (fs_object does not read it), `u_sky` and its
  fallback ramp in `fs_water` (without `palette.raw` the sea now goes unlit, white, **(inferred)**: the original always has
  the table), and `u_skyAndBump.x` (it is 0; fs_terrain no longer reads it: the sky type reaches the land only through the
  light table and the haze of vs_terrain).
- Precision: the FPU runs at 24 bits (`fn_007DEE00`, `and cw, 0xFCFF` at 0x7DEE0D), the same hypothesis as `SetCycle`. With
  it the `Time2SkyType` ramps already come out rounded to float, the `SampleFrame` loops run in float (h = −1e-7
  gives 24 → 0; with 53/64 bits it would stay at 23.9999999 and 24.0f would be stored) and the hysteresis subtraction is rounded to
  float before comparing it with the double 0.03f.
- NaN: `Time2SkyType` compares with `fcomp` / `test ah,1` / `je` (0x86A1DC..0x86A246), so «unordered» counts
  as «<»: a NaN time gives 2 (night). openblack copies it with `!(t >= x)` comparisons. In `SampleFrame` the original
  gets stuck in an infinite loop with NaN; openblack carries on (openblack difference).

**The dome** (`DomeBlend`, `Sky::UpdateDome`, `fs_sky.sc`):

- `fn_0086A3B0` creates 3 dynamic 256×256 textures (flags 0x104, format 4; [0xFA2738 + 4a]), one per alignment,
  and builds them whole. Sources: `Data\WeatherSystem\sky_<good|ntrl|evil>_<day|dusk|night>.555`, index
  3·time_of_day + alignment ([0xFA26E8]). It builds the whole dome at 0x86A595, and the dome's [0xFA26C0] is 0 again
  at 0x86A5AB.
- Each frame, `fn_0086A330` (after `SampleFrame`, 0x5E222B): if the dome is complete and |`Frame()` − built|
  > 0.03 (the double [0x99A168] = (double)0.03f, strict), it stores the new T ([0xFA26C0]) and goes back to row 0
  ([0xFA26B8]); while rows are missing it blends 32 more with the stored T (the first block in the same frame). There are
  [0xEDD470] ? 128 : 256 rows. [0xEDD470] depends on the detail level: `fn_00823AD0` copies it from the table
  [0x9A38E0 + 4·level] = 1, 1, 0, 0, 0, 0, 0 (0x823C5B..0x823C69; `DetailLevel::skyNoBlend`), and `fn_0082A8E0`
  writes it again on its two exits (0x82AB1B / 0x82AB30). At levels 2..6 there are 256 rows in 8 frames, with
  blending. At levels 0 and 1 `fn_00869670` is false: no blending, 128 rows and tint by T (below, not ported).
  openblack blends 256 rows at all levels.
- `fn_0086B7F0`: T ≤ 1 → w = ftol(T·255) between day (255 − w) and dusk (w); otherwise, w = ftol((T − 1)·255) between dusk
  and night. `fn_0086B9A0` (555 path): per 5-bit channel `(c_inf·(255 − w)) >> 8 + (c_sup·w) >> 8`; the sum of
  weights is 255/256 (a channel of 31 comes out as 30; the callee masks 255 − w with 0xFF, 0x86B9F6..0x86BA03) and bit
  15 comes out as 0. Since each term is truncated separately, the dome
  comes out somewhat darker and with more banding than the previous linear blend: about −4/−5 per channel (out of 255) at noon and
  up to 40 % less in the dark channels at night (the night textures have values 1..4 out of 31). This is what
  the original does (tables 0xFA2554 / 0xFA2514, p = 255 − ftol(T·255) at 0x86B8E3): **do not «fix» it**.
- The texture (format 4) only receives the +0x138 mark when the block reaches the last row (0x86B95B..0x86B977); the
  unlock `fn_00838EB0` only uploads formats 1, 2 and 0x20 itself. The +0x138 mark means «dirty, upload on
  the next bind»: the `SetTexture` path of the LH3D texture (0x837EC6..0x837F74) checks it, locks the D3D
  surface (vtable +0x64, flags 0x821), converts the system copy with [+0x134] (0x837F19), unlocks, calls
  `IDirect3DDevice7::SetTexture` (+0x8C) and clears the mark (0x837F74); that is, in the `DrawSky` of the same frame.
  openblack uploads the 3 layers to the GPU only when the last row is finished, so the dome changes all at once after 8
  frames, like the original.
- openblack does it on the CPU, in `Sky::BlendDome`, on a copy of the 9 textures; `fs_sky.sc` no longer blends times,
  only alignments (layer 0 evil, 1 neutral, 2 good; `u_typeAlignment.x` is not used). How the original blends the
  alignment is **not read yet** (openblack keeps its linear blend of the two closest, **(inferred)**).
- The initial dome is built with the current T (the original uses the time from `fn_0086A3B0` with its thresholds
  4.5 / 7 / 7.5 / 8.25); the last time jump of `Open` (0x5E1D9C; the first is `fn_005576F0` 0x557702) and the
  `SetScriptTime(12)` of `Reset` in openblack rebuild it whole immediately (the last jump wins), **(inferred)** with no
  visible effect. `fn_005E22A0` also calls `fn_005E1DE0` (0x5E22D3, reads [0xBF3378]) after the jump: it is not sky
  type and it is not in `SetScriptTime`.
- Not ported: the no-blend mode (`fn_00869670` false: copy of the day textures and tint of the sky colour by
  T, 0x86B1C1..0x86B2A4), the 565 path ([0xEDD46C]) and the light table after `Jump` (openblack rebuilds it every
  frame).
- Order in `GGame::Load`: the jump (0x554B6F) comes before `SetVisualTimeCycle` (0x554C9E), with the previous thresholds;
  openblack does not load saved games.

Tests: `test_sky_type` (default thresholds, `At` with Demon God and with NaN, normalisation (also
−1e-7 → 0 at 24 bits), 1.2 double, evening ramp, column and haze, weights, 555 blend, hysteresis and rows (also the
subtraction rounded to float), jump from `SetScriptTime`).

## Night lights (report `night_visuals.txt`)

- **Windows** (`Abode::Draw` 0x515F70): the submeshes with the L3D `isWindow` mark are shown unlit, with a flat
  grey 224..252 that flickers slightly. It only happens if someone is at home (Abode +0xB6) and it is visual night. Each
  house has its own time offset: the fractional part of |x + z|·0.1 + y. openblack: `night_lights::WindowColour`
  (`src/3D/NightLights.*`), which gives the colour +0x54 0xFFgggggg or 0 with the windows off, and goes in the z of the fifth
  column of the instance (`argb_colour::PackInstanceWindow`, [rendering-objects.md](rendering-objects.md#the-object-colour-fields-in-the-instance)). Since openblack does not yet send villagers home, it counts "someone at home" as
  the house having inhabitants **(approximate)**.
- **Home of the script villagers**: when creating a villager with CREATE_VILLAGER, openblack gives it the house
  closest to the script's abode position (1 unit or less away) or, if there is none, any with room, and adds it
  to its inhabitants. Own rule, not checked against the original **(approximate)**.
- **Hand light and street lantern light** (done; `night_lights::Update`, called from `Renderer::UpdateClouds`):
  - Mechanism (`fn_008229B0`): an 8-bit image, remapped on load to min(47, v·48/255 + 0.5), is stamped onto
    the luminosity of the cells with a bilinear filter that the original shifts by one texel. v = r·trunc(I·255)/255. If
    v ≤ lum·((G255·48) >> 8) >> 8 the cell does not change (G255 is the green of table[255]). Otherwise, v replaces the
    normal land (≥ 48) or the maximum with another light is kept. The range 0..47 is the warm ramp of the light table.
    openblack writes it into the same R8 layer as the cloud shadows (the shader takes the minimum).
  - Only when the mean of the table's base is below 120.
  - Hand: `light_hand.raw` (12×12) from the position of the hand model − 55, with strength clamp((120 − mean)/15).
    No light if the hand is hidden.
  - Street lanterns: `CREATE_STREET_LANTERN` (type 7 = MSH_O_TOWNLIGHT, flames at +5) and bonfires (MSH_B_CAMPFIRE, +1).
    Stamps `village_diffuse.raw` (14×14) from pos − 50, with ±0.5 of jitter every 30 ms. Strength I/255 from
    `fn_0086C220` by script time: it turns on from 16.5 to 17.5 and turns off from 6 to 7 (on at 16.5 0xC395C4, off at
    7 0xC395C8, ramp 1 0xC395CC).
  - Lantern details: the jitter clock +0x10 starts at Random(0, 30) (0x8232FB); a flame's size Random(−0.1, 0.1) + 1,
    at least 1e-4 (0x823355..0x823382); the flames start at cell 0 (0x823397); new lights go at the head of
    [0xEB99B8] (0x8232D5..0x8232E7); every 30 ms (0x8234B6..0x8234FD, 1/30 [0x8CF3F8]) Random(−0.5, 0.5) for +0x14
    (0x823502) and +0x18 (0x823514) and the glow's size Random(−0.1, 0.1) + 3 (0x823526..0x823546); fn_007349E0(dark)
    plays the lanterns' loop only while dark (both branches of 0x5E5921); fn_00823690 stamps village_diffuse.raw.
  - Creation (`fn_00823240`) is **not gated by darkness**: 6 CRT draws in this order: the clock `Random(0, 30)`
    (0x8232FB), then for each of the 3 sprites (2 flames, 1 glow) the flame size `Random(−0.1, 0.1)` (0x823368, flames
    only) and the start `ftol(Random(0, 31))` (0x8233EE). Callers: `GStreetLantern::CallVirtualFunctionsForCreation`
    0x734905, `AnimatedStatic::CallVirtualFunctionsForCreation` 0x422565 (the Norse gate's lamps) and
    `CitadelHeart::DrawLantern` 0x4666E2 (2 lights, lazily, on the first draw with +0x9C > 0.07).
  - The darkness gate: `fn_005E5830` runs once per drawn frame from `GLandscape::Draw` 0x5E488E. With the mean of
    the R, G, B of [0xFA26A4] < 120 (0x5E5921) it calls, in order, the jitter `fn_00823460`, `fn_0086D360`, the village
    alpha [0xD20184] = clamp((120 − mean)/15), `fn_007349E0(1)`, the hand light `fn_008229B0` (if the hand is shown)
    and `fn_00823690`; otherwise (0x5E5A20) [0xEB99BC] = 0, `fn_0086D360` and `fn_007349E0(0)`. 0x5E592A is the only
    caller of the jitter, so its 3 CRT draws per light per tick (newest first) are **not made while it is light**. The
    time of day only drives the brightness (`fn_0086C220`), not the gate.
  - raffclar's tree: 6 creation draws in this order at creation, and the jitter only when dark. Not checked: whether
    his `LandLightTable::GetLandColour` equals [0xFA26A4] bit for bit, and his registry order against "newest first".
  - Additive sprites: two `S_Fire` flames (frames 0..31 backwards in 700 ms with an integer global clock, which only
    runs with alpha ≠ 0; flame i starts at the global table 0xC383BC, which every new light rewrites with
    ftol(Random(0, 31)) (fn_00823240 0x8233E4..0x8233F8), so they all run in phase with the last one created;
    fn_00823570, `frame_anim::LanternCell`; half-size 1 ± 0.1) and a halo
    `smoke` cell 56 ((flags & ~7) | 0x38, 0x8233BC..0x8233EB; half-size 3 ± 0.1). Colour 0xF38421 with alpha trunc(I/2).
  - Approximate: the sprite shader only uses the alpha mask, without the texture's RGB.
  - The additive quad of the hand over the water (±60, `atmos.raw`) is done (`src/3D/HandWaterGlow.*`, see
    [rendering.md](rendering.md#sky-sun-moon-and-clouds-original)). The two lights of the Norse gate
    (MSH_O_TOWNLIGHT at (±15, 30, 0) of the gate) are done (see [map-loading.md](map-loading.md)).
- **Street lantern sound** (done; `src/Audio/LanternSounds.*`, one `audio::tags` per lantern,
  [audio.md](audio.md#soundtag)):
  - **Creation**: `GStreetLantern::CallVirtualFunctionsForCreation` 0x734810 creates a `SoundTag` (`fn_0071E8C0`,
    +0x60) with offset (0, `Object::GetHeight` 0x638120, 0), sample 0x93, 3D, mode 2, loops −1, bank 1
    (`Audio\SFX\Game\InGame.sad`) and then `SetActive([0xDA0A10])`. **Both lantern classes** have it (it does not check
    +0x58); the object with the UNAVAILABLE mark (+0xA & 1) does not have it.
  - **Sample**: 0x93 = LH_SAMPLE_G_LANTERN_01 (147) = `G_Lantern_01.wav`, 22050 Hz, ~4.1 s, priority 200, override
    marks 0x7C0: loops −1, minDist 3, maxDist 5, scale 4, mode 2. No volume or pitch mark, so
    volume 127 (gain 1) and pitch 100 % with the usual ±15 % deviation. Mode 2 = one channel per lantern: if it is already
    playing, it does nothing.
  - **Day/night switch**: `fn_007349E0(on)` stores [0xDA0A10] and does `SoundTag::SetActive` on every lantern. Only
    caller `fn_005E5830` (`GLandscape::Draw`, every frame): 1 when the mean of the light table's base is
    **< 120** (the same test as the hand and the village lights, with no time test), 0 otherwise. `SetActive(0)` calls
    `StopPlayingSoundEffect`: abrupt cut.
  - **Per turn**: `SoundTag::ProcessSoundTags` 0x71E5F0 from `GGame::EndTurn`; each active tag calls
    `GAudio::PlaySoundEffect` 0x42A100 with (x, altitude + y, z) plus the offset. 0x429E30 **does not start** the sample
    if the camera is more than maxDist (5 units) from that point; once started, the loop continues wherever the camera goes.
    If the lantern stops being available, `CreateSoundTagForDeadObject` calls `LHSampleReleaseLoop`: the current pass
    finishes and the tag is deleted.
  - openblack: `audio::lantern_sounds::SetOn` from `night_lights::Update` (the same mean < 120),
    `ProcessTurn` in the turn block of `Game.cpp` and `Clear` together with `night_lights::Clear` on map load (before
    `Registry::Reset`, which would delete the emitter entities without releasing the OpenAL source and the loop would keep
    playing). The emitter is created with `PlayType::Repeat` and released by setting `PlayType::Once` on it (= `LHSampleReleaseLoop`:
    `AudioManager::Update` removes its `AL_LOOPING` and destroys it when the pass finishes).
  - **Not ported** (unverified): QMixer's attenuation curve for {min 3, max 5, scale 4}
    (`QSWaveMixSetDistanceMapping`, it is in the external DLL). openblack keeps OpenAL's default model as in
    all 3D sounds; a general audio change could set `AL_REFERENCE_DISTANCE` / `AL_MAX_DISTANCE` per
    source from the `.sad`, but only after deciphering QMixer.
  - Because of the 5 units in the data, in normal play it can only be heard with the camera almost on top of the lantern.
  - Checked in game (`OPENBLACK_LANTERN_SOUND_TRACE=1`): with `OPENBLACK_TIME_OF_DAY=22` and the camera at the
    Land1 village lantern `2493,2534` it starts at 1.7 units and does not repeat (mode 2); at the field lantern `1365,2571`
    it starts the same way (both classes); at noon it never starts; when changing map (`OPENBLACK_TEST_MAP_CYCLE`) the cut of
    `Clear` does not leave the loop loose. The night → day transition **could not be triggered in game**: the Land1 CHL script sets the
    script time to 7.3 every turn (above the darkness threshold, which is between 7.2 and 7.3), so
    `OPENBLACK_TEST_MOVE_TIME` does not move it and `OPENBLACK_TIME_OF_DAY` pins it without a transition; the abrupt cut was verified
    through the same code path (`Clear`).
- **Fireflies** (done; `src/ECS/FireFlies.*`):
  - When: with visual time > 12 and sky type > 1 up to 50 appear, half on trees and half on random
    rocks. Each turn one flies to the closest house or lantern less than 300 m away, at +(height + 2). In the morning
    each turn one goes back to a tree or a rock.
  - Flight: it lasts distance / (3·speed) with smoothstep easing.
  - Drawing: orbit of radius 8 plus a jitter of radius 1. Sprite 37 of `S_SpriteSheet3`, half-size 0.3, alpha 190,
    which fades out between 100 and 300 m.
  - Approximate: the spiral search with 50 % per cell is reduced to "the closest one that passes a coin toss".
- **Sound**: `GSoundMap` blends the day and night ambience according to max(0, sky type − 1) (done, see
  [objects-and-resources.md](objects-and-resources.md#sounds), "Ambience"); the
  sounds of the houses are chosen by the sky type (pending).
- Gameplay only, left out for now: lions and wolves go to their den (22 / 23 h), children leave the
  crèche, the food desire and the creature's desires.

## Climate

The game weather (climates, storms, rain and its drawing) is ported and described below, in
[Weather and climate](#weather-and-climate-srcecsweather). What remains here are the sky notes from the first reading.

- Sky clouds (`CloudInSky`): done, see [rendering.md](rendering.md#sky-sun-moon-and-clouds-original) (placement, colour by time and alignment,
  sky alignment smoothed 0.001/ms). What depends on the weather goes through `Clouds::WeatherOvercastAtCamera()`
  (byte 3 of `weather::atmos::GetWeatherSmooth` at the camera × 0.01; 0 with a clear sky).

Water and weather: **there is no wet ground** in the original; the sea only changes with the weather through the overcast cap of the
light table and the lightning flash (`LandLightTable::Build`). The overcast has a single source,
`Clouds::WeatherOvercastAtCamera()` (reads `weather::atmos::GetWeatherSmooth` at the camera); `3D/SkyWeather` only gives the flash (`weather::LightningFlashAtCamera` from
`ECS/Weather/LightningFlash` at the camera position).
- **Storm clouds**: `GClimate::CreateStorm` 0x772E00 → `GWeather` 0x83F590 → `DrawClouds`
  0x83FC90, a group of up to 16 balls (8 by default, `CHANGE_CLOUD_PROPERTIES` changes number, blackness and height) at
  lx, lz ∈ −1..1, ly ∈ −10..10 + height; every 400 frames a new target (ly 0..20 + height), 1/400 of the way
  per frame; world X = R·lx/2 + cx, Z = R·lz/2 + cz, Y = ground + ly with R = radius(t) + radius2; size R·2·Random(0.01,
  0.015); colour [0xFA26A4]·(1 − 0.5·blackness), alpha·intensity·0.75, with haze; they are not drawn with alpha ≤ 5. They are the
  only cloud groups in the original. Already ported: [The flash and the clouds of the registered storms](miracles.md#the-flash-and-the-clouds-of-the-registered-storms-ecsweatherlightningflash-stormclouds).
  - A puff's colour scales each RGB byte by (1 − 0.5 × blackness) only when the blackness is above 0
    (0x83FF56..0x840000).
  - The puffs get the haze of fn_007FEB30 with a grey specular min(255, ftol(flash f3 [storm +0x8C] × 127 [0x8C4A00]))
    in all four bytes (0x83FFEE..0x840027), put in +0x50 by SetColorSpecular and drawn by the mists' effect branch: the
    storm's own flash lights its clouds.
  - The storm's shadow s = (blackness + 0.7) × fade is stamped with 1 when s ≥ 1, and otherwise only when s > 0.01
    ([0x8C5840]); only with the "CloudShadows" option [0xC381F4]: fn_0086CFF0(+0xA0, 0xEE9D3C, 40, 1, s, 2, 0)
    (0x840069..0x8400C6).

The weather items first read here (`GClimate`, `LH3DAtmos::Render3D` with rain and snow by 80×80 tiles, lightning,
overcast weather in the light table and the haze) are described in [Weather and climate](#weather-and-climate-srcecsweather);
what is left is in [Pending](#pending).

## Weather and climate (`src/ECS/Weather`)

Two layers, as in the original: **LH3DAtmos** (the 3D engine) keeps a grid of the weather made by the registered
*storms*, and **GClimate** (the game) adds the temperature and wind of the *climates* to it and creates natural storms.
All queries from the rest of the game go through `GClimate::ComputeWeather`.

> **Code rules.** The weather state (grid, storms, climates, weather objects) lives in ECS components or Locator
> services, not in globals; the info rows and textures load through the resource caches; the byte arithmetic and the
> formulas are pure functions tested with fakes in `test/`; comments describe behaviour in plain English, with
> no decompiled names or addresses (those belong here). See [the conventions](../refactor/README.md).

- `Weather.h` / `WeatherQueries.cpp`: the queries (the small header that the rest of the game includes).
- `WeatherInfo.h`: the 8-byte structure and its byte arithmetic.
- `Atmos.{h,cpp}`: LH3DAtmos, the grid of 128 × 128 cells of 40 m.
- `Storms.{h,cpp}`: LH3DStorm / GWeather, the registered volumes.
- `Climate.{h,cpp}`: GClimate, `ComputeWeather`, `ProcessAll`, the natural storms.
- `Calendar.{h,cpp}`: the date part of GGameInfo (day of the year, month, season).
- `WeatherThing.{h,cpp}`: the CHL weather objects.
- `Rain.{h,cpp}` + `Graphics/RendererRain.cpp`: the drawn rain.
- `WeatherLoop.{h,cpp}`: the calls from the turn; `WeatherDebugHooks.cpp`: the test hooks.
- Scripts: `Magic/Script/MapScriptWeather.cpp` (map commands) and `Magic/Script/CHLWeather.cpp` (CHL natives).

### `WeatherInfo` (8 bytes, `LH3DAtmos`)

It is returned in `edx:eax`, and the same layout is a grid cell, the tail of an LH3DStorm (+0x48) and the
result of `ComputeWeather`: `temperature` (degrees), `rain` (0..100), `snow`, `overcast`, `windX`, `windZ`
(× 1/8 = m/s), `snowCover` and `stamp` (the frame in which the cell was computed).

The arithmetic is on signed bytes, and the original mixes two forms: the temperature **wraps** (`add cl, al`,
`WrapAdd`) and the rest is **clamped** to -128..127 (`ClampAdd`). The interpolation is `a + ((b - a) × w >> 8)` with
arithmetic shift (`LerpByte`).

### LH3DAtmos: the 40 m grid (`Atmos.cpp`)

- 128 × 128 cells of 40 m (0xEDC350) = 5120 m, exactly the map. Outside the grid the *ambient weather* is used
  (0xEDC348), which in a game is all 0 (it is only written by `LHInetWeather` and the saved game).
- A cell is recomputed **on demand** (`fn_00834EE0` / `fn_00834E20`) when its `stamp` is not the current frame
  (0xEDC340): it starts from the ambient weather and adds each registered storm at the **corner** of the cell
  (`ix × 40, 0, iz × 40`), not at the requested point. That is why the rain comes out in 40 m steps.
- The grid 0xEDC350 (0x20000 bytes) is allocated by fn_00835AD0, which also loads `sstorm.raw` (40 × 40 grey, 0x640
  bytes) into 0xEE9D3C at 0x835E15: the storms' land shadow.
- A cell recompute (`fn_00834E20`) starts from the ambient weather (`fn_00834E10`), applies each storm (vt+0xC, list
  [0xEEA37C], next +0x3BC), stamps [0xEDC340] into byte 7 (0x834EAD..0x834EB8) and then **overwrites** byte 6
  (snowCover, 0x834EC5; it is not added) with `fn_00834DD0(corner)` = clamp(ftol(`fn_0086CA80`(x, z) × 0.5), −128, 127)
  (0x834DE4..0x834E04; `ftol` truncates towards zero), 0 without [0xEDC344]. `fn_0086CA80` (reached through
  `fn_0086CB80(p)`) filters the 128 × 128 float grid bilinearly (cells of 40, × 0.025; 0 outside [0, 127) in x or z).
  `SnowCover::Init` 0x86C960 zeroes it (0x10000 bytes) and loads `data\WeatherSystem\snowmap.raw`, with no draws.
  raffclar's tree (`WeatherSystem.cpp`) has the filter, the bounds and the overwrite, but **not the × 0.5**: its
  lying snow is twice as deep in the grid (tornado dust `>= k_SnowDust` and the debug "Lying" bar; `IsSnowCoveredAt`
  is unaffected).
- The cell lerp fn_00835620 (and the smooth sampling) lerps bytes 0..6 with a wrapping add and keeps the stamp of the
  first cell.
- A new land (`InitStaticsValues` 0x54A8CC..0x54A907): every storm deleted (0x54A8D1), the grid and the ambient
  weather zeroed, the frame back to 1.
- `GClimate::ComputeWeather` 0x771640 is reached through `GClimate::GetWeather` 0x771490; only the camera asks for the
  smooth sampling.
- `LH3DAtmos::UpdateGame` 0x8356E0 (the first call of `GGame::ProcessTurn`, with the visual time and 0.1 s) updates
  the storms and advances the frame; on overflowing 256 it clears all stamps and goes back to 1. The sun position that
  it also computes (0xEDD378: 4000, cos(t·π/12) × 1100 − 150, sin × 800) is for lighting, it is not used here.
- `LH3DAtmos::GetWeather` 0x834F80: the cell of (x, z) and then the height: **above 50 m the temperature drops
  0.075 per metre**, and above 200 m it is a fixed −11 (`add al, 0xF5`).
- `LH3DAtmos::GetWeatherSmooth` 0x835180: bilinear between the four cells in steps of 1/256; above 200 m
  it tends towards the ambient weather with weight `(height − 200) / 4` (cap 256) and then applies the same drop with height. It is the
  one used by the camera (clouds) and `GetWindAt(p, true)`; all the GClimate getters ask for the non-smoothed one.
- `SnowCover` (0xEDC344, the snow accumulated on the ground, `fn_0086CB80 × 0.5`) **is not ported**: `snowCover` stays 0.
  With no snowing storm the grid stays 0, so porting it with the × 0.5 and the truncation is identity.

### Storms: LH3DStorm and GWeather (`Storms.cpp`)

The LH3DStorm descriptor (0x50 bytes, constructor `fn_0083F3F0`) comes by default with inner radius 100, outer 300,
fade 10 s, life 100 s, strength 1, 8 clouds, blackness 0.5, elevation 160, fall speed 1, no lightning, and as
weather 10 degrees / rain 100 / overcast 100 / wind (10, 0). The GWeather object (0x3C0, list 0xEEA37C, the newest
first) copies it and adds age, destination, speed, deletion counter, lightning timers, drawing
position, radii and fade.

- `GWeather::Update` 0x83F900 (from `fn_0083F840`, once per turn with 0.1 s):
  - when `lifeTime` passes it is marked for deletion;
  - the **fade** rises from 0 to `strength` in `fadeInTime` and goes down the same way at the end, and the **inner radius** follows it
    (the outer one does not);
  - it moves towards its destination at `speed` m/s in x,z;
  - it only moves when the distance to its target is above 0.001 ([0x8AA3B0], 0x83FA11);
  - the lightning, once faded in: `forkTimer` and `sheetTimer` count down and on reaching 0 they are reloaded with
    `min + rand(max − min)`. The fork one creates a lightning PSys (0xEEA384) and the sheet one plays the thunder (0xEEA388);
    here they are two *callbacks* (`SetForkCallback` / `SetSheetCallback`), still empty. The flash
    (`fn_00837290`, `Storm::flash`) is in [The flash and the clouds of the registered storms](miracles.md#the-flash-and-the-clouds-of-the-registered-storms-ecsweatherlightningflash-stormclouds) (`ECS/Weather/LightningFlash`).
- In the original the lightning callbacks are function pointers set by the engine: fork 0xEEA384 = 0x68E8F0 (from
  `PSysGlobal::InitializeOneTimeOnly`, a PSys strike), sheet 0xEEA388 = 0x429CE0 (GAudio, the thunder). Both get the
  storm, the point (storm x, z, land height + elevation) and the outer radius.
- The lightning flash bitmap 0xED92F0 is built once for all flashes with a refcount at 0xEDD3A8 (values capped at 255
  inside d < 32).
- A new storm: fn_0083F6F0 → fn_0083F590 puts a new GWeather at the head of the list with speed 1 and target = its own
  position. The GWeather ctor fn_0083F4E0 leaves the speed (+0x68, metres per second towards the target) at 0.
- fn_0083F8D0 answers whether a storm is still in the list and not marked for deletion (the original keeps a GWeather
  pointer and asks it); the deleting destructor is vt 0x10, after fn_0083F630 unlinks it (and deletes its cloud
  mists).
- **Deletion over two turns**: `fn_0083F7B0` only marks (+0x94 = 1); the counter rises on each `UpdateAll` and the storm
  is destroyed when it goes past 2. While it is marked it no longer contributes anything to the grid. `GClimate::ToBeDeleted` 0x7713E0
  does destroy them instantly.
- `GWeather::CalcAtmos` 0x8400E0: if the point is in the box and the circle of the **outer** radius, the weight is 1 inside
  the inner one and goes down linearly to 0 at the outer one; `w = weight × fade × 256`. With `w == 0` it does nothing. The
  temperature **tends** towards the storm's (`LerpByte`) and rain, snow, overcast and wind are **added**
  (`ClampAdd(x, (value × w) >> 8)`).
- `fn_0083F750` (`KILL_STORMS_IN_AREA`) marks the storms whose 2D distance to the point is less than radius + their outer
  radius.

### GClimate: the climates (`Climate.cpp`)

List `g_game+0x205CF4`, the newest first; the **world climate** (id 0, `g_game+0x250534`) is at (2560, 2560)
with radius 5120 and type `WORLD`. `ComputeWeather` creates it if it is missing, so in practice it always exists.

- They are created by the map script with `CREATE_WEATHER_CLIMATE(id, info, "x,z", r1, r2)` (`fn_00771300`, case 60): id 0 makes
  the world one (replacing the previous one and ignoring the rest of the arguments), any other id a local one with the radii
  sorted and `maxStorms = int(r2 × 0.001 + 1)`. `CREATE_WEATHER_CLIMATE_RAIN/TEMP/WIND` then set the rest.
  **Land1 has four** (lines 2270..2285 of `Scripts\Land1.txt`): the world one with 10.8 degrees and wind (24, 0), two
  warm ones of 37.8 degrees at (2157, 2425) and (1740, 3145), and one **very cold one of −35.2** at (2701, 2568) with wind (40, 0).
- `GClimateInfo` (7 rows of info.dat) gives per season `rainMin/Max`, `tempMin/Max` and `windMin/Max`. The season comes from
  `GGameInfo::GetSeason`: 0 spring (day 79), 1 summer (171), 2 autumn (263), 3 winter.
- **Temperature** (`fn_00773ED0` / `fn_00773F40`, every turn): the target is
  `hourFactor[hour] × monthFactor[month] × (max − min) + min`, with the tables 0xC249D8 (0.5 at 0 h, 1.5 at 16 h) and
  0xC249A4 (1.0 in July, 0.1 in January; **February is 0**, as in the exe). The temperature approaches the target by
  `|int(target)| × 0.1` degrees per turn, so it **never converges: it oscillates** around it (in Land1 the script stores 10.8
  with target 12).
- **Rain** (`fn_00773D60`, once per game day): if it is already raining it only counts the days; if not, the desire grows
  with two rolls of `rainMax × 0.02` and is clamped to 1. The month, hour and nature terms of `GClimateRainInfo`
  are 0 because that table **is not in info.dat** (0xDCB8D0 stays zeroed).
- **Wind** (`fn_00774AA0`, per day): from the season's band in the `windAngle` direction. The weight is
  `int(exp(−((desire − 0.5) × 5)²))`, which is only 1 with a desire of exactly 0.5, so in practice it is always
  `min + max`.
- **One game day** is 36000 turns per year / 365.25 = **98.56 turns** (9.9 s). The land starts on 5 May
  1998 at 18:05:30 (`GGameInfo::GGameInfo` 0x557730), that is, on day 125.75 of the year.
- **Calendar**:
  - `_SecondsInDay` 0x8DF8E0 = 86400 and `_NumDaysInYear` 0x8DF8DC = 365.25.
  - The month lengths 0xBEC948 are made cumulative by the `GGameInfo` ctor 0x557730; **October has 30 days** in the
    original, 364 days in all.
  - `GGameInfo::GetDaysFromStart` 0x557940 = (turn + start) × (+0x18) / 86400, without the whole years of the start
    date (the day of the year is the same, they are multiples of 365.25 days).
  - fn_00557960: the month 1..12 (the first whose cumulative day count is above the day of the year; 12 past the last);
    fn_005579C0: the day of the month from 0.
  - fn_00557810 builds the start from `SetStartDate(1998, 5, 5)` and `SetStartTime(18, 5, 30)`: the 120 days before May
    from the table, the day of the month (5, added whole), then hours, minutes and seconds, in turns. The map commands
    that change the date (GSetup 0x714D57 / 0x714D81) are not used by the original lands.
- Climate fields: +0x68 storm elevation 500 (ctor 0x77113E, fn_00771170 0x7712C6); +0x6C fall speed = windMax[season]
  × 1/30 [0x980518] + 0.5 [0x8AB260] (0x77114B); +0x70 = 1.0, meaning unknown (0x771137, 0x7712B6); +0x74..+0x80 the
  lightning ranges of its hot storms, 5 and 60 s (0x77111B..0x771134, 0x7712A6..0x7712C3).
- A local climate starts from its info: fn_00773D30(rainMin, rainMax) sets the desire to rainMax and the raining days
  to int(rainMin × 100); fn_00774A90 does nothing, so there is no wind until the script sets it.
- Script setters: `CREATE_WEATHER_CLIMATE_RAIN` 0x717250 → 0x773200 (id 0 = the world climate, made if missing);
  `_TEMP` 0x7172A2 → 0x773290 writes +0x44 / +0x48 and reads the world climate **without making it** (id 0 needs it to
  exist already); `_WIND` 0x7172E0 → 0x7732D0 writes +0x4C..+0x54.
- Globals: last day 0xDCB8B8, last season 0xDCB8BC, the season the climates use 0xDCB8C0 (changed on the first day of
  a new season), next id 0xC2475C. A new land (`InitStaticsValues` 0x54A829..0x54A849): no climate, the climate system
  and storm creation on, day, season and id counters back to 0 / 0 / 1.
- `GClimate::ComputeWeather` 0x771640:
  1. the grid at the point (smoothed or not);
  2. the temperature and wind **of the world climate** as the base;
  3. **plus each climate in the list, the world one included again**, with the radius weight (1 inside the inner one, 0 at
     the outer one). It is not a copying oversight: the original reads 0x250534 and then walks the complete list, so
     **the world climate counts twice**. That is why Land1 gives 20 degrees on the plain (9 + 9) and not 10.
  4. the result is added with clamping to the grid bytes.
- `GClimate::ProcessAll` 0x771BE0 (turn, after the scripts) and `fn_00772330` per climate: updates the temperature;
  its storms drift with the grid wind (× 0.01 m per turn); on a new day, a storm that left
  its climate (or, for the world one, that entered any climate) jumps to the fade-out stretch and the desire goes back to 1; and if
  it has already rained enough this season it also fades out, and the desire starts again (fn_00773D50). Then, on a new day, rain and wind, and with desire 1 a
  new storm.
- `GClimate::CreateStorm` 0x772E00: `FindWhereToCreateStorm` 0x772BE0 chooses the place (the world one, a random 10 m cell
  out of the 512, retrying up to 20 times while it falls in another climate and off the island; a local one, at
  `r² × innerRadius` in a random direction). The size is `rand(1000)` (world) or the distance to the centre, clamped
  to 160..900; the outer one is `int(size × 1.1)`; the life `(rainMax × 100 − rainingDays) × 10` s with a minimum of 20;
  the elevation 500. It is placed with fn_0083F4A0(pos, inner, outer) at its 10 m cell at a height of 300
  (0x43960000, 0x772F5B) and fades in over 10 s (0x772FAF). The distances of `FindWhereToCreateStorm` (0x772D86 /
  0x772D97), `CreateStorm` (0x772ED6, truncated) and fn_00772330 (0x7724E2 / 0x77254C) are
  `GUtils::GetDistance(LHPoint, LHPoint)` 0x74CDE0. The blackness is `exp(−((t − 30) / 15)²)`, and above 30 degrees it is an electrical storm with the
  climate's lightning. **Below 0 degrees it is pure snow**; above, the snow part is `exp(−(t × 0.2)²)`
  (all rain beyond about 10 degrees) and the rest rain.
- `PAUSE_UNPAUSE_CLIMATE_SYSTEM` and `PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM` (0xC24759 / 0xC24758) switch off the
  update and the creation. The natives are 0x6FF4E0 (it gates the temperature, rain and wind updates) and 0x6FF500.

### Queries (`Weather.h`)

All of them on `ComputeWeather` at an LHPoint (x, z in metres and the **absolute height** in y, which is what lowers the
temperature):

| Query | Original | Note |
|---|---|---|
| `GetMaxRainingOrSnowingAt` | 0x771600 | `max(rain, snow)`; fire cools with `1 + 0.01 ×` this |
| `GetRainAt` / `GetSnowAt` | 0x771570 / 0x7715B0 | 0 as long as there is no world climate |
| `IsRainingAt` / `IsSnowingAt` / `IsSnowCoveredAt` | 0x7714B0 / 0x7714F0 / 0x771530 | the byte > 0 |
| `GetTemperatureAt` | `GClimate::GetTemp` 0x771A80 | it is **not** the ambient temperature of fire, which is the constant 24.7 of `MapCoords::GetTemperature` 0x605CC0 |
| `GetWindXAt` / `GetWindZAt` | fn_00771AB0 / fn_00771AE0 | the bytes |
| `GetWindAt` | fn_00771B10 | `(windX/8, 0, windZ/8)`; read by fire, `UR_CloudMoverNew` and the bounce of the fireballs |

The first five check beforehand that the world climate exists (they return 0 / false if not); the others create it.

### CHL weather objects (`WeatherThing.cpp`)

`CREATE` of a `SCRIPT_OBJECT_TYPE_WEATHER_THING` (type 15) creates a WeatherThing with a storm made from
`GWeatherInfo[subtype]` (inner radius 100, outer 300, life 100 s, clouds at 500, and the temperature, humidity, snow,
overcast and wind of the row as bytes). It keeps a **copy** of the descriptor; the natives edit it and `UpdateStats`
0x774370 dumps it into the storm (reloading the lightning timers): `CHANGE_WEATHER_PROPERTIES` (123),
`CHANGE_LIGHTNING_PROPERTIES` (124), `CHANGE_TIME_FADE_PROPERTIES` (125), `CHANGE_CLOUD_PROPERTIES` (126).
`WeatherThing::ProcessWeatherThings` 0x7741A0 runs every turn: if its storm is no longer there it forgets it, with
`SetAffectedByWind` the storm drifts with the wind of `ComputeWeather` × 0.01, and the object moves with it.

`CREATE_WEATHER_STORM` from the map script (case 0x717328) creates a storm of a climate. Its third string
(`"overcast,snow,temp,rain,windX,windZ"`) is read with `%d` **over the bytes** of the descriptor, 4 bytes per value,
so each value overwrites the next three: only temperature, rain and wind survive (snow and overcast end up as
0 or −1). No original land uses it.

- `WeatherThing` is 0x88 bytes, in the list g_game +0x205C54 (next +0x80), newest first; its descriptor copy is at
  +0x28; +0x7C is `SetAffectedByWind` 0x55DF20.
- CHL `CREATE`: GScript 0x6F151E → 0x7741F0 → ctor 0x774030(pos, 100, 300, 100, &GWeatherInfo[subtype]); fn_00770EA0
  writes the info's temperature, wetness, snowFall, overCast and wind as the storm's bytes at LHPoint(pos, land + y);
  fn_00770F10 makes the storm at once as a 0x3C8 object (fn_00770E50 sets its +0x3C0 to 0).
- `WeatherThing::Process` 0x774230 returns 1 while it has a storm; the drift is fn_00771A30 (`ComputeWeather`'s wind at
  the storm × the factor); the thing copies the descriptor back. A thing whose storm is gone would be deleted if no
  script held it (openblack keeps it). `ProcessWeatherThings` runs after `GLandAlignement::UpdateTime`.
- `ProcessWeatherThings` sets 0xD016CD to 1 while some thing has a storm and to 2 when none (its reader is not ported).
- Natives: fn_00774400 `CHANGE_WEATHER_PROPERTIES` (temperature in whole degrees; rain, snow and overcast × 100; fall
  speed), fn_00774460 `CHANGE_TIME_FADE_PROPERTIES` (lifeTime, fadeInTime), fn_00774500 `CHANGE_CLOUD_PROPERTIES`
  (blackness, number of clouds as an int, elevation), fn_00774520 `CHANGE_LIGHTNING_PROPERTIES` (sheet and fork
  ranges).
- `WeatherThing::SetMovement` 0x774480 writes the wind bytes as clamp(v × 8, −127, 127); fn_00774550 / fn_00774580 set
  the storm's target and speed.

### The drawn rain (`Rain.cpp`, `Graphics/RendererRain.cpp`)

`LH3DAtmos` has a rain object (`fn_00833DA0`) with **128 streaks** of 0x1C bytes. Each streak (`fn_00833D10`) is a
line from `(x, −50, z)` to `(x + dx, elevation, z + dz)` around the centre of the tile, with `x, z` in ±80, the
slant `dx, dz` in ±15, a texture offset 0..1 and a speed of 0.1..0.2 turns per second. The u goes
from the offset to offset + 1 (the texture repeats across the width) and the v is fixed, 0x3F010000 = 0.50390625, the
row 129 of `Data\Textures\atmos.raw`, which is a row of white strokes.

- `LH3DAtmos::Update3D` 0x8357A0 (per frame): the storm **closest to the camera** sets the elevation and the
  fall speed (160 and 1 with no storm); both approach by 0.3 of the way per frame and are clamped to
  40..640 m and 0.3..5. Then, *if it was drawn in the previous frame* (0xEDC300), the streaks advance: the phase rises 2.4
  per second and on overflowing the streak is placed again.
- The 128 streaks (0x1C bytes each) are at 0xEDC358; the elevation is 0xC38E10 (160 at start) and the fall speed
  factor 0xC38E14 (1 at start).
- fn_00833D10 places a streak with 7 draws of `Random` 0x81D180 (the CRT `rand()`, not GRand) (0x833D1D..0x833D8F);
  the step fn_00833EA0 makes 4 more draws per reborn streak (0x833F23..0x833F5F).
- The streaks are placed **once per process, twice**, not per land: the `LH3DAtmos` init `fn_00835AD0` (from
  `start_system` 0x6433C2 with flags = −(Weather != 0), [0xC381FC] = 1 by default, read from the "Weather" key at
  0x823A0C; also `DoCitadelMultiplayer` 0x5555A8 with flags −1) calls `fn_00833DA0` at 0x835DA8 (the constructor)
  and again at 0x835DC4: 2 × 128 × 7 = 1792 CRT draws at start-up, after the snow's (below). Land load does not reach
  it.
- `LH3DAtmos::Render3D` 0x836250: for each land block, two grid samples (the centre of the block and the
  centre + 40) and the largest of their rain and snow bytes; if it exceeds 5, it queues an object in the Z-sorter with the three
  low bytes of its user data = `(x/80, z/80, value × 88 / 100)` (`fn_008341B0`). When flushing the Z-sorter
  (`fn_0082F280`) the callback `fn_00833F80` unpacks them and calls `fn_00834370(x, z, 0, 128, alpha)`. That is why there
  is a single tile per block, centred on its origin + 80.
- The tile alpha in `Render3D` is 88 × max / 100 (imul 0x51EB851F, 0x836431..0x83645D); fn_008341B0 clamps it to 0xFF
  (0x8341B8) and drops only a negative one (0x8341CC).
- `fn_00834370`: nothing beyond 400 m from the camera; between 100 and 400 the alpha and the number of streaks are multiplied by
  `1 − (d − 100) / 300`. The bottom alpha is that, and the top one `alpha / ((2d/400 + 1) × 5)`, so the streak
  fades towards the cloud. Each streak is also attenuated by its phase: below 0.05, `phase × 20`; above 0.95,
  `(1 − phase) × 20`. The material is `AtmosMaterial` in mode 6 (alpha) with Z test and no Z write.
- With the value above 0x2C the original also calls `g_water_drop_cb` for the drops that splash on the ground:
  **it is not ported**. Neither is the drawn snow (below) nor the lightning flash.
- In openblack the streaks are one-pixel lines with the `WorldQuad` program (`atmos.raw` + `atmosa.raw`) in the
  `MainBlended` pass. `rain::Reset` places the 128 streaks (896 CRT draws) at every `OnLoadMap` instead of 2 × 896
  once at start-up (see [Pending](#pending)).

### The drawn snow

Not ported. 256 flakes of 0x34 bytes at [0xEDC354], only when the "Weather" setting is on (`fn_00835AD0`,
`test [flags], 2` at 0x835D27).

- Placement: `fn_00834890` (0x834890..0x8348BA), per flake `fn_00834700` (`Random(−80, 80)·0.5` 0x834710,
  `Random(0, [0xC38E10] = 160)` 0x834727, `Random(−80, 80)·0.5` 0x83473A, then `fn_008345F0` with 10 draws
  0x834611..0x8346D0: fall `(0,1) + 7`, sway speed `(0,10) − 5`, heading `(0,2π)`, sway `(0,π)`, spin `(0,2π)` ×3, spin
  speed `(0,π/2)` ×3), then y = `Random(−20, 160)` into +4 (0x8348AA): **14 CRT draws per flake, 3584 per call**.
- The init calls it **twice** in a row (0x835D51 as the constructor, then 0x835D6D on the same object, [0xEDC354] set
  at 0x835D67): **7168 CRT draws at start-up**, before the rain's 1792. Land load does not reach it.
- Per frame (`Update3D` 0x835AA0..0x835AB9): only if a snow tile was drawn in the previous frame ([0xEDC2FC], set
  at 0x836627), the step `fn_008348C0` clears the flag and moves the flakes: a flake at y ≤ −20 is placed again by
  `fn_00834700` (13 draws), and `fn_00834770` adds `Random(0, π/4)` to the heading each time the sway passes 2π
  (0x8347B9). With no snowing storm there are no per-frame draws.
- Drawing (`Render3D` 0x8365CD..0x83665C): for a block whose largest snow byte (offset 2) is > 5, `fn_00834900`
  (prepare the flakes) once per frame, then `fn_00834290` queues the tile with alpha snow·256/100 and the Z-sorter
  callback `fn_00834120` (0x834354). `fn_00834120` is the snow tile's **draw callback, not a scatter**: it unpacks
  [0xEE9D30] (x/80, z/80, amount), calls `fn_00834BF0` when the amount is > 5, and makes no draws.
- raffclar's tree (`SnowfallSystem.cpp`, `3D/Snowfall.cpp`): the same 13 placement ranges plus `(−20, 160)` in
  `Scatter` (14 per flake; the "13 × 256 = 3328" count is short by one per flake), and the step gated like
  [0xEDC2FC]; but it scatters **once**, at Locator emplace, where the original does it twice.

### Order in the turn (`WeatherLoop.cpp`)

In the steps of `GGame::ProcessTurn` 0x54E5C0 ([engine-loop.md](engine-loop.md) §2): `ProcessTurnStart` from
`Magic/MagicLoop.cpp` (`LH3DAtmos::UpdateGame`, 0x54E5D7); `ProcessWeatherThings` (0x54E6CB) and `ProcessClimate`
(`GClimate::ProcessAll`, 0x54E6DA, with the bookmarks and highlights in between) from `Game::GameLogicLoop`; and
`UpdateFrame` every frame from `magic::Update` (the rain streaks). `OnLoadMap` leaves everything empty, and places
the rain streaks again (`rain::Reset`, 896 CRT draws; the original does not, see [Pending](#pending)).

### Hooks

`OPENBLACK_TEST_WEATHER="x,z,radius[,rain[,fade[,temperature]]]"`, `OPENBLACK_TEST_WEATHER_AT="x,z[;x,z...]"` and
`OPENBLACK_WEATHER_TRACE=1`; see
[openblack-internals.md](openblack-internals.md#debug-environment-variables).

## Pending

- `SnowCover` (the accumulated snow, 0xEDC344) and the drawn snow. Not ported either: the SnowCover step
  `fn_0086C7A0(seconds)` in `UpdateGame`, and the snow a storm lays, `ftol(snow × snowCoverRate × fade)` through
  fn_0086C2D0 in the inner / outer radius at amount × seconds × 0.03.
- **Fidelity bug (ours), the rain placement**: openblack places the rain streaks at every `OnLoadMap` (`rain::Reset`,
  128 × 7 = 896 CRT draws), where the original places them 2 × 896 once, at start-up, and never on land load (see
  [The drawn rain](#the-drawn-rain-raincpp-graphicsrendererraincpp)). The fix is a measured behaviour change, together
  with the snow scatter: it shifts the whole CRT stream (clouds, lanterns and everything after them).
- The snow scatter is missing: the original spends 2 × 3584 = 7168 CRT draws on it at start-up, before the rain's (see [The drawn snow](#the-drawn-snow)); raffclar's tree does it once (13 + 1 draws per flake) at Locator emplace, which matches neither tree.
- The lanterns: openblack's 30 ms jitter (3 CRT draws per light per tick) runs every frame, also while it is light,
  where the original runs it only with the mean < 120 (see [Night lights](#night-lights-report-night_visualstxt)); the
  6 creation draws per light come at the first `Rescan` (the first `night_lights::Update`, the first island frame), not at lantern
  creation; and every light is drawn again (6 each) whenever the set changes (checked every 1000 ms), where the
  original only draws for the new light. A measured behaviour change: first measure whether the Land 1 run crosses
  120, since the gate alone removes the jitter's draws while it is light.
- The lightning flash (`fn_00837290`) and [0xFA2768] are already there, its light stamp on the terrain included (see [The flash and the clouds of the registered storms](miracles.md#the-flash-and-the-clouds-of-the-registered-storms-ecsweatherlightningflash-stormclouds)); the bolt and thunder *callbacks* are set but empty.
- The decay of the lightning flash values 0xEDD384 / 0xEDC374 that `Update3D` does in the rain step.
- The drops on the ground (`g_water_drop_cb`).
- The virtual influence of the weather, `LHInetWeather` and the ambient weather of the saved game.
- `GClimate+0x84` ("lightning even if it is below 30 degrees"): **who writes it has not been found**, 0 is assumed.
- The per-block distance field (+0x9BC) that `Render3D` uses to discard blocks: here the 2D distance to the
  camera is used with the same threshold (400 + 160), and the real cut-off is still the 400 m one of `fn_00834370`.
- The sounds of the houses chosen by the sky type; the alignment blend of the dome (not read yet); the no-blend dome
  mode, the 565 path and the light table after `Jump`; the `EveningRamp` callers (the Relaxation and Sleep desires).
- Unverified: `ecs/Weather/Climate.cpp` `climate::CreateStorm`, at `if (d.lifeTime < 8.0f)`: "// [0x8C2C70] (0x773155), then 20 (0x773162)". A second life clamp after the "< 20 → 20" one; what branch it belongs to is not stated. Unverified.
- Unverified: `ECS/Weather/WeatherThing.cpp` `weather_thing::Create`: "fn_00770F10: the 0x3C8 GWeather (fn_00770E50: +0x3C0 = 0)", while day-night-weather.md gives GWeather as 0x3C0 bytes. Whether the weather thing's storm is a subclass with one more field is unverified.
