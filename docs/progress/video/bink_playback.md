# Bink video playback

How Black & White plays its five Bink videos, and how openblack can play them. The research is done: Diego's
`docs/bw1-notes/video.md`, checked bit for bit against the game's own Bink 1.0w player. His newest branch,
`diegoscood/AI-Decompiled` (fetched 2026-10-07, one commit rebuilding his fork on upstream openblack), has a complete
player in `src/Video/` with **his own Bink 1 decoder, identical to the game's on every one of the 5,353 frames of the
five videos**; FFmpeg is gone from it. The earlier `bw-clean` branch used a trimmed FFmpeg instead. None of it is in
openblack yet: `SET_AVI_SEQUENCE` is an empty stub (`src/CHLApi.cpp`) and there is no decoder, player, start-up sequence
or loading screen. What each video is for is in [bink_videos.md](bink_videos.md).

**Progress: 0/54 done, 0 partial — 0%**

## How to play them in openblack (the plan)

1. **The container** — a component, as `components/<name>` like the other file formats (the parsers-as-components
   rule): reads the 44-byte header, the frame index (keyframe bit, offsets) and one packet per frame. Diego's
   `BikFile` (checked on all five files) is the starting point; it moves out of `src/` into its own component with tests.
2. **The decoder** — take Diego's own Bink 1 decoder from `AI-Decompiled` (`BinkDecoder` with its bit reader, Huffman
   trees, the nine per-plane bundles and the ten block types, `src/Video/Bink*`), behind a decoder interface with a
   black-frame fallback for tests. It matches the original on all 5,353 frames, in order and by seeking, with no
   dependency to build; its constant tables follow FFmpeg's decoder (LGPL-2.1-or-later, credited, used under GPL-3).
   What makes it exact: plane order Y, V, U; pixel stores wrap modulo 256 with no clamping; the inverse DCT wraps at
   32 bits and rounds rows with (x + 127) >> 8; a motion vector is only checked against the plane's block area; the
   first frame is its own reference. His build option `OPENBLACK_USE_BINK` (on by default) and the debug menu's "Game
   films" switch let films be turned off. The FFmpeg route (the `bw-clean` branch) is the fallback if the decoder is
   not taken.
3. **The colours** — the original converts YUV 4:2:0 to RGB with its own integer tables (chroma not interpolated, one
   value per 2×2 block, one constant differing from textbook BT.601). Do it in a bgfx shader that samples the three
   planes as R8 textures and reproduces those tables exactly (no CPU conversion, three small uploads a frame), with a
   test against golden frames taken from the original player. The 16-bit (555/565) copy the original makes does not
   need reproducing beyond what it does to the picture (it truncates each channel to 5 or 6 bits; decide whether to keep
   that look).
4. **The player** — a system (`VideoSystemInterface` in `Locator`, as Diego's `AI-Decompiled` now has it): opens a video, keeps time by the real clock (frame
   i at i ÷ fps), pauses the game and turns the cinema bars on while it covers the screen, fades, handles ESC, and tells
   the audio and the renderer what it is doing. Pure rules (fade alpha, schedule, letterbox rectangle, frames due) as
   free functions with unit tests.
5. **The renderer** — one screen-space quad in the overlay pass, between the cinema bars and the script fade, with the
   world not drawn while the video is opaque.
6. **Wiring** — `SET_AVI_SEQUENCE` (1 = intro, 2 = falling spell), the start-up logos, the first-run pre-intro, and the
   loading screen's tips.

## The files

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| All five are Bink 1 (revision i) with no audio track; every sound comes from the game's own audio | todo | logo 768×512 15 fps 2 frames; pre_intro 768×512 25 fps 2515 frames (100.6 s); INTRO 640×360 24 fps 1601 frames (66.7 s); tips 768×512 15 fps 35 frames; fall 640×360 24 fps 1200 frames (50 s) |
| INTRO, pre_intro and fall have a single keyframe (frame 0), so they only play from the start; logo and tips are all keyframes, so any frame can be shown | todo | |
| Videos are looked for on the CD first when the game runs from the CD | n/a | openblack reads the install folder |

## Container and decoding

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Read the header, frame index and per-frame packets, rejecting a malformed file | todo | Diego's `BikFile` + `test_bik_file` |
| Decode Bink 1 revision i: planes Y, V, U on 32-bit boundaries, nine Huffman-coded bundles per plane read before each block row, ten block types (skip, scaled, motion, run, residue, intra DCT, fill, inter DCT, pattern, raw) | todo | Diego's `BinkDecoder` (`AI-Decompiled`), `test_bink_bitstream`, `test_bink_blocks` |
| Integer behaviour exactly as the original: stores wrap modulo 256, the inverse DCT wraps at 32 bits and rounds (x + 127) >> 8, motion vectors checked only against the block area, the first frame is its own reference | todo | `test_bink_decoder` against CRCs of the original's frames (`BinkGolden.h`) |
| Films can be switched off (build option and a debug switch), skipping them as if absent | todo | `OPENBLACK_USE_BINK`, the Video debug menu (`AI-Decompiled`) |
| A debug window plays any of the five films with play, pause, step and restart | todo | `src/Debug/VideoViewer` (`AI-Decompiled`) |
| The integer frame rate is the header's fraction rounded down (a failed open counts as 1 fps, 0 frames) | todo | |
| Decode frames in order; showing an arbitrary frame decodes from the nearest keyframe before it | todo | Diego's `FfmpegDecoder` |
| A frame that fails to decode keeps the previous picture | todo | |
| Frames are never skipped when running late: each due frame is decoded | todo | the original opens videos with "no skip" |
| YUV to RGB with the original player's exact integer tables (no chroma interpolation) | todo | Diego's `BinkYuv.h`, bit-exact on 55 golden frames; one doubtful value at V = 219 |
| The picture goes through a 16-bit (555, or 565 on cards without 555) surface before it is drawn | todo | decide whether to keep the banding |
| Builds without the decoder still run every video's timing, pause and skip, showing black | todo | |

## The full-screen video (intro and falling spell)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Starting a video clears any tip picture, replaces a video already playing, and forgets the music that is playing | todo | |
| The game pauses while the video plays (remembering whether it was already paused) | todo | |
| The cinema bars come on fully in the first frame and go away over 2 s of game time when the video ends | todo | |
| A player exists even if the file fails to open; it then ends on the next frame | todo | |
| By default the video plays to its last frame and fades out over its last 5 seconds | todo | |
| The intro (sequence 1) fades from 58 s to 60 s and ends at 60 s: its last 160 frames are never seen | todo | |
| Frames are paced by the real clock, at most one decoded per 16 ms tick | todo | openblack: decode every frame due by the real clock each frame |
| While opaque, the 3D world is not drawn; script fades and the cinema bars and text still are | todo | |
| During the fade the game runs again (the earlier pause state returns) and the world is drawn under the video | todo | |
| Fade alpha falls linearly from 1 to 0 across the fade frames | todo | pure function, unit test |
| The picture fills the screen between the 16:9 bars, scaled to fit; on screens wider than 16:9 the bars go negative and the picture overflows top and bottom | todo | keep, or clamp as a deliberate difference |
| Drawn as a screen quad, half a texel in from each edge, no wrapping, no depth test or write, alpha blended | todo | |
| While the video is opaque and no new frame is ready, the original waits up to 0.5 s for one | todo | openblack should keep the last frame instead of blocking |
| When it ends: the game's pause state and the bars are restored on the next frame | todo | |
| A video that replaces another keeps the second one's pause and bars state (the game stays paused afterwards) | todo | |

## Skipping

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| ESC skips: if already fading, the video ends at once; otherwise it fades out over 48 frames (2 s at 24 fps) and the game resumes | todo | |
| ESC with Shift or Ctrl held does nothing | todo | |
| Skipping releases the script's music | todo | |
| The first video of a new profile can't be skipped | todo | needs profiles (`../interface/profiles.md`) |
| Skipping the falling spell ends the spell, which then fades its video normally | todo | |
| One skip per key press (key repeats ignored) | todo | |

## The falling spell (fall.bik)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Plays only when the player has a creature; script sequence 2 starts it and turning it off ends it | todo | |
| The video has no fade of its own; it fades when the spell ends | todo | |
| Drawn at about 31% strength over the scene, which hides the land | todo | |
| The player's creature falls in front of the video with its glows, sparks and a light burst, the camera following its path with a 45° field of view | todo | the sparks, light burst and path are ported in the miracles work; the falling creature and camera are not |
| Sounds timed to the video: rumble at 17.45 s, laser explosion at 19.45 s, chant at 31.65 s, citadel explosion and volcano at 13.45 s, heal chakra at 37.75 s | todo | |
| At 43.9 s the screen fades to white over 1 s, all music fades out, then the white fades back | todo | |

## Start-up and loading screen

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The logo video's two still pictures show at every start-up, with a fade, before the audio starts | todo | needs a start-up sequence |
| The pre-intro plays when there is no profile yet, in its own loop with no bars, the trailer music playing, cursor hidden | todo | |
| The pre-intro stops at its end or on a key press, and the music is cut | todo | |
| The loading screen shows one of 35 tip pictures from tips.bik | todo | needs a loading screen |

## Audio during videos

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Logo: silent | todo | |
| Pre-intro: the trailer music at full volume, cut off sharply when it ends | todo | |
| Intro: ambience stops while paused; the music already playing carries on | todo | |
| Falling spell: as the intro, plus its timed sounds and the music fade at 43.9 s | todo | |
| The audio knows a video is playing (from any thread) | todo | |

## Tests to carry

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Container parsing on synthetic and the game's files | todo | Diego's `test_bik_file` |
| Decoding and colours against golden frames | todo | Diego's `test_bink_decoder` and `test_bink_yuv` (`AI-Decompiled`); the full 5,353-frame check runs outside the repo against the game's own player |
| Player timing, fade, schedule, skip and pause rules | todo | Diego's `test_video_player` |
| Falling spell timeline | todo | Diego's `test_falling_spell_video` |
