/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The helpers that the renderer's source files share. Only those files include this header.

#include <cstdint>

#include <array>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>

#include <bgfx/bgfx.h>
#include <entt/core/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "3D/L3DMesh.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/VertexBuffer.h"
#include "Visual/RenderPass.h"

namespace openblack::graphics
{
class FrameBuffer;
class ShaderProgram;
class Texture2D;

/// The layouts a creature's variant meshes are read with as the second to fourth vertex streams: their positions and
/// normals as the attributes the morphing vertex shader takes them in, the rest of each vertex skipped
class MorphStreamLayouts
{
public:
	MorphStreamLayouts()
	{
		constexpr std::array<std::pair<bgfx::Attrib::Enum, bgfx::Attrib::Enum>, 3> k_Attributes {{
		    {bgfx::Attrib::Tangent, bgfx::Attrib::Bitangent},
		    {bgfx::Attrib::Color1, bgfx::Attrib::Color2},
		    {bgfx::Attrib::Color3, bgfx::Attrib::Weight},
		}};
		// As L3DSubMesh packs a vertex: position, texture coordinates, normal and bone indices
		constexpr uint8_t k_TexCoordBytes = 2 * sizeof(float);
		constexpr uint8_t k_IndicesBytes = 4 * sizeof(int16_t);
		for (size_t axis = 0; axis < k_Attributes.size(); ++axis)
		{
			bgfx::VertexLayout layout;
			layout.begin()
			    .add(k_Attributes.at(axis).first, 3, bgfx::AttribType::Float)
			    .skip(k_TexCoordBytes)
			    .add(k_Attributes.at(axis).second, 3, bgfx::AttribType::Float)
			    .skip(k_IndicesBytes)
			    .end();
			_layouts.at(axis) = fromBgfx(bgfx::createVertexLayout(layout));
		}
	}
	~MorphStreamLayouts()
	{
		for (const auto& layout : _layouts)
		{
			if (bgfx::isValid(toBgfx(layout)))
			{
				bgfx::destroy(toBgfx(layout));
			}
		}
	}
	MorphStreamLayouts(const MorphStreamLayouts&) = delete;
	MorphStreamLayouts& operator=(const MorphStreamLayouts&) = delete;
	MorphStreamLayouts(MorphStreamLayouts&&) = delete;
	MorphStreamLayouts& operator=(MorphStreamLayouts&&) = delete;

	[[nodiscard]] VertexLayoutHandle Get(size_t axis) const { return _layouts.at(axis); }

private:
	std::array<VertexLayoutHandle, 3> _layouts {};
};

namespace detail
{
// clang-format off
constexpr auto k_BgfxDefaultStateInvertedZ = 0 \
                                     | BGFX_STATE_WRITE_RGB \
                                     | BGFX_STATE_WRITE_A \
                                     | BGFX_STATE_WRITE_Z \
                                     | BGFX_STATE_CULL_CW \
                                     | BGFX_STATE_DEPTH_TEST_GREATER \
                                     | BGFX_STATE_MSAA;
// clang-format on

/// The moving shadows' data comes in rows of this many texels (see ground_shadow.sh)
constexpr uint16_t k_GroundShadowDataWidth = 64;

/// How finely the land's block textures are painted, for the terrain shader (see u_landTexels there)
glm::vec4 LandTexelsUniform(int blockTexels);
/// How deep the snow lies over the island, as a texture the shaders read point by point, refreshed when the snow
/// changes; none without snow
const Texture2D* SnowDepth(std::unique_ptr<Texture2D>& texture, std::optional<uint32_t>& revision);

/// The land's light a creature, its eyes and its hair take: halved in the sea's reflection, as the game halves every
/// level of the land's light while it draws the reflection
float CreatureLandLightScale(RenderPass viewId);

/// A view drawing into a target a texel for each of the land's cell corners, in texels, a row for each cell along z,
/// whichever way up the backend keeps its targets. Its draws are kept in order.
bgfx::ViewId SetUpLandView(RenderPass pass, const FrameBuffer& frameBuffer, glm::u16vec2 size);
/// A rectangle of cells drawn into a land view, from a position and texture coordinate to another; the program's
/// samplers and uniforms are set before
void SubmitLandQuad(bgfx::ViewId viewId, const ShaderProgram& program, glm::vec2 from, glm::vec2 to, glm::vec2 uvFrom,
                    glm::vec2 uvTo, uint64_t state);
} // namespace detail
} // namespace openblack::graphics
