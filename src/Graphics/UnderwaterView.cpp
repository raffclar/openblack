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
#include <numbers>
#include <random>
#include <span>
#include <utility>
#include <vector>

#include <bx/uint32_t.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "3D/SeabedTiles.h"
#include "GraphicsHandleBgfx.h"
#include "Underwater.h"

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
	_environmentFaces.fill(BGFX_INVALID_HANDLE);
	_environmentSkyFaces.fill(BGFX_INVALID_HANDLE);
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
	for (auto* faces : {&_environmentFaces, &_environmentSkyFaces})
	{
		for (auto& face : *faces)
		{
			if (bgfx::isValid(face))
			{
				bgfx::destroy(face);
			}
		}
	}
	for (auto* texture : {&_environmentColour, &_environmentDepth, &_environmentSky, &_environmentSkyDepth})
	{
		if (bgfx::isValid(*texture))
		{
			bgfx::destroy(*texture);
		}
	}
	if (bgfx::isValid(_sprayNoise))
	{
		bgfx::destroy(_sprayNoise);
	}
	for (auto* buffer : {&_sprayMesh, &_sprayInstances, &_bubbleMesh, &_spoutMesh})
	{
		if (bgfx::isValid(*buffer))
		{
			bgfx::destroy(*buffer);
		}
	}
	for (auto* indices : {&_sprayIndices, &_bubbleIndices, &_spoutIndices})
	{
		if (bgfx::isValid(*indices))
		{
			bgfx::destroy(*indices);
		}
	}
	for (auto& kind : _reef)
	{
		for (auto* buffer : {&kind.mesh, &kind.instances})
		{
			if (bgfx::isValid(*buffer))
			{
				bgfx::destroy(*buffer);
			}
		}
		if (bgfx::isValid(kind.indices))
		{
			bgfx::destroy(kind.indices);
		}
	}
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
		// The shore map: one texel for each corner of the map, at half precision. x the distance to the coast, yz the
		// direction towards it, shortened by up to a half at the foot of a cliff, w the height of the ground.
		const auto size = glm::max(seabed.size, glm::ivec2(1));
		const auto texelCount = static_cast<size_t>(size.x) * static_cast<size_t>(size.y);
		std::vector<uint16_t> texels(texelCount * 4, 0);
		for (size_t i = 0; i < seabed.shore.size() && i < texelCount; ++i)
		{
			const auto texel = underwater::ShoreTexel(seabed.shore[i], i < seabed.cliff.size() ? seabed.cliff[i] : 0.0f);
			texels[i * 4 + 0] = bx::halfFromFloat(texel.x);
			texels[i * 4 + 1] = bx::halfFromFloat(texel.y);
			texels[i * 4 + 2] = bx::halfFromFloat(texel.z);
			texels[i * 4 + 3] = bx::halfFromFloat(texel.w);
		}
		_seabed = std::make_unique<Texture2D>("Underwater Shore Map");
		_seabed->CreateWithinFrame(static_cast<uint16_t>(size.x), static_cast<uint16_t>(size.y), 1, TextureFormat::RGBA16F,
		                           Wrapping::ClampEdge, Filter::Linear,
		                           bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size() * sizeof(uint16_t))));
		_seabedRevision = revision;
	}
	return *_seabed;
}

const Texture2D& UnderwaterView::BlockLayers(std::span<const float> layers, uint32_t revision)
{
	constexpr uint16_t k_Blocks = 32;
	if (_blockLayers == nullptr || _blockLayersRevision != revision)
	{
		std::vector<float> texels(static_cast<size_t>(k_Blocks) * k_Blocks, 0.0f);
		std::copy_n(layers.begin(), std::min(layers.size(), texels.size()), texels.begin());
		_blockLayers = std::make_unique<Texture2D>("Underwater Block Layers");
		_blockLayers->CreateWithinFrame(k_Blocks, k_Blocks, 1, TextureFormat::R32F, Wrapping::ClampEdge, Filter::Nearest,
		                                bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size() * sizeof(float))));
		_blockLayersRevision = revision;
	}
	return *_blockLayers;
}

void UnderwaterView::BindMotes() const
{
	bgfx::setVertexBuffer(0, _moteVertices);
	bgfx::setIndexBuffer(_moteIndices);
}

namespace
{
/// A corner of the sea bed past the blocks: where it is (y how far it hangs down), and the two corners of the map whose
/// heights its height is between (the same corner twice for most)
struct FarVertex
{
	glm::vec3 position;
	glm::vec2 from;
	glm::vec2 to;
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
		const auto isPresent = [&](int x, int z) {
			return x >= 0 && z >= 0 && x < k_MapBlocks && z < k_MapBlocks &&
			       present[static_cast<size_t>(x) * k_MapBlocks + static_cast<size_t>(z)];
		};
		std::vector<FarVertex> vertices;
		std::vector<uint32_t> indices;
		// The vertex shader takes each corner's height from the shore map, as the land's blocks do
		const auto vertexAt = [&](glm::ivec2 corner, float drop, glm::ivec2 from, glm::ivec2 to) {
			const glm::vec3 position {static_cast<float>(corner.x) * seabed.spacing, drop,
			                          static_cast<float>(corner.y) * seabed.spacing};
			vertices.push_back({position, glm::vec2(from), glm::vec2(to)});
			return static_cast<uint32_t>(vertices.size() - 1);
		};
		const auto isFine = [&](int x, int z) {
			if (x < 0 || z < 0 || x >= k_MapBlocks || z >= k_MapBlocks || isPresent(x, z))
			{
				return false;
			}
			return seabed::FarTileIsFine(x, z, isPresent);
		};
		for (int bx = 0; bx < k_MapBlocks; ++bx)
		{
			for (int bz = 0; bz < k_MapBlocks; ++bz)
			{
				if (isPresent(bx, bz))
				{
					continue;
				}
				const bool fine = isFine(bx, bz);
				const int quads = fine ? k_BlockCells : k_BlockCells / seabed::k_CoarseStep;
				const int step = k_BlockCells / quads;
				const int x0 = bx * k_BlockCells;
				const int z0 = bz * k_BlockCells;
				// Which sides of a fine tile meet a coarse tile: x - 1, x + 1, z - 1, z + 1
				const std::array<bool, 4> coarseSide {
				    fine && !isPresent(bx - 1, bz) && !isFine(bx - 1, bz) && bx > 0,
				    fine && !isPresent(bx + 1, bz) && !isFine(bx + 1, bz) && bx + 1 < k_MapBlocks,
				    fine && !isPresent(bx, bz - 1) && !isFine(bx, bz - 1) && bz > 0,
				    fine && !isPresent(bx, bz + 1) && !isFine(bx, bz + 1) && bz + 1 < k_MapBlocks,
				};
				// A corner and the two corners of the map its height is between
				const auto placed = [&](int x, int z, float drop) {
					const glm::ivec2 corner {x, z};
					const auto [from, to] = seabed::FarCornerSpan(corner - glm::ivec2(x0, z0), coarseSide);
					return vertexAt(corner, drop, from + glm::ivec2(x0, z0), to + glm::ivec2(x0, z0));
				};
				const auto first = static_cast<uint32_t>(vertices.size());
				for (int j = 0; j <= quads; ++j)
				{
					for (int i = 0; i <= quads; ++i)
					{
						placed(x0 + i * step, z0 + j * step, 0.0f);
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
						const auto top0 = placed(ax, az, 0.0f);
						const auto top1 = placed(bx2, bz2, 0.0f);
						const auto low0 = placed(ax, az, k_SkirtDepth);
						const auto low1 = placed(bx2, bz2, k_SkirtDepth);
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
		    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord1, 2, bgfx::AttribType::Float)
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

namespace
{
struct ReefVertex
{
	glm::vec3 position;
	glm::vec2 uv;
};

/// One coral: three quads a unit wide and high that cross at their middle, a third of a turn apart
void CoralMesh(std::vector<ReefVertex>& vertices, std::vector<uint16_t>& indices)
{
	for (int q = 0; q < 3; ++q)
	{
		const float angle = static_cast<float>(q) * std::numbers::pi_v<float> / 3.0f;
		const glm::vec3 side {std::cos(angle) * 0.5f, 0.0f, std::sin(angle) * 0.5f};
		const auto first = static_cast<uint16_t>(vertices.size());
		vertices.push_back({-side, {0.0f, 1.0f}});
		vertices.push_back({side, {1.0f, 1.0f}});
		vertices.push_back({side + glm::vec3(0.0f, 1.0f, 0.0f), {1.0f, 0.0f}});
		vertices.push_back({-side + glm::vec3(0.0f, 1.0f, 0.0f), {0.0f, 0.0f}});
		indices.insert(indices.end(), {first, static_cast<uint16_t>(first + 1), static_cast<uint16_t>(first + 2), first,
		                               static_cast<uint16_t>(first + 2), static_cast<uint16_t>(first + 3)});
	}
}

/// One plant: a clump of five blades of different heights round its foot, each a strip in five pieces, narrower towards
/// its top
void PlantMesh(std::vector<ReefVertex>& vertices, std::vector<uint16_t>& indices)
{
	constexpr int k_Pieces = 5;
	constexpr std::array<glm::vec4, 5> k_Blades {{
	    {0.0f, 0.0f, 0.0f, 1.0f},
	    {0.18f, 0.05f, 1.3f, 0.75f},
	    {-0.12f, 0.15f, 2.4f, 0.85f},
	    {-0.05f, -0.2f, 3.6f, 0.6f},
	    {0.15f, -0.14f, 4.9f, 0.7f},
	}};
	for (const auto& blade : k_Blades)
	{
		const glm::vec3 foot {blade.x, 0.0f, blade.y};
		const glm::vec3 side {std::cos(blade.z), 0.0f, std::sin(blade.z)};
		const auto first = static_cast<uint16_t>(vertices.size());
		for (int i = 0; i <= k_Pieces; ++i)
		{
			const float h = static_cast<float>(i) / static_cast<float>(k_Pieces);
			const float half = glm::mix(0.045f, 0.008f, h);
			const glm::vec3 up {0.0f, h * blade.w, 0.0f};
			vertices.push_back({foot - side * half + up, {0.0f, h}});
			vertices.push_back({foot + side * half + up, {1.0f, h}});
		}
		for (int i = 0; i < k_Pieces; ++i)
		{
			const auto a = static_cast<uint16_t>(first + i * 2);
			indices.insert(indices.end(),
			               {a, static_cast<uint16_t>(a + 1), static_cast<uint16_t>(a + 2), static_cast<uint16_t>(a + 1),
			                static_cast<uint16_t>(a + 3), static_cast<uint16_t>(a + 2)});
		}
	}
}
} // namespace

bool UnderwaterView::BindReef(std::span<const seabed::ReefPlant> reef, uint32_t revision, seabed::ReefPlant::Kind kind)
{
	if (_reefRevision != revision)
	{
		_reefRevision = revision;
		bgfx::VertexLayout meshLayout;
		meshLayout.begin()
		    .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
		    .end();
		bgfx::VertexLayout instanceLayout;
		instanceLayout.begin()
		    .add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)
		    .add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)
		    .end();
		for (size_t k = 0; k < _reef.size(); ++k)
		{
			auto& made = _reef.at(k);
			if (!bgfx::isValid(made.mesh))
			{
				std::vector<ReefVertex> vertices;
				std::vector<uint16_t> indices;
				if (k == 0)
				{
					CoralMesh(vertices, indices);
				}
				else
				{
					PlantMesh(vertices, indices);
				}
				made.mesh = bgfx::createVertexBuffer(
				    bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(ReefVertex))), meshLayout);
				made.indices = bgfx::createIndexBuffer(
				    bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint16_t))));
				made.triangles = static_cast<uint32_t>(indices.size() / 3);
			}
			if (bgfx::isValid(made.instances))
			{
				bgfx::destroy(made.instances);
				made.instances = BGFX_INVALID_HANDLE;
			}
			// Two vectors an instance: the foot and the size, then the angle, the cell or kind, the colour and the kind
			std::vector<glm::vec4> data;
			const auto wanted = static_cast<seabed::ReefPlant::Kind>(k);
			for (const auto& plant : reef)
			{
				if (plant.kind == wanted)
				{
					// The foot a little into the sea bed, so that no gap shows under it
					data.emplace_back(plant.position - glm::vec3(0.0f, 0.15f, 0.0f), plant.scale);
					data.emplace_back(plant.yaw, plant.variant, plant.tint, static_cast<float>(k));
				}
			}
			made.count = static_cast<uint32_t>(data.size() / 2);
			if (made.count > 0)
			{
				made.instances = bgfx::createVertexBuffer(
				    bgfx::copy(data.data(), static_cast<uint32_t>(data.size() * sizeof(glm::vec4))), instanceLayout);
			}
		}
	}
	const auto& made = _reef.at(static_cast<size_t>(kind));
	if (made.count == 0 || !bgfx::isValid(made.instances))
	{
		return false;
	}
	bgfx::setVertexBuffer(0, made.mesh);
	bgfx::setIndexBuffer(made.indices);
	bgfx::setInstanceDataBuffer(made.instances, 0, made.count);
	return true;
}

uint32_t UnderwaterView::ReefTriangles(seabed::ReefPlant::Kind kind) const
{
	return _reef.at(static_cast<size_t>(kind)).triangles;
}

uint32_t UnderwaterView::ReefCount(seabed::ReefPlant::Kind kind) const
{
	return _reef.at(static_cast<size_t>(kind)).count;
}

bool UnderwaterView::BindSpray(std::span<const glm::vec4> cliffFoot, uint32_t revision)
{
	if (!bgfx::isValid(_sprayMesh))
	{
		// A square for each particle, each corner with the number of its particle
		std::vector<glm::vec3> vertices;
		std::vector<uint16_t> indices;
		constexpr std::array<glm::vec2, 4> k_Corners {{{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}}};
		for (uint32_t puff = 0; puff < k_SprayPuffs; ++puff)
		{
			const auto first = static_cast<uint16_t>(vertices.size());
			for (const auto& corner : k_Corners)
			{
				vertices.emplace_back(corner, static_cast<float>(puff));
			}
			indices.insert(indices.end(), {first, static_cast<uint16_t>(first + 1), static_cast<uint16_t>(first + 2), first,
			                               static_cast<uint16_t>(first + 2), static_cast<uint16_t>(first + 3)});
		}
		bgfx::VertexLayout layout;
		layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
		_sprayMesh = bgfx::createVertexBuffer(
		    bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(glm::vec3))), layout);
		_sprayIndices =
		    bgfx::createIndexBuffer(bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint16_t))));
	}
	if (_sprayRevision != revision)
	{
		_sprayRevision = revision;
		if (bgfx::isValid(_sprayInstances))
		{
			bgfx::destroy(_sprayInstances);
			_sprayInstances = BGFX_INVALID_HANDLE;
		}
		_sprayCount = static_cast<uint32_t>(cliffFoot.size());
		if (_sprayCount > 0)
		{
			bgfx::VertexLayout layout;
			layout.begin().add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float).end();
			_sprayInstances = bgfx::createVertexBuffer(
			    bgfx::copy(cliffFoot.data(), static_cast<uint32_t>(cliffFoot.size() * sizeof(glm::vec4))), layout);
		}
	}
	if (_sprayCount == 0)
	{
		return false;
	}
	bgfx::setVertexBuffer(0, _sprayMesh);
	bgfx::setIndexBuffer(_sprayIndices);
	bgfx::setInstanceDataBuffer(_sprayInstances, 0, _sprayCount);
	return true;
}

TextureHandle UnderwaterView::SprayNoise()
{
	if (!bgfx::isValid(_sprayNoise))
	{
		const auto texels = underwater::SprayNoiseVolume();
		constexpr auto k_Side = underwater::k_SprayNoiseSide;
		_sprayNoise = bgfx::createTexture3D(k_Side, k_Side, k_Side, false, bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_NONE,
		                                    bgfx::copy(texels.data(), static_cast<uint32_t>(texels.size())));
		bgfx::setName(_sprayNoise, "Underwater Spray Noise");
	}
	return fromBgfx(_sprayNoise);
}

bool UnderwaterView::BindBubbles(std::span<const glm::vec4> cliffFoot, uint32_t revision)
{
	// The cliffs' instances are the spray's: bind those, then put the bubbles' squares in place of the spray's
	if (!BindSpray(cliffFoot, revision))
	{
		return false;
	}
	if (!bgfx::isValid(_bubbleMesh))
	{
		std::vector<glm::vec3> vertices;
		std::vector<uint16_t> indices;
		constexpr std::array<glm::vec2, 4> k_Corners {{{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}}};
		for (uint32_t bubble = 0; bubble < k_Bubbles; ++bubble)
		{
			const auto first = static_cast<uint16_t>(vertices.size());
			for (const auto& corner : k_Corners)
			{
				vertices.emplace_back(corner, static_cast<float>(bubble));
			}
			indices.insert(indices.end(), {first, static_cast<uint16_t>(first + 1), static_cast<uint16_t>(first + 2), first,
			                               static_cast<uint16_t>(first + 2), static_cast<uint16_t>(first + 3)});
		}
		bgfx::VertexLayout layout;
		layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
		_bubbleMesh = bgfx::createVertexBuffer(
		    bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(glm::vec3))), layout);
		_bubbleIndices =
		    bgfx::createIndexBuffer(bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint16_t))));
	}
	bgfx::setVertexBuffer(0, _bubbleMesh);
	bgfx::setIndexBuffer(_bubbleIndices);
	return true;
}

bool UnderwaterView::BindWaterspouts(std::span<const glm::vec4> spouts)
{
	const auto count = static_cast<uint32_t>(spouts.size());
	if (count == 0 || bgfx::getAvailInstanceDataBuffer(count, sizeof(glm::vec4)) < count)
	{
		return false;
	}
	if (!bgfx::isValid(_spoutMesh))
	{
		std::vector<glm::vec3> vertices;
		std::vector<uint16_t> indices;
		constexpr std::array<glm::vec2, 4> k_Corners {{{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}}};
		for (uint32_t puff = 0; puff < k_SpoutPuffs; ++puff)
		{
			const auto first = static_cast<uint16_t>(vertices.size());
			for (const auto& corner : k_Corners)
			{
				vertices.emplace_back(corner, static_cast<float>(puff));
			}
			indices.insert(indices.end(), {first, static_cast<uint16_t>(first + 1), static_cast<uint16_t>(first + 2), first,
			                               static_cast<uint16_t>(first + 2), static_cast<uint16_t>(first + 3)});
		}
		bgfx::VertexLayout layout;
		layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
		_spoutMesh = bgfx::createVertexBuffer(
		    bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(glm::vec3))), layout);
		_spoutIndices =
		    bgfx::createIndexBuffer(bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint16_t))));
	}
	bgfx::InstanceDataBuffer instances;
	bgfx::allocInstanceDataBuffer(&instances, count, sizeof(glm::vec4));
	std::copy_n(reinterpret_cast<const uint8_t*>(spouts.data()), count * sizeof(glm::vec4), instances.data);
	bgfx::setVertexBuffer(0, _spoutMesh);
	bgfx::setIndexBuffer(_spoutIndices);
	bgfx::setInstanceDataBuffer(&instances);
	return true;
}

bgfx::FrameBufferHandle UnderwaterView::EnvironmentFace(uint8_t face)
{
	if (!bgfx::isValid(_environmentColour))
	{
		constexpr auto k_Size = underwater::k_EnvironmentSize;
		constexpr uint64_t k_Sampled = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_W_CLAMP;
		// The view of the scene copies the face of the sky into this cube before it draws the scene over it
		_environmentColour =
		    bgfx::createTextureCube(k_Size, false, 1, bgfx::TextureFormat::RGBA8, k_Sampled | BGFX_TEXTURE_BLIT_DST);
		bgfx::setName(_environmentColour, "Underwater Environment");
		_environmentSky = bgfx::createTextureCube(k_Size, false, 1, bgfx::TextureFormat::RGBA8, k_Sampled);
		bgfx::setName(_environmentSky, "Underwater Environment Sky");
		_environmentSkyDepth = bgfx::createTextureCube(k_Size, false, 1, bgfx::TextureFormat::D32F, BGFX_TEXTURE_RT_WRITE_ONLY);
		// The depth is read too: it gives how far away each texel's scene is, for the reflection's parallax
		_environmentDepth = bgfx::createTextureCube(k_Size, false, 1, bgfx::TextureFormat::D32F,
		                                            k_Sampled | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT);
		bgfx::setName(_environmentDepth, "Underwater Environment Depth");
		for (uint8_t i = 0; i < underwater::k_EnvironmentFaces; ++i)
		{
			std::array<bgfx::Attachment, 2> attachments {};
			attachments.at(0).init(_environmentColour, bgfx::Access::Write, i, 1, 0, BGFX_RESOLVE_NONE);
			attachments.at(1).init(_environmentDepth, bgfx::Access::Write, i, 1, 0, BGFX_RESOLVE_NONE);
			_environmentFaces.at(i) = bgfx::createFrameBuffer(static_cast<uint8_t>(attachments.size()), attachments.data());
			std::array<bgfx::Attachment, 2> sky {};
			sky.at(0).init(_environmentSky, bgfx::Access::Write, i, 1, 0, BGFX_RESOLVE_NONE);
			sky.at(1).init(_environmentSkyDepth, bgfx::Access::Write, i, 1, 0, BGFX_RESOLVE_NONE);
			_environmentSkyFaces.at(i) = bgfx::createFrameBuffer(static_cast<uint8_t>(sky.size()), sky.data());
		}
	}
	++_environmentDrawn;
	return _environmentFaces.at(face % underwater::k_EnvironmentFaces);
}

TextureHandle UnderwaterView::EnvironmentColour() const
{
	return fromBgfx(_environmentColour);
}

TextureHandle UnderwaterView::EnvironmentDepth() const
{
	return fromBgfx(_environmentDepth);
}

TextureHandle UnderwaterView::EnvironmentSky() const
{
	return fromBgfx(_environmentSky);
}

bgfx::FrameBufferHandle UnderwaterView::EnvironmentSkyFace(uint8_t face) const
{
	return _environmentSkyFaces.at(face % underwater::k_EnvironmentFaces);
}

uint8_t UnderwaterView::NextEnvironmentFace()
{
	const uint8_t face = _environmentFace;
	_environmentFace = underwater::NextEnvironmentFace(face);
	return face;
}
