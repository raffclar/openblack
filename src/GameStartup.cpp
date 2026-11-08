/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The parts of the start of the game and of a land that the game itself does: the players' profiles, the new player's
// box and the skirmish box. The loading screen system draws the screens. The load finish system does the work behind
// the loading screen.

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <SDL.h>
#include <spdlog/spdlog.h>

#include "Common/MachineClock.h"
#include "Common/UserData.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/InspectorLoading.h"
#include "ECS/Systems/LoadFinishSystemInterface.h"
#include "ECS/Systems/LoadingScreenSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/ProfileFolderSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "Gui/GameInterface.h"
#include "Gui/LoadingScreenRules.h"
#include "Gui/MenuBackdrop.h"
#include "Gui/TextDatabase.h"
#include "Input/WindowPointer.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;

namespace
{
/// Milliseconds since the program started, as the game counts frames with
uint32_t Milliseconds()
{
	return SDL_GetTicks();
}
} // namespace

bool Game::HasPlayerProfiles() noexcept
{
	return Locator::playerProfileSystem::has_value() && Locator::playerProfileSystem::value().GetProfileCount() > 0;
}

player_profiles::Store Game::ProfileStore() const
{
	auto store = player_profiles::StoreFor(
	    {.profileFolder = _profileFolder, .userFolder = user_data::ProfileFolder(), .automated = _automatedRun});
	if (store.developer)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "A developer's run: playing as the profile \"{}\", its creature kept in {}",
		                   player_profiles::k_DeveloperName,
		                   store.creatureMinds.has_value() ? store.creatureMinds->generic_string() : "no folder");
	}
	else if (store.folder.has_value())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The players' profiles are kept in {}", store.folder->generic_string());
	}
	return store;
}

std::optional<std::u16string> Game::PlayerForProfileFiles() const
{
	if (_developerProfile || !Locator::playerProfileSystem::has_value())
	{
		return std::nullopt;
	}
	const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile();
	if (!profile.has_value())
	{
		return std::nullopt;
	}
	return gui::ToUtf16(profile->name);
}

void Game::LoadHelpRecord()
{
	const auto player = PlayerForProfileFiles();
	if (!player.has_value() || !Locator::profileFolderSystem::has_value() || !Locator::helpProfileSystem::has_value())
	{
		return;
	}
	Locator::helpProfileSystem::value().LoadRecord(Locator::profileFolderSystem::value(), *player);
}

void Game::SaveHelpRecord()
{
	const auto player = PlayerForProfileFiles();
	if (!player.has_value() || !Locator::profileFolderSystem::has_value() || !Locator::helpProfileSystem::has_value())
	{
		return;
	}
	Locator::helpProfileSystem::value().SaveRecord(Locator::profileFolderSystem::value(), *player);
}

bool Game::AskForNewPlayer()
{
	auto& profiles = Locator::playerProfileSystem::value();
	if (profiles.GetProfileCount() > 0)
	{
		return true;
	}
	if (!Locator::gameInterface::has_value() || !Locator::windowing::has_value() ||
	    Locator::config::value().graphicsBackend == GraphicsBackend::Noop)
	{
		// With no box to ask in, the player gets the profile the game makes for one who gives no name: the account's
		profiles.CreateProfile(u"", 0);
		return !_quitRequested;
	}
	// Tools wait while the player is asked
	const ecs::systems::InspectorLoading asking("new player");
	// Tools type the name and press Return through the inspector
	const ecs::systems::InspectorStartupBox typing;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "No player has a profile: asking the new player for their name");
	SDL_StartTextInput();
	Locator::gameInterface::value().ShowNewPlayerBox(
	    false, [&profiles](std::u16string_view name) { return profiles.CheckNewName(name); });
	gui::menu_backdrop::Fade fade;
	auto last = Milliseconds();
	while (!gui::menu_backdrop::FadeOver(fade) && !_quitRequested)
	{
		const auto now = Milliseconds();
		const auto frame = static_cast<int>(now - last);
		last = now;
		auto& box = Locator::gameInterface::value().GetNewPlayerBox();
		const auto resolution = static_cast<glm::u16vec2>(Locator::windowing::value().GetSize());
		SDL_Event event;
		while (SDL_PollEvent(&event) != 0)
		{
			// On the web the mouse events get the place of the pointer of the game
			input::PointWindowEvent(event);
			if (event.type == SDL_QUIT)
			{
				RequestQuit();
			}
			else
			{
				Locator::gameInterface::value().ProcessNewPlayerBoxEvent(event, resolution);
			}
		}
		// The profile is made as the box is answered
		if (const auto answer = box.TakeAnswer(); answer.has_value())
		{
			profiles.CreateProfile(answer->name, answer->symbol);
		}
		gui::menu_backdrop::StepFade(fade, !box.IsActive(), frame);
		const auto mouse = input::WindowPointer();
		Locator::loadingScreenSystem::value().PresentStartupFrame([mouse, frame, &fade](glm::u16vec2 size) {
			Locator::gameInterface::value().DrawNewPlayerScreen(size, mouse, machine_clock::Ticks(), static_cast<float>(frame),
			                                                    fade.alpha);
		});
	}
	SDL_StopTextInput();
	if (const auto profile = profiles.GetCurrentProfile(); profile.has_value())
	{
		Locator::gameInterface::value().SetPlayerProfile(gui::ToUtf16(profile->name), gui::ToUtf16(profile->creatureName),
		                                                 profile->symbol, profile->settings);
		RefreshPlayersPage();
	}
	return !_quitRequested;
}

std::optional<std::filesystem::path> Game::AskForSkirmishLand()
{
	if (!Locator::gameInterface::has_value() || !Locator::windowing::has_value() ||
	    Locator::config::value().graphicsBackend == GraphicsBackend::Noop)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The -SKIRMISH switch: there is no window for the skirmish box");
		return std::nullopt;
	}
	// Tools wait while the player chooses
	const ecs::systems::InspectorLoading asking("skirmish box");
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The -SKIRMISH switch: the skirmish box asks for the land");
	_skirmishLands = ReadSkirmishLands();
	std::vector<std::u16string> names;
	names.reserve(_skirmishLands.size());
	for (const auto& land : _skirmishLands)
	{
		names.push_back(land.titleText.has_value()
		                    ? std::u16string(Locator::gameInterface::value().GetTexts().Get(*land.titleText))
		                    : land.stem);
	}
	Locator::gameInterface::value().ShowSkirmishBox(std::move(names));
	gui::menu_backdrop::Fade fade;
	auto last = Milliseconds();
	std::optional<std::filesystem::path> picked;
	// A run with a number of frames (-n) stops after that number of frames, also in the box
	const auto frameLimit = Locator::config::value().numFramesToSimulate;
	uint32_t boxFrames = 0;
	while (!gui::menu_backdrop::FadeOver(fade) && !_quitRequested)
	{
		if (frameLimit > 0 && ++boxFrames > frameLimit)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "The -SKIRMISH switch: the run stops in the skirmish box after {} frames",
			                   frameLimit);
			RequestQuit();
			break;
		}
		const auto now = Milliseconds();
		const auto frame = static_cast<int>(now - last);
		last = now;
		auto& box = Locator::gameInterface::value().GetSkirmishBox();
		const auto resolution = static_cast<glm::u16vec2>(Locator::windowing::value().GetSize());
		SDL_Event event;
		while (SDL_PollEvent(&event) != 0)
		{
			// On the web the mouse events get the place of the pointer of the game
			input::PointWindowEvent(event);
			if (event.type == SDL_QUIT)
			{
				RequestQuit();
			}
			else
			{
				Locator::gameInterface::value().ProcessSkirmishBoxEvent(event, resolution);
			}
		}
		if (const auto answer = box.TakeAnswer();
		    answer.choice == gui::SkirmishBox::Choice::Start && answer.land.has_value() && *answer.land < _skirmishLands.size())
		{
			picked = Locator::filesystem::value().GetPath<filesystem::Path::Playgrounds>() /
			         _skirmishLands.at(*answer.land).fileName;
		}
		gui::menu_backdrop::StepFade(fade, !box.IsActive(), frame);
		const auto mouse = input::WindowPointer();
		Locator::loadingScreenSystem::value().PresentStartupFrame([mouse, frame, &fade](glm::u16vec2 size) {
			Locator::gameInterface::value().DrawNewPlayerScreen(size, mouse, machine_clock::Ticks(), static_cast<float>(frame),
			                                                    fade.alpha);
		});
	}
	return _quitRequested ? std::nullopt : picked;
}

void Game::PlayStartupScreens()
{
	Locator::loadingScreenSystem::value().PlayStartupScreens(
	    {.build = _buildVersion.Short(), .skipLogos = _skipLogos, .preIntro = _preIntro});
}

void Game::BeginLoadingScreen(loading::LoadingClock::Mode look)
{
	Locator::loadingScreenSystem::value().BeginLoading(look);
}

bool Game::RenderLoadingFrame(bool drawNow, const std::function<void()>& drawUnder)
{
	return Locator::loadingScreenSystem::value().RenderLoadingFrame(drawNow, drawUnder);
}

void Game::EndLoadWithoutScreen()
{
	Locator::loadFinishSystem::value().EndWithoutScreen();
}

void Game::EndLoadingScreen()
{
	auto& screens = Locator::loadingScreenSystem::value();
	if (!screens.IsLoading())
	{
		return;
	}
	Locator::loadFinishSystem::value().FinishBehindScreen();
	screens.EndLoading();
}
