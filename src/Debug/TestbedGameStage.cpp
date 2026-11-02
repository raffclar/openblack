/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedGameStage.h"

#include <algorithm>
#include <string>
#include <utility>

#include <SDL_keycode.h>
#include <fmt/format.h>

#include "Debug/TestbedMultiplayer.h"
#include "ECS/Systems/TattooMenuSystemInterface.h"
#include "Game.h"
#include "Gui/GameInterface.h"
#include "Gui/OnlineMapTexts.h"
#include "Locator.h"

namespace openblack::testbed_scenarios
{

bool TestbedGameStage::HasInterface() const
{
	auto* game = Game::Instance();
	return game != nullptr && game->GetInterface() != nullptr;
}

const gui::TextDatabase* TestbedGameStage::GetTexts() const
{
	return HasInterface() ? &Game::Instance()->GetInterface()->GetTexts() : nullptr;
}

std::string TestbedGameStage::OpenTattooMenu()
{
	if (!HasInterface() || !Locator::tattooMenuSystem::has_value())
	{
		return "failed: no menu";
	}
	// The menu opens on its Players page, and the tattoo editor over it, as the Edit Tattoo button of the page opens it
	auto& interface = *Game::Instance()->GetInterface();
	interface.OpenMenu();
	interface.GetMenu().ShowPage(gui::GameMenu::Page::Players);
	Locator::tattooMenuSystem::value().Open();
	return Locator::tattooMenuSystem::value().IsOpen() ? "the tattoo editor shows its creature over the menu"
	                                                   : "failed: the editor did not open";
}

std::string TestbedGameStage::TattooEditorAsNetworkGame(bool asNetworkGame)
{
	if (!HasInterface())
	{
		return "failed: no interface";
	}
	Game::Instance()->GetInterface()->SetTattooEditorAsNetworkGame(asNetworkGame);
	return asNetworkGame ? "OK warns about a change first" : "OK closes the editor";
}

std::string TestbedGameStage::OpenControlsBox(size_t row)
{
	if (!HasInterface())
	{
		return "failed: no menu";
	}
	auto& interface = *Game::Instance()->GetInterface();
	interface.OpenMenu();
	auto& menu = interface.GetMenu();
	menu.ShowPage(gui::GameMenu::Page::Controls);
	const auto& list = menu.GetControls();
	for (int y = list.rect.min.y; y < list.rect.max.y; ++y)
	{
		if (list.ItemAt(y) == row)
		{
			const glm::ivec2 point {list.rect.Centre().x, y + 4};
			menu.MouseDown(point);
			menu.MouseUp(point);
			menu.DoubleClick(point);
			break;
		}
	}
	return menu.IsCapturing() ? fmt::format("the box asks for the new input of row {}", row) : "failed: no box";
}

void TestbedGameStage::OpenChat(std::u16string_view line)
{
	auto& controls = Game::Instance()->GetInterface()->GetChatControls();
	controls.Open();
	controls.TextInput(std::u16string(line));
}

std::string TestbedGameStage::OpenMultiplayerBox(size_t stage)
{
	auto* game = Game::Instance();
	if (!HasInterface() || stage >= static_cast<size_t>(testbed_multiplayer::Stage::_Count))
	{
		return "failed: no interface";
	}
	auto maps = gui::ReadOnlineMapTexts();
	auto frontEnd = testbed_multiplayer::MakeDemoFrontEnd(static_cast<testbed_multiplayer::Stage>(stage), u"Alice",
	                                                      std::move(maps.localMaps), std::move(maps.conditionTemplate));
	if (!game->OpenMultiplayerFrontEnd(std::move(frontEnd)))
	{
		return "failed: no temple";
	}
	return fmt::format("the front end goes to stage {}", stage);
}

std::string TestbedGameStage::OpenMenuPage(size_t page, bool noSound)
{
	if (!HasInterface() || page >= static_cast<size_t>(gui::GameMenu::Page::_Count))
	{
		return "failed: no menu";
	}
	auto& interface = *Game::Instance()->GetInterface();
	interface.OpenMenu();
	auto& menu = interface.GetMenu();
	if (noSound)
	{
		menu.SetSoundInstalled(false);
	}
	menu.ShowPage(static_cast<gui::GameMenu::Page>(page));
	return fmt::format("the menu shows page {}{}", page, noSound ? " as with no sound" : "");
}

std::string TestbedGameStage::PressMenuKey(size_t key, bool shift)
{
	if (!HasInterface() || !Game::Instance()->GetInterface()->GetMenu().IsOpen())
	{
		return "failed: the menu is not open";
	}
	auto& menu = Game::Instance()->GetInterface()->GetMenu();
	const auto action = static_cast<int>(key) == SDLK_ESCAPE ? menu.Escape() : menu.KeyDown(static_cast<int>(key), shift);
	if (action == gui::GameMenu::Action::Continue)
	{
		menu.Close();
	}
	return fmt::format("key {}{} pressed", key, shift ? " with Shift" : "");
}

std::string TestbedGameStage::OpenSkirmishMenu(bool askLeave)
{
	auto& interface = *Game::Instance()->GetInterface();
	auto& menu = interface.GetMenu();
	menu.SetSkirmish(true);
	interface.OpenMenu();
	if (!askLeave)
	{
		return "the main menu of a skirmish opens";
	}
	// A click on Leave Skirmish Game: the menu asks whether to leave
	const auto controls = menu.GetNamedControls();
	const auto leave = std::ranges::find_if(controls, [&interface](const gui::Dialog::NamedControl& control) {
		return control.name == interface.GetTexts().Get("HELP_TEXT_DIALOG_ADDITION_170");
	});
	if (leave != controls.end())
	{
		const auto middle = (leave->rect.min + leave->rect.max) / 2;
		menu.MouseDown(middle);
		[[maybe_unused]] const auto action = menu.MouseUp(middle);
	}
	return fmt::format("the main menu of a skirmish {}", menu.IsAskingToQuit() ? "asks to leave the skirmish" : "did not ask");
}

std::string TestbedGameStage::OpenSkirmishBox(bool selectSecond)
{
	auto* game = Game::Instance();
	// The box holds the game, so that the clock of the scenario stops while the box is up
	const bool opened = game->OpenSkirmishBoxOverGame();
	if (opened && selectSecond)
	{
		game->GetInterface()->GetSkirmishBox().Select(1);
	}
	return opened ? fmt::format("the box shows {} lands", game->GetSkirmishLands().size()) : "failed";
}

std::string TestbedGameStage::OpenEndBox(size_t value)
{
	auto* game = Game::Instance();
	if (value == 3)
	{
		game->GetInterface()->GetMenu().SetNetworkGame(true);
		game->GetInterface()->OpenMenu();
		return "the main menu of a network game opens";
	}
	if (value == 0)
	{
		game->AskToLeaveNetworkGame();
		return "the question to leave opens";
	}
	if (value >= 4)
	{
		const auto notches = static_cast<int>(value) - 3;
		const bool moved = game->GetInterface()->GetEndGameBox().ScrollGodWindow(notches);
		return fmt::format("the window of gods {} {} notches on", moved ? "scrolls" : "does not scroll", notches);
	}
	const bool opened = game->GetInterface()->GetEndGameBox().OpenTab(value);
	return fmt::format("tab {} {}", value, opened ? "opens its box" : "does not open");
}

std::string TestbedGameStage::ShowCustomMusicControls(size_t value)
{
	auto& controls = Game::Instance()->GetInterface()->GetMusicControls();
	controls.ShowControls(value >= 1, value >= 2);
	return fmt::format("buttons {}, play list {}, the controls {}", controls.GetPanel().controlsShown ? "shown" : "hidden",
	                   controls.GetPanel().playListShown ? "shown" : "hidden", controls.IsActive() ? "show" : "hide now");
}

void TestbedGameStage::MakePlayersSymbols(std::optional<uint32_t> symbol)
{
	if (auto* game = Game::Instance(); game != nullptr)
	{
		game->MakePlayersSymbols(symbol);
	}
}

void TestbedGameStage::SetDemoRoundTrip(std::optional<uint64_t> microseconds)
{
	if (auto* game = Game::Instance(); game != nullptr)
	{
		game->SetDemoRoundTrip(microseconds);
	}
}

void TestbedGameStage::SetMousePosition(glm::ivec2 position)
{
	Game::Instance()->SetMousePosition(position);
}

bool TestbedGameStage::RequestJoinOnline()
{
	auto* game = Game::Instance();
	if (game == nullptr)
	{
		return false;
	}
	game->RequestJoinOnline();
	return true;
}

void TestbedGameStage::PlayLogoEffect()
{
	if (auto* game = Game::Instance(); game != nullptr)
	{
		game->PlayLogoEffect();
	}
}

bool TestbedGameStage::ContinueFromExitSave()
{
	auto* game = Game::Instance();
	return game != nullptr && game->ContinueFromExitSave();
}

void TestbedGameStage::RequestQuit()
{
	if (auto* game = Game::Instance(); game != nullptr)
	{
		game->RequestQuit();
	}
}

void TestbedGameStage::ReportChecks(checks::Report report, bool quit)
{
	auto* game = Game::Instance();
	if (game == nullptr)
	{
		return;
	}
	if (quit)
	{
		// The lines go to the console too, where a tool that runs the game reads them
		for (const auto& line : report.lines)
		{
			fmt::print("{}\n", line);
		}
		game->SetExitCode(checks::ExitCodeOf(report.verdict));
		game->RequestQuit();
	}
	game->SetScenarioChecks(std::move(report));
}

} // namespace openblack::testbed_scenarios
