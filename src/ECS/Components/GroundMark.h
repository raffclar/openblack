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

/// The roots of a dead tree that fell, which fade by their own alpha. The piles of roots of blasts and of trees that the
/// hand pulls free are not things of the world (see roots_pile).
struct GroundMark
{
	/// Not in use. The saves of earlier versions keep it.
	float millisecondsLeft {0.0f};
	/// Its alpha, 0 to 255, once it is fading; drawn without blending until then
	std::optional<uint8_t> alpha;
};

} // namespace openblack::ecs::components
