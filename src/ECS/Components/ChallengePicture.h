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
#include <string>
#include <vector>

namespace openblack::ecs::components
{

/// The picture of a challenge the temple's challenge room shows, kept with its record (ChallengeRecord): taken of the
/// screen when the challenge's script asks for it, in high detail for a close look and in low detail for the wall
struct ChallengePicture
{
	/// 256 by 128 RGB, top row first; empty until the first picture is back
	std::vector<uint8_t> full;
	/// 128 by 64 RGB, top row first
	std::vector<uint8_t> small;
	/// The picture of the screen asked for and not back yet, by the name it was asked under
	std::optional<std::string> waiting;
	/// How many pictures have been asked for, which names the next
	uint32_t asked {0};
	/// How many pictures have come back, so that whoever draws them knows when one changed
	uint32_t taken {0};
};

} // namespace openblack::ecs::components
