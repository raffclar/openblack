/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec2.hpp>

#include "Graphics/GraphicsHandle.h"

namespace openblack::ecs::components
{

/// The entity's mesh is drawn with this texture in place of its own skins, slid across it by an offset, as the temple's
/// leashes are drawn with their bands of the leash texture. Every entity of the mesh is drawn alike.
struct SkinOverride
{
	graphics::TextureHandle texture;
	glm::vec2 uvOffset {0.0f};
};

} // namespace openblack::ecs::components
