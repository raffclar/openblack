/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <InspectorJson.h>

#include "Story/NewGameChoice.h"

namespace openblack::inspector
{

class InputTargetInterface;
class WorldGui;

/// The parts of the game on the player's computer that the inspector controls: the dialogs of its interface, the
/// loading of lands, the pictures of the screen and the testbed's scenarios. The dedicated server has no such parts,
/// and its inspector answers as a game that has not made them yet.
class GameSeamInterface
{
public:
	GameSeamInterface() = default;
	GameSeamInterface(const GameSeamInterface&) = delete;
	GameSeamInterface& operator=(const GameSeamInterface&) = delete;
	GameSeamInterface(GameSeamInterface&&) = delete;
	GameSeamInterface& operator=(GameSeamInterface&&) = delete;
	virtual ~GameSeamInterface() = default;

	/// The windows with the game's dialogs, which are clicked through the input given
	[[nodiscard]] virtual std::unique_ptr<WorldGui> MakeGui(InputTargetInterface& input) = 0;

	/// Loads a land as the story loads it, or else as the land menu loads it. False when the land does not load.
	virtual bool LoadLand(const std::filesystem::path& path, bool story) = 0;
	/// Starts a new game, in the way given or else as the game asks. False when the game does not start.
	virtual bool StartNewGame(std::optional<new_game_choice::NewGameStart> start) = 0;

	/// Asks for a picture of the next frame drawn
	virtual void RequestScreenshot(const std::filesystem::path& path, bool hideDebugGui) = 0;
	/// Hides the debug windows for the frame drawn now
	virtual void HideDebugGuiThisFrame() = 0;
	/// The folder of the pictures that the player gave the game, if any
	[[nodiscard]] virtual std::optional<std::filesystem::path> GetScreenshotRoot() const = 0;
	/// The name of the graphics back end that draws the game, or empty for an other back end
	[[nodiscard]] virtual std::string GetRendererBackend() const = 0;

	/// Asks the game to load a testbed scenario. False when the game does not take the request.
	virtual bool RequestScenario(std::string_view id) = 0;
	/// The scenario that the game loads now, if any
	[[nodiscard]] virtual std::optional<std::string> GetPendingScenario() const = 0;

	/// Adds the state of the incoming message strip to the answer of a query
	virtual void AddIncomingMessages(Json& result) const = 0;
	/// Adds the state of the chat controls to the answer of a query
	virtual void AddChatControls(Json& result) const = 0;
	/// Adds the tooltip that the hand shows to the answer of a query
	virtual void AddToolTip(Json& result) const = 0;
};

} // namespace openblack::inspector
