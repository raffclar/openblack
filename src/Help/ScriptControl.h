/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <string_view>

// Which script task has the dialogue, the wide screen and the camera: the script side of CHL 030 START_CAMERA_CONTROL,
// 031 END_CAMERA_CONTROL, 032 SET_WIDESCREEN, 120 START_DIALOGUE, 121 END_DIALOGUE and 122 IS_DIALOGUE_READY, and the
// part of the task-stop callback that gives them back. The HelpSystem side is in HelpSystem.h.

namespace openblack::audio
{
struct ScriptAudioState;
}

namespace openblack::help
{
class HelpSystem;
}

namespace openblack::help::script_control
{

/// The task's script type (lhvm::ScriptType): Script
constexpr uint32_t k_ScriptTypeScript = 1;
/// Script type Help
constexpr uint32_t k_ScriptTypeHelp = 2;
/// Inside the citadel only TempleHelp | TempleSpecial tasks get the camera
constexpr uint32_t k_CitadelCameraTypes = 0x18;
/// The help scripts stopped for a new help: Help | TempleHelp | MultiplayerHelp
constexpr uint32_t k_HelpScriptTypes = 0x4A;
/// The field of view set when the script camera is released: 1.2217305 rad (70 degrees) over 0.5 s
constexpr float k_ScriptEndFov = 1.2217305f;
constexpr float k_ScriptEndFovTime = 0.5f;

/// What these functions ask the script VM. Unset: task 0 and type 1.
struct Vm
{
	/// The task running now (0 outside a task)
	std::function<uint32_t()> taskNumber;
	/// The current task's script type (1 outside a task)
	std::function<uint32_t()> currentTaskType;
	/// A task's script type (1 for a task that does not exist)
	std::function<uint32_t(uint32_t task)> taskType;
	/// Stops every task whose type is in the mask
	std::function<void(uint32_t typeMask)> stopTasksOfType;
	/// Pushes a float on the VM stack
	std::function<void(float value)> pushFloat;
	/// Starts a script (name, script type mask): the parameters come from the stack
	std::function<void(std::string_view name, uint32_t typeMask)> startScript;
	/// The game is a multiplayer one. Unset: false.
	std::function<bool()> multiplayer;
};

/// The types a started script may have, 0x7F in a single-player game, 0x60 in a multiplayer one
constexpr uint32_t k_SinglePlayerScriptTypes = 0x7F;
constexpr uint32_t k_MultiplayerScriptTypes = 0x60;
/// A task that has the dialogue and whose type has neither Help (2) nor MultiplayerHelp (0x40) keeps it: false; else
/// the help scripts (the types 0x4A) are stopped and true
bool StopHelpScriptsForNewHelp(const HelpSystem& help, const Vm& vm);
/// Used by the bubble properties: a dialogue owner whose type is Help (2) -> the help scripts are stopped; then, with
/// no dialogue owner left, the script is started (true); else nothing (false)
bool StartScriptIfNoDialogue(const HelpSystem& help, std::string_view name, const Vm& vm);
/// false for first > last or when StopHelpScriptsForNewHelp refuses; else the message turn is set, first and last are
/// pushed as floats and the script started (the guidance spirits' "MultiHelpJustTalkWithText", whose two parameters
/// WhichTextFirst / WhichTextLast are these); true
bool RunMessage(HelpSystem& help, uint32_t first, uint32_t last, std::string_view script, const Vm& vm, uint32_t turn);

/// The script camera state. A script reset sets drawHighlight = drawLeash = 1 and field7C = 0; it does not write owner
/// (0 from the constructor: inferred)
struct CameraControl
{
	/// The task that has the camera (0 once released)
	uint32_t owner {0};
	/// The leashes are drawn: CHLApi SetDrawLeash (SET_DRAW_LEASH) and START/END_CAMERA_CONTROL write it, the leash
	/// draw reads it
	int32_t drawLeash {1};
	/// Only the camera release (= 0) and the script reset write it; no reader found
	int32_t field7C {0};
	/// The highlights are drawn (SET_DRAW_HIGHLIGHT; a did-you-know is drawn anyway): CHLApi SetDrawHighlight writes it,
	/// ecs::script_highlight::UpdateFrame reads it
	int32_t drawHighlight {1};
	/// The task that controls the game speed (StartGameSpeed; 0 in RestoreGameSpeed). The script reset does not write
	/// it (0 from the constructor: inferred); a stopped task gives it back. (pending) its save and load with the script
	/// state
	uint32_t gameSpeedOwner {0};

	/// The camera part of the script reset
	void Reset();
};

/// The script camera state of the running game (one)
CameraControl& GetCameraControl();

/// CHL 120 START_DIALOGUE: the bool it pushes
bool StartDialogue(HelpSystem& help, const Vm& vm);
/// CHL 121 END_DIALOGUE
void EndDialogue(HelpSystem& help, audio::ScriptAudioState& audio, const Vm& vm);
/// CHL 122 IS_DIALOGUE_READY: !HelpSystem::IsDialogueControlled
[[nodiscard]] bool IsSpiritReady(const HelpSystem& help);
/// CHL 032 SET_WIDESCREEN, after the pop (`on` is the popped value as it is): true when it called
/// HelpSystem::SetWideScreen
bool SetWideScreen(HelpSystem& help, int32_t on, const Vm& vm);

/// CHL 030 START_CAMERA_CONTROL: the bool it pushes. `insideCitadel`: the player is in the citadel; `cameraTaken`: the
/// script camera mode was created (it is not when the current camera mode cannot be left)
bool StartCameraControl(CameraControl& camera, const Vm& vm, bool insideCitadel, bool cameraTaken);
/// CHL 031 END_CAMERA_CONTROL: ReleaseCameraControl when this task has the camera. True when it released it
bool EndCameraControl(CameraControl& camera, audio::ScriptAudioState& audio, const Vm& vm);
/// Gives the camera back
void ReleaseCameraControl(CameraControl& camera, audio::ScriptAudioState& audio);
/// From the task-stop callback: ReleaseCameraControl when the task has the camera
bool ReleaseCameraOf(CameraControl& camera, audio::ScriptAudioState& audio, uint32_t task);

/// CHL 128 START_GAME_SPEED: when nobody or this task controls the game speed, the task takes it (the original also
/// turns on a script slow-motion flag of the camera that nothing else reads; not ported)
void StartGameSpeed(CameraControl& camera, const Vm& vm);
/// CHL 129 END_GAME_SPEED: RestoreGameSpeed when nobody or this task controls it
void EndGameSpeed(CameraControl& camera, const Vm& vm);
/// CHL 066 SET_GAMESPEED, after the pop: sets the game speed only for the task that controls it (an owner of 0 matches
/// no running task)
void SetGameSpeed(const CameraControl& camera, const Vm& vm, float speed);
/// From the task-stop callback: RestoreGameSpeed when the task controls the game speed
void ReleaseGameSpeedOf(CameraControl& camera, uint32_t task);

/// The task-stop callback, for the parts ported: nothing without a game or HelpSystem; else the dialogue, the wide
/// screen (ReleaseWideScreenOf), the camera and the game speed are released and the hand demo playback of the task
/// ends (Input/HandDemo.h). The VM calls it before the task leaves the list, so the task's script type is still known
/// while releasing the dialogue, as LHVM::StopTask does (InvokeStopTaskCallback before the erase)
void OnTaskStopped(uint32_t task, HelpSystem* help, CameraControl& camera, audio::ScriptAudioState& audio);

} // namespace openblack::help::script_control
