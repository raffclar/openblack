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

/// A wonder: a building that adds its power to the power that its tribe gives the player of its town, while it is
/// finished. A new wonder has the power of its size. A wonder that a scaffold starts gets the power of the things near
/// the scaffold.
struct Wonder
{
	float power {0.0f};
};

} // namespace openblack::ecs::components
