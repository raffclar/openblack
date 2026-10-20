/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Graphics/RenderModes.h"

namespace openblack::ecs::components
{

/// The game changed the properties of the materials of the model of this thing while it ran. The model stays changed.
/// Thus the heart of a temple that fades in its destruction writes no depth, and its alpha multiplies the alpha of its
/// texture.
struct ChangedMaterials
{
	graphics::render_modes::Properties properties;
};

} // namespace openblack::ecs::components
