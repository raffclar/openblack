/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "Camera/Camera.h"
#include "CameraControl.h"
#include "GuiControl.h"
#include "InputControl.h"
#include "LevelControl.h"
#include "ScriptControl.h"

namespace openblack::inspector
{

/// The camera through the game: placed as the scripts place it, flown as the bookmarks fly it
class GameCamera final: public CameraControlInterface
{
public:
	[[nodiscard]] std::optional<CameraState> State() const override;
	std::string Set(const CameraPose& pose) override;
	std::string Fly(const CameraPose& pose) override;
	std::string Pin(const CameraPose& pose) override;
	void Unpin() override;
	void SetOverride(std::optional<CameraPose> pose) override { _override = pose; }
	[[nodiscard]] std::optional<CameraPose> Override() const override { return _override; }
	[[nodiscard]] float GroundHeight(glm::vec2 point) const override;
	[[nodiscard]] std::optional<glm::vec3> EntityPosition(uint32_t id) const override;

private:
	/// The camera's own state while it is shown elsewhere for a frame
	std::optional<Camera::Motion> _own;
	std::optional<CameraPose> _override;
};

/// The debug windows, and the game's own menu and Creature Cave. The menu's buttons are clicked through the game's
/// input, as the player clicks them.
class GameGui final: public GuiTargetInterface
{
public:
	explicit GameGui(InputTargetInterface& input);

	[[nodiscard]] std::vector<WindowInfo> Windows() const override;
	std::string Open(std::string_view window) override;
	std::string Close(std::string_view window) override;
	std::string Press(std::string_view window, const std::vector<ButtonPathStep>& path) override;

private:
	InputTargetInterface& _input;
};

/// The land's scripts through the game's script machine
class GameScripts final: public ScriptTargetInterface
{
public:
	[[nodiscard]] bool Loaded() const override;
	[[nodiscard]] std::vector<ScriptInfo> Scripts() const override;
	std::variant<uint32_t, std::string> Run(std::string_view name) override;
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
	[[nodiscard]] std::vector<LevelInfo> Levels() const override;
	std::string Load(std::string_view name, LoadHow how) override;
	std::string LoadTestbed() override;
	std::string NewGame(std::string_view start) override;
	[[nodiscard]] std::string Current() const override;
};

/// Pictures of the screen through the game's own screenshot
class GameScreenshots final: public ScreenshotTargetInterface
{
public:
	std::string Capture(const std::filesystem::path& path, bool hideDebugGui) override;
	void HideDebugGui() override;
	[[nodiscard]] std::filesystem::path Directory() const override;
	[[nodiscard]] std::optional<std::filesystem::path> Root() const override;
	[[nodiscard]] ShotSource Source() const override;
	[[nodiscard]] bool Exists(const std::filesystem::path& path) const override;
	std::string AppendLine(const std::filesystem::path& file, std::string_view line) override;
};

/// Everything the inspector controls the game through, owned by the inspector's system
struct GameControls
{
	CameraControlInterface& camera;
	GuiTargetInterface& gui;
	ScriptTargetInterface& scripts;
	LevelTargetInterface& levels;
};

/// The game's controls, owned together
struct GameControlSet
{
	explicit GameControlSet(InputTargetInterface& input)
	    : gui(input)
	{
	}
	[[nodiscard]] GameControls View() { return {.camera = camera, .gui = gui, .scripts = scripts, .levels = levels}; }

	GameCamera camera;
	GameGui gui;
	GameScripts scripts;
	GameLevels levels;
	GameScreenshots screenshots;
};

} // namespace openblack::inspector
