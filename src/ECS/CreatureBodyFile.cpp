/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureBodyFile.h"

#include <vector>

#include <glm/vec3.hpp>

#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "Locator.h"

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
		body.tattooPictures = tattoos->pictures;
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
		// A body brought back from a file shows the strength it has now
		if (const auto* self = registry.TryGet<const Creature>(creature))
		{
			morph->strengthLook =
			    creature_morph::StrengthLook(self->strength, archetypes::CreatureArchetype::SpeciesStrength(self->species));
		}
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
		tattoos->pictures = body.tattooPictures.value_or(std::vector<uint8_t> {});
		// A creature of a player makes the player's masks again from the designs that the game ships with
		const auto* owner = registry.TryGet<const Creature>(creature);
		if (owner != nullptr && owner->owner != PlayerNames::NEUTRAL && Locator::creatureSkinSystem::has_value())
		{
			Locator::creatureSkinSystem::value().RemakeTattooMasks(creature, creature_tattoo::Sheet::Shipped);
		}
		++tattoos->revision;
	}
	if (auto* marks = registry.TryGet<CreatureMarks>(creature))
	{
		// The healing counted since the marks last aged isn't kept
		marks->marks = {.wounds = body.wounds, .blood = body.blood, .counts = 0};
		++marks->revision;
	}
}
