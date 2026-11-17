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

#include <cstdint>

#include <algorithm>
#include <array>
#include <iterator>
#include <memory>
#include <optional>
#include <vector>

#include <LNDFile.h>
#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/OceanInterface.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/LakeSurface.h"
#include "Graphics/Mesh.h"
#include "Graphics/RendererDetail.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/VertexBuffer.h"
#include "Locator.h"
#include "Renderer.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::graphics::detail;

namespace
{
/// A corner outside a lake counts as land at least one altitude above the lake's level
constexpr float k_ShoreReach = 0.67f;

/// The water surface of the lakes of the land, as a mesh that gives its data again after a restore of the device
std::unique_ptr<Mesh> MakeLakeMesh(const LandIslandInterface& island, const land_lakes::LakeMap& lakes)
{
	const auto cellAt = [&island](int x, int z) -> const lnd::LNDCell* {
		if (x < 0 || z < 0 || x >= LandIslandInterface::k_MapCellsPerSide || z >= LandIslandInterface::k_MapCellsPerSide)
		{
			return nullptr;
		}
		return island.FindCell({static_cast<uint16_t>(x), static_cast<uint16_t>(z)});
	};
	const auto surface = lake_surface::Build(
	    lakes,
	    [&cellAt](int x, int z) -> std::optional<float> {
		    const auto* cell = cellAt(x, z);
		    return cell != nullptr ? std::optional<float>(LandIslandInterface::GetDrawnAltitude(cell->altitude)) : std::nullopt;
	    },
	    [&cellAt](int x, int z) {
		    const auto* cell = cellAt(x, z);
		    return cell != nullptr && cell->properties.split != 0;
	    },
	    k_ShoreReach);
	if (surface.indices.empty())
	{
		return nullptr;
	}

	VertexDecl decl;
	decl.reserve(2);
	decl.emplace_back(VertexAttrib::Attribute::Position, static_cast<uint8_t>(3), VertexAttrib::Type::Float);
	decl.emplace_back(VertexAttrib::Attribute::TexCoord0, static_cast<uint8_t>(2), VertexAttrib::Type::Float);
	const auto bytes = [](const auto& values) {
		const auto* first = reinterpret_cast<const uint8_t*>(values.data());
		return std::vector<uint8_t>(first, first + (values.size() * sizeof(values[0])));
	};
	// The device takes where each vertex is, the height that its water fades by and the level of its lake
	std::vector<std::array<float, 5>> drawn;
	drawn.reserve(surface.vertices.size());
	std::ranges::transform(surface.vertices, std::back_inserter(drawn), [](const lake_surface::Vertex& vertex) {
		return std::array<float, 5> {vertex.x, vertex.y, vertex.z, vertex.fade, vertex.level};
	});
	auto vertices = bytes(drawn);
	auto indices = bytes(surface.indices);
	const auto* vertexMemory = bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size()));
	auto* vertexBuffer = new VertexBuffer("Lakes", vertexMemory, decl);
	// The buffers take copies of the bytes, because the device reads them after this function
	const auto* indexMemory = bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size()));
	auto* indexBuffer = new IndexBuffer("Lakes", indexMemory, IndexBuffer::Type::Uint32);
	auto mesh = std::make_unique<Mesh>(vertexBuffer, indexBuffer);
	mesh->KeepContentsForRestore(std::move(vertices), std::move(indices));
	return mesh;
}
} // namespace

void Renderer::CopyLakeSky() const
{
	if (!Locator::terrainSystem::has_value() || Locator::terrainSystem::value().GetLakeMap() == nullptr ||
	    !Locator::oceanSystem::has_value())
	{
		return;
	}
	const auto& from = Locator::oceanSystem::value().GetReflectionFramebuffer();
	glm::u16vec2 size {0};
	from.GetSize(size.x, size.y);
	if (!_lakeSky.has_value() || _lakeSkySize != size)
	{
		if (_lakeSky.has_value())
		{
			bgfx::destroy(toBgfx(*_lakeSky));
		}
		constexpr uint64_t k_Flags = BGFX_TEXTURE_BLIT_DST | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
		_lakeSky = fromBgfx(bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::RGBA8, k_Flags));
		bgfx::setName(toBgfx(*_lakeSky), "Lake Sky");
		_lakeSkySize = size;
	}
	// The view of the reflection's land comes after the view of its sky. Its copies go before its draws.
	bgfx::TextureRegion to;
	to.init(toBgfx(*_lakeSky));
	bgfx::TextureRegion sky;
	sky.init(toBgfx(from.GetColorAttachment().GetNativeHandle()));
	bgfx::blit(static_cast<bgfx::ViewId>(RenderPass::Reflection), to, sky);
}

void Renderer::ReleaseLakes() const
{
	_lakeMesh.reset();
	_lakeMeshDigest = 0;
	if (_lakeSky.has_value())
	{
		bgfx::destroy(toBgfx(*_lakeSky));
		_lakeSky.reset();
	}
}

void Renderer::SetLakeUniforms(const ShaderProgram& lakeShader) const
{
	lakeShader.SetUniformValue("u_seaParams", &_seaUniforms[0]);
	lakeShader.SetUniformValue("u_seaRows", &_seaUniforms[1]);
	lakeShader.SetUniformValue("u_seaMode", &_seaUniforms[2]);
	lakeShader.SetUniformValue("u_seaColour", &_seaUniforms[3]);
	lakeShader.SetUniformValue("u_seaCamera", &_seaUniforms[4]);
	lakeShader.SetUniformValue("u_seaDrift", &_seaUniforms[5]);
}

void Renderer::DrawPassLakes(const DrawSceneDesc& desc) const
{
	// Only the view itself has the lakes' water
	if (desc.viewId != RenderPass::Main || !desc.drawWater || !desc.drawIsland || _warmingUp ||
	    !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto& island = Locator::terrainSystem::value();
	const auto* lakes = island.GetLakeMap();
	if (lakes == nullptr)
	{
		_lakeMesh.reset();
		_lakeMeshDigest = 0;
		return;
	}
	if (_lakeMeshDigest != lakes->Digest())
	{
		_lakeMesh = MakeLakeMesh(island, *lakes);
		_lakeMeshDigest = lakes->Digest();
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Lakes: {} lakes, {} indices of water", lakes->GetLakes().size(),
		                   _lakeMesh ? _lakeMesh->GetIndexBuffer().GetCount() : 0u);
	}
	if (!_lakeMesh)
	{
		return;
	}
	const auto* lakeShader = _shaderManager->GetShader("Lake");
	const auto& ocean = Locator::oceanSystem::value();
	_lakeMesh->GetIndexBuffer().Bind(_lakeMesh->GetIndexBuffer().GetCount(), 0);
	_lakeMesh->GetVertexBuffer().Bind();
	// The water is tested against the depth of the land and writes no depth, as the sea. It is blended over the land by
	// the land's coast alpha under it, so the land in front of a lake hides it.
	constexpr auto k_LakeState = (k_BgfxDefaultStateInvertedZ & ~(BGFX_STATE_WRITE_Z | BGFX_STATE_CULL_MASK)) |
	                             BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
	bgfx::setState(k_LakeState);
	auto texture = Locator::resources::value().GetTextures().Handle(ocean.GetTexture());
	lakeShader->SetTextureSampler("s_diffuse", 0, *texture);
	// The lakes mirror the sky of the sea's reflection, without the land that the sea mirrors at its own level
	if (_lakeSky.has_value())
	{
		lakeShader->SetTextureSampler("s_reflection", 1, *_lakeSky);
	}
	else
	{
		lakeShader->SetTextureSampler("s_reflection", 1, ocean.GetReflectionFramebuffer().GetColorAttachment());
	}
	SetLakeUniforms(*lakeShader);
	// The view itself comes after the land's view, whose blocks the workers can submit late, so the land is all drawn
	// before the water
	lakeShader->Submit(static_cast<bgfx::ViewId>(desc.viewId));
}
