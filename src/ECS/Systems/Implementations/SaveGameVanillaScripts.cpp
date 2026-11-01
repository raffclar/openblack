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
#include <memory>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <LHVM.h>
#include <SaveSlots.h>
#include <VanillaWorld.h>
#include <glm/mat3x3.hpp>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Archetypes/ScriptMarkerArchetype.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/ScriptTimer.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SaveGameFiles.h"
#include "ECS/ScriptSpotVisuals.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/VanillaParticleImport.h"
#include "ECS/VanillaScriptImport.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"
#include "Particles/ParticleImport.h"
#include "SaveGameSystem.h"
#include "SaveGameVanillaPlaces.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace script_import = openblack::ecs::vanilla_script_import;
namespace particle_import = openblack::ecs::vanilla_particle_import;
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

/// The effect that the import started for a particle container of the file, if any
std::optional<uint32_t> EffectOfContainer(const std::unordered_map<uint32_t, uint32_t>& effects, uint32_t number)
{
	const auto found = effects.find(number);
	return found != effects.end() ? std::optional(found->second) : std::nullopt;
}

/// The temple takes the values of its heart in the file: the life, the turn of the last hit, the town that it was made
/// for, the last beam of a hurt heart, the clocks of a destruction under way, and the effects of its beams and of the
/// glow of a destruction, which continue from their saved state
void PutHeart(ecs::Registry& registry, entt::entity entity, const script_import::ImportedTemple& file,
              const std::unordered_map<uint32_t, entt::entity>& made, const std::unordered_map<uint32_t, uint32_t>& effects)
{
	registry.AssignOrReplace<ObjectLife>(entity, ObjectLife {.life = file.life});
	auto& temple = registry.Get<Temple>(entity);
	temple.lastHitTurn = file.lastHitTurn;
	if (const auto town = EntityOf(made, file.town); town != entt::null && registry.AllOf<Town>(town))
	{
		temple.town = static_cast<int32_t>(registry.Get<const Town>(town).id);
	}
	else if (file.town == 0)
	{
		temple.town.reset();
	}
	temple.beamTarget = EntityOf(made, file.beamTarget);
	temple.beamTurn = file.beamTurn;
	temple.destroying = file.destroying;
	temple.destructionClock = file.destructionClock;
	temple.destructionBeamClock = file.destructionBeamClock;
	temple.beamSource = EffectOfContainer(effects, file.beams);
	temple.destructionBeamSource = EffectOfContainer(effects, file.destructionBeams);
	temple.destructionGlow = EffectOfContainer(effects, file.destructionGlow);
}

/// Starts the effect of a particle container of the file from its saved state. The effect follows the thing of the
/// file and keeps its turns left. Gives the number of the effect, none when no effect started.
std::optional<uint32_t> StartContainer(const particle_import::ImportedContainer& container,
                                       const std::unordered_map<uint32_t, entt::entity>& made)
{
	auto& effects = Locator::effectRequestSystem::value();
	auto state = std::make_shared<particles::RestoredEffect>(container.effect.state);
	// Each step of the game gives the effect the strength of its container
	state->info.power = container.scale;
	const auto origin = state->origin;
	const auto handle = effects.Start({
	    .type = container.effect.type,
	    .position = origin,
	    .synced = container.effect.synced,
	    .turns = container.turns,
	    .owner = EntityOf(made, container.follows),
	    .drawPath = container.singleSort ? particles::draw::DrawPath::Queued : particles::draw::DrawPath::Sorted,
	    .restored = std::move(state),
	});
	if (!effects.IsRunning(handle))
	{
		SPDLOG_LOGGER_WARN(ImportLog(), "The effect of the particle container {} of the file does not start", container.number);
		return std::nullopt;
	}
	return handle;
}

/// Makes the particle containers of the file, the oldest first. A container of a temple heart is a part of the temple;
/// each other container becomes a thing that holds its effect, as the things of the visuals that scripts start.
std::unordered_map<uint32_t, uint32_t> PutContainers(const gamesave::VanillaWorld& world, ecs::Registry& registry,
                                                     const std::vector<script_import::ImportedTemple>& temples,
                                                     std::unordered_map<uint32_t, entt::entity>& made)
{
	std::unordered_map<uint32_t, uint32_t> effects;
	if (!Locator::effectRequestSystem::has_value())
	{
		return effects;
	}
	std::unordered_set<uint32_t> hearts;
	for (const auto& temple : temples)
	{
		hearts.insert({temple.beams, temple.destructionBeams, temple.destructionGlow});
	}
	// The game makes the flames of a bonfire as a particle container that follows it, and a load makes no new flames.
	// A bonfire that the import made or kept loses the flames that it got as it was made; the containers of the file
	// give the flames back from their saved state.
	for (const auto& [number, entity] : made)
	{
		const auto* still = registry.TryGet<MobileStatic>(entity);
		if (still != nullptr && still->type == MobileStaticInfo::Bonfire)
		{
			Locator::effectRequestSystem::value().RemoveOwnedBy(entity);
		}
	}
	const auto read = particle_import::ReadContainers(world, [&made](uint32_t number) { return EntityOf(made, number); });
	for (const auto& why : read.skipped)
	{
		SPDLOG_LOGGER_WARN(ImportLog(), "The import does not make the particle container {}", why);
	}
	for (const auto& container : read.containers)
	{
		const auto handle = StartContainer(container, made);
		if (!handle.has_value())
		{
			continue;
		}
		effects.emplace(container.number, *handle);
		if (!hearts.contains(container.number))
		{
			made.emplace(container.number,
			             ecs::script_spot_visuals::MakeThing(registry, *handle, GroundPosition(container.position)));
		}
	}
	return effects;
}

/// Makes the vortices of the file that draw things in, with the effect that draws things in from its saved state
uint32_t PutVorticesIn(const gamesave::VanillaWorld& world, std::unordered_map<uint32_t, entt::entity>& made)
{
	if (!Locator::vortexSystem::has_value())
	{
		return 0;
	}
	const auto read = particle_import::ReadVorticesIn(world, [&made](uint32_t number) { return EntityOf(made, number); });
	for (const auto& why : read.skipped)
	{
		SPDLOG_LOGGER_WARN(ImportLog(), "The import does not make the vortex {}", why);
	}
	uint32_t count = 0;
	for (const auto& file : read.vortices)
	{
		const auto vortex = Locator::vortexSystem::value().Restore({
		    .position = file.centre,
		    .type = VortexType::In,
		    .state = file.state,
		    .altitude = file.altitude,
		    .groundHeights = file.groundHeights,
		    .groundAverage = file.groundAverage,
		    .mover = file.mover.has_value() ? std::make_shared<particles::RestoredEffect>(file.mover->state) : nullptr,
		    .crossing = file.crossing,
		});
		if (vortex != entt::null)
		{
			made.emplace(file.number, vortex);
			++count;
		}
	}
	return count;
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

/// The values of the scripts that stay after a load: the entry to the temple and the level of the interface. As the
/// game's load does, the import sets the interface to the level of the file again.
void PutScriptValues(const gamesave::VanillaWorld& world, ecs::Registry& registry)
{
	const auto values = script_import::ReadScriptValues(world);
	if (!values.has_value())
	{
		SPDLOG_LOGGER_WARN(ImportLog(), "The game object of the file has no values of the scripts");
		return;
	}
	registry.Context().scriptLetsTempleBeEntered = values->templeEntry;
	if (Locator::cameraHelpSystem::has_value() && !chlapi::CHLApi::ApplyInterfaceLevel(values->interfaceLevel))
	{
		SPDLOG_LOGGER_WARN(ImportLog(), "The level {} of the interface of the file is not a level of the game",
		                   values->interfaceLevel);
	}
	if (values->countdownOn)
	{
		SPDLOG_LOGGER_WARN(ImportLog(), "The import does not make the countdown timer of the scripts ({} left)",
		                   values->countdownLeft);
	}
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
			const auto entity = system.Restore({.kind = static_cast<uint32_t>(highlight.kind),
			                                    .at = GroundPosition(highlight.position),
			                                    .text = highlight.text,
			                                    .category = highlight.category,
			                                    .active = highlight.active,
			                                    .drawHeight = highlight.drawHeight});
			if (entity == entt::null)
			{
				continue;
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
	const auto temples = script_import::ReadTemples(world);
	const auto containers = PutContainers(world, registry, temples, made);
	const auto vortices = PutVorticesIn(world, made);
	// The heart of a temple is a part of the temple of the land, and the temple takes the values of the heart
	uint32_t hearts = 0;
	for (const auto& file : temples)
	{
		const auto temple = TempleOf(registry, file.player);
		if (!temple.has_value())
		{
			continue;
		}
		made.emplace(file.heart, *temple);
		PutHeart(registry, *temple, file, made, containers);
		++hearts;
	}
	SPDLOG_LOGGER_INFO(ImportLog(),
	                   "Made {} markers, {} timers, {} highlights and {} influences of the scripts; {} particle containers, {} "
	                   "vortices and {} temple hearts",
	                   things.markers.size(), things.timers.size(), highlights, things.rings.size(), containers.size(),
	                   vortices, hearts);
}

void SaveGameSystem::ImportVanillaScripts(const gamesave::VanillaWorld& world, const VanillaMade& made)
{
	if (!_vanilla.has_value() || !Locator::vm::has_value() || !Locator::scriptObjects::has_value())
	{
		return;
	}
	PutScriptValues(world, Locator::entitiesRegistry::value());
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
