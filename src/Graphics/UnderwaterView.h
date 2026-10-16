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

#include <array>
#include <memory>
#include <optional>
#include <span>

#include <bgfx/bgfx.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "3D/Seabed.h"
#include "FrameBuffer.h"
#include "Texture2D.h"

namespace openblack::graphics
{

/// EXPERIMENT (not for merge): what the renderer keeps for the view under the sea: the target the scene is drawn
/// into, the sea bed's texture and the motes in the water
class UnderwaterView
{
public:
	/// The motes in the water
	static constexpr uint32_t k_Motes = 5000;

	UnderwaterView();
	~UnderwaterView();
	UnderwaterView(const UnderwaterView&) = delete;
	UnderwaterView& operator=(const UnderwaterView&) = delete;

	/// The target the scene is drawn into, made again when the window changes size
	[[nodiscard]] const FrameBuffer& Target(glm::u16vec2 size);
	/// The sea bed's depth at each cell corner, made again when the sea bed changes
	[[nodiscard]] const Texture2D& Seabed(const seabed::Seabed& seabed, uint32_t revision);
	/// Binds the motes' squares to draw them
	void BindMotes() const;
	/// The sea bed past the land's blocks, out to the map's edge, made again when the sea bed changes: a tile of 160
	/// units for each square of the map with no block, 4 quads a side, or 16 next to a block so that it meets the
	/// block's corners. Each tile has a skirt down its sides, so no gap shows between tiles. `present` is whether each
	/// block of the 32 by 32 map has one, x first. Binds it, and gives false when there is none.
	bool BindFarSeabed(const seabed::Seabed& seabed, uint32_t revision, std::span<const bool> present);
	/// The reef's instances, made again when the sea bed changes; then binds the coral's crossed quads (or the plants'
	/// crossed strips) and their instances. False when there are none of the kind.
	bool BindReef(std::span<const seabed::ReefPlant> reef, uint32_t revision, seabed::ReefPlant::Kind kind);
	/// How many triangles one coral and one plant have, and how many of each the reef has
	[[nodiscard]] uint32_t ReefTriangles(seabed::ReefPlant::Kind kind) const;
	[[nodiscard]] uint32_t ReefCount(seabed::ReefPlant::Kind kind) const;
	/// The spray at the foot of the cliffs: sixteen puffs for each point along a cliff's foot, made again when the sea
	/// bed changes. Binds them; false when there are no cliffs.
	bool BindSpray(std::span<const glm::vec4> cliffFoot, uint32_t revision);
	/// The puffs of spray there are at once at most
	[[nodiscard]] uint32_t SprayCount() const { return _sprayCount * k_SprayPuffs; }
	static constexpr uint32_t k_SprayPuffs = 16;

private:
	std::unique_ptr<FrameBuffer> _target;
	glm::u16vec2 _targetSize {0, 0};
	std::unique_ptr<Texture2D> _seabed;
	std::optional<uint32_t> _seabedRevision;
	bgfx::VertexBufferHandle _moteVertices = BGFX_INVALID_HANDLE;
	bgfx::VertexBufferHandle _farVertices = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle _farIndices = BGFX_INVALID_HANDLE;
	std::optional<uint32_t> _farRevision;
	/// The reef: a mesh and a buffer of instances for the coral, then the same for the plants
	struct ReefKind
	{
		bgfx::VertexBufferHandle mesh = BGFX_INVALID_HANDLE;
		bgfx::IndexBufferHandle indices = BGFX_INVALID_HANDLE;
		uint32_t triangles {0};
		bgfx::VertexBufferHandle instances = BGFX_INVALID_HANDLE;
		uint32_t count {0};
	};
	std::array<ReefKind, 2> _reef {};
	std::optional<uint32_t> _reefRevision;
	bgfx::VertexBufferHandle _sprayMesh = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle _sprayIndices = BGFX_INVALID_HANDLE;
	bgfx::VertexBufferHandle _sprayInstances = BGFX_INVALID_HANDLE;
	uint32_t _sprayCount {0};
	std::optional<uint32_t> _sprayRevision;
	bgfx::IndexBufferHandle _moteIndices = BGFX_INVALID_HANDLE;
};

/// EXPERIMENT (not for merge): the uniforms of the view under the sea for one frame (see underwater.sh)
struct UnderwaterUniforms
{
	glm::vec4 params {0.0f};
	glm::vec4 light {0.0f, 1.0f, 0.0f, 0.0f};
	glm::vec4 lightColour {0.0f};
	glm::vec4 scatter {0.0f};
	glm::vec4 eye {0.0f};
	glm::vec4 sky {0.0f};
	/// x: the reflections, y: the foam, z: the refraction, w: the shafts of light; 1 on, 0 off
	glm::vec4 flags {1.0f};
	/// xyz: the direction towards the light above the surface; w: the caustics, 1 on, 0 off
	glm::vec4 above {0.0f, 1.0f, 0.0f, 1.0f};
	/// x: how high the waves are, y: how stormy, z: the wind's angle, w: the run of the waves up the beach
	glm::vec4 waves {1.0f, 0.0f, 0.0f, 0.25f};
	/// x: the storm's whitecaps, 1 on, 0 off
	glm::vec4 flags2 {1.0f, 0.0f, 0.0f, 0.0f};
	/// The sea's currents: xy the steady flow, z the swirls, w the gusts
	glm::vec4 current {0.0f, 0.0f, 1.0f, 0.4f};
	/// Where the things that float are, w 1 for each
	std::array<glm::vec4, 8> floaters {};
	/// Whether the motes show
	bool motes {true};
	/// Whether the eye is under the surface, or near enough to it that the surface can cut the view
	bool eyeNearOrUnder {false};
};

} // namespace openblack::graphics
