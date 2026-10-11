/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{

struct BigForest
{
	int type;
	/// The wood it holds, which its life scales into what it is worth: at first its size times its kind's wood, then less
	/// as wood is taken from it
	float wood {0.0f};
};

struct Forest
{
	int type;
};

} // namespace openblack::ecs::components
