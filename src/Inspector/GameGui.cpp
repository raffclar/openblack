/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers

 * *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit
 * https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.

 * *******************************************************************************/

#include "GameGui.h"

#include <array>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL_keyboard.h>
#include <fmt/format.h>

#include "3D/TempleInteriorInterface.h"
#include "DialogControls.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "Game.h"
#include "Gui/GameInterface.h"
#include "Gui/GameMenu.h"
#include "Gui/IncomingMessageStripRules.h"
#include "Gui/MultiplayerFrontEnd.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::inspector::game_windows;

namespace
{

/// The names of the multiplayer front end's boxes, for gui.windows
constexpr std::array<std::string_view, 6> k_MultiplayerBoxes = {"choose_connection", "find_games", "login",
                                                                "game_list",         "room",       "conditions"};

std::string_view PageName(gui::GameMenu::Page page)
{
	constexpr std::array<std::string_view, 7> k_Pages {"main",     "options",    "players",   "advanced",
	                                                   "controls", "statistics", "conditions"};
	const auto index = static_cast<size_t>(page);
	return index < k_Pages.size() ? k_Pages.at(index) : "unknown";
}

/// The point on the screen at a point of the game's dialog space, for the size of the window now
glm::ivec2 DialogPointOnScreen(glm::ivec2 dialog)
{
	const auto size = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(800, 600);
	return DialogToScreen(dialog, size);
}

/// The reason that Escape does not open the game's menu now. The checks follow the order in which the game gives
/// Escape to its parts. The reason is empty when Escape opens the menu.
std::string WhyEscapeKeepsMenuShut(Game& game, gui::GameInterface& interface)
{
	if (Locator::videoSystem::has_value() && Locator::videoSystem::value().IsPlaying())
	{
		return "a full-screen video plays, and Escape skips the video; send input_key Escape to skip it";
	}
	if (interface.GetTattooEditor().IsOpen())
	{
		return "the tattoo editor is open, and it takes Escape";
	}
	if (Locator::temple::has_value() && Locator::temple::value().Active())
	{
		return "the player is in the temple, and Escape leads out of the temple";
	}
	if (Locator::creatureCaveSystem::has_value() && Locator::creatureCaveSystem::value().IsOpen() &&
	    !Locator::creatureCaveSystem::value().InTemple())
	{
		return "the Creature Cave is open, and Escape closes it";
	}
	if (interface.GetSkipBox().IsActive())
	{
		return "the start-of-game question is open, and no key gets past it; answer it with gui.press skip_box OK";
	}
	return std::string(escape_key::WhyMenuStaysShut(game.EscapeSituation(static_cast<uint16_t>(SDL_GetModState()))));
}

/// The reason that a click on the game's menu does not get to the menu now. The reason is empty when the click gets
/// to the menu.
std::string WhyMenuMissesClicks(gui::GameInterface& interface)
{
	if (interface.GetTattooEditor().IsOpen())
	{
		return "the tattoo editor is open, and it takes the clicks";
	}
	if (interface.GetSkipBox().IsActive())
	{
		return "the start-of-game question is open, and it takes the clicks; answer it with gui.press skip_box OK";
	}
	if (interface.GetNewPlayerBox().IsActive())
	{
		return "the new player's box is open, and it takes the clicks";
	}
	return {};
}

} // namespace

std::vector<WindowInfo> GameGui::Windows() const
{
	std::vector<WindowInfo> windows;
	if (auto* game = Game::Instance(); game != nullptr && game->GetInterface() != nullptr)
	{
		const auto& menu = game->GetInterface()->GetMenu();
		WindowInfo info {.name = std::string(k_Menu),
		                 .kind = "game",
		                 .open = menu.IsOpen(),
		                 .page = menu.IsAskingToQuit() ? "question" : std::string(PageName(menu.GetPage())),
		                 .buttons = {}};
		// Every page's buttons, check boxes, sliders and tabs, or the question's answers while it is asked
		for (const auto& control : DialogControls(menu.GetNamedControls()))
		{
			info.buttons.push_back(control.name);
		}
		windows.push_back(std::move(info));

		// The start-of-game question: OK and its answers' radio buttons, and the answer picked
		const auto& skipBox = game->GetInterface()->GetSkipBox();
		WindowInfo skip {.name = std::string(k_SkipBox), .kind = "game", .open = skipBox.IsActive()};
		for (const auto& control : SkipBoxControls(skipBox))
		{
			skip.buttons.push_back(control.name);
		}
		skip.selected = SkipBoxPicked(skipBox);
		windows.push_back(std::move(skip));

		const auto& tattoo = game->GetInterface()->GetTattooEditor();
		WindowInfo editor {.name = std::string(k_TattooEditor), .kind = "game", .open = tattoo.IsOpen()};
		for (const auto& control : DialogControls(tattoo.GetNamedControls()))
		{
			editor.buttons.push_back(control.name);
		}
		windows.push_back(std::move(editor));

		// The custom music controls at the top right of the screen, open while they show
		const auto& music = game->GetInterface()->GetMusicControls();
		WindowInfo player {.name = std::string(k_MusicPlayer), .kind = "game", .open = music.IsActive()};
		for (const auto& control : DialogControls(music.GetNamedControls()))
		{
			player.buttons.push_back(control.name);
		}
		windows.push_back(std::move(player));

		// The end box of a skirmish or a network game: its title, and its controls
		const auto& endBox = game->GetInterface()->GetEndGameBox();
		WindowInfo end {.name = std::string(k_EndBox), .kind = "game", .open = endBox.IsOpen()};
		end.page = endBox.ShowsStatistics()   ? "statistics"
		           : endBox.ShowsConditions() ? "conditions"
		                                      : gui::ToUtf8(endBox.GetContents().title);
		for (const auto& control : DialogControls(endBox.GetNamedControls()))
		{
			end.buttons.push_back(control.name);
		}
		windows.push_back(std::move(end));
		// The skirmish box: its arrows and its lands, and the land picked
		const auto& skirmish = game->GetInterface()->GetSkirmishBox();
		WindowInfo skirmishInfo {.name = std::string(k_SkirmishBox), .kind = "game", .open = skirmish.IsActive()};
		for (const auto& control : DialogControls(skirmish.GetNamedControls()))
		{
			skirmishInfo.buttons.push_back(control.name);
		}
		for (const auto& item : skirmish.GetList().GetItems())
		{
			skirmishInfo.lines.push_back(gui::ToUtf8(item.text));
		}
		if (const auto picked = skirmish.GetSelection(); picked.has_value() && *picked < skirmish.GetList().GetItems().size())
		{
			skirmishInfo.selected = gui::ToUtf8(skirmish.GetList().GetItems().at(*picked).text);
		}
		windows.push_back(std::move(skirmishInfo));
		// The land information box: its title as the page, its arrows, and the lines of its list
		const auto& landInfo = game->GetInterface()->GetLandInfoBox();
		WindowInfo landInfoWindow {.name = std::string(k_LandInfoBox), .kind = "game", .open = landInfo.IsActive()};
		landInfoWindow.page = gui::ToUtf8(landInfo.GetTitle().text);
		for (const auto& control : DialogControls(landInfo.GetNamedControls()))
		{
			landInfoWindow.buttons.push_back(control.name);
		}
		for (const auto& item : landInfo.GetList().GetItems())
		{
			landInfoWindow.lines.push_back(gui::ToUtf8(item.text));
		}
		windows.push_back(std::move(landInfoWindow));
		// The incoming message at the bottom left of the screen, open while its button shows
		const auto& incoming = game->GetInterface()->GetIncomingMessages();
		WindowInfo message {.name = std::string(k_IncomingMessage), .kind = "game", .open = incoming.IsActive()};
		if (incoming.IsActive())
		{
			message.buttons.emplace_back("message");
		}
		windows.push_back(std::move(message));

		// The chat at the top right of the screen: open while its controls show. The page tells whether the line is
		// open, and the lines are the last lines that came to this computer.
		const auto& chatControls = game->GetInterface()->GetChatControls();
		WindowInfo chatWindow {.name = std::string(k_Chat), .kind = "game", .open = chatControls.IsActive()};
		chatWindow.page = chatControls.IsQuickChatOpen() ? "quick chat" : chatControls.IsOpen() ? "open" : "shut";
		chatWindow.selected = gui::ToUtf8(chatControls.GetLine());
		for (const auto& control : DialogControls(chatControls.GetNamedControls()))
		{
			chatWindow.buttons.push_back(control.name);
		}
		if (Locator::chatSystem::has_value())
		{
			constexpr size_t k_ShownChatLines = 10;
			const auto received = Locator::chatSystem::value().GetReceived();
			const auto first = received.size() > k_ShownChatLines ? received.size() - k_ShownChatLines : 0;
			for (const auto& line : received.subspan(first))
			{
				chatWindow.lines.push_back(fmt::format("turn {} god {}{}: {}", line.turn, static_cast<int>(line.sender),
				                                       line.taunt.has_value() ? fmt::format(" taunt {}", *line.taunt) : "",
				                                       gui::ToUtf8(line.text)));
			}
		}
		windows.push_back(std::move(chatWindow));

		// The multiplayer front end over the temple's future room: its box, and the controls of the box or its message
		const auto& boxes = game->GetInterface()->GetMultiplayerBoxes();
		WindowInfo multiplayer {.name = std::string(k_Multiplayer), .kind = "game", .open = boxes.IsActive()};
		if (const auto* frontEnd = boxes.GetFrontEnd(); frontEnd != nullptr)
		{
			multiplayer.page = boxes.GetMessageText().empty()
			                       ? std::string(k_MultiplayerBoxes.at(static_cast<size_t>(frontEnd->GetBox())))
			                       : "message: " + gui::ToUtf8(boxes.GetMessageText());
		}
		for (const auto& control : DialogControls(boxes.GetNamedControls()))
		{
			multiplayer.buttons.push_back(control.name);
		}
		// The games of the game list, or the last lines of the room's chat, for a check of the push channel
		constexpr size_t k_ShownChatLines = 10;
		if (const auto* frontEnd = boxes.GetFrontEnd();
		    frontEnd != nullptr && frontEnd->GetBox() == gui::multiplayer::Box::GameList)
		{
			for (const auto& server : frontEnd->GetGames())
			{
				multiplayer.lines.push_back(
				    fmt::format("{} ({}/{}) {}", server.hostname, server.numplayers, server.maxplayers, server.gamemode));
			}
		}
		else if (frontEnd != nullptr)
		{
			const auto& chat = frontEnd->GetChat();
			for (size_t i = chat.size() > k_ShownChatLines ? chat.size() - k_ShownChatLines : 0; i < chat.size(); ++i)
			{
				multiplayer.lines.push_back(gui::ToUtf8(chat[i].text));
			}
		}
		windows.push_back(std::move(multiplayer));
	}
	auto world = WorldGui::Windows();
	windows.insert(windows.end(), std::make_move_iterator(world.begin()), std::make_move_iterator(world.end()));
	return windows;
}

std::string GameGui::Open(std::string_view window)
{
	if (Lower(window) == k_Menu)
	{
		auto* game = Game::Instance();
		if (game == nullptr || game->GetInterface() == nullptr)
		{
			return "the game's menu isn't made yet";
		}
		// Opened as the player opens it, with the Escape key. When the game does not open the menu on Escape now, the
		// inspector sends no key and gives the reason.
		if (!game->GetInterface()->GetMenu().IsOpen())
		{
			if (auto why = WhyEscapeKeepsMenuShut(*game, *game->GetInterface()); !why.empty())
			{
				return "the game does not open its menu now: " + why;
			}
			const std::array escape {InputEvent {.kind = InputEvent::Kind::KeyDown, .key = "Escape"},
			                         InputEvent {.kind = InputEvent::Kind::KeyUp, .key = "Escape"}};
			return Send(escape);
		}
		return {};
	}
	return WorldGui::Open(window);
}

std::string GameGui::Close(std::string_view window)
{
	if (Lower(window) == k_Menu)
	{
		auto* game = Game::Instance();
		if (game != nullptr && game->GetInterface() != nullptr && game->GetInterface()->GetMenu().IsOpen())
		{
			const std::array escape {InputEvent {.kind = InputEvent::Kind::KeyDown, .key = "Escape"},
			                         InputEvent {.kind = InputEvent::Kind::KeyUp, .key = "Escape"}};
			return Send(escape);
		}
		return {};
	}
	if (Lower(window) == k_SkipBox)
	{
		// The player closes the question only by answering it: OK is pressed, as a click on it, keeping the answer picked
		auto* game = Game::Instance();
		if (game == nullptr || game->GetInterface() == nullptr || !game->GetInterface()->GetSkipBox().IsActive())
		{
			return {};
		}
		const auto& interface = *game->GetInterface();
		const auto ok = SkipBoxOk(interface.GetSkipBox());
		if (!ok.has_value())
		{
			return "the start-of-game question has no OK button";
		}
		return Send(ClickAt(DialogPointOnScreen(*ok)));
	}
	return WorldGui::Close(window);
}

OverlayInfo GameGui::Overlays() const
{
	auto info = WorldGui::Overlays();
	if (!Locator::entitiesRegistry::has_value())
	{
		return info;
	}
	if (auto* game = Game::Instance(); game != nullptr)
	{
		info.nameFade = game->GetVillagerNameFade();
		if (const auto* face = game->GetInterface(); face != nullptr)
		{
			info.labels = face->GetVillagerLabels().size();
		}
	}
	return info;
}

std::string GameGui::Press(std::string_view window, const std::vector<ButtonPathStep>& path)
{
	const auto lower = Lower(window);
	if (lower == k_IncomingMessage)
	{
		auto* game = Game::Instance();
		if (game == nullptr || game->GetInterface() == nullptr)
		{
			return "the game's dialogs aren't made yet";
		}
		auto& interface = *game->GetInterface();
		if (!interface.GetIncomingMessages().IsActive())
		{
			return "no incoming message shows now (the list is empty, or a dialog, the temple, a video or a cut scene "
			       "hides it)";
		}
		const std::vector<gui::Dialog::NamedControl> named {
		    {.name = u"message", .kind = "button", .rect = gui::incoming_message_strip::ButtonRect()}};
		const auto found = FindDialogControl(DialogControls(named), path);
		if (const auto* why = std::get_if<std::string>(&found); why != nullptr)
		{
			return "the incoming message: " + *why + "; its one button is message";
		}
		const auto size = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(800, 600);
		return Send(ClickAt(interface.FromIncomingMessages(std::get<glm::ivec2>(found), static_cast<glm::u16vec2>(size))));
	}
	if (lower == k_Chat)
	{
		auto* game = Game::Instance();
		if (game == nullptr || game->GetInterface() == nullptr)
		{
			return "the game's dialogs aren't made yet";
		}
		auto& interface = *game->GetInterface();
		if (!interface.GetChatControls().IsActive())
		{
			return "the chat doesn't show now (a game of one player with the chat shut, a dialog, a video or the "
			       "creature in the hand hides it)";
		}
		const auto controls = DialogControls(interface.GetChatControls().GetNamedControls());
		const auto found = FindDialogControl(controls, path);
		if (const auto* why = std::get_if<std::string>(&found); why != nullptr)
		{
			return "the chat: " + *why + "; gui.windows lists its buttons";
		}
		const auto size = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(800, 600);
		return Send(ClickAt(interface.FromChatControls(std::get<glm::ivec2>(found), static_cast<glm::u16vec2>(size))));
	}
	if (lower == k_MusicPlayer)
	{
		auto* game = Game::Instance();
		if (game == nullptr || game->GetInterface() == nullptr)
		{
			return "the game's dialogs aren't made yet";
		}
		auto& interface = *game->GetInterface();
		if (!interface.GetMusicControls().IsActive())
		{
			return "the custom music controls don't show now (a dialog, the temple, a video or a cut scene hides them)";
		}
		const auto controls = DialogControls(interface.GetMusicControls().GetNamedControls());
		const auto found = FindDialogControl(controls, path);
		if (const auto* why = std::get_if<std::string>(&found); why != nullptr)
		{
			return "the custom music controls: " + *why + "; gui.windows lists them";
		}
		const auto size = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(800, 600);
		return Send(ClickAt(interface.FromMusicControls(std::get<glm::ivec2>(found), static_cast<glm::u16vec2>(size))));
	}
	if (lower == k_EndBox)
	{
		auto* game = Game::Instance();
		if (game == nullptr || game->GetInterface() == nullptr || !game->GetInterface()->GetEndGameBox().IsOpen())
		{
			return "the end box isn't open: it opens when a skirmish or a network game ends for this player";
		}
		const auto controls = DialogControls(game->GetInterface()->GetEndGameBox().GetNamedControls());
		const auto found = FindDialogControl(controls, path);
		if (const auto* why = std::get_if<std::string>(&found); why != nullptr)
		{
			return "the end box: " + *why + "; gui.windows lists them";
		}
		return Send(ClickAt(DialogPointOnScreen(std::get<glm::ivec2>(found))));
	}
	if (lower == k_SkirmishBox)
	{
		auto* game = Game::Instance();
		if (game == nullptr || game->GetInterface() == nullptr || !game->GetInterface()->GetSkirmishBox().IsActive())
		{
			return "the skirmish box isn't open: Start Skirmish Game of the menu opens it";
		}
		const auto controls = DialogControls(game->GetInterface()->GetSkirmishBox().GetNamedControls());
		const auto found = FindDialogControl(controls, path);
		if (const auto* why = std::get_if<std::string>(&found); why != nullptr)
		{
			return "the skirmish box: " + *why + "; gui.windows lists them";
		}
		return Send(ClickAt(DialogPointOnScreen(std::get<glm::ivec2>(found))));
	}
	if (lower == k_LandInfoBox)
	{
		auto* game = Game::Instance();
		if (game == nullptr || game->GetInterface() == nullptr || !game->GetInterface()->GetLandInfoBox().IsActive())
		{
			return "the land information box isn't open: the script of a playground land opens it as the land loads";
		}
		const auto controls = DialogControls(game->GetInterface()->GetLandInfoBox().GetNamedControls());
		const auto found = FindDialogControl(controls, path);
		if (const auto* why = std::get_if<std::string>(&found); why != nullptr)
		{
			return "the land information box: " + *why + "; gui.windows lists them";
		}
		return Send(ClickAt(DialogPointOnScreen(std::get<glm::ivec2>(found))));
	}
	if (lower == k_Multiplayer)
	{
		auto* game = Game::Instance();
		if (game == nullptr || game->GetInterface() == nullptr || !game->GetInterface()->GetMultiplayerBoxes().IsActive())
		{
			return "the multiplayer front end isn't open: Join Online Game opens it";
		}
		const auto controls = DialogControls(game->GetInterface()->GetMultiplayerBoxes().GetNamedControls());
		const auto found = FindDialogControl(controls, path);
		if (const auto* why = std::get_if<std::string>(&found); why != nullptr)
		{
			return "the multiplayer front end: " + *why + "; gui.windows lists them";
		}
		return Send(ClickAt(DialogPointOnScreen(std::get<glm::ivec2>(found))));
	}
	if (lower == k_Menu || lower == k_SkipBox || lower == k_TattooEditor)
	{
		auto* game = Game::Instance();
		if (game == nullptr || game->GetInterface() == nullptr)
		{
			return "the game's dialogs aren't made yet";
		}
		auto& interface = *game->GetInterface();
		std::vector<DialogControl> controls;
		std::string where;
		if (lower == k_Menu)
		{
			const auto& menu = interface.GetMenu();
			if (!menu.IsOpen())
			{
				return "the game's menu isn't open: gui.open it first";
			}
			if (auto why = WhyMenuMissesClicks(interface); !why.empty())
			{
				return "the click does not get to the menu: " + why;
			}
			controls = DialogControls(menu.GetNamedControls());
			where = "the menu's " + std::string(menu.IsAskingToQuit() ? "question" : PageName(menu.GetPage()));
		}
		else if (lower == k_SkipBox)
		{
			if (!interface.GetSkipBox().IsActive())
			{
				return "the start-of-game question isn't up: it comes as a new game starts (level.new_game with skip "
				       "ask)";
			}
			controls = SkipBoxControls(interface.GetSkipBox());
			where = "the start-of-game question";
		}
		else
		{
			if (!interface.GetTattooEditor().IsOpen())
			{
				return "the tattoo editor isn't open";
			}
			controls = DialogControls(interface.GetTattooEditor().GetNamedControls());
			where = "the tattoo editor";
		}
		// Clicked in the middle of the control, through the game's input as the player clicks it
		const auto found = FindDialogControl(controls, path);
		if (const auto* why = std::get_if<std::string>(&found); why != nullptr)
		{
			return where + ": " + *why + "; gui.windows lists them";
		}
		return Send(ClickAt(DialogPointOnScreen(std::get<glm::ivec2>(found))));
	}
	return WorldGui::Press(window, path);
}
