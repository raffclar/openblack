/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>

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

/// What a blend of a temple's mesh comes from: the temple's look (its size, then its alignment) and the set of
/// textures of its player
struct Blend
{
	glm::vec2 look {0.0f};
	uint32_t textureSet {0};

	bool operator==(const Blend&) const = default;
};

/// Whether the temple's mesh is to be blended again after a load. Each temple that the save keeps a look for gets a new
/// blend. The cache can keep a mesh that this computer blended for another world, such as the world before a network
/// game loads the server's world. A mesh in the cache that is the blend of the same look and textures stays.
/// @param inCache The blend of the mesh that the cache keeps for the temple, nothing when the cache keeps none
[[nodiscard]] bool BlendsAfterLoad(std::optional<glm::vec2> blendedFor, std::optional<Blend> inCache, Blend wanted);

/// The temples whose blends change in one call get one mesh for each blend, which all the temples of that blend share.
/// Each temple that changes has a group, which is its blend. The groups have numbers in the order of their first
/// temple. Each mesh that a temple has now has a number. A temple that has no mesh now has no number.
struct MeshChange
{
	size_t group;
	std::optional<size_t> mesh;
};
/// For each group, the mesh that the group changes and keeps, or nothing when the group gets a new mesh. A group can
/// keep a mesh only when no other temple has that mesh after the call. Thus each other temple that has the mesh now must
/// go to another group in the call. A group that comes later can keep a mesh that the temples of an earlier group
/// leave. The temples of a group try their meshes in their order.
/// @param changes The temples that change, in the order of the temples
/// @param others The mesh of each temple that does not change, one number for each temple
[[nodiscard]] std::vector<std::optional<size_t>> MeshesToKeep(std::span<const MeshChange> changes, size_t groups,
                                                              std::span<const size_t> others);

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

/// The name in the cache of the own mesh of the temple with this entity number
[[nodiscard]] std::string OwnMeshName(uint32_t temple);
/// The name in the cache of the worship site mesh that wears the skin of the temple's own mesh with this id
[[nodiscard]] std::string SiteMeshName(uint32_t templeMesh);
/// The ids in the cache of the meshes that are made for the temple's own mesh with this id: that mesh and the mesh of
/// its worship sites
[[nodiscard]] std::array<uint32_t, 2> MeshesMadeFor(uint32_t templeMesh);

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
/// The texels of the temple's skin, blended from one picture of 256 by 256 texels to another by weight from 0 to 255.
/// A skin of half size first takes the texel at the start of each two by two square of each picture, as the game loads
/// the pictures at the lowest detail levels. Nothing for pictures of another size.
[[nodiscard]] std::optional<std::vector<uint16_t>> SkinTexels(std::span<const uint16_t> from, std::span<const uint16_t> to,
                                                              uint8_t weight, bool halfSize);

} // namespace openblack::TempleExteriorMorph
