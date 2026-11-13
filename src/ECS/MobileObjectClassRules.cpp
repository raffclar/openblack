/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MobileObjectClassRules.h"

#include "ECS/Components/CreedObject.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;

glm::mat4 mobile_object_class::CreedGlowModel(const glm::mat4& objectModel, float glowScale)
{
	auto model = objectModel;
	for (int axis = 0; axis < 3; ++axis)
	{
		model[axis] = glm::vec4(glm::vec3(objectModel[axis]) * glowScale, objectModel[axis].w);
	}
	return model;
}

void mobile_object_class::SlideCreedGlows(Registry& registry, std::chrono::milliseconds frameRealTime)
{
	registry.Each<components::CreedObject>([frameRealTime](entt::entity, components::CreedObject& creed) {
		const auto stepMs = frame_pace::Step(creed.slidePace, frameRealTime, frameRealTime);
		creature_hand_glow::Slide(creed.motion, static_cast<int32_t>(stepMs.count()));
	});
}

float mobile_object_class::CropScale(float life, float scalePerLife)
{
	return life * scalePerLife;
}
