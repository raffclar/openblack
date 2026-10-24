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
#include <memory>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>

#include <fmt/format.h>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "3D/TempleInteriorInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Common/MachineClock.h"
#include "Common/RandomNumberManager.h"
#include "Common/UserData.h"
#include "Debug/TestbedScenarioRunner.h"
#include "ECS/ChallengePictures.h"
#include "ECS/Registry.h"
#include "ECS/StatisticsRules.h"
#include "ECS/Systems/CreatureWebPageSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
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
#include "Graphics/RendererInterface.h"
#include "Gui/GameInterface.h"
#include "Gui/TextDatabase.h"
#include "Locator.h"
#include "Profiler.h"
#include "Resources/ResourcesInterface.h"
#include "Simulation/CommandLog.h"
#include "Simulation/FrameBody.h"
#include "Simulation/ProfileMenu.h"
#include "Simulation/TurnBody.h"

// The game role has sockets and TLS, which only the desktop builds have
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
#define OPENBLACK_GAME_ROLE 1
#include "Server/Desktop/GameRole.h"
#endif

namespace openblack::server
{
namespace
{

constexpr std::string_view k_TurnWord = "turn ";
constexpr std::string_view k_ChecksumWord = " checksum ";

/// The world of the server. It starts the services of the world, loads the land and plays the frames of the world,
/// with no window and no view of a player.
class ServerWorld final:
    public simulation::WorldDataViewInterface,
    public simulation::PlayStartViewInterface,
    public simulation::TurnViewInterface
{
public:
	explicit ServerWorld(const ServerSettings& settings)
	    : _settings(settings)
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
	/// The game role, which gives the turns of a network game
	/// What the game role lets the world do in a frame
	enum class NetworkStep : uint8_t
	{
		/// The game ended
		Stop,
		/// The world waits: before the start, and with no god's player. No step of the world runs, not even its clock.
		Wait,
		Play,
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

private:
	ServerSettings _settings;
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

	// The services that the server shares with the game, then the renderer that makes the resources with no device
	InitializeSimulationCore();
	_servicesStarted = true;
	if (!InitializeEngine(GraphicsBackend::Noop, false))
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
	else
	{
		_startMap = fileSystem.GetPath<filesystem::Path::Scripts>() / _startMap;
	}

	// The server plays as the profile of a tool's run, or as its own profile
	const auto profiles = player_profiles::StoreFor(
	    {.profileFolder = _settings.profileFolder, .userFolder = user_data::ProfileFolder(), .automated = true});
	if (!InitializeGame(profiles))
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Failed to initialize game services.");
		return false;
	}
	auto& land = Locator::landSystem::value();
	land.SetView(&_landView);
	land.SetNewGameStart(_settings.opening);

	// Every random number from the seed, the date pinned
	Locator::rng::value().SetRunSeed(_settings.seed);
	Locator::time::value().RestartClock(ecs::systems::TimeSystemInterface::k_DeterministicDate);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Seeded run: seed {}, date pinned", _settings.seed);
	// The server's world is the world of a network game
	Locator::time::value().SetNetworkGame(true);
	// Each frame takes a fixed game time from the first, so that the server plays the same turns at any speed
	Locator::simulation::value().SetSettings(_simulation.settings);
	if (_simulation.frameTime.has_value())
	{
		Locator::time::value().SetFixedFrameTime(*_simulation.frameTime);
	}
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
	}
	if (_settings.testbed)
	{
		land.LoadTestbed();
	}
	else
	{
		// A server with no profile plays as the profile that the game makes for a player who gives no name
		auto& profiles = Locator::playerProfileSystem::value();
		if (profiles.GetProfileCount() == 0)
		{
			profiles.CreateProfile(u"", 0);
		}
		if (!land.LoadMap(_startMap, loading::LoadingClock::Mode::Tips, land_change::Kind::NewGame))
		{
			return false;
		}
	}
	Locator::playerProfileSystem::value().StartPlaying(static_cast<int64_t>(std::time(nullptr)));
	// The story's scripts run the first land, but not the testbed or a playground
	const bool startStory =
	    !_settings.testbed && new_game_choice::FirstLandStartsStory(Locator::entitiesRegistry::value().Context().skirmish);
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
	_texts = gui::GameInterface::LoadTexts();
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
		if (step == NetworkStep::Wait)
		{
			// The world waits as it is, so that it is the same as the clients' worlds when the turns start
			if (Locator::inspector::has_value())
			{
				Locator::inspector::value().Service();
			}
			std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::microseconds>(
			    clock.GetFixedFrameTime().value_or(std::chrono::microseconds(100'000))));
			_frameStart = std::chrono::steady_clock::now();
			return true;
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
	// The scenario starts in the first frame and gives its commands in the frames before the turns, at the time of the
	// game's debug windows
	if (_scenarioRunner != nullptr)
	{
		if (_scenario != nullptr)
		{
			_scenarioRunner->Start(*std::exchange(_scenario, nullptr));
		}
		_scenarioRunner->Update(std::chrono::duration<float>(clock.GetFrameGameTime()).count());
	}
	// The turn of the world when it is due, with no view, and the commands that the players sent
	land.PlayTurns(*this);
	Locator::playerCommandSystem::value().SendOpenBlock();
	clock.UpdateFrame();
	// The real time since the last frame, as the game's draw measures it. The storms' clouds and other frame effects step
	// by it.
	clock.StartDraw();
	// The frame steps that change the world: the creatures' fights, walks and animations, and the frame's effects
	simulation::PlayCreaturePoses();
	simulation::PlayFrameEffects(_frameView);
	Locator::audio::value().Update();
	land.KeepAlignmentInProfile();
	if (sim_run::Finished(Locator::simulation::value().GetSettings(), clock.GetTurn()))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The simulation run is at its last turn, {}", clock.GetTurn());
		return false;
	}

	// What the loading threads finished goes into the caches, and the renderer with no device takes what they made
	Locator::resources::value().UpdateLoading(simulation::k_FrameLoadBudget, simulation::k_FrameUploads);
	auto& renderer = Locator::rendererInterface::value();
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
	// Unless the run is fast, the frame waits until it took its fixed game time in real time too
	auto& simulation = Locator::simulation::value();
	if (const auto wait = sim_run::PaceWait(
	        simulation.GetSettings(), clock.GetFixedFrameTime(),
	        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - _frameStart));
	    wait.count() > 0)
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

#if defined(OPENBLACK_GAME_ROLE)
/// Starts the game role after the land loaded: the clients start the same land with the same seed
bool StartNetwork(const ServerSettings& settings, ServerWorld& world, std::unique_ptr<GameRole>& network,
                  const std::function<void(uint32_t turn, uint64_t checksum)>& onTurn)
{
	network = std::make_unique<GameRole>();
	const game_socket::Start start {
	    .land = Locator::landSystem::value().GetLandPath().filename().string(),
	    .testbed = settings.testbed,
	    .seed = settings.seed,
	    .turn = Locator::time::value().GetTurn(),
	};
	if (const auto error = network->Start(*settings.network, start); error.has_value())
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "The game role cannot start: {}", *error);
		network.reset();
		return false;
	}
	auto& host = network->Host();
	// The world turns only with the turns that the host closes: before the start and with no player, it does no turn.
	// The server's clock never pauses: a paused clock of the server parted its world from the clients' (Land 1, turn 49).
	Locator::playerCommandSystem::value().SetSource(&host);
	world.SetNetwork([&host, role = network.get()] {
		role->Update();
		if (host.GameOver())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Every god left the game: the game ends");
			return ServerWorld::NetworkStep::Stop;
		}
		return host.HasTurn(Locator::time::value().GetTurn()) ? ServerWorld::NetworkStep::Play : ServerWorld::NetworkStep::Wait;
	});
	Locator::syncChecksumSystem::value().SetTurnListener(
	    [&host, onTurn](const ecs::systems::SyncChecksumSystemInterface::TurnChecksum& turn) {
		    SPDLOG_LOGGER_INFO(spdlog::get("game"), "{}", ChecksumLine(turn.turn, turn.checksum));
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
bool StartNetwork(const ServerSettings& /*settings*/, ServerWorld& /*world*/, std::unique_ptr<int>& /*network*/,
                  const std::function<void(uint32_t turn, uint64_t checksum)>& /*onTurn*/)
{
	SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "This build has no game role");
	return false;
}
#endif

} // namespace

EngineConfig ServerConfig(const ServerSettings& settings)
{
	EngineConfig config;
	// No window and no device: the renderer makes its resources and draws nothing, and the audio plays nothing
	config.resolution = {0, 0};
	config.displayMode = windowing::DisplayMode::Windowed;
	config.background = true;
	config.dedicatedServer = true;
	config.graphicsBackend = GraphicsBackend::Noop;
	config.vsync = false;
	config.clockedAudio = sim_run::FrameTimeFromMs(settings.frameMilliseconds).has_value();
	return config;
}

sim_run::Request SimulationFor(const ServerSettings& settings)
{
	sim_run::Request simulation {};
	simulation.settings.fast = settings.fast;
	simulation.settings.drawEvery = 0;
	simulation.settings.paceToWallClock = !settings.fast;
	simulation.settings.untilTurn = settings.turns;
	simulation.settings.stillHand = true;
	simulation.frameTime = sim_run::FrameTimeFromMs(settings.frameMilliseconds);
	if (settings.recordPath.has_value())
	{
		simulation.recordPath = settings.recordPath;
		simulation.recordEvery = 1;
		simulation.hashParts = settings.recordFields ? sim_run::HashDetail::Fields : sim_run::HashDetail::Components;
	}
	return simulation;
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

int Run(const ServerSettings& settings, const std::atomic<bool>& stop,
        const std::function<void(uint32_t turn, uint64_t checksum)>& onTurn)
{
	auto world = std::make_unique<ServerWorld>(settings);
	if (!world->Initialize())
	{
		return EXIT_FAILURE;
	}
	// The server makes the checksum of the synced world at the end of each turn, as each player's game does in a
	// network game
	auto& checksums = Locator::syncChecksumSystem::value();
	checksums.SetEnabled(true);
	checksums.SetTurnListener([&onTurn](const ecs::systems::SyncChecksumSystemInterface::TurnChecksum& turn) {
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "{}", ChecksumLine(turn.turn, turn.checksum));
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
	if (!world->StartLand())
	{
		exitCode = EXIT_FAILURE;
	}
	else if (settings.network.has_value() && !StartNetwork(settings, *world, network, onTurn))
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
	}
	checksums.SetTurnListener({});
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
