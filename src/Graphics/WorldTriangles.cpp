/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

// The original's world triangle draws for openblack: see WorldTriangles.h.

#include "WorldTriangles.h"

#include <cmath>
#include <cstring>

#include <algorithm>
#include <memory>
#include <unordered_set>

#include <bgfx/bgfx.h>
#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/SeaPass.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;

const bgfx::VertexLayout& world_triangles::Layout()
{
	static const bgfx::VertexLayout k_Layout = [] {
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		    .add(bgfx::Attrib::Color1, 4, bgfx::AttribType::Uint8, true)
		    .end();
		return layout;
	}();
	return k_Layout;
}

void world_triangles::Frame::Clear()
{
	vertices.clear();
	batches.clear();
}

void world_triangles::Frame::Append(const MaterialRef& material, render_modes::Table table, uint8_t globalAlpha,
                                    std::span<const Vertex> v, const void* tag)
{
	if (v.empty())
	{
		return;
	}
	const auto first = static_cast<uint32_t>(vertices.size());
	vertices.insert(vertices.end(), v.begin(), v.end());
	if (!batches.empty())
	{
		auto& last = batches.back();
		if (last.material == material && last.table == table && last.globalAlpha == globalAlpha && last.tag == tag &&
		    last.firstVertex + last.vertexCount == first)
		{
			last.vertexCount += static_cast<uint32_t>(v.size());
			return;
		}
	}
	batches.push_back({material, table, globalAlpha, first, static_cast<uint32_t>(v.size()), tag});
}

// (openblack) the lookup of a primitive's texture the models (Renderer::DrawSubMesh, the shadow casters) and these
// triangles share; it was Renderer.cpp's GetTexture
const Texture2D* world_triangles::PrimitiveTexture(const L3DMesh& mesh, uint32_t skinId)
{
	if (skinId == 0xFFFFFFFF)
	{
		return nullptr;
	}
	const auto& meshSkins = mesh.GetSkins();
	if (const auto it = meshSkins.find(skinId); it != meshSkins.end())
	{
		return it->second.get();
	}
	const auto& textureManager = Locator::resources::value().GetTextures();
	if (textureManager.Contains(skinId))
	{
		return &*textureManager.Handle(skinId);
	}
	if (!meshSkins.empty())
	{
		// Some modded packs embed a mesh's skin under a placeholder id (0x1001) while its material still names a pack
		// texture that the pack does not have: use the mesh's own skin.
		return meshSkins.begin()->second.get();
	}
	static std::unordered_set<uint32_t> reported;
	if (reported.insert(skinId).second)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("graphics"), "Could not find the texture {:#x}", skinId);
	}
	return nullptr;
}

uint32_t world_triangles::Submit(RenderPass view, const Frame& frame, const ShaderManager& shaders, const void* only)
{
	if (frame.batches.empty() || !Locator::resources::has_value())
	{
		return 0;
	}
	const auto* program = shaders.GetShader("WorldTriangles");
	if (program == nullptr)
	{
		return 0;
	}
	const auto& layout = Layout();
	// the batches drawn and their vertices, in order
	uint32_t total = 0;
	for (const auto& batch : frame.batches)
	{
		if (only == nullptr || batch.tag == only)
		{
			total += batch.vertexCount;
		}
	}
	if (total == 0)
	{
		return 0;
	}
	// (openblack guard) the transient buffer is shared with the sprites, chains and rain of the frame: what does not
	// fit is not drawn this frame
	const uint32_t room = bgfx::getAvailTransientVertexBuffer(total, layout);
	if (room < total)
	{
		static bool warned = false;
		if (!warned)
		{
			warned = true;
			SPDLOG_LOGGER_WARN(spdlog::get("graphics"),
			                   "world_triangles: {} vertices this frame, room for {}: the last batches are not drawn", total,
			                   room);
		}
	}
	const uint32_t uploaded = std::min(room, total);
	if (uploaded < 3)
	{
		return 0;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, uploaded, layout);
	auto* out = reinterpret_cast<Vertex*>(buffer.data);
	static_assert(sizeof(Vertex) == 28, "Layout(): 3 + 2 floats and 2 x 4 bytes");

	auto& meshes = Locator::resources::value().GetMeshes();
	uint32_t start = 0;
	uint32_t drawn = 0;
	for (const auto& batch : frame.batches)
	{
		if (only != nullptr && batch.tag != only)
		{
			continue;
		}
		// whole triangles only
		const uint32_t count = std::min(batch.vertexCount, (uploaded - start) / 3 * 3);
		if (count == 0)
		{
			break;
		}
		std::memcpy(out + start, frame.vertices.data() + batch.firstVertex, count * sizeof(Vertex));
		const uint32_t first = start;
		start += count;
		if (!meshes.Contains(batch.material.meshId))
		{
			continue;
		}
		const auto mesh = meshes.Handle(batch.material.meshId);
		const auto& subMeshes = mesh->GetSubMeshes();
		if (batch.material.subMesh >= subMeshes.size() ||
		    batch.material.primitive >= subMeshes[batch.material.subMesh]->GetPrimitives().size())
		{
			continue;
		}
		const auto& prim = subMeshes[batch.material.subMesh]->GetPrimitives()[batch.material.primitive];
		const auto* texture = PrimitiveTexture(*mesh, prim.skinID);
		// the material's mode through the current table
		const auto drawnMode = render_modes::Select(static_cast<render_modes::Mode>(prim.materialType), batch.table);
		if (texture != nullptr)
		{
			// the models' sampler: clamped without the material's tiling bit (Renderer::DrawSubMesh).
			// (inferred) the tiling switch as the models have it while the main draw runs
			const uint32_t samplerFlags = prim.wrap
			                                  ? UINT32_MAX
			                                  : (texture->GetSamplerFlags() & ~(BGFX_SAMPLER_U_MASK | BGFX_SAMPLER_V_MASK)) |
			                                        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
			program->SetTextureSampler("s_diffuse", 0, *texture, samplerFlags);
		}
		// y: ALPHAREF / 255 of the selected mode (-1 without the alpha test), w: its stage 0 alpha (fs_object).
		// (approximate) ALPHAREF of modes 9 / 15 with the alpha table is scaled by the object diffuse, which the pieces'
		// draw does not set (it is the last object drawn's): the batch's alpha is used
		const auto alpha = render_modes::PrimitiveAlpha(
		    drawnMode, batch.table, static_cast<uint8_t>(std::lround(prim.alphaCutoutThreshold * 255.0f)), batch.globalAlpha);
		const glm::vec4 u_skyAlphaThreshold = {0.0f, alpha.ref, 0.0f, static_cast<float>(alpha.source)};
		program->SetUniformValue("u_skyAlphaThreshold", &u_skyAlphaThreshold); // fs
		// an untextured primitive: its material colour x the vertex colour (fs_object)
		const glm::vec4 u_materialColour = {glm::vec3(prim.colour), texture == nullptr ? 1.0f : 0.0f};
		program->SetUniformValue("u_materialColour", &u_materialColour); // fs
		program->SetUniformValue("u_objectClip", &sea_pass::k_NoClip);   // fs: not a sea draw
		bgfx::setVertexBuffer(0, &buffer, first, count);
		// CULLMODE from the material's two-sided bit; the blended modes leave the target's alpha alone
		// as the models' blended primitives do (render_modes::PrimitiveState)
		auto options = render_modes::k_ModelPass;
		options.cull = render_modes::CullFor(prim.twoSided, false);
		const bool blended = render_modes::Desc(drawnMode).blend != render_modes::Blend::Disabled;
		bgfx::setState(render_modes::PrimitiveState(drawnMode, options, blended));
		bgfx::submit(static_cast<bgfx::ViewId>(view), toBgfx(program->GetRawHandle()));
		++drawn;
	}
	return drawn;
}

bool world_triangles::SubmitRaw(RenderPass view, std::span<const Vertex> vertices, std::span<const uint16_t> indices,
                                const Texture2D& diffuse, const Texture2D& alpha, const render_modes::Material& material,
                                const ShaderManager& shaders, const render_modes::StateOptions& options)
{
	const auto* program = shaders.GetShader("WorldQuad");
	if (program == nullptr || vertices.empty() || indices.size() < 3)
	{
		return false;
	}
	const auto& layout = Layout();
	const auto vertexCount = static_cast<uint32_t>(vertices.size());
	const auto indexCount = static_cast<uint32_t>(indices.size() / 3 * 3);
	// (openblack guard) the transient buffers are shared with the rest of the frame
	if (bgfx::getAvailTransientVertexBuffer(vertexCount, layout) < vertexCount ||
	    bgfx::getAvailTransientIndexBuffer(indexCount) < indexCount)
	{
		return false;
	}
	bgfx::TransientVertexBuffer vertexBuffer;
	bgfx::TransientIndexBuffer indexBuffer;
	bgfx::allocTransientVertexBuffer(&vertexBuffer, vertexCount, layout);
	bgfx::allocTransientIndexBuffer(&indexBuffer, indexCount);
	static_assert(sizeof(Vertex) == 28, "Layout(): 3 + 2 floats and 2 x 4 bytes");
	std::memcpy(vertexBuffer.data, vertices.data(), vertexCount * sizeof(Vertex));
	std::memcpy(indexBuffer.data, indices.data(), indexCount * sizeof(uint16_t));
	program->SetTextureSampler("s_diffuse", 0, diffuse);
	program->SetTextureSampler("s_alpha", 1, alpha);
	// set on every draw of this function
	const glm::vec4 u_alphaTest {render_modes::AlphaTest(material), 0.0f, 0.0f, 0.0f};
	program->SetUniformValue("u_alphaTest", &u_alphaTest);
	bgfx::setVertexBuffer(0, &vertexBuffer);
	bgfx::setIndexBuffer(&indexBuffer);
	// the material through the current table (the normal one for these draws) and CULLMODE from its two-sided bit; the
	// vertices are in the world, no mirror
	bgfx::setState(render_modes::State(material, options));
	bgfx::submit(static_cast<bgfx::ViewId>(view), toBgfx(program->GetRawHandle()));
	// bgfx keeps a uniform until it is set again, and other draws use this program without setting it: put the test
	// back off so that they never inherit it
	if (u_alphaTest.x >= 0.0f)
	{
		const glm::vec4 u_noAlphaTest {-1.0f, 0.0f, 0.0f, 0.0f};
		program->SetUniformValue("u_alphaTest", &u_noAlphaTest);
	}
	return true;
}
