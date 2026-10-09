/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureBodyFile.h"

#include <glm/vec3.hpp>

#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

creature_mind_body::Body creature_body_file::Capture(const Registry& registry, entt::entity creature)
{
	creature_mind_body::Body body;
	if (const auto* self = registry.TryGet<const Creature>(creature))
	{
		body.species = self->species;
		body.alignment = self->alignment;
		body.strength = self->strength;
		body.size = self->size;
		body.fatness = self->fatness;
		body.shownFatness = self->fatness;
	}
	if (const auto* morph = registry.TryGet<const CreatureMorph>(creature))
	{
		body.shownFatness = morph->shownFatness;
	}
	if (const auto* needs = registry.TryGet<const CreatureNeeds>(creature))
	{
		if (needs->started)
		{
			body.needs = creature_physiology::Keep(needs->needs);
		}
		else if (needs->kept.has_value())
		{
			body.needs = needs->kept;
		}
	}
	if (const auto* tattoos = registry.TryGet<const CreatureTattoos>(creature))
	{
		body.tattoos = tattoos->slots;
	}
	if (const auto* marks = registry.TryGet<const CreatureMarks>(creature))
	{
		body.wounds = marks->marks.wounds;
		body.blood = marks->marks.blood;
	}
	return body;
}

void creature_body_file::Apply(Registry& registry, entt::entity creature, const creature_mind_body::Body& body)
{
	if (auto* self = registry.TryGet<Creature>(creature))
	{
		if (body.alignment.has_value())
		{
			self->alignment = *body.alignment;
			self->pendingAlignment = 0.0f;
		}
		self->strength = body.strength;
		self->fatness = body.fatness;
		if (body.size.has_value())
		{
			self->size = *body.size;
			if (auto* transform = registry.TryGet<Transform>(creature))
			{
				transform->scale = glm::vec3(archetypes::CreatureArchetype::DrawnScale(self->species, self->size));
			}
		}
	}
	if (auto* morph = registry.TryGet<CreatureMorph>(creature))
	{
		morph->shownFatness = body.shownFatness;
		++morph->revision;
	}
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature); needs != nullptr && body.needs.has_value())
	{
		if (needs->started)
		{
			creature_physiology::TakeUp(needs->needs, *body.needs);
		}
		else
		{
			needs->kept = body.needs;
		}
	}
	if (auto* tattoos = registry.TryGet<CreatureTattoos>(creature); tattoos != nullptr && body.tattoos.has_value())
	{
		tattoos->slots = *body.tattoos;
		++tattoos->revision;
	}
	if (auto* marks = registry.TryGet<CreatureMarks>(creature))
	{
		// The healing counted since the marks last aged isn't kept
		marks->marks = {.wounds = body.wounds, .blood = body.blood, .counts = 0};
		++marks->revision;
	}
}
