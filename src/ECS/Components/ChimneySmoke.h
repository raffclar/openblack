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

#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// The smoke of an Abode whose mesh has a chimney, made with the abode. Simulated and drawn by ecs::chimney_smoke.
struct ChimneySmoke
{
	static constexpr uint32_t k_Puffs = 10; ///< 10 sprites

	enum class State : uint8_t
	{
		Active = 0, ///< Puffs are reborn visible
		Dying = 2,  ///< the house emptied: every puff finishes its life and is reborn hidden
		Dead = 3,   ///< Nothing drawn any more
	};

	struct Puff
	{
		glm::vec3 position {0.0f}; ///< The sprite's position
		glm::vec3 velocity {0.0f};
		int32_t age {0};        ///< In 1/255 s; i x 90 at the start
		float angle {0.0f};     ///< Random(0, pi) at the start
		bool clockwise {false}; ///< (int)Random(1, 100) & 1, the sense of the spin
		bool hidden {true};     ///< 1 at the start
	};

	glm::vec3 position {0.0f}; ///< The chimney in world space (GetChimneyPos)
	State state {State::Active};
	uint32_t rgb {0xFFFFFF};            ///< 0x808080 for a workshop
	float ageRemainder {0.0f};          ///< the fraction of the age step kept between frames (openblack, see ChimneySmoke.cpp)
	std::array<Puff, k_Puffs> puffs {}; ///< The 10 sprites
};

} // namespace openblack::ecs::components
