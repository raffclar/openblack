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

#include <exception>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <spdlog/spdlog.h>

#include "3D/CameraEdits.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/WalkPath.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"

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
/// seeds of the game, the stream that particle effects draw from, the seed of the run and the state of each stream of the
/// run
gamesave::Chunk ClockChunk()
{
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
	}
	else
	{
		reflection::Save(out, game_random::k_InitialSeed);
		reflection::Save(out, game_random::k_InitialSeed);
		reflection::Save(out, ParticleRandomStream::None);
	}
	std::vector<std::string> streams;
	uint32_t runSeed = 0;
	if (Locator::rng::has_value())
	{
		auto& rng = Locator::rng::value();
		runSeed = rng.GetRunSeed();
		for (size_t stream = 0; stream < static_cast<size_t>(RandomStream::Count); ++stream)
		{
			streams.push_back(GeneratorText(rng.Stream(static_cast<RandomStream>(stream))));
		}
	}
	reflection::Save(out, runSeed);
	reflection::Save(out, streams);
	return {.tag = gamesave::k_ClockTag, .bytes = std::move(out.Bytes())};
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
	uint32_t runSeed = 0;
	std::vector<std::string> streams;
	reflection::Load(in, speed);
	reflection::Load(in, paused);
	reflection::Load(in, remainderMs);
	reflection::Load(in, seeds.synced);
	reflection::Load(in, seeds.local);
	reflection::Load(in, particles);
	reflection::Load(in, runSeed);
	reflection::Load(in, streams);
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

} // namespace

std::vector<gamesave::Chunk> world_save::SaveWorld(const entt::meta_ctx& reflection, SaveSummary& summary)
{
	std::vector<gamesave::Chunk> chunks;
	const auto& registry = Locator::entitiesRegistry::value();
	chunks.push_back(
	    {.tag = gamesave::k_EntitiesTag, .bytes = reflection::SaveWorld(registry.Underlying(), reflection, &summary.world)});
	chunks.push_back(
	    {.tag = gamesave::k_ContextTag, .bytes = reflection::SaveValue(reflection, registry.Context(), &summary.context)});
	chunks.push_back(ClockChunk());
	if (Locator::camera::has_value())
	{
		chunks.push_back(CameraChunk());
	}
	if (Locator::vm::has_value())
	{
		chunks.push_back({.tag = gamesave::k_ScriptTag, .bytes = Locator::vm::value().SaveState()});
	}
	if (Locator::scriptObjects::has_value())
	{
		chunks.push_back({.tag = gamesave::k_ScriptObjectsTag, .bytes = Locator::scriptObjects::value().SaveTable()});
	}
	SPDLOG_LOGGER_INFO(Log(), "The saved world keeps {} entities with {} components", summary.world.entities,
	                   summary.world.components);
	LogNames("components with no registration", summary.world.unregisteredComponents);
	std::vector<std::string> unsaved;
	for (const auto& value : summary.world.unsavedValues)
	{
		unsaved.push_back(value.place + " (" + value.type + ")");
	}
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
			registry.Context() = std::move(loaded);
		}
		else
		{
			SPDLOG_LOGGER_WARN(Log(), "The context of the saved world does not read; the land keeps a new context");
		}
	}
	// The physics and the hand do not keep their bodies and held things in a save: what was in flight or in the hand
	// stays where it was
	registry.Underlying().clear<components::InPhysics, components::InHand>();
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
	SPDLOG_LOGGER_INFO(Log(), "Loaded the saved world: {} entities with {} components", result->report.entities,
	                   result->report.components);
	LogNames("components that this build does not know", result->report.unknownComponents);
	LogNames("fields that this build does not know", result->report.unknownFields);
	LogNames("fields that did not read", result->report.damagedFields);
	LogNames("components that the load could not add", result->report.refusedComponents);
	return result->report;
}

bool world_save::LoadRunningState(const gamesave::WorldFile& file, uint32_t turn)
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
	return read;
}
