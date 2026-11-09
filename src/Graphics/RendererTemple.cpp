/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The renderer is one of the locator's implementations
#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>
#include <cstring>

#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <bgfx/bgfx.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#include "3D/Clouds.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSkinTexels.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLightTable.h"
#include "3D/OceanInterface.h"
#include "3D/OrientedText.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/TempleMap.h"
#include "3D/TemplePool.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Temple.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/CreatureReflection.h"
#include "Graphics/DebugLines.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/GroundBlobs.h"
#include "Graphics/HandLight.h"
#include "Graphics/HandReflection.h"
#include "Graphics/HandWaterGlow.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/MeshDetail.h"
#include "Graphics/RendererDetail.h"
#include "Graphics/ShaderManager.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/Rereads.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "entt/locator/locator.hpp"

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::graphics::detail;
using namespace openblack::ecs::systems;

namespace
{
/// The triangle that a warm-up draws in place of the temple's text or map when they have none: a triangle of no size
constexpr std::array<OrientedTextVertex, 3> k_WarmUpTriangle {};
} // namespace

void Renderer::DrawTempleText(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || !Locator::temple::has_value() || !Locator::temple::value().Active())
	{
		return;
	}
	const auto& temple = Locator::temple::value();
	// The words come with the rooms' scrolls on a visit. A warm-up draws a triangle of no size in their place.
	const auto vertices = _warmingUp && temple.GetText().empty() ? std::span<const OrientedTextVertex>(k_WarmUpTriangle)
	                                                             : std::span<const OrientedTextVertex>(temple.GetText());
	const auto* texture = temple.GetTextTexture();
	if (vertices.empty() || texture == nullptr)
	{
		return;
	}
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	static_assert(sizeof(OrientedTextVertex) == (3 + 2 + 1) * sizeof(float));
	const auto count = static_cast<uint32_t>(vertices.size());
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, vertices.data(), count * sizeof(OrientedTextVertex));

	// The game draws the glyphs blended by their coverage, tested against the room's depth
	const auto* shader = _shaderManager->GetShader("Text3D");
	const auto model = glm::translate(glm::mat4(1.0f), temple.GetPosition());
	bgfx::setTransform(glm::value_ptr(model));
	bgfx::setVertexBuffer(0, &buffer);
	shader->SetTextureSampler("s_texture", 0, *texture);
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA |
	               BGFX_STATE_BLEND_ALPHA);
	shader->Submit(static_cast<bgfx::ViewId>(desc.viewId));
}

void Renderer::DrawTemplePool(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || !Locator::temple::has_value() || !Locator::temple::value().Active())
	{
		return;
	}
	const auto& temple = Locator::temple::value();
	const auto pool = entt::hashed_string("temple/interior/mainwater_l3d").value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!temple.IsRoomDrawn(TempleRoom::Main) || !meshes.Contains(pool))
	{
		return;
	}
	// The alpha of the water follows the sine of the time
	const float time = temple.GetPoolTime();
	const auto alpha = temple_pool::FirstLayerAlpha(time);
	const auto& light = temple.GetLight();
	const auto layerLight = temple_pool::LayerLight(light);
	struct Layer
	{
		glm::vec3 position;
		float yaw;
		int32_t alpha;
		glm::vec2 uvOffset;
	};
	const std::array layers = {
	    Layer {glm::vec3(0.0f), 0.0f, alpha, glm::vec2(-0.01f, 0.007f) * time},
	    Layer {glm::vec3(0.0f, 0.05f, 0.0f), glm::quarter_pi<float>(), temple_pool::SecondLayerAlpha(alpha),
	           glm::vec2(0.01f, 0.005f) * time},
	};
	// Under the water the room's reflection shows, where the floor leaves it uncovered
	{
		const auto model = glm::translate(glm::mat4(1.0f), temple.GetPosition());
		const auto* reflection = _shaderManager->GetShader("Reflection");
		L3DMeshSubmitDesc submitDesc = {};
		submitDesc.viewId = desc.viewId;
		submitDesc.program = reflection;
		// Beneath the first layer, at its height, so it leaves the depth to the layers
		submitDesc.state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_MSAA;
		submitDesc.useMaterialCulling = true;
		submitDesc.modelMatrices = &model;
		submitDesc.matrixCount = 1;
		// Each submesh's draw lets go of the textures bound for it
		const auto& mesh = *meshes.Handle(pool);
		for (uint8_t subMesh = 0; subMesh < mesh.GetNumSubMeshes(); ++subMesh)
		{
			reflection->SetTextureSampler("s_reflection", 4,
			                              Locator::oceanSystem::value().GetReflectionFramebuffer().GetColorAttachment());
			DrawMesh(mesh, submitDesc, subMesh);
		}
	}
	for (const auto& layer : layers)
	{
		const auto model = glm::translate(glm::mat4(1.0f), temple.GetPosition() + layer.position) *
		                   glm::rotate(glm::mat4(1.0f), layer.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
		L3DMeshSubmitDesc submitDesc = {};
		submitDesc.viewId = desc.viewId;
		// In their colour alone, without the pool's lightmap: through it, the water is far darker than the game's
		// TODO(raffclar): confirm how the game lights an object given a colour
		submitDesc.program = _shaderManager->GetShader("Object");
		// Each primitive blended, culled and writing depth as its material says, as the room's meshes are
		submitDesc.state = k_BgfxDefaultStateInvertedZ;
		submitDesc.useMaterialBlending = true;
		submitDesc.useMaterialCulling = true;
		submitDesc.modelMatrices = &model;
		submitDesc.matrixCount = 1;
		submitDesc.uvOffset = layer.uvOffset;
		submitDesc.tint = glm::vec4(layerLight, static_cast<float>(layer.alpha) / 255.0f);
		submitDesc.lightAdd = light.add;
		DrawMesh(*meshes.Handle(pool), submitDesc, std::numeric_limits<uint8_t>::max());
	}
}

void Renderer::DrawTempleMapPass() const
{
	// The view keeps its clear from one frame to the next, so it is only touched to draw the land afresh
	const auto viewId = static_cast<bgfx::ViewId>(RenderPass::TempleMap);
	if (!Locator::temple::has_value() || !Locator::temple::value().Active() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto visit = Locator::temple::value().GetVisits();
	if (_templeMapVisit == visit)
	{
		// The map shows the land of this visit already. The warm-up of a new load makes the triangles of the land's
		// blocks for the next visit.
		if (_warmingUp)
		{
			for (const auto& block : Locator::terrainSystem::value().GetBlocks())
			{
				[[maybe_unused]] const auto* triangles = block.GetTriangles(land_detail::Mesh::Full, 0, true);
			}
		}
		return;
	}
	_templeMapVisit = visit;

	// 8 texels a block, as the game's pages of the land's textures give the map, over the 32 by 32 blocks there can be
	constexpr uint16_t k_Size = 256;
	if (!_templeMapFrameBuffer)
	{
		_templeMapFrameBuffer = std::make_unique<FrameBuffer>("Temple Map", k_Size, k_Size, TextureFormat::RGBA8, std::nullopt,
		                                                      static_cast<uint8_t>(1), Wrapping::ClampEdge);
	}
	_templeMapFrameBuffer->Bind(RenderPass::TempleMap);
	bgfx::setViewRect(viewId, 0, 0, k_Size, k_Size);
	bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR, 0x00000000);

	// From above, x across the texture and z down it, as the map's texture coordinates run. The terrain's shader pulls
	// vertices at sea level towards the view's origin, so the view is centred on the texture to keep that small.
	constexpr float k_Half = TempleMap::k_TextureSpan * 0.5f;
	const auto view = glm::translate(glm::mat4(1.0f), glm::vec3(-k_Half, 0.0f, -k_Half));
	const float down = bgfx::getCaps()->originBottomLeft ? 1.0f : -1.0f;
	auto projection = glm::mat4(0.0f);
	projection[0][0] = 1.0f / k_Half;
	projection[2][1] = down / k_Half;
	projection[3][2] = 0.5f;
	projection[3][3] = 1.0f;
	bgfx::setViewTransform(viewId, glm::value_ptr(view), glm::value_ptr(projection));
	bgfx::touch(viewId);

	const auto& island = Locator::terrainSystem::value();
	const auto* terrainShader = _shaderManager->GetShader("Terrain");
	const auto islandExtent = glm::vec4(island.GetExtent().minimum, island.GetExtent().maximum);
	auto smallBump = Locator::resources::value().GetTextures().Handle(LandIslandInterface::k_SmallBumpTextureId);
	// The land's own textures, bumped, with the footprints and the objects' shadows on them
	const glm::vec4 u_skyAndBump = {0.0f, 0.0f, 0.0f, 1.0f};
	terrainShader->SetTextureSampler("s0_blockTextures", 0, island.GetBlockTextures());
	terrainShader->SetTextureSampler("s9_landLuminosity", 9, GetLandLuminosity());
	terrainShader->SetTextureSampler("s10_landColour", 10, GetLandColour());
	terrainShader->SetTextureSampler("s2_smallBump", 2, *smallBump);
	SetObjectShadowUniforms(*terrainShader);
	terrainShader->SetTextureSampler("s7_landLight", 7, GetLandLightTexture());
	// Nor any moving shadow, on no block
	SetGroundShadowUniforms(*terrainShader, false);
	const auto noGroundShadow = glm::vec4(0.0f);
	terrainShader->SetUniformValue("u_groundShadowBlock", &noGroundShadow);
	// The temple's map of the land has no haze
	const auto noHaze = glm::vec4(0.0f);
	terrainShader->SetUniformValue("u_haze", &noHaze);
	terrainShader->SetUniformValue("u_hazeColour", &noHaze);
	terrainShader->SetUniformValue("u_skyAndBump", &u_skyAndBump);
	terrainShader->SetUniformValue("u_islandExtent", &islandExtent);
	const auto landTexels = LandTexelsUniform(island.GetBlockTextureSide());
	terrainShader->SetUniformValue("u_landTexels", &landTexels);
	// The map shows no vortex's hole
	const glm::vec4 noVortex {0.0f, 0.0f, 0.0f, -1.0f};
	terrainShader->SetUniformValue("u_vortexGround", &noVortex);
	for (size_t i = 0; const auto& block : island.GetBlocks())
	{
		const auto index = i++;
		// The map shows every block in full. A block of open sea only has no triangles and the map does not draw it.
		const auto* triangles = block.GetTriangles(land_detail::Mesh::Full, 0, true);
		if (triangles == nullptr)
		{
			continue;
		}
		const glm::vec4 mapPositionAndSize = glm::vec4(block.GetMapPosition(), 160.0f, 160.0f);
		terrainShader->SetUniformValue("u_blockPositionAndSize", &mapPositionAndSize);
		const glm::vec4 u_block {static_cast<float>(index), 0.0f, 0.0f, 0.0f};
		terrainShader->SetUniformValue("u_block", &u_block);
		block.BindVertices();
		triangles->Bind(triangles->GetCount());
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		// The textures stay bound from one block to the next
		terrainShader->Submit(viewId, 0,
		                      BGFX_DISCARD_INSTANCE_DATA | BGFX_DISCARD_INDEX_BUFFER | BGFX_DISCARD_TRANSFORM |
		                          BGFX_DISCARD_VERTEX_STREAMS | BGFX_DISCARD_STATE);
	}
	_shaderManager->DiscardBindings();
	// At the low detail levels the map has half the texels a side: 4 a block, each the texel of the full map at twice its
	// column and twice its row
	_templeMapHalved = Locator::config::has_value() && Locator::config::value().detail.lowResTextures;
	if (_templeMapHalved)
	{
		HalveTempleMap();
	}
}

void Renderer::HalveTempleMap() const
{
	constexpr uint16_t k_FullSize = 256;
	constexpr uint16_t k_HalfSize = k_FullSize / 2;
	if (!_templeMapHalfFrameBuffer)
	{
		_templeMapHalfFrameBuffer =
		    std::make_unique<FrameBuffer>("Temple Map Half", k_HalfSize, k_HalfSize, TextureFormat::RGBA8, std::nullopt,
		                                  static_cast<uint8_t>(1), Wrapping::ClampEdge);
	}
	const auto viewId = static_cast<bgfx::ViewId>(RenderPass::TempleMapHalf);
	_templeMapHalfFrameBuffer->Bind(RenderPass::TempleMapHalf);
	bgfx::setViewRect(viewId, 0, 0, k_HalfSize, k_HalfSize);
	constexpr auto k_Side = static_cast<float>(k_HalfSize);
	const auto projection =
	    bgfx::getCaps()->originBottomLeft ? glm::ortho(0.0f, k_Side, 0.0f, k_Side) : glm::ortho(0.0f, k_Side, k_Side, 0.0f);
	const glm::mat4 identity(1.0f);
	bgfx::setViewTransform(viewId, glm::value_ptr(identity), glm::value_ptr(projection));
	bgfx::touch(viewId);

	struct Vertex
	{
		glm::vec3 position;
		glm::vec2 uv;
		uint32_t colour;
	};
	static const auto k_Layout = [] {
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
		    .end();
		return layout;
	}();
	if (bgfx::getAvailTransientVertexBuffer(6, k_Layout) < 6)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, 6, k_Layout);
	// The centre of each texel of the half map reads the centre of the full map's texel at twice its column and row: the
	// texture coordinates move back by half a texel of the full map
	constexpr float k_Back = 0.5f / static_cast<float>(k_FullSize);
	constexpr uint32_t k_White = 0xFFFFFFFF;
	const std::array<Vertex, 4> corners {{
	    {{0.0f, 0.0f, 0.0f}, {-k_Back, -k_Back}, k_White},
	    {{k_Side, 0.0f, 0.0f}, {1.0f - k_Back, -k_Back}, k_White},
	    {{k_Side, k_Side, 0.0f}, {1.0f - k_Back, 1.0f - k_Back}, k_White},
	    {{0.0f, k_Side, 0.0f}, {-k_Back, 1.0f - k_Back}, k_White},
	}};
	constexpr std::array<size_t, 6> k_Indices {0, 1, 2, 2, 3, 0};
	const auto vertices = std::span(reinterpret_cast<Vertex*>(buffer.data), 6);
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		vertices[i] = corners.at(k_Indices.at(i));
	}
	const auto* shader = _shaderManager->GetShader("Interface");
	if (const auto sampler = shader->FindUniform("s_texture"); sampler.has_value())
	{
		shader->SetTextureSampler(*sampler, 0, _templeMapFrameBuffer->GetColorAttachment(),
		                          BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_POINT);
	}
	bgfx::setVertexBuffer(0, &buffer);
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
	shader->Submit(viewId);
	_shaderManager->DiscardBindings();
}

void Renderer::DrawTempleUnderside(const DrawSceneDesc& desc) const
{
	// The rooms' meshes don't quite meet everywhere: the sills of the main room's doorways stand a hundredth of a unit off
	// their doors' frames, and their edges have vertices of others part way along them. The game draws the sky
	// behind the temple, so it shows through the cracks there too, as specks of the outside. Looking down through them,
	// this shows black instead. The creature's room goes deepest, to 70.5 below the temple.
	constexpr float k_Depth = -75.0f;
	constexpr float k_Extent = 10000.0f;
	if (desc.viewId != RenderPass::Main || !_whiteTexture || !Locator::temple::has_value() ||
	    !Locator::temple::value().Active())
	{
		return;
	}
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	constexpr uint32_t k_Black = 0xFF000000;
	const std::array<OrientedTextVertex, 6> vertices {{
	    {{-k_Extent, k_Depth, -k_Extent}, {0.0f, 0.0f}, k_Black},
	    {{k_Extent, k_Depth, -k_Extent}, {1.0f, 0.0f}, k_Black},
	    {{k_Extent, k_Depth, k_Extent}, {1.0f, 1.0f}, k_Black},
	    {{-k_Extent, k_Depth, -k_Extent}, {0.0f, 0.0f}, k_Black},
	    {{k_Extent, k_Depth, k_Extent}, {1.0f, 1.0f}, k_Black},
	    {{-k_Extent, k_Depth, k_Extent}, {0.0f, 1.0f}, k_Black},
	}};
	const auto count = static_cast<uint32_t>(vertices.size());
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, vertices.data(), count * sizeof(OrientedTextVertex));

	const auto* shader = _shaderManager->GetShader("Text3D");
	const auto model = glm::translate(glm::mat4(1.0f), Locator::temple::value().GetPosition());
	bgfx::setTransform(glm::value_ptr(model));
	bgfx::setVertexBuffer(0, &buffer);
	shader->SetTextureSampler("s_texture", 0, *_whiteTexture);
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER |
	               BGFX_STATE_MSAA);
	shader->Submit(static_cast<bgfx::ViewId>(desc.viewId));
}

void Renderer::DrawTempleMap(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || !_templeMapFrameBuffer || !Locator::temple::has_value() ||
	    !Locator::temple::value().Active())
	{
		return;
	}
	const auto& temple = Locator::temple::value();
	// A visit frames the map. A warm-up draws a triangle of no size in its place.
	const auto vertices = _warmingUp && temple.GetMap().empty() ? std::span<const OrientedTextVertex>(k_WarmUpTriangle)
	                                                            : std::span<const OrientedTextVertex>(temple.GetMap());
	if (vertices.empty())
	{
		return;
	}
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
	    .end();
	const auto count = static_cast<uint32_t>(vertices.size());
	if (bgfx::getAvailTransientVertexBuffer(count, layout) < count)
	{
		return;
	}
	bgfx::TransientVertexBuffer buffer;
	bgfx::allocTransientVertexBuffer(&buffer, count, layout);
	std::memcpy(buffer.data, vertices.data(), count * sizeof(OrientedTextVertex));

	// The land's texture by the vertices' colours, blended by their alpha
	const auto* shader = _shaderManager->GetShader("Text3D");
	const auto model = glm::translate(glm::mat4(1.0f), temple.GetPosition());
	bgfx::setTransform(glm::value_ptr(model));
	bgfx::setVertexBuffer(0, &buffer);
	// At the low detail levels the room shows the map at half size
	const auto& map = _templeMapHalved && _templeMapHalfFrameBuffer ? *_templeMapHalfFrameBuffer : *_templeMapFrameBuffer;
	shader->SetTextureSampler("s_texture", 0, map.GetColorAttachment());
	bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_GREATER |
	               BGFX_STATE_MSAA | BGFX_STATE_BLEND_ALPHA);
	shader->Submit(static_cast<bgfx::ViewId>(desc.viewId));
}

void Renderer::DrawTempleMapMarkers(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || !Locator::temple::has_value() || !Locator::temple::value().Active())
	{
		return;
	}
	const auto& temple = Locator::temple::value();
	// A visit puts the markers on the map. A warm-up draws one of each kind at no size in their place.
	const bool warmUp = _warmingUp && temple.GetMapMarkers().empty();
	const std::vector<TempleMapMarker> warmUpMarkers =
	    warmUp ? std::vector<TempleMapMarker> {{.kind = TempleMapMarkerKind::Temple},
	                                           {.kind = TempleMapMarkerKind::Creature},
	                                           {.kind = TempleMapMarkerKind::Challenge}}
	           : std::vector<TempleMapMarker> {};
	const auto& markers = warmUp ? warmUpMarkers : temple.GetMapMarkers();
	if (markers.empty())
	{
		return;
	}
	const auto origin = temple.GetPosition();
	const float size = warmUp ? 0.0f : 1.0f;

	// First a glow under each marker, the room's light glow drawn over everything: 1.3 across, a tenth above the marker
	const auto& textures = Locator::resources::value().GetTextures();
	const auto atmos = entt::hashed_string("raw/ATMOS");
	const auto atmosAlpha = entt::hashed_string("raw/ATMOSA");
	if (textures.Contains(atmos.value()) && textures.Contains(atmosAlpha.value()))
	{
		const auto* spriteShader = _shaderManager->GetShader("Sprite");
		constexpr float k_GlowSize = 1.3f;
		constexpr glm::vec4 k_GlowColour {0x61 / 255.0f, 0x6E / 255.0f, 0x7C / 255.0f, 1.0f};
		// The glow is frame 22 of the atmosphere texture's 8 by 8 frames, facing the camera and adding to what is behind
		const glm::vec4 u_sampleRect {1.0f / 8.0f, 1.0f / 8.0f, 6.0f / 8.0f, 2.0f / 8.0f};
		const glm::vec4 u_spriteParams {1.0f, 1.0f, 1.0f, 0.0f};
		for (const auto& marker : markers)
		{
			const auto glow = marker.glow.value_or(marker.position + glm::vec3(0.0f, 0.1f, 0.0f));
			const auto model = glm::scale(glm::translate(glm::mat4(1.0f), origin + glow), glm::vec3(k_GlowSize * size));
			bgfx::setTransform(glm::value_ptr(model));
			spriteShader->SetUniformValue("u_sampleRect", glm::value_ptr(u_sampleRect));
			spriteShader->SetUniformValue("u_spriteParams", glm::value_ptr(u_spriteParams));
			spriteShader->SetUniformValue("u_tint", glm::value_ptr(k_GlowColour));
			spriteShader->SetTextureSampler("s_diffuse", 0, *textures.Handle(atmos));
			spriteShader->SetTextureSampler("s_alpha", 1, *textures.Handle(atmosAlpha));
			_plane->GetVertexBuffer().Bind();
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
			               BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE));
			spriteShader->Submit(static_cast<bgfx::ViewId>(desc.viewId));
		}
	}

	// Then the markers, a twentieth of their size, turning, in their colours
	constexpr std::array<entt::id_type, 3> k_Icons = {
	    entt::hashed_string("temple/icons/I_citadel_on_map"),
	    entt::hashed_string("temple/icons/I_creature_on_map"),
	    entt::hashed_string("temple/icons/I_challenge_on_map"),
	};
	constexpr float k_MarkerScale = 0.05f;
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto turn = glm::rotate(glm::mat4(1.0f), temple.GetMapMarkerTurn(), glm::vec3(0.0f, 1.0f, 0.0f));
	for (const auto& marker : markers)
	{
		const auto icon = k_Icons.at(static_cast<size_t>(marker.kind));
		if (!meshes.Contains(icon))
		{
			continue;
		}
		// The game puts the markers in the temple's light
		const auto model = glm::translate(glm::mat4(1.0f), origin + marker.position) * turn *
		                   glm::scale(glm::mat4(1.0f), glm::vec3(k_MarkerScale * size));
		L3DMeshSubmitDesc submitDesc = {};
		submitDesc.viewId = desc.viewId;
		submitDesc.program = _shaderManager->GetShader("Object");
		submitDesc.state = k_BgfxDefaultStateInvertedZ;
		submitDesc.modelMatrices = &model;
		submitDesc.matrixCount = 1;
		submitDesc.tint = glm::vec4(temple.GetLight().Colour(glm::vec3(marker.colour) / 255.0f), 1.0f);
		submitDesc.lightAdd = temple.GetLight().add;
		DrawMesh(*meshes.Handle(icon), submitDesc, std::numeric_limits<uint8_t>::max());
	}
}

void Renderer::DrawCaveTrophies(const DrawSceneDesc& desc) const
{
	if (desc.viewId != RenderPass::Main || !Locator::temple::has_value() || !Locator::temple::value().Active())
	{
		return;
	}
	const auto& trophies = Locator::temple::value().GetCaveTrophies();
	if (trophies.empty())
	{
		return;
	}
	// The game gives every material of the icons the one texture, with the alpha read from beside it
	if (!_iconsLoaded)
	{
		_iconsLoaded = true;
		constexpr uint16_t k_Size = 256;
		constexpr size_t k_Pixels = static_cast<size_t>(k_Size) * k_Size;
		auto& fileSystem = Locator::filesystem::value();
		const auto colourPath = fileSystem.GetPath<filesystem::Path::Textures>() / "icons.raw";
		const auto alphaPath = fileSystem.GetPath<filesystem::Path::Textures>() / "iconsa.raw";
		if (fileSystem.Exists(colourPath) && fileSystem.Exists(alphaPath))
		{
			const auto colour = fileSystem.ReadAll(colourPath);
			const auto alpha = fileSystem.ReadAll(alphaPath);
			if (colour.size() == k_Pixels * 3 && alpha.size() == k_Pixels)
			{
				const auto* memory = bgfx::alloc(static_cast<uint32_t>(k_Pixels * 4));
				for (size_t i = 0; i < k_Pixels; ++i)
				{
					memory->data[(i * 4) + 0] = colour[(i * 3) + 0];
					memory->data[(i * 4) + 1] = colour[(i * 3) + 1];
					memory->data[(i * 4) + 2] = colour[(i * 3) + 2];
					memory->data[(i * 4) + 3] = alpha[i];
				}
				_iconsTexture =
				    fromBgfx(bgfx::createTexture2D(k_Size, k_Size, false, 1, bgfx::TextureFormat::RGBA8, 0, memory));
				bgfx::setName(toBgfx(*_iconsTexture), "Icons");
			}
		}
	}
	if (!_iconsTexture)
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto& textures = Locator::resources::value().GetTextures();
	const auto envmap = entt::hashed_string("raw/envmap").value();
	const auto* environment = textures.Contains(envmap) ? &*textures.Handle(envmap) : nullptr;
	for (const auto& trophy : trophies)
	{
		if (!meshes.Contains(trophy.mesh))
		{
			continue;
		}
		const auto colour = glm::vec3((trophy.colour >> 16) & 0xFF, (trophy.colour >> 8) & 0xFF, trophy.colour & 0xFF);
		L3DMeshSubmitDesc submitDesc = {};
		submitDesc.viewId = desc.viewId;
		// The creature's room draws the belts and the medals past wood environment-mapped, adding the game's first
		// environment map, envmap.raw
		const bool environmentMapped = trophy.environmentMapped && environment != nullptr;
		submitDesc.program = _shaderManager->GetShader(environmentMapped ? "ObjectEnvironment" : "Object");
		submitDesc.environment = environmentMapped ? environment : nullptr;
		submitDesc.state = k_BgfxDefaultStateInvertedZ;
		submitDesc.modelMatrices = &trophy.model;
		// Their materials are two sided
		submitDesc.useMaterialCulling = true;
		submitDesc.matrixCount = 1;
		submitDesc.skinTexture = &*_iconsTexture;
		submitDesc.useMaterialBlending = true;
		// The colour multiplies what lights them: the medals come out the mid grey of the game's at 0x80. The game
		// puts it in the temple's light.
		const auto& light = Locator::temple::value().GetLight();
		// Each is fogged where it stands, its colour in the temple's light scaled in whole bytes before the light
		// shades it, and the fog's colour added takes the place of the light's
		submitDesc.objectLook =
		    L3DMeshSubmitDesc::ObjectLook {.colour = glm::floor(light.Colour(colour / 255.0f) * 255.0f + 0.5f), .alpha = 1.0f};
		submitDesc.templeFog = true;
		DrawMesh(*meshes.Handle(trophy.mesh), submitDesc, std::numeric_limits<uint8_t>::max());
	}
}
