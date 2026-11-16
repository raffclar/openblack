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
#include "Creature/LeashRules.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LeashSystemInterface.h"
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

std::optional<std::string_view> CreatureSpawnRefusal(const player_commands::EditorCreatureSpawn& spawn) noexcept
{
	const auto& point = spawn.position;
	const bool finite = std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z) &&
	                    std::isfinite(spawn.radians) && std::isfinite(spawn.scale) && std::isfinite(spawn.alignment) &&
	                    std::isfinite(spawn.fatness) && std::isfinite(spawn.strength);
	if (!finite)
	{
		return "a value is not a finite number";
	}
	if (spawn.species <= static_cast<uint8_t>(CreatureType::Unknown) ||
	    spawn.species >= static_cast<uint8_t>(CreatureType::_COUNT))
	{
		return "the game has no such creature";
	}
	if (spawn.owner >= static_cast<uint8_t>(PlayerNames::_COUNT))
	{
		return "the game has no such player";
	}
	// The creature tools give these limits to a new creature
	if (spawn.scale < creature_morph::k_MinScale || spawn.scale > creature_morph::k_MaxScale)
	{
		return "the size is not in the range of the creature tools";
	}
	if (spawn.alignment < -1.0f || spawn.alignment > 1.0f || spawn.fatness < 0.0f || spawn.fatness > 1.0f ||
	    spawn.strength < 0.0f || spawn.strength > 1.0f)
	{
		return "a value of the body is not in its range";
	}
	return std::nullopt;
}

entt::entity ApplyCreatureSpawn(const player_commands::EditorCreatureSpawn& spawn)
{
	using ecs::archetypes::CreatureArchetype;
	if (CreatureSpawnRefusal(spawn).has_value() || !Locator::entitiesRegistry::has_value())
	{
		return entt::null;
	}
	const auto entity = CreatureArchetype::Create(
	    spawn.position, static_cast<PlayerNames>(spawn.owner), static_cast<CreatureType>(spawn.species), 0, spawn.radians,
	    spawn.scale, {.alignment = spawn.alignment, .fatness = spawn.fatness, .strength = spawn.strength});
	// A creature of the creature tools knows every leash, as the creatures of the debug tools of the game do
	if (Locator::leashSystem::has_value())
	{
		for (const auto type : creature_leash::k_Types)
		{
			Locator::leashSystem::value().SetKnown(entity, type, true);
		}
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* tattoos = registry.TryGet<ecs::components::CreatureTattoos>(entity); tattoos != nullptr && spawn.hasTattoos)
	{
		tattoos->slots = spawn.tattoos;
		++tattoos->revision;
	}
	return entity;
}

void SendCreatureSpawn(const player_commands::EditorCreatureSpawn& spawn)
{
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(spawn);
	}
}

} // namespace openblack::editor
