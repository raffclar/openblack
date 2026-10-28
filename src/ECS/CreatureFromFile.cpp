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
#include <string_view>

#include <MindFile.h>
#include <fmt/format.h>
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
#include "ECS/CreatureHome.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptHeaders/ScriptPropertyRules.h"

namespace openblack::ecs::creature_from_file
{
namespace
{

/// Makes the creature on the ground at the place, and makes it the creature of the owner. The mind file gives its mind,
/// its body, its alignment and its tattoos. It knows every leash, and it grows up fully at once.
entt::entity MakeFromMind(PlayerNames owner, CreatureType species, map_coords::MapCoords coords,
                          std::shared_ptr<const creaturemind::MindFileData> file, entt::id_type mindId, std::string_view source)
{
	auto& registry = Locator::entitiesRegistry::value();
	// The new creature replaces the creature that the owner has as their creature
	const auto replaced =
	    Locator::playerSystem::has_value() && Locator::playerSystem::value().GetPrimaryCreature(owner).has_value();
	const auto position = map_coords::ToWorld(Locator::terrainSystem::value(), coords);
	const auto creature = archetypes::CreatureArchetype::Create(position, owner, species, 0, creature_file_load::k_StartYaw,
	                                                            archetypes::CreatureArchetype::StartScale(species),
	                                                            archetypes::CreatureArchetype::StartBody(species));
	if (Locator::playerSystem::has_value())
	{
		Locator::playerSystem::value().MakePrimaryCreature(creature);
	}
	if (replaced && Locator::leashSystem::has_value())
	{
		Locator::leashSystem::value().SetLeashable(creature, true);
	}

	// The creature takes up the mind, the body, the alignment and the tattoos that the file keeps
	if (file != nullptr)
	{
		if (Locator::creatureMindSystem::has_value())
		{
			Locator::creatureMindSystem::value().TakeUpMindFile(creature, file);
			// Once it takes up the file, its leash words are all set and it is fully grown
			Locator::creatureMindSystem::value().GrowUpAfterFile(creature);
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
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Made creature {} for player {} from {} at {:.1f}, {:.1f}",
	                   entt::to_integral(creature), static_cast<int>(owner), source, position.x, position.z);
	return creature;
}

} // namespace

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
	Result result;

	// The game reads the mind file through the resource cache, from the creature mind folder of the game
	std::shared_ptr<const creature::CreatureMind> mind;
	entt::id_type mindId = 0;
	if (Locator::resources::has_value() && Locator::filesystem::has_value())
	{
		// The file can be the file of the player's kept creature, so the files that wait go to the disk first
		if (Locator::creatureCarryOverSystem::has_value())
		{
			Locator::creatureCarryOverSystem::value().FinishWrites();
		}
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

	const auto replaced =
	    Locator::playerSystem::has_value() && Locator::playerSystem::value().GetPrimaryCreature(order.owner).has_value();
	if (replaced)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}", creature_file_load::k_OverwriteMessage);
	}
	result.replaced = replaced;
	result.creature = MakeFromMind(order.owner, *species, coords,
	                               mind->Loaded() ? std::make_shared<const creaturemind::MindFileData>(mind->data) : nullptr,
	                               mindId, fmt::format("the mind file {}", order.fileName));
	return result;
}

Result Bring(PlayerNames owner, std::span<const uint8_t> file)
{
	Result result;
	const auto& registry = Locator::entitiesRegistry::value();
	const auto home = creature_home::CitadelCreatureHomeOf(registry, owner);
	// The bytes came from the player before the start, as the game reads the file of a creature
	auto mind = std::make_shared<creaturemind::MindFileData>();
	const auto readable = !file.empty() && creaturemind::Read(file, *mind) == creaturemind::MindResult::Success;
	const auto species = readable ? script::property_rules::CreatureTypeFromScript(mind->speciesRow) : std::nullopt;
	result.refusal = creature_file_load::WhyNotBrought(
	    {.citadel = home.has_value(),
	     .file = !file.empty(),
	     .readable = readable,
	     .onMap = home.has_value() && map_coords::InBounds(*home) && Locator::terrainSystem::has_value(),
	     .species = species});
	if (result.refusal != creature_file_load::Refusal::None)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The god {} brings no creature into the network game: reason {}",
		                   static_cast<int>(owner), static_cast<int>(result.refusal));
		return result;
	}
	result.replaced =
	    Locator::playerSystem::has_value() && Locator::playerSystem::value().GetPrimaryCreature(owner).has_value();
	result.creature = MakeFromMind(owner, *species, *home, std::move(mind), 0,
	                               fmt::format("the creature file of {} bytes that its player sent", file.size()));
	return result;
}

} // namespace openblack::ecs::creature_from_file
