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
	/// The fish left in a farm, as food; 0 for what isn't a farm
	[[nodiscard]] virtual float FishLeft(entt::entity farm) const = 0;
	/// The farm nearest a point, nearer than a distance in metres, as the creature looks for one to fish; none
	[[nodiscard]] virtual std::optional<entt::entity> ClosestFarm(glm::vec3 point, float maxDistance) const = 0;

	// The fishermen, for the villagers' jobs

	/// The farm of a town a fisherman of it at a point is sent to; none when none of them wants one
	[[nodiscard]] virtual std::optional<entt::entity> BestFarmFor(entt::entity town, glm::vec3 fisherman) const = 0;
	/// A villager starts fishing a farm, or stops
	virtual void AddFisherman(entt::entity farm, entt::entity villager) = 0;
	virtual void RemoveFisherman(entt::entity farm, entt::entity villager) = 0;
	/// A spot round the farm a fisherman goes to fish from, drawn at random; the farm's own place for what isn't a farm
	[[nodiscard]] virtual glm::vec3 FishingSpot(entt::entity farm) = 0;
	/// Once a fisherman's fishing animation ends: the fish bite for one fisherman in as many as the farm has, and then
	/// what he catches (see fish_farm::Catch), which he picks up when it isn't 0; none when nothing bit. The farm's own
	/// fish are not taken. After a bite he takes his food to the store when fish_farm::TakesCatchToStore says so.
	[[nodiscard]] virtual std::optional<int32_t> Fish(entt::entity farm, uint32_t capacity, uint32_t held,
	                                                  float tribalPower) = 0;

	/// The world is cleared: nothing scares the fish
	virtual void Reset() = 0;
};

} // namespace openblack::ecs::systems
