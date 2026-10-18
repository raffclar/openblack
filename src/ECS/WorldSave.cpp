/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorldSave.h"

#include <cstddef>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <functional>
#include <iterator>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <LHVMFile.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/CameraEdits.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Archetypes/ArenaArchetype.h"
#include "ECS/Components/CreatureArena.h"
#include "ECS/Components/WalkPath.h"
#include "ECS/Map.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FireflySystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/LiquidParticleSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/SavedStateInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "Locator.h"
#include "Particles/LightSheet.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{

std::shared_ptr<spdlog::logger> Log()
{
	return spdlog::get("game");
}

/// The state of a generator of random numbers as text, which the standard library reads back to the same state
std::string GeneratorText(const std::mt19937& generator)
{
	std::ostringstream text;
	text << generator;
	return text.str();
}

/// The block of the game clock and the random numbers: the speed of the clock, whether the player paused it and the time
/// past the last turn, the two
/// seeds of the game, the stream that particle effects draw from, the seed of the C library's generator, the seed of the run
/// and the state of each stream of the run
world_save::PendingChunk ClockChunk()
{
	// The generators are copied now. Their text, which takes long to make, is made on the thread of the writer.
	std::vector<std::mt19937> generators;
	uint32_t runSeed = 0;
	if (Locator::rng::has_value())
	{
		auto& rng = Locator::rng::value();
		runSeed = rng.GetRunSeed();
		for (size_t stream = 0; stream < static_cast<size_t>(RandomStream::Count); ++stream)
		{
			generators.push_back(rng.Stream(static_cast<RandomStream>(stream)));
		}
		// Last, the general stream of the game's thread, which the weather and other systems draw from
		generators.push_back(rng.GeneralStream());
	}
	const entt::meta_ctx context;
	reflection::BinaryWriter out(context);
	reflection::Save(out, Locator::time::has_value() ? Locator::time::value().GetSpeed() : 1.0f);
	reflection::Save(out, Locator::time::has_value() && Locator::time::value().IsPaused());
	reflection::Save(out, Locator::time::has_value() ? Locator::time::value().GetTurnRemainderMs() : 0);
	if (Locator::gameRandom::has_value())
	{
		const auto& random = Locator::gameRandom::value();
		const auto seeds = random.GetSeeds();
		reflection::Save(out, seeds.synced);
		reflection::Save(out, seeds.local);
		reflection::Save(out, random.GetParticleStream());
		reflection::Save(out, random.GetCrtSeed());
	}
	else
	{
		reflection::Save(out, game_random::k_InitialSeed);
		reflection::Save(out, game_random::k_InitialSeed);
		reflection::Save(out, ParticleRandomStream::None);
		reflection::Save(out, std::optional<uint32_t> {});
	}
	reflection::Save(out, runSeed);
	auto head = std::make_shared<std::vector<uint8_t>>(std::move(out.Bytes()));
	auto copies = std::make_shared<const std::vector<std::mt19937>>(std::move(generators));
	return {.tag = gamesave::k_ClockTag, .bytes = [head, copies]() {
		        const entt::meta_ctx later;
		        reflection::BinaryWriter rest(later);
		        rest.Write(*head);
		        std::vector<std::string> streams;
		        std::string general;
		        if (!copies->empty())
		        {
			        std::transform(copies->begin(), copies->end() - 1, std::back_inserter(streams), GeneratorText);
			        general = GeneratorText(copies->back());
		        }
		        reflection::Save(rest, streams);
		        reflection::Save(rest, general);
		        return std::move(rest.Bytes());
	        }};
}

bool LoadClock(std::span<const uint8_t> bytes, uint32_t turn)
{
	const entt::meta_ctx context;
	const reflection::EntityRemap identity;
	reflection::BinaryReader in(context, bytes, identity);
	float speed = 1.0f;
	bool paused = false;
	int32_t remainderMs = 0;
	GameRandomSeeds seeds;
	auto particles = ParticleRandomStream::None;
	std::optional<uint32_t> crtSeed;
	uint32_t runSeed = 0;
	std::vector<std::string> streams;
	reflection::Load(in, speed);
	reflection::Load(in, paused);
	reflection::Load(in, remainderMs);
	reflection::Load(in, seeds.synced);
	reflection::Load(in, seeds.local);
	reflection::Load(in, particles);
	reflection::Load(in, crtSeed);
	reflection::Load(in, runSeed);
	reflection::Load(in, streams);
	// A save of an older build has no general stream
	std::string general;
	if (in.Remaining() > 0)
	{
		reflection::Load(in, general);
	}
	if (in.Failed())
	{
		return false;
	}
	if (Locator::time::has_value())
	{
		auto& time = Locator::time::value();
		time.SetSpeed(speed);
		time.ResumeAtTurn(turn, remainderMs);
		time.SetPaused(paused);
	}
	if (Locator::gameRandom::has_value())
	{
		Locator::gameRandom::value().SetSeeds(seeds);
		Locator::gameRandom::value().SetParticleStream(particles);
		if (crtSeed.has_value())
		{
			Locator::gameRandom::value().CrtSrand(*crtSeed);
		}
	}
	if (Locator::rng::has_value() && streams.size() == static_cast<size_t>(RandomStream::Count))
	{
		auto& rng = Locator::rng::value();
		rng.SetRunSeed(runSeed);
		for (size_t stream = 0; stream < streams.size(); ++stream)
		{
			std::istringstream text(streams[stream]);
			text >> rng.Stream(static_cast<RandomStream>(stream));
		}
		// The new run seed starts the general stream again first; then it takes the saved state
		if (!general.empty())
		{
			std::istringstream text(general);
			text >> rng.GeneralStream();
		}
	}
	return true;
}

gamesave::Chunk CameraChunk()
{
	const entt::meta_ctx context;
	reflection::BinaryWriter out(context);
	const auto& camera = Locator::camera::value();
	reflection::Save(out, camera.GetOrigin());
	reflection::Save(out, camera.GetFocus());
	return {.tag = gamesave::k_CameraTag, .bytes = std::move(out.Bytes())};
}

bool LoadCamera(std::span<const uint8_t> bytes)
{
	const entt::meta_ctx context;
	const reflection::EntityRemap identity;
	reflection::BinaryReader in(context, bytes, identity);
	glm::vec3 origin {0.0f};
	glm::vec3 focus {0.0f};
	reflection::Load(in, origin);
	reflection::Load(in, focus);
	if (in.Failed())
	{
		return false;
	}
	Locator::camera::value().SetOrigin(origin).SetFocus(focus);
	return true;
}

template <typename Service>
systems::SavedStateInterface* FindSystem()
{
	return Service::has_value() ? &Service::value() : nullptr;
}

/// The systems that keep running state of their own, by the names of their sections. A load gives each system its state
/// in this order: the map's lists first, then the systems that use them.
using SystemFinder = systems::SavedStateInterface* (*)();
constexpr std::array<std::pair<std::string_view, SystemFinder>, 31> k_SavedSystems {{
    {"map", &FindSystem<Locator::entitiesMap>},
    {"particles", &FindSystem<Locator::particleSystem>},
    {"magic", &FindSystem<Locator::magicSystem>},
    {"magicShield", &FindSystem<Locator::magicShieldSystem>},
    {"miracleFx", &FindSystem<Locator::miracleFxSystem>},
    {"cameraPath", &FindSystem<Locator::cameraPathSystem>},
    {"liquidParticles", &FindSystem<Locator::liquidParticleSystem>},
    {"creatureLocomotion", &FindSystem<Locator::creatureLocomotionSystem>},
    {"fire", &FindSystem<Locator::fireSystem>},
    {"weather", &FindSystem<Locator::weatherSystem>},
    {"snow", &FindSystem<Locator::snowSystem>},
    {"waterRings", &FindSystem<Locator::waterRingSystem>},
    {"reactions", &FindSystem<Locator::reactionSystem>},
    {"cinematicDirector", &FindSystem<Locator::cinematicDirectorSystem>},
    {"dialogueControl", &FindSystem<Locator::dialogueControlSystem>},
    {"scriptControl", &FindSystem<Locator::scriptControlSystem>},
    {"scriptSwitches", &FindSystem<Locator::chlapi>},
    {"creatureAudio", &FindSystem<Locator::creatureAudioSystem>},
    {"dynamics", &FindSystem<Locator::dynamicsSystem>},
    {"helpText", &FindSystem<Locator::helpTextSystem>},
    {"forest", &FindSystem<Locator::forestSystem>},
    {"teleport", &FindSystem<Locator::teleportSystem>},
    {"animals", &FindSystem<Locator::animalSystem>},
    {"creaturePhysiology", &FindSystem<Locator::creaturePhysiologySystem>},
    {"creatureMind", &FindSystem<Locator::creatureMindSystem>},
    {"guidance", &FindSystem<Locator::guidanceSystem>},
    {"helpMessages", &FindSystem<Locator::helpMessageSystem>},
    {"creatureHelp", &FindSystem<Locator::creatureHelpSystem>},
    {"fireflies", &FindSystem<Locator::fireflySystem>},
    {"explosions", &FindSystem<Locator::explosionSystem>},
    {"influence", &FindSystem<Locator::influenceSystem>},
}};

gamesave::Chunk SystemsChunk(const entt::meta_ctx& reflection)
{
	std::vector<reflection::NamedSection> sections;
	for (const auto& [name, find] : k_SavedSystems)
	{
		if (const auto* system = find(); system != nullptr)
		{
			reflection::BinaryWriter out(reflection);
			out.SetPlace(name, "");
			system->SaveState(out);
			sections.push_back({.name = std::string(name), .bytes = std::move(out.Bytes())});
		}
	}
	return {.tag = gamesave::k_SystemsTag, .bytes = reflection::SaveSections(sections)};
}

bool LoadSystems(const entt::meta_ctx& reflection, std::span<const uint8_t> bytes)
{
	const auto sections = reflection::LoadSections(bytes);
	if (!sections.has_value())
	{
		return false;
	}
	bool read = true;
	const reflection::EntityRemap identity;
	for (const auto& [name, find] : k_SavedSystems)
	{
		auto* system = find();
		const auto section = std::ranges::find(*sections, name, &reflection::NamedSection::name);
		if (system == nullptr || section == sections->end())
		{
			continue;
		}
		reflection::BinaryReader in(reflection, section->bytes, identity);
		if (!system->LoadState(in) || in.Failed() || in.Remaining() != 0)
		{
			SPDLOG_LOGGER_WARN(Log(), "The saved state of the system {} does not read", name);
			read = false;
		}
	}
	return read;
}

/// Lists what the archive did not keep or did not read, one line for each kind
template <typename Range>
void LogNames(std::string_view what, const Range& names)
{
	if (names.empty())
	{
		return;
	}
	std::string joined;
	for (const auto& name : names)
	{
		joined += (joined.empty() ? "" : ", ") + std::string(name);
	}
	SPDLOG_LOGGER_INFO(Log(), "The saved world: {} {}: {}", names.size(), what, joined);
}

/// What the archive of the entities kept, and what it did not keep. The writer's thread logs it.
void LogWorldReport(const reflection::WorldSaveReport& report)
{
	SPDLOG_LOGGER_INFO(Log(), "The saved world keeps {} entities with {} components", report.entities, report.components);
	LogNames("components with no registration", report.unregisteredComponents);
	std::vector<std::string> unsaved;
	for (const auto& value : report.unsavedValues)
	{
		unsaved.push_back(value.place + " (" + value.type + ")");
	}
	LogNames("values that the archive cannot keep", unsaved);
}

} // namespace

std::vector<world_save::PendingChunk> world_save::SaveWorld(const entt::meta_ctx& reflection, SaveSummary& summary)
{
	std::vector<PendingChunk> chunks;
	const auto& registry = Locator::entitiesRegistry::value();
	// The time that each block takes on the game's thread
	auto last = std::chrono::steady_clock::now();
	std::string times;
	const auto timed = [&last, &times, &chunks](PendingChunk chunk) {
		const auto now = std::chrono::steady_clock::now();
		times += fmt::format(" {}={:.1f}", std::string_view(chunk.tag.data(), chunk.tag.size()),
		                     std::chrono::duration<float, std::milli>(now - last).count());
		last = now;
		chunks.push_back(std::move(chunk));
	};
	const auto ready = [](gamesave::Tag tag, std::vector<uint8_t> bytes) {
		return PendingChunk {
		    .tag = tag, .bytes = [made = std::make_shared<const std::vector<uint8_t>>(std::move(bytes))]() { return *made; }};
	};
	// The entities and their components are copied now, and the writer encodes the copy on its own thread
	timed({.tag = gamesave::k_EntitiesTag,
	       .bytes = [snapshot = reflection::SnapshotWorld(registry.Underlying(), reflection), context = &reflection]() {
		       reflection::WorldSaveReport report;
		       auto bytes = reflection::SaveWorld(*snapshot, *context, &report);
		       LogWorldReport(report);
		       return bytes;
	       }});
	timed(ready(gamesave::k_ContextTag, reflection::SaveValue(reflection, registry.Context(), &summary.context)));
	timed(ClockChunk());
	if (Locator::camera::has_value())
	{
		const auto camera = CameraChunk();
		timed(ready(camera.tag, camera.bytes));
	}
	if (Locator::vm::has_value())
	{
		// The scripts' program and state are copied now, and written out as a script file on the thread of the writer
		timed({.tag = gamesave::k_ScriptTag,
		       .bytes = [file = std::make_shared<const lhvm::LHVMFile>(Locator::vm::value().StateFile())]() {
			       std::ostringstream stream(std::ios::binary);
			       file->Write(stream);
			       const auto text = stream.str();
			       return std::vector<uint8_t>(text.begin(), text.end());
		       }});
	}
	if (Locator::scriptObjects::has_value())
	{
		timed(ready(gamesave::k_ScriptObjectsTag, Locator::scriptObjects::value().SaveTable()));
	}
	const auto systems = SystemsChunk(reflection);
	timed(ready(systems.tag, systems.bytes));
	SPDLOG_LOGGER_INFO(Log(), "The saved world's blocks took (ms):{}", times);
	std::vector<std::string> unsaved;
	for (const auto& value : summary.context)
	{
		unsaved.push_back("context " + value.place + " (" + value.type + ")");
	}
	LogNames("values that the archive cannot keep", unsaved);
	return chunks;
}

bool world_save::HasWholeWorld(const gamesave::WorldFile& file)
{
	return gamesave::FindChunk(file, gamesave::k_EntitiesTag).has_value();
}

std::optional<reflection::WorldLoadReport> world_save::LoadEntities(const entt::meta_ctx& reflection,
                                                                    const gamesave::WorldFile& file)
{
	const auto entities = gamesave::FindChunk(file, gamesave::k_EntitiesTag);
	if (!entities.has_value())
	{
		return std::nullopt;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto result = reflection::LoadWorld(registry.Underlying(), reflection, *entities, reflection::WorldLoadMode::Exact);
	if (!result.has_value())
	{
		SPDLOG_LOGGER_WARN(Log(), "The entities of the saved world do not read");
		return std::nullopt;
	}
	if (const auto context = gamesave::FindChunk(file, gamesave::k_ContextTag); context.has_value())
	{
		RegistryContext loaded;
		if (reflection::LoadValue(reflection, *context, loaded, result->remap))
		{
			// The maps that the game works out from the land are made again when they are next needed
			loaded.landAvoid.reset();
			loaded.explorationMap.reset();
			registry.Context() = std::move(loaded);
		}
		else
		{
			SPDLOG_LOGGER_WARN(Log(), "The context of the saved world does not read; the land keeps a new context");
		}
	}
	// What the hand holds stays in the hand: the hand's grab and the held thing are components. The physics takes its
	// bodies back with the running state of the systems.
	// A walk along a camera track takes its track again from the land's tracks, by its number
	std::vector<entt::entity> lost;
	for (auto&& [entity, path] : registry.Underlying().view<components::WalkPath>().each())
	{
		path.track = camera_edits::FindTrack(path.number);
		if (path.track == nullptr)
		{
			lost.push_back(entity);
		}
	}
	registry.Underlying().remove<components::WalkPath>(lost.begin(), lost.end());
	// An arena makes its ring of light again, shown while a fight is on in it
	for (auto&& [entity, arena] : registry.Underlying().view<components::CreatureArena>().each())
	{
		arena.ring = archetypes::ArenaArchetype::MakeRing(arena.place, arena.radius);
		arena.ring->SetHidden(!arena.fightOn);
	}
	SPDLOG_LOGGER_INFO(Log(), "Loaded the saved world: {} entities with {} components", result->report.entities,
	                   result->report.components);
	LogNames("components that this build does not know", result->report.unknownComponents);
	LogNames("fields that this build does not know", result->report.unknownFields);
	LogNames("fields that did not read", result->report.damagedFields);
	LogNames("components that the load could not add", result->report.refusedComponents);
	return result->report;
}

bool world_save::LoadRunningState(const entt::meta_ctx& reflection, const gamesave::WorldFile& file, uint32_t turn)
{
	bool read = true;
	if (const auto clock = gamesave::FindChunk(file, gamesave::k_ClockTag); clock.has_value())
	{
		read = LoadClock(*clock, turn) && read;
	}
	if (const auto camera = gamesave::FindChunk(file, gamesave::k_CameraTag);
	    camera.has_value() && Locator::camera::has_value())
	{
		read = LoadCamera(*camera) && read;
	}
	if (const auto script = gamesave::FindChunk(file, gamesave::k_ScriptTag); script.has_value() && Locator::vm::has_value())
	{
		try
		{
			const std::vector<uint8_t> bytes(script->begin(), script->end());
			read = Locator::vm::value().RestoreState(bytes) == EXIT_SUCCESS && read;
		}
		catch (const std::exception& error)
		{
			SPDLOG_LOGGER_WARN(Log(), "The scripts of the saved world do not read: {}", error.what());
			read = false;
		}
	}
	// The table of the scripts' objects comes after the scripts, which let go of the objects of their old tasks first
	if (const auto table = gamesave::FindChunk(file, gamesave::k_ScriptObjectsTag);
	    table.has_value() && Locator::scriptObjects::has_value() && !Locator::scriptObjects::value().LoadTable(*table))
	{
		SPDLOG_LOGGER_WARN(Log(), "The table of the scripts' objects in the saved world does not fit");
		read = false;
	}
	// The systems take their running state last, with every entity and the clock back
	if (const auto systems = gamesave::FindChunk(file, gamesave::k_SystemsTag); systems.has_value())
	{
		read = LoadSystems(reflection, *systems) && read;
	}
	return read;
}
