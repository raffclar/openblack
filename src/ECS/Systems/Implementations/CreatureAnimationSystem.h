/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>

#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs
{
class Registry;
}
namespace openblack::ecs::components
{
struct CreatureEyes;
}

namespace openblack::ecs::systems
{

class CreatureAnimationSystem final: public CreatureAnimationSystemInterface
{
public:
	void ProcessTurn() override;
	void Update(std::chrono::duration<float, std::milli> gameTime) override;
	[[nodiscard]] std::optional<glm::vec3> BoneInAnimation(entt::entity creature, size_t animation, float timeMs, uint32_t bone,
	                                                       bool mirrored) override;
	[[nodiscard]] std::optional<float> AnimationDuration(entt::entity creature, size_t animation) override;

	/// The eyes placed for the frame on the body as it is drawn (ecs::DrawnBodyModel, turned as ecs::creature_pose::
	/// DrawnPlacementOf), posed by `bones`. Only reads the registry
	static void PlaceEyes(const ecs::Registry& registry, entt::entity entity, components::CreatureEyes& eyes,
	                      const creature::CreatureRig::Eyes& rig, const creature_morph::Morph& morph,
	                      const std::vector<glm::mat4>& bones, float size, float seconds);
};

} // namespace openblack::ecs::systems
