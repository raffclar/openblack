/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HandDemoSystem.h"

#include <HNDFile.h>
#include <SDL.h>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Input/GameActionMapInterface.h"
#include "Input/InjectedInput.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// The game's time now, as the recordings count it
uint32_t GameTime()
{
	if (!Locator::time::has_value())
	{
		return 0;
	}
	const auto& time = Locator::time::value();
	return hand_demo::GameHundredths(time.GetTurn(), time.GetTurnFraction());
}

/// A demonstration's file, read once
std::shared_ptr<const hnd::HNDFile> LoadDemo(std::string_view name)
{
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return nullptr;
	}
	auto& cache = Locator::resources::value().GetHandDemos();
	const auto id = entt::hashed_string(std::string(name).c_str()).value();
	if (!cache.Contains(id))
	{
		const auto path =
		    Locator::filesystem::value().GetPath<filesystem::Path::Data>() / "HandDemo" / (std::string(name) + ".hnd");
		try
		{
			cache.Load(id, resources::HandDemoLoader::FromDiskTag {}, path);
		}
		catch (const std::runtime_error& error)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "The hand demonstration {} can't be read: {}", name, error.what());
			return nullptr;
		}
	}
	return cache.Handle(id).handle();
}

/// The game runs at its normal speed while a demonstration plays
void NormalSpeed()
{
	if (Locator::time::has_value())
	{
		Locator::time::value().SetSpeed(1.0f);
	}
}

void PushInjected(SDL_Event& event)
{
	event.motion.windowID = Locator::windowing::has_value() ? Locator::windowing::value().GetID() : 0;
	input::MarkInjected(event);
	SDL_PushEvent(&event);
}

/// The mouse button a recorded button message is: the one that grips the land, or the action button
std::optional<std::pair<uint8_t, bool>> RecordedButton(hnd::HNDMessage message)
{
	switch (message)
	{
	case hnd::HNDMessage::MoveButtonDown:
		return std::pair {static_cast<uint8_t>(SDL_BUTTON_LEFT), true};
	case hnd::HNDMessage::MoveButtonUp:
		return std::pair {static_cast<uint8_t>(SDL_BUTTON_LEFT), false};
	case hnd::HNDMessage::ActionButtonDown:
		return std::pair {static_cast<uint8_t>(SDL_BUTTON_RIGHT), true};
	case hnd::HNDMessage::ActionButtonUp:
		return std::pair {static_cast<uint8_t>(SDL_BUTTON_RIGHT), false};
	default:
		return std::nullopt;
	}
}
} // namespace

void HandDemoSystem::Play(std::string_view name, uint32_t task, bool pauseOnTrigger, bool withoutHandModify)
{
	// The hand lets go of what it holds first, unless the demonstration needs it (a miracle to cast)
	// TODO(advisor-help): the game also cancels the player's spells being charged and takes off an idle creature's
	// leash here; neither is reachable this way in openblack yet
	if (!withoutHandModify && Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().GetHeld().has_value())
	{
		Locator::handGrabSystem::value().ForceDrop();
	}
	if (_playback)
	{
		Stop();
	}
	NormalSpeed();
	_task = task;
	_name = std::string(name);
	// The cinema bars snap all the way in, and the interface takes the demonstration's input even in a cut scene
	if (Locator::cinematicDirectorSystem::has_value())
	{
		auto& director = Locator::cinematicDirectorSystem::value();
		if (!director.IsWideScreenOn())
		{
			director.SetWideScreen(true, 0);
		}
		director.SnapWideScreen();
		director.SetInterfaceActive(true);
	}
	_file = LoadDemo(name);
	if (_file != nullptr && !_file->records.empty())
	{
		if (Locator::gameActionSystem::has_value())
		{
			auto& actions = Locator::gameActionSystem::value();
			actions.SetScriptedPointer(input::GameActionInterface::ScriptedPointer {
			    .position = actions.GetPointerPosition(),
			    .buttons = 0,
			});
			actions.SetPlayerMouseBlocked(true);
		}
		std::vector<hand_demo::Played> played;
		_playback = std::make_unique<hand_demo::Playback>(_file->records, GameTime(), played);
		Apply(played);
	}
	// The script's start clears the mark, the first record's too
	_pauseOnTrigger = pauseOnTrigger;
	_triggerReached = false;
}

bool HandDemoSystem::IsPlaying(uint32_t task) const
{
	return _playback != nullptr && (task == 0 || task == _task);
}

bool HandDemoSystem::TakeTrigger()
{
	return std::exchange(_triggerReached, false);
}

void HandDemoSystem::Update()
{
	if (!_playback)
	{
		return;
	}
	NormalSpeed();
	std::vector<hand_demo::Played> played;
	const bool goesOn = _playback->Advance(GameTime(), _pauseOnTrigger, _triggerReached, played);
	Apply(played);
	if (!goesOn)
	{
		Stop();
	}
}

void HandDemoSystem::Apply(const std::vector<hand_demo::Played>& played)
{
	if (played.empty() || !Locator::gameActionSystem::has_value())
	{
		return;
	}
	auto& actions = Locator::gameActionSystem::value();
	auto pointer = actions.GetScriptedPointer().value_or(
	    input::GameActionInterface::ScriptedPointer {.position = actions.GetPointerPosition()});
	const auto screen = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(0);
	const float bars = Locator::cinematicDirectorSystem::has_value()
	                       ? Locator::cinematicDirectorSystem::value().GetWideScreenFraction()
	                       : 0.0f;
	for (const auto& [record, hints] : played)
	{
		if (record->message == hnd::HNDMessage::Move)
		{
			// The cursor moves, and the hand with it
			const auto to = hand_demo::CursorPixel(record->cursor, screen, bars);
			const auto moved = to - pointer.position;
			pointer.position = to;
			actions.SetScriptedPointer(pointer);
			SDL_Event event {};
			event.type = SDL_MOUSEMOTION;
			event.motion.state = pointer.buttons;
			event.motion.x = pointer.position.x;
			event.motion.y = pointer.position.y;
			event.motion.xrel = moved.x;
			event.motion.yrel = moved.y;
			PushInjected(event);
		}
		else if (const auto button = RecordedButton(record->message); button.has_value())
		{
			// A button goes down or up where the cursor last was
			// TODO(advisor-help): the game also puts back what the hand held where the recording had it, and sends a
			// click on a thing to the nearest of the same kind within 3 m when the one under the hand isn't it
			const auto [which, down] = *button;
			pointer.buttons = down ? (pointer.buttons | SDL_BUTTON(which)) : (pointer.buttons & ~SDL_BUTTON(which));
			actions.SetScriptedPointer(pointer);
			SDL_Event event {};
			event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
			event.button.button = which;
			event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
			event.button.clicks = 1;
			event.button.x = pointer.position.x;
			event.button.y = pointer.position.y;
			PushInjected(event);
		}
		_camera = CameraPose {
		    .origin = {record->cameraPosition[0], record->cameraPosition[1], record->cameraPosition[2]},
		    .focus = {record->cameraFocus[0], record->cameraFocus[1], record->cameraFocus[2]},
		};
		_hints = hints;
	}
}

void HandDemoSystem::Stop()
{
	if (!_playback)
	{
		return;
	}
	_playback.reset();
	_file.reset();
	_camera.reset();
	_hints = 0;
	_pauseOnTrigger = false;
	_task = 0;
	// The player's mouse has the hand again; in a script's cut scene the interface is put away again
	if (Locator::gameActionSystem::has_value())
	{
		auto& actions = Locator::gameActionSystem::value();
		actions.SetScriptedPointer(std::nullopt);
		actions.SetPlayerMouseBlocked(false);
	}
	if (Locator::cinematicDirectorSystem::has_value())
	{
		auto& director = Locator::cinematicDirectorSystem::value();
		if (director.IsWideScreenOn() && director.GetWideScreenOwner() != 0)
		{
			director.SetInterfaceActive(false);
		}
	}
}

void HandDemoSystem::TaskStopped(uint32_t task)
{
	if (IsPlaying(task) && task != 0)
	{
		Stop();
	}
}

void HandDemoSystem::Reset()
{
	Stop();
	_triggerReached = false;
	_name.clear();
}

std::optional<HandDemoSystemInterface::CameraPose> HandDemoSystem::GetCamera() const
{
	return _camera;
}

std::optional<HandDemoSystemInterface::Status> HandDemoSystem::GetStatus() const
{
	if (!_playback || _file == nullptr)
	{
		return std::nullopt;
	}
	return Status {
	    .name = _name,
	    .task = _task,
	    .record = _playback->Next(),
	    .records = _file->records.size(),
	    .pauseOnTrigger = _pauseOnTrigger,
	    .triggerReached = _triggerReached,
	    .holding = _pauseOnTrigger && _triggerReached,
	};
}
