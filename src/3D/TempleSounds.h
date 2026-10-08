/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <functional>

#include <glm/vec3.hpp>

#include "Audio/Device/Sound.h"
#include "Audio/Engine/SamplePlay.h"

/// The temple's sounds, as its doors, scrolls, camera and creature room ask for them. Every one of them is of the InGame
/// bank; only the creature room's have a place.
namespace openblack::temple_sounds
{

using Options = audio::sample_play::Options;

/// Plays one of the temple's sounds
using PlaySound = std::function<void(const Options&)>;

/// A sound without a place or an owner, once
[[nodiscard]] inline Options Unplaced(audio::SoundId sound, int mode)
{
	return {.sound = static_cast<entt::id_type>(sound), .is3D = false, .track = false, .loops = 0, .mode = mode};
}

/// The main room's doorway as it starts to swing open, and as it starts to close. Nothing while it still plays.
[[nodiscard]] inline Options DoorOpens()
{
	return Unplaced(audio::SoundId::G_CitadelDoorOpen_01, 2);
}
[[nodiscard]] inline Options DoorCloses()
{
	return Unplaced(audio::SoundId::G_CitadelDoorClose_02, 2);
}

/// A scroll as it starts to turn: one of its six squeaks, picked by the clock's milliseconds. Nothing while the same
/// squeak still plays.
[[nodiscard]] inline Options ScrollSqueak(uint32_t tickCount)
{
	constexpr std::array k_Squeaks {
	    audio::SoundId::G_ScrollSqueak_01, audio::SoundId::G_ScrollSqueak_02, audio::SoundId::G_ScrollSqueak_03,
	    audio::SoundId::G_ScrollSqueak_04, audio::SoundId::G_ScrollSqueak_05, audio::SoundId::G_ScrollSqueak_06,
	};
	return Unplaced(k_Squeaks.at(tickCount % k_Squeaks.size()), 2);
}

/// The camera as it starts to look at a scroll: one of the four wooshes, picked by the clock's milliseconds, as the
/// island's camera picks them. The same woosh starts again.
[[nodiscard]] inline Options Woosh(uint32_t tickCount)
{
	constexpr std::array k_Wooshes {
	    audio::SoundId::G_Woosh_01,
	    audio::SoundId::G_Woosh_02,
	    audio::SoundId::G_Woosh_03,
	    audio::SoundId::G_Woosh_04,
	};
	return Unplaced(k_Wooshes.at(tickCount & 3u), 3);
}

/// The creature room's fire, at the fire's place, and its water, at the foot of its waterfall: played every frame the
/// room is drawn, without an owner and staying where they are. Nothing while they still play.
[[nodiscard]] inline Options CreatureCaveFire(glm::vec3 place)
{
	return {.sound = static_cast<entt::id_type>(audio::SoundId::G_FireCreatureCave_01),
	        .is3D = true,
	        .track = false,
	        .position = place,
	        .mode = 2};
}
[[nodiscard]] inline Options CreatureCaveWater()
{
	constexpr glm::vec3 k_WaterfallFoot {160.0f, -45.0f, -30.0f};
	return {.sound = static_cast<entt::id_type>(audio::SoundId::G_WaterCreatureCave_01),
	        .is3D = true,
	        .track = false,
	        .position = k_WaterfallFoot,
	        .mode = 2};
}
/// The InGame bank's samples of the creature room's fire and water, which stop as the room stops being drawn
constexpr int k_CreatureCaveFireSample = 175;
constexpr int k_CreatureCaveWaterSample = 177;

} // namespace openblack::temple_sounds
