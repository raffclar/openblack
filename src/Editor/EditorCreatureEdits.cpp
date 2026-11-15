/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EditorCreatureEdits.h"

#include <cmath>

#include <algorithm>

#include <glm/vec3.hpp>

#include "Creature/CreatureMorph.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::editor
{

using creature_edit_values::Value;
using ecs::components::Creature;
using ecs::components::CreatureMindState;
using ecs::components::CreatureNeeds;
using ecs::components::Transform;

ValueLimits LimitsOf(Value value, float shownSize, uint32_t stages) noexcept
{
	switch (value)
	{
	case Value::Alignment:
	case Value::Warmth:
		return {.low = -1.0f, .high = 1.0f};
	case Value::Size:
		return {.low = creature_morph::k_MinScale, .high = creature_morph::k_MaxScale};
	case Value::Energy:
		return {.low = 0.0f, .high = std::max(1.0f, shownSize)};
	case Value::GrownUp:
		return {.low = 0.0f, .high = static_cast<float>(std::max(stages, uint32_t {1}) - 1)};
	case Value::Fatness:
	case Value::Strength:
	case Value::Exhaustion:
	case Value::Thirst:
	case Value::Poo:
	case Value::Life:
	case Value::Itchiness:
	case Value::Count:
		break;
	}
	return {.low = 0.0f, .high = 1.0f};
}

std::optional<float> AmountInLimits(Value value, float amount, float shownSize, uint32_t stages) noexcept
{
	if (!std::isfinite(amount))
	{
		return std::nullopt;
	}
	const auto limits = LimitsOf(value, shownSize, stages);
	const auto inLimits = std::clamp(amount, limits.low, limits.high);
	// A stage of growing up is a whole number
	return value == Value::GrownUp ? std::floor(inLimits) : inLimits;
}

float CreatureValueOf(Value value, const Creature& creature, const creature_physiology::Needs& needs,
                      const CreatureMindState& mind) noexcept
{
	switch (value)
	{
	case Value::Alignment:
		return creature.alignment;
	case Value::Fatness:
		return creature.fatness;
	case Value::Strength:
		return creature.strength;
	case Value::Size:
		return creature.size;
	case Value::Energy:
		return needs.energy;
	case Value::Exhaustion:
		return needs.exhaustion;
	case Value::Thirst:
		return needs.dehydration;
	case Value::Poo:
		return needs.poo;
	case Value::Life:
		return needs.life;
	case Value::Warmth:
		return needs.warmth;
	case Value::Itchiness:
		return needs.itchiness;
	case Value::GrownUp:
		return static_cast<float>(mind.developmentPhase);
	case Value::Count:
		break;
	}
	return 0.0f;
}

void SetCreatureValue(Value value, float amount, Creature& creature, creature_physiology::Needs& needs,
                      CreatureMindState& mind) noexcept
{
	switch (value)
	{
	case Value::Alignment:
		creature.alignment = amount;
		break;
	case Value::Fatness:
		creature.fatness = amount;
		break;
	case Value::Strength:
		creature.strength = amount;
		break;
	case Value::Size:
		creature.size = creature_morph::ClampScale(amount);
		break;
	case Value::Energy:
		needs.energy = amount;
		break;
	case Value::Exhaustion:
		needs.exhaustion = amount;
		break;
	case Value::Thirst:
		needs.dehydration = amount;
		break;
	case Value::Poo:
		needs.poo = amount;
		break;
	case Value::Life:
		needs.life = amount;
		break;
	case Value::Warmth:
		needs.warmth = amount;
		break;
	case Value::Itchiness:
		needs.itchiness = amount;
		break;
	case Value::GrownUp:
		mind.developmentPhase = static_cast<uint32_t>(amount);
		break;
	case Value::Count:
		break;
	}
}

bool ApplyCreatureEdit(ecs::Registry& registry, const player_commands::EditorCreatureEdit& edit)
{
	const auto value = creature_edit_values::ValueFrom(edit.value);
	if (!value.has_value() || !registry.Valid(edit.creature))
	{
		return false;
	}
	auto* creature = registry.TryGet<Creature>(edit.creature);
	auto* needs = registry.TryGet<CreatureNeeds>(edit.creature);
	auto* mind = registry.TryGet<CreatureMindState>(edit.creature);
	if (creature == nullptr || needs == nullptr || mind == nullptr)
	{
		return false;
	}
	// The limits come only from the synced world and the info table, so that each computer sets the same amount
	const auto stages = Locator::infoConstants::has_value()
	                        ? static_cast<uint32_t>(Locator::infoConstants::value().creatureDevelopmentPhaseEntry.size())
	                        : uint32_t {1};
	const auto amount = AmountInLimits(*value, edit.amount, ecs::components::ShownSize(*creature), stages);
	if (!amount.has_value())
	{
		return false;
	}
	SetCreatureValue(*value, *amount, *creature, needs->needs, *mind);
	if (*value == Value::Size)
	{
		// The body shows the new size at once
		if (auto* transform = registry.TryGet<Transform>(edit.creature); transform != nullptr)
		{
			transform->scale = glm::vec3(ecs::archetypes::CreatureArchetype::DrawnScale(creature->species, creature->size));
		}
	}
	if (*value == Value::Strength)
	{
		// As a script sets the strength, the body shows it at once
		ecs::archetypes::CreatureArchetype::ShowStrength(edit.creature);
	}
	registry.SetDirty();
	return true;
}

void SendCreatureEdit(entt::entity creature, Value value, float amount)
{
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(
		    player_commands::EditorCreatureEdit {.creature = creature, .value = static_cast<uint8_t>(value), .amount = amount});
	}
}

} // namespace openblack::editor
