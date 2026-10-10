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
#include <unordered_map>
#include <unordered_set>

#include <entt/entity/fwd.hpp>

#include "3D/LandAvoid.h"
#include "3D/VillageLights.h"
#include "Components/Footpath.h"
#include "Components/MapScriptGlobals.h"
#include "Components/Stream.h"
#include "Components/Town.h"
#include "Components/VillageLight.h"

namespace openblack::ecs
{
struct RegistryContext
{
	std::unordered_map<components::Footpath::Id, entt::entity> footpaths;
	std::unordered_map<components::Stream::Id, entt::entity> streams;
	std::unordered_map<uint32_t, entt::entity> towns;
	components::VillageLightFlames villageLightFlames {.clock = 0, .starts = village_lights::k_FlameStarts};
	components::MapScriptGlobals mapScriptGlobals;
	/// The walkers' obstacles by the cells their bounding circles reach, made afresh each turn: the walkers don't yet
	/// sweep the map's cells for obstacles as the game does, and keep to this coarser look until they do
	std::unordered_map<uint32_t, std::unordered_set<entt::entity>> wallHugObstacles;
	/// The rock-tapping sounds go round in turn: the next of the four a tap of the hand plays, and the next a creature's
	/// smash of a rock plays
	uint8_t nextHandRockTap {0};
	uint8_t nextCreatureRockSmash {0};
	/// The scripts let the player go into their temple by tapping its entrance. They allow it until they say otherwise, and
	/// every land starts allowing it again.
	bool scriptLetsTempleBeEntered {true};
	/// The game is over: the local player's temple is being destroyed
	bool gameOver {false};
	/// A skirmish: a playground land played rather than the story's. Losing a temple doesn't end it. openblack plays no
	/// online games, which would be the same.
	bool skirmish {false};
	/// The temple hearts' beam sounds go round in turn: the next of the five a beam plays
	uint32_t nextHeartBeamSound {0};
	/// The order towns are gained by their players in, the next to be given
	uint32_t nextTownGained {0};
	/// Where creatures may go on the land, worked out once the land is there and first asked for
	std::optional<land_avoid::Map> landAvoid;
};
} // namespace openblack::ecs
