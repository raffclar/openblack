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

/// The smoke from the chimney of a home, or from the point of a creature fart. Ten puffs rise from the point. Each puff
/// turns, grows and fades, and moves with the air or with the hand when the hand moves near
struct ChimneySmoke
{
	static constexpr size_t k_Puffs = 10;

	enum class State : uint8_t
	{
		/// The fire is lit: puffs start again where they can be seen
		Smoking,
		/// The home has emptied: every puff finishes its life and starts again unseen
		Dying,
		/// All the puffs are gone
		Out,
	};

	struct Puff
	{
		glm::vec3 position {0.0f};
		glm::vec3 velocity {0.0f};
		/// From 0 to 900 over its life. Each step adds 255 for each second of the step and removes the fraction
		int32_t age {0};
		/// Its turn in the plane of the screen, and which way it spins
		float angle {0.0f};
		bool clockwise {false};
		bool hidden {true};
	};

	/// The chimney's top in the world
	glm::vec3 chimney {0.0f};
	/// The smoke's colour: grey from a workshop, white from a home
	uint32_t rgb {0xFFFFFF};
	State state {State::Smoking};
	std::array<Puff, k_Puffs> puffs {};
};

} // namespace openblack::ecs::components
