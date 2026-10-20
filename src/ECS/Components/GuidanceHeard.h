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
#include <vector>

#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{

/// The kinds of guidance the player at this machine hears, each kept from being heard again too soon
enum class GuidanceSound : uint8_t
{
	DeathInVillage = 7,
};

/// When the player at this machine last heard each kind of guidance, by turn, on the player's entity, and what the
/// advisors' remarks keep track of for them
struct GuidanceHeard
{
	static constexpr size_t k_Kinds = 33;
	static constexpr size_t k_OneOffs = 7;
	std::array<uint32_t, k_Kinds> lastTurn {};
	/// How far the player's alignment has been drifting lately
	float alignmentDrift {0.0f};
	/// The remarks said at most once a land, already said on this one
	std::array<bool, k_OneOffs> oneOffSaid {};
	/// The voice that the town desires played last, 0 for none on this land
	uint32_t lastDesireVoice {0};
	/// A thing that a town desire voice looked at, and the turn from which its time counts
	struct SeenThing
	{
		entt::entity thing {entt::null};
		uint32_t since {0};
	};
	/// The things that the town desire voices looked at on this land, the newest first
	std::vector<SeenThing> seenThings;
};

} // namespace openblack::ecs::components
