/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "Camera/Camera.h"
#include "CameraControl.h"
#include "GuiControl.h"
#include "InputControl.h"
#include "LevelControl.h"
#include "ScriptControl.h"

namespace openblack::inspector
{

class GameSeamInterface;

/// The names of the windows of the game, as gui.windows gives them
namespace game_windows
{
constexpr std::string_view k_Menu = "menu";
constexpr std::string_view k_Cave = "cave";
constexpr std::string_view k_SkipBox = "skip_box";
constexpr std::string_view k_EndBox = "end_box";
constexpr std::string_view k_TattooEditor = "tattoo_editor";
constexpr std::string_view k_MusicPlayer = "music_player";
/// The incoming message at the bottom left of the screen; its button is "message"
constexpr std::string_view k_IncomingMessage = "incoming_message";
constexpr std::string_view k_Multiplayer = "multiplayer";
constexpr std::string_view k_SkirmishBox = "skirmish_box";
constexpr std::string_view k_LandInfoBox = "land_info_box";
/// The chat of a network game at the top right of the screen
constexpr std::string_view k_Chat = "chat";

/// A name in lower case, so that names of any case are the same
[[nodiscard]] std::string Lower(std::string_view text);
} // namespace game_windows

/// The camera through the game: placed as the scripts place it, flown as the bookmarks fly it
class GameCamera final: public CameraControlInterface
{
public:
	[[nodiscard]] std::optional<CameraState> State() const override;
	std::string Set(const CameraPose& pose) override;
	std::string Fly(const CameraPose& pose) override;
	std::string Pin(const CameraPose& pose) override;
	void Unpin() override;
	[[nodiscard]] bool Pinned() const override { return _own.has_value(); }
	/// The sounds are heard from the view shown while it is shown, and from the camera again once it is let go
	void SetOverride(std::optional<CameraPose> pose) override;
	[[nodiscard]] std::optional<CameraPose> Override() const override { return _override; }
	[[nodiscard]] float GroundHeight(glm::vec2 point) const override;
	[[nodiscard]] std::optional<glm::vec3> EntityPosition(uint32_t id) const override;
	[[nodiscard]] std::optional<CameraKeepers> Keepers(const CameraPose& pose) const override;

private:
	/// The camera's own state while it is shown elsewhere for a frame
	std::optional<Camera::Motion> _own;
	std::optional<CameraPose> _override;
};

/// The debug windows and the Creature Cave, which the world has with no interface of the game. The game's dialogs are
/// windows too, which answer as not made yet. The game on the player's computer adds its dialogs (see GameGui).
class WorldGui: public GuiTargetInterface
{
public:
	/// Makes input for the game's dialogs: in order, behind any input still to come; empty when made, else why not
	using InputSender = std::function<std::string(std::span<const InputEvent>)>;

	explicit WorldGui(InputTargetInterface& input);

	[[nodiscard]] std::vector<WindowInfo> Windows() const override;
	std::string Open(std::string_view window) override;
	std::string Close(std::string_view window) override;
	std::string Press(std::string_view window, const std::vector<ButtonPathStep>& path) override;
	[[nodiscard]] OverlayInfo Overlays() const override;
	std::string SetOverlays(std::optional<bool> names, std::optional<bool> details) override;

	/// The input's queue, so that what the dialogs are sent waits behind input asked for before; without one, it is
	/// made at once
	void SetInputSender(InputSender send) { _send = std::move(send); }

protected:
	std::string Send(std::span<const InputEvent> events);

private:
	InputTargetInterface& _input;
	InputSender _send;
};

/// The land's scripts through the game's script machine
class GameScripts final: public ScriptTargetInterface
{
public:
	[[nodiscard]] bool Loaded() const override;
	[[nodiscard]] std::vector<ScriptInfo> Scripts() const override;
	std::variant<uint32_t, std::string> Run(std::string_view name, const std::vector<ScriptValue>& args) override;
	std::string StopTask(uint32_t task) override;
	[[nodiscard]] std::vector<ScriptGlobal> Globals() const override;
	std::string SetGlobal(std::string_view name, const ScriptValue& value) override;
	[[nodiscard]] std::vector<ScriptNative> Natives() const override;
	std::variant<std::vector<ScriptValue>, std::string> CallNative(uint32_t id, const std::vector<ScriptValue>& args) override;
};

/// The lands, loaded through the game's land menu's loading or the story's change of land
class GameLevels final: public LevelTargetInterface
{
public:
	/// With no game, a land does not load and a new game does not start
	explicit GameLevels(GameSeamInterface* seam)
	    : _seam(seam)
	{
	}

	[[nodiscard]] std::vector<LevelInfo> Levels() const override;
	std::string Load(std::string_view name, LoadHow how) override;
	std::string LoadTestbed() override;
	std::string NewGame(std::string_view start) override;
	[[nodiscard]] std::string Current() const override;
	[[nodiscard]] bool IsSkirmish() const override;

private:
	/// The name of the land last found, with the land's path and the count of levels it was found for. The inspector
	/// asks for the name every frame, and the search tests the files of all the levels.
	struct FoundName
	{
		std::filesystem::path land;
		size_t levels;
		std::string name;
	};
	GameSeamInterface* _seam;
	mutable std::optional<FoundName> _found;
};

/// Pictures of the screen through the game's own screenshot
class GameScreenshots final: public ScreenshotTargetInterface
{
public:
	/// With no game, no picture is taken
	explicit GameScreenshots(GameSeamInterface* seam)
	    : _seam(seam)
	{
	}

	std::string Capture(const std::filesystem::path& path, bool hideDebugGui) override;
	[[nodiscard]] std::optional<std::string> CaptureFailure(const std::filesystem::path& path) override;
	void HideDebugGui() override;
	[[nodiscard]] std::filesystem::path Directory() const override;
	[[nodiscard]] std::optional<std::filesystem::path> Root() const override;
	[[nodiscard]] ShotSource Source() const override;
	[[nodiscard]] bool Exists(const std::filesystem::path& path) const override;
	std::string Remove(const std::filesystem::path& path) override;
	std::string AppendLine(const std::filesystem::path& file, std::string_view line) override;

private:
	GameSeamInterface* _seam;
};

/// Everything the inspector controls the game through, owned by the inspector's system
struct GameControls
{
	CameraControlInterface& camera;
	GuiTargetInterface& gui;
	ScriptTargetInterface& scripts;
	LevelTargetInterface& levels;
	/// The parts of the game on the player's computer, or null with none
	GameSeamInterface* seam;
};

/// The game's controls, owned together. With no seam, as on the dedicated server, the controls answer as a game that
/// has no interface, no land loading and no pictures.
struct GameControlSet
{
	GameControlSet(InputTargetInterface& input, GameSeamInterface* seam);
	GameControlSet(const GameControlSet&) = delete;
	GameControlSet& operator=(const GameControlSet&) = delete;
	GameControlSet(GameControlSet&&) = delete;
	GameControlSet& operator=(GameControlSet&&) = delete;
	~GameControlSet();

	[[nodiscard]] GameControls View()
	{
		return {.camera = camera, .gui = *gui, .scripts = scripts, .levels = levels, .seam = seam};
	}

	GameSeamInterface* seam;
	GameCamera camera;
	std::unique_ptr<WorldGui> gui;
	GameScripts scripts;
	GameLevels levels;
	GameScreenshots screenshots;
};

} // namespace openblack::inspector
