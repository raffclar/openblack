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

#include <optional>

namespace openblack::ecs::components
{

/// The squat of a creature, for a poo or for a fart. It counts the game turns of the squat loop until the next cloud or
/// lump. The count continues from one squat to the next.
struct CreatureSquat
{
	uint32_t turns {0};
	/// The plan that made the smoke of the fart, by the mind's count of agendas. The smoke stops when this plan stops
	std::optional<uint32_t> smokePlan;
};

} // namespace openblack::ecs::components
