/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the GPU's part of the experiment's rivers and taps

#include "UnderwaterStreamView.h"

#include <array>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Graphics/UnderwaterTap.h"

using namespace openblack::graphics;

namespace
{
template <typename Handle>
void Destroy(Handle& handle)
{
	if (bgfx::isValid(handle))
	{
		bgfx::destroy(handle);
		handle = BGFX_INVALID_HANDLE;
	}
}

/// Makes the squares of `count` particles, each corner with the number of its particle
void MakeSquares(uint32_t count, bgfx::VertexBufferHandle& vertexBuffer, bgfx::IndexBufferHandle& indexBuffer)
{
	std::vector<glm::vec3> vertices;
	std::vector<uint16_t> indices;
	constexpr std::array<glm::vec2, 4> k_Corners {{{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}}};
	for (uint32_t particle = 0; particle < count; ++particle)
	{
		const auto first = static_cast<uint16_t>(vertices.size());
		for (const auto& corner : k_Corners)
		{
			vertices.emplace_back(corner, static_cast<float>(particle));
		}
		indices.insert(indices.end(), {first, static_cast<uint16_t>(first + 1), static_cast<uint16_t>(first + 2), first,
		                               static_cast<uint16_t>(first + 2), static_cast<uint16_t>(first + 3)});
	}
	bgfx::VertexLayout layout;
	layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
	vertexBuffer = bgfx::createVertexBuffer(
	    bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(glm::vec3))), layout);
	indexBuffer = bgfx::createIndexBuffer(bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint16_t))));
}
} // namespace

UnderwaterStreamView::~UnderwaterStreamView()
{
	Destroy(_tapVertices);
	Destroy(_tapIndices);
	Destroy(_quadVertices);
	Destroy(_quadIndices);
}

const Texture2D* UnderwaterStreamView::FlowMap(const underwater::streams::FlowMap& map, uint32_t revision)
{
	if (map.texels.empty())
	{
		_flowMap.reset();
		return nullptr;
	}
	if (_flowMap == nullptr || _flowRevision != revision)
	{
		const auto texels = underwater::streams::PackFlowMap(map);
		_flowMap = std::make_unique<Texture2D>("Underwater Stream Flow");
		_flowMap->CreateWithinFrame(static_cast<uint16_t>(map.size.x), static_cast<uint16_t>(map.size.y), 1,
		                            TextureFormat::RGBA16F, Wrapping::ClampEdge, Filter::Linear,
		                            bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size() * sizeof(uint16_t))));
		_flowRevision = revision;
	}
	return _flowMap.get();
}

void UnderwaterStreamView::BindTapMesh()
{
	if (!bgfx::isValid(_tapVertices))
	{
		MakeSquares(underwater::tap::k_Particles, _tapVertices, _tapIndices);
	}
	bgfx::setVertexBuffer(0, _tapVertices);
	bgfx::setIndexBuffer(_tapIndices);
}

void UnderwaterStreamView::BindQuad()
{
	if (!bgfx::isValid(_quadVertices))
	{
		MakeSquares(1, _quadVertices, _quadIndices);
	}
	bgfx::setVertexBuffer(0, _quadVertices);
	bgfx::setIndexBuffer(_quadIndices);
}
