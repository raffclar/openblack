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

#include <array>
#include <optional>
#include <string>

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::archetypes
{
class VillagerArchetype
{
public:
	/// Who a new villager joins: the town nearest its abode, or nobody yet (a newborn joins its mother's home)
	enum class Joins : uint8_t
	{
		NearestTown,
		Nobody,
	};
	static entt::entity Create(const glm::vec3& abodePosition, const glm::vec3& position, VillagerInfo type, uint32_t age,
	                           Joins joins = Joins::NearestTown);

	/// The values of a villager that a saved game of the game keeps
	struct Loaded
	{
		glm::vec3 position;
		float heading;
		float scale;
		VillagerInfo type;
		/// The row of its name in the table of named villagers, for a named villager
		std::optional<uint8_t> nameRow;
		float life;
		uint32_t birthTurn;
		float food;
		uint32_t lastCheckTurn;
		/// The place of the villager in the order of all things that the game made
		uint32_t made;
		uint8_t foodSpeedUp;
		int16_t foodHeld;
		int16_t woodHeld;
		int16_t pregnancy;
		/// The top state, the final state and the state before
		std::array<uint8_t, 3> states;
		uint16_t turnsUntilStateChange;
		uint16_t turnsSinceStateChange;
	};
	/// A villager with the values of a saved game. It uses no random numbers, and it joins no town and no abode.
	static entt::entity CreateLoaded(const Loaded& loaded);
	VillagerArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
