/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureMimic.h"

#include <cstddef>

#include <tuple>
#include <utility>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Common/EventManager.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillagerLastInteraction.h"
#include "ECS/Events/CreatureMimicEvents.h"
#include "ECS/Events/Publish.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs::creature_mimic
{
static_assert(std::tuple_size_v<decltype(InfoConstants::creatureMimic)> == creature_watching::k_DeedCount,
              "one deed per row of the mimicry table");

namespace
{
/// Where something is, read without making any storage: the origin for something with no place
glm::vec3 PlaceOfObject(entt::entity object)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (object == entt::null || !registry.Valid(object))
	{
		return glm::vec3(0.0f);
	}
	const auto* transform = registry.TryGet<const components::Transform>(object);
	return transform != nullptr ? transform->position : glm::vec3(0.0f);
}

/// The minds learn of the deed, if the game has them
void PassDeedToMinds(const events::PlayerDeedForMimic& event)
{
	if (!Locator::creatureMindSystem::has_value())
	{
		return;
	}
	const auto object = event.object == entt::null ? std::nullopt : std::optional(event.object);
	Locator::creatureMindSystem::value().PlayerDid(event.player, static_cast<size_t>(event.deed), event.point, object);
}
} // namespace

void Consider(PlayerNames player, creature_watching::Deed deed, entt::entity object, std::optional<MagicType> magic)
{
	events::Publish(events::PlayerDeedForMimic {
	    .player = player,
	    .deed = deed,
	    .object = object,
	    .point = PlaceOfObject(object),
	    .magic = magic,
	});
}

void ConsiderThrownInTheSea(entt::entity villager)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(villager))
	{
		return;
	}
	// the player whose hand last dropped it, if a hand did
	if (const auto* last = registry.TryGet<const components::VillagerLastInteraction>(villager); last != nullptr)
	{
		Consider(last->player, creature_watching::Deed::ThrowInTheSea, villager);
	}
}

bool ShouldMimicBuildingHit(std::optional<PlayerNames> player, uint32_t buildingBodyFlags)
{
	return player.has_value() && (buildingBodyFlags & physics::PhysicsObject::k_FromHand) != 0;
}

void EmpathiseWithTownDesire(std::optional<PlayerNames> player, TownDesireInfo desire, float weight, entt::entity villager)
{
	if (!player.has_value())
	{
		return;
	}
	events::Publish(events::CreatureEmpathyWithTownDesire {
	    .player = *player,
	    .desire = desire,
	    .weight = weight,
	    .point = PlaceOfObject(villager),
	});
}

void AddMimicEventHandlers(EventManager& manager)
{
	manager.AddHandler<events::PlayerDeedForMimic>(PassDeedToMinds);
}
} // namespace openblack::ecs::creature_mimic
