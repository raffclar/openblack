/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <functional>
#include <optional>
#include <span>

#include "3D/MapCoords.h"

/// The rules of the flocks the scripts gather villagers into: who leads, where a newcomer goes in the line, and where a
/// member walks to keep up with its flock
namespace openblack::ecs::script_flock_rules
{

/// A new flock keeps its members within 80 metres of its place, and its followers within 30 metres of its leader, until
/// a script says otherwise
inline constexpr uint16_t k_DefaultDomainRadius = 80;
inline constexpr uint16_t k_DefaultFlockDistance = 30;
/// The place in the line of the first leader of an empty flock
inline constexpr uint8_t k_FirstLeaderOrder = 5;

/// Where a newcomer goes in a flock's line, given the places of its members from the first to the last (the leader):
/// before the first whose place is not below its own, else at the end
[[nodiscard]] std::size_t InsertAt(std::span<const uint8_t> orders, uint8_t order);
/// The place a new leader takes: one past the last member's, or the first leader's in an empty flock
[[nodiscard]] uint8_t LeaderOrder(std::optional<uint8_t> lastOrder);

using FloatRandom = std::function<float(float)>;
/// Whether a living can stand at a point: on the map and on nothing it can't walk on
using Accepts = std::function<bool(const map_coords::MapCoords&)>;

/// A random point between `least` and `most` metres from a centre, at a random angle: the angle is drawn first, then
/// the distance. A point the living can't stand on is moved round a spiral of cells, up to 25 cells; after two draws
/// that find nothing, the centre itself.
[[nodiscard]] map_coords::MapCoords CalcRandomPos(const map_coords::MapCoords& centre, float least, float most,
                                                  const FloatRandom& random, const Accepts& accepts);

/// Whether a point is within a flock's domain: no further from the flock's place than its domain radius
[[nodiscard]] bool WithinDomain(const map_coords::MapCoords& point, const map_coords::MapCoords& place, uint16_t radius);

/// What a member of a flock sees of it
struct FlockView
{
	/// The flock's own place, which its leader wanders about
	map_coords::MapCoords place;
	uint16_t domainRadius {k_DefaultDomainRadius};
	uint16_t flockDistance {k_DefaultFlockDistance};
	/// Whether the member is the flock's leader, and where the leader is
	bool isLeader {false};
	map_coords::MapCoords leader;
};

/// A turn of a member keeping up with its flock
struct FlockStep
{
	/// What the state says to the villager's state machine: 1 nothing to do, 0 waiting, 0x23 walking off
	uint32_t result {1};
	/// Where it walks to, coming back to keep up with its flock on arrival
	std::optional<map_coords::MapCoords> goal;
};

/// A member within its flock's domain stays where it is. Outside it, the leader walks back to a random point within
/// the domain; another member more than the flock distance from the leader walks to a random point within that
/// distance of it, if that point is within the domain or the leader is outside it too, and otherwise waits a turn.
[[nodiscard]] FlockStep MoveInFlock(const map_coords::MapCoords& position, const FlockView& flock, const FloatRandom& random,
                                    const Accepts& accepts);

} // namespace openblack::ecs::script_flock_rules
