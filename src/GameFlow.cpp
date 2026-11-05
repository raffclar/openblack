/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The flow of the game: how the game loads lands and scenarios, the end of the game and its save, the trips to the
// playgrounds, the music of each turn, and the logs and tools of a tool's run

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include <GameSocket.h>
#include <LHVM.h>
#include <SaveSlots.h>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/HandAnimation.h"
#include "3D/HandMorph.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/SkyDome.h"
#include "Audio/AudioManagerInterface.h"
#include "CHLApi.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/ScriptCameraModel.h"
#include "Commands/PlayerCommands.h"
#include "Common/BufferedLogSink.h"
#include "Common/DeveloperToolsGate.h"
#include "Game.h"
#include "GameDetail.h"
#include "Sound/ClipSounds.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/HelpSpeech.h"
#include "Sound/ScriptSoundEffect.h"
#if defined(OPENBLACK_DEVELOPER_TOOLS)
#include "Common/DrawTrace.h"
#endif
#include "Common/GameRandom.h"
#include "Creature/LeashOrders.h"
#include "Debug/DebugGuiInterface.h"
#include "Debug/FightAutopilotInterface.h"
#include "Debug/FrameStatsLog.h"
#include "Debug/FrameWriteGuard.h"
#include "ECS/Components/Alignment.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/GamePlayers.h"
#include "ECS/LearningMarker.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/ClanCreatureSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FightTranscriptSystemInterface.h"
#include "ECS/Systems/GameAudioSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/GatheringBoxSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/InspectorSystemInterface.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/PlaygroundTripSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/SimulationSystemInterface.h"
#include "ECS/Systems/SkyClockSystemInterface.h"
#include "ECS/Systems/SyncChecksumSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Gui/Canvas.h"
#include "Gui/EscapeKeyRules.h"
#include "Gui/GameInterface.h"
#include "Gui/MultiplayerFrontEnd.h"
#include "Gui/PingDisplay.h"
#include "Gui/StatisticsBoxRules.h"
#include "Hand/HandGrabRules.h"
#include "Hand/HandTrace.h"
#include "Hand/ScriptHandState.h"
#include "Help/DialogueText.h"
#include "Help/Spirits.h"
#include "Help/TurnCheer.h"
#include "Input/GameActionMapInterface.h"
#include "LHScriptX/Script.h"
#include "LivingTurnOrder.h"
#include "Locator.h"
#if !defined(__EMSCRIPTEN__)
#include "Online/Desktop/UdpLanDiscovery.h"
#endif
#include "Online/HttpClientInterface.h"
#include "Online/LanDiscoveryInterface.h"
#include "Online/NetworkLand.h"
#include "Online/NetworkSession.h"
#include "Online/OnlineMapStoreInterface.h"
#include "Online/OpenPage.h"
#include "Online/WebSocketInterface.h"
#include "Particles/LiquidParticles.h"
#include "PlayerSlots.h"
#include "Profiler.h"
#include "Resources/GameAssets.h"
#include "Serializer/FotFile.h"
#include "Simulation/ChecksumLog.h"
#include "Simulation/CommandLog.h"
#include "Simulation/FrameBody.h"
#include "Simulation/ProfileMenu.h"
#include "Simulation/TurnBody.h"
#include "Simulation/WorldStart.h"
#include "Temple/TempleHelp.h"
#include "Video/VideoRules.h"
#include "Visual/DetailLevel.h"

#if defined(OPENBLACK_INSPECTOR)
#include "Inspector/GameSeam.h"
#endif

using namespace openblack;
using namespace openblack::game_detail;

void Game::RecordScenarioGive(uint32_t turn, size_t command)
{
	if (_scenarioGives != nullptr)
	{
		_scenarioGives->Write(turn, command);
	}
}

void Game::FollowGameEnd()
{
	if (!Locator::gameInterface::has_value() || !Locator::gameEndSystem::has_value())
	{
		return;
	}
	// Tests: at a turn, the player leaves the network game as a player does
	if (const auto leaveAt = Locator::simulation::value().GetSettings().leaveAt;
	    leaveAt.has_value() && !_testLeaveDone && Locator::time::value().GetTurn() >= *leaveAt && InNetworkGame())
	{
		_testLeaveDone = true;
		LeaveAsAPlayerDoes();
	}
	auto& gameEnd = Locator::gameEndSystem::value();
	const auto* session = Locator::networkGame::has_value() ? Locator::networkGame::value().Session() : nullptr;
	// The end box stops to show itself when it closes, or when the box of one of its tabs opens over it
	if (!Locator::gameInterface::value().GetEndGameBox().ShowsItself())
	{
		end_game_box::StoppedShowing(_endGameBoxState);
	}
	// The player's choice in the end box: Leave Game leaves the network game. In a skirmish it ends the skirmish, and the
	// skirmish box shows again.
	if (Locator::gameInterface::value().GetEndGameBox().TakeAction() == gui::EndGameBox::Action::Leave)
	{
		if (session != nullptr)
		{
			LeaveNetworkGame();
			return;
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The end box: Leave Game ends the skirmish");
		_gameEndAsked = game_session::End::Skirmish;
	}
	const auto events = gameEnd.TakeEventsToShow();
	if (events.empty())
	{
		return;
	}
	const auto local = LocalGodForGameEnd();
	const bool networkGame = Locator::time::has_value() && Locator::time::value().IsNetworkGame();
	const auto opened = end_game_box::Follow(events, local, networkGame, _endGameBoxState);

	// The name of a god: the name of the player of its slot in a network game, the profile's name for the god of this
	// computer in another game
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	const auto nameOf = [session, local, &profile](PlayerNames god) {
		std::optional<std::u16string> human;
		if (session != nullptr && session->GetStart().has_value())
		{
			// The names follow the god slots: the gods after the neutral player have the slot before their number
			const auto& names = session->GetStart()->names;
			if (const auto slot = player_slots::SlotOfPlayer(god); slot.has_value() && *slot < names.size())
			{
				human = gui::ToUtf16(names.at(*slot));
			}
		}
		else if (god == local && profile.has_value())
		{
			human = gui::ToUtf16(profile->name);
		}
		return end_game_box::GodName(god, human.has_value() ? std::optional<std::u16string_view>(*human) : std::nullopt);
	};
	if (opened.godOut.has_value())
	{
		Locator::gameInterface::value().ShowGodOut(nameOf(*opened.godOut));
	}
	if (!opened.endBox.has_value())
	{
		return;
	}
	const auto& texts = Locator::gameInterface::value().GetTexts();
	const auto winner = gameEnd.LastGodIn();
	const auto winnerName = winner.has_value() ? nameOf(*winner) : std::u16string();
	const bool looksOn = end_game_box::LooksOn(local, networkGame);
	const auto title = end_game_box::Title(opened.endBox->kind, looksOn, winnerName,
	                                       {.leave = texts.Get("HELP_TEXT_DIALOG_AREYOUSUREQUIT"),
	                                        .lost = texts.Get("HELP_TEXT_DIALOG_ADDITION_152"),
	                                        .won = texts.Get("HELP_TEXT_WIN_GAME_04"),
	                                        .gameOver = texts.Get("HELP_TEXT_GAME_OVER"),
	                                        .winnerFormat = u"%s - %s Won"});
	// The band under the title has the colour of the player's god. A looker-on has no god that takes part, and the game
	// does not draw its band (vanilla-bugs 470).
	const auto landNumber = Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber;
	const auto colour = looksOn ? glm::vec4(0.0f) : gui::statistics_box::PlayerColour(local, landNumber);
	Locator::gameInterface::value().OpenEndGameBox({.box = *opened.endBox, .title = title, .colour = colour});
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The end box opens for player {}: {}", static_cast<int>(local), gui::ToUtf8(title));
	// In a game on the internet, an end box of a decided game or of a god that is out sends the clan creature of the god
	// of this computer to the lobby, with the alignment that the god has now
	if (session != nullptr && session->Origin() == online::GameOrigin::InternetLobby &&
	    opened.endBox->kind != end_game_box::Kind::LeaveQuestion && session->OwnGod().has_value() &&
	    Locator::clanCreatureSystem::has_value())
	{
		const auto player = Locator::playerSystem::value().FindPlayer(*session->OwnGod());
		const auto* alignment =
		    player.has_value() ? Locator::entitiesRegistry::value().TryGet<const ecs::components::Alignment>(*player) : nullptr;
		static_cast<void>(Locator::clanCreatureSystem::value().SendAtGameEnd(alignment != nullptr ? alignment->value : 0.0f));
	}
	// The end box of a decided game asks the game server to stop the turns of the game
	if (session != nullptr && end_game_box::StopsTheGame(*opened.endBox, _endGameBoxState))
	{
		Locator::networkGame::value().StopGame();
	}
}

void Game::StartSimulationLogs()
{
	if (!_simulation.has_value())
	{
		return;
	}
	if (_simulation->frameGuard != sim_run::HashDetail::None)
	{
		_frameGuard = std::make_unique<frame_write_guard::Guard>(_simulation->frameGuard == sim_run::HashDetail::Fields,
		                                                         _simulation->frameGuardSystems);
	}
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	if (_simulation->traceDrawsPath.has_value())
	{
		if (!draw_trace::Start(Locator::gameRandom::value(), *_simulation->traceDrawsPath))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the draw trace cannot open: {}",
			                    _simulation->traceDrawsPath->string());
		}
	}
#endif
	if (_simulation->checksumsPath.has_value())
	{
		_checksumLog = std::make_unique<checksum_log::Writer>(*_simulation->checksumsPath);
		if (_checksumLog->IsOpen())
		{
			auto& checksums = Locator::syncChecksumSystem::value();
			checksums.SetEnabled(true);
			checksums.SetKeepParts(_simulation->checksumParts != sim_run::HashDetail::None,
			                       _simulation->checksumParts == sim_run::HashDetail::Fields);
			if (_simulation->checksumDump.has_value())
			{
				checksums.SetDump(_simulation->checksumDump->first, _simulation->checksumDump->second);
			}
			checksums.SetTurnListener(
			    [log = _checksumLog.get()](const ecs::systems::SyncChecksumSystemInterface::TurnChecksum& turn) {
				    log->Write(turn.turn, turn.checksum, turn.parts, turn.unregistered);
				    if (!turn.dump.empty())
				    {
					    log->WriteDump(turn.turn, turn.dump);
				    }
			    });
		}
		else
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the checksums cannot open: {}",
			                    _simulation->checksumsPath->string());
		}
	}
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	if (_simulation->rowDumpPath.has_value())
	{
		// The rows come from the checksum of each turn, so the checksum is on also in a single player game
		auto& checksums = Locator::syncChecksumSystem::value();
		if (checksums.StartRowDump(*_simulation->rowDumpPath, _simulation->rowDump))
		{
			checksums.SetEnabled(true);
		}
		else
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the row dump cannot open: {}",
			                    _simulation->rowDumpPath->string());
		}
	}
	if (_simulation->worldStates.has_value())
	{
		Locator::syncChecksumSystem::value().WriteWorldStates(*_simulation->worldStates);
	}
#endif
	if (_simulation->playCommandsPath.has_value())
	{
		auto turns = command_log::ReadFile(*_simulation->playCommandsPath);
		if (!turns.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the commands does not read: {}",
			                    _simulation->playCommandsPath->string());
			return;
		}
		auto& commands = Locator::playerCommandSystem::value();
		for (auto& [turn, turnCommands] : *turns)
		{
			commands.Receive(turn, std::move(turnCommands));
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Commands of {} turns to play from {}", turns->size(),
		                   _simulation->playCommandsPath->string());
	}
}

void Game::StartFightTools()
{
	// A tool that steps a simulation run has each land start paused
	if (_simulation.has_value() && _simulation->startPaused && Locator::inspector::has_value())
	{
		Locator::inspector::value().PauseOnLoad(true);
	}
	// The run record of a simulation run starts with the inspector
	if (_simulation.has_value() && _simulation->recordPath.has_value() && Locator::inspector::has_value())
	{
		if (const auto error = Locator::inspector::value().StartSimRecord(*_simulation->recordPath, _simulation->recordEvery,
		                                                                  _simulation->limitsPath, _simulation->hashParts,
		                                                                  _simulation->recordFieldsEvery);
		    error.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The simulation record cannot start: {}", *error);
		}
	}
	// A join from the command line starts the input on the server's land
	if (!_networkLandPending)
	{
		StartInputTools();
	}
	// A tool's run can write the commands of each turn, for a dedicated server that does the same commands
	if (_simulation.has_value() && _simulation->commandsPath.has_value())
	{
		_commandLog = std::make_unique<command_log::Writer>(*_simulation->commandsPath);
		_scenarioGives = std::make_unique<command_log::GivesWriter>(command_log::GivesPathOf(*_simulation->commandsPath));
		if (_commandLog->IsOpen())
		{
			Locator::playerCommandSystem::value().SetTurnListener(
			    [log = _commandLog.get()](uint32_t turn, std::span<const player_commands::Command> commands) {
				    log->Write(turn, commands);
			    });
		}
		else
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The file of the commands cannot open: {}",
			                    _simulation->commandsPath->string());
		}
	}
	if (_fightTranscriptFolder.has_value() && Locator::fightTranscript::has_value())
	{
		Locator::fightTranscript::value().SetOutputFolder(_fightTranscriptFolder);
	}
	if (!_fightAutopilot.has_value())
	{
		return;
	}
	auto* autopilot = Locator::inspector::has_value() ? Locator::inspector::value().GetFightAutopilot() : nullptr;
	const auto policy = fight_autopilot::PolicyFromName(_fightAutopilot->policy);
	const auto stage = fight_autopilot::LessonStageFromName(_fightAutopilot->stage);
	const auto sequence = fight_autopilot::ParseSequence(_fightAutopilot->sequence);
	if (autopilot == nullptr || !policy.has_value() || (!_fightAutopilot->stage.empty() && !stage.has_value()) ||
	    !sequence.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The fight autopilot cannot start: check its policy, stage and sequence");
		return;
	}
	const auto error = autopilot->Start(
	    {.policy = *policy, .seed = _fightAutopilot->seed, .stage = stage, .sequence = *sequence, .enterView = true});
	if (!error.empty())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The fight autopilot cannot start: {}", error);
	}
}

const std::filesystem::path& Game::ChallengePath()
{
	if (_challengePath.has_value())
	{
		return *_challengePath;
	}
	auto& fileSystem = Locator::filesystem::value();
	_challengePath = fileSystem.GetPath<filesystem::Path::Quests>() / "challenge.chl";
	if (!_launchSwitches.settingsFile.has_value())
	{
		return *_challengePath;
	}
	// The game opens the settings file from its own folder. When the file does not open, the usual folder stays.
	const std::filesystem::path settingsFile(*_launchSwitches.settingsFile);
	if (!fileSystem.Exists(settingsFile))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Settings file {} not found", settingsFile.generic_string());
		return *_challengePath;
	}
	const auto bytes = fileSystem.ReadAll(settingsFile);
	const auto folder =
	    launch_switches::QuestFolderOf(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
	if (folder.has_value())
	{
		_challengePath = std::filesystem::path(launch_switches::ChallengePath(*folder));
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Settings file {}: the challenge file is {}", settingsFile.generic_string(),
		                   _challengePath->generic_string());
	}
	return *_challengePath;
}

bool Game::LoadMap(const std::filesystem::path& path, loading::LoadingClock::Mode look, land_change::Kind change) noexcept
{
	return Locator::landSystem::value().LoadMap(path, look, change);
}

bool Game::IsPlaygroundLand(const std::filesystem::path& path)
{
	return Locator::landSystem::value().IsPlaygroundLand(path);
}

bool Game::LoadFromLandMenu(const std::filesystem::path& path) noexcept
{
	// A playground land starts a skirmish: a new game on that land, the last game's creature kept for the profile it was
	// played as, every player starting afresh and the player as good or evil as their profile last kept them
	return LoadMapWithFreshScripts(path, land_change::FromLandMenu(IsPlaygroundLand(path)));
}

bool Game::LoadMapWithFreshScripts(const std::filesystem::path& path, land_change::Kind change) noexcept
{
	return Locator::landSystem::value().LoadMapWithFreshScripts(path, change);
}

void Game::LoadTestbed() noexcept
{
	// The testbed is a developer tool. A live network game never loads it.
	if (!developer_tools::Allowed(developer_tools::Action::ScenarioLoad))
	{
		return;
	}
	static_cast<void>(Locator::landSystem::value().LoadTestbed());
}

bool Game::RequestScenario(ScenarioRequest request)
{
	if (!developer_tools::Allowed(developer_tools::Action::ScenarioLoad))
	{
		return false;
	}
	_scenarioRequest = std::move(request);
	return true;
}

bool Game::LoadGodsRingMap() noexcept
{
	return Locator::landSystem::value().LoadGodsRingMap();
}

bool Game::LoadSavedLand(const std::filesystem::path& landPath, const std::filesystem::path& landscape,
                         const std::function<bool()>& restore) noexcept
{
	return Locator::landSystem::value().LoadSavedLand(landPath, landscape, restore,
	                                                  ecs::systems::LandSystemInterface::SavedLandUse::SavedGame);
}

void Game::StartLandAudio()
{
	Locator::gameAudioSystem::value().StartLandAudio(!_launchSwitches.noLoadMusic);
}

void Game::MakeSymbolSheet()
{
	MakePlayersSymbols();
}

void Game::TestbedLoaded()
{
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	if (Locator::debugGui::has_value() && _testbedWindow)
	{
		Locator::debugGui::value().OpenWindow(debug::gui::k_TestbedScenariosWindow);
	}
#endif
}

std::optional<std::filesystem::path> Game::ChallengeFile()
{
	return ChallengePath();
}

std::optional<std::filesystem::path> Game::MapScriptLand() const
{
	return ecs::systems::MapScriptLand();
}

bool Game::StartNewGame(std::optional<new_game_choice::NewGameStart> start) noexcept
{
	_newGameStart = start;
	Locator::landSystem::value().SetNewGameStart(start);
	_startTestbed = false;
	// A new game begins on the land that the map script names
	const auto land = MapScriptLand();
	if (!land || !LoadMapWithFreshScripts(*land, land_change::Kind::NewGame))
	{
		return false;
	}
	if (!Locator::vm::has_value())
	{
		return false;
	}
	try
	{
		Locator::landSystem::value().StartStoryScripts();
	}
	catch (const std::exception& err)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The story's scripts didn't start: {}", err.what());
		return false;
	}
	return true;
}

void Game::FollowPlaygroundTrip()
{
	if (!_tripQuestion.has_value() || !Locator::gameInterface::has_value())
	{
		return;
	}
	auto answer = Locator::gameInterface::value().TakeTripAnswer();
	if (!answer.has_value())
	{
		if (Locator::gameInterface::value().IsTripQuestionUp())
		{
			return;
		}
		// The box went with no answer: this is the same as No
		answer = false;
	}
	const auto question = *std::exchange(_tripQuestion, std::nullopt);
	// The answer gives the keyboard back to the game at once
	Locator::gameActionSystem::value().BoxAnswered();
	auto& trip = Locator::playgroundTripSystem::value();
	// Each answer unpauses the game, but Yes to leave the Playground. The menu then comes back if it is open, and its main
	// page pauses the game again.
	auto& menu = Locator::gameInterface::value().GetMenu();
	if (question == playground_trip::Question::Go || !*answer)
	{
		Locator::time::value().SetPlayerPause(false);
		if (menu.IsOpen() && playground_trip::PausesWhenBack(Locator::gameInterface::value().GetTripMenuBox()))
		{
			Locator::time::value().SetPlayerPause(true);
		}
	}
	if (!*answer)
	{
		return;
	}
	if (question == playground_trip::Question::Go)
	{
		// The saved game keeps the pause that the game has now. The land of the Playground then loads. The trip closes no
		// box: an open menu and the land information box stay over the new land.
		_boxesStayForTrip = true;
		if (!trip.SaveBeforeTrip())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "The save before the trip to the Gods' Playground did not write");
		}
		if (!trip.Travel())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The trip to the Gods' Playground did not load the land");
		}
		_boxesStayForTrip = false;
		return;
	}
	// The saved game loads with its own pause. With no saved game, the game stays paused.
	if (!trip.Leave())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "No saved game loaded as the player left the Gods' Playground");
	}
}

PlayerNames Game::LocalGodForGameEnd() const
{
	// A looker-on of a network game has no god. The original game names it player seven. In a network game every god
	// takes part, and player seven can be a god, so the looker-on gets the neutral player, which is no god.
	const auto* session = Locator::networkGame::has_value() ? Locator::networkGame::value().Session() : nullptr;
	const bool looksOn =
	    session != nullptr && session->GetJoined().has_value() && session->GetJoined()->role == game_socket::Role::Observer;
	return looksOn ? PlayerNames::NEUTRAL : LocalInputPlayer();
}

void Game::EndGame(game_session::End end)
{
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game ends: {}",
	                   end == game_session::End::NewGame    ? "a new game starts"
	                   : end == game_session::End::Online   ? "the player left the network game"
	                   : end == game_session::End::Skirmish ? "the skirmish box shows"
	                                                        : "the game continues as the profile picked");
	SaveAsGameEnds(end, false);
	// The next game has no previous profile until the Players page opens again
	if (Locator::gameInterface::has_value())
	{
		Locator::gameInterface::value().GetMenu().ForgetPreviousProfile();
	}
	_gameEnding = end;
}

void Game::StartNextGameWhenSaved()
{
	// The save on exit takes the picture of the last frame of the game that ended. The next game starts after it.
	if (!_gameEnding.has_value() ||
	    (Locator::saveGameSystem::has_value() && Locator::saveGameSystem::value().AnyPictureWaiting()))
	{
		return;
	}
	const auto end = *std::exchange(_gameEnding, std::nullopt);
	auto& profiles = Locator::playerProfileSystem::value();
	if (const auto picked = std::exchange(_profileAfterSave, std::nullopt); picked.has_value())
	{
		profiles.SelectProfile(*picked);
	}
	const auto profile = profiles.GetCurrentProfile();
	if (profile.has_value() && Locator::gameInterface::has_value())
	{
		Locator::gameInterface::value().SetPlayerProfile(gui::ToUtf16(profile->name), gui::ToUtf16(profile->creatureName),
		                                                 profile->symbol, profile->settings);
		RefreshPlayersPage();
	}
	switch (game_session::NextStart(end).value_or(game_session::Start::NewGame))
	{
	case game_session::Start::Continue:
		ContinueFromExitSave();
		break;
	case game_session::Start::NewGame:
		if (!StartNewGame(std::nullopt))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The new game could not start");
		}
		break;
	case game_session::Start::Online:
		StartAfterNetworkGame();
		break;
	case game_session::Start::Skirmish:
		// A player who left a network game plays alone again: the skirmish box starts a game of this computer alone
		if (Locator::networkGame::has_value())
		{
			Locator::networkGame::value().EndNetworkGame();
		}
		OpenSkirmishBoxOverGame();
		break;
	case game_session::Start::SavedGame:
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "This start of a game is not available yet");
		break;
	}
}

game_session::ExitSave Game::ExitSaveAsGameEnds(game_session::End end) const
{
	const auto& context = Locator::entitiesRegistry::value().Context();
	game_session::ExitSaveFacts facts {
	    .end = end,
	    .gameOver = context.gameOver,
	    .multiplayer = Locator::time::value().IsNetworkGame(),
	    .skirmish = context.skirmish,
	    .godsPlayground = context.mapScriptGlobals.landNumber == escape_key::k_GodsPlaygroundLand,
	    .previousProfile = std::nullopt,
	    .currentProfile = {},
	    .previousExists = false,
	};
	if (Locator::gameInterface::has_value())
	{
		facts.previousProfile = Locator::gameInterface::value().GetMenu().GetPreviousProfile();
	}
	if (Locator::playerProfileSystem::has_value())
	{
		const auto& profiles = Locator::playerProfileSystem::value();
		if (const auto profile = profiles.GetCurrentProfile(); profile.has_value())
		{
			facts.currentProfile = gui::ToUtf16(profile->name);
		}
		facts.previousExists = facts.previousProfile.has_value() &&
		                       player_profiles::Find(profiles.GetProfiles(), *facts.previousProfile) != nullptr;
	}
	return game_session::ExitSaveFor(facts);
}

bool Game::SaveAsGameEnds(game_session::End end, bool quitting)
{
	if (!Locator::saveGameSystem::has_value())
	{
		return false;
	}
	auto& saves = Locator::saveGameSystem::value();
	switch (ExitSaveAsGameEnds(end))
	{
	case game_session::ExitSave::None:
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game ends with no save on exit");
		return false;
	case game_session::ExitSave::Current:
		return saves.SaveOnExit(quitting);
	case game_session::ExitSave::Previous:
		break;
	}
	// The save goes to the profile that the player played before they picked another one. The game plays as that
	// profile while it saves, and then plays as the profile picked again.
	auto& profiles = Locator::playerProfileSystem::value();
	const auto picked = profiles.GetCurrentProfile();
	const auto previous = Locator::gameInterface::value().GetMenu().GetPreviousProfile();
	if (!picked.has_value() || !previous.has_value() || !profiles.SelectProfile(*previous))
	{
		return false;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The save on exit goes to the profile played before");
	const bool saved = saves.SaveOnExit(quitting);
	if (quitting)
	{
		profiles.SelectProfile(gui::ToUtf16(picked->name));
	}
	else
	{
		// The picture of the save comes back after the next frame, into the folder of the previous profile
		_profileAfterSave = gui::ToUtf16(picked->name);
	}
	return saved;
}

bool Game::ContinueGame()
{
	auto& saves = Locator::saveGameSystem::value();
	if (saves.HasExitSave() && saves.Load(gamesave::k_ExitSlot))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The game continues from the save on exit");
		return true;
	}
	// With no save on exit, or a load that did not happen, a new game starts on the first land
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "No save on exit loaded: a new game starts");
	if (!StartNewGame(_newGameStart))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The new game could not start");
	}
	return false;
}

bool Game::ContinueFromExitSave() noexcept
{
	if (!ContinueGame())
	{
		return false;
	}
	ShowMenuOnStart();
	return true;
}

void Game::LoadLandscape(const std::filesystem::path& path)
{
	Locator::landSystem::value().LoadLandscape(path);
}

void Game::SetTime(float time) noexcept
{
	Locator::skyClockSystem::value().SetTime(time);
}
