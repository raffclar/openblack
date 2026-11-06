/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The network game: the multiplayer front end, how a player joins and leaves, and the start of a network land

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <GameSocket.h>
#include <SDL.h>
#include <spdlog/spdlog.h>

#include "3D/HandAnimation.h"
#include "3D/HandMorph.h"
#include "3D/SkyDome.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/FreeCameraModel.h"
#include "Commands/PlayerCommands.h"
#include "Common/BuildVersion.h"
#include "Common/DeveloperToolsGate.h"
#include "Game.h"
#include "Sound/ClipSounds.h"
#if defined(OPENBLACK_DEVELOPER_TOOLS)
#include "Common/DrawTrace.h"
#endif
#include "Common/GameRandom.h"
#include "Common/MainThreadQueue.h"
#include "Common/RandomNumberManager.h"
#include "Common/SideThread.h"
#include "Creature/LeashOrders.h"
#include "Debug/FightAutopilotInterface.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/GameEndConditions.h"
#include "ECS/LearningMarker.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/ClanCreatureSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/GatheringBoxSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/LobbyClientSystemInterface.h"
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/SimulationSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Gui/Canvas.h"
#include "Gui/EscapeKeyRules.h"
#include "Gui/GameInterface.h"
#include "Gui/JoinOnlineEntry.h"
#include "Gui/MultiplayerFrontEnd.h"
#include "Gui/OnlineMapTexts.h"
#include "Gui/StartupRules.h"
#include "Gui/StatisticsBoxRules.h"
#include "Hand/HandGrabRules.h"
#include "Hand/ScriptHandState.h"
#include "Help/DialogueText.h"
#include "Help/Spirits.h"
#include "Help/TurnCheer.h"
#include "Input/GameActionMapInterface.h"
#include "LivingTurnOrder.h"
#include "Locator.h"
#if !defined(__EMSCRIPTEN__)
#include "Online/Desktop/UdpLanDiscovery.h"
#endif
#include "Online/HttpClientInterface.h"
#include "Online/LanDiscoveryInterface.h"
#include "Online/NetworkLand.h"
#include "Online/NetworkSession.h"
#include "Online/OnlineMapLand.h"
#include "Online/OnlineMapStoreInterface.h"
#include "Online/OpenPage.h"
#include "Online/StartRules.h"
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

#if defined(__EMSCRIPTEN__)
#include "Web/WebPlatform.h"
#endif

using namespace openblack;

void Game::UpdateOnline()
{
	// The web client and the WSS client move their transfers on and run the handlers. Then the lobby client moves on.
	if (Locator::httpClient::has_value())
	{
		Locator::httpClient::value().Poll();
	}
	if (Locator::webSocketClient::has_value())
	{
		Locator::webSocketClient::value().Poll();
	}
	if (Locator::onlineMapStore::has_value())
	{
		Locator::onlineMapStore::value().Poll();
	}
	if (Locator::lobbyClient::has_value())
	{
		Locator::lobbyClient::value().Update();
	}
	// The gathering box talks to the ping server with the time of the computer, not the time of the turns
	if (Locator::gatheringBoxSystem::has_value())
	{
		Locator::gatheringBoxSystem::value().Update(SDL_GetTicks());
	}
	// The network game takes the turns and the checksums that came, and comes back after a loss
	if (Locator::networkGame::has_value())
	{
		auto& network = Locator::networkGame::value();
		network.Update();
		const auto* session = network.Session();
		if (Locator::gameInterface::has_value())
		{
			// The words "Resynchronising..." show while the world resynchronises (user decision MI-4)
			Locator::gameInterface::value().ShowResync(session != nullptr && network.InGame() && session->ShowsResync());
			// After the last try the game asks whether the player plays on, as the game asks when the checksums differ
			// A run of the tools asks nothing: the game stops as its network game ends
			const bool toolRun = Locator::simulation::value().GetSettings().untilTurn.has_value();
			if (session != nullptr && !network.InGame() && session->GetEnd() == online::NetworkSession::End::OutOfSync &&
			    !_outOfSyncAsked && !toolRun)
			{
				_outOfSyncAsked = true;
				Locator::gameInterface::value().AskOutOfSync();
			}
			if (const auto answer = Locator::gameInterface::value().TakeOutOfSyncAnswer(); answer.has_value())
			{
				if (*answer)
				{
					network.PlayAlone();
				}
				else
				{
					_leaveNetworkGame = true;
				}
			}
			// After the last try to come back, the box tells the player that the connection to the game is lost
			if (_connectionLost.Update({
			        .hasSession = session != nullptr,
			        .started = session != nullptr && session->GetStart().has_value(),
			        .state = session != nullptr ? session->GetState() : online::NetworkSession::State::Idle,
			        .end = session != nullptr ? session->GetEnd() : online::NetworkSession::End::None,
			        .toolRun = toolRun,
			    }))
			{
				Locator::gameInterface::value().ShowConnectionLost();
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "The connection to the network game is lost ({}): the box tells the player",
				                   online::EndName(session->GetEnd()));
			}
			// The OK goes back to the network front end. A join from the command line has no front end: the game stops.
			// A box with no lost network session (a testbed scenario) only closes.
			if (Locator::gameInterface::value().TakeConnectionLostOk() && _connectionLost.Told())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "The player closed the box of the lost connection");
				if (_joinedFromCommandLine)
				{
					_leaveNetworkGame = true;
				}
				else
				{
					_gameEndAsked = game_session::End::Online;
				}
			}
		}
	}
}

std::unique_ptr<gui::MultiplayerFrontEnd> Game::MakeMultiplayerFrontEnd()
{
	if (_lanDiscovery == nullptr)
	{
#if defined(__EMSCRIPTEN__)
		// A browser cannot search the local network
		_lanDiscovery = std::make_unique<online::NoLanDiscovery>();
#else
		_lanDiscovery = online::MakeUdpLanDiscovery([] {
			return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
			    .count();
		});
#endif
	}
	gui::MultiplayerFrontEnd::Services services {
	    .lobby = Locator::lobbyClient::has_value() ? &Locator::lobbyClient::value() : nullptr,
	    .network = Locator::networkGame::has_value() ? &Locator::networkGame::value() : nullptr,
	    .lan = _lanDiscovery.get(),
	    .openPage =
	        [](std::string_view url) {
		        online::OpenPage(url);
#if !defined(__EMSCRIPTEN__)
		        // The game makes its window small, so that the browser's page is in front
		        if (Locator::windowing::has_value())
		        {
			        SDL_MinimizeWindow(static_cast<SDL_Window*>(Locator::windowing::value().GetHandle()));
		        }
#endif
	        },
	    .owner = {},
	    .poll = {},
	    .maps = Locator::onlineMapStore::has_value() ? &Locator::onlineMapStore::value() : nullptr,
	    .clanCreature = Locator::clanCreatureSystem::has_value() ? &Locator::clanCreatureSystem::value() : nullptr,
	};
	gui::MultiplayerFrontEnd::Settings settings;
	// The registration page is on the lobby's site: Register on the site, then log in in the game
#if defined(__EMSCRIPTEN__)
	settings.browser = true;
	settings.registerPage = "/register/";
	settings.accountPage = "/account/";
#else
	const auto origin = Locator::config::value().lobbyOrigin.value_or(std::string());
	settings.registerPage = origin.empty() ? std::string() : origin + "/register/";
	settings.accountPage = origin.empty() ? std::string() : origin + "/account/";
#endif
	// The internet connection's name in the list is the lobby's host: in the browser, the host of the page
#if defined(__EMSCRIPTEN__)
	settings.internetName = gui::ToUtf16(web::PageHost());
#else
	const auto host = origin.find("://");
	settings.internetName = gui::ToUtf16(host == std::string::npos ? origin : origin.substr(host + 3));
#endif
	if (const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile(); profile.has_value())
	{
		settings.profileName = gui::ToUtf16(profile->name);
	}
	// A game server of another version shows "(Wrong Version)" in the game list. The lobby keeps 16 characters of a
	// server's version, so the game servers report their semantic version and the list compares that.
	settings.gameVersion = std::string(build_version::Semantic());
	auto maps = gui::ReadOnlineMapTexts();
	settings.localMaps = std::move(maps.localMaps);
	settings.conditionTemplate = std::move(maps.conditionTemplate);
	settings.showPing = Locator::config::value().showPing;
	return std::make_unique<gui::MultiplayerFrontEnd>(std::move(services), std::move(settings));
}

void Game::AnswerJoinOnline()
{
	if (!_joinOnlineAsked || !Locator::gameInterface::has_value())
	{
		return;
	}
	const auto& director = Locator::cinematicDirectorSystem::value();
	const bool networkGame = _networkLandPending || Locator::time::value().IsNetworkGame() ||
	                         (Locator::networkGame::has_value() && Locator::networkGame::value().InGame());
	const auto decision = join_online_entry::Decide({
	    .frontEndOpen = Locator::gameInterface::value().GetMultiplayerBoxes().IsActive(),
	    .networkGame = networkGame,
	    .boxToAnswer = Locator::gameInterface::value().GetNewPlayerBox().IsActive() ||
	                   Locator::gameInterface::value().GetSaveGameBoxes().IsActive() ||
	                   Locator::gameInterface::value().GetTattooEditor().IsOpen() || _gameEnding.has_value() ||
	                   _gameEndAsked.has_value(),
	    .scriptHoldsCinemaBars = director.IsWideScreenOn() && director.GetWideScreenOwner() != 0,
	    .startQuestionOpen = Locator::gameInterface::value().GetSkipBox().IsActive(),
	});
	switch (decision.outcome)
	{
	case join_online_entry::Outcome::Wait:
		return;
	case join_online_entry::Outcome::Ignore:
		_joinOnlineAsked = false;
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Join Online Game: the front end is open or a network game plays");
		return;
	case join_online_entry::Outcome::Open:
		break;
	}
	_joinOnlineAsked = false;
	// The start-of-game question waits behind the front end, as it does for the -MULTIPLAYER switch
	std::optional<bool> question;
	if (decision.askQuestionAfter)
	{
		const bool hasCreature =
		    Locator::gameInterface::value().GetSkipBox().IsOffered(new_game_choice::Choice::KeepOldCreature);
		if (Locator::gameInterface::value().WithdrawSkipBox())
		{
			question = hasCreature;
		}
	}
	if (!OpenMultiplayerFrontEnd(MakeMultiplayerFrontEnd()))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The multiplayer front end needs the temple and the interface");
		if (question.has_value())
		{
			Locator::gameInterface::value().ShowSkipBox(*question);
		}
		return;
	}
	if (question.has_value())
	{
		_startQuestionAfterFrontEnd = question;
	}
	if (decision.holdLand)
	{
		_frontEndHoldsLand = true;
		Locator::time::value().SetPlayerPause(true);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Join Online Game of the page: the multiplayer front end opens{}",
	                   decision.holdLand ? ", and the cut scene stops until the player leaves it" : "");
}

bool Game::OpenFrontEndInPlaceOfMenu()
{
	if (OpenMultiplayerFrontEnd(MakeMultiplayerFrontEnd()))
	{
		return true;
	}
	SPDLOG_LOGGER_WARN(spdlog::get("game"), "The multiplayer front end needs the temple and the interface");
	// The land's start-of-game question waited for the front end, which did not open
	if (const auto hasCreature = std::exchange(_startQuestionAfterFrontEnd, std::nullopt);
	    hasCreature.has_value() && Locator::gameInterface::has_value())
	{
		Locator::gameInterface::value().ShowSkipBox(*hasCreature);
	}
	return false;
}

void Game::StartAfterNetworkGame()
{
	// The network game of this computer ends: the next land is a land of this computer alone. As the -MULTIPLAYER switch
	// does, the start land loads as a plain start does, and the multiplayer front end opens in place of the menu.
	if (Locator::networkGame::has_value())
	{
		Locator::networkGame::value().EndNetworkGame();
	}
	_endGameBoxState = {};
	_frontEndOnStart = true;
	ContinueGame();
	_frontEndOnStart = false;
	if (OpenFrontEndInPlaceOfMenu())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The player left the network game: the multiplayer front end opens");
	}
}

void Game::AskToLeaveNetworkGame()
{
	if (!Locator::gameInterface::has_value())
	{
		return;
	}
	// The end box asks the question to leave, in place of the menu
	const auto box = end_game_box::AskToLeave(_endGameBoxState);
	const auto& texts = Locator::gameInterface::value().GetTexts();
	const auto title = end_game_box::Title(
	    box.kind, false, {},
	    {.leave = texts.Get("HELP_TEXT_DIALOG_AREYOUSUREQUIT"), .lost = {}, .won = {}, .gameOver = {}, .winnerFormat = {}});
	const auto local = LocalGodForGameEnd();
	const auto landNumber = Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber;
	const bool networkGame = Locator::time::has_value() && Locator::time::value().IsNetworkGame();
	const auto colour =
	    end_game_box::LooksOn(local, networkGame) ? glm::vec4(0.0f) : gui::statistics_box::PlayerColour(local, landNumber);
	Locator::gameInterface::value().OpenEndGameBox({.box = box, .title = title, .colour = colour});
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The end box asks the question to leave the network game");
}

void Game::ExitOnlineGame()
{
	// The question to leave, unless the god of the player is out of the game already: then the player leaves at once
	if (Locator::gameEndSystem::has_value())
	{
		const auto local = LocalGodForGameEnd();
		for (const auto& [god, standing] : Locator::gameEndSystem::value().Standings())
		{
			if (god == local && standing.out)
			{
				if (Locator::gameInterface::has_value())
				{
					Locator::gameInterface::value().GetMenu().Close();
				}
				LeaveNetworkGame();
				return;
			}
		}
	}
	AskToLeaveNetworkGame();
}

void Game::LeaveAsObserverAtTheEndOfTheRun()
{
	if (!Locator::networkGame::has_value())
	{
		return;
	}
	auto& network = Locator::networkGame::value();
	const auto* session = network.Session();
	if (session == nullptr || session->GetState() == online::NetworkSession::State::Ended ||
	    !session->GetJoined().has_value() || session->GetJoined()->role != game_socket::Role::Observer)
	{
		return;
	}
	// The update closes the socket after the leave, so the leave goes before the game stops
	network.Leave();
	network.Update();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The observer leaves the network game at the end of the run");
}

void Game::LeaveNetworkGame(game_session::NetworkLeave leave)
{
	if (!Locator::networkGame::has_value() || Locator::networkGame::value().Session() == nullptr)
	{
		return;
	}
	// The game server makes the god of the player a leaver for every computer
	Locator::networkGame::value().Leave();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The player leaves the network game");
	// A join from the command line, and a run of the tools, stop the game. A join from the front end goes to the box
	// that the control gives.
	const bool toolRun = Locator::simulation::value().GetSettings().untilTurn.has_value();
	if (_joinedFromCommandLine || toolRun)
	{
		_leaveNetworkGame = true;
		return;
	}
	_gameEndAsked = game_session::EndAfterNetworkLeave(leave);
}

void Game::LeaveAsAPlayerDoes()
{
	// The centre of the control of a name, among the controls that a box shows now
	const auto centreOf = [](const std::vector<gui::Dialog::NamedControl>& controls,
	                         std::u16string_view name) -> std::optional<glm::ivec2> {
		const auto found = std::ranges::find(controls, name, &gui::Dialog::NamedControl::name);
		if (found == controls.end())
		{
			return std::nullopt;
		}
		return (found->rect.min + found->rect.max) / 2;
	};
	// Escape opens the menu, Exit Online Game asks the question to leave, and Leave Game leaves
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tests: the player leaves the network game at turn {}",
	                   Locator::time::value().GetTurn());
	auto& menu = Locator::gameInterface::value().GetMenu();
	TellMenuTheGame();
	Locator::gameInterface::value().OpenMenu();
	const auto& texts = Locator::gameInterface::value().GetTexts();
	if (const auto exit = centreOf(menu.GetNamedControls(), texts.Get("HELP_TEXT_FRONT_END_03")); exit.has_value())
	{
		menu.MouseDown(*exit);
		if (menu.MouseUp(*exit) == gui::GameMenu::Action::ExitOnlineGame)
		{
			ExitOnlineGame();
		}
	}
	auto& endBox = Locator::gameInterface::value().GetEndGameBox();
	if (const auto leave = centreOf(endBox.GetNamedControls(), texts.Get("HELP_TEXT_DIALOG_LEAVE_GAME")); leave.has_value())
	{
		endBox.MouseDown(*leave);
		std::ignore = endBox.MouseUp(*leave);
	}
}

void Game::TellLeaves()
{
	if (!Locator::gameInterface::has_value() || !Locator::networkGame::has_value())
	{
		return;
	}
	auto& network = Locator::networkGame::value();
	const auto* session = network.Session();
	auto gods = Locator::gameEndSystem::has_value() ? Locator::gameEndSystem::value().TakeLeavesToShow()
	                                                : std::vector<game_end::Leave>();
	auto observers = network.TakeObserversLeft();
	if (session == nullptr)
	{
		return;
	}
	const auto own = session->OwnGod();
	std::vector<std::pair<game_end::Leave, std::string>> leaves;
	for (const auto& leave : gods)
	{
		// The player of this computer does not see its own leave
		if (!leave.god.has_value() || leave.god == own)
		{
			continue;
		}
		// The start gives the names by god slot, not by player number
		std::string name;
		const auto slot = player_slots::SlotOfPlayer(*leave.god);
		if (session->GetStart().has_value() && slot.has_value() && *slot < session->GetStart()->names.size())
		{
			name = session->GetStart()->names.at(*slot);
		}
		leaves.emplace_back(leave, std::move(name));
	}
	// An observer has no god, and its god is never out. Its leave comes at once, not at a turn.
	const bool decided = Locator::gameEndSystem::has_value() && Locator::gameEndSystem::value().IsDecided();
	for (auto& name : observers)
	{
		leaves.emplace_back(game_end::Leave {.god = std::nullopt, .godOut = false, .decided = decided}, std::move(name));
	}
	for (const auto& [leave, name] : leaves)
	{
		const auto notice = game_end::NoticeOf(leave);
		if (notice.remark && Locator::guidanceSystem::has_value())
		{
			Locator::guidanceSystem::value().SayOneOff(guidance::OneOff::EnemyLeaving);
		}
		if (notice.box)
		{
			Locator::gameInterface::value().ShowPlayerLeft(gui::ToUtf16(name));
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "{} left the network game{}", name,
		                   notice.box ? ": the box tells the players" : "");
	}
}

bool Game::OpenMultiplayerFrontEnd(std::unique_ptr<gui::MultiplayerFrontEnd> frontEnd)
{
	if (!Locator::gameInterface::has_value() || !Locator::temple::has_value() || frontEnd == nullptr)
	{
		return false;
	}
	Locator::temple::value().EnterMultiplayerFrontEnd();
	Locator::gameInterface::value().OpenMultiplayer(std::move(frontEnd));
	return true;
}

void Game::UpdateMultiplayerFrontEnd()
{
	if (!Locator::gameInterface::has_value())
	{
		return;
	}
	// The game server started the game that the player joined in the front end: the front end closes, and the server's
	// land loads with the server's seed. The network game keeps its session with the server.
	if (Locator::networkGame::has_value())
	{
		auto& network = Locator::networkGame::value();
		if (const auto start = network.TakeStart(); start.has_value())
		{
			auto& startBoxes = Locator::gameInterface::value().GetMultiplayerBoxes();
			if (startBoxes.IsActive())
			{
				startBoxes.Close();
			}
			if (Locator::temple::has_value() && Locator::temple::value().IsMultiplayerFrontEnd())
			{
				Locator::temple::value().LeaveMultiplayerFrontEnd();
			}
			// The land of the question goes: the network game's land does not ask it. A land that the front end held
			// goes too.
			const bool held = std::exchange(_frontEndHoldsLand, false);
			if (std::exchange(_startQuestionAfterFrontEnd, std::nullopt).has_value() || held)
			{
				Locator::time::value().SetPlayerPause(false);
			}
			if (StartNetworkLand(*start))
			{
				network.BeginPlay();
			}
			else
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The land of the network game did not load");
				network.Leave();
			}
			return;
		}
	}
	auto& boxes = Locator::gameInterface::value().GetMultiplayerBoxes();
	if (!boxes.IsActive())
	{
		return;
	}
	// The cut scene stays stopped while the front end is open, also when the menu closed and let the game continue
	if (_frontEndHoldsLand)
	{
		Locator::time::value().SetPlayerPause(true);
	}
	const bool templeGone = !Locator::temple::has_value() || !Locator::temple::value().IsMultiplayerFrontEnd();
	if (boxes.TakeLeft() || templeGone)
	{
		boxes.Close();
		if (Locator::temple::has_value())
		{
			Locator::temple::value().LeaveMultiplayerFrontEnd();
		}
		// The land's start-of-game question waited for the front end
		const auto hasCreature = std::exchange(_startQuestionAfterFrontEnd, std::nullopt);
		if (hasCreature.has_value())
		{
			Locator::gameInterface::value().ShowSkipBox(*hasCreature);
		}
		// The cut scene that the front end stopped continues. The question keeps the land stopped until its answer.
		if (std::exchange(_frontEndHoldsLand, false) && !hasCreature.has_value())
		{
			Locator::time::value().SetPlayerPause(false);
		}
	}
}

bool Game::StartNetworkLand(const game_socket::Start& start)
{
	const auto scripts = Locator::filesystem::value().GetPath<filesystem::Path::Scripts>();
	const auto landPath = start.testbed || start.godsRingMap ? std::optional<std::filesystem::path>(std::filesystem::path())
	                      : online::start_rules::IsLandPath(start.land) ? network_land::PathOf(start.land, scripts)
	                                                                    : std::nullopt;
	if (!landPath.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The game server gives a land that is not a land script: {}", start.land);
		return false;
	}
	// A join from the command line: the server's land, seed and start take the place of those of this computer
	if (std::exchange(_networkLandPending, false))
	{
		const auto& lands = Locator::landSystem::value();
		const auto differences = network_land::OwnValuesThatDiffer({.land = lands.GetLandPath(),
		                                                            .testbed = _startTestbed,
		                                                            .godsRingMap = _startGodsRingMap,
		                                                            .seed = Locator::rng::value().GetRunSeed(),
		                                                            .opening = _newGameStart},
		                                                           start);
		if (!differences.empty())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "The land, the seed and the start of the game server win: this computer had another {}",
			                   differences);
		}
		Locator::toolRunSystem::value().StartInputTools();
	}
	// The client starts the land as the game server started it: the same new-game start and the same seed, as a network
	// game. Each computer reads the date from its own clock, as in the original game. Only a run of a tool keeps its
	// pinned date. No synced value reads the date.
	_newGameStart = start.opening.empty() ? std::nullopt : new_game_choice::ParseNewGameStart(start.opening);
	auto& lands = Locator::landSystem::value();
	lands.SetNewGameStart(_newGameStart);
	auto& clock = Locator::time::value();
	const auto pinnedDate = clock.GetPinnedDate();
	clock.RestartClock(pinnedDate);
	clock.SetNetworkGame(true);
	// The time limit of the game comes from the server, so every computer decides the game at the same turn
	if (Locator::gameEndSystem::has_value())
	{
		Locator::gameEndSystem::value().SetTimeLimit(start.timeLimit.value_or(game_end::k_DefaultTimeLimitMinutes));
		// The winning conditions come from the server too. The gods get them at the first turn.
		std::vector<game_end::ConditionPick> picks;
		for (const auto& each : start.conditions)
		{
			picks.push_back(
			    {.condition = static_cast<game_end::Condition>(each.condition), .value = static_cast<int32_t>(each.value)});
		}
		Locator::gameEndSystem::value().SetConditions(std::move(picks));
	}
	// The land starts from the state that the game server started it from
	simulation::PrepareNetworkLand(start.seed);
	// The modes of the AI soak come from the server, as the seed does. A build for players refuses them.
	if (const auto refusal = simulation::ApplySoakTools({.allComputerGods = start.allComputerGods, .aiSeed = start.aiSeed});
	    refusal.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The game server asks for a game this build cannot play: {}", *refusal);
		return false;
	}
	// The testbed scenario of the tools comes from the server too. The scenarios' window lays it out before the first turn,
	// as the server does. A build for players has no such window and refuses it.
	if (start.scenario.has_value())
	{
		if (const auto refusal = developer_tools::ScenarioRefusalOf(developer_tools::k_Built); refusal.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The game server asks for a game this build cannot play: {}", *refusal);
			return false;
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The network game plays the testbed scenario {}", *start.scenario);
		_scenarioRequest = ScenarioRequest {.id = *start.scenario, .hideWindow = true, .benchmark = false};
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The network game starts: seed {}, {}", start.seed,
	                   pinnedDate.has_value() ? "date pinned" : "date from the clock of the computer");
	_startTestbed = start.testbed;
	_startGodsRingMap = start.godsRingMap;
	// The server chose the land: the testbed of a server is not a developer tool of this computer
	if (start.testbed)
	{
		// A build for players has no testbed
		if (!lands.LoadTestbed())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The game server asks for a game this build cannot play: {}",
			                    "the build has no developer tools, so it has no testbed");
			return false;
		}
	}
	else if (start.godsRingMap)
	{
		// A network client does not make the map for 32 gods: it loads the file of the server, which it got from the
		// lobby (deviation 171)
		const auto* store = Locator::onlineMapStore::has_value() ? &Locator::onlineMapStore::value() : nullptr;
		const auto file = online::ReadStartLand(start.land, store, Locator::filesystem::value());
		if (!file.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "This computer does not have the map of the game server: {}", start.land);
			return false;
		}
		if (!lands.LoadGodsRingMapFile(*file))
		{
			return false;
		}
	}
	else if (online_map_land::IsOnlineMap(start.land))
	{
		// A land of a network game comes from the store of the lands that the player got from the lobby, else from the
		// game folder. The start check before compared the checksum of these same bytes.
		const auto* store = Locator::onlineMapStore::has_value() ? &Locator::onlineMapStore::value() : nullptr;
		const auto file = online::ReadStartLand(start.land, store, Locator::filesystem::value());
		if (!file.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "This computer does not have the land of the game server: {}", start.land);
			return false;
		}
		if (!lands.LoadNetworkMapFile(*landPath, *file, ecs::systems::LandSystemInterface::ChallengeScripts::StartedAgain))
		{
			return false;
		}
	}
	else if (!LoadMapWithFreshScripts(*landPath, land_change::Kind::NewGame))
	{
		// The server names a land in the Scripts folder of the game, and never a path out of it
		return false;
	}
	// The story's scripts run the first land, but not the testbed, the map for 32 gods, a playground or a network game
	const bool startStory = !start.testbed && !start.godsRingMap &&
	                        new_game_choice::FirstLandStartsStory(Locator::entitiesRegistry::value().Context().skirmish, true);
	if (!lands.StartChallengeScripts(startStory))
	{
		return false;
	}
	// The world starts as a new world starts to play: everything that the land made goes into the map's cells, and the
	// clock starts from the start of the current turn
	Locator::entitiesMap::value().Sync();
	// Every resource loads before the first turn, as on the game server, so that a thing that waits for its picture or
	// its model starts at the same turn on each computer
	simulation::LoadEveryResource();
	clock.Start();
	clock.ResetTimerToTurn();
	return true;
}
