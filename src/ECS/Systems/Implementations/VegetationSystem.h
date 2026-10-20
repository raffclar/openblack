/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <chrono>
#include <optional>
#include <unordered_map>

#include "ECS/Systems/VegetationInterface.h"
#include "Nature/TreeBend.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

/// How Black & White's trees move.
///
/// Trees share 16 sways, each a slow back and forth whose speed changes at random every two seconds of game time. A
/// tree follows the sway its facing picks, so trees facing the same way move together. The sway leans the tree along
/// the world's z axis by up to 3% of its height, whatever the weather.
///
/// Things that pass through the trees bend them instead. The thing that the hand holds (never the empty hand), the
/// bodies that move in the physics and the primary creature of the local player are bend points. A tree in reach of
/// the point that marked it last leans away by up to 27 degrees, less the further it is (Nature/TreeBend.h). Bent trees
/// crash about, and the trees round the camera rustle by themselves (TreeRustle).
///
/// Fully grown fields follow the same sways, leaning their crops 1.75 times as far.
// TODO(raffclar): creatures knock trees down
class VegetationSystem final: public VegetationInterface
{
public:
	void Update(std::chrono::duration<float, std::milli> gameTime) override;
	void UpdateBendPoints(bool handShown) override;
	void Rustle(std::chrono::duration<float, std::milli> gameTime) override;
	[[nodiscard]] glm::mat4 GetTreeMatrix(entt::entity tree, const glm::mat4& model, const glm::vec3& position, float scale,
	                                      uint8_t swaySlot) const override;
	[[nodiscard]] glm::mat4 GetFieldMatrix(const glm::mat4& model, float scale, uint8_t swaySlot) const override;

private:
	/// Marks the trees round a bend point with its slot
	void Mark(const nature::tree_bend::Source& source, std::optional<uint8_t> slot);

	/// Lean of each sway along the z axis, per unit of a tree's height
	std::array<float, k_SwayCount> _leans {};
	std::array<float, k_SwayCount> _phases {};
	/// Phase speed of each sway, 1 to 2
	std::array<float, k_SwayCount> _speeds {};
	/// Game time since the speeds last changed
	std::chrono::duration<float, std::milli> _speedTime {0.0f};
	/// The bend points of all slots, kept from frame to frame
	nature::tree_bend::BendTable _bendTable;
	/// How far each tree bends this frame
	std::unordered_map<entt::entity, nature::tree_bend::Bend> _bends;
};
} // namespace openblack::ecs::systems
