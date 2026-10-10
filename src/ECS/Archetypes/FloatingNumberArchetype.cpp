/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FloatingNumberArchetype.h"

#include "Common/FixedFormat.h"
#include "ECS/Components/FloatingNumber.h"
#include "ECS/FloatingNumber.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity FloatingNumberArchetype::Create(const glm::vec3& position, float value, uint32_t colour)
{
	auto& registry = openblack::Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// Written three wide with nothing after the point
	registry.Assign<FloatingNumber>(entity, FloatingNumber {.text = openblack::fixed_format::Fixed(value, 3, 0),
	                                                        .position = position,
	                                                        .colour = colour,
	                                                        .life = floating_number::k_LifeSeconds});
	return entity;
}
