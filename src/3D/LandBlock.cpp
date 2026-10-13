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

#include <LNDFile.h>
#include <bgfx/bgfx.h>

#include "Graphics/IndexBuffer.h"
#include "Graphics/VertexBuffer.h"

using namespace openblack;
using namespace openblack::graphics;

LandVertex openblack::MakeLandVertex(glm::vec2 cornerPosition, uint8_t altitude)
{
	const auto height = static_cast<float>(altitude);
	return {
	    .position = glm::vec3(cornerPosition.x, LandIslandInterface::GetDrawnAltitude(altitude), cornerPosition.y),
	    .altitude = height,
	    .halfAltitude = height,
	    .quarterAltitude = height,
	};
}

LandBlock::LandBlock() = default;
LandBlock::LandBlock(LandBlock&&) noexcept = default;
LandBlock& LandBlock::operator=(LandBlock&&) noexcept = default;
LandBlock::~LandBlock() = default;

void LandBlock::BuildMesh(LandIslandInterface& island, std::span<LandVertex> vertices)
{
	assert(vertices.size() == k_VertexCount);
	BuildVertexList(vertices, island);
}

const IndexBuffer& LandBlock::GetTriangles(land_detail::Mesh mesh, uint8_t coarserSides, bool dropOpenSea) const
{
	const size_t set = (static_cast<size_t>(mesh) * 16 + (coarserSides & 0xF)) * 2 + (dropOpenSea ? 1 : 0);
	auto& triangles = _triangles.at(set);
	if (triangles == nullptr)
	{
		assert(_block);
		assert(_triangleSets != nullptr);
		std::array<land_detail::CellFlags, land_detail::k_CornerCount> flags;
		// Bit 1 of a cell's flags marks open sea, where the land isn't drawn
		constexpr uint8_t k_OpenSeaFlag = 0x02;
		std::ranges::transform(_block->cells, flags.begin(), [](const lnd::LNDCell& cell) {
			return land_detail::CellFlags {.split = cell.properties.split != 0, .openSea = (cell.flags & k_OpenSeaFlag) != 0};
		});
		triangles = &_triangleSets->Get(
		    land_detail::BlockTriangles(mesh, coarserSides, dropOpenSea, flags), [](std::span<const uint16_t> indices) {
			    // bgfx keeps a copy: the list is gone before it reads it
			    const auto* memory = bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint16_t)));
			    return std::make_unique<IndexBuffer>("LandBlockTriangles", memory, IndexBuffer::Type::Uint16);
		    });
	}
	return *triangles;
}

void LandBlock::SetVertices(const VertexBuffer& buffer, uint32_t firstVertex)
{
	_vertices = &buffer;
	_firstVertex = firstVertex;
}

void LandBlock::SetTriangleSets(SharedIndexLists<IndexBuffer>& sets)
{
	_triangleSets = &sets;
	_triangles.fill(nullptr);
}

void LandBlock::BindVertices() const
{
	if (_vertices != nullptr)
	{
		_vertices->Bind(_firstVertex, k_VertexCount);
	}
}

void LandBlock::BuildVertexList(std::span<LandVertex> vertices, LandIslandInterface& island)
{
	// The land is coloured by its block texture (see block_texture) and lit by its luminosity of the frame: its vertices
	// carry only where they are and their altitudes
	const auto blockOffset = static_cast<glm::u16vec2>(GetBlockPosition() * 16);
	std::array<uint8_t, land_detail::k_CornerCount> altitudes {};
	for (size_t x = 0; x < land_detail::k_CornersPerSide; ++x)
	{
		for (size_t z = 0; z < land_detail::k_CornersPerSide; ++z)
		{
			altitudes.at(x * land_detail::k_CornersPerSide + z) = island.GetCell(blockOffset + glm::u16vec2(x, z)).altitude;
		}
	}
	for (size_t x = 0; x < land_detail::k_CornersPerSide; ++x)
	{
		for (size_t z = 0; z < land_detail::k_CornersPerSide; ++z)
		{
			const size_t corner = x * land_detail::k_CornersPerSide + z;
			auto vertex = MakeLandVertex(glm::vec2(x, z) * LandIslandInterface::k_CellSize, altitudes.at(corner));
			const auto targets = land_detail::TargetsAt(altitudes, x, z);
			vertex.halfAltitude = static_cast<float>(targets.half);
			vertex.quarterAltitude = static_cast<float>(targets.quarter);
			vertices[corner] = vertex;
		}
	}
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
	_triangles.fill(nullptr);
}

void LandBlock::SetCellAltitude(size_t index, uint8_t altitude)
{
	assert(_block);
	if (_block && index < _block->cells.size())
	{
		_block->cells.at(index).altitude = altitude;
	}
}
