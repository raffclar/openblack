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

#include <vector>

#include "Creature/CreatureWebPage.h"

namespace openblack::ecs::components
{

/// The pictures of a creature for its web page (see creature_web_page). The creature keeps them only while it lives
/// on the land: a creature that a land loads starts with none.
struct CreaturePictures
{
	/// The pictures that it keeps, at most five, in the order that they came
	std::vector<creature_web_page::Picture> pictures;
	/// The game turn of the last picture. The first picture can come once more than ten seconds of the game went by.
	uint32_t lastTurn {0};
};

} // namespace openblack::ecs::components
