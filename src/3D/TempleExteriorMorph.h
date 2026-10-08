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
#include <functional>
#include <span>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>

#include "3D/LandMorph.h"

namespace openblack::l3d
{
struct L3DVertex;
}

namespace openblack::TempleExteriorMorph
{

/// A temple's outside is three sizes, by its player's share of influence, of five stages each, from evil to good: the
/// meshes B_TEMPLE<size><stage>, all of the same shape. Its mesh is a blend of the four about where it is.
constexpr uint32_t k_Sizes = 3;
constexpr uint32_t k_Stages = 5;

/// The game moves each a step a turn toward where it is to be, and the rest of the way when that is near
constexpr float k_Step = 0.016f;
constexpr float k_Near = 0.001f;
[[nodiscard]] float Step(float current, float target);

/// Where the temple's alignment heads, from 0, evil, to just short of 1, good: its player's alignment, from -1 to 1,
/// taken to that range
[[nodiscard]] float AlignmentTarget(float playerAlignment);
/// Where the temple's size heads, from 0 to 1: twice its player's share of the influence. On the first land the share
/// is always a small one; elsewhere it is the player's influence power over the sum of every player's, that small
/// share again when nobody has any
[[nodiscard]] float SizeTarget(bool firstLand, float ownPower, float allPowers);

/// The game blends a temple's mesh again only once its look has moved more than this from the one last blended
constexpr float k_Changed = 0.03f;

/// Where a temple's outside is, where it heads, and what its mesh was last blended for. A new temple is neutral and
/// small, and its mesh is blended once as it is made, before it has moved at all
struct State
{
	float alignment {0.5f};
	float alignmentTarget {0.5f};
	float blendedAlignment {0.5f};
	float size {0.0f};
	float sizeTarget {0.0f};
	float blendedSize {0.0f};
	bool neverBlended {true};
};
/// One turn: the targets taken and each value a Step toward its own. True when either value is now more than
/// k_Changed from the one last blended, which is when the game blends the mesh again
[[nodiscard]] bool Turn(State& state, float alignmentTarget, float sizeTarget);
/// Whether a blend is due: the look moved far enough, or the mesh was never blended
[[nodiscard]] bool NeedsBlend(const State& state);
/// A blend done for the state's look
void Blended(State& state);

/// One of the meshes a temple's is blended from, and how much of it
struct Corner
{
	uint32_t size;
	uint32_t stage;
	float weight;
};
/// The meshes for a size and an alignment, each from 0 to 1, and their weights
[[nodiscard]] std::array<Corner, 4> Corners(float size, float alignment);
/// The name of a size's and stage's mesh
[[nodiscard]] std::string MeshName(uint32_t size, uint32_t stage);
/// A size's and stage's vertices, or null when that mesh is missing
using VerticesOf = std::function<const std::vector<l3d::L3DVertex>*(uint32_t size, uint32_t stage)>;
/// Each vertex the sum of the corners' vertices by their weights: position, texture coordinates and normal, the
/// normal not made unit again. False when a corner of any weight is missing or not of the blend's shape; `blended` is
/// then left part written
[[nodiscard]] bool BlendVertices(const std::array<Corner, 4>& corners, const VerticesOf& verticesOf,
                                 std::span<l3d::L3DVertex> blended);
/// Each blended vertex is laid on the land: lowered by how far the temple stands above the land under that vertex
/// (land_morph::BakeAgainstY with the temple's matrix)
void BakeToLand(const land_morph::Ground& ground, const glm::mat4& object, std::span<l3d::L3DVertex> vertices);

/// The two images of a temple's texture blended from evil to neutral, or neutral to good, and how far, from 0 to 255
enum class Look : uint8_t
{
	Evil,
	Neutral,
	Good,
};
struct TextureBlend
{
	Look from;
	Look to;
	uint8_t weight;
};
[[nodiscard]] TextureBlend TextureOf(float alignment);
/// The name of a look's image of one of the four sets of textures
[[nodiscard]] std::string ImageName(Look look, uint32_t set);
/// Each 16 bit texel, four bits a channel, blended from one image to another by weight from 0 to 255, keeping the first
/// image's alpha
void BlendTexels(std::span<const uint16_t> from, std::span<const uint16_t> to, uint8_t weight, std::span<uint16_t> blended);

} // namespace openblack::TempleExteriorMorph
