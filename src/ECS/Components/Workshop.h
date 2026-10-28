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
#include <vector>

#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{

/// A workshop of a town: it makes scaffolds from its wood, one at a time, and keeps them in the three places of its yard
struct Workshop
{
	/// What a place of the yard holds
	enum class Place : uint8_t
	{
		/// No scaffold uses the place
		Free = 0,
		/// The hand took the scaffold of the place away, but the workshop still counts the scaffold
		Away = 1,
		/// The scaffold of the place stands in it
		Holds = 2,
	};

	/// The game turns until the scaffold that the workshop makes is finished, 0 when it makes none
	int32_t makingTurns {0};
	/// The scaffolds that the workshop counts as its own
	uint32_t counted {0};
	std::array<Place, 3> places {Place::Free, Place::Free, Place::Free};
	/// Its scaffolds, the newest first
	std::vector<entt::entity> scaffolds;
	/// The pile that holds its wood, made when the first wood comes
	entt::entity woodPile {entt::null};
	/// How much its flag shows that it wants wood, 0 to 1, from its last game turn (see workshop::WoodWant)
	float woodWant {0.0f};
	/// Its wood flag, made at its first game turn
	entt::entity flag {entt::null};
};

/// The wood flag of a workshop (see need_flag). The frames move it, thus only the frames read this.
struct WorkshopFlag
{
	entt::entity workshop {entt::null};
	/// How far up it flies, as a share of its full height, and how fast it moves
	float level {0.0f};
	float speed {0.0f};
	/// Whether it showed in the last frame
	bool shown {false};
};

} // namespace openblack::ecs::components
