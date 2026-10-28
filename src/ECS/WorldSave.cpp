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
#include <mutex>
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
#include "3D/TempleInteriorInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/GameMusic.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "Common/RandomNumberManager.h"
#include "Creature/CreatureHandGlow.h"
#include "ECS/Archetypes/ArenaArchetype.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureArena.h"
#include "ECS/Components/CreatureHandGlows.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/SoundTag.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/WalkPath.h"
#include "ECS/ComputerPlayerStart.h"
#include "ECS/Map.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/SyncedState.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
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
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptMusicSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "Game.h"
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

/// The block of the game clock and the random numbers, from the services of the game
world_save::PendingChunk ClockChunk()
{
	// The generators are copied now. Their text, which takes long to make, is made on the thread of the writer.
	world_save::ClockState state;
	if (Locator::rng::has_value())
	{
		auto& rng = Locator::rng::value();
		state.runSeed = rng.GetRunSeed();
		for (size_t stream = 0; stream < static_cast<size_t>(RandomStream::Count); ++stream)
		{
			state.streams.push_back(rng.Stream(static_cast<RandomStream>(stream)));
		}
		// Last, the general stream of the game's thread, which the weather and other systems draw from
		state.general = rng.GeneralStream();
	}
	if (Locator::time::has_value())
	{
		const auto& time = Locator::time::value();
		state.speed = time.GetSpeed();
		state.paused = time.IsPaused();
		state.remainderMs = time.GetTurnRemainderMs();
	}
	if (Locator::gameRandom::has_value())
	{
		const auto& random = Locator::gameRandom::value();
		state.seeds = random.GetSeeds();
		state.particles = random.GetParticleStream();
		state.crtSeed = random.GetCrtSeed();
		state.syncedDraws = random.GetSyncedDraws();
	}
	auto saved = std::make_shared<const world_save::ClockState>(std::move(state));
	return {.tag = gamesave::k_ClockTag, .bytes = [saved]() { return world_save::WriteClock(*saved); }};
}

bool LoadClock(std::span<const uint8_t> bytes, uint32_t turn)
{
	const auto state = world_save::ReadClock(bytes);
	if (!state.has_value())
	{
		return false;
	}
	if (Locator::time::has_value())
	{
		auto& time = Locator::time::value();
		time.SetSpeed(state->speed);
		time.ResumeAtTurn(turn, state->remainderMs);
		time.SetPaused(state->paused);
	}
	if (Locator::gameRandom::has_value())
	{
		Locator::gameRandom::value().SetSeeds(state->seeds);
		Locator::gameRandom::value().SetParticleStream(state->particles);
		if (state->crtSeed.has_value())
		{
			Locator::gameRandom::value().CrtSrand(*state->crtSeed);
		}
		if (state->syncedDraws.has_value())
		{
			Locator::gameRandom::value().SetSyncedDraws(*state->syncedDraws);
		}
	}
	if (Locator::rng::has_value() && state->streams.size() == static_cast<size_t>(RandomStream::Count))
	{
		auto& rng = Locator::rng::value();
		rng.SetRunSeed(state->runSeed);
		for (size_t stream = 0; stream < state->streams.size(); ++stream)
		{
			rng.Stream(static_cast<RandomStream>(stream)) = state->streams[stream];
		}
		// The new run seed starts the general stream again first; then it takes the saved state
		if (state->general.has_value())
		{
			rng.GeneralStream() = *state->general;
		}
	}
	return true;
}

gamesave::Chunk CameraChunk()
{
	const entt::meta_ctx context;
	reflection::BinaryWriter out(context);
	const auto& camera = Locator::camera::value();
	const auto saved =
	    world_save::CameraToSave(Locator::temple::has_value() ? Locator::temple::value().GetOutsideCamera() : std::nullopt,
	                             {.origin = camera.GetOrigin(), .focus = camera.GetFocus()});
	reflection::Save(out, saved.origin);
	reflection::Save(out, saved.focus);
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

/// The sounds of a world state start again on this computer: the emitters are of the computer that made the state
void ForgetEmitters(Registry& registry)
{
	if (Locator::audio::has_value())
	{
		Locator::audio::value().ForgetSavedEmitters();
	}
	registry.Each<components::SoundTag>([](entt::entity, components::SoundTag& tag) { tag.emitter = entt::null; });
}

/// The rooms of the temple are not a part of the land. The temple makes them again when the player goes into it.
void ForgetTempleRooms(entt::registry& registry)
{
	const auto rooms = registry.view<const components::TempleInteriorPart>();
	registry.destroy(rooms.begin(), rooms.end());
}

/// The sounds that played at the save do not come back, as the game keeps no sound in its saves. The objects' own sounds
/// start again on the next turn, and a creature whose creed powers glow hums again, as a creature that the game loads
/// gets its creed powers one by one.
void ForgetSavedSounds(Registry& registry)
{
	if (Locator::audio::has_value())
	{
		Locator::audio::value().ForgetSavedEmitters();
	}
	if (!Locator::soundTagSystem::has_value())
	{
		return;
	}
	Locator::soundTagSystem::value().ForgetSavedSounds();
	if (!Locator::creatureAnimationSystem::has_value())
	{
		return;
	}
	using creature_hand_glow::k_PlaceCount;
	std::vector<std::pair<entt::entity, std::array<creature_hand_glow::SavedPower, k_PlaceCount>>> creatures;
	registry.Underlying().view<const components::Creature, components::CreatureHandGlows>().each(
	    [&creatures](entt::entity entity, const components::Creature&, components::CreatureHandGlows& glows) {
		    std::array<creature_hand_glow::SavedPower, k_PlaceCount> powers {};
		    for (size_t place = 0; place < k_PlaceCount; ++place)
		    {
			    powers.at(place) = creature_hand_glow::SavedPowerOf(glows.glows, static_cast<creature_hand_glow::Place>(place));
		    }
		    // The loaded creature has no glows before it gets its powers
		    glows.glows = {};
		    glows.motion = {};
		    creatures.emplace_back(entity, powers);
	    });
	for (const auto& [entity, powers] : creatures)
	{
		Locator::creatureAnimationSystem::value().LoadCreedPowers(entity, powers);
	}
}

template <typename Service>
systems::SavedStateInterface* FindSystem()
{
	return Service::has_value() ? &Service::value() : nullptr;
}

/// The game's music, which the game keeps for as long as it runs
systems::SavedStateInterface* FindGameMusic()
{
	auto* game = Game::Instance();
	return game != nullptr ? game->GetGameMusic() : nullptr;
}

/// The systems that keep running state of their own, by the names of their sections. A load gives each system its state
/// in this order: the map's lists first, then the systems that use them.
using SystemFinder = systems::SavedStateInterface* (*)();
constexpr std::array<std::pair<std::string_view, SystemFinder>, 40> k_SavedSystems {{
    {"map", &FindSystem<Locator::entitiesMap>},
    {"particles", &FindSystem<Locator::particleSystem>},
    {"towns", &FindSystem<Locator::townSystem>},
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
    {"scriptMusic", &FindSystem<Locator::scriptMusicSystem>},
    {"music", &FindGameMusic},
    {"dynamics", &FindSystem<Locator::dynamicsSystem>},
    {"helpText", &FindSystem<Locator::helpTextSystem>},
    {"temple", &FindSystem<Locator::temple>},
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
    {"vortex", &FindSystem<Locator::vortexSystem>},
    {"cameraZones", &FindSystem<Locator::cameraZoneSystem>},
    {"cameraHelp", &FindSystem<Locator::cameraHelpSystem>},
    {"scriptHighlights", &FindSystem<Locator::scriptHighlightSystem>},
    {"cameraBookmarks", &FindSystem<Locator::cameraBookmarkSystem>},
}};

gamesave::Chunk SystemsChunk(const entt::meta_ctx& reflection)
{
	return {.tag = gamesave::k_SystemsTag, .bytes = reflection::SaveSections(world_save::SavedSystemSections(reflection))};
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
	// A component with no registration is not in the save, so a load loses it: tell of each name once as an error. The
	// writer's thread calls this, so the names that were told of are kept under a lock.
	static std::mutex toldMutex;
	static synced_state::UnregisteredReporter told;
	std::vector<std::string> fresh;
	{
		const std::lock_guard lock(toldMutex);
		fresh = told.Fresh(report.unregisteredComponents);
	}
	if (!fresh.empty())
	{
		SPDLOG_LOGGER_ERROR(Log(), "{}", synced_state::UnregisteredLine(fresh));
	}
	std::vector<std::string> unsaved;
	for (const auto& value : report.unsavedValues)
	{
		unsaved.push_back(value.place + " (" + value.type + ")");
	}
	LogNames("values that the archive cannot keep", unsaved);
}

} // namespace

std::vector<reflection::NamedSection> world_save::SavedSystemSections(const entt::meta_ctx& reflection, bool valueEnds)
{
	std::vector<reflection::NamedSection> sections;
	for (const auto& [name, find] : k_SavedSystems)
	{
		if (const auto* system = find(); system != nullptr)
		{
			reflection::BinaryWriter out(reflection);
			if (valueEnds)
			{
				out.KeepValueEnds();
			}
			out.SetPlace(name, "");
			system->SaveState(out);
			sections.push_back({.name = std::string(name), .bytes = std::move(out.Bytes()), .valueEnds = out.ValueEnds()});
		}
	}
	return sections;
}

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
		// The scripts' running state is copied now. The VM shares its copy of the program, which does not change. The
		// thread of the writer puts them together into a script file.
		const auto& vm = Locator::vm::value();
		timed({.tag = gamesave::k_ScriptTag,
		       .bytes = [program = vm.ProgramFile(),
		                 state = std::make_shared<const lhvm::LHVM::RunningState>(vm.CopyRunningState())]() {
			       std::ostringstream stream(std::ios::binary);
			       lhvm::LHVM::StateFile(*program, *state).Write(stream);
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
                                                                    const gamesave::WorldFile& file, LoadKind kind)
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
			// The creatures' map of the land comes from the land and its towns at the start of the land, and the towns
			// changed after. The load keeps the saved map, so that the creatures find the same places as before the save.
			registry.Context() = std::move(loaded);
		}
		else
		{
			SPDLOG_LOGGER_WARN(Log(), "The context of the saved world does not read; the land keeps a new context");
		}
	}
	// Things the save does not keep go back to their start. A world state keeps them.
	if (kind == LoadKind::SavedGame)
	{
		ForgetUnsavedState(registry.Underlying());
	}
	else
	{
		ForgetTempleRooms(registry.Underlying());
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
	// A dance takes its file again, which the save does not keep
	TakeDanceFilesAgain(registry.Underlying(), [](DanceInfo type) {
		return Locator::danceSystem::has_value() ? Locator::danceSystem::value().FileOf(type) : nullptr;
	});
	// An arena makes its ring of light again, shown while a fight is on in it
	for (auto&& [entity, arena] : registry.Underlying().view<components::CreatureArena>().each())
	{
		arena.ring = archetypes::ArenaArchetype::MakeRing(arena.place, arena.radius);
		arena.ring->SetHidden(!arena.fightOn);
	}
	if (kind == LoadKind::SavedGame)
	{
		ForgetSavedSounds(registry);
	}
	else
	{
		ForgetEmitters(registry);
	}
	SPDLOG_LOGGER_INFO(Log(), "Loaded the saved world: {} entities with {} components", result->report.entities,
	                   result->report.components);
	LogNames("components that this build does not know", result->report.unknownComponents);
	LogNames("fields that this build does not know", result->report.unknownFields);
	LogNames("fields that did not read", result->report.damagedFields);
	LogNames("components that the load could not add", result->report.refusedComponents);
	return result->report;
}

std::vector<uint8_t> world_save::WriteClock(const ClockState& state)
{
	const entt::meta_ctx context;
	reflection::BinaryWriter out(context);
	reflection::Save(out, state.speed);
	reflection::Save(out, state.paused);
	reflection::Save(out, state.remainderMs);
	reflection::Save(out, state.seeds.synced);
	reflection::Save(out, state.seeds.local);
	reflection::Save(out, state.particles);
	reflection::Save(out, state.crtSeed);
	reflection::Save(out, state.runSeed);
	std::vector<std::string> streams;
	std::ranges::transform(state.streams, std::back_inserter(streams), GeneratorText);
	reflection::Save(out, streams);
	reflection::Save(out, state.general.has_value() ? GeneratorText(*state.general) : std::string {});
	reflection::Save(out, state.syncedDraws.value_or(0));
	return std::move(out.Bytes());
}

std::optional<world_save::ClockState> world_save::ReadClock(std::span<const uint8_t> bytes)
{
	const entt::meta_ctx context;
	const reflection::EntityRemap identity;
	reflection::BinaryReader in(context, bytes, identity);
	ClockState state;
	std::vector<std::string> streams;
	reflection::Load(in, state.speed);
	reflection::Load(in, state.paused);
	reflection::Load(in, state.remainderMs);
	reflection::Load(in, state.seeds.synced);
	reflection::Load(in, state.seeds.local);
	reflection::Load(in, state.particles);
	reflection::Load(in, state.crtSeed);
	reflection::Load(in, state.runSeed);
	reflection::Load(in, streams);
	// A save of an older build has no general stream
	std::string general;
	if (in.Remaining() > 0)
	{
		reflection::Load(in, general);
	}
	// Nor the count of the synced draws
	if (in.Remaining() > 0)
	{
		uint64_t draws = 0;
		reflection::Load(in, draws);
		state.syncedDraws = draws;
	}
	if (in.Failed())
	{
		return std::nullopt;
	}
	for (const auto& text : streams)
	{
		std::istringstream read(text);
		read >> state.streams.emplace_back();
	}
	if (!general.empty())
	{
		std::istringstream read(general);
		read >> state.general.emplace();
	}
	return state;
}

OutsideCamera world_save::CameraToSave(const std::optional<OutsideCamera>& outside, const OutsideCamera& current)
{
	return outside.value_or(current);
}

void world_save::ForgetUnsavedState(entt::registry& registry)
{
	registry.view<components::CreatureMindState>().each([](components::CreatureMindState& mind) {
		mind.leftAreaActions.clear();
		mind.agendaRefused = false;
	});
	// A footpath that the game still looks for starts its search again from its two ends, as the game loads it
	registry.view<components::FootpathFinder>().each([](components::FootpathFinder& finder) { finder.plan.reset(); });
	// A save from inside the temple has the rooms of the temple, so the load removes them
	ForgetTempleRooms(registry);
	// The mind of each computer god starts again. It keeps only the parts that a saved game keeps.
	registry.view<components::ComputerPlayer>().each(
	    [](components::ComputerPlayer& player) { computer_player::StartAgainAfterLoad(player); });
}

void world_save::TakeDanceFilesAgain(entt::registry& registry,
                                     const std::function<std::shared_ptr<const dance::DanceFile>(DanceInfo)>& fileOf)
{
	for (auto&& [entity, dance] : registry.view<components::Dance>().each())
	{
		dance.file = fileOf(dance.type);
	}
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
	// The help shows its kept texts again, and the temple gives back the dialogue it kept, as the game does once every
	// part of a saved game has loaded
	if (Locator::temple::has_value())
	{
		Locator::temple::value().ShowHelpAfterLoad();
	}
	else if (Locator::helpTextSystem::has_value())
	{
		Locator::helpTextSystem::value().ShowTextAgain();
	}
	return read;
}
