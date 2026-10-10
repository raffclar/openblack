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
#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/Animal.h"

namespace openblack::ecs::components
{

/// A flock: livings that keep together about a place, whether a land's birds, a miracle's animals or the villagers a
/// script gathers. Its members keep up with it in their own states; the flock has no turn of its own.
struct Flock
{
	/// A new flock keeps its members within 80 metres of its place, and its followers within 30 metres of its leader
	static constexpr uint16_t k_DefaultDomainRadius = 80;
	static constexpr uint16_t k_DefaultFlockDistance = 30;

	/// Its own place, which its leader wanders about and the scripts and miracles move it by
	map_coords::MapCoords place;
	/// Its members in line, the first to the last; the last is its leader, and a newcomer of no particular place in the
	/// line goes first
	std::vector<entt::entity> members;
	/// How far from its place its leader may wander, and its followers from its leader, in whole metres
	uint16_t domainRadius {k_DefaultDomainRadius};
	uint16_t flockDistance {k_DefaultFlockDistance};
	/// How calm it is, as a script set it; nothing reads it
	int32_t calm {0};

	/// For a flock of animals: the state the followers take once the leader has set off, how (3 for in formation), and
	/// the state a special move ends in
	AnimalState followState {AnimalState::DecideWhatToDo};
	int followMode {0};
	AnimalState afterMove {AnimalState::DecideWhatToDo};
	/// The turns its leader has kept to its present leg, which a land bird's leader gives up after its kind's stay time
	uint32_t turnsOnLeg {0};
	/// The height above the land its leader picks its legs about, none to take its kind's: a temple's flock follows its
	/// temple's height
	float height {0.0f};
	/// The number the land script made it with, none for a flock the script didn't number; the script's animals join
	/// the latest flock made with their number
	std::optional<int32_t> scriptId;
	/// The order the animals' flocks were made in
	uint32_t made {0};
	/// The temple it circles, none for a flock of the land
	entt::entity temple {entt::null};
};

/// A villager's (or other living's not an animal) part in a flock: the flock it is in, if any, and its place in a
/// flock's line, which it keeps after leaving. An animal keeps its flock in its own data, with no place in the line.
struct FlockMember
{
	entt::entity flock {entt::null};
	uint8_t order {0};
};

} // namespace openblack::ecs::components
