# The Land 1 intro and the tutorial's script side

How the original runs the Land 1 intro (the challenge script `FollowUs` and what it calls) and what openblack has of
it. Marks: **(inferred)**, **(approximate)**, **(pending)**, **(not ported)**.

- [Flow](#flow)
- [State in openblack](#state-in-openblack)
- [Dialogue texts](#dialogue-texts)
- [The advisor spirits](#the-advisor-spirits)
- [Script look-ups: CALL and CALL_NEAR](#script-look-ups-call-and-call_near)
- [Interface interaction levels](#interface-interaction-levels)
- [Script highlights](#script-highlights)
- [Timers, help events and field of view](#timers-help-events-and-field-of-view)
- [The family in high detail (SuperVillager)](#the-family-in-high-detail-supervillager)
- [JC specials and the confirmation sounds](#jc-specials-and-the-confirmation-sounds)
- [Test hooks](#test-hooks)
- [Pending](#pending)

## Flow

`LandControlAll` (challenge dump `rt_chl_code.txt` 171106) runs `LandControl1` (74956), which runs `SetupLand1` and,
when the tutorial is not skipped (`IsSkippingToCreatureSelect == 0`), `FollowUs` (49528..53196):

1. 49619..50103 the family is created (mother 4/49, father 4/53, son 4/52), `SET_GAME_TIME 7.3`.
2. 50104..50134 the cinema starts: `START_CAMERA_CONTROL`, `START_DIALOGUE`, `START_GAME_SPEED`, `SET_WIDESCREEN 1`,
   `START_MUSIC 54`, fade in.
3. 50141..51024 the kiss, the son and the sharks, then `SET_AVI_SEQUENCE(1, 1)`: `INTRO.bik` (video.md).
4. 51027..52153 the rescue, the welcome crowd, the texts and the advisors (4431..4437, 5189..5192).
5. 52154 `RUN Drag` (12627..12722): `PLAY_HAND_DEMO(279, 1, 0)` with `Data\HandDemo\drag.hnd`, three
   `HAND_DEMO_TRIGGER` waits and `IS_PLAYING_HAND_DEMO` (it pushes `!IsPlayBack(0)` 0x6FDB9A: wait for the end).
6. 52172..52175 the hand-over: `SET_WIDESCREEN 0`, `END_GAME_SPEED`, `END_CAMERA_CONTROL`, `END_DIALOGUE`.
7. After it: the player follows the family, the citadel (`BUILD_BUILDING` 52474, `CALL_NEAR(Citadel)` 52490), the
   second cinematic from 52520, then `TunnelOfUltimateDoom`, `TeachRotate` / `TeachPitch` / `TeachZoom`, `GiveFood`,
   `CheckCitadel`, and `CitadelGuide` (33817).

`SetupLand1` sets the skip from `CAN_SKIP_TUTORIAL` (25397..25403), that is from the answer to the tutorial requester
([map-loading.md](map-loading.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial)).

Script game speed (CHL 128 START_GAME_SPEED, 129 END_GAME_SPEED, 066 SET_GAMESPEED):
- GScript +0xAC holds the task that controls the game speed; `GScript::Reset` 0x6EB2D0 does not write it (0 from the
  constructor, inferred).
- START_GAME_SPEED 0x6FA9E0: when nobody or this task controls it, `GCamera::SetScriptSlomoControl(1)` 0x441F40
  (GCamera+0x70 = 1; nothing but Save / Load reads it) and the task takes it.
- END_GAME_SPEED 0x6FAAB0: `ActualEndGameSpeed` 0x6FAA60 when nobody or this task controls it: SetScriptSlomoControl(0),
  `GGame::SetSpeed(1.0)` 0x5537F0, +0xAC = 0.
- SET_GAMESPEED 0x6FAAE0: `GGame::SetSpeed` only for the task that controls the game speed (an owner of 0 matches no
  running task).
- A stopped task gives the game speed back (fn_006FAA40 from the task-stop callback 0x6EC71E) and the wide screen too
  (fn_005C78C0 at 0x6EC6FA: SetWideScreen(0, 0) when that task owns it).
- GScript +0x78 (draw leashes) is written by SET_DRAW_LEASH 0x708C9D and read by `GInterface::DrawAllLeashes` 0x5D9382;
  +0x80 (draw highlights) by SET_DRAW_HIGHLIGHT 0x708CCD and read by `ScriptHighlight::Draw` 0x709C9D, which draws a
  did-you-know sign anyway.
- GScript +0x7C: only fn_006ECD70 (0x6ECEDB) and GScript::Reset write it; no reader found.
- The release of the script camera (fn_006ECD70) also Release()s every SuperVillager and empties the list
  (0x6ECEA6..0x6ECECA), and sets `GLandscape::DrawListRebuildCount` = 1 (0x6ECEC4).

The tutorial requester (SkipBox; Init, the callback and FrontEnd::Init are in
[map-loading.md](map-loading.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial)):
- `FrontEnd::Init` calls `SkipBox::Init(400, 290, 0x544480)` at 0x53BB95..0x53BBAC, through `DialogBoxBase::Init`
  0x513400. The box is centred at (400, 300), 400 × 290 (0x513525 / 0x51352C).
- The texts are read at Init: the question is entry 4 of `InfoScriptPatch2` [0xD17CA0] and the four labels are entries
  5..8. An entry past the database's count gives entry 0 (0x544214 / 0x5442D8..0x544321).
- The button's text is HELP_TEXT_REQUESTER_BOXES_04 (0x5442A0: [0xD17CA8] + 0x79B0 = entry 0xA24 "Aceptar"). It falls
  back to entry 0 when the count is 0xA24 or less.
- The question is a `SetupControl(999, 150, 190, 500, 70)` (0x544231..0x54425A) with +0x23C = 4 (wrapped, centred). The
  labels are at (290, 225 + 44 i, 300, 40) (0x5443A7..0x5443D4) with +0x23C = 3 (wrapped, left).
- Check box i is `SetupCheckBox(60 + i, 250, 225 + 44 i, 1, i == +0x20, L"", 24)` (0x54435A..0x54437D), so the selected
  answer starts checked. A click on a check box sets +0x20 = id % 10 (0x5444D1).
- The button is `SetupBigButton(11, 220, 400, main[0xA24], 32, 0, 2)` (0x5442A9..0x5442BD): size 32, side 0, look 2.
- The selection +0x20 is also set to 0 at 0x5442C2 (besides 0x544206).
- The callback, event 1 (a click): button 11 gives the answer. `PauseGame(0)` follows at 0x54458B and
  `DialogBoxBase::Hide` at 0x544593.
- `DialogBoxBase::Init` 0x5134C6..0x513507 starts two Zoomers, both over 0.5 s ([0x3F000000]). The one at +0x34 goes to 0
  and the one at +4 goes to +0xC4 (1.0).
- The box's background is `DrawBg(400 − w / 2, 300 − h / 2, 400 + w / 2, 300 + h / 2, 0xFFFFFF, 0, −1)`. It is see-through
  and drawn on every edge when +0x94 == 1 and +0x98 == 0 (fn_00408340 0x4085F4..0x4086B5).
- `SetupBigButton::Draw` 0x40CEB0 with side +0x244 == 0 (the ctor's 6th argument, 0x40D2BA) puts the label to the right of
  the 32 × 32 button.
- That label is left-justified at x1 + 2, its shadow ([0xC4CCDC]) is drawn first (0x40D10E..0x40D21A), and its size is the
  box's +0xAC (GetMidTextSize). The button sets no +0x20.
- [0xC4CCE4] / [0xC4CCF8] are the label's text and hover colours (inferred).
- `SetupStaticText::Draw` takes its size at +0x240 from +0x20 and shrinks it at 0x4094BA..0x409558. The shadow pass and
  then the text are drawn at 0x4095A8..0x409709.

## State in openblack

> **Code rules.** Intro and help state lives in ECS components (highlights, timers, SuperVillagers) or Locator services
> (the help system, the advisors), never in globals; the `.hd`, `.l3d`, `.anm` and text files load through the resource
> caches; resources are owned with RAII and standard types; pure logic (text layout, timers, spirit motion) is tested
> with fakes in `test/`; comments describe behaviour in plain English, with no decompiled names or addresses (those
> belong here). See [the conventions](../refactor/README.md).

| Part | State |
|---|---|
| Steps 1..6 above | ported and checked in game: the hand-over comes about 4 min after the start (d5dae62e) |
| Dialogue texts | drawn (below) |
| Advisors | drawn and moving (ae5b5287); voice tags not fed (pending) |
| Villagers' focus, override animations, flocks, dance, CALL_IN | pending |
| Citadel as a plan, INSIDE_TEMPLE, SET_INTERFACE_CITADEL | pending |
| Family in high detail, JC specials | drafted |

## Dialogue texts

HelpSystem+0x14 is a HelpText (`Help/HelpTextDisplay`): a ring of 6 texts, the newest sliding in over 333 ms
(+0xA8 += dt × 0.003, fn_005CC760), older ones shrinking (×0.86, then ×0.93) and dimming (alpha − 20). The box is a
full-width strip above the bottom bar (fn_005C57B0: bottom = H − margin − bar − 1, top = bottom − trunc(0.126667 H)),
black at alpha 0x80 (fn_005CCE60), no texture. Fonts by narrator (fn_005CCEA0): 2 good spirit → f1 in RGB(235,235,183),
3 evil spirit → f3 in RGB(255,180,180), others j0 white. Line height H / 30. One splitter for the display and the word
count (`Help/TextSplitter`, fn_005CB590). Drawn by `Renderer::DrawHelpText` between the bars and the film (callback
0x5CD020, priority 20000), from the pre-draw `graphics::OverlayFrame`. Texts never expire: only GAME_CLEAR_DIALOGUE /
GAME_CLOSE_DIALOGUE / END_DIALOGUE or a single-line RUN_TEXT clear them; the reading time only drives TEXT_READ. The
"Continuar" click cue is a KMIcon (Draw3D 0x5C59D0; openblack draws it with `help::input_prompt`, was `help::kmicon`).
All layout steps are float (the FPU runs at 24 bits, fn_007DEE00).

Drawing order:
- HelpText's callback 0x5CD020 is registered with priority 20000 (RegisterFinishFrameCallback 0x5CAD74): after the bars,
  before the film and the fade; it draws the box (fn_005CCE60), the dialogue texts (fn_005CC760) and the click cue
  (KMIcon of Draw3D 0x5C59D0..0x5C5ACE).
- The frame of that callback is fn_005CCAB0; it is drawn only if g_game+0x250188 (a front-end state) allows it.
- The script's fade colour and the bars of fn_005C5780 are drawn in `FinishFrame` 0x82F460 (e) / (h).

The text database (`GSetup::LoadTextScripts` 0x719280):
- Texts come from `Scripts\InfoScript2.txt`; each ADD_TEXT goes through the callback 0x7192E0 → fn_005CCF80 →
  HelpTextData fn_005CAD00. GetHelpText gives entry 0 for an id out of range (0x5C5FAD..0x5C5FCA); count at
  HelpTextDatabase +4 [0xD17CAC].
- A second database, `Scripts\InfoScriptPatch2.txt` [0xD17CA0] (callback 0x7193F0, its own ids in file order, count
  [0xD17CA4]), is read by SkipBox::Init 0x54420F.
- fn_007191F0 also turns " ~" and "~ " (the space and the tilde together) into one 0xF8FE (0x719200 / 0x71921A).
- The language g_game+0x250080 (start_system 0x6432F9..0x6433A4): the first two characters of the game folder's
  `Country.txt` (fopen "rt" 0x643313), matched case-insensitively against the table 0xBEA970 (uk 0, us, fr, de, se, es 5,
  jp 6, nl, br, it, sc 10, tc 11, pl, kr 13, th 14); 0 without the file or a match. Read once.
- `NeedsBiggerText` 0x4079C0: true for the languages 6, 10, 11, 13 and 14 (0x4079D2..0x4079E9).
- When the options close, the help's reading speed (READ_SPEED) comes from the slider; with no value in the profile,
  `ReadRegistrySettings` [0x514742..0x514755] uses fn_005C6CA0 (+0x4610).

The box and the stack (HelpText, 0xBC bytes, `HelpText::Create` 0x5CB090):
- Line height (+0xA0): H / 30 [0x8CF3F8], or H / 28 [0x92A53C] when NeedsBiggerText; computed once in the ctor
  (fn_005CADC0 0x5CADE2). (pending) ReInitialiseText 0x5C7A80 after a resolution change.
- The box's margin = trunc((H − 2 × bar) × 0.025) [0x92A440]; its height 0.126667 H is 19/150 [0x92A43C]. fn_005C6BB0
  recomputes the region every frame.
- The slide-in (+0xA8) only advances while the texts are drawn (the TEXT_DRAW gate fn_005C6E60 and not hidden), and in
  the citadel with g_delta_time instead of g_game_time_inc.
- Six entries are laid out while the slide-in runs, four once it reaches 0.9999 [0x8CF368] (0x5CC90B..0x5CC926).
- The newest enters from a scale of 1 / 0.93 (0x3F89A269, 0x5CC88E); each slot lerps scale and y from the previous slot;
  alpha = ftol(scale × 255), times the slide-in for the newest (0x5CC9AB..0x5CC9F9).
- Profile value TEXT_TOPTOBOTTOM ([0xD16180], 0 without a profile): the first slot at y0 = trunc(lineH × −1/3) instead of
  +1/3 (0x5CC799..0x5CC7C9) and each entry bottom-aligned.
- Profile value TEXT_DRAW (HelpSystem +0x4604, 1 without a profile; fn_005C6E60): 0 never draws, 1 draws the current text
  if its HelpTextData +4 is set, any other value always.
- A single-line text is centred with no zoom (fade only) (0x5CC8A1..0x5CC8C6). A second text clears the single-line flag
  (0x5CCF2C).
- `HelpText::Reset` 0x5CB020: with "all" the ring is emptied (text and narrator; number and colour stay); always font
  +0x8, hidden +0xAC, single line +0xB0 and box shown +0xB4 = 0.
- fn_005CB010 (GAME_CLOSE_DIALOGUE, ClearDialogueControl): box hidden (+0xB4 = 0) and fn_005CB000 (+0xAC = 1: the texts
  are not drawn).
- Data\f1 is "Footlight MT", Data\f3 "Orange LET" (table 0xECCD08); a font that did not load ([0xECCD0C] / [0xECCD14]) is
  replaced by j0 (0x5CAE3F..0x5CAE6E).

Word layout (fn_005CB750 measure, fn_005CB960 draw):
- The wrapped block is centred horizontally (x0 = trunc((W − widest) / 2)) and every line starts at x0: ragged right
  (0x5CBA1D..0x5CBA37). The widest line keeps its trailing space in the measure (0x5CB7F3, then the wrap at 0x5CB8DD).
- Max lines = ftol((bottom − top + 1) / lineH); the words past the last line are dropped, with no ellipsis (0x5CBC5C).
- $D / $P print the entry's number with "%3.3f" (0xBF1A68) / "%3.3f%%" (0xBF1A74); the draw puts a space after it but the
  measure does not (0x5CBB89).
- The measure walks the codes too, so the draw starts with the font and colour the measure ended with (fn_005CCF50 runs
  once, before fn_005CB750).
- Each word is one `GatheringText::DrawTextRaw` call at z = near × 1.2, clipped to the box's top and bottom
  (0x5CBDFE..0x5CBE7F).
- vAlign 2 bottom-aligns the block, any other non-zero value centres it (not truncated).

The splitter (fn_005CB590, shared with the word count fn_005CBEC0):
- Blanks: space, tab, CR, LF, 0xF8FE (fn_005CB0F0); escapes '$' and '\' (fn_005CB190). A word stops at a blank, an
  escape, the end or after 47 (0x2F) characters (0x5CB6E3).
- "$$" / "\\" (any two escapes) is the second character as a literal.
- Codes (fn_005CB1D0 / fn_005CB220): a digit or one of C D F M N P (either case, byte table 0x5CB26C). $C<n> colour,
  $F<n> font (fn_005CCEA0), $M<n> the control icon, $D number, $P percent, $1 stop (fn_005CB4C0); unknown codes ($I, $s,
  $g…) only lose the escape (0x5CB54C). After $C<digits> one more character is skipped (0x5CB53A).
- $C<n> (fn_005CB400): n read as 0xBBGGRR; $C0 goes back to the narrator default (2 → RGB(235,235,183), 3 →
  RGB(255,180,180), else white).
- $M<n> only acts in the draw (flag of fn_005CB590): fn_005CB3E0 → HelpSystem fn_005C5B50(n), the control-icon KMIcon at
  the right of the box (HelpSystem +0x28).
- The blanks after a code belong to its empty piece; a word followed by blanks has type 3, by 0xF8FE type 6, after a
  line feed type 1 (fn_005CB120).

The "click to continue" cue (HelpSystem::Draw3D 0x5C59D0..0x5C5ACE, KMIcon +0x24):
- Made once while a text waits for the click: fn_005C78F0(1) resolves the action (no cue when it fails, 0x5C5A4F);
  animType 1 (the button blinks); text 0xE79 (HELP_TEXT_TOOLTIP_07; entry 0 when [0xD17CAC] <= 0xE79); fade 1.0 s
  [0x92A444]; c1 yellow.
- Size trunc((bottom − top + 1) / 3); x = (W & 0xFFFF) − 4 (0x5C5AA0); y = bottom − trunc(size / 2) + 1 (fn_005C5970),
  moved there every frame (+0x124, 0x5C5AD0).
- Deleted by AddText (0x5C615E..0x5C6196), ClearTextDisplayed (0x5C54F1 / 0x5C550B), the used click (0x5C6A56) and, once
  a turn, fn_005C5E50 when HelpText's newest entry is empty.

The text functions:
- TEMP_TEXT 0x6F7E40 / TEMP_TEXT_WITH_NUMBER 0x6F7F50 show the string prefixed with "*" (UNICODE_sprintf "*%s" 0xC0D420,
  0x6F7E8D), narrator 1, text id 0, and report "Development text being used in game!" (0xC0D3F8) as a script warning.
- AddText fn_005C6100: withInteraction 1 = wait for the click (+0x57C), 2 = no click (+0x580); the text id becomes
  current (+0x584, 0x5C615C).
- ShownLongEnough fn_005C68C0: 1 s [0x92A444] while the text waits for the click, else 0.5 s [0x915D1C]; in ms inside
  the citadel (0x5C68ED..0x5C6949), else in turns.
- ProcessInterface returns 0x14 when the click was taken by the text (0x5C6A25, 0x5C6ABA); during a hand demo
  (IsPlayBack(0), 0x5C69ED) the click does nothing.
- ClearTextDisplayed 0x5C54E0: the KMIcons +0x24 / +0x28, then +0x580, +0x57C, +0x45DC and +0x45E4 = 0.
- HelpSystem::Reset 0x5C5580 (from GScript::Reset 0x6EB340) also clears the sets' sent turns +0x78 and counts +0x2FC
  (0x98 dwords each), the banter count +0x45D4, the history (+0x45C8 / +0x45C4) and ResetIcons; it does not touch the
  dialogue owner +0x45CC.
- `HelpSystem::SetWideScreen` 0x5C6AD0, when +0x45E8 changes, also calls `DialogBoxBase::HideAll` (on) and sets the bars'
  timer +0x45F0 = wideScreenTime 0xD16174 × 1000 × the part already done (0x5C6B3F..0x5C6B4E).

The turn step (HelpSystem::Process 0x5C8FE0), in order:
- fn_005D7E40 (the hand state, 0x5C8FF0); ProcessIcons fn_005C5E50 (0x5C9001); the spirits fn_005C9060 (0x5C9008:
  HelpSpirit::Process of +8, the evil one, then of +0xC, the good one); the tooltips fn_005C9D00 (0x5C900F); the
  did-you-know bubble fn_005C82F0 (0x5C9020); with +0x45F8 && +0x45F4, ProcessConditions and ProcessBanter
  (0x5C9025..0x5C904B).

Help message sets (HELP_SYSTEM_MESSAGE_SET):
- The table 0xBF0D60: 0x98 sets of 20 bytes {first, last, mode, category, script}.
- `HelpSystem::ProcessConditions` 0x5C86B0: +0x30, 18 dwords, cleared every turn, [1] = 1; nothing more while a script
  holds the wide screen; [2] = 1 when HelpProfile's AllInterface (43) was last used more than 120 s ago ([0x92A49C]).
- `ProcessBanter` 0x5C81C0: when condition 2 is set, RunMessageSet(GetRandomBanterSet()).
- `GetRandomBanterSet` 0x5C81E0: the n-th set of 1..25 not sent yet, n = LocalRand(25 − count) (the local stream, not the
  synced one); once all 25 are sent, their sent counts and +0x45D4 are cleared and set 1 is given.
- `SetSetSent` 0x5C8A80: +0x78[set] = turn, TriggerCategory(category), +0x2FC[set] += 1, +0x55C = set, +0x560 = turn,
  HelpProfile event HelpQuery (42).
- `RunMessageSet` 0x5C8B80: refused when StopRunningScripts refuses; mode 3 runs the set's script (RunMessage 0x5C8C90);
  else GetStartAndEndTextForSet 0x5C8AE0: mode 0 the table's first..last, mode 1 one of them (GetRandomTextFromSet
  0x5C8C10: first + LocalRand(last − first + 1)), mode 2 the thing's GetQueryFirstEnumText / GetQueryLastEnumText (vt
  +0x4D8 / +0x4DC), 0 and 0 without a thing.

## The advisor spirits

Two `HelpDude`s (0x5B8F00..0x5C2800) driven by `HelpDudeControl` (HelpSystem+0x10): dude 0 the good one
(`Data\HelpSprite\markgood.hd`, 97 bones), dude 1 the evil one (`markevil.hd`, 73 bones). A `.hd` holds a plain L3D0
mesh and 80 clips in CAnim format (not `.anm`: rotations are YXZ Euler angles, layers additive; checked against the
exe's functions in an emulator). Ported in `Help/HelpDudeFile`, `Help/SpiritAnimClip` (was `Help/CAnim`), `Help/Spirits`, `Help/SpiritsRuntime`,
`Graphics/RendererSpirits`.

- They float 7..9 units in front of the camera in "hover" coordinates (x and y over half the screen width, clamped
  ±0.75 / ±0.6), moving on cubic splines in time (1.0 s for eject, home and fly) and a potential field (rest zones at
  ±0.66, the partner, the mouse, the screen centre).
- Drawn after the scene in their own view (`RenderPass::FinishFrame3D`, depth cleared) while their in-world blend
  +0x35DC < 0.5, else in the world; alpha fades +3/s in, −2/s out; halo (good only) and puff from `smoke.raw`, the
  rainbow trail.
- No random idle gestures exist: gestures and emotions come from the voice samples' cue labels (pending: not read).
- The mouth: vowels 7/8/9 from `audio::advisor`'s lip-sync key.
- CHL 7, 8, 9, 10, 100, 137, 138, 139, 140, 165, 166, 167, 300, 301, 418 (below).

Objects and files:
- HelpDude: 0x37F0 bytes, vtable 0x900D04, ctor fn_005C17D0(isEvil, 0), then SetState(4). HelpDudeControl Init
  fn_005C2A40 builds both and puts them at their home point, state 0 (0x5C2CEA..0x5C2D16).
- The script-facing HelpSpirit holders are HelpSystem +0xC (good) and +8 (evil); help spirit type 1 → dude 0, any other
  → dude 1 (fn_005C5250 / fn_005C5260).
- A `.hd` is a "LiOnHeAd" file with one segment "helpdude" (`HelpDude::Load` 0x5C2194; names at 0x915D20 good, 0x915D24
  evil; lower case on the disc).
- Read order: bone count (+0x0C, replaced by the mesh's at 0x5C101F), anim count 80, rest height (+0x10: 63.423 good /
  79.154 evil), mesh name char[0x80] ("DATA\Yogi_Mesh.l3d" / "C:\dev\TESTBED\DATA\Demon_Mesh.l3d"), the 80 anim names, a
  has-data flag (0 skips mesh and clips), u32 mesh size then the L3D0, then LoadAnims 0x5C14E0.
- LoadAnims: a count (80), then per slot with a non-empty name a u32 record size (0 = no clip) and CAnim::ReadBinary
  (fn_005C1090 0x5C10CD..0x5C112E); an empty name has no record.
- After the clips: the starting emotion (+0x2C28, 0 in both), u32 n + n bytes of face records (+0x2C38, n = 0x200: 8
  records of 16 floats, one per emotion), near depth +0x35B8 (8.7333 both), model scale S +0x35B4 (0.02552 / 0.02683),
  +0x35C0, the per-anim events, +0x35C4 (0.3467 / 0), +0x35C8 (0 / 0.3067), far depth +0x35BC (6.0267 / 5.64, × 1.2 in
  Init 0x5C2C9B), the 80 anim flags +0x2E7C, halo scale +0x34DC (0.4733 / 0) and halo offset +0x34E0 ((0, −0.28, 0) / 0).
- Per-anim events +0x333C[i] (16 bytes): count, the 12-byte sounds (`PlaySoundFX` 0x5C2800 when the anim's phase crosses
  each one), and +8 / +0xC the loop window of anim mode 4 (0x5BCE49..0x5BCE7A).
- Anim flag & 0x20: skipped while clinging (0x5BCEDC).
- S scales the rows (0x5BE323), the partner zone, the bank lean and the trail; the halo is S × +0x34DC × the model scale ×
  100 (0x5C0AB8).
- +0x35C4 / +0x35C8 place the fingertip of the point arm: bone 0 × M + r0 × +0x35C4 × size + r2 × +0x35C8 × size
  (0x5BF1DC..0x5BF2FB) **(approximate)**.

The CAnim clips (CAnim::ReadBinary 0x860860, CFrame::ReadBinary 0x860C30):
- CAnim is 0x38 bytes (`new` at 0x5C1108); the header is eleven u32 (0x860878..0x860965); +0x04 the looping flag (word &
  1); +0x10..+0x18 the root move of the whole clip; channel → bone tables at 0x8609B8 / 0x8609E3; each key holds the
  rotation channels then the position channels.
- The binary size is 0x2C + 4 per channel + 12 per channel and key (fn_00860BE0); the .hd record size before a clip is
  that + 4.
- `frameCount` keys evenly spaced over the duration: a looping clip wraps the last key to the first; a one-shot clip puts
  its last key at the duration (period = duration × frames / (frames − 1), integer maths, 0x860E1A / 0x861EF9). The key
  after the last is always key 0, even for a one-shot clip (0x860E5B / 0x861F33).
- A rotation channel is three Euler angles turned into a matrix by `LHMatrix::SetYXZMatrixOnly` 0x7FAC10 called as (y,
  x, z): M = Rz(z) Rx(x) Ry(y) with column vectors. The two keys' matrices are lerped element by element and each row is
  normalised (fn_007FB5C0).
- The angles rotate the bone in model axes from its rest pose: local = rest world(bone) × key × inverse rest
  world(parent) (row vectors, 0x8610B7..0x861456). A position channel is the bone's absolute translation relative to its
  parent (lerped, 0x861964).
- fn_00860E00 (Set): bones with no channel keep their matrix, or take the stand clip's key 0 (HelpDude +0x28) when called
  with fill = 1 (0x5BB8BE). It clamps a negative key to 0 (0x860E42); fn_00861EE0 does not.
- fn_00861EE0 (Add): the change from a reference key of the same clip to the sampled key, applied to the current local
  matrices (rotation in the parent's rest axes, translation added, 0x8621C9..0x8629AF). There are no blend weights:
  HelpDude passes the "weight" as the time into the clip (vowels 7..9 fn_005BF810, look 18 / 19 fn_005BF8C0).
- The mirrored path (a bone remap table, last argument) is never used: HelpDude always passes 0.
- `HelpDude::ApplyAnim` 0x5BB980(anim, phase, referencePhase, wrap): the phase wrapped to its fraction (0x5BB9A8) or
  clamped to 1 (0x5BBB26), then ≥ 0; ms = ftol(duration × phase), at most duration − 1; reference key = ftol((frames − 1)
  × referencePhase), at most duration − 1 (sic, 0x5BBA0C); the root move = phase × displacement, added through the rows
  (0x5BBA19..0x5BBAD6).
- The rest skeleton: `LH3DAnim::SetTransform` 0x83A1D0 into +0x28AC (each bone under its parent, the root under the
  identity), its inverses into +0x28B0 (LHMatrix::SetInverse 0x7FB290); SetTransform returns max − min of the world y,
  which the original writes over the .hd file's rest height +0x10 (0x5C1250). FinishAnimStack 0x5C0610 → 0x839F10
  composes the world bones for the draw.

Indices and states:
- Anim names at 0xBF029C (0..9 static, 10..17 copied from the emotion names 0xBF027C "Normal", "Pleased", "Displeased",
  "Sad"… "Furious", 18.. written at 0x5BAD6A). Emotion clips 10..17 (clips + 0x28); avoid 32..35 L / R / U / D; cling
  65..68 L / R / U / D (table 0x5BF5BC); look 18 / 19 (or 70 / 71).
- States +0x3490 / +0x3494 (`SetState` 0x5BD4A0, tables 0x5BD9A0 / 0x5BD9D0): 4 hover, 8 point, 0x40 avoid, 0x80
  fly-to-anim heading to 0x200 (play), 0x100 cling; a state | 0x20 is its outro; 0x29 / 0x2A fall back to 1 (0x5BD535).
- fn_005BBEF0 (playing an anim): 0x80 current or queued heading to 0x200, or 0x200 current or queued. PLAY_SPIRIT_ANIM
  fn_005BBF30: anim 0 while playing → SetState(4); else target, anim +0x350C, speed +0x3508, then SetState(0x80).
- Cling edge fn_005BBD20: |x| > 1.28205 |y| ([0x900C64]) → right (x = 1.04) or left (−1.04), else bottom (y = 0.78) or
  top (−0.78).
- Home anchors fn_005BD440: (W/2, 3H/2), (−W/2, H/2), (W/2, −H/2), (3W/2, H/2); the home point fn_005C2E90 is the
  nearest, or (3W/2, −H/2) / (−W/2, −H/2) when within 8 px. Home / vanish: fn_005C32A0 "at home" = control state 0 or 1.
- FLY_SPIRIT fn_005C3250: only out of home, FlyTo in 1 s, clamped. CLING_SPIRIT fn_005C4D40 does not eject. Both take x,
  y in 0..1 of the screen.

The spirit opcodes (each POPs the spirit, `ConvertScriptSpiritToHelpSpirit`; an unavailable object does nothing):
- `SPIRIT_EJECT` 7 0x710410 → `HelpSystem::SpiritEject` 0x5C6570(t, help script) → fn_005C4BD0: a help script's spirit
  appears (0x5C3400), any other is ejected (0x5C32C0). `SPIRIT_APPEAR` 300 0x710460 → SpiritEject(t, 1).
- `SPIRIT_HOME` 8 0x710490 → `HelpSystem::SpiritHome` 0x5C6670(spirit, arg) → fn_005C5200: spirit +0x58 / +0x5C = 0; arg
  ≠ 0 (a help script's spirit) → fn_005C3540, it vanishes (the dude's state +0xC = 1 at once, 0x5C357A); arg 0 →
  fn_005C3590, it flies home (fn_005C2E90 / fn_005BBDD0 first: the advisor flies off, inferred). `SPIRIT_DISAPPEAR` 301
  0x7104E0 → SpiritHome(t, 1).
- `SPIRIT_POINT_POS` 9 0x710510: POP in_world (the raw dword), the position, the spirit → `HelpSystem::SpiritPoint`
  0x5C6590 → fn_005C4EF0 (eject, then point mode 1 with side 8.0 / height 5.0).
- `SPIRIT_POINT_GAME_THING` 10 0x7105B0: POP in_world, the object, the spirit (converted before the test, 0x7105F1); no
  object → "Object no longer valid" (0x7105FD); else `HelpSystem::SpiritPoint` 0x5C6650 → fn_005C4FA0 (the thing's
  position + GetHeight vt +0x42C), re-sent every turn by 0x5C50C0.
- `LOOK_GAME_THING` 100 0x710630: POP the object, the spirit; no object → "Object no longer valid" (0x71066A); else
  fn_005C65F0 → fn_005C4E50.
- `HelpSpirit::Process` 0x5C5270 once a turn re-sends the point object (0x5C50C0) and the look object (0x5C5170); the look
  object goes through the point function (original bug).
- `STOP_POINTING` 137 0x710860 → fn_005C6630 → fn_005C5060; `STOP_LOOKING` 138 0x710890 → fn_005C6610 → fn_005C5090;
  `LOOK_AT_POSITION` 139 0x7108C0 → fn_005C65D0 → fn_005C4E00.
- `SPIRIT_PLAY_ANIM` 140 0x710940: POP the time, the anim (raw), y, x, the spirit; anim outside 0..0x50 → "Invalid enum"
  (0xC20B70), then "Invalid Y" / "Invalid X"; none stops it; `HelpSystem::SpiritPlayAnim` 0x5C6690(t, x, y, anim, time) →
  fn_005C4C80.
- `SPIRIT_PLAYED` 165 0x710A50: push `!HelpSystem::IsSpiritPlayingAnim` 0x5C66C0 (neg / sbb / inc; the negation of
  fn_005C4D10) as a bool.
- `CLING_SPIRIT` 166 0x710AA0 → `HelpSystem::SpiritCling` 0x5C66E0 → fn_005C4D40; `FLY_SPIRIT` 167 0x710B70 → `SpiritFly`
  0x5C6700 → fn_005C4DA0; both POP y, x, the spirit.
- `SPIRIT_SCREEN_POINT` 418 0x710CA0: POP y, x, the spirit; the pixel (ftol(W·x), ftol(H·y)) with W / H = [0xE85058] /
  [0xE8505A] (0x710D45..0x710D7B) → fn_005C65B0 → fn_005C4F50.
- The screen checks (0x7109CA..0x710A28 / 0x710AE8..0x710B4B): "Invalid Y" (0xC20B64) then "Invalid X" (0xC20B58) for a
  value below 0 (NaN too: `fcomp, test ah, 1`) or above 1; the opcode goes on.
- `SPIRIT_SPEAKS`: a text past 6974 becomes 0 (0x710C6E). (inferred) the spirit conversion uses the local player's
  alignment.

Motion:
- Hover x / y (Sethoverx 0x5B96D0 / Sethovery 0x5B98D0) are ignored while the puff runs; a target under 0.001 s snaps.
- PointAt fn_005BC4A0: the world target snaps out of the world, else moves in 0.5 s; off screen when the projection
  fails, the depth is under 1.1 × near (0x5BC607) or y > H (0x5BC62C); the point arm side is LocalRand(2) (0x5BD784).
- `HelpDude::Feel` 0x5B9AD0: the 6 zones (HoverZone::Feel 0x5B9620, at +0x3380) minus 250 e², with the edge limit 0.45 in
  the wide screen, else 0.6; y is weighed × 4/3.
- Zones (fn_005BDAF0): the partner −0.5 with focus, else −5; the point {4, 0.2, 0.6} and {−1, 0.08, 0.16} at the point
  target; the mouse inner radius 0.16 and strength −1.5 × mouse speed − 2; the centre {−4, 0, 0.6}.
- `HelpDudeControl::Process` 0x5C3A30(dt, 0.4) from HelpSystem::Draw3D with g_delta_time × 0.001; the model scales 0.8
  e^(−0.5 (bias − 0.5)): 0.841 / 0.761 (0x5C3A9E..0x5C3AFF).
- Depth fn_005BD2A0: 2 × near when flagged, else lerp(+0x35B8, +0x35BC, smooth(+0x34D0)) × (1 + 0.3 k); smooth fn_005BD250
  = (1 − cos πt) / 2 clamped. Depth spacing fn_005BA010: the two dudes kept 0.9 apart (0.81 squared).
- The HUD matrix (0x5BE2F5..0x5BE6E4): the camera's world axes × S × the model scale; pitch from the depth speed × 0.3 +
  the pitch offset; yaw = hover x × −0.8; roll toward the cling edge the short way at 3 rad/s.
- In the world (0x5BE702..0x5BE9B6): one shared bob [0xD15AA4] += dt × 0.3, y + sin(bob × 2.3); the ground is GetAltitude
  + 3; the rows grow by (3 sb + 1) × S × the model scale.
- The base pose clip runs at rate × 1.01 for the evil dude and × 0.999811 for the good one (0x5BCA99 area); the hover
  clip's sound uses the stable clip's time over the hover clip's duration (sic, 0x5BCA99).
- The bank lean: hover x speed / S × 0.025 × (1 − 0.8 stable), dead zone 0.01 (0x5BF124..0x5BF1CA).
- The point arm angle: atan2 in degrees × 0.57295777918682045 [0x900C80] (sic), × the point weight, + 0.5
  (0x5BF36B..0x5BF37D).
- GoInvisible flicker windows: phase 0.13..0.25 and 0.55..0.7, or 0.16..0.25 (0x5BB94B..0x5BB969); [0xD15AAC] /
  [0xD15AA8] drive it with GetTickCount (0x5C0827).

Face and head:
- fn_005BD0B0: the face record lerped from neutral by the emotion weight (fn_005BAA40); the blink fn_005BAC10 (record 0 /
  1 the wait, 2 its length; −0.01 [0x8CEFC4] makes a double blink); the two lids (fn_005C0310, ms clamped to [0, dur −
  1], reference key 0); the emotion clip at +0x2C28 + 10.
- Eye bone scale (fn_005C0310, +0x2ECC..+0x2EE0 by face rec[3] / rec[4]): 1 except the evil Normal 1.01 and Afraid 1.47.
- Emotion cross-fade dt × 3 (0x5BE207); the peak decays 0.3 / s (0x5BE2D0); SetEmotion fn_005BD210 stores max(peak, 0).
- CalcHeadPos 0x5BFE00: the head bone's frame with its origin moved between the two eye bones, the target in it (nothing
  when behind, local y < 0, 0x5BFFA5); +0x34BC = atan2(z, y), +0x34C0 = asin(x), each clamped to ±1.0472 and × 0.477465.
- The head follows at 4.5 × dt (evil) or 1.35 × dt (good), the step × 0.95 (fn_005BF620); the look noise amplitudes
  rec[13] / rec[14] are 0 in both files.
- Look target fn_005BC0A0: interest in a fast mouse (speed 4, distance 0.3, Random < 0.3; decay 0.5 / s, clamp [0, 3]);
  alternation timers Random(2, 4) / Random(3, 6); mode 1 the partner's model point, 2 the camera while talking or
  pointing.
- The mouse in hover space there is computed by integer division (idiv 0x5BC0CB / 0x5BC0FD): −1, 0 or 1 per axis (sic).
  The converted look position (0x5BC350) is then not used (sic): the point target is.

Voice tags (the voice samples' cue labels):
- `AudioTag::BuildAudioTags` 0x42AE70 runs fn_0042A6D0 per label until its NUL (0x42B1B4..0x42B237); ';' and '/' end the
  label (0x42A6F1..0x42A70F).
- A tag record is 20 bytes {time, who, action, index, value}: "[" then "<talker><type> <name>[digits]"; the talker by
  table 0x42AC04 / 0x42AC1C, the type by 0x42AC50 / 0x42AC64; trailing digits are the value (100 without).
- Type 3 names an anim, type 4 an emotion: strnicmp over min(strlen(name), letters) against the 80 anim names / 8
  emotions (0x42AB04..0x42ABEA); type 7 by its first letter (table 0x42AC94 / 0x42ACAC). An unknown name logs
  "unrecognised name in audio tag [%s]" and counts in [0xC5836C].
- The word is read with sscanf "%s " into the static buffer [0xC5716C]; an empty rest leaves the previous word in it, and
  the position still moves on by its length (0x42A989..0x42A9C0).
- fn_005BCBC0 fires every tag the sentence time has passed (strictly, 0x5BCBE1..0x5BCBF3); the time is the tick time
  while the play position is still −1 (Say's delay), so tags fire on it then; fn_0042ACC0 applies a tag.

Drawing:
- The model (fn_005C0700) is an LH3DObject of type 3 drawn through vt+0x100 in the frame (Draw3D 0x5C5B26, in-world blend
  ≥ 0.5) or vt+0x108 from the FinishFrame callback 0x5C2E10 (priority 100, after the Z reset quad of FinishFrame (d),
  before the 2D rectangles at 10000 and HelpText at 20000); fn_005C3920 decides by the blend (< 0.5 = overlay). Skinned
  with world bones from FinishAnimStack 0x5C0610 (97 / 73 bones).
- Its alpha is obj+0x4C's byte = +0x2C1C × 255 with the GoInvisible flicker (0x5C080F); 0 draws no model (0x5C0952);
  vt+0x48(a < 255) at 0x5C0952..0x5C0966. Light 0 is moved for the overlay (0x5C0732..0x5C080C / 0x5C0763..0x5C080C) and
  put back at 0x5C0EA7. The halo, then the puff (0x5C0986..0x5C0E95), smoke.raw mode 6.
- The overlay light: Get3DPointFromScreen((0, H/2), −10), moved toward the eye by 2 × blend while blend < 0.5
  (0x5C0732..0x5C080C).
- The in-world colour (0x5BE9B6..0x5BEA8F): white lerped to the land diffuse by ftol(255 × smooth(blend)), specular = land
  specular × w >> 8 **(approximate)**.
- The puff: 16 particles of 0x20 bytes (+0x2C10), made on the first draw (0x5C0B24..0x5C0C06); six Random draws each (the
  CRT stream): grey 116..250, vx −0.8..0.8, vy −1.9..1.5, vz, size, spin; k = 2 − vy.
- StartPuff fn_005C1D20: the alpha target flips (alpha > 0.5 → 0, else 1), so every eject or home is a puff.
- Puff draw (0x5C0AE5..0x5C0E95): time unit ms × 0.0032; fade-in clamp(t × 4, 0, 1); alpha = ftol(255 − 32 age) × fade;
  ARGB = ((2a)/4) << 24 + grey & mask (evil 0x7F1F1F, good 0xFFFFFF); size ((sizeBase − k age fade × 0.5) + 1) × 0.12, at
  least 0.0001; angle spin × age + i; cell (age × 8) & 15; at hover + v × 0.1, depth −depth − 0.6; then vy −= (spin + 1) ×
  u × 0.3. It ends when no particle was drawn.
- The trail: 32 points (+0x28BC), reset to the hover by fn_005BBCD0; fn_005C3850 feeds fn_005B8F00 only for the dudes
  drawn out of the world with g_delta_time × 0.01; fn_005B90C0 gives 64 vertices per dude through the index list
  0xD15788.
- The rainbow trails: callback 0x5C2E30 (priority 100, after the Z-sorter's flush, before the Z reset) → fn_005C3850 →
  fn_005B90C0, Draw3DWorldTriangle with rainbow.raw in mode 15 (+4 = 5, +5 |= 1; 0x5C386B), ZFUNC LESS (2) during the
  call and 4 after.

## Script look-ups: CALL and CALL_NEAR

CALL 026 / CALL_NEAR 051 (0x6F0EB0) pop the flag, (the radius), the point, the subtype and the type; the type table
0xC0C728 picks the finder: `map_cells::FindNearForScript` with radius 1.0 for CALL, `GetNearestTown` (10.0 or the
radius) for towns, the creature list for creatures; marker, dance, flock, influence ring and weather thing have none.
An object's script (type, subtype) is `ecs::script_type` (vt +0x4E8 / fn_006F6D00): subtype 5000 means any; the citadel
heart is type 18. The 13 look-ups of the intro resolve.

- `CALL` = `GScript::Call` 0x6F0D60: pops excludingScripted, the point (z, y, x), the sub-type, then the type
  (0x6F0D75..0x6F0DDF). `CALL_NEAR` pops excludingScripted, the radius, the point, the sub-type, the type
  (0x6F0EC5..0x6F0F41).
- Type 1..41 only (`test esi, esi; jle` / `cmp esi, 0x2A; jl`), else "Invalid type=%d" (0xC0CB48) and 0. The table
  0xC0C728 has 24 bytes a type: +0 the AtPos finder (CALL), +4 the NearPos one (CALL_NEAR); NULL → "No find function for
  type=%d" (0xC0CB18) and 0. Found: `AddScriptGameThing(thing, 0)` 0x70D0F0; else "Thing not found" (0xC0CB38) and 0.
  `PUSH(id, 4)` at 0x6F0E94 / 0x6F0FFD.
- `FindTownAtPos` 0x6F7340 = `FindTownNearPos(…, 10.0 [0x41200000])`; `FindTownNearPos` 0x6F7370 =
  `MapCoords::GetNearestTown(r)` 0x6020E0. The type, sub-type, filter and excludingScripted are not used.
- `FindAtPos` 0x6F7220 / `FindNearPos` 0x6F7280: GScript +0x14 = the point, +0x20 = 1.0 / r, then
  `FindNearForScript(filter, type, subtype, 1.0 / r)` 0x604370: the nearest accepted thing of the square's cells, not cut
  at r. The filter is 0x6F6FA0 (type and sub-type) for CALL, 0x6F7070 for CALL_NEAR (also `GetDistanceInMetres(point,
  the thing or its totem) <= r`, `test ah, 0x41`), wrapped by 0x6F79F0 / 0x6F7A20 (IsInScript vt +0x448 rejects) when
  excludingScripted.
- `FindCreatureAtPos` 0x6F7380 / `FindCreatureNearPos` 0x6F73C0: the first creature of `Creature::CreatureList` 0xC5FCF8
  with `GetDistanceInMetres(point, its MapCoords) <= 1.0 [0x8AA390]` (AtPos: no type or sub-type test) / `<= r` and info
  +0x1F4 == subtype (NearPos).
- `GET_NEAREST_TOWN_OF_PLAYER` (`GScript::GetNearestTownOfPlayer` 0x6F2A20): five POPs (0x6F2A2F..0x6F2A83): the radius,
  the player (a float, `__ftol` 0x6F2A96), then z, y, x; `ConvertScriptPlayerToGamePlayer` 0x6EB9A0 (0x6F2A9C),
  `GetPlayer` 0x5509B0 (0x6F2AC0), `MapCoords(LHPoint)` (0x6F2AD7). None: "Did not find town" (0x6F2AEA) and 0; else
  `AddScriptGameThing(town, 0)` (0x6F2B0D) and the town.
- **GetScriptObjectType** (vt +0x4E8) per class: ScriptMarker 0x70D960 = 1, Abode 0x406810 = 2 (every abode class: Field,
  StoragePit, TownCentre, Totem, Creche, Graveyard, Windmill, Workshop, Wonder, Football, PuzzleTotem), Feature 0x5276C0
  = 3 (also AnimatedStatic, the chess pieces, Flowers, WorshipSiteUpgrade), Villager 0x753020 = IsAChild ? 5 : 4, Animal
  0x41B200 = 6, Reward 0x6E5CA0 = 7, MobileStatic 0x609330 = 8 (Bonfire 0x439A70, Fragment, MagicTeleport,
  GStreetLantern 0x734D40 too), Town 0x73E200 = 9, Dance 0x50C3C0 = 10, Flock 0x530490 = 11, Creature 0x47C8B0 = 12,
  DeadTree 0x5115B0 = 13 (FelledTree too), InfluenceRing 0x5CDC50 = 14, WeatherThing 0x774360 = 15, Pot 0x66F530 = 16,
  ScriptTimer 0x711600 = 17, CitadelHeart 0x4680B0 = 18, WorshipSite 0x77D2E0 = 19.
- (continued) MobileObject 0x607B60 = 20 (Whale, OneOffSpellSeed, crops, creeds), Dove 0x41EAA0 = 21 (Bat, Crow, Dove,
  Pigeon, Seagull, SpellBat, SpellDove, Swallow, Vulture: the vtable sweep), Tree 0x74C130 = 22 (MagicTree too),
  LandscapeVortex 0x5FFFF0 = 23, SpellSeed 0x729C90 = +0x72 ? 30 : 24 (`neg byte +0x72; sbb; and 6; add 0x18`), Poo
  0x6083C0 = 25 (`fn_00607000` for GMobileObjectInfo 5), Ball 0x436100 = 28, Mist 0x606910 = 29, Rock 0x6E79E0 = 33,
  SpellDispenser 0x722FB0 = 36, ScriptHighlight 0x70AE30 = 37, GComputerPlayer 0x6587B0 = 38, Scaffold 0x6EAB60 = 39,
  TotemStatue 0x738EB0 = 40; every other class 0 (`GameThingWithPos` 0x570200: `xor eax, eax`: the Citadel, PuzzleGame,
  BigForest, FishFarm, MapShield, the spell icons, GBaseOnly 0x609340...).
- `Villager::IsAChild` (vt +0x458, 0x55CB00): IsChild vt +0xAF8 == 1, flags +0xE0 bit 3.
- The class factory `fn_00419E00` has no case for the Vulture, CitadelDove and CitadelBat (0x419FCE), so none is ever
  made.
- **The sub-type** (`fn_006F6D00`, byte table 0x6F6F78, jump table 0x6F6F28, types 2..40; `(type − 2)` unsigned above
  0x26 at 0x6F6D09 → "Unknown type for search" 0xC0D368 and 9999 = 0x270F): the info record's index (Object +0x28 −
  array) / size. Abode (0x6F6DB1) 0xC3C690 / 456; Feature (0x6F6DC4) 0xCC99A0 / 292; Villager (0x6F6D2A) 0xDA6BE8 / 932;
  Animal (0x6F6D4A) 0xC4D030 / 716; Reward (0x6F6DE4) 0xD50BF8 / 304; MobileStatic (0x6F6E02) 0xD3A6D8 / 300; Creature
  (0x6F6D6A) info +0x1F4; Store (0x6F6D75) 0xD4C660 / 324; WorshipSite (0x6F6EB1) (+0x8C − 0xDA57A8) / 28; MobileObject
  (0x6F6E20) 0xD38448 / 276; Tree (0x6F6D93) 0xDA3AD8 / 320; Vortex (0x6F6E3E) +0xE0; SpellSeed (0x6F6E6D) (+0x28 −
  0xD9D678) / 400; OneShotSpell (0x6F6E5A) `CastOneOffSpellSeed` (vt +0xAC) +0x68, else as SpellSeed; PuzzleGame
  (0x6F6E8B) +0x48 (never reached: its type is 0); Field (0x6F6E90) (+0x120 − 0xCCF070) / 340 (never reached: its type is
  2); Highlight (0x6F6ED4) 0xD96390 / 272; TotemStatue (0x6F6EF2) 0xDA1D18 / 292; DEAD_TREE (0x6F6E46) "Not implemented"
  (0xC0D380); the default is byte 19 of the table (0x6F6F12).
- No sub-type (9999) for TOWN, DANCE, FLOCK, INFLUENCE_RING, WEATHER_THING, TIMER, CITADEL, BALL, MIST,
  ONE_SHOT_SPELL_IN_HAND, TOTEM, COMPUTER_PLAYER, SCAFFOLD, below 2 or above 40.
- Fixed sub-types: a Fragment (0x76E9D0) has &MSInfo 0xD3A930 = 2; a OneOffSpellSeed (ctor 0x72A3A0) has 0xD39F3C = row
  25. Both SpellSeed ctors store the same info at +0x28 (Object ctor) and +0x58 (0x7280A0: ebx at 0x7280AB and 0x7280F0;
  0x727FF0: the icon's +0x80 at 0x728003 and 0x728052).
- The filter 0x6F6FA0(thing, type, subtype): type equal (0x6F6FB0), then sub-type 5000 (0x1388, 0x6F6FBA) accepts any
  without asking it, else own == subtype and own ≠ 9999 (0x6F6FCB..0x6F6FD6).
- (inferred) The WorshipSite's +0x8C is its tribe info: the 28-byte records of 0xDA57A8 by tribe.

## Interface interaction levels

SET_INTERFACE_INTERACTION 063 (0x70B220, 16 levels): GInterface+0x28's limit bits (`interface_active`), CameraHelp's
feature mask (`camera_help`, storage only: the camera does not read it yet, pending; the mask is in
[script-camera.md](script-camera.md#player-camera-features-camerahelp)), the hand's reach (fn_0046BF20 with 75 in
JUST_GRAB, 0x42960000 at 0x70B30D, and 1800 in the other levels that set it: the hand's `SetHandReach`; JUST_HAND_MOVE 8
(0x70B4BC), JUST_HAND_INTERACTION 12 (0x70B520, which only sets bit 2) and the invalid levels (9 and > 15, 0x70B785)
leave it), the two key switches; level 9 logs "Unexpected interaction".
GInterface::SetActive (0x5CEDC0): the script's wide screen makes the interface inactive (0x5C6AF4), a hand demo active.

- `SET_INTERFACE_INTERACTION` = `GScript::SetInterfaceInteraction` 0x70B200: POP the level (0x6F6BC0) → 0x70B220.
- `CleanGameForScriptReboot` 0x6EB330 ends a hand demo that is still running. Also [0x6EBC77..0x6EBD10]: GInterface+0x28
  = 0, hand reach 1800, camera traits 0x1BF, ControlMap switches 1 / 1, and then `SetInterfaceInteraction(0)`, which
  writes them all again. fn_005D1260 [0x6EBC67] is not ported.
- `INSIDE_TEMPLE` 0x6FF5A0: `PUSH(g_game +0x205A28 == 1, VMType 6)` (0x6FF5B4).
- `FADE_FINISHED` 0x6FCE40: no fade in progress.

## Script highlights

`ecs::script_highlight` (CREATE_HIGHLIGHT 272, HIGHLIGHT_PROPERTIES, SET_DRAW_HIGHLIGHT, ProcessHighlights 0x70A460 at
turn step 0x54E6D5): the bronze / silver / gold scrolls and the Did You Know sign (info rows at 0xD96390 in the W120
order 0 bronze, 1 sign, 2 silver, 3 gold: the CHL enum and openblack's Enums.h order are wrong for BW1), script type
37. GAME_THING_CLICKED 016 compares GInterface+0x45C, the hand's tap memory (15 s). A script-created highlight is
deleted when its last reference goes, unless RELEASE_FROM_SCRIPT released it (0x70F600..0x70F670).

The script side:
- `CREATE_HIGHLIGHT` (`GScript::CreateHighlight` 0x6F1C20): pops the challenge id (0x6F1C30), the point (z, y, x:
  0x6F1C44..0x6F1C70) and the info row (0x6F1C88: &GScriptHighlightInfo[row] = 0xD96390 + row × 0x110);
  `ScriptHighlight::Create(MapCoords(point), info, challenge, 0.0, 1.0)` 0x709A40 (scale 1.0, 0x6F1C92);
  `AddScriptGameThing(thing, 1)` (0x6F1CC6) and `PUSH(id, 4)` (0x6F1CD7); none → "Highlight not created" (0xC0CC18,
  0x6F1CF5) and 0.
- `HIGHLIGHT_PROPERTIES` 334 0x6FE1F0: pops the DYK_CATEGORY, the text, then the thing ("Thing not valid" 0xC0CD88);
  `ScriptHighlight::SetScriptId(text, category)` 0x709A20: +0x78, +0x84.
- `SET_DRAW_HIGHLIGHT` 306 0x708CB0: GScript +0x80 = the popped value as it is (0x708CCD).
- `GAME_THING_CLICKED` (`GScript::GameThingClicked` 0x70AEB0): a multiplayer game logs "This is not multiplayer friendly
  yet!" and pushes true without the pop (0x70AEC0..0x70AEEE). The test is GInterface +0x45C (`BaseInfo::GetBase`
  0x436B80) == the thing; result pushed at 0x70AFCA (type 6).
- `GAME_THING_CLICKED` on a scroll (vt +0x48C, not IsDidYouKnow 0x70AC20) of the challenges 0x38, 0x3B, 0x3C, 0x3D
  (fn_0070AC50, 0x70AF67): +0x45C cleared (+4 / +8), `SaveGameRoom::InstantSaveGame(14)` 0x792FB0, then tapped again
  (fn_005D36D0: +0x45C = it, +0x468 = the turn).
- `SET_ACTIVE` 255 (`GScript::SetActive` 0x6FD720) tests a ScriptHighlight (vt +0x48C) first.
- `GET_PROPERTY` altitude (0x70E70A): Pos.altitude (+0x1C) of any thing as a float. `SET_PROPERTY` altitude
  (0x70EF9B..0x70EFBF): a ScriptHighlight → `SetDrawHeight(val)` 0x709C40 (+0x7C = 1, +0x80 = h); then, for any thing,
  +0x1C = val and its 3D object moved there with LH3DObject::SetPosition(GetAltitude + altitude, angle, scale).
- The finders see a highlight as script type 37 (0x70AE30), with its info row as the sub-type (fn_006F6D00 case 17).
  FollowUs' CALL_NEAR(HIGHLIGHT, 1, ...) at CHL L53068 / L53085 finds the Did You Know sign.

The class:
- `ScriptHighlight` is 0x8C bytes, vtable 0x94228C, derived from SingleMapFixed. It lives in ScriptHighlight.cpp
  0x7095D0..0x70AE60.
- Highlights are made by scripts (CREATE_HIGHLIGHT 272) or by an abode by its door (`Abode::CreateAbodeSurroundingObjects`).
- `ScriptHighlight::Create` 0x709A40(coords, info, scriptId, yAngle, scale) allocates it with new (0x8C bytes) and runs
  the ctor 0x7098A0.
- The ctor goes through FixedObject 0x52DDC0, clears the fields with fn_00709910, puts the highlight at the head of the
  list g_game+0x205C94 (count +0x205C98, next at +0x88) and sets +0x78 = scriptId (0x7098FB).
- `CallVirtualFunctionsForCreation` 0x709AA0 then runs SingleMapFixed's version 0x52E880, which makes the 3D object.
  `InsertMapObject` 0x52E620 puts it at the head of its cell's fixed list.
- An info index past the 4 rows reads past the array in the original.
- GetSaveType 0x709850 = 0x3F. vt +0x48C (IsHighlight) is 1 for ScriptHighlight (0x709740) and 0 for GameThingWithPos
  (0x4023A0).
- The info rows are 4 GScriptHighlightInfo of 0x110 bytes (GetBaseInfo 0x709640), in info.dat order.
- Row 1 "Script Did You Know Sign" uses MSH_O_INFO_SIGN as both meshes and has no particles. `IsDidYouKnow` 0x70AC20
  tests row index == 1.
- Fields: +0x5C hidden, +0x60 active (IsActive 0x709750), +0x6C the glints effect (info +0x108), +0x70 the active effect
  (info +0x10C) and +0x74 a PSysRenderParticle (0x1C bytes, fn_006CA7A0).
- More fields: +0x78 the script id, +0x7C / +0x80 the draw height flag and value, +0x84 the did-you-know category. +0x64
  is the sprite and +0x68 the glow object.
- +0x78 starts as CREATE_HIGHLIGHT's challenge id. HIGHLIGHT_PROPERTIES then sets it to its text through `SetScriptId`:
  the HelpText of a did-you-know, or the challenge of a scroll.
- +0x5C hidden is kept by Save / Load. No other writer has been found (pending below).

Creation and meshes:
- The mesh is the info's normal mesh (+0x100, through SingleMapFixed::GetMesh vt +0x608) or its active mesh (+0x104,
  GetActiveMesh 0x70A750). An index out of range gives MeshPack[0] (0x709BAC).
- `SetActivated` 0x70A656..0x70A6BE and CallVirtualFunctionsForCreation 0x709B8A..0x709BA4 choose between the two meshes.
- The glow object's mesh is MeshPack[0x20F] MSH_S_BLAST_CENTRE (0x709B07), with SetMaterialProperties {1, 0, 0, 1, 1}
  (0x709B4B).
- The sprite is `LH3DSprite::Create(1, 1)` (one sprite, +0x28 |= 0x80), cell 0, in CreateMaterial(13,
  ".\data\textures\S_SpriteSheet3.raw") with material +5 |= 1 (fn_0057D590, mode 13 pushed at 0x57D5A4).
- At creation (0x709B60..0x709B85), a did-you-know whose text was already read is lit at once.
- At 0x709BC7, unless +0xA & 1, the glints (info +0x108) start at GetLHPoint with the highlight as their target (vt +0x114
  AddTarget_), followed by the +0x74 particle.
- Effects are made with `GJPSysInterface::Create(nullptr, type, GetLHPoint, (0, 0, 0), 1.0, 0)` 0x68E910
  (0x709BDA..0x709C0E, 0x70A6D2..0x70A707). The result is 0 when the row has no particle type or the type has no file.

Turn processing:
- `ProcessHighlights` 0x70A460 runs after Bookmark::ProcessAll and before GClimate::ProcessAll.
- It steps the shared pulse first: [0xD967D4] phase, [0xD967D8] value, [0xD967DC] last value.
- The phase grows by [0xD01A38] ms per turn × 5 [0x942244] × 0.001. It drops by 2π once when above 2π. Then previous =
  value and value = (1 − cos phase) × 0.5 (0x70A463..0x70A4D6).
- Then it walks the list from the head and calls `Process` 0x70A580 (vt +0x5FC) on each available one (vt +0x2C). The
  next link is read before Process runs (0x70A4DC..0x70A506).
- `Process` 0x70A580: with a draw height, Pos.altitude = the draw height. Otherwise it is fn_006022C0(Pos, this, 1), the
  top of what the highlight stands on (0x70A583..0x70A59D).
- A did-you-know not lit yet is checked every 8 turns, by (turn + its unique id) & 7 (fn_00436A80
  UniqueKeyHeap::GetUniqueIdFromAddress). It is lit once its text has been read (0x70A5A0..0x70A5E7).
- `OnClearMap` 0x7096E0 (from GGame::ClearMap, after Bookmark::ClearAll) zeroes the pulse.

Activation and taps:
- `SetActivated` 0x70A630 sets +0x60 at 0x70A643 and closes the old active effect (0x70A63D..0x70A64F).
- It plays sample 0x86 with GAudio::PlaySoundEffect (LH_SamplePlayOptions, bank GGlobal.audio +0x3AC, owner this) at
  0x70A67A..0x70A6B6.
- It then starts the active effect (0x70A6C3..0x70A70F) and sets the mesh on the 3D object with vt +0xF4
  (0x70A712..0x70A732).
- `InterfaceTap` 0x70AC70 is reached from GInterface fn_005D38A0 through packet 0x20 and always returns 1.
- For the local player's tap (GetPlayer() == the local player, 0x70ACA4..0x70ACA9) it calls HelpProfile::Trigger with
  0x22 when +0x78 is set, else 0x23 (0x70AC83..0x70ACCF).
- When the tapping status is GGame::MyInterfaceStatus it plays sample 0xAD, bank InGame (GGlobal +0x3AC), through
  GAudio::PlaySoundEffect 0x429E30 (0x70ACF9..0x70AD4A).
- A did-you-know then goes to fn_0070AC00 → `HelpSystem::SetBubbleProperties(this, +0x78, +0x84)` 0x5C8330 (the bubble,
  below).
- A scroll goes on at 0x70AD68..0x70ADAD. A help task holding the dialogue (HelpSystem +0x45CC,
  ScriptDLL::GetScriptType == 2) leads to GScript::StopHelpScripts 0x6EC780.
- With no dialogue owner it calls Temple::StartScript(+0x78) 0x794A20 → ChallengeRoom::StartScript 0x784C10.
- `InterfaceValidToTap` 0x70ADD0 (vt +0x740, called from fn_005D38A0 0x5D38DB): a did-you-know needs a script id, a
  scroll needs to be active and have a script id.
- `GetOverwriteTapToolTip` 0x70AE10 gives 0xEF2 when the info's OBJECT_TYPE (+0x10) is 1, else 0. Every W120 row is 35, so
  it is always 0.
- The texts already read are kept by fn_0078CC10 (the question) and fn_0078CD20 (the append). Each DYK_CATEGORY 0..4 has
  one list of at most 48 entries (`cmp edx, 0x30`).
- The counts are at [0xE01E6C + 4 c] and the entries at 0xDF9D1C, 0xDF5C58, and so on.
- `ToBeDeleted` 0x709980 takes the highlight out of the list (0x709980..0x7099CF) and closes the glints
  (0x7099D5..0x7099E3), the active effect (0x7099E6..0x7099F4) and the +0x74 particle before Object::ToBeDeleted.

Draw (0x709C60):
- Nothing is drawn while +0x5C is set, or for a scroll while the scripts' SET_DRAW_HIGHLIGHT is 0 (GScript +0x80). The 3D
  object stays where it was (0x709C7C..0x709CB9).
- A lit did-you-know faces the eye. Every other highlight turns (0x709D13..0x709DBD).
- The turn adds g_game_time_inc × 0.00314159 [0x942AF4] (π / 1000 per game ms) to the 3D object's Y angle (its +0x48).
- The angle then wraps by ftol(a × 1 / 2π [0x8C6CAC]) × 2π [0x8AB210] (0x709CE8..0x709DBD). ftol truncates.
- The scale (0x709E5D..0x709EA6): d = ftol(distance from the camera), bound to [10, 30] with unsigned compares.
- A scroll's scale is scale × d × 1 / 30 [0x8CF3F8]. A did-you-know keeps its scale. The camera is read at GCamera +0x118
  / +0x148 / +0x178.
- The 3D object is placed with LH3DObject::SetPosition(point, angle, scale) at 0x709EB7..0x709ED2.
- The centre is the mesh's +0x18 point through the 3D object's matrix, with half extents |M × (mesh +0x24)|
  (0x709ED7..0x709FC4).
- An active scroll's glow (0x709FCF..0x70A10A) uses the rows that face the eye, with the Y row × 3 [0x8C2C50] (its +0x44 =
  3.0), placed at the centre.
- The glow's ARGB (0x70A0BD..0x70A0E6) is 0x14B4DCFF for Silver (row 2). Otherwise it is ((byte)ftol(0.6 t + 0.4) × 0x50)
  << 24 | 0xFFFF00, using [0x942AF8] and [0x8C7A44].
- Because the active pulse is 0.4..1 and ftol truncates, that alpha is 0x50 only at 1.0 and 0 otherwise.
- A scroll is clicked through an invisible sphere of radius r + 0.5 [0x942230] (GInterface::SendInvisibleDrawCollision,
  0x70A2BA..0x70A2DC).
- The sprite is drawn for everything but a lit did-you-know (0x70A2DF..0x70A447, AddDrawing 0x70A447, Screen mode).
- It stands 1.4 [0x8C7E18] radii towards the eye and is 2 [0x94222C] radii wide, at least 0.0001 [0x8BF518]
  (0x70A36C..0x70A3D3).
- The sprite's alpha by row (0x70A3E2..0x70A416): 1 → 0x32, 2 → 0x96, 3 → 0x64, otherwise 0.

The did-you-know bubble (HelpSystem +0x18, Bubble 0xB0 bytes, made by fn_005C8490):
- Drawn by `HelpSystem::Draw3D` [0x5C5B38] → fn_005C83D0 → `Bubble` fn_00576F20, a Z object in the world (0x5767D0):
  GatheringText::DrawBubble 0x8334C0 (9-slice and tail); the text lines in j0 (the bubble's +0xAC) through
  GatheringText::DrawText 0x8315B0 (shadow, then text); gatheringtext.raw goes in CreateMaterial(6).
- `Bubble::SetDefaultSizes` 0x575E20 with (400, 250, 19): the near sizes and the far ones (× 1/7 [0x8FFF3C], 1/35
  [0x9000D4]).
- `GatheringText::GetFrac` 0x831550: 0 at or below a, 1 at or above b, else (v − a) / (b − a). (pending) the a == b branch
  at 0x831589.
- fn_00575EA0(point, offset, place): nothing below a depth of 1 (0x575EE5); the distance fade in below the fade-in end and
  out above the fade-out start, both bounded by the middle of the range (0x575F58..0x575FFB).
- The bubble's lerps are a + (b − a)·f in float, as the exe's `fsub` / `fmul` / `fadd` (0x575F0F..0x575F1D): at f = 1 they
  land a few ulp off b.
- The tail (fn_00576F20 0x5770DB..0x577134): u = 2 sx / W − 1 clamped to −1..1; offset +10 when u <= 0, −10 when u > 0;
  place |u| × 0.6 + 0.2.
- fn_005766E0: drawn 5 m in front of the sign, pulled toward n = 2 × near as the camera looks down: p = clamp(|pitch|, 0,
  π/2) × 2/π, e = max(d − 5, n) − n, depth max(n + e (1 − p²), n).
- `GatheringText::DrawBubble` 0x8334C0(left, top, right, bottom, depth, corner, tailX, tailRight, colour, hasTail, a0,
  a1): a 9-slice of gatheringtext.raw cell (0, 0) on a 4 × 4 grid (u {0, 0.5, 0.5, 1} × 0.25, v {0 or 0.3, 0.5, 0.5, 1} ×
  0.25; jump tables 0x8339A4 / 0x8339B4), corner = min(corner, w/2, h/2), nothing when w or h <= 0.
- Its alpha fades from a0 / 255 at the top to a1 / 255 at the bottom (0x8335AE..0x833614); the tail (cell (1, 0),
  mirrored on the right) is as wide as the corner, kept 1.8 corners from the ends, 0.85 / 0.15 of it left of the point,
  from 0.091 corners above the bottom down, in the bottom's colour.
- Dark colour of the bubble 0xFF3BBDED, at 255 × alpha [0x57714E..0x57715A, 0x576819..0x57683B].
- Text area [0x576A62..0x576B06]. m = margin · 0.333333 [0x8AB26C]; t = (bottom − top) · 0.333333 + top
  [0x576A87..0x576A99]; the bottom b = bottom − m, or t if smaller [0x576A9D..0x576AB6]; the clip, b + m/4; the initial y,
  b + the pixel offset.
- Text parts [0x576B9F..0x576CB3]: a part that starts with $g / $m keeps only that token (4 characters). Otherwise, the
  text up to the first $g (or, if there is none, $m) is taken and then that token; whatever follows is not drawn.
- Line breaking: `GatheringText::DrawText` 0x8315B0 grows a line up to 4 characters at a time while GetStringWidth fits;
  on overflow it backs off to the last whitespace or U+F8FE (dropped) or after a '-' (kept), else one character at a time
  (0x831762); a CR / LF ends the line and is dropped. Lines go from bottom to top [0x576D4E..0x576D66].
- $g<n> (0 < n < 24), fn_005760C0: a square of 2 × size in its own step, with cell n % 16 of S_Gesture<n/16>
  (fn_0057D5D0, CreateMaterial(6) with alpha), a shadow at +2 in the dark colour and then the glyph. The following step is
  a product × 0.7 [0x576C62..0x576C90]. For $m<n>, the key bound to the action and then `DrawKeyOrMouse`
  [0x5761E7..0x5763D1] (fn_005C78F0 → `CameraHelp::DrawKeyOrMouse` 0x447EA0; the icons at 0x57636B..0x5763CC).
- fn_005763E0 clips the quad to [top, clip] and moves its v according to the clipped part (a quarter of the cell per
  side) [0x5765B1..0x576680].
- +0x7C (0 at 0x576AFB; then the height of each part and icon is added to it) is written only by the Z object's draw.
  fn_00576E90 queues nothing while alpha × fade ≤ 0 [0x576ECC], and then +0x7C keeps its last value. Restarting the
  bubble sets +0x7C = 0 (0x5C83AF).
- Bubble arrows [0x5768AF..0x576A5F]: visible while GetTickCount % 750 < 400. With h = box height, s = 0.2·h and k = s/9
  (the float −1/9 [0x9000D8]): arrow +0x94 at top right [0x576927..0x5769BB] and +0x95 at bottom right
  [0x5769CC..0x576A5F]. They are squares of 0.5·s + 2·k in atmos.raw (SetupThing::DrawBox 0x412980 in AtmosMaterial),
  colour {0, 0xFF, 0xFF, 0xFF}, and DrawAlpha = dark colour's alpha / 2 (0x5768D4..0x5768FD).
- Pre-transformed rectangles: SetupThing::DrawBox 0x412980 / DrawAndClip2D 0x82A5B0 (FVF 0x1C4; DrawBox clips at ±0xA000
  with inv_w 100.0).
- The hand drags [0x5772AE..0x5773AF, 0x5772C8] (GInterface +0x3D0 / +0x3AC == 0x1D) and hovers over it (fn_005D5A30); it
  offers the screen object [0x5773A0]; while it stays alive, +0x98 = 3.0 [0x5773A5].
- `SetBubbleProperties` 0x5C8330(thing, text, category): the same thing again closes the bubble (+0x20 = 0); else +0x1C =
  text, +0x20 = thing, scroll +0xA8 = 1000 (the last line), +0x7C = 0; with no did-you-know read yet (the five counts sum
  to 0) it starts "FirstDYKExplained" through fn_006EB7E0 (0x5C834F..0x5C8385), and the text is marked read (fn_0078CD20,
  0x5C8393); a box that closes marks nothing (0x5C83BA).
- fn_006EB7E0(name): a dialogue owner of type Help (2) → StopHelpScripts 0x6EC780; then, with no owner left,
  GScript::StartScript 0x6EB710.
- fn_005C82F0 once a turn: the thing gone (IsAvailable vt +0x2C) or off the screen (fn_0081F1A0 on its mesh, no temple
  test) closes the bubble; else its life +0x98 = 3.0.
- The life ages by g_game_time_inc × 0.0002, not below 0 (0x5773B3..0x5773D9): 3.0 lasts 15 s of game time.
- The scroll is in lines, clamped to the text that does not fit, measured with the text height +0x7C of the last draw
  (0x577141..0x577262).

## Timers, help events and field of view

- Script timers 145..148: a ScriptTimer (script type 0x11) keeps the turn it was set and its length in turns; the time
  left is computed when asked and never below 0. It is the only class deleted when no script variable holds it
  (0x561300). The countdown (084/087/092/099/103/144) ticks once a turn (0x6EB6BA).
- GET_TOTAL_EVENTS 237: HelpProfile's 49 counters (g_game+0x250060), each at most once a turn, not while paused or
  while a script holds the wide screen; reset by HelpProfile::Process (0x5C4660, after HelpSystem::Process). Writers:
  the camera 25..30 (the player's camera model), casting 9..22, the hand 1..8, 5/6 (buildings),
  the creature (pending). API `help_profile::Trigger(Event)`.
- GAME_THING_FIELD_OF_VIEW 011 / POS_FIELD_OF_VIEW 012: `Graphics/RegionOnScreen` (sphere: LH3DBoundingBox::
  CheckRegionOnScreen 0x868C80; point: fn_0081F1D0); both false inside the temple. 011 (0x6F8130) in a multiplayer game
  pushes 1 without popping; no thing → "Object no longer valid" and 0. Details in
  [script-camera.md](script-camera.md#opcodes).

Script timers (ScriptTimer.cpp):
- A `ScriptTimer` is vtable 0x8E0BAC, 0x30 bytes, made by fn_007115A0: a GameThingWithPos at (0, 0, 0) (SetToZero
  0x5705D0).
- +0x28 is the game turn it was set at (g_game +0x205A40). +0x2C is its length in turns.
- It is never processed: it follows the game turns, so it stops in pause and follows the game speed.
- GetScriptObjectType 0x711600 = 0x11, IsScriptTimer 0x5612F0 = 1, GetSaveType 0x561310 = 0x7D. On release:
  GScript::ReleaseScriptThingIntoTheGame 0x70F61E..0x70F670.
- Elapsed turns: fn_007116C0 = g_game +0x205A40 − +0x28 (unsigned).
- Time left: fn_00711670 = (+0x2C − elapsed, signed, 0 below 0) × [0xD01A38] × 0.001 [0x8AA3B0].
- Time since set: fn_007116D0 = elapsed × [0xD01A38] × 0.001. It keeps growing after the timer has run out.
- SetTurns 0x711610..0x711622 writes the current turn to +0x28 and the length to +0x2C. 0x711630(seconds) sets the
  length in seconds.
- `CREATE_TIMER` 146 0x6F1D20: POP the seconds; fn_007115A0; `AddScriptGameThing(t, 1)` and PUSH it. CREATE 027 with
  SCRIPT_OBJECT_TYPE_TIMER makes one too (fn_006F11A0 0x6F1253, the sub-type as the seconds, fild qword).
- In both cases fn_007115A0(seconds) does Base::new(0x30, "ScriptTimer.cpp", 13) and the caller adds it to the script.
  Out of memory gives 0 and "Thing not created".
- `SET_TIMER_TIME` 0x711280: POP the time, then the thing (none → "Object no longer valid"). A ScriptTimer starts again
  from this turn (0x711318..0x711324, 0x71131C; 0x711630); anything but a timer or a dispenser is "Invalid script
  thing".
- `GET_TIMER_TIME_REMAINING` 0x711370 (type check 0x7113EE): none → "Object no longer valid" and 0.0; not a timer →
  "Invalid script thing" and 0.0; else fn_00711670 (the seconds left, 0 once out). `GET_TIMER_TIME_SINCE_SET` 0x711410
  (type check 0x71148E): none or not a timer → the error and FLT_MAX ([0x95722C]); else fn_007116D0.
- `ScriptTimer::Save` 0x711700 / `Load` 0x7117B0 write +0x28 then +0x2C (4 bytes each), after GameThingWithPos's own
  block.

The countdown timer (GScript's, at g_game +0x250090):
- Its fields: +8 on, +0xC turns left, +0x10 shown.
- `GScript::InitialiseCountDownTimer` 0x6EB8B0 (START_COUNTDOWN_TIMER 084, 0x711150) sets on and shown first
  (0x6EB8B0..0x6EB8C8).
- Seconds ≤ 0 is the script error "Invalid time for timer" (0xC0C0A4), which does not stop the script. Below 0 counts as
  0 (0x6EB8DF..0x6EB8F0).
- turns = ftol(1000 / [0xD01A38] × seconds) (0x6EB8F8..0x6EB91E).
- REMOVE_COUNTDOWN_TIMER 087 0x711180 clears only +8 (0x71118B). The turns and +0x10 stay.
- `GetCountDownTimerRemainingTime` 0x6EB950 (GET_COUNTDOWN_TIMER 092, 0x7111A0) = turns left / (1000 / [0xD01A38]). Both
  are unsigned integer divisions, so the result is whole seconds. It reads +0xC even when the timer is off
  (0x6EB953..0x6EB977).
- A [0xD01A38] above 1000 would divide by 0 (inferred).
- COUNTDOWN_TIMER_EXISTS 099 0x7111D0 pushes +8. HIDE 103 0x7111F0 / REVEAL 144 0x711210 write +0x10.
- fn_006EB930 (from GScript::Process 0x6EB6BA) does `dec [ecx+0xC]; jne` while the timer is on, and turns it off at 0. A
  0-turn countdown goes on below 0.
- The display (GGame::Process3dEngine 0x54E449..0x54E4B4) is drawn while on and shown (0x54E44F..0x54E45B).
- It is sprintf "Time: %.1f" of the time left, at (320, 90), size 24.0, through CreatureMentalEditor::DrawTextA 0x4DF310.
- Below 10 s ([0x8AB414], 0x54E47B) 0xFF goes in the first colour argument, otherwise in the second. The arguments are r,
  g, b (inferred), so it is red below 10 s and green otherwise.

HelpProfile (g_game +0x250060, `HelpProfile::Create` 0x5C4500 from GGame::Init 0x54F4F3):
- One CameraHelpAccumulator (0x10C bytes) per HELP_EVENT_TYPE (names at 0xBF084C): +0 total count, +4 smoothed rate, +8
  ring head, +9 slots used (<= 64), +0xA triggered this turn, +0xC 64 trigger times in AccumulatedTime ms [0xC5AFD8].
- `CameraHelpAccumulator::Trigger` 0x449040(now): ++count, times[head] = now, head = (head + 1) & 63, used = min(used + 1,
  64).
- HelpProfile::Process 0x5C4660 (GGame::ProcessTurn 0x54E6A9, after GScript::Process and HelpSystem::Process): nothing
  while paused, in a script's wide screen or with [0xC4CCEE]; else ProcessSpecialTriggers, rate += (flag − rate) × 0.005
  [0x8C7674] and flag = 0 for the 49, AccumulatedTime += 100, and the same step for CameraHelp's 12 (fn_00449240).
- `HelpProfile::Trigger` 0x5C46E0(type) also counts 14..23 in GestureTotal (24), 1..42 in AllInterface (43), 9..11 in
  CastAll (11) and every type in AllEvents (48).
- `ProcessSpecialTriggers` 0x5C45A0 (only with the player's CameraModeNew3): the screen centre on the land → LookAtLand
  (44), and with heading distance < 15 [0x915448] and pitch > 0.55 [0x91544C] → LookAtLandTooClose (45); off the land with
  pitch < −0.3 [0x915450] → LookAtSky (46).
- `CameraHelp::CameraHelpCallback` 0x449140(reason, point, inputs): 0x3nn → Trigger(25 + nn) and each input bit into
  CameraHelp's input table (0xC5A320); 0x2nn → 0xC5A860 + nn; 0x1nn → 0xC5AC90 + nn; the point is not read. Only
  CameraHelp's debug page (fn_005C47A0) and its tooltips read those tables.
- Reasons: 0x100 CameraExclusion::InsideExclusion (0x45DF7D), 0x101 InsideInclusion (0x45DF2C), 0x102 (0x45C05D);
  0x200..0x203 from CameraModeNew3 (0x45C235, 0x45F77F, 0x45FC10, 0x45FE4A…).
- CameraModeNew3::Update's input mask (0x45C06A..0x45C0C5, names at 0x9CDDF0): 0x01 keyboard, 0x04 both mouse buttons,
  0x10 wheel down.
- The camera's events (0x45C38E..0x45C838): Zoom when the zoom delta ≠ 0; Rotate when |yaw| > 0.01 (the double
  [0x8C7A10]), then RotateCW (yaw > 0) or RotateCCW (yaw < 0); Pitch when the pitch delta ≠ 0. The original also needs the
  mode +0x8C to be 2 or 3 and, for the pitch, no auto-pitch **(approximate)**.
- GET_EVENTS_PER_SECOND 235 (0x70B7F0): POP the HELP_EVENT_TYPE; GetTriggerPerSecond 0x448FC0 = 0 with fewer than 2
  times, else (used × 1000 − 1000) / (now − oldest time); a negative span clamps the ring (fn_00448F90) and gives 0.
- GET_TIME_SINCE 236 (0x70B880): `CameraHelpAccumulator::GetTimeSinceLastUsed` 0x448F40 = 0 if never, else (now − newest
  time) × 0.001; a time in the future clamps the ring and gives 0.
- GET_TOTAL_EVENTS 237 (0x70B910): the count `fild [+8 + type × 0x10C]` (TeachRotate 25, TeachPitch 28, TeachZoom /
  TrackZoomUsage 29, DoubleClicking 30 / 31).
- The three readers take types 1..48 only (0x70B924..0x70B941); another gives the script error "Invalid event"
  (0xC20664) and 0.0.
- The counts belong to the player profile: Save 0x5C4820 / Load 0x5C4830 write and read the 49 accumulators (0x334C
  bytes) to "<user path>\helpstats.dat" (GGame::Save 0x554569, GGame::Load 0x554D1A, PlayerProfile::GetProfileByName
  0x66BC9A, WriteBackToRegistry 0x66BDFD). (not ported)
- `HelpProfile::SetToZero` 0x5C4770 (from its ctor): the 49 Reset (0x448F20: +0, +4, +8, +9, +0xA; the ring stays), then
  CameraHelp::ResetStats 0x4491E0; AccumulatedTime is a static and keeps going.

## The family in high detail (SuperVillager)

SET_HIGH_GRAPHICS_DETAIL 290 (0x708CE0) makes a villager a SuperVillager (fn_00825F20): its mesh is swapped for the
high-detail one — `Data\MISC\Intro\nors_man.l3d` (501, father), `nors_woman.l3d` (498, mother), `nors_boy.l3d` (439,
son and every Celtic / Norse male child), `Data\MISC\sable.l3d` (420, the creature trainer, info.dat row 77) — with
22 bones identical to the villager meshes', so the villagers' own clips drive them unchanged. Its own draw path
(fn_008254A0 → fn_00825530): a 300 ms clip cross-fade, a second yaw smoothing at 5π/4 rad/s (turned on a local copy of
the drawn matrix, 0x8255AB), the default sun as light, no haze, and the eyes: one `eye_ball.l3d` drawn twice and four
lids on bone 8, blinking every Random(1000..5000) ms (the CRT stream) with a shared glance. Released every frame unless
a script holds the wide screen (0x5E4B3A). THING_JC_SPECIAL 349 gives the cinematic's orders (7 follow the intro
hand's grip, 8 snap, 9 mirror the yaw, 16/17 ±π/2). Drafted in openblack (`ECS/SuperVillager*`).

Its shadow is a TemporaryShadow (fn_00825090, list 0xEB99FC): a projected shadow (ShadowInfo, rendering.md) lit by the
fixed sun 0xEA1C88 and falling on the land only, updated by GLandscape::Draw 0x5E4BFB. While it lasts the villager loses
its ground blobs (SetHumanShadowed(0)) and its fade with the distance (SetDisappear(0), the same as feature 19); both
come back with it (fn_008250F0). openblack: a `components::DynamicShadow` on the thing, as the launched boat's. The
eyes are composed in the world, not through the camera space and [0xEA9DE0] as the exe does: the same matrix to a few
ulp until the LH3D camera matrix is ported bit for bit.

- A SuperVillager is 0x34 bytes, in the list g_first [0xEB9A08] (newest first).
- `SET_HIGH_GRAPHICS_DETAIL` 0x708CE0: POP the object (0x708CEF), then the bool (0x708D0A); no thing → "Thing not found!"
  (0xC0CFAC, 0x708D13). With "off" it calls fn_00825440 (Release).
- `THING_JC_SPECIAL` (`GScript::ThingJCSpecial` 0x709000): POP the object, the feature (0x70902C), then the bool
  (0x70903E); no thing → "Object no longer valid" (0x709047).
- Release is fn_00825440 / `SuperVillager::Release` 0x826180. fn_00825E70 deletes the Eyes whatever the thing's state
  (fn_00884570).
- Every SuperVillager is released and the list emptied from four places: GGame::ClearMap 0x552BE7,
  CleanGameForScriptReboot 0x6EBBC2, the script camera release 0x6ECEA6 and GLandscape::Draw 0x5E4B6D.
- The on-screen test: fn_00825400 0x82541D runs LH3DBoundingBox::CheckRegionOnScreen on the drawn mesh's box. Only when it
  passes does fn_00825530 run (0x825422 je 0x82543D): the fade, the yaw stage and the eyes, Random draws included.
- The feature-7 position is set in the driver loop at GLandscape::Draw 0x5E4BC8..0x5E4BEE, after Villager::Draw
  0x5E4BC2.
- The HD mesh comes from `LH3DMesh::CreateFromHD(path, 0)` 0x8067F0. The EBone block is flagged 0x200000 (0x826004 /
  0x826122).
- fn_00825530 0x82555D lights the drawn position with fn_00801C90 into the host's +0x4C / +0x50 (both written on every
  path, 0x8020E4 / 0x802107..0x802109).
- fn_00825530 calls no fn_007FEB30, so the haze that Villager::Draw 0x5E4BC2 put there is overwritten and the family has
  no haze.
- The root of the bone buffer [0xC37D9C] is fn_00825530's turned local copy of the matrix (0x8257CD..0x8259AC).
  fn_00883560 reads that buffer after the body.
- Each SuperVillager's eyes (fn_008254A0, in fn_005E5CD0 +0x585 = 0x5E6255) are drawn after its body, on screen only,
  after the fish (0x5E4B2B) and `PetitNavire::PostDraw` 0x5E6250. Before that, the draw loop draws the `Random(0, 2π)` of
  the swimming rings [0x5E4CB4..0x5E4CC2].

The eyes (`Eyes`, 0x54 bytes): ctor fn_00884100, update and draw fn_00883560, lids fn_00883EA0, dtor fn_00884570.
- The files are in Data\MISC\Eyes, slots 0..4: r_paupe_up, r_paupe_down, l_paupe_up, l_paupe_down, eye_ball
  (0xC3A21C..0xC3A340).
- Eye types 0 and 1 use the same files. Type 2 uses the "*2" ones (0x88415F..0x884393).
- Each file is `LH3DMesh::CreateFromHD(Data\MISC\Eyes\<file>.l3d, 1)` (0x884431).
- The eyeball's first material texture becomes [0xEA1A90] .\data\Textures\misc0.raw (0x884456..0x884466).
- Each lid's first material texture becomes the host mesh's first material texture (0x884491..0x88449F; mesh +0x10 [0]
  +8 [0] +8, 0x88440B..0x884415).
- For 0 ≤ type < 3, fn_008840E0 doubles every vertex uv of every primitive (fadd st, st; 0x8844B8..0x884533).
- Objects (0x8843E3..0x884553): LH3DObject::Create(0), vt+0x98(0) and vt+0x58(0), which turn off obj+4 bits 8 and 0x20.
  The meaning of those bits is pending.
- The eyeball's SetAnimatedUV is (k / 8, 0.75), with the iris cell k = {4, 3, 5}[type] ([0xC3A208], ctor 0x88446B, fmul
  [0x8AB620]).
- The lids' SetAnimatedUV (fn_00884000) is zeroed by fn_008840E0 for these types, so they get none.
- The eyeball (+0x4C) is one object drawn twice (0x883B22 eye 0, 0x883E6B eye 1). Slots +0x3C..: 0 r_up, 1 r_down, 2
  l_up, 3 l_down.
- Timers: +0x38 hold = 200 at creation (0x884153). +0x28 is zeroed when a blink ends (0x8835AA); no reader has been found.
- The shared state: glance target [0xFAA7E8], squint [0xFAA7EC], squint clock [0xFAA7F0].
- The step (0x88356A..0x883712) starts with closure = 0 (0x883574). Closing and opening take 50 ms each (0x8835B2 /
  0x8835D0), and the blink ends after hold + 100 ms (0x883583).
- The closure changes by 0.02 [0x8CB930] per ms.
- When a blink ends, the next hold is Random(100, 200) ms (ftol, 0x88358D..0x8835AD). When the countdown drops below 0 a
  blink starts and the next is Random(1000, 5000) ms away (0x883614..0x88363F).
- The glance moves towards the shared target at 0.7 [0xC3A214] rad/s, scaled by 0.001 [0x8AC418] (0x883642..0x8836C0).
  On arrival there is a new target, Random(−0.25, 0.25) (0x8836AF / 0x8836B6).
- The squint is Random(0, 0.3) (0x8836E2) every 300 ms [0xC3A218] of its clock. The clock resets to 0, not to −300
  (0x8836C6..0x883712). The squint is the closure's floor.
- Eye k's matrix (0x883712..0x883802; eye 1 at 0x883BD6..0x883C07) is v R E_k bone, in rows.
- The bone is copied from [0xC37D9C] + 0x30 × (+0x304) (0x88371C..0x883735) in camera space and taken back to the world
  with [0xEA9DE0] = SetInverse(g_world_to_clipping [0xEA9E40]) (0x819BB0; fn_007FAFF0 0x883742). Then come E_k
  (fn_007FAE60 0x883753) and R (0x883802).
- The ground blobs' fn_0081FFF0 makes the same round trip (0x8200E5..0x82065E).
- R (0x883758..0x883802) uses c = cos π and s = sin π of the double 3.1415927410125732 [0x8D45D0]. Its rows are (1, 0,
  −s), (0, 1, 0), (−s, 0, −1): a mirror of z (det −1), not a turn.
- The glance (0x883A98..0x883B1A) turns the eyeball about its own Y when the glance is not 0: r0' = c r0 + s r2, r2' = c
  r2 − s r0. c is stored as a float (0x883AB5; eye 1 0x883DF6) and s stays on the FPU stack.
- The lids (fn_00883EA0) turn rows 1 and 2 by closure × 0.47 [0x8C7A4C] (upper) or closure × −0.35 [0x9A3D80] (lower):
  r1' = c r1 − s r2, r2' = c r2 + s r1 (0x883ED5..0x883F42).
- c is stored as a float there too (0x883ECF upper, 0x883F72 lower). The upper lid is slot 2k and the lower is 2k + 1.
- The shade (0x883865..0x8839CE; eye 1 0x883C0C..0x883D5B): the table 0xC3A1D8 (type × 16 + eye × 8) gives the yaw a and
  pitch b of the eye's lighting normal n = (sin a cos b, −sin b, −cos a cos b).
- cos a, −sin a and cos b are stored as floats (0x88386F, 0x88387C, 0x883889; eye 1 0x883C1B, 0x883C28, 0x883C35). sin b
  is rounded by `0 + sin b` (0x8838C9 / 0x883C71).
- n M is summed as (y r1.x + z r2.x) + x r0.x for both n0 and n1 (0x88390F..0x883950; eye 1 0x883CB9..0x883CE4).
- n2 is (x r0.z + z r2.z) + y r1.z for eye 0 (0x883950..0x88396C) but (z r2.z + y r1.z) + x r0.z for eye 1
  (0x883CE7..0x883CF8).
- |n|² is (ny² + nz²) + nx² for eye 0 (0x88396F..0x883982) and (nx² + ny²) + nz² for eye 1 (0x883CFB..0x883D0F). It is
  normalised with InverseSquareRoot 0x841170 (0x88398B / 0x883D18).
- The default sun [0xEA1C88] is normalised as (y² + z²) + x² (0x88381B..0x883838; InverseSquareRoot 0x883841; stored
  0x883846..0x883860).
- The dot is (nz sz + ny sy) + nx sx (0x8839A9..0x8839BD / 0x883D36..0x883D4A), then I = fistp(255 × dot).
- Colour (0x8839D0..0x883A3B): the host's colour × the eye's factor, alpha kept. The host's specular goes to all five
  objects. They are written into the objects' +0x4C / +0x50 (0x883A3D..0x883A82), which are drawn instead of the land
  light.

## JC specials and the confirmation sounds

PLAY_JC_SPECIAL 326 in the intro: 0 the light falling from the sky on the son (20 sprites from 4000 to 10 units at
0.45 u/ms), 1 / 2 the engine's debug camera following it, 4 / 5 the intro hand (hand_intro.l3d) and the son in its
grip, 14 / 15 bookmarks on / off, 18 nothing, 6 the missionaries' boat (water.md). START_ANGLE_SOUND 285 / 348 are
GConfirmation (yes / better samples of HelpSprites); silent until openblack's camera feeds them (pending). Drafted in
openblack.

- `PLAY_JC_SPECIAL` 14 (0x708F5A) turns the bookmarks on ([0x9CD384] = 1), 15 (0x708F66) off; only `FollowUs` uses 15
  (L50058). 7..13 and above 15 do nothing (0x708F70; `FollowUs` passes 18 at L52177).
- [0x9CD384] is 1 from InitStaticsValues 0x54A81E and CleanGameForScriptReboot 0x6EBD04 and is saved by Bookmark::SaveAll
  0x43A49C; the bookmark keys (GGame::ProcessKey 0x63F46B) and the bookmarks' drawing (fn_00439B90 from fn_005E5CD0
  0x5E6180) only work while it is set.

The intro hand and the light (JCMisc.cpp's `Intro`):
- PLAY_JC_SPECIAL 0, 1, 2, 4 and 5 go to fn_005DF9C0. The per-frame update is fn_005DF640 (from fn_005E5CD0 0x5E6241).
- The hand's grip is fn_005DFCE0 (GLandscape::Draw 0x5E4B35). `Intro::ReleaseAll` is 0x5DFC40.
- The sequence: 0 makes the light. 1 puts the drawn camera behind it (LH3DTech's debug camera mode 2,
  [script-camera.md](script-camera.md#shake)). 2 gives the camera back, 4.5 s later in FollowUs.
- When the light lands, the intro hand appears at the Son's spot and plays Data\MISC\hand_intro2.anm (0xBF3298;
  "Hand_Pick_Up_Swimmer", 766 ms, one shot).
- Then 4 puts the hand at the boat with Data\MISC\hand_intro.anm (0xBF32D0, set with vt+0x180; "Hand_Put_Down_Swimmer",
  3066 ms, looping).
- 5 plays that clip with the Son in its grip (feature 7). The second 5 jumps to 2379 ms (0x5DFBCE) and lets the Son go
  (0x5DFBC6). At the end of the clip the hand goes.
- The globals: [0xD19C88] the light, [0xD19C8C] the material, [0xD19C90] the debug camera follows the Son's spot,
  [0xD19C98] the hand object, [0xD19C9C] the hand mesh.
- More globals: [0xD19CA0] the clip (fn_00839900, 0 when none), [0xD19CA4] playing, [0xD19CA8] the pick-up yaw (never
  written, so 0), [0xD19CB0] the pick-up wait, [0xD19C34] follow the Son, [0xEB9A78] the light timer, [0xBF2AFC] the
  state (initially −1).
- The hand is `LH3DObject::Create(2)` 0x80B4D0 (an animated object), then vt+0x58(1), then
  `LH3DMesh::CreateFromHD("Data\\MISC\\hand_intro.l3d", 0)` 0x8067F0 (string 0xBF32B4), then SetMesh vt+0xF4
  (0x5DFB00..0x5DFB3B / 0x5DF67B..0x5DF6C1).
- The hand is freed through vt+0x4 (0x5DF8DE / 0x5DFC70), fn_00839970 and LH3DMesh::Release 0x806D00.
- The Son's spot is [0xD19A28] (initialiser 0x5DF620). The put-down hand is at [0xD19A38] (initialiser 0x5DF5F0).
- The pick-up hand in state 12 stands at [0xD19A18], 1.5 below the Son's spot, set once (fn_005DF640
  0x5DF75B..0x5DF76F). Its yaw is π [0xBF2B00] (0x5DF830).
- The hand's scale is 0.008 [0xBF2AF4] (also written to [0xD19A14] at 0x5DF83B).
- The put-down yaw is −π/4 [0x8C79E4] − 15 [0xBF2B04] × π/180 [0x92B20C], each step a float (fn_005DF9C0
  0x5DF9C3..0x5DF9DD).
- Special 0 (0x5DF9EE) makes CreateMaterial(13, [0xEA1A90]) once. It clears [0xD19C34] at 0x5DFA5C (0x5DFA72 when the new
  fails).
- The light's direction is set before it is normalised at 0x5DFA3F..0x5DFA4F.
- Special 1 (0x5DFA7D) runs only in state 0. It sets the debug camera focus 0xEA1B68 = [0xD19A28], sets [0xD19C90] = 1
  and puts the debug camera's position [0xEA1B58] at the light's start (0x828760).
- Special 2 (0x5DFAD5) runs only in state 0.
- Special 4 (0x5DFAF5) frees an old clip first (fn_00839970, 0x5DFB44). The clip time is set to 0 with vt+0x188(0)
  (0x5DFB81) and the position with vt+0x20 (0x5DFBA0).
- Special 5 (0x5DFBB2) runs only in state 4 and sets playing at 0x5DFBE4..0x5DFBEE.
- The update, state 0 (0x5DF92D): the light falls. When it is done it is deleted (fn_008282E0 + delete, 0x5DF937).
- When the light has landed (hold, 0x5DF968) the pick-up hand comes. The camera follow is at 0x5DF978..0x5DF99C.
- State 12 (0x5DF66F) is the pick-up hand: [0xD19CB0] and [0xD19CAC] (never read) are cleared at 0x5DF680. Once the light
  is gone (0x5DF711), [0xD19CB0] = 1000 (0x5DF731) and the clip may run (0x5DF7B0, time at 0x5DF81B).
- When the pick-up clip wraps it sets [0xD19C94] (0x5DF807). It is never cleared.
- State 4 (0x5DF865) is the put-down hand. When its clip wraps everything goes except [0xD19C34] (0x5DF8DC..0x5DF910).
  The clip time is written at 0x5DF921.
- The clip time (0x5DF7EC..0x5DF803 / 0x5DF8A8..0x5DF8D2): n = time + ms. A looping clip (+0x50 & 0x100) gives n %
  duration (signed idiv), otherwise min(n, duration − 1). It wraps when n < time (0x5DF805 / 0x5DF8DA jge).
- The grip (fn_005DFCE0 0x5DFD03..0x5DFEB2) needs a hand, [0xD19CA4] set, its time ≤ 1817 ms ([0xBF2B08],
  0x5DFD05..0x5DFD1E) and its mesh with an EBone block (flag 0x200000). The check is at 0x5DFCE0..0x5DFD9F.
- The hand is posed with GetPose 0x839980(bones [0xC37D9C], mesh, time, +0x14) at 0x5DFDA1..0x5DFDD2.
- Then bones[EBone +0x304] times the EBone's matrix 0 (fn_007FAE60) gives the EBone point through the bone
  (0x5DFE65..0x5DFE8A). Its y is lowered by 0.65 [0xBF2B0C] (0x5DFE93..0x5DFEB2).
- GLandscape::Draw 0x5E4B30..0x5E4BEE draws the feature-7 SuperVillagers at the grip while [0xD19C34] is set: from the
  first special 5 to the second, or from special 0.
- That read happens before the update, so it sees the hand as the last frame left it (0x5E4BCE).
- When fn_005DFCE0 writes nothing, the original reads a stale stack slot (approximate).
- `Intro::ReleaseAll` 0x5DFC40 is called by THING_JC_SPECIAL 18 (0x70910E) and CleanGameForScriptReboot 0x6EBBFB.
- It frees the light (0x5DFC41..0x5DFC5E), the hand object, mesh and clip (0x5DFC64..0x5DFC9D) and the material
  (0x5DFCA3..0x5DFCBE). It sets [0xD19CA4] = 0, turns the debug camera off ([0xEA9EC8] = 0) and sets the state to −1.
- It keeps [0xD19C34], [0xD19C90], [0xD19C94], [0xD19CB0] and the light timer.

The light of special 0 (fn_00827F20(target, direction, material) 0x827F20..0x8282CF, 0x34 bytes in [0xD19C88]):
- It is 20 LH3DSprites (`Create(20, 1)` 0x8404A0: SetToZero 0x8404F0, then +0x28 |= 0x80 for the sprite's own material;
  0x828000..0x8281E3).
- The direction is normalised as (x² + y²) + z² with InverseSquareRoot 0x841170 (0x827F41..0x827F87).
- start = target + direction × −4000 [0x9A3950], written to both +0x04 and +0x10 (0x827F9A..0x827FF8).
- Sprite i is placed at start + direction × (−i × 6) (0x82804D..0x8280A3).
- Sprites above 5 use cell 50 at double size (0x82806D, 0x8280AE..0x8280B7, fadd 0x8280DD). The others use cell 48
  (0x8280F0..0x8280F9).
- The size is float(15 (19 − i)) × 0.05 + 3, at least 1e-4 (0x828108..0x82812E).
- Each sprite gets the angle Random(0, 2π) (0x828136, 0x828151) and the colour white with alpha (19 − i) × 255 / 20,
  truncated (0x82815C..0x828186).
- Sprite 18 is the glow at the head, 80 [0x8D060C] wide with alpha 0x28 (0x828183, 0x8281AB).
- Sprite 19 (0x8281E9..0x8282B9) is a thin streak 350 [0x9A3948] × 17.5 [0x9A3944] (half sizes, 0.05 at 0x8282A9), 6
  [0x9A394C] behind the start.
- The streak is turned −π/8 (0x828257), coloured 0x0EFFFFFF (0x828269) and uses cell 49 (0x828280..0x828286).
- Creation ends with +0x28, +0x2C and +0x30 = 0 (0x8282BC..0x8282C4).
- The head (fn_00828350): d = elapsed × 0.45 [0x9A3958]. Past 3990 [0x9A3954], d = 3990 and +0x30 = 1 (0x828369).
- 0x828760 (special 1) copies the head into +0x04 and into the debug camera's position.
- It is one Z object (fn_00828300 from fn_005DF640, fn_005E5CD0 0x5E6241), key |head − g_camera|² summed (x² + y²) + z²
  (0x828300..0x828336); its callback 0x8283D0 draws LH3DSprite::Draw sprites in [0xD19C8C] (mode 13, CULLMODE 3 from +5 =
  0 at 0x840C08..0x840C27), ZFUNC 8 while depthAlways (0x828483 / 0x828673), else 4.
- The callback, falling state (0x828500): sprites 18 and 19 stay at the head (0x828556) and the others follow at −i × 6
  (0x828572..0x8285CE).
- Sprite 19 turns by g_game_time_inc × 0.00015708 [0x9A395C] rad (0x82852D).
- When the game time stands (0 ms), there are no Random draws (0x8285D6). Otherwise every sprite gets a new angle
  Random(0, 2π) (0x8285E1).
- The five front sprites flicker with float(15 (19 − i)) × 0.05 + Random(−2, 2) + 3 (0x8285E6, 0x8285F1..0x828649).
- After 8237 ms of fall, ZFUNC ALWAYS (8) is set around the 20 draws (0x828656..0x8286CE, 0x828659). LESSEQUAL (4) comes
  back after.
- Then elapsed += ms (0x8286D4). The debug camera's position is put 150 [0x8CC7E8] back up the beam and 30 above
  (0x8286D7..0x828746).
- On arrival the light holds for 500 ms [0xC383E0] (0x82850E, 0x828519; hold code 0x8283F0..0x828418). The light timer is
  [0xEB9A78].
- Then it fades over 1000 ms [0xC383E4] (0x828412): alpha = ftol((timer / 1000) × (255 − 50) + 50), with 50 [0xC383E8] as
  the floor (0x82841E..0x828470).
- In hold and fade (0x828473..0x8284FB), sprite 0 is drawn three times with ZFUNC ALWAYS and a new Random(0, 2π) angle
  after the first and the second.
- The callback's CRT Random draws are at 0x8284A8, 0x8284C0, 0x8285E1 and 0x8285FB. They run when the Z queue is drained.
- The sprites are drawn with `LH3DSprite::Draw` 0x840530 in mode A: in the plane of the screen, nothing at or before the
  near plane.

## Test hooks

- `OPENBLACK_TEST_TEXT_CLICK=1` clicks the texts that wait for a click;
  `OPENBLACK_TEXT_TRACE=1` logs the texts; `OPENBLACK_HAND_DEMO_TRACE=1` the hand demo.
- `OPENBLACK_TEST_TEXT_SHOT="id,path[,ms]"`: a screenshot `ms` (1000 by default) after text `id` is shown, e.g.
  `4431` ("Saludos.", both advisors on screen).

## Addresses of the original, moved out of the code

The code's comments describe what it does in plain words; the original's addresses and function names they used to quote are kept here, next to the openblack symbol each one corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `[0xC383D8]` | the SuperVillager clip cross-fade length, 300 ms | `super_villager::k_FadeMs` |
| `[0x9A392C], fn_00825530 0x8255E1` | the SuperVillager drawn-yaw turn rate (float 0x407B53D2 = 5 pi / 4 rad/s), read by the draw path | `super_villager::k_TurnRate` |
| `SuperVillager +0x14 / +0x18 / +0x1C / +0x20..+0x28` | the smoothed yaw (set from obj+0x48 at 0x825FBC, Villager::Draw's +0x108), the thing, its Eyes, the three saved meshes | `Entry / components::SuperVillager` |
| `[0xBF3584]` | the swim ring period, 1000 ms | `super_villager::k_SwimRingEveryMs` |
| `[0xC37D9C] +0x114 / +0x11C` | bone 5's x and z in the bone buffer (5 x 0x30 + 0x24 / + 0x2C), where the swim ring is made | `super_villager::k_SwimRingBone` |
| `LH3DObject +0x98, GetDetailMesh(2, 1, 0)` | the high detail slot SET_HIGH_GRAPHICS_DETAIL compares (set by Villager::SetAge 0x7528C0) | `HighMesh` |
| `fn_00825E70: obj+0x88..+0x94` | the release clears the drawing object's cross-fade state | `ClearSmoothing` |
| `fn_00825F20 0x825F49` | making a SuperVillager returns early when the thing is one already (the HD mesh leaks) | `Create` |
| `fn_00825E40` | links a new SuperVillager at the head of g_first [0xEB9A08] | `Create / testing::Adopt` |
| `0x825F85..0x825FB5` | TemporaryShadow creation: ShadowInfo holder with si+0xC = 1 (land only, fn_008745A0), holder+4 = 1 fixed sun (0x825FB5); fn_00825090 calls vt +0xC4(0) (IsHumanShadowed off) and vt +0x98(0) = fn_007F98E0 (IsDisappear off) | `Create (DynamicShadow)` |
| `0x708DE7..0x708DEC` | SET_HIGH_GRAPHICS_DETAIL stores the thing (+0x18), sets the shadow object's +0x10 = 0 so TemporaryShadow::UpdateAll 0x825190 skips it, DrawListRebuildCount = 1 | `SetHighGraphicsDetail` |
| `0x825FC5..0x825FF8, 0x825C8A` | the three meshes saved at +0x20..+0x28, then vt+0xF4(hd, 0, 0) with an HD mesh; the draw uses the high slot | `Create` |
| `0x825FFB..0x82615D` | Eyes made when the high slot's mesh has an EBone block and the eye type is not -1 | `Create` |
| `0x826183..0x8261A1` | SuperVillager::Release gives the three meshes back | `Restore` |
| `fn_00825E70 0x825E79 -> fn_00825130 -> fn_008250F0, fn_008745E0` | the TemporaryShadow's holder freed and the saved flags given back (vt +0xC4(+8) IsHumanShadowed, vt +0x98(+0xC) IsDisappear) | `Restore` |
| `fn_00825E70 0x825EDF..0x825EF6` | deletes the Eyes whatever the thing's state | `ReleaseEntry` |
| `fn_00825400 0x825403..0x82541D` | on-screen test: CheckRegionOnScreen 0x868C80 (path [0xEA9EB4] == 0) of mesh box +0x14; centre box +4, radius obj +0x44 x box +0x1C, origin matrix +0x38; side effects g_b_last_on_screen, g_last_selected_box, g_last_distance, vt+0xA0 and the fn_007ACC60 path not ported | `RegionOnScreen` |
| `GLandscape::Draw 0x5E4C7D..0x5E4D4B, [0xEA9EC0]` | swim ring: clock += g_game_time_inc, past 1000 ms (jle) reset to 0; ring +0x18 growth 1, +0x1C kept, +0x20 angle, +0x24 1, +0x28 aspect 1, +0x2C rate 1, +0x30 cell 0x30, +0x34 0xFFFFFFFF; body posed later at fn_008254A0 0x5E6255 | `SwimRing` |
| `0x708E02..0x708E1A` | SET_HIGH_GRAPHICS_DETAIL off: vt+0x70(0) (normal draw back), DrawListRebuildCount = 1, fn_00825440 | `SetHighGraphicsDetail` |
| `0x708D2D, LH3DObject::AddDrawing 0x815A7C` | SET_HIGH_GRAPHICS_DETAIL on: vt+0x70(1) sets obj+8 | `= 0x10, which AddDrawing skips` |
| `0x708D40..0x708DC5` | the high slot compared against MeshPack[501], [498], [439], [420] | `SetHighGraphicsDetail` |
| `0xC20574 / 0xC20554 / 0xC20534 / 0xC20520` | the HD mesh path strings nors_man / nors_woman / nors_boy / sable | `SetHighGraphicsDetail` |
| `0x70905B..0x709085, jump table 0x709130` | THING_JC_SPECIAL: the thing must be a SuperVillager except for feature 19; jump on feature - 7 (0..12) | `ThingJcSpecial` |
| `0x70909E / 0x7090AE / 0x7090C2 / 0x7090D0 / 0x7090E2 / 0x70910E` | THING_JC_SPECIAL features 7 (follow grip), 8 (snap), 9 (fchs yaw mirror), 16 (+pi/2 [0x8C78D8]), 17 (-pi/2), 18 (Intro::ReleaseAll 0x5DFC40) | `ThingJcSpecial` |
| `0x5E4B3A..0x5E4B93, HelpSystem +0x45E8 / +0x45EC` | every SuperVillager released each frame without a script's wide screen | `Update` |
| `0x5E4BA5..0x5E4BB7 / 0x5E4E3C, IsAvailable vt+0x2C` | a SuperVillager whose thing is no longer available is released | `Update` |
| `fn_008254A0 0x8254DD..0x825504, [0xEB9A0C], [0xEA9F30]` | dead SetPosition to (0, 0, 0) under [0xEB9A0C] != 0, a bss flag with one reference (the read 0x8254E3) | `FollowHand` |
| `fn_00825530 0x825E1F..0x825E28` | the eyes updated after the body | `Draw` |

## Pending

- The advisors' voice-tag gestures (WAV cue labels), the pupils, the eye-bone scales, the anim sounds.
- The camera feature mask's effects, the angle / pitch sound feeds (the player's camera is not CameraModeNew3).
- The second cinematic's villager opcodes, the citadel plan.
- HelpSystem::Process 0x5C8FE0 as one function (today its parts sit at its turn step: tooltips, icons).
- `GatheringText::GetFrac` (Bubble.cpp): "0x831550..0x8315A0. (pending) the a == b branch at 0x831589 is not read: b == a
  gives 1 here".
- `HelpDudeFile` `value35C0`: "+0x35C0 (0x5C2374) (pending), 0 in both".
- `HelpProfile` `processBlocked`: "[0xC4CCEE] (meaning pending: GGame clears it at 0x54B1C5 before GoInsideCitadel; the
  hand is not drawn while it is set)".
- `HelpTextDisplay::DrawEntryText`: "(pending) the missionaries song karaoke (0x5CBA3C..0x5CBA62, 0x5CBB91..0x5CBDFB):
  music type GGlobal+0x28 in 0x33..0x35, fn_00426C40, [0xD17CB0] (newest) and the word index == GScript+0x9C get a
  highlight box fn_00447BA0 driven by the Zoomers 0xD17BE8/0xD17C18/0xD17C48; not drawn."
- `AdvisorSpirit::AdvisorSpirit` (was `HelpDude::HelpDude`): "(pending) the record fn_005C2090 builds before the first UpdateFace".
- `AdvisorSpirit::UpdateMotion` (was `HelpDude::Update1`): "6. the zones; fn_005BDD40's four records +0x3404 (pending: nothing reads them here)".
- `RunMessageSet`: "2 the thing's GetQueryFirstEnumText / GetQueryLastEnumText (vt +0x4D8 / +0x4DC), 0 and 0 without a
  thing (always here: (pending) the thing variant 0x5C8D90)".
- ScriptTimer: "(not ported) GameThing::SetScriptNameOfCreate 0x56FA70" (fn_007115A0's call on CREATE).
- ScriptHighlight `hidden` +0x5C: "(pending) no writer found besides fn_00709910 (= 0) and Load".
- ScriptHighlight::Create: "(pending, renderer) 0x709AB7 fn_0080B440(0) on the 3D object; 0x709ACE vt +0x98(0) on it for
  a scroll; the sprite +0x64 (0x709ADE) and the glow object +0x68 (0x709AF7: mesh k_GlowMesh, SetMaterialProperties {1,
  0, 0, 1, 1} 0x709B4B) are ExtrasOf's; 0x709B5A vt +0x80 on the 3D object (UseFootpathIfNecessary in its vtable, not
  read)".
- ScriptHighlight::SetActivated: "(pending, audio) the options' mapping to audio::PlaySoundEffect(PlayOptions) is not
  checked" (sample 0x86).
- ScriptHighlight::Create: "0x709BC7: unless +0xA & 1 (inferred clear for a new object)".
- did_you_know_read::MarkRead (was dyk_read): "fn_0078CD20 (not read in detail: inferred an append when the list has room)"; and "(pending) where
  the original saves them (the help profile)".
- Countdown display: "(inferred) the arguments are r, g, b" of CreatureMentalEditor::DrawTextA 0x4DF310.
- Eyes ctor: "LH3DObject::Create(0), vt+0x98(0) and vt+0x58(0) (obj+4 bits 8 and 0x20 off; meaning pending)".
- Intro hand: "LH3DObject::Create(2) (an animated object) in [0xD19C98] with vt+0x58(1) (obj+4 bit 0x20 when [0xC38224]:
  pending)".
- Intro hand grip: "(pending) a bone outside the pose (the original reads past the buffer) gives nothing".
- SkipBox: "(pending) SetupBigButton::HitTest 0x40D310: the button's own extent (DrawBigButton's size 32)"; "(pending) the
  button's hit test 0x40D310 and SetupCheckBox::HitTest 0x410F90 / Click 0x411020; here the check box squares and a
  (inferred) 32 x 32 button square".
- SkipBox: "(pending) SetupBox's own draw (vt 0x8D85F0): the background DrawBg 0x413960 over k_Box and the controls in
  their list order; the controls' draws: SetupControl's text (DrawText 0x4119B0 with +0x23C as the justification),
  SetupBigButton::Draw 0x40CEB0, SetupCheckBox::Draw 0x410B80 (DrawBigButton(x, y, +0x23C != 0, hovered, w, +0x248 != 0,
  1, -0xA000, 0xA000))"; "(approximate) the vertical place as BigButton's labels: the centre less half the size (the
  exe's - 5 / the text height, 0x40D153, not re-read)"; "(pending) the arrow's look 2 (+0x248): DrawBigButton 0x412150";
  k_Box "(inferred: to confirm in SetupBox's draw) the box spans (200, 155)..(600, 445)".
- Controls / SkipBox "look 2" is read as the left-arrow look, from GameMenu's call sites. (inferred), not from a
  documented table.
- `InterfaceActive`: "(pending) other effects" is read from the offsets as a help-system flag copied from the interface
  plus an interface state bit cleared. (unknown) beyond that.
- `HandDemo` `Record::trigger`: "the trigger key state" is (inferred) from the space-key note.
