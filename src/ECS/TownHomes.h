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

#include <functional>
#include <optional>

#include <entt/entity/entity.hpp>

/// Where a town's people live, as the game has it: a villager joining a town moves into the building that suits it best,
/// or is one of the town's homeless; a building's people are made homeless when it goes. A building's people and a town's
/// homeless are both kept newest first.
namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::town_homes
{

/// How many a building can house, and whether it works
struct AbodeRoom
{
	uint32_t maxAdults {0};
	uint32_t maxChildren {0};
	bool functional {false};
};

/// What the rules ask of the game's buildings and people
struct Homes
{
	/// The room of a building, none for what isn't one
	std::function<std::optional<AbodeRoom>(entt::entity abode)> room;
	/// A villager is about to leave its home: one inside it comes out and decides again what to do
	std::function<void(entt::entity villager)> leaving;
};

/// What a building is like for one more villager
struct Occupancy
{
	/// Whether the newcomer is a child
	bool child {false};
	uint32_t adults {0};
	uint32_t children {0};
	uint32_t maxAdults {0};
	uint32_t maxChildren {0};
	/// Its people, and those of them of the newcomer's sex
	uint32_t people {0};
	uint32_t sameSex {0};
	/// From the building to the newcomer, in metres
	float distance {0.0f};
};

/// How well a building suits one more villager: by how much room it has left for the villager's age, then a building
/// mostly of the other sex and a near one are liked better. Nothing for a building that can't house its age at all.
[[nodiscard]] float ScoreForAddingVillager(const Occupancy& occupancy);

/// The functional building of a town that suits a villager best, scoring above a least score: the first of the best,
/// in the town's order
[[nodiscard]] entt::entity FindAbodeWithSpace(const Registry& registry, const Homes& homes, entt::entity town,
                                              entt::entity villager, float leastScore);

/// A villager joins a town: one without a home there moves into the building that suits it best, or is one of the
/// homeless. False for a town nobody can live in.
bool AddVillagerToTown(Registry& registry, const Homes& homes, entt::entity town, entt::entity villager);

/// A villager moves into a building, leaving the homeless or its old home, and joins the building's town
void AddVillagerToAbode(Registry& registry, const Homes& homes, entt::entity abode, entt::entity villager);

/// A villager leaves its home and keeps its town, joining the town's homeless at the head. False with no town, or when
/// it is already homeless there.
bool MakeHomeless(Registry& registry, const Homes& homes, entt::entity villager);

/// The town a building belongs to, none if it has gone
[[nodiscard]] entt::entity TownOfAbode(const Registry& registry, entt::entity abode);

} // namespace openblack::ecs::town_homes
