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

/// How a creature takes a fire: whether a fire matters to it, what the burning thing belongs to, and whether it goes to
/// put the fire out with the water miracle or runs from it
namespace openblack::creature_fire
{
/// A creature puts out a fire only when the usefulness of what burns, times its compassion, is more than this
constexpr float k_LeastCompassionateUse = 0.05f;
/// A creature heeds a fire only when it is at least this share of the burning thing's height
constexpr float k_LeastHeightShare = 0.2f;

/// How much a fire matters to a creature: the fire reaction's own priority when the creature is within the burning
/// thing's reach and its own, both across the land, and big enough beside the thing; nothing otherwise
[[nodiscard]] uint8_t Priority(float distance, float targetRadius, float selfRadius, float selfHeight, float targetHeight,
                               uint8_t firePriority);

/// What a creature does about a fire
enum class Response : uint8_t
{
	/// It goes near the burning thing and casts the water miracle at it
	PutOut,
	/// It runs from the burning thing
	RunAway,
};

/// What a creature does about a fire, by how useful it has learnt the thing the fire belongs to is for compassion
/// (none when the fire belongs to nothing), its compassion, and how often it has seen the water miracle against how
/// often it needs to have seen it to cast it
[[nodiscard]] Response Choose(std::optional<float> usefulness, float compassion, float seen, float needed);

/// What kind of thing burns
enum class Burning : uint8_t
{
	Building,
	Villager,
	Field,
	TotemStatue,
	Tree,
	Animal,
	Creature,
	TemplePart,
	/// Anything else: a pot, a rock, a mobile object, a feature, the temple itself and so on
	Other,
};

/// What a burning thing belongs to, which is what the creature weighs when deciding whether to put the fire out
enum class BelongsTo : uint8_t
{
	/// Its town
	Town,
	/// Its forest, or else the nearest forest with trees within k_NearestForestDistance
	Forest,
	/// Its flock
	Flock,
	/// The burning creature itself
	Itself,
	/// Its temple
	Temple,
	Nothing,
};
/// How near a forest must be, across the land, for a tree in no forest to count as belonging to it (strictly nearer)
constexpr float k_NearestForestDistance = 1000.0f;

[[nodiscard]] BelongsTo Owner(Burning burning);

} // namespace openblack::creature_fire
