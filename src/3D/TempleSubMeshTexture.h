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

#include "Graphics/GraphicsHandle.h"

namespace openblack
{

/// A texture a room's mesh draws one of its submeshes with, in place of its skin
struct TempleSubMeshTexture
{
	uint32_t subMesh;
	graphics::TextureHandle texture;
	/// The submesh is a picture of the challenge room or the save game room. The game draws a picture in one colour,
	/// fogged once at its first vertex, with no lightmap and no light.
	bool picture {false};
};

} // namespace openblack
