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

#include <span>
#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Graphics/RenderModes.h"
#include "Graphics/RenderPass.h"

namespace bgfx
{
struct VertexLayout;
}

// Triangles already in the world, already lit on the CPU, drawn at once in an L3D primitive's material, as the
// original draws the exploded pieces of Particles/Rules/ExplodeObject.h: a software transform through the world-to-clipping
// matrix as it is (the world matrix is the identity), no specular, the culling of the material (none with its
// two-sided bit, else counter-clockwise), the material set through the current mode table and ONE draw for the whole
// primitive, at once: no Z object.
// Its indexed sibling (for FragMesh and the pieces' indexed draw) is the same transform, the same culling (without the
// two-sided bit a backface test on the screen points, which keeps what that CULLMODE keeps) and the same material,
// but it also copies each vertex's specular (colour + specular pairs): Vertex::specular.
//
// A frame's batches go up in one transient vertex buffer (no bgfx handle per piece: the 4096 handles were the cause
// of an old crash) and are submitted in the order they were appended. The program is "WorldTriangles" =
// vs_world_triangles + fs_object, so the alpha test and stage 0 alpha are the models'.
namespace openblack::graphics
{
class L3DMesh;
class ShaderManager;
class Texture2D;
} // namespace openblack::graphics

namespace openblack::graphics::world_triangles
{

/// One vertex: in the world, with its colour already lit (D3DCOLOR 0xAARRGGBB turned to bgfx's Color0 ABGR); the
/// non-indexed draw has no specular, the indexed one has (`specular`, Color1 ABGR, 0 for the pieces)
struct Vertex
{
	glm::vec3 position;
	glm::vec2 uv;
	uint32_t abgr;
	uint32_t specular {0}; ///< ABGR, added after the texture (fs_object), its alpha unused
};
/// Position 3 floats, TexCoord0 2 floats, Color0 and Color1 4 normalised bytes each
[[nodiscard]] const bgfx::VertexLayout& Layout();

/// 0xAARRGGBB to the ABGR bytes bgfx reads for Color0
[[nodiscard]] constexpr uint32_t ToAbgr(uint32_t argb)
{
	return (argb & 0xFF00FF00u) | ((argb >> 16) & 0xFFu) | ((argb & 0xFFu) << 16);
}

/// The material a primitive of triangles is drawn with: an L3D mesh's primitive (a piece's source primitive): its
/// texture (skin), mode, flag bits (two-sided, tiling) and ALPHAREF
struct MaterialRef
{
	entt::id_type meshId {0};
	uint16_t subMesh {0};
	uint16_t primitive {0};

	[[nodiscard]] bool operator==(const MaterialRef&) const = default;
};

/// A run of triangles (a list without indices: three vertices each) in one material and mode table
struct Batch
{
	MaterialRef material;
	render_modes::Table table {render_modes::Table::Normal}; ///< the current mode table while it is drawn
	/// The draw's alpha byte, for render_modes::PrimitiveAlpha (ALPHAREF of modes 9 / 15 with the alpha table)
	uint8_t globalAlpha {255};
	uint32_t firstVertex {0};
	uint32_t vertexCount {0};
	/// Who appended it (a Queued / Immediate atom drawn on its own: Submit's `only`), nullptr for the Sorted ones
	const void* tag {nullptr};
};

/// One frame's triangles, refilled where they are drawn
struct Frame
{
	std::vector<Vertex> vertices;
	std::vector<Batch> batches;

	void Clear();
	/// The vertices of one primitive (three per triangle) at the end; joined to the last batch when its material,
	/// table, alpha and tag are the same (the pieces of one source primitive come one after the other from ExplodeMesh,
	/// so the order of the draws is kept with few calls)
	void Append(const MaterialRef& material, render_modes::Table table, uint8_t globalAlpha, std::span<const Vertex> v,
	            const void* tag = nullptr);
};

/// Uploads the vertices of the batches drawn in ONE transient vertex buffer and submits one draw per batch, in order,
/// to `view`: the primitive's texture (s_diffuse, clamped without its tiling bit as Renderer::DrawSubMesh does), its
/// alpha test and stage 0 alpha (u_skyAlphaThreshold = render_modes::PrimitiveAlpha(Select(mode, table), table,
/// ALPHAREF, globalAlpha)), the material colour of an untextured primitive (u_materialColour), and the state
/// render_modes::PrimitiveState of the selected mode with the culling of the material (its two-sided bit).
/// `only`: just the batches with that tag (nullptr: all). (openblack guard) when the transient buffer cannot take them
/// all, the first ones that fit are drawn and a warning is given once. Returns the batches drawn
uint32_t Submit(RenderPass view, const Frame& frame, const ShaderManager& shaders, const void* only = nullptr);

/// The same world triangles in a material made on a .raw texture (the influence border's, the ripples' smoke) instead
/// of an L3D primitive's: indexed triangles (three indices each) drawn at once with the program "WorldQuad" (vs_blob +
/// fs_world_quad: colour = texture x diffuse, alpha = the alpha file x diffuse alpha, the stage 0 of mode 6; no fog and
/// no specular), `diffuse` and `alpha` the X.raw / Xa.raw textures with their own samplers (the .raw loader's Repeat,
/// which the tiling bit of the materials drawn this way asks for), and render_modes::State of the material (its mode,
/// the culling of its two-sided bit). Only for the modes whose stage 0 is that one (mode 6; mode 13, the intro light's
/// SRCALPHA / ONE, has the same stage 0). `options`: the draw's own render states (the intro light's ZFUNC; the default
/// is the plain material). (openblack guard) nothing is drawn when the transient buffers cannot take it all.
/// Returns true when it was drawn
bool SubmitRaw(RenderPass view, std::span<const Vertex> vertices, std::span<const uint16_t> indices, const Texture2D& diffuse,
               const Texture2D& alpha, const render_modes::Material& material, const ShaderManager& shaders,
               const render_modes::StateOptions& options = {});

/// The texture of an L3D primitive: the mesh's skins (or its SetSkinSource's), else the texture manager; nullptr for
/// an untextured one. The models' lookup (Renderer::DrawSubMesh and the shadow casters use it too)
[[nodiscard]] const Texture2D* PrimitiveTexture(const L3DMesh& mesh, uint32_t skinId);

} // namespace openblack::graphics::world_triangles
