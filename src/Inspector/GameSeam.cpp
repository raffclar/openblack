/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameSeam.h"

#include <array>
#include <string>
#include <utility>

#include <bgfx/bgfx.h>

#include "Game.h"
#include "GameGui.h"
#include "Gui/GameInterface.h"
#include "Gui/TextDatabase.h"

namespace openblack::inspector
{
namespace
{

/// The running game, which can be absent while the program starts and stops
class GameSeam final: public GameSeamInterface
{
public:
	[[nodiscard]] std::unique_ptr<WorldGui> MakeGui(InputTargetInterface& input) override
	{
		return std::make_unique<GameGui>(input);
	}

	bool LoadLand(const std::filesystem::path& path, bool story) override
	{
		auto* game = Game::Instance();
		if (game == nullptr)
		{
			return false;
		}
		return story ? game->LoadMap(path) : game->LoadFromLandMenu(path);
	}

	bool StartNewGame(std::optional<new_game_choice::NewGameStart> start) override
	{
		auto* game = Game::Instance();
		return game != nullptr && game->StartNewGame(start);
	}

	void RequestScreenshot(const std::filesystem::path& path, bool hideDebugGui) override
	{
		if (auto* game = Game::Instance(); game != nullptr)
		{
			game->RequestScreenshot(path, hideDebugGui);
		}
	}

	void HideDebugGuiThisFrame() override
	{
		if (auto* game = Game::Instance(); game != nullptr)
		{
			game->HideDebugGuiThisFrame();
		}
	}

	[[nodiscard]] std::optional<std::filesystem::path> GetScreenshotRoot() const override
	{
		const auto* game = Game::Instance();
		return game != nullptr ? game->GetScreenshotRoot() : std::nullopt;
	}

	[[nodiscard]] std::string GetRendererBackend() const override
	{
		switch (bgfx::getRendererType())
		{
		case bgfx::RendererType::Vulkan:
			return "vulkan";
		case bgfx::RendererType::Direct3D12:
			return "d3d12";
		case bgfx::RendererType::Direct3D11:
			return "d3d11";
		case bgfx::RendererType::OpenGL:
			return "opengl";
		case bgfx::RendererType::Metal:
			return "metal";
		default:
			return {};
		}
	}

	bool RequestScenario(std::string_view id) override
	{
		auto* game = Game::Instance();
		return game != nullptr && game->RequestScenario({.id = std::string(id), .benchmark = false});
	}

	[[nodiscard]] std::optional<std::string> GetPendingScenario() const override
	{
		const auto* game = Game::Instance();
		return game != nullptr && game->PendingScenario().has_value() ? std::optional(game->PendingScenario()->id)
		                                                              : std::nullopt;
	}

	void AddIncomingMessages(Json& result) const override
	{
		auto* game = Game::Instance();
		if (game == nullptr || game->GetInterface() == nullptr)
		{
			return;
		}
		const auto& strip = game->GetInterface()->GetIncomingMessages();
		result["shown"] = strip.IsActive();
		if (const auto& look = strip.GetLook(); look.has_value())
		{
			constexpr std::array<std::string_view, 5> k_Looks {"speech", "speech_arrow", "envelope", "envelope_arrow",
			                                                   "weather"};
			result["look"] = k_Looks.at(static_cast<size_t>(look->look));
			result["look_picture"] = look->picture;
		}
		result["line"] = gui::ToUtf8(strip.GetText());
	}

	void AddChatControls(Json& result) const override
	{
		auto* game = Game::Instance();
		if (game == nullptr || game->GetInterface() == nullptr)
		{
			return;
		}
		const auto& controls = game->GetInterface()->GetChatControls();
		Json players = Json::array();
		for (const auto& person : controls.GetPeople().players)
		{
			players.push_back(
			    {{"slot", static_cast<int>(person.slot)}, {"name", gui::ToUtf8(person.name)}, {"selected", person.selected}});
		}
		Json recipients = Json::array();
		for (const auto player : controls.Recipients())
		{
			recipients.push_back(static_cast<int>(player));
		}
		result["controls"] = {{"shown", controls.IsActive()},
		                      {"open", controls.IsOpen()},
		                      {"quick_chat", controls.IsQuickChatOpen()},
		                      {"line", gui::ToUtf8(controls.GetLine())},
		                      {"expanded", controls.GetPeople().expanded},
		                      {"messages_on", controls.GetPeople().messagesOn},
		                      {"players", std::move(players)},
		                      {"recipients", std::move(recipients)}};
	}
};

} // namespace

std::unique_ptr<GameSeamInterface> MakeGameSeam()
{
	return std::make_unique<GameSeam>();
}

} // namespace openblack::inspector
