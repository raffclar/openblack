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

namespace openblack::ecs::components
{

/// The squat of a creature, for a poo or for a fart. It counts the frames of the squat loop until the next cloud. The
/// count continues from one squat to the next.
struct CreatureSquat
{
	uint32_t frames {0};
};

} // namespace openblack::ecs::components
