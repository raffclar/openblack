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

#include <optional>

#include <glm/vec3.hpp>

#include "ECS/Systems/WeatherSystemInterface.h"
#include "Enums.h"
#include "ScriptHeaders/ScriptEnums.h"

namespace openblack
{
struct GWeatherInfo;
}

/// How the scripts' CREATE natives make things: which types they make, where and how things are placed, and what a
/// scripted weather thing brings
namespace openblack::script::create_rules
{

/// The scripts can create things of these types, from a marker to an animated static
inline constexpr int32_t k_FirstCreateType = static_cast<int32_t>(ObjectType::Marker);
inline constexpr int32_t k_LastCreateType = static_cast<int32_t>(ObjectType::AnimatedStatic);

/// Whether a script may ask to create a thing of the type; any other is refused with an error
[[nodiscard]] bool IsCreatableType(int32_t type);

/// Whether a thing a script creates keeps the height above the land the script gave its place. Only a marker and a mobile
/// static do; everything else is made standing on the land under that place, whatever height the script gave.
[[nodiscard]] bool KeepsScriptHeight(ObjectType type);

/// CREATE_WITH_ANGLE_AND_SCALE gives its angle in degrees; things are turned in radians
[[nodiscard]] float AngleFromDegrees(float degrees);

/// A tree's facing is brought to within one turn: whole turns are taken off towards zero
[[nodiscard]] float TreeAngle(float radians);

/// Whether a subtype numbers a row of a table with this many rows
[[nodiscard]] bool IsRow(uint32_t subtype, std::size_t rows);

/// Each tribe has this many kinds of villager in the villagers' table, one after another: its housewife, forester,
/// fisherman, farmer, shepherd, leader and trader
inline constexpr uint32_t k_VillagersPerTribe = 7;

/// The kind of villager a script's random villager of a tribe is: the tribe's kind numbered by a draw below the kinds
/// per tribe. None for a tribe with no kinds in a table of this many rows.
[[nodiscard]] std::optional<uint32_t> TribeVillagerRow(int32_t tribe, uint32_t draw, std::size_t rows);

/// A villager a script makes with CREATE: how old it is made, and whether it joins a town
struct ScriptVillager
{
	uint32_t age;
	bool joinsTown;
};

/// A child is made ten years old, a grown-up a year past the age its kind grows up. Either joins no town: the script
/// puts it in one itself, or it finds its own way once the script lets it go.
[[nodiscard]] ScriptVillager ScriptVillagerOf(bool child, uint32_t grownUpAge);

/// The miracle a script names by its number, if the game has one by that number (none is 0)
[[nodiscard]] std::optional<MagicType> MagicTypeFromScript(int32_t number);

/// The turns between a dispenser's bubbles that SET_MAGIC_PROPERTIES gives it: the seconds it is given as turns, or,
/// for none or fewer, its building's own period
[[nodiscard]] uint32_t DispenserTurns(float seconds, float buildingPeriodTurns);

/// The storm a scripted weather thing brings to where it is made: a small storm of fixed size and life, its weather of
/// the kind it is made as
[[nodiscard]] ecs::systems::ScriptStorm WeatherThingStorm(const GWeatherInfo& info, const glm::vec3& centre);

} // namespace openblack::script::create_rules
