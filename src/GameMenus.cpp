/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game's interface and menus: the interface's actions, the tattoo menu, the player's profile and the boxes

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL.h>
#include <spdlog/spdlog.h>

#include "3D/HandAnimation.h"
#include "3D/LandLightTable.h"
#include "3D/SkyDome.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/AtmosAudio.h"
#include "Audio/AudioManagerInterface.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/FightCameraModel.h"
#include "Camera/ScriptCameraModel.h"
#include "Commands/PlayerCommands.h"
#include "Game.h"
#include "GameDetail.h"
#include "Sound/GameMusic.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/HelpSpeech.h"
#include "Sound/ScriptSoundEffect.h"
#if defined(OPENBLACK_DEVELOPER_TOOLS)
#include "Common/DrawTrace.h"
#endif
#include "Common/EventManager.h"
#include "Common/GameRandom.h"
#include "Common/MainThreadQueue.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureNames.h"
#include "Creature/LeashOrders.h"
#include "Debug/DebugGuiInterface.h"
#include "Debug/FightAutopilotInterface.h"
#include "Debug/FrameStatsLog.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Archetypes/SkyArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/GameEndConditions.h"
#include "ECS/GamePlayers.h"
#include "ECS/LearningMarker.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/ComputerSettingsSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/GameEndSystemInterface.h"
#include "ECS/Systems/GameKeysSystemInterface.h"
#include "ECS/Systems/GatheringBoxSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/IncomingMessageSystemInterface.h"
#include "ECS/Systems/LoadingScreenSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/Systems/PauseTextSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/ProfileFolderSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/SimulationSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "ECS/Systems/TattooMenuSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "ECS/VillagerLabelSource.h"
#include "EngineConfig.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/RendererInterface.h"
#include "Gui/Canvas.h"
#include "Gui/DialogPainter.h"
#include "Gui/GameInterface.h"
#include "Gui/LoadingScreen.h"
#include "Gui/MultiplayerFrontEnd.h"
#include "Gui/ScreenFit.h"
#include "Gui/StartupRules.h"
#include "Gui/StatisticsBoxRules.h"
#include "Hand/HandBlend.h"
#include "Hand/HandGrabRules.h"
#include "Hand/HandTrace.h"
#include "Help/DialogueText.h"
#include "Help/SingAlong.h"
#include "Help/Spirits.h"
#include "Help/TurnCheer.h"
#include "Input/GameActionMapInterface.h"
#include "LivingTurnOrder.h"
#include "Locator.h"
#include "Magic/TownBelief.h"
#include "Online/NetworkLand.h"
#include "Online/NetworkSession.h"
#include "Particles/LiquidParticles.h"
#include "PlayerSlots.h"
#include "Profiler.h"
#include "Resources/GameAssets.h"
#include "Simulation/ChecksumLog.h"
#include "Simulation/CommandLog.h"
#include "Simulation/FrameBody.h"
#include "Simulation/ProfileMenu.h"
#include "Simulation/TurnBody.h"
#include "Simulation/WorldStart.h"
#include "Visual/DetailLevel.h"

using namespace openblack;
using namespace openblack::game_detail;

namespace
{

/// What the box of the winning conditions shows: the score history and the conditions of each god among the first four
/// players that plays, and the conditions of the player of this computer. A network game shows every god that plays,
/// so a game of more than 4 gods shows them 4 at a time (deviation 195).
gui::conditions_box::Figures ConditionsFigures()
{
	gui::conditions_box::Figures figures;
	if (!Locator::gameEndSystem::has_value() || !Locator::playerSystem::has_value())
	{
		return figures;
	}
	const auto local =
	    (Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE);
	const auto landNumber = Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber;
	const bool networkGame = Locator::time::has_value() && Locator::time::value().IsNetworkGame();
	for (const auto& [god, standing] : Locator::gameEndSystem::value().Standings())
	{
		if (god == local)
		{
			for (size_t number = 0; number < standing.conditions.size(); ++number)
			{
				if (const auto& condition = standing.conditions.at(number); condition.has_value())
				{
					figures.conditions.push_back({.number = number, .target = condition->target});
				}
			}
		}
		// The god of the observers has no column of its own, as in the original game
		if (!game_end::CanLose(god, networkGame) || god == Locator::gameEndSystem::value().ObserversGod())
		{
			continue;
		}
		const auto scores = game_end::ShownScores(standing);
		figures.gods.push_back({.player = god,
		                        .colour = gui::statistics_box::PlayerColour(god, landNumber),
		                        .scores = std::vector<float>(scores.begin(), scores.end()),
		                        .conditions = standing.conditions});
	}
	return figures;
}

} // namespace

const gui::TextDatabase* Game::MakeInterface()
{
	// The game's menu, which greets the player by their profile's name and shows their creature's name and their
	// symbol; before a first start's player has a profile, by the name they log in with
	auto [playerName, settings] = simulation::ProfileMenuSettings();
	auto& audio = Locator::audio::value();
	const auto kept = gui::ToProfileSettings(settings);
	const auto& pictures = Locator::tattooEditorSystem::value().GetPictures();
	// The locator owns the interface. The game removes it when the game closes
	Locator::gameInterface::reset(gui::GameInterface::Create(playerName, std::move(settings), pictures).release());
	if (Locator::gameInterface::has_value())
	{
		// The films find the fade through the locator; the interface stays the owner
		Locator::screenFade::reset(&Locator::gameInterface::value().GetScreenFade(), [](gui::ScreenFade* /*unowned*/) {});
	}
	ApplyPlayerSettings(kept);
	LoadProfileKeys();
	RefreshPlayersPage();
	if (!Locator::gameInterface::has_value())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The game's menu is not available, Escape quits");
	}
	else
	{
		// The slider of the help's text speed shows only when the game has no sound
		Locator::gameInterface::value().GetMenu().SetSoundInstalled(audio.IsInstalled());
		// Each save and each load shows its box first
		if (Locator::saveGameSystem::has_value())
		{
			Locator::saveGameSystem::value().SetShowBox([this](bool loading, bool darken) { ShowSavingBox(loading, darken); });
			// A save that did not write its world files completely is told to the player in a box
			Locator::saveGameSystem::value().SetReportProblem([]() { Locator::gameInterface::value().ShowSaveProblem(); });
		}
		// The Game Statistics box takes the players' figures as it opens, from the menu and from the end box
		const auto statistics = [] {
			const auto local =
			    Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
			return std::pair(player_statistics::Box(), local);
		};
		Locator::gameInterface::value().GetMenu().SetStatisticsSource(statistics);
		Locator::gameInterface::value().GetEndGameBox().SetStatisticsSource(statistics);
		// The box of the winning conditions takes the gods' standings as it opens
		Locator::gameInterface::value().GetMenu().SetConditionsSource(ConditionsFigures);
		Locator::gameInterface::value().GetEndGameBox().SetConditionsSource(ConditionsFigures);
		// Each time the end box shows that the player lost while the game goes on, the advisors can say their one-off
		// line about it
		Locator::gameInterface::value().GetEndGameBox().SetShownHandler([](const gui::EndGameBox::Contents& contents) {
			if (contents.box.kind == end_game_box::Kind::Lost && Locator::guidanceSystem::has_value())
			{
				Locator::guidanceSystem::value().SayOneOff(guidance::OneOff::NotVeryGood);
			}
		});
		if (Locator::temple::has_value())
		{
			Locator::temple::value().SetInterface(&Locator::gameInterface::value());
		}
		if (Locator::creatureCaveSystem::has_value())
		{
			Locator::creatureCaveSystem::value().SetInterface(&Locator::gameInterface::value());
		}
		if (Locator::miracleFxSystem::has_value())
		{
			Locator::miracleFxSystem::value().SetInterface(&Locator::gameInterface::value());
		}
		if (Locator::pauseTextSystem::has_value())
		{
			Locator::pauseTextSystem::value().SetInterface(&Locator::gameInterface::value());
		}
		if (Locator::incomingMessageSystem::has_value())
		{
			Locator::incomingMessageSystem::value().SetTexts(&Locator::gameInterface::value().GetTexts());
		}
		if (Locator::chatSystem::has_value())
		{
			Locator::chatSystem::value().SetTexts(&Locator::gameInterface::value().GetTexts());
		}
		if (Locator::gatheringBoxSystem::has_value())
		{
			auto* controls = &Locator::gameInterface::value().GetChatControls();
			Locator::gatheringBoxSystem::value().SetBox({
			    .people = [controls]() { return &controls->GetPeople(); },
			    .peopleChanged = [controls]() { controls->PeopleChanged(); },
			    .open = [controls]() { controls->Open(); },
			    .setFriendsOn = [controls](bool on) { controls->SetFriendsOn(on); },
			    .takeChangedFriends = [controls]() { return controls->TakeChangedFriends(); },
			    .setFriends =
			        [controls](std::span<const chat::people::FriendRecord> friends) { controls->SetFriends(friends); },
			});
		}
	}
	return Locator::gameInterface::has_value() ? &Locator::gameInterface::value().GetTexts() : nullptr;
}

void Game::ShowSavingBox(bool loading, bool darken)
{
	// A run that draws no frame shows no box
	if (!Locator::gameInterface::has_value() || !Locator::windowing::has_value() ||
	    !sim_run::DrawsFrame(Locator::simulation::value().GetSettings(), _frameCount))
	{
		return;
	}
	// A picture asked for this frame shows the box, as the screen does
	std::optional<std::filesystem::path> screenshot;
	if (_requestScreenshot.has_value() && _requestScreenshot->first == _frameCount)
	{
		screenshot = std::move(_requestScreenshot->second);
		_requestScreenshot.reset();
	}
	Locator::loadingScreenSystem::value().ShowSavingBox(
	    {.loading = loading, .darken = darken, .frame = _frameCount, .screenshot = std::move(screenshot)});
}

void Game::HandleInterfaceAction()
{
	using Action = gui::GameMenu::Action;
	const auto action = Locator::gameInterface::value().TakeAction();

	// The answer to the start-of-game question tells the story what to skip, and the game goes on
	if (const auto answer = Locator::gameInterface::value().TakeSkipBoxAnswer(); answer.has_value())
	{
		Locator::tutorialSkipSystem::value().Set(new_game_choice::SkipFor(*answer));
		Locator::time::value().SetPlayerPause(false);
	}

	// The edits on the Players page are kept with the profile played, before anything its buttons do
	if (const auto edits = Locator::gameInterface::value().GetMenu().TakePlayerEdits(); edits.has_value())
	{
		Locator::playerProfileSystem::value().EditCurrentProfile(edits->creatureName, edits->symbol);
		NameCreatureAfterProfile();
		SaveHelpRecord();
	}
	// A profile made in the new player's box from the Players page is played from now on
	if (auto& box = Locator::gameInterface::value().GetNewPlayerBox(); !box.IsActive())
	{
		if (const auto answer = box.TakeAnswer(); answer.has_value())
		{
			Locator::playerProfileSystem::value().CreateProfile(answer->name, answer->symbol);
			PlayerProfileChanged();
		}
		(void)box.TakeCancelled();
	}

	// The settings the player changes take effect at once
	if (Locator::gameInterface::value().TakeSettingsChanged())
	{
		const auto& settings = Locator::gameInterface::value().GetMenu().GetSettings();
		auto& audio = Locator::audio::value();
		audio.SetSfxVolume(settings.sfxVolume);
		audio.SetMusicVolume(settings.musicVolume);
		// The detail and the volumes are kept for this computer, as the options show them
		if (Locator::computerSettingsSystem::has_value())
		{
			Locator::computerSettingsSystem::value().Keep({
			    .detailLevel = static_cast<uint8_t>(settings.detail),
			    .maxDetailLevel = Locator::config::value().maxDetailLevel,
			    .effectsVolume = settings.sfxVolume,
			    .musicVolume = settings.musicVolume,
			});
		}
		const auto kept = gui::ToProfileSettings(settings);
		ApplyPlayerSettings(kept);
		// The player's options are kept with their profile
		Locator::playerProfileSystem::value().SetCurrentSettings(kept);
		SaveHelpRecord();
	}

	// A binding changed on the Controls page takes effect at once
	auto& menu = Locator::gameInterface::value().GetMenu();
	if (menu.TakeControlsChanged() && Locator::gameActionSystem::has_value())
	{
		Locator::gameActionSystem::value().SetKeyBindings(menu.GetControlBindings());
	}
	// The menu reads the bindings from the profile as it opens. Back from the Controls page, and the menu as it
	// closes, keep the bindings with the profile.
	if (menu.IsOpen() && !_menuWasOpen)
	{
		LoadProfileKeys();
	}
	if (menu.TakeControlsToKeep() || (!menu.IsOpen() && _menuWasOpen))
	{
		KeepProfileKeys();
	}
	_menuWasOpen = menu.IsOpen();

	// The menu pauses the game while it is open
	if (const auto pause = _menuPause.Follow(Locator::gameInterface::value().GetMenu().IsOpen()); pause.has_value())
	{
		Locator::time::value().SetPlayerPause(*pause);
		if (!_menuPause.IsHeld())
		{
			Locator::gameKeysSystem::value().MenuClosed();
		}
	}
	// The main menu comes back under each page that the menu shows, and pauses the game again. This is important after
	// the question of the trip unpaused the game over another page. Inside the temple, the options show alone.
	if (menu.TakePageShown() && menu.IsOpen() && !menu.IsInsideTemple())
	{
		Locator::time::value().SetPlayerPause(true);
	}

	switch (action)
	{
	case Action::Quit:
		RequestQuit();
		break;
	case Action::CreatePlayer:
		AskForAnotherPlayer(true);
		break;
	case Action::PickPlayer:
		if (const auto picked = Locator::gameInterface::value().GetMenu().GetPickedPlayer(); picked.has_value())
		{
			auto& profiles = Locator::playerProfileSystem::value();
			const auto& list = profiles.GetProfiles().profiles;
			if (*picked < list.size())
			{
				profiles.SelectProfile(gui::ToUtf16(list.at(*picked).name));
				PlayerProfileChanged();
			}
		}
		break;
	case Action::DeletePlayer:
	{
		auto& profiles = Locator::playerProfileSystem::value();
		// The player's folder is emptied first: its saved games, pictures and help record go, but the empty folder stays
		const auto deleted = Locator::gameInterface::value().GetMenu().GetPlayerToDelete();
		if (Locator::profileFolderSystem::has_value())
		{
			Locator::profileFolderSystem::value().EmptyPlayerFolder(deleted);
		}
		profiles.DeleteProfile(deleted);
		const auto next = Locator::gameInterface::value().GetMenu().GetPickedPlayer();
		const auto& list = profiles.GetProfiles().profiles;
		if (next.has_value() && *next < list.size())
		{
			profiles.SelectProfile(gui::ToUtf16(list.at(*next).name));
			PlayerProfileChanged();
		}
		else
		{
			// With no profile left the new player's box asks for one, and can't be cancelled
			PlayerProfileChanged();
			AskForAnotherPlayer(false);
		}
		break;
	}
	case Action::StartNewGame:
	case Action::Restart:
		// This game ends. Start a New Game starts a new game on the first land. A restart as the profile picked continues
		// from the save on exit of that profile, or starts a new game when it has none. The land changes between frames,
		// not while the game reads the input.
		Locator::gameInterface::value().GetMenu().Close();
		_gameEndAsked = action == Action::StartNewGame ? game_session::End::NewGame : game_session::End::Continue;
		break;
	case Action::EditTattoo:
		// The profile is kept first, then the editor shows the profile's creature over the menu
		if (Locator::tattooMenuSystem::has_value())
		{
			Locator::tattooMenuSystem::value().Open();
		}
		break;
	case Action::JoinOnline:
		if (!OpenMultiplayerFrontEnd(MakeMultiplayerFrontEnd()))
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "The multiplayer front end needs the temple and the interface");
		}
		break;
	case Action::StartSkirmish:
		// The game ends at once, without a question. The story keeps its save on exit, then the skirmish box shows. The
		// land changes between frames, not while the game reads the input.
		Locator::gameInterface::value().GetMenu().Close();
		_gameEndAsked = game_session::End::Skirmish;
		break;
	case Action::LeaveSkirmish:
		// Yes to the question ends the skirmish, and the skirmish box shows again. A skirmish keeps no save on exit.
		Locator::gameInterface::value().GetMenu().Close();
		_gameEndAsked = game_session::End::Skirmish;
		break;
	case Action::ExitOnlineGame:
		ExitOnlineGame();
		break;
	case Action::AskToLeave:
		AskToLeaveNetworkGame();
		break;
	case Action::Continue:
	case Action::None:
		break;
	}
}

void Game::UpdateTattooMenu(float milliseconds)
{
	if (!Locator::tattooMenuSystem::has_value() || !Locator::gameInterface::has_value())
	{
		return;
	}
	auto& tattooMenu = Locator::tattooMenuSystem::value();
	auto& menu = Locator::gameInterface::value().GetMenu();
	if (menu.IsOpen())
	{
		menu.SetTattooAvailable(tattooMenu.IsButtonShown(menu.IsRestartPending()));
	}
	else if (tattooMenu.IsOpen())
	{
		// The creature goes with the menu
		tattooMenu.Close();
	}
	if (!tattooMenu.IsOpen() || !Locator::windowing::has_value())
	{
		return;
	}
	// The editor's view is the dialogs' space on the window, as the dialog is laid out
	const auto size = Locator::windowing::value().GetSize();
	const auto space = gui::FrontEndSpace(static_cast<int32_t>(size.x), static_cast<int32_t>(size.y));
	const auto origin = space.ToScreen(glm::vec2(0.0f));
	const auto corner = space.ToScreen(glm::vec2(gui::DialogPainter::k_Size));
	const auto pointer = Locator::gameActionSystem::value().GetPointerPosition();
	tattooMenu.Update(milliseconds, pointer, Locator::gameInterface::value().IsTattooEditorButtonHeld(),
	                  Locator::gameInterface::value().IsTattooEditorLeftHeld(), {.origin = origin, .size = corner - origin});
}

void Game::ShowTattooMenuCreature() const
{
	if (!Locator::tattooMenuSystem::has_value() || !Locator::rendererInterface::has_value())
	{
		return;
	}
	if (const auto shown = Locator::tattooMenuSystem::value().GetShown(); shown.has_value())
	{
		Locator::rendererInterface::value().ShowMenuCreature({.copy = shown->copy,
		                                                      .view = shown->view,
		                                                      .projection = shown->projection,
		                                                      .viewOrigin = shown->viewport.origin,
		                                                      .viewSize = shown->viewport.size});
	}
}

void Game::CloseMenuForNewLand()
{
	// The trip to the Gods' Playground closes no box. An open menu keeps its pause, and the land information box stays,
	// while the land of the Playground loads. The cinema bars of the first lesson close them.
	if (std::exchange(_boxesStayForTrip, false))
	{
		return;
	}
	// Each other change of land comes only when the menu is closed. The menu closes before the choices in it that change
	// the land. This also closes the menu for a land that the debug tools load.
	if (Locator::gameInterface::has_value() && Locator::gameInterface::value().GetMenu().IsOpen())
	{
		Locator::gameInterface::value().GetMenu().Close();
	}
	// The end of a game closes every box of the game. The script of the next land shows the land information box
	// again when the land has one. This also closes the box for a land that the debug tools load.
	if (Locator::gameInterface::has_value() && Locator::gameInterface::value().GetLandInfoBox().IsActive())
	{
		Locator::gameInterface::value().GetLandInfoBox().Close();
	}
	// The game gets the pause that it had before the menu opened, before the next land asks for a pause of its own
	if (const auto pause = _menuPause.Release(); pause.has_value())
	{
		Locator::time::value().SetPlayerPause(*pause);
		Locator::gameKeysSystem::value().MenuClosed();
	}
}

void Game::RefreshPlayersPage()
{
	if (!Locator::gameInterface::has_value() || !Locator::playerProfileSystem::has_value())
	{
		return;
	}
	const auto& profiles = Locator::playerProfileSystem::value().GetProfiles();
	std::vector<std::u16string> names;
	names.reserve(profiles.profiles.size());
	std::optional<size_t> current;
	for (const auto& profile : profiles.profiles)
	{
		if (profile.name == profiles.current)
		{
			current = names.size();
		}
		names.push_back(gui::ToUtf16(profile.name));
	}
	Locator::gameInterface::value().GetMenu().SetPlayers(std::move(names), current);
}

void Game::PlayerProfileChanged()
{
	// The help record of the new player comes from their profile. Without their file, the record stays as it is
	LoadHelpRecord();
	// The pictures of the new player replace those of the designs that they have pictures for
	ReadTattooPictures();
	RefreshPlayersPage();
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	if (!profile.has_value() || !Locator::gameInterface::has_value())
	{
		return;
	}
	// The page shows the profile's creature's name, symbol and options, which take effect
	Locator::gameInterface::value().GetMenu().ShowPlayer(gui::ToUtf16(profile->creatureName), profile->symbol,
	                                                     profile->settings);
	ApplyPlayerSettings(profile->settings);
	// The bindings of the new player come from their profile
	LoadProfileKeys();
	NameCreatureAfterProfile();
}

void Game::ReadTattooPictures()
{
	// The symbols' picture and the tattoo designs change with the pictures. The game loads them one time only
	// (vanilla-bugs 182), and openblack fixes this.
	if (const auto pictures = simulation::ReadTattooPictures(); pictures.has_value() && Locator::gameInterface::has_value())
	{
		Locator::gameInterface::value().SetSymbolPictures(*pictures);
	}
}

void Game::MakePlayersSymbols(std::optional<uint32_t> symbolInPlace)
{
	game_assets::MakePlayersSymbols(symbolInPlace);
}

void Game::NameCreatureAfterProfile()
{
	// In a game alone, the player's creature takes the name its profile gives it
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	if (!profile.has_value() || !Locator::playerSystem::has_value() ||
	    !creature_names::ProfileRenamesCreature(Locator::time::value().IsNetworkGame()))
	{
		return;
	}
	const auto creature = Locator::playerSystem::value().GetPrimaryCreature(PlayerNames::PLAYER_ONE);
	auto& registry = Locator::entitiesRegistry::value();
	if (!creature.has_value() || !registry.Valid(*creature))
	{
		return;
	}
	if (auto* component = registry.TryGet<ecs::components::Creature>(*creature); component != nullptr)
	{
		component->name = gui::ToUtf16(profile->creatureName);
	}
}

void Game::AskForAnotherPlayer(bool canCancel)
{
	auto& profiles = Locator::playerProfileSystem::value();
	SDL_StartTextInput();
	Locator::gameInterface::value().ShowNewPlayerBoxOverGame(
	    canCancel && profiles.GetProfileCount() > 0,
	    [&profiles](std::u16string_view name) { return profiles.CheckNewName(name); });
}

void Game::TellMenuTheGame()
{
	if (!Locator::gameInterface::has_value())
	{
		return;
	}
	auto& menu = Locator::gameInterface::value().GetMenu();
	menu.SetNetworkGame(InNetworkGame());
	// The land of a network game also plays as a skirmish, but its menu is the menu of a network game
	menu.SetSkirmish(Locator::entitiesRegistry::value().Context().skirmish && !Locator::time::value().IsNetworkGame());
}

void Game::ShowMenuOnStart()
{
	if (!Locator::gameInterface::has_value())
	{
		return;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The menu shows over the game that continued");
	TellMenuTheGame();
	Locator::gameInterface::value().OpenMenu();
	// The menu reads the profile's own pictures again as it opens
	ReadTattooPictures();
	// The menu pauses the game while it is open
	HandleInterfaceAction();
}

void Game::ApplyPlayerSettings(const player_profiles::Settings& settings)
{
	simulation::ApplyPlayerSettings(settings);
	if (Locator::gameInterface::has_value())
	{
		constexpr int k_Levels = 4;
		Locator::gameInterface::value().GetToolTips().SetLevel(
		    static_cast<gui::ToolTipLevel>(std::clamp(settings.toolTips, 0, k_Levels - 1)));
	}
}

bool Game::FrontEndBoxShows() const
{
	return Locator::gameInterface::has_value() &&
	       (Locator::gameInterface::value().IsDialogOpen() || Locator::gameInterface::value().GetTattooEditor().IsOpen());
}

bool Game::AskStartQuestion(bool hasCreature)
{
	if (!Locator::gameInterface::has_value())
	{
		return false;
	}
	// The game waits, paused, for the answer
	Locator::time::value().SetPlayerPause(true);
	// The multiplayer front end is in front of the land: the question comes when the player leaves the front end to
	// the land. A join from the command line loads the server's land, which does not ask the question.
	if (Locator::gameInterface::value().GetMultiplayerBoxes().IsActive() || _frontEndOnStart || _networkLandPending)
	{
		_startQuestionAfterFrontEnd = hasCreature;
		return true;
	}
	Locator::gameInterface::value().ShowSkipBox(hasCreature);
	return true;
}

bool Game::WithdrawSkipBox()
{
	return Locator::gameInterface::has_value() && Locator::gameInterface::value().WithdrawSkipBox();
}

void Game::ShowLoadingFrame()
{
	Locator::loadingScreenSystem::value().RenderLoadingFrame(false, nullptr);
}
