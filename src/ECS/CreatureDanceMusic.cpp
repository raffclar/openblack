/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureDanceMusic.h"

#include <cmath>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureDance.h"
#include "ECS/Components/CreatureDance.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

std::optional<audio::GameMusic::TurnInputs::CreatureDance> ecs::creature_dance_music::Heard(glm::vec3 camera)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	float nearest = creature_dance::k_MusicFurthest;
	std::optional<entt::entity> dancing;
	entt::entity firstDancer = entt::null;
	entt::entity dance = entt::null;
	registry.Each<const CreatureMindState, const CreatureDance, const Transform>(
	    [&](entt::entity creature, const CreatureMindState& mind, const CreatureDance& record, const Transform& at) {
		    const auto& idle = mind.idle;
		    // Only while the creature's agenda is at its dance
		    if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::Dance ||
		        idle.agenda[idle.step].dance.kind != creature_mind::DanceOrder::Kind::Dance ||
		        !dances::IsDance(registry, record.dance))
		    {
			    return;
		    }
		    const auto first = dances::FirstDancer(registry, record.dance, entt::null);
		    if (first == entt::null || !registry.AllOf<Villager>(first))
		    {
			    return;
		    }
		    const float distance = gutils::GetDistanceInMetres(camera, at.position);
		    if (distance < nearest)
		    {
			    nearest = distance;
			    dancing = creature;
			    firstDancer = first;
			    dance = record.dance;
		    }
	    });
	if (!dancing.has_value())
	{
		return std::nullopt;
	}
	const auto position = registry.Get<const Transform>(*dancing).position;
	const float ground =
	    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::xz(position)) : position.y;
	if (!(std::abs(camera.y - ground) < creature_dance::k_MusicHighest))
	{
		return std::nullopt;
	}
	const auto tribe = static_cast<int32_t>(registry.Get<const Villager>(firstDancer).tribe);
	const auto type = creature_dance::MusicType(tribe, registry.Get<const Dance>(dance).dancers);
	return audio::GameMusic::TurnInputs::CreatureDance {.type = static_cast<audio::MusicType>(type), .position = position};
}
