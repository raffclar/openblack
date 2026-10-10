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
#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"

namespace openblack::dance
{
struct DanceFile;
}

namespace openblack::ecs
{
class Registry;
}

/// The dances: made about a place, joined and left by dancers, gone when what they are danced about goes
namespace openblack::ecs::dances
{

/// What a new dance is
struct DanceSetup
{
	/// Its row in the dances' table, and whether it starts by itself once it has dancers
	uint32_t type {0};
	bool autostart {false};
	map_coords::MapCoords place;
	/// What it is danced about, if anything
	entt::entity centre {entt::null};
	/// The turns it dances before stopping to start again; none for no end
	uint32_t durationTurns {0};
	bool madeByScript {false};
};

/// A new dance with the groups of its file, each set up by the file's key frames up to its first beat
entt::entity Create(Registry& registry, const DanceSetup& setup, std::shared_ptr<const dance::DanceFile> file);
/// A dance goes: its dancers leave it and each is told it has finished dancing
void Destroy(Registry& registry, entt::entity dance, const std::function<void(entt::entity)>& finished);

[[nodiscard]] bool IsDance(const Registry& registry, entt::entity thing);
/// The dance a living dances in, or none
[[nodiscard]] entt::entity DanceOf(const Registry& registry, entt::entity living);
[[nodiscard]] uint32_t Size(const Registry& registry, entt::entity dance);

/// A living joins a dance, in the group whose turn it is that takes it; false when none does, and then it dances in
/// no group
bool AddDancer(Registry& registry, entt::entity dance, entt::entity living, uint32_t sex);
/// A dancer leaves its dance
void RemoveDancer(Registry& registry, entt::entity living);
/// The first dancer of the first group with one other than `exclude`
[[nodiscard]] entt::entity FirstDancer(const Registry& registry, entt::entity dance, entt::entity exclude);
/// A dance's dancers, group by group
[[nodiscard]] std::vector<entt::entity> Dancers(const Registry& registry, entt::entity dance);

/// What a dance's turn needs to know of the game
struct TurnContext
{
	uint32_t turn {0};
	uint32_t turnsPerSecond {10};
	/// Whether the thing danced about is still about
	std::function<bool(entt::entity)> available;
	/// A dancer told it has finished dancing
	std::function<void(entt::entity)> finished;
};

/// A dance's turn: it ends once what it is danced about has gone; it starts once it has dancers, stops when its time is
/// up to start again on its next turn, and while dancing does its key frames and goes on a beat
void ProcessTurn(Registry& registry, entt::entity dance, const TurnContext& context);

} // namespace openblack::ecs::dances
