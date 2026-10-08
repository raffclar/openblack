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

namespace openblack::ecs::components
{

/// The reactions a living thing reacted to lately, {type, turn}, at most 3 (the others are forgotten after 1800
/// turns). Every Living class keeps them (ECS/Effects/Reactions: Records, RecordTurn, RefreshRecord).
struct ReactionRecords
{
	struct Record
	{
		uint8_t type;
		uint32_t turn;
	};
	std::array<Record, 3> records {};
	uint8_t count {0};
};

} // namespace openblack::ecs::components
