/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed runner: the crowds of a benchmark and the measure of its frames.

#include <ctime>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <glm/trigonometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Commands/PlayerCommands.h"
#include "Common/PortableMath.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureScriptPlay.h"
#include "Debug/TestbedMultiplayer.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/TownBelief.h"
#include "Sound/GameMusic.h"
#include "TestbedScenarioRunner.h"
#include "TestbedScenarioRunnerParts.h"
#include "Visual/RendererRequestsInterface.h"
#include "Windowing/WindowingInterface.h"

#include "../Profiler.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::runner_parts;
using openblack::ecs::archetypes::CreatureArchetype;
using openblack::ecs::components::Creature;
using openblack::ecs::components::Transform;

namespace
{
/// The live results of a benchmark are summed up again every so many frames
constexpr uint32_t k_FramesPerSummary = 30;

/// What the store of a crowd's town starts with
constexpr uint32_t k_CrowdFood = 2000;

constexpr uint32_t k_CrowdWood = 2000;

double Milliseconds(std::chrono::steady_clock::duration duration)
{
	return std::chrono::duration<double, std::milli>(duration).count();
}

#ifdef NDEBUG
constexpr std::string_view k_Build = "optimised";
#else
constexpr std::string_view k_Build = "debug";
#endif
} // namespace

namespace openblack::testbed_scenarios::runner_parts
{
/// The profiler's stages as benchmarks name them: the drawing of the reflection and of the main pass told apart, and
/// everything from drawing the scene on counted as rendering
std::vector<benchmark::StageInfo> BenchmarkStages()
{
	static const std::vector<std::string> k_Names = [] {
		std::vector<std::string> names;
		for (size_t i = 0; i < Profiler::k_StageNames.size(); ++i)
		{
			const auto stage = static_cast<Profiler::Stage>(i);
			std::string name(Profiler::k_StageNames.at(i));
			if (stage > Profiler::Stage::ReflectionPass && stage <= Profiler::Stage::ReflectionDrawParticles)
			{
				name = "Reflection " + name;
			}
			else if (stage > Profiler::Stage::MainPass && stage <= Profiler::Stage::MainPassDrawParticles)
			{
				name = "Main " + name;
			}
			names.push_back(std::move(name));
		}
		return names;
	}();
	std::vector<benchmark::StageInfo> stages;
	for (size_t i = 0; i < k_Names.size(); ++i)
	{
		stages.push_back({.name = k_Names.at(i), .render = static_cast<Profiler::Stage>(i) >= Profiler::Stage::SceneDraw});
	}
	return stages;
}
} // namespace openblack::testbed_scenarios::runner_parts

std::optional<CrowdProgress> Runner::GetCrowdProgress() const
{
	if (_scenario == nullptr || !_scenario->crowd.has_value())
	{
		return std::nullopt;
	}
	return _crowdProgress;
}

std::span<const benchmark::StageInfo> Runner::GetStages() const
{
	if (_recorder == nullptr)
	{
		return {};
	}
	return _recorder->Stages();
}

uint32_t Runner::GetWarmUpLeft() const
{
	return _settledFrames >= _benchmark.warmUpFrames ? 0 : _benchmark.warmUpFrames - _settledFrames;
}

void Runner::SpawnCrowd()
{
	if (_scenario == nullptr || !_scenario->crowd.has_value() || _crowdProgress.Done() ||
	    !Locator::terrainSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto start = std::chrono::steady_clock::now();
	const auto batch = std::min(_scenario->crowd->perFrame, _crowdProgress.total - _crowdNext);
	for (size_t i = 0; i < batch; ++i)
	{
		SpawnCrowdMember(_crowdNext++);
	}
	_crowdProgress.spawned = _crowdNext;
	_crowdProgress.spawnMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	++_crowdProgress.spawnFrames;
	if (_crowdProgress.Done())
	{
		Log(fmt::format("Spawned {} in {:.0f} ms over {} frames", _crowdProgress.total, _crowdProgress.spawnMs,
		                _crowdProgress.spawnFrames));
	}
}

void Runner::SpawnCrowdMember(size_t index)
{
	const auto& land = Locator::terrainSystem::value();
	const auto onLand = [&land, this](glm::vec2 offset) {
		const auto point = MapPoint(_middle, offset);
		return glm::vec3(point.x, land.GetHeightAt(point), point.y);
	};
	// The towns first, then their homes and stores, then the villagers who live in them, then the creatures
	const auto towns = _village.towns.size();
	const auto abodes = _village.abodes.size();
	const auto village = towns + abodes + _village.villagers.size();
	if (index >= village)
	{
		const auto& member = _crowdCreatures.at(index - village);
		_crowdEntities.push_back(CreatureArchetype::Create(onLand(member.offset), member.owner, member.species, 0,
		                                                   glm::radians(member.facingDegrees), k_DefaultSize,
		                                                   CreatureArchetype::StartBody(member.species)));
		return;
	}
	if (index < towns)
	{
		const auto& town = _village.towns.at(index);
		_crowdEntities.push_back(
		    ecs::archetypes::TownArchetype::Create(static_cast<int>(index), onLand(town.offset), town.owner, town.tribe));
		return;
	}
	if (index < towns + abodes)
	{
		const auto& abode = _village.abodes.at(index - towns);
		_crowdAbodes.push_back(ecs::archetypes::AbodeArchetype::Create(static_cast<uint32_t>(abode.town), onLand(abode.offset),
		                                                               abode.type, glm::radians(abode.yawDegrees), 1.0f,
		                                                               k_CrowdFood, k_CrowdWood));
		return;
	}
	const auto& member = _village.villagers.at(index - towns - abodes);
	const auto& home = _village.abodes.at(member.abode);
	const auto entity =
	    ecs::archetypes::VillagerArchetype::Create(onLand(home.offset), onLand(member.offset), member.type, member.age);
	_crowdEntities.push_back(entity);
	// It lives in the home laid out for it, rather than the first in its town with room
	auto& registry = Locator::entitiesRegistry::value();
	const auto abode = _crowdAbodes.at(member.abode);
	const auto& townIds = registry.Context().towns;
	const auto town = townIds.find(static_cast<uint32_t>(home.town));
	if (abode != entt::null && registry.Valid(abode) && town != townIds.end())
	{
		Locator::townSystem::value().AddVillagerToAbode(abode, entity);
		registry.Get<ecs::components::Villager>(entity).town = town->second;
	}
}

void Runner::SetBenchmarkReads(bool reads)
{
	if (Locator::profiler::has_value())
	{
		Locator::profiler::value().SetReader(Profiler::Reader::Benchmark, reads);
	}
}

void Runner::Measure()
{
	if (_recorder == nullptr || !_crowdProgress.Done() || !Locator::profiler::has_value())
	{
		return;
	}
	if (_settledFrames < _benchmark.warmUpFrames)
	{
		++_settledFrames;
		return;
	}
	// The last frame, whole: it ended as this one started
	const auto& profiler = Locator::profiler::value();
	const auto& entry = profiler.GetEntries().at(profiler.GetEntryIndex(-1));
	const auto frame = entry.frameEnd - entry.frameStart;
	const auto& draw = entry.stages.at(static_cast<size_t>(Profiler::Stage::SceneDraw));
	const bool drawn = draw.finalized && draw.start >= entry.frameStart && draw.start <= entry.frameEnd;
	const auto update = drawn ? draw.start - entry.frameStart : frame;
	std::array<float, static_cast<size_t>(Profiler::Stage::_count)> stages {};
	for (size_t i = 0; i < stages.size(); ++i)
	{
		stages.at(i) = static_cast<float>(Milliseconds(entry.stages.at(i).total));
	}
	// What the renderer last drew, the frame before
	const auto draws = Locator::rendererRequests::has_value() ? Locator::rendererRequests::value().GetDrawsOfLastFrame() : 0u;
	_recorder->Add(static_cast<float>(Milliseconds(frame)), static_cast<float>(Milliseconds(update)),
	               static_cast<float>(Milliseconds(frame - update)), stages, static_cast<float>(draws));
	if (++_framesSinceSummary >= k_FramesPerSummary)
	{
		_framesSinceSummary = 0;
		_liveResults = _recorder->SummariseNewest(Runner::k_LiveFrames);
	}
	if (_benchmark.autoSave.has_value() && !_saved && _recorder->Count() >= _recorder->Capacity())
	{
		_saved = true;
		_liveResults = _recorder->Summarise();
		const auto written = SaveResults(_benchmark.autoSave);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Benchmark {}: frame mean {:.2f} ms p95 {:.2f} max {:.2f}; results {}",
		                   _scenario->id, _liveResults.frame.average, _liveResults.frame.p95, _liveResults.frame.max,
		                   written.has_value() ? written->generic_string() : "not written");
		if (_stage != nullptr)
		{
			_stage->RequestQuit();
		}
	}
}

std::vector<std::pair<std::string, size_t>> Runner::EntityCounts() const
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return {};
	}
	auto& registry = Locator::entitiesRegistry::value();
	return {
	    {"placed", registry.Size<Transform>()},
	    {"creatures", registry.Size<Creature>()},
	    {"villagers", registry.Size<ecs::components::Villager>()},
	    {"abodes", registry.Size<ecs::components::Abode>()},
	    {"towns", registry.Size<ecs::components::Town>()},
	};
}

std::optional<std::filesystem::path> Runner::SaveResults(std::optional<std::filesystem::path> base)
{
	if (_scenario == nullptr || _recorder == nullptr || _recorder->Count() == 0)
	{
		return std::nullopt;
	}
	if (!base.has_value())
	{
		const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		std::tm local {};
#ifdef _WIN32
		localtime_s(&local, &now);
#else
		localtime_r(&now, &local);
#endif
		base = std::filesystem::path("benchmarks") / fmt::format("{}_{}_{:04}{:02}{:02}_{:02}{:02}{:02}", _scenario->id,
		                                                         k_Build, local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
		                                                         local.tm_hour, local.tm_min, local.tm_sec);
	}
	benchmark::RunInfo run {
	    .scenarioId = std::string(_scenario->id),
	    .scenarioName = std::string(_scenario->name),
	    .build = std::string(k_Build),
	    .crowd = _crowdProgress.total,
	    .spawnMs = _crowdProgress.spawnMs,
	    .spawnFrames = _crowdProgress.spawnFrames,
	    .warmUpFrames = _benchmark.warmUpFrames,
	    .entityCounts = EntityCounts(),
	};
	if (Locator::windowing::has_value())
	{
		const auto size = Locator::windowing::value().GetSize();
		run.width = static_cast<uint32_t>(size.x);
		run.height = static_cast<uint32_t>(size.y);
	}
	std::error_code error;
	if (base->has_parent_path())
	{
		std::filesystem::create_directories(base->parent_path(), error);
	}
	auto jsonPath = *base;
	jsonPath += ".json";
	auto csvPath = *base;
	csvPath += ".csv";
	std::ofstream json(jsonPath, std::ios::binary);
	std::ofstream csv(csvPath, std::ios::binary);
	if (!json || !csv)
	{
		Log(fmt::format("Couldn't write {}", jsonPath.generic_string()));
		return std::nullopt;
	}
	const auto results = _recorder->Summarise();
	json << benchmark::ToJson(run, results, _recorder->Stages());
	csv << benchmark::ToCsv(run, results, _recorder->Stages());
	Log(fmt::format("Saved {}", jsonPath.generic_string()));
	return jsonPath;
}
