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

/// Whether a thing a script creates keeps the height above the land that the script gave its place. `subtype` is the
/// row of the thing in the info table of its type.
/// - A marker keeps the height.
/// - A mobile static that can not move in the physics keeps the height: a holder of a singing stone, a lantern and a
///   bonfire. Every other mobile static goes down onto the land when the script makes it.
/// - A mobile static of the rock kind keeps the height. The physics then lets it fall onto the land.
/// - Everything else stands on the land under that place, whatever height the script gave.
/// `rockKind` tells if the info table puts the row in the rock kind.
[[nodiscard]] bool KeepsScriptHeight(ObjectType type, uint32_t subtype = 0, bool rockKind = false);

/// How a rock that a script makes starts in the physics. The rock starts at rest with no thrower and no player.
struct ScriptRockStart
{
	/// The physics puts the body on the land and turns it to the slope before it lifts the body clear of things.
	/// Only a rock that the script asks for by the rock type does this.
	bool settlesOnLand;
};

/// Whether a thing that a script makes is a rock that starts in the physics, and how it starts. A mobile static or a
/// rock does this when the info table puts its row in the rock kind. The holder of a singing stone, the lanterns and
/// the bonfire never do, because the physics can not move them.
[[nodiscard]] std::optional<ScriptRockStart> RockStartOf(ObjectType type, uint32_t subtype, bool rockKind);

/// Whether a thing a script creates takes no notice of reactions until a script tells it to. Only a creature does: it
/// does not react to what happens near it, and it does not catch what flies to it.
[[nodiscard]] bool IgnoresReactions(ObjectType type);

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
