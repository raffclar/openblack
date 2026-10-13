/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SnowLevels.h"

#include "3D/FieldCrop.h"
#include "3D/SnowCover.h"
#include "Animals/BirdRules.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/SnowLevel.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/TownDesireFlags.h"

namespace openblack::ecs::snow_levels
{

using components::Animal;
using components::Creature;
using components::Field;
using components::SnowLevel;
using components::TownDesireFlag;
using components::Transform;

std::optional<Hold> HoldOf(const Registry& registry, entt::entity entity)
{
	if (registry.AllOf<Creature>(entity))
	{
		return std::nullopt;
	}
	if (const auto* animal = registry.TryGet<const Animal>(entity); animal != nullptr && animals::birds::IsBird(animal->type))
	{
		return std::nullopt;
	}
	if (registry.AllOf<Field>(entity))
	{
		return Hold {.rate = field_crop::k_SnowCap, .cap = field_crop::k_SnowCap};
	}
	if (registry.AllOf<TownDesireFlag>(entity))
	{
		return Hold {.rate = town_desire_flags::k_SnowCap, .cap = town_desire_flags::k_SnowCap};
	}
	return Hold {.rate = snow_cover::k_ObjectRate, .cap = snow_cover::k_ObjectRate};
}

void Step(Registry& registry, std::span<const entt::entity> things, const std::function<float(glm::vec2)>& depthAt,
          int32_t step)
{
	if (step <= 0)
	{
		return;
	}
	for (const auto entity : things)
	{
		const auto* transform = registry.TryGet<const Transform>(entity);
		const auto hold = HoldOf(registry, entity);
		if (transform == nullptr || !hold.has_value())
		{
			continue;
		}
		const auto target =
		    snow_cover::ObjectLevel(depthAt({transform->position.x, transform->position.z}), hold->rate, hold->cap);
		if (auto* snow = registry.TryGet<SnowLevel>(entity); snow != nullptr)
		{
			snow->level = snow_cover::EaseLevel(snow->level, target, step);
		}
		else if (target != 0)
		{
			// A thing without snow gets its level the first time that it starts to show some
			registry.Assign<SnowLevel>(entity, snow_cover::EaseLevel(0, target, step));
		}
	}
}

int32_t LevelOf(const Registry& registry, entt::entity entity)
{
	const auto* snow = registry.TryGet<const SnowLevel>(entity);
	return snow != nullptr ? snow->level : 0;
}

} // namespace openblack::ecs::snow_levels
