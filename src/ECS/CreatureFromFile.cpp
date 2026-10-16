/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFromFile.h"

#include <filesystem>
#include <memory>

#include <MindFile.h>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Creature/CreatureDevelopment.h"
#include "Creature/CreatureMindFileBody.h"
#include "Creature/LeashRules.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/Player.h"
#include "ECS/CreatureBodyFile.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptHeaders/ScriptPropertyRules.h"

namespace openblack::ecs::creature_from_file
{
bool PlayerInGame(PlayerNames name)
{
	if (name == PlayerNames::NEUTRAL)
	{
		return true;
	}
	bool found = false;
	Locator::entitiesRegistry::value().Each<const components::Player>(
	    [&found, name](const components::Player& player) { found = found || player.name == name; });
	return found;
}

Result Make(const Order& order)
{
	auto& registry = Locator::entitiesRegistry::value();
	Result result;

	// The game reads the mind file through the resource cache, from the creature mind folder of the game
	std::shared_ptr<const creature::CreatureMind> mind;
	entt::id_type mindId = 0;
	if (Locator::resources::has_value() && Locator::filesystem::has_value())
	{
		auto& minds = Locator::resources::value().GetCreatureMinds();
		const auto path = Locator::filesystem::value().GetPath<filesystem::Path::CreatureMind>(true) / order.fileName;
		const auto loaded = minds.Load(order.fileName, resources::CreatureMindLoader::FromDiskTag {}, path);
		mindId = loaded.first->first;
		mind = loaded.first->second.handle();
	}
	const auto fileOpens = mind != nullptr && mind->result != creaturemind::MindResult::ErrCantOpen;
	const auto coords = map_coords::FromMetres(order.place);
	const auto species = script::property_rules::CreatureTypeFromScript(order.speciesRow);
	result.refusal = creature_file_load::WhyNot({.playerInGame = PlayerInGame(order.owner),
	                                             .fileOpens = fileOpens,
	                                             .onMap = map_coords::InBounds(coords) && Locator::terrainSystem::has_value(),
	                                             .species = species});
	if (result.refusal != creature_file_load::Refusal::None)
	{
		if (result.refusal == creature_file_load::Refusal::NoPlayer)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}", creature_file_load::k_NoPlayerMessage);
		}
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "No creature made from the mind file {}: reason {}", order.fileName,
		                   static_cast<int>(result.refusal));
		return result;
	}

	// The new creature replaces the creature that the owner has as their creature
	const auto had =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetPrimaryCreature(order.owner) : std::nullopt;
	result.replaced = had.has_value();
	if (result.replaced)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}", creature_file_load::k_OverwriteMessage);
	}
	const auto position = map_coords::ToWorld(Locator::terrainSystem::value(), coords);
	const auto creature = archetypes::CreatureArchetype::Create(position, order.owner, *species, 0, 0.0f,
	                                                            archetypes::CreatureArchetype::StartScale(*species),
	                                                            archetypes::CreatureArchetype::StartBody(*species));
	if (Locator::playerSystem::has_value())
	{
		Locator::playerSystem::value().MakePrimaryCreature(creature);
	}
	if (result.replaced && Locator::leashSystem::has_value())
	{
		Locator::leashSystem::value().SetLeashable(creature, true);
	}

	// The creature takes up the mind, the body, the alignment and the tattoos that the file keeps
	if (mind->Loaded())
	{
		auto file = std::make_shared<const creaturemind::MindFileData>(mind->data);
		if (Locator::creatureMindSystem::has_value())
		{
			Locator::creatureMindSystem::value().TakeUpMindFile(creature, file);
		}
		if (const auto body = creature_mind_body::FromMindFile(*file); body.has_value())
		{
			creature_body_file::Apply(registry, creature, *body);
		}
		registry.Get<components::Creature>(creature).mind = mindId;
	}

	// It knows every leash, and it grows up fully at once
	if (Locator::leashSystem::has_value())
	{
		auto& leashes = Locator::leashSystem::value();
		for (const auto type : creature_leash::k_Types)
		{
			leashes.SetKnown(creature, type, true);
		}
	}
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().MoveToStage(creature, creature_file_load::k_LoadedStage);
	}
	if (Locator::leashSystem::has_value())
	{
		auto& leashes = Locator::leashSystem::value();
		const auto& leash = registry.Get<const components::CreatureLeash>(creature);
		if (creature_development::ReleasedFromHome(creature_file_load::k_LoadedStage, leash.confinementRadius,
		                                           leashes.IsLeashed(creature)))
		{
			leashes.ClearConfinement(creature);
		}
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Made creature {} for player {} from the mind file {} at {:.1f}, {:.1f}",
	                   entt::to_integral(creature), static_cast<int>(order.owner), order.fileName, position.x, position.z);
	result.creature = creature;
	return result;
}

} // namespace openblack::ecs::creature_from_file
