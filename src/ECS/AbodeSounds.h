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

#include <chrono>
#include <string_view>

#include "3D/DayNightClock.h"
#include "Audio/AnimEffectKeys.h"
#include "Enums.h"

/// The rules of the sounds that the buildings of a town make by themselves. Pure, for the abode sound system and its
/// tests.
///
/// The game draws a building, then it rolls a chance for the sound of the building. The chance gives about one sound
/// in k_SoundTime of game time. The building sounds only when the camera is nearer than k_Reach. The sound changes with
/// the time of day: day, dusk, dawn or night.
namespace openblack::ecs::abode_sounds
{

/// The bank that holds the sounds
inline constexpr std::string_view k_Bank = "editor.sad";

/// A building sounds only when the camera is nearer to it than this
inline constexpr float k_Reach = 200.0f;
/// On average, a building near the camera sounds one time in this much game time
inline constexpr std::chrono::duration<float, std::milli> k_SoundTime {80000.0f};

/// The darkness of the sky goes from 0 by day to 2 by night. Below k_DayUntil, the buildings make the day sound. At
/// k_NightFrom and above, they make the night sound. Between the two, they make the dawn sound before noon and the dusk
/// sound after noon.
inline constexpr float k_DayUntil = 0.7f;
inline constexpr float k_NightFrom = 1.3f;
inline constexpr float k_Noon = 12.0f;

/// Whether a building of this type makes the sounds. The fields, the football pitch, the spell dispenser and the totem
/// do not make them.
[[nodiscard]] constexpr bool MakesSounds(AbodeType type) noexcept
{
	return type != AbodeType::Field && type != AbodeType::FootballPitch && type != AbodeType::SpellDispenser &&
	       type != AbodeType::Totem;
}

/// The darkness of the sky at an hour of visual time, from 0 by day to 2 by night. The sky turns at the hours of
/// `times`, and the hours after noon count back from midnight.
[[nodiscard]] constexpr float Darkness(float visualHour, const DayNightTimes& times) noexcept
{
	const float hour = visualHour > k_Noon ? 24.0f - visualHour : visualHour;
	if (hour < times.nightFull)
	{
		return 2.0f;
	}
	if (hour < times.duskStart)
	{
		return 2.0f - ((hour - times.nightFull) / (times.duskStart - times.nightFull));
	}
	if (hour < times.duskEnd)
	{
		return 1.0f;
	}
	if (hour < times.dayFull)
	{
		return 1.0f - ((hour - times.duskEnd) / (times.dayFull - times.duskEnd));
	}
	return 0.0f;
}

/// The sound that a building makes at an hour of visual time, with the sky at a darkness
[[nodiscard]] constexpr audio::SoundAction ActionAt(float darkness, float visualHour) noexcept
{
	if (darkness < k_DayUntil)
	{
		return audio::SoundAction::AbodeDay;
	}
	if (darkness >= k_NightFrom)
	{
		return audio::SoundAction::AbodeNight;
	}
	return visualHour < k_Noon ? audio::SoundAction::AbodeDawn : audio::SoundAction::AbodeDusk;
}

/// The keys of the sound that a building makes
[[nodiscard]] constexpr audio::AnimEffectKeys KeysAt(float darkness, float visualHour) noexcept
{
	return {.object = audio::SoundObject::Abode, .action = ActionAt(darkness, visualHour)};
}

/// A drawn building sounds on a frame when a roll from 0 to this number, less one, gives 1. Thus it sounds about one
/// time in k_SoundTime. The result is 0 when the game time does not move, for example when the game is paused.
[[nodiscard]] uint32_t Chance(std::chrono::duration<float, std::milli> gameTime) noexcept;

/// Whether a roll from 0 to `chance`, less one, lets the building sound
[[nodiscard]] constexpr bool RollSounds(uint32_t roll) noexcept
{
	return roll == 1;
}

/// Whether the camera is near enough to a building to hear it
[[nodiscard]] constexpr bool InReach(float distance) noexcept
{
	return distance < k_Reach;
}

} // namespace openblack::ecs::abode_sounds
