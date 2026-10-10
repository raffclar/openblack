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

#include <array>
#include <optional>
#include <string>

#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

struct Player
{
	/// The game's magic types, None first
	static constexpr size_t k_MagicTypeCount = 42;
	/// The tribes, by tribe number
	static constexpr size_t k_TribeCount = static_cast<size_t>(Tribe::_COUNT);
	/// Every tribe's power at its usual, 1
	static constexpr std::array<float, k_TribeCount> k_UsualTribalPower = [] {
		std::array<float, k_TribeCount> power {};
		power.fill(1.0f);
		return power;
	}();

	/// The miracles the player may cast and the power each tribe gives them
	struct Miracles
	{
		/// How many things enable each magic type for the player, by magic type: the type is enabled while any does
		std::array<uint32_t, k_MagicTypeCount> enabled {};
		/// The magic types ever enabled for the player
		std::array<bool, k_MagicTypeCount> everEnabled {};
		/// Every magic type is enabled, whatever enables it
		bool allEnabled {false};
		/// The power each tribe gives the player's miracles, by tribe, and the most it has been; 1 is no more than usual
		std::array<float, k_TribeCount> tribalPower {k_UsualTribalPower};
		std::array<float, k_TribeCount> maxTribalPower {k_UsualTribalPower};
	};

	PlayerNames name;
	/// The harm each player's miracles did to what is this player's, by player number: their crushing and hitting, and the
	/// burning they would do. A creature's miracles aren't counted.
	std::array<float, 8> damageFrom {};
	/// Where, what and the turn the player last cast a miracle, and how many of each magic type they have cast
	struct Cast
	{
		glm::vec3 position {0.0f};
		MagicType type {MagicType::None};
		uint32_t turn {0};
	};
	std::optional<Cast> lastCast;
	std::array<uint32_t, k_MagicTypeCount> castsOfType {};
	/// A script made the things the player's hand throws fly without the air's drag
	uint32_t windResistance {0};
	/// How many of the player's people have died, how many people the player has been put down for killing, and how
	/// many the player has sacrificed
	uint32_t villagersLost {0};
	uint32_t villagersKilled {0};
	uint32_t sacrifices {0};
	/// All the prayer power the player's worship sites have given
	float totalChantsUsed {0.0f};
	Miracles miracles;

	/// Each player's colour, 0xAARRGGBB, by player number; the neutral player's is black
	static constexpr std::array<uint32_t, 8> k_Colours = {
	    0xFFFF4646u, // red
	    0xFF47FF54u, // green
	    0xFFE347FFu, // magenta
	    0xFF47F9FFu, // cyan
	    0xFFFFFD47u, // yellow
	    0xFF4777FFu, // blue
	    0xFFFFA247u, // orange
	    0xFF000000u, // black
	};
};
} // namespace openblack::ecs::components
