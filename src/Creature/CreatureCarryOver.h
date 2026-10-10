/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <string_view>

#include <glm/vec2.hpp>

#include "3D/MapCoords.h"

/// The player's creature carried on to the next land: kept with its file as a land is cleared, and loaded from it where
/// the next land's script says, sparkling into sight
namespace openblack::creature_carry_over
{

/// A loaded creature sparkles into sight over this many seconds, from out of sight
constexpr float k_ArrivalSeconds = 3.0f;

/// How far a creature has sparkled out of sight, 0 to 1, and where it is going: a share of the way a second
struct Fizz
{
	float now {0.0f};
	float target {0.0f};
	float perSecond {0.0f};

	bool operator==(const Fizz&) const = default;
};

/// The fizz set going to a target over some seconds; over no seconds, or when already there, it is there at once
[[nodiscard]] Fizz SetFizz(const Fizz& fizz, float target, float seconds);
/// One game turn of so many milliseconds of the fizz going to its target, stopping there
[[nodiscard]] Fizz StepFizz(const Fizz& fizz, float turnMilliseconds);
/// A loaded creature's fizz: out of sight at once, then coming back into sight over three seconds
[[nodiscard]] Fizz ArrivalFizz();

/// Where on the map a script's place puts a loaded creature: only the place's cell is taken, and the creature stands on
/// the ground in the middle of it
[[nodiscard]] map_coords::MapCoords ArrivalCoords(glm::vec2 place);

/// The player's creature is kept in two files named after the player's profile file: its mind under the profile's name,
/// and its physique under the name with "Physique" before it
struct KeptFiles
{
	std::filesystem::path mind;
	std::filesystem::path physique;
};
[[nodiscard]] KeptFiles KeptFilesIn(const std::filesystem::path& folder, std::string_view profileFile);

} // namespace openblack::creature_carry_over
