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
#include <string_view>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs
{
class Registry;
}

/// What a temple's destruction needs of the rest of the game: the entities, the turn's length, the local player, its
/// sounds and spot visuals, a random share, the game-over script and taking the temple away. The game's own world works
/// through the game's systems; tests give a fake.
namespace openblack::ecs::temple_world
{

class World
{
public:
	virtual ~World() = default;

	/// The game's entities, none before there are any
	[[nodiscard]] virtual Registry* Entities() = 0;
	[[nodiscard]] virtual uint32_t MillisecondsPerTurn() const = 0;
	/// The player at this machine, none when there are no players
	[[nodiscard]] virtual std::optional<PlayerNames> LocalPlayer() const = 0;

	/// A sound looping at a place until stopped; none when it couldn't be played
	virtual entt::entity StartLoop(entt::id_type sound, glm::vec3 position, entt::entity owner) = 0;
	/// A sound played once at a place
	virtual void PlayOnce(entt::id_type sound, glm::vec3 position, entt::entity owner) = 0;
	virtual void StopSound(entt::entity emitter) = 0;

	/// A spot visual at a place for so many turns, given a player; none when there are no spot visuals
	virtual std::optional<uint32_t> StartSpotVisual(SpotVisualType type, glm::vec3 position, int32_t turns,
	                                                PlayerNames player) = 0;
	/// The spot visual stays over a thing
	virtual void FollowWithSpotVisual(uint32_t visual, entt::entity target) = 0;

	/// A random share of the game's synced draws, from nothing up to the spread
	[[nodiscard]] virtual float RandomShare(float spread) = 0;
	/// A script of that name is started
	virtual void StartScript(std::string_view name) = 0;
	/// The thing goes from the world
	virtual void Remove(entt::entity object) = 0;
};

} // namespace openblack::ecs::temple_world
