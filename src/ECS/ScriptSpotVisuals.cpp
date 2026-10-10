/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptSpotVisuals.h"

#include <vector>

#include <glm/mat3x3.hpp>

#include "ECS/Components/ScriptSpotVisual.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"

namespace openblack::ecs::script_spot_visuals
{

entt::entity MakeThing(Registry& registry, uint32_t effect, glm::vec3 position)
{
	const auto thing = registry.Create();
	registry.Assign<components::Transform>(thing, position, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<components::ScriptSpotVisual>(thing, effect);
	return thing;
}

void RemoveEnded(Registry& registry, const std::function<bool(uint32_t effect)>& running)
{
	std::vector<entt::entity> ended;
	registry.Each<const components::ScriptSpotVisual>(
	    [&ended, &running](entt::entity thing, const components::ScriptSpotVisual& visual) {
		    if (!running(visual.effect))
		    {
			    ended.push_back(thing);
		    }
	    });
	for (const auto thing : ended)
	{
		registry.Destroy(thing);
	}
}

} // namespace openblack::ecs::script_spot_visuals
