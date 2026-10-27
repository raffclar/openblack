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
#include <optional>
#include <span>
#include <string>

#include <glm/vec2.hpp>

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

/// The share of the influence a player has when the land is the first land of the story, or when nobody has any
constexpr float k_ShareOfNoInfluence = 0.01f;
/// The size a temple grows toward: twice its player's share of the influence. On the first land of the story the game
/// gives every player the share of no influence.
/// @param share The player's share of the influence, nothing when nobody has any
[[nodiscard]] float SizeTarget(int32_t landNumber, std::optional<float> share);

/// Whether the temple's mesh is to be blended again: its look changed, it was never blended, or its mesh is not loaded.
/// A loaded game keeps the look that the mesh was blended for, but the blended mesh itself is made again.
[[nodiscard]] bool NeedsBlend(std::optional<glm::vec2> blendedFor, glm::vec2 look, bool meshLoaded);

/// Whether the temple's mesh is to be blended again after a load. Each temple that the save keeps a look for gets a new
/// blend. The cache can keep a mesh that this computer blended for another world, such as the world before a network
/// game loads the server's world.
[[nodiscard]] bool BlendsAfterLoad(std::optional<glm::vec2> blendedFor);

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
