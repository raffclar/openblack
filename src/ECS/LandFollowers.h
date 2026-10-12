/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace openblack
{
class LandIslandInterface;
}

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::land_followers
{

/// A living thing keeps its height above the land: when the land under it rises or falls, it rises or falls as much.
/// One standing on the land stays on it.
[[nodiscard]] constexpr float FollowedHeight(float height, float landBefore, float landAfter)
{
	return height + (landAfter - landBefore);
}

/// Whether a thing goes up and down with the land under it: villagers, animals and creatures do, except while they
/// fly in the physics or a hand holds them
[[nodiscard]] constexpr bool FollowsTheLand(bool living, bool inPhysics, bool inHand)
{
	return living && !inPhysics && !inHand;
}

/// A living thing and the land's height under it before the land changed
struct Follower
{
	entt::entity entity {entt::null};
	float landBefore {0.0f};
};

/// The living things that follow the land, between two corners of the land in metres, with the land's height under
/// each now. Taken before the land's heights are changed there.
[[nodiscard]] std::vector<Follower> Capture(const Registry& registry, const LandIslandInterface& land, glm::vec2 lowest,
                                            glm::vec2 highest);

/// Once the land's changed heights are in, each of them keeps its height above the land under it
void Follow(Registry& registry, const LandIslandInterface& land, std::span<const Follower> followers);

} // namespace openblack::ecs::land_followers
