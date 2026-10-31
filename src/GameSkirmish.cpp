/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The skirmish box of the game: the land list from the playground folder, and the choice that the player makes in it

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "ECS/Systems/SimulationSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "Gui/GameInterface.h"
#include "Gui/SkirmishBox.h"
#include "Gui/SkirmishBoxRules.h"
#include "Gui/TextDatabase.h"
#include "Locator.h"

using namespace openblack;

namespace
{
/// This computer plays a network game with a session of a game server
bool HasNetworkSession()
{
	return Locator::networkGame::has_value() && Locator::networkGame::value().Session() != nullptr;
}
} // namespace

std::vector<gui::skirmish_box::Land> Game::ReadSkirmishLands()
{
	auto& fileSystem = Locator::filesystem::value();
	std::vector<std::u16string> fileNames;
	// The box reads the folder each time it opens, so that a land added meanwhile shows too
	fileSystem.Iterate(fileSystem.GetPath<filesystem::Path::Playgrounds>(), false,
	                   [&fileNames](const std::filesystem::path& file) { fileNames.push_back(file.filename().u16string()); });
	return gui::skirmish_box::ListLands(fileNames);
}

bool Game::OpenSkirmishBoxOverGame()
{
	if (!_interface)
	{
		return false;
	}
	_skirmishLands = ReadSkirmishLands();
	std::vector<std::u16string> names;
	names.reserve(_skirmishLands.size());
	for (const auto& land : _skirmishLands)
	{
		names.push_back(land.titleText.has_value() ? std::u16string(_interface->GetTexts().Get(*land.titleText)) : land.stem);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The skirmish box shows {} lands", names.size());
	// The game that ended stays under the box and does not move on
	Locator::time::value().SetPlayerPause(true);
	_interface->ShowSkirmishBoxOverGame(std::move(names));
	return true;
}

void Game::FollowSkirmishBox()
{
	if (!_interface)
	{
		return;
	}
	if (auto answer = _interface->GetSkirmishBox().TakeAnswer(); answer.choice != gui::SkirmishBox::Choice::None)
	{
		_skirmishAnswer = answer;
	}
	// The game acts on the answer once the screen is black after the box
	if (!_skirmishAnswer.has_value() || _interface->ShowsBoxOverGame())
	{
		return;
	}
	const auto answer = *std::exchange(_skirmishAnswer, std::nullopt);
	switch (answer.choice)
	{
	case gui::SkirmishBox::Choice::None:
		return;
	case gui::SkirmishBox::Choice::Back:
		// Back goes to the story: its save on exit loads with the menu over it, or a new game starts without one
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The skirmish box: Back continues the story");
		Locator::time::value().SetPlayerPause(false);
		ContinueFromExitSave();
		return;
	case gui::SkirmishBox::Choice::Start:
		break;
	}
	if (!answer.land.has_value() || *answer.land >= _skirmishLands.size())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The skirmish box: Start Game has no land");
		return;
	}
	const auto path =
	    Locator::filesystem::value().GetPath<filesystem::Path::Playgrounds>() / _skirmishLands.at(*answer.land).fileName;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The skirmish box: the skirmish starts on {}", path.generic_string());
	Locator::time::value().SetPlayerPause(false);
	if (!LoadFromLandMenu(path))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The skirmish land {} did not load", path.generic_string());
	}
}

void Game::ShowLandMessage(const land_message::Message& message)
{
	if (!_interface)
	{
		return;
	}
	// A number other than 0 names a help text that the box shows in place of the text of the script line
	const auto helpText = land_message::HelpTextOf(message.number);
	auto text = helpText.has_value() ? std::u16string(_interface->GetTexts().Get(*helpText)) : gui::ToUtf16(message.text);
	_interface->ApplyLandMessage(std::move(text), message.kind == land_message::Message::Kind::Start);
}

void Game::FollowLandInfoBox()
{
	if (!_interface)
	{
		return;
	}
	PressLandInfoBoxBackForTests();
	switch (_interface->GetLandInfoBox().TakeChoice())
	{
	case gui::LandInfoBox::Choice::None:
	case gui::LandInfoBox::Choice::Go:
		return;
	case gui::LandInfoBox::Choice::Back:
		break;
	}
	// Back ends the skirmish, and the skirmish box shows again. In a network game, Back also leaves the network game
	// as Leave Game does, and the other computers see a god that leaves. Then the skirmish box shows.
	if (HasNetworkSession())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The land information box: Back leaves the network game");
		LeaveNetworkGame(game_session::NetworkLeave::LandInfoBoxBack);
		return;
	}
	// A tool can play the world as a network game with no network: there Back only closes the box
	if (Locator::time::value().IsNetworkGame())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "The land information box: Back with no network session only closes the box");
		return;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "The land information box: Back ends the skirmish");
	_gameEndAsked = game_session::End::Skirmish;
}

void Game::PressLandInfoBoxBackForTests()
{
	const auto backAt = Locator::simulation::value().GetSettings().landInfoBoxBackAt;
	if (!backAt.has_value() || _testLandInfoBoxBackDone || Locator::time::value().GetTurn() < *backAt || !HasNetworkSession())
	{
		return;
	}
	_testLandInfoBoxBackDone = true;
	auto& box = _interface->GetLandInfoBox();
	const auto back = std::u16string(_interface->GetTexts().Get("HELP_TEXT_DIALOG_BACK"));
	const auto controls = box.GetNamedControls();
	const auto found = std::ranges::find(controls, back, &gui::Dialog::NamedControl::name);
	if (found == controls.end())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Tests: the land information box does not show at turn {}",
		                   Locator::time::value().GetTurn());
		return;
	}
	// The press and the release of the mouse button over Back, as a player clicks
	const auto centre = (found->rect.min + found->rect.max) / 2;
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tests: the player presses Back in the land information box at turn {}",
	                   Locator::time::value().GetTurn());
	box.MouseDown(centre);
	std::ignore = box.MouseUp(centre);
}
