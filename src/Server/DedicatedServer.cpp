/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DedicatedServer.h"

#include <charconv>
#include <cstdlib>
#include <ctime>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>

#include <fmt/format.h>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "3D/TempleInteriorInterface.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#if defined(OPENBLACK_DEVELOPER_TOOLS)
#include "Common/DrawTrace.h"
#endif
#include "Common/GameRandom.h"
#include "Common/MachineClock.h"
#include "Common/RandomNumberManager.h"
#include "Debug/TestbedScenarioRunner.h"
#include "ECS/ChallengePictures.h"
#include "ECS/Components/Temple.h"
#include "ECS/GameEndConditions.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Registry.h"
#include "ECS/SaveFileWriter.h"
#include "ECS/StatisticsRules.h"
#include "ECS/SyncedState.h"
#include "ECS/Systems/CreatureWebPageSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/InspectorSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/MusicMoodSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/SimulationSystemInterface.h"
#include "ECS/Systems/SyncChecksumSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Gui/GameTexts.h"
#include "Gui/MenuSettings.h"
#include "Gui/MultiplayerFrontEndRules.h"
#include "Gui/OnlineMapTexts.h"
#include "Gui/TextDatabase.h"
#include "Locator.h"
#include "Online/NetworkLand.h"
#include "Online/OnlineMapLand.h"
#include "PlayerSlots.h"
#include "Profiler.h"
#include "Resources/ResourcesInterface.h"
#include "Simulation/CommandLog.h"
#include "Simulation/FrameBody.h"
#include "Simulation/ProfileMenu.h"
#include "Simulation/TurnBody.h"
#include "Visual/RendererRequestsInterface.h"

// The game role has sockets and TLS, which only the desktop builds have
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
#define OPENBLACK_GAME_ROLE 1
#include "Online/StartRules.h"
#include "Server/Desktop/GameRole.h"
#endif

namespace openblack::server
{
namespace
{

constexpr std::string_view k_TurnWord = "turn ";
constexpr std::string_view k_ChecksumWord = " checksum ";

/// Logs that the server ran with no graphics library. The server program does not contain the library, so no part of
/// the world can start it.
void LogGraphicsLibrary()
{
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The server ran with no graphics library");
}

/// The world of the server. It starts the services of the world, loads the land and plays the frames of the world,
/// with no window and no view of a player.
class ServerWorld final:
    public simulation::WorldDataViewInterface,
    public simulation::PlayStartViewInterface,
    public simulation::TurnViewInterface
{
public:
	ServerWorld(const ServerSettings& settings, StartDevices startDevices)
	    : _settings(settings)
	    , _startDevices(std::move(startDevices))
	    , _simulation(SimulationFor(settings))
	{
	}

	ServerWorld(const ServerWorld&) = delete;
	ServerWorld& operator=(const ServerWorld&) = delete;
	ServerWorld(ServerWorld&&) = delete;
	ServerWorld& operator=(ServerWorld&&) = delete;

	~ServerWorld() override
	{
		if (!_servicesStarted)
		{
			return;
		}
		// What the scripts asked of openblack that it can't do yet, for the natives to write next
		if (Locator::chlapi::has_value())
		{
			Locator::chlapi::value().LogStubCalls();
		}
		// The profile keeps the time played as the server stops
		if (Locator::playerProfileSystem::has_value())
		{
			Locator::playerProfileSystem::value().StopPlaying(static_cast<int64_t>(std::time(nullptr)));
		}
		if (Locator::landSystem::has_value())
		{
			Locator::landSystem::value().SetView(nullptr);
		}
		ShutDownServices();
		spdlog::shutdown();
	}

	/// Starts the services of the world and registers its data. Gives false when the world cannot start.
	[[nodiscard]] bool Initialize();
	/// Loads the first land and starts its scripts and its clock. Gives false when the land cannot start.
	[[nodiscard]] bool StartLand();
	/// Plays one frame of the world. Gives false when the world is at its last turn.
	[[nodiscard]] bool Frame();
	/// Loads the world of a snapshot of the server. The world waits at the start of the turn of the snapshot. Gives false
	/// when the world does not load.
	[[nodiscard]] bool LoadSnapshot(const snapshots::Snapshot& snapshot);
	/// What the game role lets the world do in a frame
	enum class NetworkStep : uint8_t
	{
		/// The game ended
		Stop,
		/// The world waits: before the start, and with no god's player. The clock stands still: the frames take no game
		/// time and no turn comes.
		Wait,
		Play,
		/// The world does again a turn that the server sent before a restart. The frame does not wait for the wall clock.
		Replay,
	};
	void SetNetwork(std::function<NetworkStep()> beforeTurns) { _beforeTurns = std::move(beforeTurns); }

	void StartupStep(std::string_view /*step*/) override {}
	void LoadHandAnimation() override {}
	[[nodiscard]] const gui::TextDatabase* MakeInterface() override;

	void SizeSceneToWindow() override {}
	void StartPlayView() override;
	void BeforeFirstFrame(bool /*continued*/) override {}

	void PlayViewStep(game_turn::Step /*step*/, glm::vec3 /*cameraPosition*/) override {}
	[[nodiscard]] int32_t FrameRate() const override { return _frameRate.Rate(); }
	/// The testbed scenario gives its commands before the turns, as on each client of the network game
	void BeforeTurn() override;

private:
	ServerSettings _settings;
	/// Starts the renderer and the audio, which use no hardware
	StartDevices _startDevices;
	sim_run::Request _simulation;
	std::filesystem::path _startMap;
	bool _servicesStarted {false};
	/// The texts of the game, which the help's dialogue reads
	std::optional<gui::TextDatabase> _texts;
	ecs::systems::NoLandView _landView;
	simulation::NoFrameView _frameView;
	/// The frame rate that the statistics of the game keep, by the clock of the machine as the game reads it
	statistics::FrameRateMeter _frameRate;
	uint32_t _frameCount {0};
	bool _reloaded {false};
	/// The frame does again a turn that the server sent before a restart
	bool _replaying {false};
	std::chrono::steady_clock::time_point _frameStart;
	/// The testbed scenario that the server plays, as the game's window of scenarios plays it
	const testbed_scenarios::Scenario* _scenario {nullptr};
	std::unique_ptr<testbed_scenarios::Runner> _scenarioRunner;
	/// Gives false when the game of the network ended
	std::function<NetworkStep()> _beforeTurns;
};

bool ServerWorld::Initialize()
{
	simulation::LogLevels levels {};
	std::ranges::fill(levels, _settings.logLevel);
	simulation::StartLogs(_settings.logFile, levels);
	Locator::camera::emplace(glm::zero<glm::vec3>());
	Locator::config::emplace(ServerConfig(_settings));

	// The services that the server shares with the game, then the renderer that does not start the graphics library
	InitializeSimulationCore();
	_servicesStarted = true;
	if (!_startDevices || !_startDevices())
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Failed to initialize engine services.");
		return false;
	}
	auto& fileSystem = Locator::filesystem::value();
	if (!fileSystem.IsPathValid(_settings.gamePath))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to find the GameDir.");
		return false;
	}
	fileSystem.SetGamePath(_settings.gamePath);

	// The land of the server, found as the game finds its first land
	_startMap = _settings.land;
	if (_startMap.is_absolute())
	{
		auto folder = _startMap.parent_path();
		for (auto p = _startMap; p != p.parent_path(); p = p.parent_path())
		{
			if (p.filename() == "Scripts")
			{
				folder = p.parent_path();
				break;
			}
		}
		fileSystem.AddAdditionalPath(folder);
	}
	else if (const auto land = _startMap == k_DefaultStartLevel ? ecs::systems::MapScriptLand() : std::nullopt; land)
	{
		// With no land asked for, a new game starts on the land that the map script names
		_startMap = *land;
	}
	else if (online_map_land::IsOnlineMap(_startMap.generic_string()))
	{
		// A land of a network game is in the game's folder, such as "Online Maps/mpm_2p_1.map"
	}
	else
	{
		_startMap = fileSystem.GetPath<filesystem::Path::Scripts>() / _startMap;
	}

	// The server plays as a profile in memory. No file holds it, and the server has no folder for saved games, so the
	// server never saves a game and never keeps the data of a player.
	const auto profiles = player_profiles::ServerStore();
	if (_settings.profileFolder.has_value())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The server keeps no profile: the profile folder {} is not used",
		                   _settings.profileFolder->generic_string());
	}
	// The server shows nothing, so it makes none of the systems that only show the world
	if (!InitializeSimulation(profiles))
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Failed to initialize game services.");
		return false;
	}
	// The input of the player, the online services, the editor and the videos. The server draws nothing, so it has no
	// drawing of the world.
	InitializePlayerInput();
	InitializeOnlineAndTools(profiles);
	auto& land = Locator::landSystem::value();
	land.SetView(&_landView);
	land.SetNewGameStart(_settings.opening);
	// The game server gets the humans of its land from the network session
	land.SetHumansFromSession(_settings.network.has_value());
	// No player sits at the game server, so no god's tap takes the server into the temple or another state of a player
	if (Locator::handSystem::has_value())
	{
		Locator::handSystem::value().ClearOwnPlayer();
	}

	// Every random number from the seed of the world. Only the seed of a tool pins the date.
	const auto start = RoomStartFor(_settings);
	Locator::rng::value().SetRunSeed(start.seed);
	Locator::time::value().RestartClock(start.pinnedDate);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "{}", RoomStartLine(start));
	// The server's world is the world of a network game
	Locator::time::value().SetNetworkGame(true);
	// Each frame takes a fixed game time from the first, so that the server plays the same turns at any speed
	Locator::simulation::value().SetSettings(_simulation.settings);
	if (_simulation.frameTime.has_value())
	{
		Locator::time::value().SetFixedFrameTime(*_simulation.frameTime);
	}
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	if (_simulation.traceDrawsPath.has_value() && !draw_trace::Start(Locator::gameRandom::value(), *_simulation.traceDrawsPath))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the draw trace cannot open: {}",
		                    _simulation.traceDrawsPath->string());
	}
#endif
	// The run record of the tools starts with the inspector
	if (_simulation.recordPath.has_value() && StartInspector(0))
	{
		if (const auto error = Locator::inspector::value().StartSimRecord(*_simulation.recordPath, _simulation.recordEvery,
		                                                                  _simulation.limitsPath, _simulation.hashParts);
		    error.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The simulation record cannot start: {}", *error);
		}
	}
	// A tool can give the commands that a game did at each turn, which the server then does at the same turns
	if (_settings.commandsPath.has_value())
	{
		auto turns = command_log::ReadFile(*_settings.commandsPath);
		if (!turns.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the commands does not read: {}",
			                    _settings.commandsPath->string());
			return false;
		}
		auto& commands = Locator::playerCommandSystem::value();
		for (auto& [turn, turnCommands] : *turns)
		{
			commands.Receive(turn, std::move(turnCommands));
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Commands of {} turns from {}", turns->size(),
		                   _settings.commandsPath->string());
	}
	return simulation::RegisterWorldData(*this);
}

bool ServerWorld::StartLand()
{
	auto& land = Locator::landSystem::value();
	if (_settings.scenario.has_value())
	{
		_scenario = testbed_scenarios::Find(*_settings.scenario);
		if (_scenario == nullptr)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "No testbed scenario {}", *_settings.scenario);
			return false;
		}
		_scenarioRunner = std::make_unique<testbed_scenarios::Runner>();
		// A server that plays the commands of a game gives the commands of the scenario before the turns that the game
		// gave them, when the game wrote them
		if (_settings.commandsPath.has_value())
		{
			const auto gives = command_log::GivesPathOf(*_settings.commandsPath);
			std::error_code error;
			if (std::filesystem::exists(gives, error))
			{
				_scenarioRunner->FollowGives(command_log::ReadGivesFile(gives));
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "The scenario gives its commands as the game did: {}", gives.string());
			}
		}
	}
	// The land starts from the state that every client of the network game starts it from
	simulation::PrepareNetworkLand(RoomStartFor(_settings).seed);
	if (const auto refusal = simulation::ApplySoakTools(_settings.soak); refusal.has_value())
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "The server cannot start: {}", *refusal);
		return false;
	}
	if (_settings.godsRingMap)
	{
		if (!land.LoadGodsRingMap())
		{
			return false;
		}
	}
	else if (_settings.testbed)
	{
		land.LoadTestbed();
	}
	else
	{
		// The server plays as the profile in memory, so it makes none
		if (!land.LoadMap(_startMap, loading::LoadingClock::Mode::Tips, land_change::Kind::NewGame))
		{
			return false;
		}
	}
	// The winning conditions of the operator keep to the limits that the host has on the land, or on the land that the
	// option names
	if (_settings.conditions.has_value())
	{
		const auto& request = *_settings.conditions;
		const auto limitsLand =
		    request.limitsLand.empty() ? std::filesystem::path(_settings.land).stem().string() : request.limitsLand;
		const auto limits = gui::multiplayer::FindConditions(gui::ReadOnlineMapTexts().conditionTemplate, limitsLand);
		if (const auto refusal = gui::multiplayer::ConditionsRefusal(limits, request.picks); refusal.has_value())
		{
			SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "The server cannot start: {} ({})", *refusal, limitsLand);
			return false;
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The network game has {} winning conditions in the limits of {}",
		                   request.picks.size(), limitsLand);
	}
	Locator::playerProfileSystem::value().StartPlaying(static_cast<int64_t>(std::time(nullptr)));
	// The story's scripts run the first land, but not the testbed or a playground. The server's world is the world of a
	// network game, which starts no story.
	const bool startStory = !_settings.testbed && !_settings.godsRingMap &&
	                        new_game_choice::FirstLandStartsStory(Locator::entitiesRegistry::value().Context().skirmish,
	                                                              Locator::time::value().IsNetworkGame());
	if (!land.StartChallengeScripts(startStory))
	{
		return false;
	}
	simulation::BeginPlay(false, _simulation.frameTime.has_value(), *this);
	_frameStart = std::chrono::steady_clock::now();
	return true;
}

const gui::TextDatabase* ServerWorld::MakeInterface()
{
	// The server has no menu, but the world uses the options that the profile keeps, and the help reads the texts
	simulation::ApplyPlayerSettings(gui::ToProfileSettings(simulation::ProfileMenuSettings().settings));
	_texts = gui::LoadGameTexts();
	return &*_texts;
}

void ServerWorld::StartPlayView()
{
	// The custom music player gets the tracks of the custom music folder in the profile folder
	if (Locator::musicMoodSystem::has_value())
	{
		Locator::musicMoodSystem::value().LoadFolder();
	}
}

void ServerWorld::BeforeTurn()
{
	if (_scenarioRunner != nullptr)
	{
		_scenarioRunner->BeforeTurn();
	}
}

bool ServerWorld::Frame()
{
	auto& profiler = Locator::profiler::value();
	auto& clock = Locator::time::value();
	auto& land = Locator::landSystem::value();
	// The game role takes the joins and the commands, and starts or holds the world
	if (_beforeTurns)
	{
		const auto step = _beforeTurns();
		if (step == NetworkStep::Stop)
		{
			return false;
		}
		// While the world waits, its clock stands still and its frames take no game time, as the clocks of the clients
		// stand still until the start. Thus the frames before the first turn are the same as theirs.
		const bool wait = step == NetworkStep::Wait;
		_replaying = step == NetworkStep::Replay;
		if (clock.IsPaused() != wait)
		{
			clock.SetPaused(wait);
		}
	}
	// The clock of the frame, and the inspector of the tools before the frame's turn
	profiler.Frame();
	clock.Update();
	if (Locator::inspector::has_value())
	{
		Locator::inspector::value().Service();
	}
	// A hand demonstration plays its records that are due now
	if (Locator::handDemoSystem::has_value())
	{
		Locator::handDemoSystem::value().Update();
	}
	// The scenario is laid out in the first frame in which the world turns: after the start of a network game, before its
	// first turn, as each client lays it out
	if (_scenario != nullptr && !clock.IsPaused())
	{
		_scenarioRunner->Start(*std::exchange(_scenario, nullptr));
		// Give the commands, throws and rewards that are due at the start, as each client does when it lays out the
		// scenario
		_scenarioRunner->Update(0.0f);
	}
	// The tools check that a world state loads into the same world: the server loads its own world once
	if (_settings.reloadAt.has_value() && clock.GetTurn() == *_settings.reloadAt && !_reloaded &&
	    Locator::saveGameSystem::has_value())
	{
		_reloaded = true;
		auto& saves = Locator::saveGameSystem::value();
		entt::meta_ctx reflection;
		ecs::reflection::RegisterComponents(reflection);
		const auto hash = [&reflection] {
			return synced_state::HashOf(Locator::entitiesRegistry::value().Underlying(), reflection,
			                            synced_state::PresentationParts(), true);
		};
		// The order of each storage, which the turns follow as they go through the entities
		const auto orders = [] {
			std::vector<std::pair<std::string, uint64_t>> out;
			for (const auto& [id, storage] : Locator::entitiesRegistry::value().Underlying().storage())
			{
				uint64_t order = 1469598103934665603ull;
				for (const auto entity : storage)
				{
					order = (order ^ entt::to_integral(entity)) * 1099511628211ull;
				}
				out.emplace_back(std::string(storage.info().name()), order);
			}
			std::ranges::sort(out);
			return out;
		};
		const auto ordersBefore = orders();
		// The time of each step in milliseconds, so that the tools can see the cost of a world state for a client
		const auto timed = [](auto&& step) {
			const auto start = std::chrono::steady_clock::now();
			auto result = step();
			return std::pair(std::move(result),
			                 std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
		};
		const auto [before, hashMs] = timed(hash);
		const auto drawsBefore = Locator::gameRandom::value().GetSyncedDraws();
		const auto [state, saveMs] = timed([&saves] { return saves.WorldState(); });
		const auto [loaded, loadMs] = timed([&saves, &state] { return saves.LoadWorldState(state); });
		const auto after = hash();
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "The world state of turn {} ({} bytes) loaded again: {}, synced state the same: {}, draws {} and {}",
		                   *_settings.reloadAt, state.size(), loaded, before.hash == after.hash, drawsBefore,
		                   Locator::gameRandom::value().GetSyncedDraws());
		SPDLOG_LOGGER_INFO(
		    spdlog::get("game"),
		    "The world state took {:.2f} ms to make and {:.2f} ms to load; the hash of each field took {:.2f} ms", saveMs,
		    loadMs, hashMs);
		const auto ordersAfter = orders();
		for (const auto& [name, order] : ordersBefore)
		{
			const auto found = std::ranges::find(ordersAfter, name, &std::pair<std::string, uint64_t>::first);
			if (found == ordersAfter.end() || found->second != order)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "The world state did not put back the order of {}", name);
			}
		}
		// The parts that the load did not put back as they were
		for (const auto& [name, value] : before.parts)
		{
			const auto found = std::ranges::find(after.parts, name, &std::pair<std::string, uint64_t>::first);
			if (found == after.parts.end() || found->second != value)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "The world state did not put back the part {}", name);
			}
		}
	}
	// The turn of the world when it is due, with no view, and the commands that the players sent
	land.PlayTurns(*this);
	// As the game that plays the commands of a file, the server then does no command of its own player
	switch (OwnCommandsOf(_settings))
	{
	case OwnCommands::Send:
		Locator::playerCommandSystem::value().SendOpenBlock();
		break;
	case OwnCommands::Drop:
		Locator::playerCommandSystem::value().Reset();
		break;
	}
	clock.UpdateFrame();
	// The real time since the last frame, as the game's draw measures it. The storms' clouds and other frame effects step
	// by it.
	clock.StartDraw();
	// The frame steps that change the world: the creatures' fights, walks and animations, and the frame's effects
	simulation::PlayCreaturePoses();
	simulation::PlayFrameEffects(_frameView);
	land.KeepAlignmentInProfile();
	if (sim_run::Finished(Locator::simulation::value().GetSettings(), clock.GetTurn()))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The simulation run is at its last turn, {}", clock.GetTurn());
		return false;
	}

	// What the loading threads finished goes into the caches, and the renderer with no device takes what they made
	Locator::resources::value().UpdateLoading(simulation::k_FrameLoadBudget, simulation::k_FrameUploads);
	auto& renderer = Locator::rendererRequests::value();
	renderer.Frame();
	// The pictures that the world asked for come back with none: the server draws nothing
	const auto take = [&renderer](const std::string& name) { return renderer.TakeScreenPicture(name); };
	const auto failed = [&renderer](const std::string& name) { return renderer.TakeScreenshotFailure(name).has_value(); };
	ecs::challenge_pictures::Collect(Locator::entitiesRegistry::value(), {.take = take, .failed = failed},
	                                 ecs::challenge_pictures::ProfileFiles());
	if (Locator::saveGameSystem::has_value())
	{
		Locator::saveGameSystem::value().CollectPictures({.take = take, .failed = failed});
		Locator::saveGameSystem::value().ReportWriteProblems();
	}
	if (Locator::temple::has_value())
	{
		Locator::temple::value().FrameDrawn();
	}
	if (Locator::creatureWebPageSystem::has_value())
	{
		Locator::creatureWebPageSystem::value().CollectPictures({.take = take, .failed = failed});
	}

	++_frameCount;
	// The frame rate of the statistics uses the machine clock of the game. With a fixed frame time each run gets the
	// same rate.
	_frameRate.Frame(machine_clock::Ticks(), _frameCount);
	// Unless the run is fast, the frame waits until it took its fixed game time in real time too. The turns that the server
	// does again after a restart come with no wait.
	auto& simulation = Locator::simulation::value();
	if (const auto wait = sim_run::PaceWait(
	        simulation.GetSettings(), clock.GetFixedFrameTime(),
	        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - _frameStart));
	    wait.count() > 0 && !_replaying)
	{
		std::this_thread::sleep_for(wait);
	}
	const auto frameEnd = std::chrono::steady_clock::now();
	simulation.FrameEnded(std::chrono::duration<float, std::milli>(frameEnd - _frameStart).count());
	{
		// The run record keeps the time of each stage too
		const auto& entry = profiler.GetEntries().at(profiler.GetEntryIndex(0));
		std::array<float, static_cast<size_t>(Profiler::Stage::_count)> stageMs {};
		std::ranges::transform(entry.stages, stageMs.begin(), [](const Profiler::Scope& scope) {
			return std::chrono::duration<float, std::milli>(scope.total).count();
		});
		simulation.StagesEnded(stageMs);
	}
	_frameStart = frameEnd;
	return true;
}

bool ServerWorld::LoadSnapshot(const snapshots::Snapshot& snapshot)
{
	const auto start = std::chrono::steady_clock::now();
	if (!Locator::saveGameSystem::has_value() || !Locator::saveGameSystem::value().LoadWorldState(snapshot.world))
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "The world of the snapshot of turn {} does not load", snapshot.turn);
		return false;
	}
	// The world is still the world of a network game. It waits until the server turns again, as the world of a client
	// that loads the server's world.
	auto& clock = Locator::time::value();
	clock.SetNetworkGame(true);
	clock.SetPaused(true);
	Locator::syncChecksumSystem::value().SetEnabled(true);
	if (clock.GetTurn() != snapshot.turn)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "The world of the snapshot is at turn {}, not at turn {}", clock.GetTurn(),
		                       snapshot.turn);
		return false;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The world of the snapshot of turn {} ({} bytes) loaded in {:.2f} ms",
	                   snapshot.turn, snapshot.world.size(),
	                   std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
	return true;
}

#if defined(OPENBLACK_GAME_ROLE)
/// Keeps the snapshots of a network game on the server's disk (deviation 100). The game thread takes copies of the world
/// at the start of a turn, and a writer thread makes the bytes and writes the file. Thus the turns do not wait for the
/// disk. The journal keeps each turn that the server sends after the snapshot.
class SnapshotKeeper
{
public:
	explicit SnapshotKeeper(const snapshots::Settings& settings)
	    : _settings(settings)
	    , _store(settings.folder, settings.keep)
	{
	}

	/// Makes the data of the world states ready after the land loaded, before the game turns. Thus the first snapshot
	/// takes no more time than the next.
	void Prepare() const
	{
		if (_settings.everyTurns > 0 && Locator::saveGameSystem::has_value())
		{
			Locator::saveGameSystem::value().PrepareWorldState();
		}
	}

	SnapshotKeeper(const SnapshotKeeper&) = delete;
	SnapshotKeeper& operator=(const SnapshotKeeper&) = delete;
	SnapshotKeeper(SnapshotKeeper&&) = delete;
	SnapshotKeeper& operator=(SnapshotKeeper&&) = delete;
	~SnapshotKeeper() = default;

	/// Once a frame, after the game role read the sockets and before the turn: a snapshot when one is due, else the room
	/// in the journal when it changed
	void BeforeTurn(const LockstepHost& host)
	{
		if (_ended || _settings.everyTurns == 0 || !host.Started() || host.Replaying() || !Locator::saveGameSystem::has_value())
		{
			return;
		}
		auto room = host.RoomRecord();
		auto blocks = host.BlocksInTurns();
		const auto turn = host.TurnsDone();
		// The first turn brings the creatures from the files of the players, so the first snapshot comes after it
		const bool due = turn >= 1 && (!_lastTurn.has_value() || turn >= *_lastTurn + _settings.everyTurns);
		if (due && _writer->Pending() == 0)
		{
			const auto start = std::chrono::steady_clock::now();
			auto make = Locator::saveGameSystem::value().WorldStateLater();
			const auto takenMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
			if (!_store.OpenJournal(turn, room))
			{
				SPDLOG_LOGGER_WARN(spdlog::get("game"), "The journal of the snapshots cannot open in {}",
				                   _store.Folder().generic_string());
			}
			_lastRoom = room;
			_lastTurn = turn;
			_writer->Queue(fmt::format("the snapshot of turn {}", turn), [store = &_store, make = std::move(make),
			                                                              room = std::move(room), blocks = std::move(blocks),
			                                                              turn, takenMs]() {
				const auto writeStart = std::chrono::steady_clock::now();
				const auto bytes = snapshots::EncodeSnapshot({.turn = turn, .room = room, .world = make(), .blocks = blocks});
				const bool written = store->Write(turn, bytes);
				SPDLOG_LOGGER_INFO(
				    spdlog::get("game"),
				    "The snapshot of turn {} ({} bytes): {:.2f} ms on the game thread, {:.2f} ms on the writer", turn,
				    bytes.size(), takenMs,
				    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - writeStart).count());
				return written;
			});
			return;
		}
		if (room != _lastRoom && _store.JournalOpen())
		{
			_store.AddRoom(room);
			_lastRoom = std::move(room);
		}
	}

	/// A turn that the server closed, before it sends the turn
	void Turn(uint32_t turn, std::span<const uint8_t> bytes, const snapshots::BlockSequences& blocks)
	{
		if (!_ended)
		{
			_store.AddTurn(turn, bytes, blocks);
		}
	}

	/// The game ended, so no server continues it: the snapshots go
	void GameOver()
	{
		if (std::exchange(_ended, true))
		{
			return;
		}
		_writer->Flush();
		_store.Clear();
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game ended: the server removed its snapshots");
	}

private:
	snapshots::Settings _settings;
	snapshots::Store _store;
	std::optional<uint32_t> _lastTurn;
	snapshots::Room _lastRoom;
	bool _ended {false};
	/// Last, so that it writes the snapshots that wait before the store goes
	std::unique_ptr<ecs::save_files::BackgroundWriter> _writer {std::make_unique<ecs::save_files::BackgroundWriter>(
	    [](const std::string& name) { SPDLOG_LOGGER_WARN(spdlog::get("game"), "The server did not write {}", name); })};
};

/// Starts the game role after the land loaded: the clients start the same land with the same seed. A server that
/// continues a game from a snapshot restores the room of the game.
bool StartNetwork(const ServerSettings& settings, ServerWorld& world, std::unique_ptr<GameRole>& network,
                  const std::function<void(uint32_t turn, uint64_t checksum)>& onTurn, SnapshotKeeper* keeper,
                  const std::optional<snapshots::Restored>& restored)
{
	network = std::make_unique<GameRole>();
	// The land goes as its path under the Scripts folder, so that each client finds it in its own game folder, and the
	// clients compare the checksum of its file before the start. The clients take the land, the seed and the start from
	// the server as they join.
	auto& fileSystem = Locator::filesystem::value();
	const bool scriptLand = !settings.testbed && !settings.godsRingMap;
	const auto landPath = Locator::landSystem::value().GetLandPath();
	uint64_t landChecksum = online::start_rules::LandChecksum({});
	if (scriptLand && fileSystem.Exists(landPath))
	{
		landChecksum = online::start_rules::LandChecksum(fileSystem.ReadAll(landPath));
	}
	game_socket::Start start {
	    .land = scriptLand ? network_land::WireName(landPath) : std::string(),
	    .testbed = settings.testbed,
	    .seed = RoomStartFor(settings).seed,
	    .turn = Locator::time::value().GetTurn(),
	    .opening = scriptLand ? settings.openingName : std::string(),
	    .godsRingMap = settings.godsRingMap,
	    .allComputerGods = settings.soak.allComputerGods,
	    .aiSeed = settings.soak.aiSeed,
	    .scenario = settings.scenario,
	    .timeLimit = settings.timeLimit.value_or(game_end::k_DefaultTimeLimitMinutes),
	    .conditions = {},
	    .gods = {},
	};
	if (settings.conditions.has_value())
	{
		for (const auto& pick : settings.conditions->picks)
		{
			start.conditions.push_back(
			    {.condition = static_cast<uint8_t>(pick.condition), .value = static_cast<uint32_t>(pick.value)});
		}
	}
	// A land of a network game has a god slot for each citadel, as the room of the land has a team for each citadel. The
	// map for 32 gods has a slot for each of its citadels too. The other lands of the tools and the testbed keep the
	// default slots.
	uint8_t godSlots = k_DefaultGodSlots;
	if ((scriptLand && online_map_land::IsOnlineMap(settings.land)) || settings.godsRingMap)
	{
		// Each god slot of the land, the slots after the seventh too (deviation 166)
		std::array<bool, player_slots::k_MaxPlayers> citadels {};
		Locator::entitiesRegistry::value().Each<const ecs::components::Temple>([&citadels](const auto& temple) {
			if (const auto slot = player_slots::SlotOfPlayer(temple.owner); slot.has_value() && *slot < citadels.size())
			{
				citadels.at(*slot) = true;
			}
		});
		godSlots = GodSlotsOfLand(citadels);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The room has {} god slots and starts with {} gods at least", godSlots,
	                   settings.network->minGods);
	// The game of a snapshot continues with the start that its clients got
	if (restored.has_value())
	{
		const auto saved = game_socket::Parse(restored->room.start);
		const auto* savedStart = saved.has_value() ? std::get_if<game_socket::Start>(&*saved) : nullptr;
		if (savedStart == nullptr || !snapshots::SameGame(*savedStart, start))
		{
			SPDLOG_LOGGER_CRITICAL(spdlog::get("game"),
			                       "The snapshot is of another game than the land and the seed of this server");
			return false;
		}
		start = *savedStart;
	}
	if (const auto error = network->Start(*settings.network, start, landChecksum, godSlots); error.has_value())
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "The game role cannot start: {}", *error);
		network.reset();
		return false;
	}
	auto& host = network->Host();
	if (restored.has_value())
	{
		host.Restore(restored->room, restored->snapshot.turn, restored->turns, restored->blocks);
	}
	if (keeper != nullptr)
	{
		keeper->Prepare();
		host.SetTurnJournal([keeper](uint32_t turn, std::span<const uint8_t> bytes, const snapshots::BlockSequences& blocks) {
			keeper->Turn(turn, bytes, blocks);
		});
	}
	// The world turns only with the turns that the host closes: before the start and with no player, it does no turn.
	Locator::playerCommandSystem::value().SetSource(&host);
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	// The row dump of the tools writes the rows of the turn at which the world of a client parts
	host.SetPartListener([](uint8_t seat, uint32_t turn) {
		Locator::syncChecksumSystem::value().DumpPart(turn, fmt::format("slot {} parts", seat), seat);
	});
#endif
	// A client whose world parted gets the world of the server, at the start of the next turn
	host.SetWorldState([] {
		return Locator::saveGameSystem::has_value() ? Locator::saveGameSystem::value().WorldState() : std::vector<uint8_t> {};
	});
	world.SetNetwork([&host, role = network.get(), keeper] {
		// After a restart, the world first does again the turns that the server sent. The server reads no socket until then.
		if (host.Replaying())
		{
			return ServerWorld::NetworkStep::Replay;
		}
		role->Update();
		if (host.GameOver())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Every god left the game: the game ends");
			if (keeper != nullptr)
			{
				keeper->GameOver();
			}
			return ServerWorld::NetworkStep::Stop;
		}
		if (keeper != nullptr)
		{
			keeper->BeforeTurn(host);
		}
		return host.HasTurn(Locator::time::value().GetTurn()) ? ServerWorld::NetworkStep::Play : ServerWorld::NetworkStep::Wait;
	});
	Locator::syncChecksumSystem::value().SetTurnListener(
	    [&host, onTurn,
	     logChecksums = settings.logChecksums](const ecs::systems::SyncChecksumSystemInterface::TurnChecksum& turn) {
		    if (logChecksums)
		    {
			    SPDLOG_LOGGER_INFO(spdlog::get("game"), "{}", ChecksumLine(turn.turn, turn.checksum));
		    }
		    if (onTurn)
		    {
			    onTurn(turn.turn, turn.checksum);
		    }
		    // Every client gets the checksum of the server's world
		    host.TurnChecksum(turn.turn, turn.checksum);
	    });
	return true;
}
#else
/// A build with no game role keeps no snapshots
class SnapshotKeeper
{
public:
	explicit SnapshotKeeper(const snapshots::Settings& /*settings*/) {}
};

bool StartNetwork(const ServerSettings& /*settings*/, ServerWorld& /*world*/, std::unique_ptr<int>& /*network*/,
                  const std::function<void(uint32_t turn, uint64_t checksum)>& /*onTurn*/, SnapshotKeeper* /*keeper*/,
                  const std::optional<snapshots::Restored>& /*restored*/)
{
	SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "This build has no game role");
	return false;
}
#endif

/// Loads the newest snapshot that reads, for a server that continues its game. With no snapshot, the server starts a new
/// game. Gives false when the server cannot start.
bool PrepareResume(const ServerSettings& settings, ServerWorld& world, std::optional<snapshots::Restored>& restored)
{
	if (settings.scenario.has_value())
	{
		// The testbed scenario keeps its state out of the world, so no snapshot can continue it
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "A game with a testbed scenario cannot continue from a snapshot");
		return false;
	}
	const auto& snapshotSettings = settings.network->snapshots;
	restored = snapshots::Store(snapshotSettings.folder, snapshotSettings.keep).LoadNewest();
	if (!restored.has_value())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "No snapshot in {}: the server starts a new game",
		                   snapshotSettings.folder.generic_string());
		return true;
	}
	for (const auto& skipped : restored->skipped)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The snapshot {} does not read: the server uses an older snapshot", skipped);
	}
	if (restored->tornJournal)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "The journal ends with a torn record: the server continues from the last whole turn");
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The server continues its game from the snapshot of turn {} and {} sent turns",
	                   restored->snapshot.turn, restored->turns.size());
	return world.LoadSnapshot(restored->snapshot);
}

} // namespace

EngineConfig ServerConfig(const ServerSettings& settings)
{
	EngineConfig config;
	// No window and no device: the renderer draws nothing, the audio plays nothing, and the loaders keep only the data
	config.resolution = {0, 0};
	config.displayMode = windowing::DisplayMode::Windowed;
	config.background = true;
	config.dedicatedServer = true;
	config.graphicsBackend = GraphicsBackend::Noop;
	config.resourceUploads = graphics::UploadMode::DataOnly;
	config.vsync = false;
	config.clockedAudio = sim_run::FrameTimeFromMs(settings.frameMilliseconds).has_value();
	return config;
}

OwnCommands OwnCommandsOf(const ServerSettings& settings)
{
	return settings.commandsPath.has_value() ? OwnCommands::Drop : OwnCommands::Send;
}

sim_run::Request SimulationFor(const ServerSettings& settings)
{
	sim_run::Request simulation {};
	simulation.settings.fast = settings.fast;
	simulation.settings.drawEvery = 0;
	simulation.settings.paceToWallClock = !settings.fast;
	simulation.settings.untilTurn = settings.turns;
	simulation.settings.stillHand = true;
	simulation.settings.templeLoss = settings.templeLoss;
	simulation.settings.godsNeedNoTemple = settings.godsNeedNoTemple;
	simulation.settings.timeLimit = settings.timeLimit;
	if (settings.conditions.has_value())
	{
		simulation.settings.conditions = settings.conditions->picks;
	}
	simulation.frameTime = sim_run::FrameTimeFromMs(settings.frameMilliseconds);
	simulation.traceDrawsPath = settings.traceDrawsPath;
	if (settings.recordPath.has_value())
	{
		simulation.recordPath = settings.recordPath;
		simulation.recordEvery = 1;
		simulation.hashParts = settings.recordFields ? sim_run::HashDetail::Fields : sim_run::HashDetail::Components;
	}
	return simulation;
}

RoomStart RoomStartFor(const ServerSettings& settings)
{
	if (settings.seed.has_value())
	{
		return {.seed = *settings.seed, .pinnedDate = ecs::systems::TimeSystemInterface::k_DeterministicDate};
	}
	return {.seed = k_RoomSeed, .pinnedDate = std::nullopt};
}

std::string RoomStartLine(const RoomStart& start)
{
	return start.pinnedDate.has_value()
	           ? fmt::format("The seed of the world: {} (from a tool), date pinned", start.seed)
	           : fmt::format("The seed of the world: {}, date from the clock of the computer", start.seed);
}

std::string ChecksumLine(uint32_t turn, uint64_t checksum)
{
	return fmt::format("{}{}{}{:016x}", k_TurnWord, turn, k_ChecksumWord, checksum);
}

std::optional<std::pair<uint32_t, uint64_t>> ParseChecksumLine(std::string_view line)
{
	const auto start = line.find(k_TurnWord);
	if (start == std::string_view::npos)
	{
		return std::nullopt;
	}
	line.remove_prefix(start + k_TurnWord.size());
	uint32_t turn = 0;
	const auto turnEnd = std::from_chars(line.data(), line.data() + line.size(), turn);
	if (turnEnd.ec != std::errc {})
	{
		return std::nullopt;
	}
	line.remove_prefix(static_cast<size_t>(turnEnd.ptr - line.data()));
	if (!line.starts_with(k_ChecksumWord))
	{
		return std::nullopt;
	}
	line.remove_prefix(k_ChecksumWord.size());
	uint64_t checksum = 0;
	const auto checksumEnd = std::from_chars(line.data(), line.data() + line.size(), checksum, 16);
	if (checksumEnd.ec != std::errc {})
	{
		return std::nullopt;
	}
	return std::pair {turn, checksum};
}

int RunWithDevices(const ServerSettings& settings, const std::atomic<bool>& stop,
                   const std::function<void(uint32_t turn, uint64_t checksum)>& onTurn, const StartDevices& startDevices)
{
	auto world = std::make_unique<ServerWorld>(settings, startDevices);
	if (!world->Initialize())
	{
		return EXIT_FAILURE;
	}
	// The server makes the checksum of the synced world at the end of each turn, as each player's game does in a
	// network game
	auto& checksums = Locator::syncChecksumSystem::value();
	checksums.SetEnabled(true);
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	if (settings.rowDumpPath.has_value() && !checksums.StartRowDump(*settings.rowDumpPath, settings.rowDump))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the row dump cannot open: {}", settings.rowDumpPath->string());
	}
	if (settings.worldStates.has_value())
	{
		checksums.WriteWorldStates(*settings.worldStates);
	}
#endif
	checksums.SetTurnListener(
	    [&onTurn, logChecksums = settings.logChecksums](const ecs::systems::SyncChecksumSystemInterface::TurnChecksum& turn) {
		    if (logChecksums)
		    {
			    SPDLOG_LOGGER_INFO(spdlog::get("game"), "{}", ChecksumLine(turn.turn, turn.checksum));
		    }
		    if (onTurn)
		    {
			    onTurn(turn.turn, turn.checksum);
		    }
	    });

	int exitCode = EXIT_SUCCESS;
#if defined(OPENBLACK_GAME_ROLE)
	std::unique_ptr<GameRole> network;
#else
	std::unique_ptr<int> network;
#endif
	// The game server keeps snapshots of its world, and can continue the game of the newest snapshot
	std::unique_ptr<SnapshotKeeper> keeper;
	std::optional<snapshots::Restored> restored;
	if (settings.network.has_value() && settings.network->snapshots.everyTurns > 0)
	{
		keeper = std::make_unique<SnapshotKeeper>(settings.network->snapshots);
	}
	bool ready = world->StartLand();
	if (ready && settings.network.has_value() && settings.network->snapshots.resume)
	{
		ready = PrepareResume(settings, *world, restored);
	}
	if (!ready)
	{
		exitCode = EXIT_FAILURE;
	}
	else if (settings.network.has_value() && !StartNetwork(settings, *world, network, onTurn, keeper.get(), restored))
	{
		exitCode = EXIT_FAILURE;
	}
	else
	{
		while (!stop.load() && world->Frame())
		{
		}
		if (stop.load())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "The server stops on a signal");
		}
		LogGraphicsLibrary();
	}
	checksums.SetTurnListener({});
	// The writer of the snapshots finishes before the services of the world stop
	keeper.reset();
#if defined(OPENBLACK_GAME_ROLE)
	if (network)
	{
		for (const auto& line : network->Host().SummaryLines())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Network game: {}", line);
		}
		network->Close();
		Locator::playerCommandSystem::value().SetSource(nullptr);
		world->SetNetwork({});
		network.reset();
	}
#endif
	world.reset();
	return exitCode;
}

} // namespace openblack::server
