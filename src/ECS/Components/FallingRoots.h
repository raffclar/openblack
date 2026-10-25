/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Physics/ObjectRules.h"

namespace openblack::ecs::components
{

/// A tree that fell dead: the first time it is drawn, its roots drop from it
struct DropsRoots
{
};

/// A tree out of the land, drawn with its roots under it, and where it is, which says how many roots and how big
struct ShownRoots
{
	physics::objects::TreePlace place {physics::objects::TreePlace::OutOfTheLand};
};

} // namespace openblack::ecs::components
