/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureFizzSystem.h"

#include <chrono>
#include <vector>

#include "Audio/Sound.h"
#include "ECS/Components/CreatureFizz.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
constexpr auto k_TurnMilliseconds =
    static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(TimeSystemInterface::k_TurnDuration).count());

/// The creature is drawn as fizzed as its fizz says
void Show(ecs::Registry& registry, entt::entity creature, float fizz)
{
	auto* spells = registry.TryGet<CreatureSpells>(creature);
	if (spells == nullptr)
	{
		spells = &registry.Assign<CreatureSpells>(creature);
	}
	spells->fizz = fizz;
}
} // namespace

void CreatureFizzSystem::SetFizz(entt::entity creature, float target, float seconds, bool goesForGood)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	auto* current = registry.TryGet<CreatureFizz>(creature);
	const auto set =
	    creature_fizz::SetFizz(current != nullptr ? current->fizz : creature_fizz::Fizz {}, target, seconds, goesForGood);
	if (set.sound && Locator::soundTagSystem::has_value())
	{
		if (const auto* transform = registry.TryGet<const Transform>(creature))
		{
			Locator::soundTagSystem::value().CreatePointSound(
			    static_cast<entt::id_type>(audio::SoundId::G_SpellTeleportEnergiseGo), transform->position, false);
		}
	}
	registry.AssignOrReplace<CreatureFizz>(creature, CreatureFizz {.fizz = set.fizz});
	Show(registry, creature, set.fizz.now);
}

void CreatureFizzSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> settled;
	std::vector<entt::entity> gone;
	registry.Each<CreatureFizz>([&](entt::entity entity, CreatureFizz& component) {
		component.fizz = creature_fizz::Step(component.fizz, k_TurnMilliseconds);
		if (creature_fizz::Gone(component.fizz))
		{
			gone.push_back(entity);
		}
		else if (creature_fizz::Settled(component.fizz))
		{
			settled.push_back(entity);
		}
	});
	for (const auto entity : settled)
	{
		Show(registry, entity, 0.0f);
		registry.Remove<CreatureFizz>(entity);
	}
	registry.Each<const CreatureFizz>([&registry](entt::entity entity, const CreatureFizz& component) {
		if (auto* spells = registry.TryGet<CreatureSpells>(entity))
		{
			spells->fizz = component.fizz.now;
		}
	});
	// Fizzed right out for good, it leaves the world
	for (const auto entity : gone)
	{
		registry.Destroy(entity);
	}
	if (!gone.empty())
	{
		registry.SetDirty();
	}
}

float CreatureFizzSystem::FizzOf(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return 0.0f;
	}
	if (const auto* component = registry.TryGet<const CreatureFizz>(creature))
	{
		return component->fizz.now;
	}
	return 0.0f;
}
