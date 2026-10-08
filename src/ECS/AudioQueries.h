/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

// The ECS side of the audio: src/Audio includes no ECS component, so what the audio engine and its callers read of
// the ECS things (the animated villagers and animals, the street lanterns, the map's surface, the weather) is answered
// here, through audio::GameQueries (src/Audio/GameQueries.h), which Game.cpp gives to audio::Init.
// docs/bw1-notes/audio.md, "openblack audio architecture".

namespace openblack::audio
{
struct GameQueries;
}

namespace openblack::ecs::audio_queries
{

/// Fills the queries that read the ECS registry and its systems: surfaceType (ecs::sea_cells::GetSurfaceType),
/// weatherSmooth (weather::atmos::GetWeatherSmooth), cameraAlignment (the audio's alignment value, from
/// ecs::effects::alignment::GetInterfaceAlignment), animatedThing (what the animation sounds read of a villager or
/// animal), animationClipName (the clips of the resources) and streetLanterns (the lanterns with their heights)
void Fill(audio::GameQueries& queries);

/// The audio's alignment value from x ((alignment + 1) / 2): x clamped to 0..1 (a NaN is 0), then 2 - 2 (1 - x) - 1
/// in float steps
[[nodiscard]] float AudioAlignmentValue(float x);

/// (openblack test hooks) once a game turn, after audio::ProcessTurn:
///  - OPENBLACK_AUDIO_TEST_VIEW="turn,n[,distance]" flies the camera to look at the n-th villager from that distance (4)
///    at that game turn, and OPENBLACK_AUDIO_TEST_ANIM="clip" plays that clip in a loop on every villager from the same
///    turn (after the Land 1 intro has given the camera back);
///  - OPENBLACK_AUDIO_TEST_LANTERN="turn[,distance]": at that turn (counted by these calls since the start) the camera
///    flies to look at the first lantern's top from that distance (3: inside the sample's 5)
///  - OPENBLACK_AUDIO_TEST_CITADEL (RunCitadelTestHook, below)
void RunTestHooks(uint32_t turn);

/// (openblack test hook) OPENBLACK_AUDIO_TEST_CITADEL="<in>[,<out>]": at those hook turns the temple interior is entered
/// / left, as ENTER_EXIT_CITADEL(1) / (0) would (the citadel's music and filters). The hook turns are counted by the
/// world's turns (RunTestHooks) and, while the world is paused inside the temple, by the temple's own turns (Game), so
/// the way out comes as many 100 ms turns after the way in as before
void RunCitadelTestHook();

} // namespace openblack::ecs::audio_queries
