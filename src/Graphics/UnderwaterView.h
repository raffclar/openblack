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
#include "Graphics/Underwater.h"
#include "GraphicsHandle.h"
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
	/// The layer of the land's block textures for each block of the 32 by 32 map, plus 1, and 0 where there is no block.
	/// `layers` has one value for each block, x first. Made again when it changes. The water reads the original game's
	/// coast alpha from the block textures through it, so that the water shows only where the original sea shows.
	[[nodiscard]] const Texture2D& BlockLayers(std::span<const float> layers, uint32_t revision);
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
	/// The spray at the foot of the cliffs: k_SprayPuffs particles for each point along the foot of a cliff, made again
	/// when the sea bed changes. Binds them; false when there are no cliffs.
	bool BindSpray(std::span<const glm::vec4> cliffFoot, uint32_t revision);
	/// The puffs of spray there are at once at most
	[[nodiscard]] uint32_t SprayCount() const { return _sprayCount * k_SprayPuffs; }
	static constexpr uint32_t k_SprayPuffs = 128;
	/// The air that the waves drive under the water at the cliffs: k_Bubbles particles for each point along the foot
	/// of a cliff, on the same instances as the spray. Binds them; false when there are no cliffs.
	bool BindBubbles(std::span<const glm::vec4> cliffFoot, uint32_t revision);
	static constexpr uint32_t k_Bubbles = 96;
	/// The waterspouts this frame: xy the foot, z the strength, w the waterspout's own number. Binds them with
	/// k_SpoutPuffs particles each; false when there are none.
	bool BindWaterspouts(std::span<const glm::vec4> spouts);
	static constexpr uint32_t k_SpoutPuffs = 900;
	/// The spray's noise volume (see underwater::SprayNoiseVolume), made the first time it is asked for
	[[nodiscard]] TextureHandle SprayNoise();

	/// The cube of the far scene round the eye, which the water reflects (see underwater::k_EnvironmentSize): the
	/// target that draws into one face of its colour and its depth. The cube is made the first time a face is asked for.
	[[nodiscard]] bgfx::FrameBufferHandle EnvironmentFace(uint8_t face);
	/// The target that draws the sky alone into one face of a second cube. A ray that meets no scene takes the sky from
	/// it, because the scene near the middle of the first cube can hide the sky there.
	[[nodiscard]] bgfx::FrameBufferHandle EnvironmentSkyFace(uint8_t face) const;
	[[nodiscard]] TextureHandle EnvironmentColour() const;
	[[nodiscard]] TextureHandle EnvironmentDepth() const;
	[[nodiscard]] TextureHandle EnvironmentSky() const;
	[[nodiscard]] bgfx::TextureHandle EnvironmentColourTexture() const { return _environmentColour; }
	[[nodiscard]] bgfx::TextureHandle EnvironmentSkyTexture() const { return _environmentSky; }
	/// The face of the cube to draw this frame. Each call moves on to the next face.
	[[nodiscard]] uint8_t NextEnvironmentFace();
	/// Whether every face that the water reflects has been drawn once
	[[nodiscard]] bool EnvironmentReady() const { return _environmentDrawn >= underwater::k_EnvironmentFaces - 1; }

private:
	std::unique_ptr<FrameBuffer> _target;
	glm::u16vec2 _targetSize {0, 0};
	std::unique_ptr<Texture2D> _seabed;
	std::optional<uint32_t> _seabedRevision;
	std::unique_ptr<Texture2D> _blockLayers;
	std::optional<uint32_t> _blockLayersRevision;
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
	bgfx::TextureHandle _sprayNoise = BGFX_INVALID_HANDLE;
	bgfx::VertexBufferHandle _bubbleMesh = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle _bubbleIndices = BGFX_INVALID_HANDLE;
	bgfx::VertexBufferHandle _spoutMesh = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle _spoutIndices = BGFX_INVALID_HANDLE;
	bgfx::IndexBufferHandle _moteIndices = BGFX_INVALID_HANDLE;
	bgfx::TextureHandle _environmentColour = BGFX_INVALID_HANDLE;
	bgfx::TextureHandle _environmentDepth = BGFX_INVALID_HANDLE;
	bgfx::TextureHandle _environmentSky = BGFX_INVALID_HANDLE;
	bgfx::TextureHandle _environmentSkyDepth = BGFX_INVALID_HANDLE;
	std::array<bgfx::FrameBufferHandle, underwater::k_EnvironmentFaces> _environmentFaces {};
	std::array<bgfx::FrameBufferHandle, underwater::k_EnvironmentFaces> _environmentSkyFaces {};
	uint8_t _environmentFace {0};
	uint32_t _environmentDrawn {0};
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
	/// x: the wet rock of the cliffs, y: the spray's mist and haze; 1 on, 0 off
	glm::vec4 flags3 {1.0f, 1.0f, 0.0f, 0.0f};
	/// The sea's currents: xy the steady flow, z the swirls, w the gusts
	glm::vec4 current {0.0f, 0.0f, 1.0f, 0.4f};
	/// The waves of the sea (see underwater::WaveTable)
	std::array<glm::vec4, 14> waveTable {};
	/// Where the things that float are, w 1 for each
	std::array<glm::vec4, 8> floaters {};
	/// The far sea: x 1 while the waves are filtered for each pixel, y 1 while the long waves show, z the angle one
	/// pixel takes up on the screen (radians), w 1 while the water reflects the cube of the far scene
	glm::vec4 sea {0.0f};
	/// xyz: where the middle of the cube of the far scene is
	glm::vec4 environment {0.0f};
	/// The long waves of the open sea (see underwater::FarWaveTable)
	std::array<glm::vec4, underwater::k_FarWaveCount> farWaves {};
	/// Whether the motes show
	bool motes {true};
	/// Whether the eye is under the surface, or near enough to it that the surface can cut the view
	bool eyeNearOrUnder {false};
};

} // namespace openblack::graphics
