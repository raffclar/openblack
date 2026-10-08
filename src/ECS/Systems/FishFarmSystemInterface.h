/*******************************************************************************
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

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Animals/FishFarmRules.h"

namespace openblack::ecs::systems
{

/// The towns' fish farms (components::FishFarm): their fish coming back, their shoals swimming and scared, and the hand
/// finding a farm through its fish
class FishFarmSystemInterface
{
public:
	virtual ~FishFarmSystemInterface() = default;

	/// Once a game turn: a fish comes back to each farm every so many turns, and its shoal shows as many as it holds
	virtual void ProcessTurn(uint32_t turn) = 0;
	/// Once a frame of game time: the shoals near enough to the camera swim, and what scared them since the last frame
	/// sends them off
	virtual void Update(float seconds, glm::vec3 camera) = 0;
	/// Something splashes into the sea or steps in it: the fish near it dart away when the shoals next swim
	virtual void Scare(glm::vec3 point) = 0;

	/// The farm whose showing fish are near a point on the sea, none where there are none
	[[nodiscard]] virtual std::optional<entt::entity> FarmWithFishAt(glm::vec2 point) const = 0;
	/// The fish farms' row of the tables, as the rules read it
	[[nodiscard]] virtual fish_farm::Type GetType() const = 0;
	/// Fish taken out of a farm, no more than it has; what was taken
	virtual uint32_t TakeFish(entt::entity farm, uint32_t wanted) = 0;

	/// The world is cleared: nothing scares the fish
	virtual void Reset() = 0;
};

} // namespace openblack::ecs::systems
