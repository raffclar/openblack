/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Life.h"

#include <spdlog/spdlog.h>

#include "ECS/Components/Animal.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/VillagerSpeed.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

float openblack::ecs::life::LifeOf(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* villager = registry.TryGet<const Villager>(entity))
	{
		return villager->life;
	}
	if (const auto* life = registry.TryGet<const Life>(entity))
	{
		return life->value;
	}
	// (inferred) no Life component yet = full life: every object has a life in the original, and openblack only
	// assigns the component on the first change
	return 1.0f;
}

void openblack::ecs::life::SetLife(entt::entity entity, float life)
{
	// the original keeps the life above 0.01 when flag 0x40 (or 0x200 in some interface state) is set: UNVERIFIED
	// which objects those are, not ported. A villager's life change also counts the town's villagers under 0.7 life:
	// openblack's Town has no such count yet.
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* villager = registry.TryGet<Villager>(entity))
	{
		villager->life = life;
		return;
	}
	auto& component = registry.AllOf<Life>(entity) ? registry.Get<Life>(entity) : registry.Assign<Life>(entity);
	component.value = life;
}

float openblack::ecs::life::ReduceLife(entt::entity entity, float amount)
{
	const float life = LifeOf(entity);
	SetLife(entity, life < amount ? 0.0f : life - amount);
	return LifeOf(entity);
}

float openblack::ecs::life::IncreaseLife(entt::entity entity, float amount)
{
	const float life = LifeOf(entity);
	if (life + amount > 1.0f)
	{
		amount = 1.0f - life;
	}
	if (amount != 0.0f)
	{
		SetLife(entity, life + amount);
	}
	return LifeOf(entity);
}

bool openblack::ecs::life::IsPoisoned(entt::entity entity)
{
	// the living's poisoned bit
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(entity) && registry.AllOf<Poisoned>(entity);
}

void openblack::ecs::life::SetPoisoned(entt::entity entity, bool poisoned)
{
	// Only a Living has the bit: an object's own poisoned flag (the pots', components::Pot::poisoned) is not this
	// one
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AnyOf<Villager, Animal>(entity))
	{
		return;
	}
	if (poisoned == registry.AllOf<Poisoned>(entity))
	{
		return;
	}
	if (poisoned)
	{
		registry.Assign<Poisoned>(entity);
	}
	else
	{
		registry.Remove<Poisoned>(entity);
	}
}

void openblack::ecs::life::TakePoisonedResource(entt::entity living)
{
	// adding and taking a poisoned resource both just SetPoisoned(1)
	SetPoisoned(living, true);
}

float openblack::ecs::life::HungerLifeLoss(float food, float hungryForFood, float hungerToLifeMultiplier)
{
	// 1 - food / hungryForFood, then the *bigger* of that and 1 (kept only when it is above 1), x
	// hungerToLifeMultiplier. food is clamped to >= 0 before, so the first term never wins and the loss is the
	// multiplier alone; it is written out as the original computes it
	const float hunger = 1.0f - food / hungryForFood;
	return (hunger > 1.0f ? hunger : 1.0f) * hungerToLifeMultiplier;
}

float openblack::ecs::life::ProcessPoison(entt::entity villager)
{
	if (!IsPoisoned(villager))
	{
		return 0.0f;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* v = registry.TryGet<const Villager>(villager);
	const auto* info = ecs::VillagerInfoOf(villager);
	if (v == nullptr || info == nullptr)
	{
		return 0.0f; // the animals' poison: the hunger check is the villagers' (Animal has no belly)
	}
	const float loss = HungerLifeLoss(v->food, info->hungryForFood, info->hungerToLifeMultiplier);
	const float before = LifeOf(villager);
	ReduceLife(villager, loss); // no player
	return before - LifeOf(villager);
}

void openblack::ecs::life::Kill(entt::entity entity, const char* reason)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(entity))
	{
		return; // already gone or marked (two deaths in one turn)
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: {} died ({})", registry.AllOf<Villager>(entity) ? "villager" : "animal",
	                   reason);
	// the class's deletion: the villager's / animal's DeleteDependants, out of the physics and the registry
	// (ECS/ToBeDeleted.h)
	ecs::ToBeDeleted(entity);
}
