/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "UnderwaterView.h"

#include <cmath>
#include <cstddef>

#include <algorithm>
#include <array>
#include <random>
#include <span>
#include <utility>
#include <vector>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include "GraphicsHandleBgfx.h"

using namespace openblack::graphics;

namespace
{
struct MoteVertex
{
	/// Where the mote is in its box, 0 to 1 on each axis
	float x, y, z;
	/// The corner of its square, -1 or 1 on each axis
	float u, v;
};
} // namespace

UnderwaterView::UnderwaterView()
{
	bgfx::VertexLayout layout;
	layout.begin()
	    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
	    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
	    .end();
	constexpr std::array<glm::vec2, 4> k_Corners {{{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}}};
	const auto* vertexMemory = bgfx::alloc(static_cast<uint32_t>(k_Motes * k_Corners.size() * sizeof(MoteVertex)));
	const auto vertices = std::span(reinterpret_cast<MoteVertex*>(vertexMemory->data), k_Motes * k_Corners.size());
	const auto* indexMemory = bgfx::alloc(static_cast<uint32_t>(k_Motes * 6 * sizeof(uint32_t)));
	const auto indices = std::span(reinterpret_cast<uint32_t*>(indexMemory->data), k_Motes * 6);
	// The same motes every time
	std::mt19937 random(1998u);
	std::uniform_real_distribution<float> unit(0.0f, 1.0f);
	for (uint32_t i = 0; i < k_Motes; ++i)
	{
		const float x = unit(random);
		const float y = unit(random);
		const float z = unit(random);
		for (size_t c = 0; c < k_Corners.size(); ++c)
		{
			vertices[(i * k_Corners.size()) + c] = {x, y, z, k_Corners.at(c).x, k_Corners.at(c).y};
		}
		const auto first = static_cast<uint32_t>(i * k_Corners.size());
		constexpr std::array<uint32_t, 6> k_Triangles {0, 1, 2, 0, 2, 3};
		for (size_t k = 0; k < k_Triangles.size(); ++k)
		{
			indices[(i * 6) + k] = first + k_Triangles.at(k);
		}
	}
	_moteVertices = bgfx::createVertexBuffer(vertexMemory, layout);
	_moteIndices = bgfx::createIndexBuffer(indexMemory, BGFX_BUFFER_INDEX32);
	bgfx::setName(_moteVertices, "Underwater Motes");
}

UnderwaterView::~UnderwaterView()
{
	if (bgfx::isValid(_farVertices))
	{
		bgfx::destroy(_farVertices);
	}
	if (bgfx::isValid(_farIndices))
	{
		bgfx::destroy(_farIndices);
	}
	if (bgfx::isValid(_moteVertices))
	{
		bgfx::destroy(_moteVertices);
	}
	if (bgfx::isValid(_moteIndices))
	{
		bgfx::destroy(_moteIndices);
	}
}

const FrameBuffer& UnderwaterView::Target(glm::u16vec2 size)
{
	if (_target == nullptr || size != _targetSize)
	{
		_target.reset();
		_target = std::make_unique<FrameBuffer>("Underwater Scene", size.x, size.y, TextureFormat::RGBA8,
		                                        TextureFormat::Depth24Stencil8, 1, Wrapping::ClampEdge);
		_targetSize = size;
	}
	return *_target;
}

const Texture2D& UnderwaterView::Seabed(const seabed::Seabed& seabed, uint32_t revision)
{
	if (_seabed == nullptr || _seabedRevision != revision)
	{
		// One texel for each corner of the map, its depth in units; one texel of zero when there is no sea bed
		const auto size = glm::max(seabed.size, glm::ivec2(1));
		std::vector<float> texels(static_cast<size_t>(size.x) * static_cast<size_t>(size.y), 0.0f);
		std::copy_n(seabed.depth.begin(), std::min(seabed.depth.size(), texels.size()), texels.begin());
		_seabed = std::make_unique<Texture2D>("Underwater Seabed");
		_seabed->CreateWithinFrame(static_cast<uint16_t>(size.x), static_cast<uint16_t>(size.y), 1, TextureFormat::R32F,
		                           Wrapping::ClampEdge, Filter::Nearest,
		                           bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size() * sizeof(float))));
		_seabedRevision = revision;
	}
	return *_seabed;
}

void UnderwaterView::BindMotes() const
{
	bgfx::setVertexBuffer(0, _moteVertices);
	bgfx::setIndexBuffer(_moteIndices);
}

namespace
{
struct FarVertex
{
	glm::vec3 position;
	glm::vec3 normal;
};

/// The blocks along a side of the map, and the cells along a side of a block
constexpr int k_MapBlocks = 32;
constexpr int k_BlockCells = 16;
/// How far the skirts hang down
constexpr float k_SkirtDepth = 4.0f;
} // namespace

bool UnderwaterView::BindFarSeabed(const seabed::Seabed& seabed, uint32_t revision, std::span<const bool> present)
{
	if (_farRevision != revision)
	{
		_farRevision = revision;
		if (bgfx::isValid(_farVertices))
		{
			bgfx::destroy(_farVertices);
			_farVertices = BGFX_INVALID_HANDLE;
		}
		if (bgfx::isValid(_farIndices))
		{
			bgfx::destroy(_farIndices);
			_farIndices = BGFX_INVALID_HANDLE;
		}
		const auto corners = seabed.size;
		if (corners.x < k_MapBlocks * k_BlockCells + 1 || corners.y < k_MapBlocks * k_BlockCells + 1 ||
		    present.size() < static_cast<size_t>(k_MapBlocks * k_MapBlocks))
		{
			return false;
		}
		const auto depthAt = [&](int x, int z) {
			x = std::clamp(x, 0, corners.x - 1);
			z = std::clamp(z, 0, corners.y - 1);
			return seabed.depth[static_cast<size_t>(z) * static_cast<size_t>(corners.x) + static_cast<size_t>(x)];
		};
		const auto isPresent = [&](int x, int z) {
			return x >= 0 && z >= 0 && x < k_MapBlocks && z < k_MapBlocks &&
			       present[static_cast<size_t>(x) * k_MapBlocks + static_cast<size_t>(z)];
		};
		std::vector<FarVertex> vertices;
		std::vector<uint32_t> indices;
		const auto vertexAt = [&](int x, int z, float drop) {
			// The normal from the depths either side of the corner
			const float dx = depthAt(x + 1, z) - depthAt(x - 1, z);
			const float dz = depthAt(x, z + 1) - depthAt(x, z - 1);
			const auto normal = glm::normalize(glm::vec3(dx, 2.0f * seabed.spacing, dz));
			const glm::vec3 position {static_cast<float>(x) * seabed.spacing, -depthAt(x, z) - drop,
			                          static_cast<float>(z) * seabed.spacing};
			vertices.push_back({position, normal});
			return static_cast<uint32_t>(vertices.size() - 1);
		};
		for (int bx = 0; bx < k_MapBlocks; ++bx)
		{
			for (int bz = 0; bz < k_MapBlocks; ++bz)
			{
				if (isPresent(bx, bz))
				{
					continue;
				}
				bool nearBlock = false;
				for (int ox = -1; ox <= 1; ++ox)
				{
					for (int oz = -1; oz <= 1; ++oz)
					{
						nearBlock = nearBlock || isPresent(bx + ox, bz + oz);
					}
				}
				const int quads = nearBlock ? 16 : 4;
				const int step = k_BlockCells / quads;
				const int x0 = bx * k_BlockCells;
				const int z0 = bz * k_BlockCells;
				const auto first = static_cast<uint32_t>(vertices.size());
				for (int j = 0; j <= quads; ++j)
				{
					for (int i = 0; i <= quads; ++i)
					{
						vertexAt(x0 + i * step, z0 + j * step, 0.0f);
					}
				}
				const auto row = static_cast<uint32_t>(quads + 1);
				for (uint32_t j = 0; j < static_cast<uint32_t>(quads); ++j)
				{
					for (uint32_t i = 0; i < static_cast<uint32_t>(quads); ++i)
					{
						const auto a = first + j * row + i;
						indices.insert(indices.end(), {a, a + row, a + 1, a + 1, a + row, a + row + 1});
					}
				}
				// A skirt down each side of the tile
				const auto skirt = [&](auto cornerOf) {
					for (int k = 0; k < quads; ++k)
					{
						const auto [ax, az] = cornerOf(k);
						const auto [bx2, bz2] = cornerOf(k + 1);
						const auto top0 = vertexAt(ax, az, 0.0f);
						const auto top1 = vertexAt(bx2, bz2, 0.0f);
						const auto low0 = vertexAt(ax, az, k_SkirtDepth);
						const auto low1 = vertexAt(bx2, bz2, k_SkirtDepth);
						indices.insert(indices.end(), {top0, low0, top1, top1, low0, low1});
					}
				};
				skirt([&](int k) { return std::pair {x0 + k * step, z0}; });
				skirt([&](int k) { return std::pair {x0 + k * step, z0 + k_BlockCells}; });
				skirt([&](int k) { return std::pair {x0, z0 + k * step}; });
				skirt([&](int k) { return std::pair {x0 + k_BlockCells, z0 + k * step}; });
			}
		}
		if (indices.empty())
		{
			return false;
		}
		bgfx::VertexLayout layout;
		layout.begin()
		    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
		    .end();
		_farVertices = bgfx::createVertexBuffer(
		    bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(FarVertex))), layout);
		_farIndices = bgfx::createIndexBuffer(
		    bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint32_t))), BGFX_BUFFER_INDEX32);
		bgfx::setName(_farVertices, "Underwater Far Seabed");
	}
	if (!bgfx::isValid(_farVertices))
	{
		return false;
	}
	bgfx::setVertexBuffer(0, _farVertices);
	bgfx::setIndexBuffer(_farIndices);
	return true;
}
