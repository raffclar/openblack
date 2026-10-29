/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <exception>
#include <optional>
#include <ranges>
#include <unordered_set>
#include <vector>

#include <LHVM.h>
#include <SaveSlots.h>
#include <VanillaWorld.h>
#include <glm/mat3x3.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Archetypes/ScriptMarkerArchetype.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/ScriptTimer.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SaveGameFiles.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/VanillaScriptImport.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"
#include "SaveGameSystem.h"
#include "SaveGameVanillaPlaces.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace script_import = openblack::ecs::vanilla_script_import;
using openblack::ecs::systems::vanilla_import::EntityOf;
using openblack::ecs::systems::vanilla_import::GroundPosition;

namespace
{
std::shared_ptr<spdlog::logger> ImportLog()
{
	return spdlog::get("game");
}

/// The flags of the position of a thing that tell that it is in a script and that a script controls it
constexpr uint16_t k_InScriptFlag = 1U << 9U;
constexpr uint16_t k_ControlledFlag = 1U << 10U;

/// The game clears the map before it reads the world of a file. The import removes the markers, the timers, the
/// highlights and the influences of the scripts of the land, before it makes them from the file.
void RemoveScriptThings(ecs::Registry& registry)
{
	std::vector<entt::entity> things;
	registry.Each<const ScriptMarker>([&things](entt::entity entity) { things.push_back(entity); });
	registry.Each<const ScriptTimer>([&things](entt::entity entity, const ScriptTimer&) { things.push_back(entity); });
	registry.Each<const ScriptHighlight>([&things](entt::entity entity, const ScriptHighlight&) { things.push_back(entity); });
	registry.Each<const InfluenceSource>([&things](entt::entity entity, const InfluenceSource&) { things.push_back(entity); });
	for (const auto entity : things)
	{
		if (registry.Valid(entity))
		{
			ecs::world_objects::Remove(entity);
		}
	}
}

/// The temple of a player, if the player has one
std::optional<entt::entity> TempleOf(ecs::Registry& registry, PlayerNames player)
{
	std::optional<entt::entity> found;
	registry.Each<const Temple>([&found, player](entt::entity entity, const Temple& temple) {
		if (!found.has_value() && temple.owner == player)
		{
			found = entity;
		}
	});
	return found;
}
} // namespace

void SaveGameSystem::ImportVanillaScriptThings(const gamesave::VanillaWorld& world, VanillaMade& made)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto things = script_import::ReadScriptThings(world);
	for (const auto& why : things.skipped)
	{
		SPDLOG_LOGGER_WARN(ImportLog(), "The import does not make the script thing {}", why);
	}
	RemoveScriptThings(registry);
	for (const auto& marker : things.markers)
	{
		made.emplace(marker.number, ecs::archetypes::ScriptMarkerArchetype::Create(GroundPosition(marker.position)));
	}
	for (const auto& timer : things.timers)
	{
		const auto entity = registry.Create();
		registry.Assign<ScriptTimer>(entity, script::timers::Timer {.setTurn = timer.setTurn, .turns = timer.turns});
		made.emplace(timer.number, entity);
	}
	uint32_t highlights = 0;
	if (Locator::scriptHighlightSystem::has_value())
	{
		auto& system = Locator::scriptHighlightSystem::value();
		for (const auto& highlight : things.highlights)
		{
			const auto entity =
			    system.Create(static_cast<uint32_t>(highlight.kind), GroundPosition(highlight.position), highlight.text);
			if (entity == entt::null)
			{
				continue;
			}
			system.SetProperties(entity, highlight.text, highlight.category);
			if (highlight.drawHeight.has_value())
			{
				system.SetDrawHeight(entity, *highlight.drawHeight);
			}
			if (highlight.active)
			{
				system.SetActive(entity, true);
			}
			made.emplace(highlight.number, entity);
			++highlights;
		}
	}
	for (const auto& ring : things.rings)
	{
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, GroundPosition(ring.position), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<InfluenceSource>(entity, ring.player, ring.radius, ring.anti, EntityOf(made, ring.follows));
		made.emplace(ring.number, entity);
	}
	// The heart of a temple is a part of the temple of the land
	uint32_t hearts = 0;
	for (const auto* heart : world.OfType(gamesave::VanillaType::CitadelHeart))
	{
		const auto player = script_import::HeartPlayer(world, heart->number);
		const auto temple = player.has_value() ? TempleOf(registry, *player) : std::nullopt;
		if (temple.has_value())
		{
			made.emplace(heart->number, *temple);
			++hearts;
		}
	}
	SPDLOG_LOGGER_INFO(ImportLog(),
	                   "Made {} markers, {} timers, {} highlights and {} influences of the scripts; {} temple hearts",
	                   things.markers.size(), things.timers.size(), highlights, things.rings.size(), hearts);
}

void SaveGameSystem::ImportVanillaScripts(const gamesave::VanillaWorld& world, const VanillaMade& made)
{
	if (!_vanilla.has_value() || !Locator::vm::has_value() || !Locator::scriptObjects::has_value())
	{
		return;
	}
	const auto table = script_import::ReadScriptTable(world);
	const auto path = _vanilla->folder / gamesave::VanillaScriptFileName(_vanilla->slot);
	const auto bytes = ecs::save_files::ReadFile(path);
	if (!table.has_value() || !bytes.has_value())
	{
		SPDLOG_LOGGER_WARN(ImportLog(), "The game's file of slot {} has no table of script objects or no script file",
		                   _vanilla->slot);
		return;
	}
	lhvm::LHVMFile file;
	try
	{
		file.Open(*bytes);
	}
	catch (const std::exception& error)
	{
		SPDLOG_LOGGER_WARN(ImportLog(), "The script file {} does not read: {}", path.generic_string(), error.what());
		return;
	}
	if (!file.HasStatus())
	{
		SPDLOG_LOGGER_WARN(ImportLog(), "The script file {} has no running scripts", path.generic_string());
		return;
	}
	auto imported = script_import::ImportTable(*table, [&made](uint32_t number) -> std::optional<uint32_t> {
		const auto entity = EntityOf(made, number);
		if (entity == entt::null)
		{
			return std::nullopt;
		}
		return entt::to_integral(entity);
	});
	for (const auto number : imported.lost)
	{
		const auto* record = world.Record(number);
		SPDLOG_LOGGER_WARN(ImportLog(), "The scripts hold the object {} of type {}, which the import does not make", number,
		                   record != nullptr ? record->type : 0);
	}
	const auto scripts = script_import::ImportScripts(file, imported.table);
	// The scripts of the land stop and the running scripts of the file take their place, then the file's table of the
	// script objects replaces the table
	if (Locator::vm::value().RestoreState(scripts.file) != EXIT_SUCCESS)
	{
		SPDLOG_LOGGER_WARN(ImportLog(), "The running scripts of {} do not load", path.generic_string());
		return;
	}
	std::vector<ScriptHold> holds;
	for (uint16_t place = 1; place < ecs::script_objects::k_Places; ++place)
	{
		const auto& entry = imported.table.At(place);
		const auto number = table->places.at(place).number;
		const auto* record = number != 0 ? world.Record(number) : nullptr;
		if (!entry.used || entry.object == 0 || record == nullptr)
		{
			continue;
		}
		const auto flags = world.Half(*record, "positionFlags").value_or(0);
		holds.push_back({.object = static_cast<entt::entity>(entry.object),
		                 .inScript = (flags & k_InScriptFlag) != 0,
		                 .controlled = (flags & k_ControlledFlag) != 0});
	}
	std::unordered_set<entt::entity> kept;
	for (const auto& [number, entity] : made)
	{
		kept.insert(entity);
	}
	Locator::scriptObjects::value().PutSavedTable(imported.table, holds,
	                                              [&kept](entt::entity object) { return kept.contains(object); });
	SPDLOG_LOGGER_INFO(ImportLog(),
	                   "Loaded {} script tasks; the table holds {} objects ({} not made, {} gone); {} object values ({} with "
	                   "no object)",
	                   scripts.file.GetTasks().size(), imported.kept, imported.lost.size(), imported.gone, scripts.objects,
	                   scripts.lost);
}
