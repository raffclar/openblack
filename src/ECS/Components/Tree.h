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

#include "Enums.h"

namespace openblack::ecs::components
{

enum class MagicTreeType
{
};

struct Tree
{
	TreeInfo type;
	/// The largest it grows to
	float maxSize;
	/// Turns until it next grows while among its forest's growing trees, counted down on a 16 bit clock
	uint16_t growthCountdown {0};
	/// Made smaller than its largest size: only such a tree grows, and only while in a forest
	bool madeToGrow {false};
};

} // namespace openblack::ecs::components
