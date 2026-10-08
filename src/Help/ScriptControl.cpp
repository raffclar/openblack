/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptControl.h"

#include <cstdlib>

#include <string_view>

#include <spdlog/spdlog.h>

#include "Audio/Services/ScriptAudioState.h"
#include "Debug/DebugEnv.h"
#include "ECS/SuperVillager.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "GameClock.h"
#include "HelpSystem.h"
#include "Input/HandDemo.h"
#include "Locator.h"

namespace openblack::help::script_control
{

namespace
{
bool Tracing()
{
	static const bool k_Trace = debug_env::TextTrace();
	return k_Trace && spdlog::get("game") != nullptr;
}

/// A script error or warning: a debug message, the script goes on
void ScriptMessage(std::string_view message)
{
	if (auto logger = spdlog::get("scripting"))
	{
		SPDLOG_LOGGER_WARN(logger, "{}", message);
	}
}

uint32_t TaskNumber(const Vm& vm)
{
	return vm.taskNumber ? vm.taskNumber() : 0;
}

uint32_t CurrentTaskType(const Vm& vm)
{
	return vm.currentTaskType ? vm.currentTaskType() : k_ScriptTypeScript;
}

uint32_t TaskType(const Vm& vm, uint32_t task)
{
	return vm.taskType ? vm.taskType(task) : k_ScriptTypeScript;
}
} // namespace

void CameraControl::Reset()
{
	drawHighlight = 1;
	drawLeash = 1;
	field7C = 0;
}

CameraControl& GetCameraControl()
{
	return openblack::Locator::scriptState::value().Get<CameraControl>();
}

bool StartDialogue(HelpSystem& help, const Vm& vm)
{
	auto owner = help.GetDialogueOwner();
	const auto task = TaskNumber(vm);
	if (owner == 0)
	{
		// Both advisors go home, then the request
		help.SpiritHome(1, 0);
		help.SpiritHome(2, 0);
	}
	else
	{
		if (owner == task)
		{
			ScriptMessage("Script Asking For Dialogue Control It already has! - Dangerous");
			return true;
		}
		// A Script task takes the dialogue from a Help one: stop the help scripts (their task-stop callbacks give it
		// back) and read the owner again
		if (TaskType(vm, owner) == k_ScriptTypeHelp && CurrentTaskType(vm) == k_ScriptTypeScript)
		{
			if (vm.stopTasksOfType)
			{
				vm.stopTasksOfType(k_HelpScriptTypes);
			}
			owner = help.GetDialogueOwner();
		}
		if (owner != 0)
		{
			if (Tracing())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Script: START_DIALOGUE task {}: task {} has it", task, owner);
			}
			return false;
		}
	}
	// The result of DialogueControlRequest is not used, START_DIALOGUE pushes true even when the wide screen of another
	// task refuses it
	const bool granted = help.DialogueControlRequest(task);
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Script: START_DIALOGUE task {} (control {})", task, granted);
	}
	return true;
}

void EndDialogue(HelpSystem& help, audio::ScriptAudioState& audio, const Vm& vm)
{
	if (help.GetDialogueOwner() != TaskNumber(vm))
	{
		return;
	}
	// Both advisors go home; the second argument says whether the task is a Help script
	const int32_t helpScript = CurrentTaskType(vm) == k_ScriptTypeHelp ? 1 : 0;
	help.SpiritHome(1, helpScript);
	help.SpiritHome(2, helpScript);
	const auto task = TaskNumber(vm);
	help.ReleaseDialogueControl(task);
	audio.EndDialogue(); // creature sound back on, music beat 0
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Script: END_DIALOGUE task {}", task);
	}
}

bool IsSpiritReady(const HelpSystem& help)
{
	return !help.IsDialogueControlled();
}

bool SetWideScreen(HelpSystem& help, int32_t on, const Vm& vm)
{
	const auto owner = help.GetWideScreenOwner();
	const auto task = TaskNumber(vm);
	if (on != 0 && owner == task)
	{
		ScriptMessage("Script asking for Widescreen it has control of! Bad");
	}
	if (owner != 0 && owner != task) // another task holds it
	{
		return false;
	}
	help.SetWideScreen(on, TaskNumber(vm));
	return true;
}

bool StartCameraControl(CameraControl& camera, const Vm& vm, bool insideCitadel, bool cameraTaken)
{
	bool result = false;
	if (insideCitadel)
	{
		// Only temple scripts get it there, without a camera mode
		if ((CurrentTaskType(vm) & k_CitadelCameraTypes) != 0)
		{
			camera.owner = TaskNumber(vm);
			result = true;
		}
	}
	else if (cameraTaken)
	{
		camera.owner = TaskNumber(vm);
		camera.drawHighlight = 0;
		camera.drawLeash = 0;
		result = true;
	}
	else
	{
		ScriptMessage("Note-Camera control Failed");
	}
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Script: START_CAMERA_CONTROL task {} -> {}", TaskNumber(vm), result);
	}
	return result;
}

void ReleaseCameraControl(CameraControl& camera, audio::ScriptAudioState& audio)
{
	// (pending, openblack has no camera modes) the original checks the current camera mode: none -> "Script camera has
	// been removed!"; else the dual camera is released and, when the mode is the script one, it is replaced by the
	// normal mode, or "We are in the wrong camera mode! - exception happened?". Pending too: setting the field of view
	// to k_ScriptEndFov over k_ScriptEndFovTime
	camera.owner = 0;
	audio.creatureSound = 1;
	camera.drawHighlight = 1;
	camera.drawLeash = 1;
	// The original also clears a "sound effects off" switch of the audio system, without stopping the samples.
	// openblack has no such switch, and nothing in the game sets it, so it changes nothing.
	// Every SuperVillager is released and the list emptied; the original also rebuilds the landscape draw list
	// (openblack has none)
	ecs::super_villager::ReleaseAll();
	camera.field7C = 0;
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Script: camera control released");
	}
}

bool EndCameraControl(CameraControl& camera, audio::ScriptAudioState& audio, const Vm& vm)
{
	if (camera.owner != TaskNumber(vm))
	{
		return false;
	}
	ReleaseCameraControl(camera, audio);
	return true;
}

bool ReleaseCameraOf(CameraControl& camera, audio::ScriptAudioState& audio, uint32_t task)
{
	if (camera.owner != task)
	{
		return false;
	}
	ReleaseCameraControl(camera, audio);
	return true;
}

bool StartScriptIfNoDialogue(const HelpSystem& help, std::string_view name, const Vm& vm)
{
	if (const auto owner = help.GetDialogueOwner(); owner != 0 && TaskType(vm, owner) == 2)
	{
		if (vm.stopTasksOfType)
		{
			vm.stopTasksOfType(k_HelpScriptTypes);
		}
	}
	if (help.GetDialogueOwner() != 0)
	{
		return false;
	}
	const bool multiplayer = vm.multiplayer && vm.multiplayer();
	if (vm.startScript)
	{
		vm.startScript(name, multiplayer ? k_MultiplayerScriptTypes : k_SinglePlayerScriptTypes);
	}
	return true;
}

bool StopHelpScriptsForNewHelp(const HelpSystem& help, const Vm& vm)
{
	// A task with the dialogue whose type has neither Help nor MultiplayerHelp keeps it
	if (const auto owner = help.GetDialogueOwner(); owner != 0 && (TaskType(vm, owner) & 0x42u) == 0)
	{
		return false;
	}
	if (vm.stopTasksOfType)
	{
		vm.stopTasksOfType(k_HelpScriptTypes);
	}
	return true;
}

bool RunMessage(HelpSystem& help, uint32_t first, uint32_t last, std::string_view script, const Vm& vm, uint32_t turn)
{
	if (first > last) // unsigned compare
	{
		return false;
	}
	if (!StopHelpScriptsForNewHelp(help, vm))
	{
		return false;
	}
	help.SetMessageTurn(turn);
	if (vm.pushFloat)
	{
		vm.pushFloat(static_cast<float>(first)); // pushed as floats
		vm.pushFloat(static_cast<float>(last));
	}
	// The script types allowed: 0x7F, or 0x60 in a multiplayer game
	const bool multiplayer = vm.multiplayer && vm.multiplayer();
	if (vm.startScript)
	{
		vm.startScript(script, multiplayer ? k_MultiplayerScriptTypes : k_SinglePlayerScriptTypes);
	}
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: RunMessage({}, {}, {})", first, last, script);
	}
	return true;
}

namespace
{
/// Script slow-motion control off when there is a camera (not ported, see StartGameSpeed), game speed 1.0 and no owner
void RestoreGameSpeed(CameraControl& camera)
{
	game_clock::SetSpeed(1.0f);
	camera.gameSpeedOwner = 0;
}
} // namespace

void StartGameSpeed(CameraControl& camera, const Vm& vm)
{
	const uint32_t task = vm.taskNumber ? vm.taskNumber() : 0;
	if (camera.gameSpeedOwner == 0 || camera.gameSpeedOwner == task)
	{
		camera.gameSpeedOwner = task;
	}
}

void EndGameSpeed(CameraControl& camera, const Vm& vm)
{
	const uint32_t task = vm.taskNumber ? vm.taskNumber() : 0;
	if (camera.gameSpeedOwner == 0 || camera.gameSpeedOwner == task)
	{
		RestoreGameSpeed(camera);
	}
}

void SetGameSpeed(const CameraControl& camera, const Vm& vm, float speed)
{
	const uint32_t task = vm.taskNumber ? vm.taskNumber() : 0;
	if (camera.gameSpeedOwner == task)
	{
		game_clock::SetSpeed(speed);
	}
}

void ReleaseGameSpeedOf(CameraControl& camera, uint32_t task)
{
	if (camera.gameSpeedOwner == task)
	{
		RestoreGameSpeed(camera);
	}
}

void OnTaskStopped(uint32_t task, HelpSystem* help, CameraControl& camera, audio::ScriptAudioState& audio)
{
	if (help == nullptr)
	{
		return;
	}
	help->ReleaseDialogueControl(task);
	help->ReleaseWideScreenOf(task);
	ReleaseCameraOf(camera, audio, task);
	ReleaseGameSpeedOf(camera, task);
	hand_demo::EndIfTask(task); // ends the hand demo playback when this task started it
}

} // namespace openblack::help::script_control
