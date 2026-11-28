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
#include <iterator>
#include <memory>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "3D/TempleInteriorInterface.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#if defined(OPENBLACK_DEVELOPER_TOOLS)
#include "Common/DrawTrace.h"
#endif
#include "Commands/TurnCodec.h"
#include "Common/DesyncReports.h"
#include "Common/GameRandom.h"
#include "Common/MachineClock.h"
#include "Common/PlayerProfiles.h"
#include "Common/PreciseWait.h"
#include "Common/RandomNumberManager.h"
#include "Debug/ScenarioPlayer.h"
#include "ECS/ChallengePictures.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerStatistics.h"
#include "ECS/Components/Temple.h"
#include "ECS/ComputerPlayerStart.h"
#include "ECS/GameEndConditions.h"
#include "ECS/Map.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "ECS/Registry.h"
#include "ECS/SaveFileWriter.h"
#include "ECS/StatisticsRules.h"
#include "ECS/SyncedState.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"
#include "ECS/Systems/CreatureWebPageSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
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
#include "ECS/WorldFileGods.h"
#include "FileSystem/FileSystemInterface.h"
#include "Gui/GameTexts.h"
#include "Gui/MenuSettings.h"
#include "Gui/MultiplayerFrontEndRules.h"
#include "Gui/OnlineMapTexts.h"
#include "Gui/TextDatabase.h"
#include "Locator.h"
#include "Online/DownloadTestMap.h"
#include "Online/GameBuildId.h"
#include "Online/GameDataFiles.h"
#include "Online/GodsRingMap.h"
#include "Online/GodsRingMapFile.h"
#include "Online/GodsRingMapPackage.h"
#include "Online/NetworkLand.h"
#include "Online/OnlineMapLand.h"
#include "Online/Sha256.h"
#include "PlayerSlots.h"
#include "Profiler.h"
#include "Resources/ResourcesInterface.h"
#include "Server/TurnPace.h"
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
#include "Server/DesyncBundle.h"
#include "Server/LobbyGame.h"
#include "Server/LobbyHostReport.h"
#include "Server/LobbyResults.h"
#include "Server/ServerAdmin.h"
#if defined(OPENBLACK_INSPECTOR)
#include "Inspector/ServerProviders.h"
#endif
#endif

namespace openblack::server
{
namespace
{

/// The desyncs of the game of the server. The server plays one game in each run, so the budget of the run is here too.
struct ServerDesyncs
{
	error_reports::DesyncReportRule rule;
	error_reports::DesyncRunBudget budget;
	/// The players that a report may name, at the time of the report. Empty when the server names nobody.
	std::function<std::vector<error_reports::ReportedPlayer>()> players;

	/// Milliseconds of a steady clock
	[[nodiscard]] static int64_t Now()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
		    .count();
	}
	/// Once a frame: the events that the flood limits let go now
	void Update()
	{
		for (auto report : rule.Update(Now(), budget))
		{
			report.players = players ? players() : std::vector<error_reports::ReportedPlayer> {};
			SendDesyncReport(report);
		}
	}
	/// The end of the game: the summary, once
	void Finish()
	{
		if (auto summary = rule.Finish(budget); summary.has_value())
		{
			summary->players = players ? players() : std::vector<error_reports::ReportedPlayer> {};
			SendDesyncReport(*summary);
		}
	}
};

constexpr std::string_view k_TurnWord = "turn ";
constexpr std::string_view k_ChecksumWord = " checksum ";

/// Logs that the server ran with no graphics library. The server program does not contain the library, so no part of
/// the world can start it.
void LogGraphicsLibrary()
{
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The server ran with no graphics library");
}

/// The server has no view of a player. At the end of each load of a land it makes the creatures' maps of the land, as
/// the game does at the end of its loading screen.
class ServerLandView final: public ecs::systems::NoLandView
{
public:
	void EndLoadingScreen() override { simulation::PrepareLandMaps(); }
	void EndLoadWithoutScreen() override { simulation::PrepareLandMaps(); }
};

/// The world of the server. It starts the services of the world, loads the land and plays the frames of the world,
/// with no window and no view of a player.
/// The world after a change that the editor asked for: the turn that it is at, or why it did not change
struct WorldChangeResult
{
	std::optional<uint32_t> turn;
	std::string refusal;
	/// The SHA-256 of the file of the map for 32 gods that the world loaded. Empty for every other change.
	std::string mapSha256 {};
};

/// The land on which the world starts again: a script of the game, the testbed or the map for 32 gods
struct LandRestart
{
	/// The script of the land, as a start names it. Empty for the testbed and the map for 32 gods.
	std::string_view land;
	bool testbed {false};
	bool godsRingMap {false};
	/// How the new game on the land starts, as the start names it
	std::string_view opening;
	uint32_t seed {0};
};

/// The land that the clients load, as they find it and compare its file
struct PlayedLand
{
	/// The name of the land in the start. Empty for the testbed.
	std::string name;
	uint64_t checksum {0};
	/// The SHA-256 of the file of the land, which the lobby gives its members and which a client can give in its join
	std::string sha256;
};

/// The start in the text of a start frame. None when the text is not a start frame.
std::optional<game_socket::Start> StartOfText(std::string_view text)
{
	const auto frame = game_socket::Parse(text);
	if (const auto* start = frame.has_value() ? std::get_if<game_socket::Start>(&*frame) : nullptr; start != nullptr)
	{
		return *start;
	}
	return std::nullopt;
}

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
	/// Between two turns: the world loads a world file of the editor of a player, as a client loads the server's world.
	/// Then each player of the room keeps a god, and each other god of the file is as a slot with no player. Gives the turn
	/// of the loaded world, or why the world did not load it.
	[[nodiscard]] WorldChangeResult LoadEditorWorld(std::span<const uint8_t> file, std::span<const PlayerNames> players);
	/// Between two turns: the world starts again on the testbed with a testbed scenario, as a network game that starts
	/// on the scenario. The scenario is laid out in the first frame in which the world turns. Gives the turn of the new
	/// world, or why it did not start.
	[[nodiscard]] WorldChangeResult RestartOnScenario(std::string_view id, uint32_t seed);
	/// Between two turns: the world starts again on a land, on the testbed with no scenario, or on the map for 32 gods, as
	/// a network game that starts on it. Gives the turn of the new world, or why it did not start.
	[[nodiscard]] WorldChangeResult RestartOnLand(const LandRestart& restart);
	/// Why the server cannot load a land of the Load Island menu, or none when it can
	[[nodiscard]] static std::optional<std::string> LandRefusal(std::string_view land);
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
	/// The file of the map for 32 gods that the server plays, empty for another land
	[[nodiscard]] const std::vector<uint8_t>& GodsRingMapFile() const { return _godsRingMap; }
	/// The winning conditions of the game with their values, once the land started. The start sends them to every client.
	[[nodiscard]] const std::vector<game_end::ConditionPick>& ConditionPicks() const { return _conditionPicks; }

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
	/// The commands of the game on its lands up to this land go to the command system, each land one time
	void ReceiveCommandsOfLand(uint32_t land);

private:
	ServerSettings _settings;
	/// Starts the renderer and the audio, which use no hardware
	StartDevices _startDevices;
	sim_run::Request _simulation;
	std::filesystem::path _startMap;
	/// The file of the map for 32 gods, which the server makes from its game folder and sends to the lobby
	std::vector<uint8_t> _godsRingMap;
	/// The winning conditions of the operator, each with its value on the land
	std::vector<game_end::ConditionPick> _conditionPicks;
	bool _servicesStarted {false};
	/// The texts of the game, which the help's dialogue reads
	std::optional<gui::TextDatabase> _texts;
	ServerLandView _landView;
	simulation::NoFrameView _frameView;
	/// The frame rate that the statistics of the game keep, by the clock of the machine as the game reads it
	statistics::FrameRateMeter _frameRate;
	uint32_t _frameCount {0};
	bool _reloaded {false};
	/// The frame does again a turn that the server sent before a restart
	bool _replaying {false};
	std::chrono::steady_clock::time_point _frameStart;
	/// The due times of the frames, and the wait to each due time. The pace starts at the end of the first frame, so that
	/// the time of the start of the network does not count as lost time.
	turn_pace::Deadline _pace;
	precise_wait::Waiter _waiter;
	/// The testbed scenario that the server plays, as the game's window of scenarios plays it
	std::unique_ptr<testbed_scenarios::ScenarioPlayer> _scenario;
	/// The commands of the game that a tool gives, by the land of the game and then by the turn
	std::vector<std::map<uint32_t, std::vector<player_commands::Command>>> _commandLands;
	/// The count of the lands in _commandLands whose commands went to the command system
	uint32_t _commandLandsReceived {0};
	/// The count of the lands that the server loaded
	command_log::LandCounter _lands;
	bool _scenarioStarted {false};
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
		                   player_profiles::PathForLog(*_settings.profileFolder));
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
	// The inspector starts for the operator or for the run record of the tools, only on 127.0.0.1
	if (const bool wanted = _settings.inspectPort.has_value() || _simulation.recordPath.has_value();
	    wanted && StartInspector(_settings.inspectPort.value_or(0)))
	{
		// No query of the inspector changes the world of a network game: each client must play the same world as the server
		Locator::inspector::value().SetHostsNetworkGame(_settings.network.has_value());
	}
	if (_simulation.recordPath.has_value() && Locator::inspector::has_value())
	{
		if (const auto error = Locator::inspector::value().StartSimRecord(*_simulation.recordPath, _simulation.recordEvery,
		                                                                  _simulation.limitsPath, _simulation.hashParts,
		                                                                  _simulation.recordFieldsEvery);
		    error.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The simulation record cannot start: {}", *error);
		}
	}
	// A tool can give the commands that a game did at each turn, which the server then does at the same turns. The
	// commands of a land that the game loaded later go to the server when the server loads its next land.
	if (_settings.commandsPath.has_value())
	{
		auto lands = command_log::ReadLandsFile(*_settings.commandsPath);
		if (!lands.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the commands does not read: {}",
			                    _settings.commandsPath->string());
			return false;
		}
		_commandLands = std::move(*lands);
		size_t turns = 0;
		for (const auto& landTurns : _commandLands)
		{
			turns += landTurns.size();
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Commands of {} turns in {} lands from {}", turns, _commandLands.size(),
		                   _settings.commandsPath->string());
		ReceiveCommandsOfLand(0);
	}
	return simulation::RegisterWorldData(*this);
}

bool ServerWorld::StartLand()
{
	auto& land = Locator::landSystem::value();
	if (_settings.scenario.has_value())
	{
		_scenario = testbed_scenarios::MakeScenarioPlayer(*_settings.scenario);
		if (_scenario == nullptr)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "No testbed scenario {}", *_settings.scenario);
			return false;
		}
		// A server that plays the commands of a game gives the commands of the scenario before the turns that the game
		// gave them, when the game wrote them
		if (_settings.commandsPath.has_value())
		{
			const auto gives = command_log::GivesPathOf(*_settings.commandsPath);
			std::error_code error;
			if (std::filesystem::exists(gives, error))
			{
				_scenario->FollowGives(command_log::ReadGivesFile(gives));
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
	// The computer gods of these slots start at a random alignment, on the server and on each client (deviation 280)
	if (Locator::computerPlayerSystem::has_value())
	{
		Locator::computerPlayerSystem::value().SetStartAlignments(
		    computer_player::AlignmentsOfRandomSlots(_settings.randomAlignmentSlots));
	}
	if (_settings.godsRingMap)
	{
		// The server makes the map's file from the land of its game folder, and plays the file that its players get
		// from the lobby (deviation 171)
		auto file = gods_ring_map_file::MakeFromGameFolder(Locator::filesystem::value());
		if (!file.has_value() || !land.LoadGodsRingMapFile(*file))
		{
			return false;
		}
		_godsRingMap = std::move(*file);
	}
	else if (_settings.testbed)
	{
		// A server for players has no testbed
		if (!land.LoadTestbed())
		{
			return false;
		}
	}
	else if (_settings.customMap.has_value())
	{
		// A land of a network game from a file outside the game folder. The server loads its bytes as it loads a land of
		// the game folder, and its players get the same bytes from the lobby.
		if (!land.LoadNetworkMapFile(_startMap, _settings.customMap->bytes,
		                             ecs::systems::LandSystemInterface::ChallengeScripts::Kept))
		{
			return false;
		}
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
		// A condition with no value of the operator keeps the first value of the land, as on the page of the host
		auto picks = limits.has_value() ? gui::multiplayer::PicksOnLand(*limits, request.conditions)
		                                : std::vector<game_end::ConditionPick> {};
		if (const auto refusal = gui::multiplayer::ConditionsRefusal(limits, picks); refusal.has_value())
		{
			SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "The server cannot start: {} ({})", *refusal, limitsLand);
			return false;
		}
		for (const auto& pick : picks)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "The winning condition {} has the value {}",
			                   gui::multiplayer::ConditionKey(static_cast<size_t>(pick.condition)), pick.value);
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The network game has {} winning conditions in the limits of {}", picks.size(),
		                   limitsLand);
		_conditionPicks = std::move(picks);
		if (Locator::gameEndSystem::has_value())
		{
			Locator::gameEndSystem::value().SetConditions(_conditionPicks);
		}
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
	if (_scenario != nullptr)
	{
		_scenario->BeforeTurn();
	}
	// A command of the scenario can load a land, which starts the turn count again. The commands of the game on its next
	// land then go to the server before the first turn of that land.
	if (const auto land = _lands.Note(Locator::time::value().GetTurn()); land < _commandLands.size())
	{
		ReceiveCommandsOfLand(land);
	}
}

void ServerWorld::ReceiveCommandsOfLand(uint32_t land)
{
	auto& commands = Locator::playerCommandSystem::value();
	for (; _commandLandsReceived <= land && _commandLandsReceived < _commandLands.size(); ++_commandLandsReceived)
	{
		for (auto& [turn, turnCommands] : _commandLands[_commandLandsReceived])
		{
			commands.Receive(turn, std::move(turnCommands));
		}
		if (_commandLandsReceived > 0)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "The commands of land {} of the game go to the server: {} turns",
			                   _commandLandsReceived, _commandLands[_commandLandsReceived].size());
		}
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
	if (_scenario != nullptr && !_scenarioStarted && !clock.IsPaused())
	{
		_scenarioStarted = true;
		_scenario->Start();
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
	// The puffs, the ghosts and the shakes of the explosions go by the game time of the frame, as on a computer that draws
	// the world. Without this step, the server keeps all of them until the land changes.
	if (Locator::explosionSystem::has_value())
	{
		Locator::explosionSystem::value().Update(
		    std::chrono::duration<float, std::milli>(Locator::time::value().GetFrameGameTime()).count());
	}
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
	// Unless the run is fast, the next frame waits for its due time: the start of the pace plus a fixed game time for each
	// frame. After a slow frame, the next frame starts at once, and the server makes up the lost time. The turns that the
	// server does again after a restart come with no wait, and the pace starts again after them.
	auto& simulation = Locator::simulation::value();
	if (const auto frameTime = clock.GetFixedFrameTime(); sim_run::Paces(simulation.GetSettings(), frameTime) && !_replaying)
	{
		_waiter.WaitUntil(_pace.NextFrame(std::chrono::steady_clock::now(), *frameTime));
	}
	else
	{
		_pace.Restart(std::chrono::steady_clock::now());
	}
	const auto frameEnd = std::chrono::steady_clock::now();
	simulation.FrameEnded(std::chrono::duration<float, std::milli>(frameEnd - _frameStart).count());
	// The run record keeps the time of each stage too. A frame without stage times adds nothing to them.
	if (const auto& entry = profiler.GetEntries().at(profiler.GetEntryIndex(0)); entry.stagesTimed)
	{
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

WorldChangeResult ServerWorld::LoadEditorWorld(std::span<const uint8_t> file, std::span<const PlayerNames> players)
{
	if (!Locator::saveGameSystem::has_value())
	{
		return {.turn = std::nullopt, .refusal = "the server has no world states"};
	}
	// The checksum of the last turn takes the world of that turn, before the load replaces it
	Locator::syncChecksumSystem::value().FinishHash();
	const auto start = std::chrono::steady_clock::now();
	// The file must be a whole world of the land that is open, with the scripts of the land, as each client loads the
	// world of the server
	if (!Locator::saveGameSystem::value().LoadServerWorldState(file))
	{
		return {.turn = std::nullopt, .refusal = "the file is not a whole world of the open land with the scripts of the land"};
	}
	// The world is still the world of a network game
	auto& clock = Locator::time::value();
	clock.SetNetworkGame(true);
	Locator::syncChecksumSystem::value().SetEnabled(true);
	// The gods of the file go to the players of the room before the server sends its world to them
	ecs::world_file_gods::GiveToRoom(players);
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "The world file of the editor ({} bytes) loaded in {:.2f} ms: the world is at turn {}", file.size(),
	                   std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count(),
	                   clock.GetTurn());
	return {.turn = clock.GetTurn(), .refusal = {}};
}

WorldChangeResult ServerWorld::RestartOnScenario(std::string_view id, uint32_t seed)
{
	if (const auto refusal = testbed_scenarios::NetworkRestartRefusal(id); refusal.has_value())
	{
		return {.turn = std::nullopt, .refusal = *refusal};
	}
	auto scenario = testbed_scenarios::MakeScenarioPlayer(id);
	if (scenario == nullptr)
	{
		return {.turn = std::nullopt, .refusal = "the server has no such scenario"};
	}
	Locator::syncChecksumSystem::value().FinishHash();
	// The land starts from the state that every client starts it from, as at the start of a network game
	simulation::PrepareNetworkLand(seed);
	auto& land = Locator::landSystem::value();
	if (!land.LoadTestbed())
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "The testbed of the scenario {} did not load", id);
		return {.turn = std::nullopt, .refusal = "the testbed did not load"};
	}
	if (Locator::gameEndSystem::has_value())
	{
		Locator::gameEndSystem::value().SetConditions(_conditionPicks);
	}
	// The testbed starts no story
	if (!land.StartChallengeScripts(false))
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "The scripts of the testbed of the scenario {} did not start", id);
		return {.turn = std::nullopt, .refusal = "the scripts of the testbed did not start"};
	}
	// As each client starts the land of a start: the things go into the map's cells, and the clock starts from the start
	// of the current turn
	Locator::entitiesMap::value().Sync();
	auto& clock = Locator::time::value();
	clock.Start();
	if (_simulation.frameTime.has_value())
	{
		simulation::LoadEveryResource();
	}
	clock.ResetTimerToTurn();
	clock.SetNetworkGame(true);
	Locator::syncChecksumSystem::value().SetEnabled(true);
	_scenario = std::move(scenario);
	_scenarioStarted = false;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The world starts again on the testbed scenario {} at turn {}", id,
	                   clock.GetTurn());
	return {.turn = clock.GetTurn(), .refusal = {}};
}

std::optional<std::string> ServerWorld::LandRefusal(std::string_view land)
{
	const auto& fileSystem = Locator::filesystem::value();
	const auto path = network_land::PathOf(land, fileSystem.GetPath<filesystem::Path::Scripts>());
	if (!path.has_value())
	{
		return "the name is not a land of the game";
	}
	if (!fileSystem.Exists(*path))
	{
		return "the game has no such land";
	}
	return std::nullopt;
}

WorldChangeResult ServerWorld::RestartOnLand(const LandRestart& restart)
{
	const bool scriptLand = !restart.testbed && !restart.godsRingMap;
	std::optional<std::filesystem::path> path;
	if (scriptLand)
	{
		if (const auto refusal = LandRefusal(restart.land); refusal.has_value())
		{
			return {.turn = std::nullopt, .refusal = *refusal};
		}
		path = network_land::PathOf(restart.land, Locator::filesystem::value().GetPath<filesystem::Path::Scripts>());
	}
	// The server makes the file of the map for 32 gods from its game folder, as at the start of a network game on the
	// map. The clients make the same file, or have it from the lobby.
	std::optional<std::vector<uint8_t>> godsRingMap;
	if (restart.godsRingMap)
	{
		godsRingMap = gods_ring_map_file::MakeFromGameFolder(Locator::filesystem::value());
		if (!godsRingMap.has_value())
		{
			return {.turn = std::nullopt, .refusal = "the server cannot make the map for 32 gods"};
		}
	}
	const auto name = restart.testbed       ? std::string("the testbed")
	                  : restart.godsRingMap ? std::string("the map for 32 gods")
	                                        : std::string(restart.land);
	Locator::syncChecksumSystem::value().FinishHash();
	// The land starts from the state that every client starts it from, as at the start of a network game
	simulation::PrepareNetworkLand(restart.seed);
	auto& landSystem = Locator::landSystem::value();
	landSystem.SetNewGameStart(restart.opening.empty() ? std::nullopt : new_game_choice::ParseNewGameStart(restart.opening));
	const bool loaded = restart.testbed ? landSystem.LoadTestbed()
	                    : godsRingMap.has_value()
	                        ? landSystem.LoadGodsRingMapFile(*godsRingMap)
	                        : landSystem.LoadMap(*path, loading::LoadingClock::Mode::Tips, land_change::Kind::NewGame);
	if (!loaded)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "The land {} of the editor did not load", name);
		return {.turn = std::nullopt, .refusal = "the land did not load"};
	}
	if (Locator::gameEndSystem::has_value())
	{
		Locator::gameEndSystem::value().SetConditions(_conditionPicks);
	}
	// The world of a network game starts no story
	if (!landSystem.StartChallengeScripts(false))
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "The scripts of the land {} of the editor did not start", name);
		return {.turn = std::nullopt, .refusal = "the scripts of the land did not start"};
	}
	// As each client starts the land of a start: the things go into the map's cells, and the clock starts from the start
	// of the current turn
	Locator::entitiesMap::value().Sync();
	auto& clock = Locator::time::value();
	clock.Start();
	if (_simulation.frameTime.has_value())
	{
		simulation::LoadEveryResource();
	}
	clock.ResetTimerToTurn();
	clock.SetNetworkGame(true);
	Locator::syncChecksumSystem::value().SetEnabled(true);
	// A land has no scenario
	_scenario.reset();
	_scenarioStarted = false;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The world starts again on the land {} at turn {}", name, clock.GetTurn());
	if (!godsRingMap.has_value())
	{
		return {.turn = clock.GetTurn(), .refusal = {}};
	}
	auto sha256 = online::sha256::HexOf(*godsRingMap);
	_godsRingMap = std::move(*godsRingMap);
	return {.turn = clock.GetTurn(), .refusal = {}, .mapSha256 = std::move(sha256)};
}

/// The seconds since 1970 (UTC) on the clock of the computer
int64_t UtcSeconds()
{
	return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

/// Watches the limits of the life of the server of a lobby game (deviations 236 and 247): the time to join, the
/// deadline, and the time after the decision of the game. A server with no lobby ends only when every god left.
class LifetimeWatch
{
public:
	explicit LifetimeWatch(const ServerSettings& settings)
	    : _lobbyGame(settings.network.has_value() && settings.network->lobby.has_value())
	{
		if (_lobbyGame)
		{
			_joinWindowSeconds = settings.network->joinWindowSeconds;
			_lobbyDeadline = settings.network->deadline;
		}
	}

	/// The server started to listen: the time to join and the longest life of the server start now
	void Listened()
	{
		_listened = std::chrono::steady_clock::now();
		if (!_lobbyGame)
		{
			return;
		}
		_limits = game_lifetime::Limits {.joinWindowSeconds = _joinWindowSeconds,
		                                 .deadline = game_lifetime::DeadlineOf(_lobbyDeadline, UtcSeconds())};
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "The lobby game: the members have {} seconds to join, and the server ends {} seconds from now at "
		                   "the latest",
		                   _joinWindowSeconds, *_limits.deadline - UtcSeconds());
	}

#if defined(OPENBLACK_GAME_ROLE)
	/// True when the server ends now. The reason is then in Why().
	[[nodiscard]] bool Ends(LockstepHost& host)
	{
		if (!_lobbyGame)
		{
			if (host.GameOver())
			{
				_end = game_lifetime::End::AllGodsLeft;
			}
			return _end != game_lifetime::End::None;
		}
		const auto now = UtcSeconds();
		const auto sinceListen =
		    std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - _listened).count();
		if (!_windowOver && sinceListen >= _joinWindowSeconds)
		{
			_windowOver = true;
			host.EndJoinWindow();
		}
		const auto* gameEnd = Locator::gameEndSystem::has_value() ? &Locator::gameEndSystem::value() : nullptr;
		if (!_decidedAt.has_value() && gameEnd != nullptr && gameEnd->IsDecided())
		{
			_decidedAt = now;
			_decidedByTimeLimit = Locator::time::value().GetTurn() >= game_end::TimeLimitTurn(gameEnd->GetTimeLimit());
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "The {} decided the game: the server ends in {} seconds at the latest",
			                   _decidedByTimeLimit ? "time limit" : "winning conditions",
			                   game_lifetime::k_AfterDecisionSeconds);
		}
		if (!_warned && game_lifetime::WarningDue(_limits.deadline, now))
		{
			_warned = true;
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "The game server ends in {} seconds: its deadline comes",
			                   *_limits.deadline - now);
			host.AnnounceToPlayers(game_lifetime::WarningLine(*_limits.deadline - now));
		}
		_end = game_lifetime::Check(_limits, game_lifetime::Room {.secondsSinceListen = sinceListen,
		                                                          .now = now,
		                                                          .started = host.Started() || host.Starting(),
		                                                          .godsInRoom = host.GodsInRoom(),
		                                                          .minGods = host.GodsToWaitFor(),
		                                                          .gameOver = host.GameOver(),
		                                                          .decidedAt = _decidedAt,
		                                                          .decidedByTimeLimit = _decidedByTimeLimit});
		return _end != game_lifetime::End::None;
	}
#endif

	/// Why the server ends, or End::None
	[[nodiscard]] game_lifetime::End Why() const { return _end; }

private:
	bool _lobbyGame {false};
	int64_t _joinWindowSeconds {game_lifetime::k_DefaultJoinWindowSeconds};
	std::optional<int64_t> _lobbyDeadline;
	game_lifetime::Limits _limits;
	std::chrono::steady_clock::time_point _listened;
	bool _windowOver {false};
	bool _warned {false};
	std::optional<int64_t> _decidedAt;
	bool _decidedByTimeLimit {false};
	game_lifetime::End _end {game_lifetime::End::None};
};

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
	/// in the journal when it changed, and each new file of the editor before the turn that can use it
	void BeforeTurn(const LockstepHost& host)
	{
		if (_ended || _settings.everyTurns == 0 || !host.Started() || host.Replaying() || !Locator::saveGameSystem::has_value())
		{
			return;
		}
		auto room = host.RoomRecord();
		auto blocks = host.BlocksInTurns();
		const auto turn = host.TurnsDone();
		const auto files = host.KeptEditorFiles();
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
			// The snapshot keeps every file of the editor, so the new journal starts with none
			_lastFiles = files;
			_writer->Queue(fmt::format("the snapshot of turn {}", turn), [store = &_store, make = std::move(make),
			                                                              room = std::move(room), blocks = std::move(blocks),
			                                                              editorFiles = host.EditorFilesAfter({}), turn,
			                                                              takenMs]() mutable {
				const auto writeStart = std::chrono::steady_clock::now();
				const auto bytes = snapshots::EncodeSnapshot(
				    {.turn = turn, .room = room, .world = make(), .blocks = blocks, .editorFiles = std::move(editorFiles)});
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
		// A new file goes into the journal before the turn of the command that uses it
		if (files != _lastFiles && _store.JournalOpen())
		{
			for (const auto& file : host.EditorFilesAfter(_lastFiles))
			{
				_store.AddEditorFile(file);
			}
			_lastFiles = files;
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

	/// The editor changed the world of the room. A world that a snapshot can continue gets a new snapshot before the next
	/// turn. A room that plays a testbed scenario keeps no snapshot, as the scenario keeps its state out of the world.
	/// A room on a new land removes the snapshots of the old land, because the new land counts its turns from its start.
	/// The snapshots of the new land keep the land, so that a server that starts again loads it.
	void WorldChanged(LockstepHost::WorldChange::Kind kind)
	{
		using Kind = LockstepHost::WorldChange::Kind;
		if (_ended)
		{
			return;
		}
		if (kind != Kind::WorldFile)
		{
			_writer->Flush();
			_store.Clear();
		}
		if (kind == Kind::Scenario)
		{
			_ended = true;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "The room plays a testbed scenario: the server removed its snapshots");
			return;
		}
		if (kind == Kind::Land)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "The room plays a new land: the server removed the snapshots of the old land");
			// The data of the world states of the new land are ready before the first snapshot of the land
			Prepare();
		}
		_lastTurn.reset();
	}

	/// A command of the scenario loaded a land before a turn. A new server cannot play the scenario again to that land,
	/// so the snapshots go and the server makes no more.
	void TurnsStartedAgain()
	{
		if (std::exchange(_ended, true))
		{
			return;
		}
		_writer->Flush();
		_store.Clear();
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The scenario loaded a land: the server removed its snapshots");
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
	/// The counts of the files of the editor that the snapshot and the journal keep
	LockstepHost::EditorFileCounts _lastFiles;
	bool _ended {false};
	/// Last, so that it writes the snapshots that wait before the store goes
	std::unique_ptr<ecs::save_files::BackgroundWriter> _writer {std::make_unique<ecs::save_files::BackgroundWriter>(
	    [](const std::string& name) { SPDLOG_LOGGER_WARN(spdlog::get("game"), "The server did not write {}", name); })};
};

/// The winning conditions of the game and their state for the first four players, for the report of the host. The world
/// does not change.
GameRole::HostConditions HostConditionsOfWorld()
{
	if (!Locator::gameEndSystem::has_value())
	{
		return {};
	}
	const auto& gameEnd = Locator::gameEndSystem::value();
	lobby_host_report::ReportedStandings standings {};
	for (const auto& [god, standing] : gameEnd.Standings())
	{
		if (const auto number = static_cast<size_t>(god); number < standings.size())
		{
			standings.at(number) = standing;
		}
	}
	return lobby_host_report::ConditionsOf(gameEnd.GetConditions(), standings);
}

/// The figures of each god of the decided game, for the report to the lobby. The world does not change.
std::vector<lobby_results::GodFigures> GodFiguresOfWorld()
{
	std::vector<lobby_results::GodFigures> gods;
	for (const auto& [god, standing] : Locator::gameEndSystem::value().Standings())
	{
		gods.push_back({.god = god, .place = standing.place, .godsDefeated = 0});
	}
	Locator::entitiesRegistry::value().Each<const ecs::components::Player, const ecs::components::PlayerStatistics>(
	    [&gods](const ecs::components::Player& player, const ecs::components::PlayerStatistics& statistics) {
		    if (const auto god = std::ranges::find(gods, player.name, &lobby_results::GodFigures::god); god != gods.end())
		    {
			    god->godsDefeated = statistics.godsDefeated;
		    }
	    });
	return gods;
}

/// The id of the game for the lobby. A server that keeps snapshots keeps the id in their folder, so that the game that
/// it continues after a restart has the same id. A new game gets a new random id.
std::string GameIdFor(const NetworkSettings& settings, bool restored)
{
	// A game of the lobby has the id that the lobby gave
	if (!settings.gameId.empty())
	{
		return settings.gameId;
	}
	const auto& folder = settings.snapshots.folder;
	const bool kept = settings.snapshots.everyTurns > 0;
	if (kept && restored)
	{
		if (auto id = lobby_results::ReadGameId(folder); id.has_value())
		{
			return std::move(*id);
		}
	}
	std::random_device source;
	std::array<uint8_t, 16> random {};
	std::ranges::generate(random, [&source] { return static_cast<uint8_t>(source()); });
	auto id = lobby_results::GameIdOf(random);
	if (kept && !lobby_results::WriteGameId(folder, id))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The id of the game does not write in {}", folder.generic_string());
	}
	return id;
}

/// The land that the world of the server plays now. The land goes as its path under the Scripts folder, so that each
/// client finds it in its own game folder, and the clients compare the checksum of its file before the start.
PlayedLand PlayedLandOf(const ServerSettings& settings, const ServerWorld& world)
{
	auto& fileSystem = Locator::filesystem::value();
	const bool scriptLand = !settings.testbed && !settings.godsRingMap;
	const auto landPath = Locator::landSystem::value().GetLandPath();
	PlayedLand played {.name = scriptLand ? network_land::WireName(landPath) : std::string(),
	                   .checksum = online::start_rules::LandChecksum({}),
	                   .sha256 = {}};
	if (settings.customMap.has_value())
	{
		// The clients compare the checksum of the file that they got from the lobby
		played.checksum = online::start_rules::LandChecksum(settings.customMap->bytes);
		played.sha256 = online::sha256::HexOf(settings.customMap->bytes);
	}
	else if (scriptLand && fileSystem.Exists(landPath))
	{
		const auto bytes = fileSystem.ReadAll(landPath);
		played.checksum = online::start_rules::LandChecksum(bytes);
		played.sha256 = online::sha256::HexOf(bytes);
	}
	// The map for 32 gods is a file that the players get from the lobby, so the clients compare its checksum too
	if (settings.godsRingMap)
	{
		played.name = gods_ring_map_file::WireName();
		played.checksum = online::start_rules::LandChecksum(world.GodsRingMapFile());
		played.sha256 = online::sha256::HexOf(world.GodsRingMapFile());
	}
	return played;
}

/// Starts the game role after the land loaded: the clients start the same land with the same seed. A server that
/// continues a game from a snapshot restores the room of the game. The land of the command line is the land that the
/// server loaded first. The snapshot of a room that changed its land has the world of another land.
bool StartNetwork(const ServerSettings& settings, ServerWorld& world, const PlayedLand& commandLineLand,
                  std::unique_ptr<GameRole>& network, const std::function<void(uint32_t turn, uint64_t checksum)>& onTurn,
                  SnapshotKeeper* keeper, const std::optional<snapshots::Restored>& restored, LifetimeWatch& lifetime,
                  std::shared_ptr<ServerDesyncs>& desyncs)
{
	network = std::make_unique<GameRole>();
	// The clients take the land, the seed and the start from the server as they join
	const bool scriptLand = !settings.testbed && !settings.godsRingMap;
	// The server of a lobby game plays only the file of the land that the lobby gave its members
	if (!settings.network->mapSha256.empty() && settings.network->mapSha256 != commandLineLand.sha256)
	{
		SPDLOG_LOGGER_CRITICAL(
		    spdlog::get("game"), "The file of the land has the SHA-256 {}, but the lobby gave its members the file {}",
		    commandLineLand.sha256.empty() ? std::string("of no file") : commandLineLand.sha256, settings.network->mapSha256);
		// The role has no host yet, so the end of the run must not read it
		network.reset();
		return false;
	}
	// The start that the clients got before the restart, and the start of the command line of that game. These are the
	// same start when the land of the room did not change.
	std::optional<game_socket::Start> savedStart;
	std::optional<game_socket::Start> firstStart;
	const bool landChanged = restored.has_value() && !restored->room.firstStart.empty();
	if (restored.has_value())
	{
		savedStart = StartOfText(restored->room.start);
		firstStart = landChanged ? StartOfText(restored->room.firstStart) : savedStart;
	}
	// The clients load the land of the world of the server. After a change of the land, this is the land of the snapshot.
	auto played = commandLineLand;
	if (landChanged && savedStart.has_value())
	{
		auto changedSettings = settings;
		changedSettings.testbed = savedStart->testbed;
		changedSettings.godsRingMap = savedStart->godsRingMap;
		changedSettings.customMap.reset();
		played = PlayedLandOf(changedSettings, world);
	}
	game_socket::Start start {
	    .land = commandLineLand.name,
	    .testbed = settings.testbed,
	    .seed = RoomStartFor(settings).seed,
	    .turn = Locator::time::value().GetTurn(),
	    .opening = scriptLand ? settings.openingName : std::string(),
	    .godsRingMap = settings.godsRingMap,
	    .allComputerGods = settings.soak.allComputerGods,
	    .aiSeed = settings.soak.aiSeed,
	    .scenario = settings.scenario,
	    .timeLimit = settings.timeLimit.value_or(game_end::k_DefaultTimeLimitMinutes),
	    .gameId = settings.network.has_value() ? settings.network->gameId : std::string(),
	    .conditions = {},
	    .gods = {},
	    .randomAlignmentSlots = settings.randomAlignmentSlots,
	};
	for (const auto& pick : world.ConditionPicks())
	{
		start.conditions.push_back(
		    {.condition = static_cast<uint8_t>(pick.condition), .value = static_cast<uint32_t>(pick.value)});
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
	if (!settings.network->slotKinds.empty())
	{
		std::vector<std::string_view> names;
		std::ranges::transform(settings.network->slotKinds, std::back_inserter(names), lobby_api::SlotKindName);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The kinds of the god slots from slot 0: {}", fmt::join(names, ","));
	}
	// The game of a snapshot continues with the start that its clients got
	if (restored.has_value())
	{
		if (!savedStart.has_value() || !firstStart.has_value() || !snapshots::SameGame(*firstStart, start))
		{
			SPDLOG_LOGGER_CRITICAL(spdlog::get("game"),
			                       "The snapshot is of another game than the land and the seed of this server");
			return false;
		}
		start = *savedStart;
		// The room keeps its god slots on a new land
		if (landChanged)
		{
			godSlots = static_cast<uint8_t>(std::min<size_t>(restored->room.seats.size(), player_slots::k_MaxPlayers));
		}
	}
	// The players do not have the map for 32 gods: the server sends its file to the lobby (deviation 169)
	std::optional<lobby_status::CustomMap> customMap;
	if (settings.godsRingMap)
	{
		// The players make the file from the map package and their own game data (deviation 249)
		const auto package = gods_ring_map_package::Make();
		customMap = lobby_status::CustomMap {.file = std::string(gods_ring_map::k_File),
		                                     .name = std::string(gods_ring_map::k_Name),
		                                     .players = static_cast<int32_t>(gods_ring_map::k_Slots),
		                                     .bytes = world.GodsRingMapFile(),
		                                     .packageSha256 = online::sha256::HexOf(package),
		                                     .packageSize = static_cast<int64_t>(package.size())};
	}
	else if (settings.customMap.has_value())
	{
		// The land of --custom-map or --map-package has a god for each of its citadels. Its shown name is the name of
		// its package, its file name, or the name of the land of the map download test. The lobby gives the players the
		// package of a land from a package.
		const auto& file = settings.customMap->file;
		auto name = settings.customMap->name.empty() ? file : settings.customMap->name;
		customMap = lobby_status::CustomMap {
		    .file = file,
		    .name = file == download_test_map::k_File ? std::string(download_test_map::k_Name) : std::move(name),
		    .players = static_cast<int32_t>(godSlots),
		    .bytes = settings.customMap->bytes,
		    .packageSha256 = settings.customMap->packageSha256,
		    .packageSize = settings.customMap->packageSize,
		};
	}
	const auto landSha256Logged = played.sha256;
	if (const auto error =
	        network->Start(*settings.network, start, played.checksum, godSlots, std::move(customMap), std::move(played.sha256));
	    error.has_value())
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "The game role cannot start: {}", *error);
		network.reset();
		return false;
	}
	auto& host = network->Host();
	if (restored.has_value())
	{
		host.Restore(restored->room, restored->snapshot.turn, restored->turns, restored->blocks, restored->editorFiles);
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
	// The lobby gets the end of the game one time, with the figures of the turn that decided it
	const lobby_results::Game game {.id = GameIdFor(*settings.network, restored.has_value()), .land = start.land, .turn = 0};
	// Each desync of a client goes to the error reports, inside the flood limits. The events have the id of the game.
	desyncs = std::make_shared<ServerDesyncs>(
	    ServerDesyncs {.rule = error_reports::DesyncReportRule(game.id, turn_codec::k_Version), .budget = {}});
	desyncs->players = [&host] { return host.ReportedPlayers(); };
	host.SetDesyncListener([desyncs](uint8_t seat, uint32_t turn, uint64_t server, uint64_t client) {
		desyncs->rule.Detected(seat, {.turn = turn, .ownChecksum = server, .otherChecksum = client, .slot = seat},
		                       ServerDesyncs::Now());
	});
	host.SetPartedListener([desyncs](uint8_t seat, std::span<const std::string> parts) {
		desyncs->rule.Parted(seat, parts, ServerDesyncs::Now());
	});
	// The game data that the server's world read: each player's computer sends the print of the same files from its own
	// game folder, and the room refuses a player whose land, scripts or table of objects differ
	auto& fileSystem = Locator::filesystem::value();
	auto print = online::game_data::PrintOfReadFiles(fileSystem);
	const auto hints = online::game_data::HintsOfGameFolder(fileSystem);
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "The print of the server's game data has {} files, hash {:016x} (patch {}, language {})", print.size(),
	                   online::game_data::HashOfPrint(print), hints.patch.empty() ? "not known" : hints.patch,
	                   hints.language.empty() ? "not known" : hints.language);
	// The facts of the server's world, which the capture of a desync compares with the facts of the player
	game_socket::SyncFacts facts {
	    .turn = 0,
	    .build = online::build_id::Own(),
	    .land = start.land,
	    .landSha256 = landSha256Logged,
	    .map = settings.network->mapSha256,
	    .seed = start.seed,
	    .data = online::game_data::HashOfPrint(print),
	    .patch = hints.patch,
	    .language = hints.language,
	};
	host.SetDataPrint(std::move(print));
	// The capture of the first desync of each god: the record goes to the disk, and its detail goes with the event
	const auto records = settings.network->desyncRecords;
	desync_bundle::Prune(records, std::chrono::system_clock::now());
	desyncs->rule.SetDetailWait(true);
	host.SetCapture(
	    std::move(facts),
	    [](std::span<const std::string> parts) { return Locator::syncChecksumSystem::value().EntitiesOfParts(parts); },
	    [desyncs, records, gameId = game.id](CaptureRecord record) {
		    auto logger = spdlog::get("game");
		    for (const auto& line : desync_bundle::Lines(record))
		    {
			    SPDLOG_LOGGER_WARN(logger, "{}", line);
		    }
		    const auto folder = desync_bundle::Write(records, record, gameId, std::chrono::system_clock::now());
		    const auto name = folder.has_value() ? folder->filename().string() : std::string("not written");
		    if (folder.has_value())
		    {
			    SPDLOG_LOGGER_INFO(logger,
			                       "The server keeps the record of the desync in {} ({} bytes of the server's world, {} of "
			                       "the player's world)",
			                       folder->generic_string(), record.serverWorld.size(), record.playerWorld.size());
		    }
		    else
		    {
			    SPDLOG_LOGGER_WARN(logger, "The server could not write the record of the desync in {}",
			                       records.folder.generic_string());
		    }
		    desyncs->rule.Detailed(record.seat, desync_bundle::DetailOf(record, gameId, name));
	    });
	// The log names the parts that differ when a client sends its parts of the turn at which its world parted
	host.SetPartsOfTurn([](uint32_t turn) { return Locator::syncChecksumSystem::value().PartsOfTurn(turn); });
	// A client whose world parted gets the world of the server, at the start of the next turn
	host.SetWorldState([] {
		return Locator::saveGameSystem::has_value() ? Locator::saveGameSystem::value().WorldState() : std::vector<uint8_t> {};
	});
	// A world file, a testbed scenario or a land of the editor of a player changes the world between two turns
	host.SetScenarioRefusal([](std::string_view id) { return testbed_scenarios::NetworkRestartRefusal(id); });
	host.SetLandRefusal([](std::string_view land) { return ServerWorld::LandRefusal(land); });
	host.SetChangeWorld([&world, keeper](const LockstepHost::WorldChange& change) {
		using Kind = LockstepHost::WorldChange::Kind;
		const bool file = change.kind == Kind::WorldFile;
		const auto changed = file                            ? world.LoadEditorWorld(change.file, change.players)
		                     : change.kind == Kind::Scenario ? world.RestartOnScenario(change.scenario, change.seed)
		                                                     : world.RestartOnLand({.land = change.land,
		                                                                            .testbed = change.testbed,
		                                                                            .godsRingMap = change.godsRingMap,
		                                                                            .opening = change.opening,
		                                                                            .seed = change.seed});
		if (changed.turn.has_value() && keeper != nullptr)
		{
			keeper->WorldChanged(change.kind);
		}
		return LockstepHost::WorldChanged {.turn = changed.turn, .refusal = changed.refusal, .mapSha256 = changed.mapSha256};
	});
	if (const auto& lobby = settings.network->lobby; lobby.has_value())
	{
		std::string members;
		for (const auto& member : lobby->members)
		{
			members += fmt::format("{}{}{}", members.empty() ? "" : ", ", game_socket::RoleName(member.role),
			                       member.slot.has_value() ? fmt::format(" of slot {}", *member.slot) : std::string());
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game {} of the lobby {}, build {}, land {} (SHA-256 {}): members {}",
		                   settings.network->gameId, lobby->id, online::build_id::Own(), start.land, landSha256Logged, members);
	}
	lifetime.Listened();
	world.SetNetwork([&host, role = network.get(), keeper, &lifetime, desyncs, appliedClockSpeed = std::optional<float> {},
	                  appliedChanges = uint32_t {0}, appliedRestarts = uint32_t {0}]() mutable {
		// After a restart, the world first does again the turns that the server sent. The server reads no socket until then.
		if (host.Replaying())
		{
			return ServerWorld::NetworkStep::Replay;
		}
		role->Update();
		desyncs->Update();
		// The world of the server turns at the speed of the clock of the room, as the world of each player does. After a
		// restart the room starts again at the normal speed. A world file or a scenario of the editor sets the speed of
		// the clock of its world, so the server gives the clock the speed of the room again after each change of the world.
		// A fast room gives more turns than one in a frame, so the server then plays more turns in a frame.
		// A land that a command of the scenario loaded sets the speed of the clock too.
		const bool restarted = host.TurnRestarts() != appliedRestarts;
		const auto& room = host.RoomClock();
		if (const auto clockSpeed = online::room_clock::ClockSpeedOf(room.speed, room.period);
		    clockSpeed != appliedClockSpeed || host.WorldChanges() != appliedChanges || restarted)
		{
			auto& clock = Locator::time::value();
			clock.SetSpeed(clockSpeed);
			clock.SetMostTurnsInFrame(online::room_clock::ServerTurnsInFrame(room.speed, clock.GetFixedFrameTime()));
			appliedClockSpeed = clockSpeed;
			appliedChanges = host.WorldChanges();
			appliedRestarts = host.TurnRestarts();
		}
		if (restarted && keeper != nullptr)
		{
			keeper->TurnsStartedAgain();
		}
		// No computer god of the server's world takes the slots that the lobby closed. The start gives them before the
		// first turn, in which the humans join.
		if (host.Started() && Locator::computerPlayerSystem::has_value())
		{
			Locator::computerPlayerSystem::value().CloseSlots(host.ClosedSlots());
			// The computer gods start at the alignments that the host chose, as on each client (deviation 280)
			Locator::computerPlayerSystem::value().SetStartAlignments(
			    computer_player::AlignmentsOfRandomSlots(host.RandomAlignmentSlots()));
		}
		if (role->HostReportDue())
		{
			role->SendHostReport(Locator::time::value().GetTurn(), false, HostConditionsOfWorld());
		}
		if (host.GameOver())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Every god left the game: the game ends");
			host.FinishCaptures();
			desyncs->Finish();
		}
		if (lifetime.Ends(host))
		{
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
	    [&host, onTurn, role = network.get(), game,
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
		    if (Locator::gameEndSystem::has_value() && Locator::gameEndSystem::value().IsDecided())
		    {
			    role->SendHostReport(turn.turn, true, HostConditionsOfWorld());
		    }
		    if (role->WantsResult() && Locator::gameEndSystem::has_value() && Locator::gameEndSystem::value().IsDecided())
		    {
			    auto decided = game;
			    decided.turn = turn.turn;
			    role->ReportResult(decided, GodFiguresOfWorld());
		    }
	    });
#if defined(OPENBLACK_INSPECTOR)
	// The operator reads the room through the inspector
	if (Locator::inspector::has_value())
	{
		Locator::inspector::value().AddProvider(inspector::MakeServerProvider([&host] { return admin::RoomReportOf(host); }));
	}
#endif
	return true;
}
#else
/// A build with no game role keeps no snapshots
class SnapshotKeeper
{
public:
	explicit SnapshotKeeper(const snapshots::Settings& /*settings*/) {}
};

bool StartNetwork(const ServerSettings& /*settings*/, ServerWorld& /*world*/, const PlayedLand& /*commandLineLand*/,
                  std::unique_ptr<int>& /*network*/, const std::function<void(uint32_t turn, uint64_t checksum)>& /*onTurn*/,
                  SnapshotKeeper* /*keeper*/, const std::optional<snapshots::Restored>& /*restored*/,
                  LifetimeWatch& /*lifetime*/, std::shared_ptr<ServerDesyncs>& /*desyncs*/)
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
	// The room changed its land after the start: the world loads that land first, and then the world of the snapshot
	if (!restored->room.firstStart.empty())
	{
		const auto start = StartOfText(restored->room.start);
		if (!start.has_value())
		{
			SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "The start of the snapshot does not read");
			return false;
		}
		const auto name = start->testbed ? std::string("of the testbed") : start->land;
		if (const auto changed = world.RestartOnLand({.land = start->land,
		                                              .testbed = start->testbed,
		                                              .godsRingMap = start->godsRingMap,
		                                              .opening = start->opening,
		                                              .seed = start->seed});
		    !changed.turn.has_value())
		{
			SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "The land {} of the snapshot does not load: {}", name, changed.refusal);
			return false;
		}
		// The test tools find the words "the snapshot is of the land"
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The snapshot is of the land {}, which the room changed to", name);
	}
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
	// The limits of the life of the server of a lobby game
	LifetimeWatch lifetime(settings);
	std::shared_ptr<ServerDesyncs> desyncs;
	if (settings.network.has_value() && settings.network->snapshots.everyTurns > 0)
	{
		keeper = std::make_unique<SnapshotKeeper>(settings.network->snapshots);
	}
	bool ready = world->StartLand();
	// The land of the command line, before the land of a snapshot replaces it
	PlayedLand commandLineLand;
#if defined(OPENBLACK_GAME_ROLE)
	if (ready && settings.network.has_value())
	{
		commandLineLand = PlayedLandOf(settings, *world);
	}
#endif
	if (ready && settings.network.has_value() && settings.network->snapshots.resume)
	{
		ready = PrepareResume(settings, *world, restored);
	}
	if (!ready)
	{ // NOLINT(bugprone-branch-clone): the branches stay apart to show each case
		exitCode = EXIT_FAILURE;
	}
	else if (settings.network.has_value() &&
	         !StartNetwork(settings, *world, commandLineLand, network, onTurn, keeper.get(), restored, lifetime, desyncs))
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
		else if (lifetime.Why() != game_lifetime::End::None)
		{
			exitCode = game_lifetime::ExitCodeOf(lifetime.Why());
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "{}", game_lifetime::LineOf(lifetime.Why()));
		}
		LogGraphicsLibrary();
	}
	checksums.SetTurnListener({});
	// The summary of the desyncs goes when the game did not end before the server stops
	if (desyncs)
	{
		desyncs->Finish();
	}
	// The writer of the snapshots finishes before the services of the world stop
	keeper.reset();
#if defined(OPENBLACK_GAME_ROLE)
	if (network)
	{
		// A room that the server ends before the start tells each client, so that no client tries to come back
		if (lifetime.Why() != game_lifetime::End::None && !network->Host().Started())
		{
			network->Host().EndRoom(lobby_game::k_EndedRefusal);
		}
		for (const auto& line : network->Host().SummaryLines())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Network game: {}", line);
		}
#if defined(OPENBLACK_INSPECTOR)
		// The queries of the room read the host, so they go before the host
		if (Locator::inspector::has_value())
		{
			Locator::inspector::value().RemoveProvider(inspector::k_ServerProviderName);
		}
#endif
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
