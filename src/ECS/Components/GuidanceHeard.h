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

namespace openblack::ecs::components
{

/// The kinds of guidance the player at this machine hears, each kept from being heard again too soon
enum class GuidanceSound : uint8_t
{
	DeathInVillage = 7,
};

/// When the player at this machine last heard each kind of guidance, by turn, on the player's entity
struct GuidanceHeard
{
	static constexpr size_t k_Kinds = 33;
	std::array<uint32_t, k_Kinds> lastTurn {};
};

} // namespace openblack::ecs::components
