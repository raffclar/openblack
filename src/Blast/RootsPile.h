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

#include <optional>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"

/// A pile of roots and earth on the land: the hole that a tree leaves when the hand pulls it free, and the rubble of a
/// blast. A pile is a mark that only this computer draws. It is not a thing of the world, so the game turns do not see
/// it, a save does not keep it and the other computers of a network game do not show it.
namespace openblack::roots_pile
{

/// The model of every pile
constexpr MeshId k_Mesh = MeshId::TreeRootsPile;
/// A pile stays for this many milliseconds of game time
constexpr int32_t k_Milliseconds = 15000;
/// A pile fades out in its last second
constexpr int32_t k_FadeMilliseconds = 1000;
/// The alpha of a fading pile for each millisecond that stays, so that the alpha is 255 at the start of the fade
constexpr float k_AlphaPerMillisecond = 0.255f;
/// The size of the hole of a tree is this share of the half width and the half depth of the model of the tree
constexpr float k_TreeHoleShare = 0.3f;

struct Pile
{
	glm::vec3 position {0.0f};
	glm::mat3 rotation {1.0f};
	float scale {1.0f};
	int32_t millisecondsLeft {k_Milliseconds};
	/// The alpha of the pile, 0 to 255, after its fade starts. Until then the draw does not blend it.
	std::optional<uint8_t> alpha;

	bool operator==(const Pile&) const noexcept = default;
};

/// The size of the hole that a tree leaves: the half width and the half depth of the box of its model, added, times
/// the size of the tree and the share
[[nodiscard]] float TreeHoleScale(const glm::vec3& treeHalfSize, float treeScale);

/// One frame that draws the land, with the game time of that frame in milliseconds:
/// - In the last second, the alpha of the pile comes from the time that stays before the frame.
/// - The time of the frame then goes from the time that stays.
/// Returns false when no time stays: the frame does not draw the pile and removes it.
[[nodiscard]] bool Age(Pile& pile, int32_t frameMilliseconds);

} // namespace openblack::roots_pile
