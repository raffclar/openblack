/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameInput.h"

#include <cctype>

#include <algorithm>
#include <optional>
#include <string>
#include <tuple>

#include <SDL_events.h>
#include <SDL_keyboard.h>
#include <SDL_mouse.h>
#include <imgui.h>

#include "Camera/Camera.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "Editor/EditorEntities.h"
#include "Gestures/GesturePaths.h"
#include "Gestures/GestureRecorder.h"
#include "Gestures/GestureRequests.h"
#include "Input/GameActionMapInterface.h"
#include "Input/InjectedInput.h"
#include "Input/KeyBindings.h"
#include "Input/ScriptedMouse.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::inspector;

namespace
{

/// The gestures there are, by their numbers
constexpr uint32_t k_FirstGesture = 1;
constexpr uint32_t k_LastGesture = 23;

std::string Lower(std::string_view text)
{
	std::string lower(text);
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower;
}

uint32_t WindowId()
{
	return Locator::windowing::has_value() ? Locator::windowing::value().GetID() : 0;
}

/// While the inspector moves the pointer, the debug windows don't take the mouse as theirs, as with the testbed's
void KeepDebugWindowsOffTheMouse(bool off)
{
	if (ImGui::GetCurrentContext() == nullptr)
	{
		return;
	}
	auto& io = ImGui::GetIO();
	io.ConfigFlags = off ? (io.ConfigFlags | ImGuiConfigFlags_NoMouse) : (io.ConfigFlags & ~ImGuiConfigFlags_NoMouse);
}

std::optional<GestureType> GestureNamed(std::string_view name)
{
	const auto wanted = Lower(name);
	for (auto number = k_FirstGesture; number <= k_LastGesture; ++number)
	{
		const auto gesture = static_cast<GestureType>(number);
		if (Lower(gesture::Name(gesture)) == wanted)
		{
			return gesture;
		}
	}
	return std::nullopt;
}

const input::KeyBinding* ActionNamed(std::string_view name)
{
	const auto wanted = Lower(name);
	const auto& bindings = Locator::gameActionSystem::has_value()
	                           ? Locator::gameActionSystem::value().GetKeyBindings()
	                           : std::span<const input::KeyBinding>(input::k_DefaultKeyBindings);
	const auto found = std::ranges::find_if(bindings, [&wanted](const auto& binding) { return Lower(binding.name) == wanted; });
	return found != bindings.end() ? &*found : nullptr;
}

/// The modifiers the keys held make
uint16_t Modifiers(const std::vector<int>& held)
{
	uint16_t mod = KMOD_NONE;
	for (const auto key : held)
	{
		switch (key)
		{
		case SDL_SCANCODE_LSHIFT:
			mod |= KMOD_LSHIFT;
			break;
		case SDL_SCANCODE_RSHIFT:
			mod |= KMOD_RSHIFT;
			break;
		case SDL_SCANCODE_LCTRL:
			mod |= KMOD_LCTRL;
			break;
		case SDL_SCANCODE_RCTRL:
			mod |= KMOD_RCTRL;
			break;
		case SDL_SCANCODE_LALT:
			mod |= KMOD_LALT;
			break;
		case SDL_SCANCODE_RALT:
			mod |= KMOD_RALT;
			break;
		default:
			break;
		}
	}
	return mod;
}

} // namespace

std::string GameInput::Apply(const InputEvent& event)
{
	if (!Locator::gameActionSystem::has_value())
	{
		return "there are no controls";
	}
	auto& actions = Locator::gameActionSystem::value();
	auto pointer = actions.GetScriptedPointer().value_or(input::GameActionInterface::ScriptedPointer {
	    .position = glm::ivec2(actions.GetMousePosition()),
	});
	const auto takeMouse = [&actions, &pointer]() {
		actions.SetScriptedPointer(pointer);
		KeepDebugWindowsOffTheMouse(true);
	};
	const bool mouseTiming = event.timing == InputEvent::Timing::Mouse;
	// With a real mouse's timing, the scripted mouse is taken where the pointer is, and its moves and buttons are sent as
	// events whose state the game takes as they are pumped, after the action map has read the pointer for the frame
	const auto sendAsMouse = [&actions, &pointer, this](SDL_Event sent) {
		if (!actions.GetScriptedPointer().has_value())
		{
			actions.SetScriptedPointer(pointer);
		}
		KeepDebugWindowsOffTheMouse(true);
		// Written as the window would report it, should the scripted mouse be handed back before it is pumped
		pointer = input::QueuedAhead(*actions.GetScriptedPointer(), ScreenSize());
		std::ignore = input::Pump(pointer, sent, ScreenSize());
		SDL_PushEvent(&sent);
	};

	switch (event.kind)
	{
	case InputEvent::Kind::PointerTo:
	{
		if (mouseTiming)
		{
			// A step of a drag or a path moves the mouse on by the step, wherever the game has put the cursor meanwhile;
			// a move to a pixel goes there from where the pointer will be once what was sent before is pumped
			const auto ahead = actions.GetScriptedPointer().has_value()
			                       ? input::QueuedAhead(*actions.GetScriptedPointer(), ScreenSize()).position
			                       : pointer.position;
			const auto by = event.stepFrom.has_value() ? event.position - *event.stepFrom : event.position - ahead;
			// The window reports no move that doesn't move the pointer, but the mouse is still the inspector's
			if (by == glm::ivec2(0))
			{
				if (!actions.GetScriptedPointer().has_value())
				{
					takeMouse();
				}
				return {};
			}
			sendAsMouse(input::PumpedMotion(by, WindowId()));
			return {};
		}
		const auto moved = event.position - pointer.position;
		pointer.position = event.position;
		takeMouse();
		// The game follows the pointer by its moves, as it does the mouse's
		SDL_Event motion {};
		motion.type = SDL_MOUSEMOTION;
		motion.motion.windowID = WindowId();
		motion.motion.state = pointer.buttons;
		motion.motion.x = pointer.position.x;
		motion.motion.y = pointer.position.y;
		motion.motion.xrel = moved.x;
		motion.motion.yrel = moved.y;
		input::MarkInjected(motion);
		SDL_PushEvent(&motion);
		return {};
	}
	case InputEvent::Kind::ButtonDown:
	case InputEvent::Kind::ButtonUp:
	{
		const bool down = event.kind == InputEvent::Kind::ButtonDown;
		if (mouseTiming)
		{
			sendAsMouse(input::PumpedButton(event.button, down, event.clicks, WindowId()));
			return {};
		}
		pointer.buttons = down ? (pointer.buttons | SDL_BUTTON(event.button)) : (pointer.buttons & ~SDL_BUTTON(event.button));
		takeMouse();
		SDL_Event button {};
		button.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
		button.button.windowID = WindowId();
		button.button.button = event.button;
		button.button.state = down ? SDL_PRESSED : SDL_RELEASED;
		button.button.clicks = event.clicks;
		button.button.x = pointer.position.x;
		button.button.y = pointer.position.y;
		input::MarkInjected(button);
		SDL_PushEvent(&button);
		return {};
	}
	case InputEvent::Kind::KeyDown:
	case InputEvent::Kind::KeyUp:
	{
		const auto scancode = SDL_GetScancodeFromName(event.key.c_str());
		if (scancode == SDL_SCANCODE_UNKNOWN)
		{
			return "no key " + event.key;
		}
		const bool down = event.kind == InputEvent::Kind::KeyDown;
		const auto code = static_cast<int>(scancode);
		std::erase(_heldKeys, code);
		if (down)
		{
			_heldKeys.push_back(code);
		}
		actions.HoldScriptedKey(code, down);
		const auto mod = Modifiers(_heldKeys);
		SDL_SetModState(static_cast<SDL_Keymod>(mod));
		SDL_Event key {};
		key.type = down ? SDL_KEYDOWN : SDL_KEYUP;
		key.key.windowID = WindowId();
		key.key.state = down ? SDL_PRESSED : SDL_RELEASED;
		key.key.keysym.scancode = scancode;
		key.key.keysym.sym = SDL_GetKeyFromScancode(scancode);
		key.key.keysym.mod = mod;
		input::MarkInjected(key);
		SDL_PushEvent(&key);
		return {};
	}
	case InputEvent::Kind::Wheel:
	{
		takeMouse();
		SDL_Event wheel {};
		wheel.type = SDL_MOUSEWHEEL;
		wheel.wheel.windowID = WindowId();
		wheel.wheel.y = event.notches;
		wheel.wheel.preciseY = static_cast<float>(event.notches);
		input::MarkInjected(wheel);
		SDL_PushEvent(&wheel);
		return {};
	}
	case InputEvent::Kind::Action:
	{
		const auto* binding = ActionNamed(event.name);
		if (binding == nullptr)
		{
			return "no action " + event.name;
		}
		// Pressed for a frame through the action's own key, or held for the frame without one
		actions.QueuePress(binding->action);
		return {};
	}
	case InputEvent::Kind::Gesture:
	{
		const auto gesture = GestureNamed(event.name);
		if (!gesture.has_value() || !Locator::gestureSystem::has_value())
		{
			return "no gesture " + event.name;
		}
		auto& gestures = Locator::gestureSystem::value();
		// Drawn as the debug windows and the testbed draw it: across the middle of the screen
		const auto aspect = gestures.GetScreenAspect();
		const glm::vec2 middle {gesture::k_ReferenceHeight * aspect * 0.5f, gesture::k_ReferenceHeight * 0.5f};
		constexpr float k_Size = 320.0f;
		auto path = gesture::TraceGesture(gestures.GetTemplates(), *gesture, middle, k_Size, aspect);
		if (!path.has_value() && *gesture == GestureType::Circle)
		{
			path = gesture::TraceCircle(middle, k_Size * 0.5f, true);
		}
		else if (!path.has_value() && *gesture == GestureType::Scribble)
		{
			path = gesture::TraceScribble(middle, k_Size, 5);
		}
		if (!path.has_value())
		{
			return "no template to draw " + event.name + " from";
		}
		gestures.DrawPath(std::move(*path), event.holdAction);
		return {};
	}
	case InputEvent::Kind::Release:
		Release();
		return {};
	}
	return "unknown input";
}

void GameInput::Release()
{
	auto& actions = Locator::gameActionSystem::value();
	// Every key and button held lets go, then the mouse is the player's again
	for (const auto key : std::exchange(_heldKeys, {}))
	{
		actions.HoldScriptedKey(key, false);
		SDL_Event up {};
		up.type = SDL_KEYUP;
		up.key.windowID = WindowId();
		up.key.state = SDL_RELEASED;
		up.key.keysym.scancode = static_cast<SDL_Scancode>(key);
		up.key.keysym.sym = SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(key));
		input::MarkInjected(up);
		SDL_PushEvent(&up);
	}
	SDL_SetModState(KMOD_NONE);
	// The buttons held once what was sent is pumped, which the mouse's timing may not have yet
	if (const auto applied = actions.GetScriptedPointer(); applied.has_value())
	{
		const auto pointer = std::optional(input::QueuedAhead(*applied, ScreenSize()));
		for (uint8_t button = SDL_BUTTON_LEFT; button <= SDL_BUTTON_X2; ++button)
		{
			if ((pointer->buttons & SDL_BUTTON(button)) == 0)
			{
				continue;
			}
			SDL_Event up {};
			up.type = SDL_MOUSEBUTTONUP;
			up.button.windowID = WindowId();
			up.button.button = button;
			up.button.state = SDL_RELEASED;
			up.button.clicks = 1;
			up.button.x = pointer->position.x;
			up.button.y = pointer->position.y;
			input::MarkInjected(up);
			SDL_PushEvent(&up);
		}
	}
	actions.SetScriptedPointer(std::nullopt);
	KeepDebugWindowsOffTheMouse(false);
}

glm::ivec2 GameInput::ScreenSize() const
{
	return Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(1, 1);
}

std::optional<glm::ivec2> GameInput::WorldToScreen(glm::vec3 point) const
{
	if (!Locator::camera::has_value())
	{
		return std::nullopt;
	}
	// Through the view drawn: an override's while one is shown, so that the pointer goes where the point is seen
	std::optional<OverrideForPointer> shown;
	if (_camera != nullptr)
	{
		shown.emplace(*_camera);
	}
	const auto size = glm::vec2(ScreenSize());
	glm::vec3 screen {0.0f};
	if (!Locator::camera::value().ProjectWorldToScreen(point, {0.0f, 0.0f, size.x, size.y}, screen))
	{
		return std::nullopt;
	}
	return glm::clamp(glm::ivec2(glm::round(glm::vec2(screen))), glm::ivec2(0), glm::ivec2(size) - 1);
}

float GameInput::GroundHeight(glm::vec2 point) const
{
	return editor::LandHeight(point);
}

bool GameInput::HasKey(std::string_view name) const
{
	return SDL_GetScancodeFromName(std::string(name).c_str()) != SDL_SCANCODE_UNKNOWN;
}

bool GameInput::HasAction(std::string_view name) const
{
	return ActionNamed(name) != nullptr;
}

bool GameInput::HasGesture(std::string_view name) const
{
	return GestureNamed(name).has_value();
}

std::vector<std::string> GameInput::ActionNames() const
{
	std::vector<std::string> names;
	for (const auto& binding : input::k_DefaultKeyBindings)
	{
		names.emplace_back(binding.name);
	}
	return names;
}

std::vector<std::string> GameInput::GestureNames() const
{
	std::vector<std::string> names;
	for (auto number = k_FirstGesture; number <= k_LastGesture; ++number)
	{
		names.emplace_back(gesture::Name(static_cast<GestureType>(number)));
	}
	return names;
}

void GameInput::SetLockMode(std::string_view mode)
{
	if (!Locator::gameActionSystem::has_value())
	{
		return;
	}
	const auto lock = mode == "locked" ? input::LockMode::Locked
	                  : mode == "auto" ? input::LockMode::Auto
	                                   : input::LockMode::Unlocked;
	Locator::gameActionSystem::value().SetInputLockMode(lock);
}

Json GameInput::LockState() const
{
	return InputLockState();
}

Json openblack::inspector::InputLockState()
{
	if (!Locator::gameActionSystem::has_value())
	{
		return nullptr;
	}
	const auto& actions = Locator::gameActionSystem::value();
	return {{"mode", input::Name(actions.GetInputLockMode())}, {"locked", actions.IsPlayerInputBlocked()}};
}

Json GameInput::State() const
{
	Json state = Json::object();
	if (Locator::gameActionSystem::has_value())
	{
		const auto& actions = Locator::gameActionSystem::value();
		const auto scripted = actions.GetScriptedPointer();
		const auto pointer = actions.GetPointerPosition();
		state["pointer"] = {pointer.x, pointer.y};
		state["scripted"] = scripted.has_value();
		// With a real mouse's timing, where the pointer goes and the buttons it holds once what was sent is pumped
		if (scripted.has_value())
		{
			const auto ahead = input::QueuedAhead(*scripted, ScreenSize());
			if (ahead.position != scripted->position || ahead.buttons != scripted->buttons)
			{
				state["queued"] = {{"pointer", {ahead.position.x, ahead.position.y}}, {"buttons", ahead.buttons}};
			}
		}
		state["buttons"] = actions.GetPointerButtons();
		// A double click waits here until the camera takes it
		state["double_click"] = actions.GetUnbindable(input::UnbindableActionMap::DOUBLE_CLICK);
	}
	state["keys_held"] = Json::array();
	for (const auto key : _heldKeys)
	{
		state["keys_held"].push_back(SDL_GetScancodeName(static_cast<SDL_Scancode>(key)));
	}
	if (Locator::handSystem::has_value())
	{
		const auto positions = Locator::handSystem::value().GetPlayerHandPositions();
		if (positions[0].has_value())
		{
			state["hand"] = {positions[0]->x, positions[0]->y, positions[0]->z};
		}
	}
	if (Locator::handGrabSystem::has_value())
	{
		const auto held = Locator::handGrabSystem::value().GetHeld();
		state["held"] = held.has_value() ? Json(entt::to_integral(*held)) : Json(nullptr);
	}
	if (Locator::pickingSystem::has_value())
	{
		const auto& pick = Locator::pickingSystem::value().GetPick();
		state["pick"] = {
		    {"object", pick.object.has_value() ? Json(entt::to_integral(*pick.object)) : Json(nullptr)},
		    {"land", pick.land.has_value() ? Json::array({pick.land->x, pick.land->y, pick.land->z}) : Json(nullptr)},
		};
		// Whether the hand would take the thing under it: with the right button (Action), never the left, which grips
		// the land
		if (pick.object.has_value() && Locator::handGrabSystem::has_value())
		{
			const auto why = Locator::handGrabSystem::value().WhyNotTake(*pick.object);
			state["pick"]["grab"] = {{"takes", why.empty()}, {"button", "right"}};
			if (!why.empty())
			{
				state["pick"]["grab"]["why_not"] = why;
			}
		}
	}
	return state;
}
