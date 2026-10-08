/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandBlock.h"

#include <cassert>

#include <algorithm>
#include <ranges>

#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LNDFile.h>
#include <bgfx/bgfx.h>

#include "Dynamics/LandBlockBulletMeshInterface.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/Mesh.h"
#include "Graphics/VertexBuffer.h"

using namespace openblack;
using namespace openblack::graphics;

LandVertex::LandVertex(const glm::vec3& position, const glm::vec3& weight, const std::array<uint32_t, 6>& mat,
                       const glm::uvec3& blend, uint8_t lightLevel, glm::u8vec3 cellColour, float alpha,
                       const glm::vec3& normal)
    : position {position}
    , weight {weight}
    , firstMaterialID {static_cast<uint8_t>(mat[0]), static_cast<uint8_t>(mat[1]), static_cast<uint8_t>(mat[2]), 0}
    , secondMaterialID {static_cast<uint8_t>(mat[3]), static_cast<uint8_t>(mat[4]), static_cast<uint8_t>(mat[5]), 0}
    , materialBlendCoefficient {blend, 0u}
    , lightLevel {lightLevel, cellColour}
    , waterAlpha {alpha}
    , normal {normal}
{
}

void LandBlock::BuildMesh(LandIslandInterface& island)
{
	if (_mesh != nullptr)
	{
		_mesh.reset();
	}

	VertexDecl decl;
	decl.reserve(8);
	decl.emplace_back(VertexAttrib::Attribute::Position, static_cast<uint8_t>(3), VertexAttrib::Type::Float);
	// weight
	decl.emplace_back(VertexAttrib::Attribute::TexCoord1, static_cast<uint8_t>(3), VertexAttrib::Type::Float);
	// first material id, w: once-per-block bits
	decl.emplace_back(VertexAttrib::Attribute::Color1, static_cast<uint8_t>(4), VertexAttrib::Type::Uint8);
	// second material id, w: once-per-block bits
	decl.emplace_back(VertexAttrib::Attribute::Color2, static_cast<uint8_t>(4), VertexAttrib::Type::Uint8);
	// material blend coefficient
	decl.emplace_back(VertexAttrib::Attribute::TexCoord2, static_cast<uint8_t>(3), VertexAttrib::Type::Uint8, true);
	// light level, align to 4 bytes
	decl.emplace_back(VertexAttrib::Attribute::Color0, static_cast<uint8_t>(4), VertexAttrib::Type::Uint8, true);
	// shore fade: 0 at altitude 1 or less (no small bump, no dynamic shadow), 1 above
	decl.emplace_back(VertexAttrib::Attribute::Color3, static_cast<uint8_t>(1), VertexAttrib::Type::Float, true);
	// smooth normal
	decl.emplace_back(VertexAttrib::Attribute::Normal, static_cast<uint8_t>(3), VertexAttrib::Type::Float);

	// reserve 16*16 quads of 2 tris with 3 verts = 1536
	const bgfx::Memory* verticesMem = bgfx::alloc(sizeof(LandVertex) * k_VertexCount);
	auto vertices = std::span(reinterpret_cast<LandVertex*>(verticesMem->data), k_VertexCount);

	BuildVertexList(vertices, island);

	// the physics shape keeps every cell (it copies the positions)
	_dynamicsMeshInterface = std::make_unique<dynamics::LandBlockBulletMeshInterface>(vertices);

	// Open sea cells (bit 0x02 of the flags byte) are not drawn: the original emits no triangles for them, so there
	// only the sea shows and no Z is written.
	// Here their six vertices collapse to a point (zero area, nothing rasterised).
	constexpr uint8_t k_NotDrawnFlag = 0x02;
	constexpr size_t k_VerticesPerCell = 6;
	for (size_t cell = 0; cell < static_cast<size_t>(k_Resolution.x) * k_Resolution.y; ++cell)
	{
		const size_t x = cell / k_Resolution.y;
		const size_t z = cell % k_Resolution.y;
		if ((_block->cells.at(x * 17 + z).flags & k_NotDrawnFlag) != 0)
		{
			const auto collapsed = vertices[cell * k_VerticesPerCell].position;
			for (size_t v = 0; v < k_VerticesPerCell; ++v)
			{
				vertices[cell * k_VerticesPerCell + v].position = collapsed;
			}
		}
	}

	auto vertexBuffer = std::make_unique<VertexBuffer>("LandBlock", verticesMem, decl);
	_mesh = std::make_unique<Mesh>(std::move(vertexBuffer));

	_physicsMesh = std::make_unique<btBvhTriangleMeshShape>(_dynamicsMeshInterface.get(), true);
	_rigidBody = std::make_unique<btRigidBody>(0.0f, nullptr, _physicsMesh.get());
	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(btVector3(_block->mapX, 0, _block->mapZ));
	_rigidBody->setWorldTransform(transform);
	_rigidBody->setContactStiffnessAndDamping(300, 10);
	_rigidBody->setUserIndex(-1);
}

void LandBlock::BuildVertexList(std::span<LandVertex> vertices, LandIslandInterface& island)
{
	auto countries = island.GetCountries();

	// auto neighbourBlockR = island.GetBlock(glm::u8vec2(_block->blockX + 1, _block->blockZ));
	// auto neighbourBlockUp = island.GetBlock(glm::u8vec2(_block->blockX, _block->blockZ + 1));

	// we'll loop through each cell, 16x16
	// (the array is 17x17 but the 17th block is questionable data)

	const auto blockOffset = static_cast<glm::u16vec2>(GetBlockPosition() * 16);

	uint16_t index = 0;
	for (int x = 0; x < 16; x++)
	{
		for (int z = 0; z < 16; z++)
		{
			enum class Corner
			{
				TopLeft,
				TopRight,
				BottomLeft,
				BottomRight,

				_COUNT
			};

			std::array<glm::u16vec2, static_cast<size_t>(Corner::_COUNT)> offsets;
			offsets[static_cast<size_t>(Corner::TopLeft)] = glm::u16vec2(x, z);
			offsets[static_cast<size_t>(Corner::TopRight)] = glm::u16vec2(x + 1, z);
			offsets[static_cast<size_t>(Corner::BottomLeft)] = glm::u16vec2(x, z + 1);
			offsets[static_cast<size_t>(Corner::BottomRight)] = glm::u16vec2(x + 1, z + 1);

			std::array<const lnd::LNDCell*, static_cast<size_t>(Corner::_COUNT)> cells;
			// construct positions from cell altitudes
			std::array<glm::vec3, static_cast<size_t>(Corner::_COUNT)> pos;
			std::array<glm::vec3, static_cast<size_t>(Corner::_COUNT)> normals;
			std::array<const lnd::LNDMapMaterial*, static_cast<size_t>(Corner::_COUNT)> materials;
			for (auto [position, normal, cell, material, offset] : std::views::zip(pos, normals, cells, materials, offsets))
			{
				const auto coordinates = blockOffset + offset;
				cell = &island.GetCell(coordinates);
				// Sea flattening of the mesh: every vertex of altitude 3 or less at y = 0 (GetDrawnHeightAt; the game's
				// altitude query flattens only next to a base corner of 4 or less)
				const auto cellAltitude = island.GetCellAltitude(*cell);
				position =
				    glm::vec3(offset.x * LandIslandInterface::k_CellSize,
				              cellAltitude <= 3 ? 0.0f : static_cast<float>(cellAltitude) * LandIslandInterface::k_HeightUnit,
				              offset.y * LandIslandInterface::k_CellSize);

				// central differences of the neighbouring altitudes (clamped at the map edge)
				const auto height = [&island](int cx, int cz) {
					const int last = island.GetCellsPerSide() - 1;
					const auto clamped = glm::u16vec2(std::clamp(cx, 0, last), std::clamp(cz, 0, last));
					return static_cast<float>(island.GetCellAltitude(island.GetCell(clamped))) *
					       LandIslandInterface::k_HeightUnit;
				};
				const int cx = coordinates.x;
				const int cz = coordinates.y;
				normal =
				    glm::normalize(glm::vec3(height(cx - 1, cz) - height(cx + 1, cz), 2.0f * LandIslandInterface::k_CellSize,
				                             height(cx, cz - 1) - height(cx, cz + 1)));

				const auto& country = countries.at(cell->properties.country);
				const auto noise = island.GetNoise(blockOffset + offset);

				// The material of the block texture's texel at this corner (with the cone weights (255, 0, 0, 0)
				// there): min((255 altitude >> 8) + noise, 255). The texture itself (BlockTexture.h)
				// takes it per texel; these per-vertex materials are only drawn when an island has no block texture.
				// BWLandEditor maps above altitude 255 get the top entry too.
				const auto altitude = static_cast<int32_t>(island.GetCellAltitude(*cell));
				material = &country.materials.at(
				    static_cast<size_t>(std::min((255 * altitude >> 8) + static_cast<int32_t>(noise), 255)));
			}

			// The coast's transparency is the per-texel coast alpha (CoastAlpha.h), not a per-vertex value. Per vertex
			// only the shore fade: at altitude 1 or less the specular alpha is 0, which the small bump pass copies to
			// its vertex alpha (vs_terrain) and uses to skip the triangles whose three vertices have it, and the
			// dynamic shadows give such vertices colour 0, a fade.
			auto shoreFade = [&island](const lnd::LNDCell& cell) { return island.GetCellAltitude(cell) > 1 ? 1.0f : 0.0f; };
			auto makeVert = [&shoreFade, &pos, &normals, &cells, &materials](Corner corner, const glm::vec3& weight,
			                                                                 const std::array<Corner, 3>& m) -> LandVertex {
				const std::array<uint32_t, 6> mat = {
				    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				    materials[static_cast<size_t>(m[0])]->indices[0],
				    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				    materials[static_cast<size_t>(m[1])]->indices[0],
				    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				    materials[static_cast<size_t>(m[2])]->indices[0],

				    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				    materials[static_cast<size_t>(m[0])]->indices[1],
				    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				    materials[static_cast<size_t>(m[1])]->indices[1],
				    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				    materials[static_cast<size_t>(m[2])]->indices[1],
				};
				const glm::u32vec3 blend = {
				    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				    materials[static_cast<size_t>(m[0])]->coefficient,
				    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				    materials[static_cast<size_t>(m[1])]->coefficient,
				    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				    materials[static_cast<size_t>(m[2])]->coefficient,
				};
				// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				const auto& cell = *cells[static_cast<size_t>(corner)];
				// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				// vertex specular = the cell's first dword read as a D3DCOLOR: r, g, b bytes -> blue, green, red
				return {pos[static_cast<size_t>(corner)], weight, mat, blend, cell.luminosity,
				        glm::u8vec3(cell.b, cell.g, cell.r), shoreFade(cell),
				        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				        normals[static_cast<size_t>(corner)]};
			};

			auto makeTriangle = [&makeVert, &vertices, &index](const std::array<Corner, 3>& corners, bool forward) {
				if (forward)
				{
					vertices[index++] = makeVert(corners[0], glm::vec3(1, 0, 0), corners);
					vertices[index++] = makeVert(corners[1], glm::vec3(0, 1, 0), corners);
					vertices[index++] = makeVert(corners[2], glm::vec3(0, 0, 1), corners);
				}
				else
				{
					vertices[index++] = makeVert(corners[2], glm::vec3(0, 0, 1), corners);
					vertices[index++] = makeVert(corners[1], glm::vec3(0, 1, 0), corners);
					vertices[index++] = makeVert(corners[0], glm::vec3(1, 0, 0), corners);
				}
			};

			// cell splitting
			// winding order = clockwise
			if (!cells[static_cast<size_t>(Corner::TopLeft)]->properties.split)
			{
				makeTriangle({Corner::TopLeft, Corner::TopRight, Corner::BottomRight}, true);    //  ┐
				makeTriangle({Corner::TopLeft, Corner::BottomLeft, Corner::BottomRight}, false); // └
			}
			else
			{
				makeTriangle({Corner::BottomLeft, Corner::TopLeft, Corner::TopRight}, true);      // ┌
				makeTriangle({Corner::BottomLeft, Corner::BottomRight, Corner::TopRight}, false); //  ┘
			}
		}
	}
}

bool LandBlock::ReadsCorners(glm::ivec2 blockPosition, const CornerBox& corners)
{
	// the corners 0 to 16 of the block and the normals' neighbours, one before and one after
	const glm::ivec2 first = blockPosition * static_cast<int>(k_Resolution.x) - 1;
	const glm::ivec2 last = blockPosition * static_cast<int>(k_Resolution.x) + static_cast<int>(k_Resolution.x) + 1;
	return corners.min.x <= last.x && corners.max.x >= first.x && corners.min.y <= last.y && corners.max.y >= first.y;
}

const lnd::LNDCell* LandBlock::GetCells() const
{
	assert(_block);
	return _block ? _block->cells.data() : nullptr;
}

glm::ivec2 LandBlock::GetBlockPosition() const
{
	assert(_block);
	return {_block ? _block->blockX : -1, _block ? _block->blockZ : -1};
}

glm::vec2 LandBlock::GetMapPosition() const
{
	assert(_block);
	return {_block->mapX, _block->mapZ};
}

void LandBlock::SetLndBlock(const lnd::LNDBlock& block)
{
	_block = std::make_unique<lnd::LNDBlock>(block);
}
