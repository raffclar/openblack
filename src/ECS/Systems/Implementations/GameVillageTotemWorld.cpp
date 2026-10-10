/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameVillageTotemWorld.h"

#include <optional>

#include "3D/AllMeshes.h"
#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "Audio/Sound.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Temple.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The player's creature: their first, the one they lead
std::optional<entt::entity> PrimaryCreature(PlayerNames player)
{
	if (!Locator::leashSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::leashSystem::value().PlayersCreature(player);
}
} // namespace

ecs::Registry& GameVillageTotemWorld::Entities()
{
	return Locator::entitiesRegistry::value();
}

entt::id_type GameVillageTotemWorld::IconMeshFor(PlayerNames player) const
{
	auto icon = MeshId::BuildingSpellHand;
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto creature = PrimaryCreature(player); creature.has_value() && Locator::infoConstants::has_value())
	{
		if (const auto* body = registry.TryGet<const Creature>(*creature))
		{
			const auto& infos = Locator::infoConstants::value().creature;
			if (const auto row = creature::InfoRow(body->species); row < infos.size())
			{
				icon = infos.at(row).totemIcon;
			}
		}
	}
	return resources::HashIdentifier(icon);
}

ecs::village_totem::WorldInterface::Size GameVillageTotemWorld::SizeOf(entt::entity object) const
{
	const auto size = world_objects::SizeOf(object);
	return {.radius = size.radius, .height = size.height};
}

float GameVillageTotemWorld::LandHeightAt(glm::vec2 point) const
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

bool GameVillageTotemWorld::TempleBuilt(PlayerNames player) const
{
	bool built = false;
	Locator::entitiesRegistry::value().Each<const Temple>(
	    [player, &built](entt::entity /*unused*/, const Temple& temple) { built = built || temple.owner == player; });
	return built;
}

bool GameVillageTotemWorld::Built(entt::entity building) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(building))
	{
		return false;
	}
	const auto* progress = registry.TryGet<const BuildProgress>(building);
	return progress == nullptr || progress->built >= 1.0f;
}

void GameVillageTotemWorld::SetMovingSound(entt::entity totem, bool on)
{
	if (Locator::soundTagSystem::has_value())
	{
		Locator::soundTagSystem::value().SetActive(totem, on);
	}
}

void GameVillageTotemWorld::RingBell(glm::vec3 position)
{
	if (Locator::soundTagSystem::has_value())
	{
		Locator::soundTagSystem::value().CreatePointSound(static_cast<entt::id_type>(audio::SoundId::G_VillageBell), position,
		                                                  false);
	}
}
