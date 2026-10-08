/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameActionMap.h"

#include <algorithm>
#include <utility>

#include <SDL_events.h>
#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "FixedMouse.h"
#include "Game.h"
#include "GameCursor.h"
#include "Locator.h"
#include "RealInput.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack::input;

GameActionMap::GameActionMap()
{
	ApplyMouseBindings();
}

void GameActionMap::ApplyMouseBindings()
{
	_mouseBindings.clear();
	_mouseModBindings.clear();
	constexpr std::array<std::pair<MouseInput, int>, 3> k_Buttons {{
	    {MouseInput::LeftButton, SDL_BUTTON_LMASK},
	    {MouseInput::MiddleButton, SDL_BUTTON_MMASK},
	    {MouseInput::RightButton, SDL_BUTTON_RMASK},
	}};
	for (const auto& [mouse, mask] : k_Buttons)
	{
		if (const auto actions = ActionsForMouse(_bindings, mouse); actions != BindableActionMap::NONE)
		{
			_mouseBindings.emplace(mask, actions);
		}
	}
	const auto wheel = [this](MouseInput mouse) -> std::optional<BindableActionMap> {
		const auto actions = ActionsForMouse(_bindings, mouse);
		return actions != BindableActionMap::NONE ? std::optional(actions) : std::nullopt;
	};
	_mouseWheelBinding[0] = wheel(MouseInput::WheelUp);
	_mouseWheelBinding[1] = wheel(MouseInput::WheelDown);
}

std::span<const KeyBinding> GameActionMap::GetKeyBindings() const
{
	return _bindings;
}

void GameActionMap::SetKeyBinding(BindableActionMap action, std::optional<KeyChord> key)
{
	if (const auto index = IndexOf(_bindings, action))
	{
		// Whatever the old key held of the action is let go
		_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) & ~static_cast<uint64_t>(action));
		_bindings.at(*index).key = key;
	}
}

void GameActionMap::ResetKeyBindings()
{
	_bindings = k_DefaultKeyBindings;
	ApplyMouseBindings();
}

void GameActionMap::QueuePress(BindableActionMap action)
{
	_queuedPresses.emplace_back(action);
}

bool GameActionMap::HasQueuedPresses() const
{
	return !_queuedPresses.empty() || !_queuedReleases.empty() || _queuedHeld != BindableActionMap::NONE;
}

void GameActionMap::ApplyQueuedPresses()
{
	for (const auto& event : std::exchange(_queuedReleases, {}))
	{
		ProcessEvent(event);
	}
	_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) &
	                                              ~static_cast<uint64_t>(std::exchange(_queuedHeld, BindableActionMap::NONE)));
	for (const auto action : std::exchange(_queuedPresses, {}))
	{
		const auto index = IndexOf(_bindings, action);
		if (!index.has_value())
		{
			continue;
		}
		const auto& binding = _bindings.at(*index);
		if (binding.key.has_value())
		{
			// The key goes down with its modifier held, through the same lookup as the player's keys
			constexpr std::array<std::pair<Modifier, uint16_t>, 4> k_Modifiers {{
			    {Modifier::None, KMOD_NONE},
			    {Modifier::Ctrl, KMOD_LCTRL},
			    {Modifier::Shift, KMOD_LSHIFT},
			    {Modifier::Alt, KMOD_LALT},
			}};
			SDL_Event press {};
			press.type = SDL_KEYDOWN;
			press.key.keysym.scancode = binding.key->key;
			press.key.keysym.mod =
			    std::ranges::find(k_Modifiers, binding.key->modifier, &std::pair<Modifier, uint16_t>::first)->second;
			ProcessEvent(press);
			auto release = press;
			release.type = SDL_KEYUP;
			_queuedReleases.emplace_back(release);
		}
		else if (binding.mouse == MouseInput::WheelUp || binding.mouse == MouseInput::WheelDown)
		{
			SDL_Event wheel {};
			wheel.type = SDL_MOUSEWHEEL;
			wheel.wheel.y = binding.mouse == MouseInput::WheelUp ? 1 : -1;
			wheel.wheel.preciseY = static_cast<float>(wheel.wheel.y);
			ProcessEvent(wheel);
		}
		else
		{
			// A mouse button or an unbound action is held for the frame
			_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) | static_cast<uint64_t>(action));
			_queuedHeld = static_cast<BindableActionMap>(static_cast<uint64_t>(_queuedHeld) | static_cast<uint64_t>(action));
		}
	}
}

bool GameActionMap::GetBindable(BindableActionMap action) const
{
	return (static_cast<uint64_t>(_bindableMap) & static_cast<uint64_t>(action)) != 0;
}

bool GameActionMap::GetUnbindable(UnbindableActionMap action) const
{
	return (static_cast<uint8_t>(_unbindableMap) & static_cast<uint8_t>(action)) != 0;
}

bool GameActionMap::GetBindableChanged(BindableActionMap action) const
{
	return ((static_cast<uint64_t>(_bindableMap) ^ static_cast<uint64_t>(_bindableMapPrevious)) &
	        static_cast<uint64_t>(action)) != 0;
}

bool GameActionMap::GetUnbindableChanged(UnbindableActionMap action) const
{
	return ((static_cast<uint8_t>(_unbindableMap) ^ static_cast<uint8_t>(_unbindableMapPrevious)) &
	        static_cast<uint8_t>(action)) != 0;
}

bool GameActionMap::GetBindableRepeat(BindableActionMap action) const
{
	return ((static_cast<uint64_t>(_bindableMap) & static_cast<uint64_t>(_bindableMapPrevious)) &
	        static_cast<uint64_t>(action)) != 0;
}

bool GameActionMap::GetUnbindableRepeat(UnbindableActionMap action) const
{
	return ((static_cast<uint8_t>(_unbindableMap) & static_cast<uint8_t>(_unbindableMapPrevious)) &
	        static_cast<uint8_t>(action)) != 0;
}

glm::uvec2 GameActionMap::GetMousePosition() const
{
	return _mousePosition;
}

glm::ivec2 GameActionMap::GetMouseDelta() const
{
	return _mouseDelta;
}

void GameActionMap::Frame()
{
	if (!FixedMouse().has_value() && !IgnoreRealInput() &&
	    (SDL_GetMouseState(nullptr, nullptr) & (SDL_BUTTON_LMASK | SDL_BUTTON_RMASK)) == (SDL_BUTTON_LMASK | SDL_BUTTON_RMASK))
	{
		_unbindableMap = static_cast<UnbindableActionMap>(static_cast<uint8_t>(_unbindableMap) |
		                                                  static_cast<uint8_t>(UnbindableActionMap::TWO_BUTTON_CLICK));
		_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) &
		                                              ~(static_cast<uint64_t>(_mouseModBindings[SDL_BUTTON_LMASK].second) |
		                                                static_cast<uint64_t>(_mouseModBindings[SDL_BUTTON_RMASK].second)));
		_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) &
		                                              ~(static_cast<uint64_t>(_mouseBindings[SDL_BUTTON_LMASK]) |
		                                                static_cast<uint64_t>(_mouseBindings[SDL_BUTTON_RMASK])));
	}
	else
	{
		_unbindableMap = static_cast<UnbindableActionMap>(static_cast<uint8_t>(_unbindableMap) &
		                                                  ~static_cast<uint8_t>(UnbindableActionMap::TWO_BUTTON_CLICK));
	}

	if (_bindableMap != BindableActionMap::NONE || _unbindableMap != UnbindableActionMap::NONE)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("input"), "GameActionMap:");
	}
#define get_print(x)                                               \
	do                                                             \
	{                                                              \
		if (Get(BindableActionMap::x))                             \
		{                                                          \
			SPDLOG_LOGGER_DEBUG(spdlog::get("input"), "\t{}", #x); \
		}                                                          \
	} while (0)

	get_print(HELP);
	get_print(MOVE);
	get_print(ACTION);
	get_print(ZOOM_OUT);
	get_print(ZOOM_IN);
	get_print(TALK);
	get_print(ZOOM_ON);
	get_print(MOVE_LEFT);
	get_print(MOVE_RIGHT);
	get_print(MOVE_FORWARDS);
	get_print(MOVE_BACKWARDS);
	get_print(TILT_UP);
	get_print(TILT_DOWN);
	get_print(ROTATE_LEFT);
	get_print(ROTATE_RIGHT);
	get_print(ROTATE_ON);
	get_print(ROTATE_AROUND_MOUSE_ON);
	get_print(ZOOM_TO_TEMPLE);
	get_print(ZOOM_TO_CREATURE);
	get_print(ZOOM_TO_REALM);
	get_print(ZOOM_TO_INSIDE_TEMPLE);
	get_print(ZOOM_TO_CREATURE_ROOM);
	get_print(ZOOM_TO_CHALLENGE_ROOM);
	get_print(ZOOM_TO_SAVE_GAME_ROOM);
	get_print(ZOOM_TO_OPTIONS_ROOM);
	get_print(ZOOM_TO_LIBRARY);
	get_print(LEASH_UNLEASH_CREATURE);
	get_print(SHOW_VILLAGER_NAMES);
	get_print(SHOW_VILLAGER_DETAILS);
	get_print(QUICK_SAVE);
	get_print(QUICK_LOAD);
	get_print(PREVIOUS_LEASH);
	get_print(NEXT_LEASH);

#undef get_print

#define get_print(x)                                               \
	do                                                             \
	{                                                              \
		if (Get(UnbindableActionMap::x))                           \
		{                                                          \
			SPDLOG_LOGGER_DEBUG(spdlog::get("input"), "\t{}", #x); \
		}                                                          \
	} while (0)

	get_print(DOUBLE_CLICK);
	get_print(TWO_BUTTON_CLICK);

#undef get_print

	{
		glm::ivec2 absoluteMousePosition;
		SDL_GetMouseState(&absoluteMousePosition.x, &absoluteMousePosition.y);
		if (FixedMouse().has_value())
		{
			absoluteMousePosition = *FixedMouse();
		}
		else if (IgnoreRealInput())
		{
			// the game's cursor of the last frame (OPENBLACK_MOUSE_AT or a hand demo), not the real one
			absoluteMousePosition = GameCursor();
		}
		const auto screenSize =
		    Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(1.0f, 1.0f);
		_mousePosition = glm::clamp(absoluteMousePosition, glm::zero<decltype(screenSize)>(), screenSize);
	}
	_mouseDelta = glm::ivec2(0, 0);
	_bindableMapPrevious = _bindableMap;
	_bindableMap =
	    static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) &
	                                   ~(static_cast<uint64_t>(_mouseWheelBinding[0].value_or(BindableActionMap::NONE)) |
	                                     static_cast<uint64_t>(_mouseWheelBinding[1].value_or(BindableActionMap::NONE))));

	// After the frame's starting state is kept, so that the presses show as changes
	ApplyQueuedPresses();
}

void GameActionMap::ProcessEvent(const SDL_Event& event)
{
	if (event.type == SDL_KEYDOWN)
	{
		const auto key = event.key.keysym.scancode;
		// A binding with a modifier held takes the key from the bindings without one
		_bindableMap = static_cast<BindableActionMap>(
		    (static_cast<uint64_t>(_bindableMap) & ~static_cast<uint64_t>(ActionsForKey(_bindings, key))) |
		    static_cast<uint64_t>(ActionsForKeyDown(_bindings, key, event.key.keysym.mod)));
	}
	// Double click will not count as a single click
	else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && event.button.clicks == 2)
	{
		_unbindableMap = static_cast<UnbindableActionMap>(static_cast<uint8_t>(_unbindableMap) |
		                                                  static_cast<uint8_t>(UnbindableActionMap::DOUBLE_CLICK));
	}
	else if (event.type == SDL_MOUSEBUTTONDOWN)
	{
		const auto mod = static_cast<SDL_Keymod>(SDL_GetModState());
		const uint8_t buttonMask = SDL_BUTTON(event.button.button);
		const uint8_t newComboMask = _currentMouseButtons | buttonMask;
		if (newComboMask != buttonMask)
		{
			if (_mouseModBindings.contains(newComboMask) && (_mouseModBindings[newComboMask].first & mod) != 0)
			{
				_bindableMap = static_cast<BindableActionMap>(
				    // Remove non-modded action
				    (static_cast<uint64_t>(_bindableMap) & ~static_cast<uint64_t>(_mouseBindings[newComboMask])) |
				    // Add modded action
				    static_cast<uint64_t>(_mouseModBindings[newComboMask].second));
				// Remove non-combo keys
				for (uint8_t i = 0; i < SDL_BUTTON_X2; ++i)
				{
					const uint8_t keyMask = (1 << i);
					if ((newComboMask & keyMask) != 0u && _mouseModBindings.contains(keyMask) &&
					    (_mouseModBindings[keyMask].first & mod) != 0)
					{
						_bindableMap = static_cast<BindableActionMap>(
						    static_cast<uint64_t>(_bindableMap) & ~static_cast<uint64_t>(_mouseModBindings[keyMask].second));
					}
				}
			}
			else if (_mouseBindings.contains(newComboMask))
			{
				_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) |
				                                              static_cast<uint64_t>(_mouseBindings[newComboMask]));
				// Remove non-combo keys
				for (uint8_t i = 0; i < SDL_BUTTON_X2; ++i)
				{
					const uint8_t keyMask = (1 << i);
					if ((newComboMask & keyMask) != 0u && _mouseBindings.contains(keyMask))
					{
						_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) &
						                                              ~static_cast<uint64_t>(_mouseBindings[keyMask]));
					}
				}
			}
		}
		else if (_mouseModBindings.contains(buttonMask) && (_mouseModBindings[buttonMask].first & mod) != 0)
		{
			_bindableMap = static_cast<BindableActionMap>(
			    // Remove non-modded action
			    (static_cast<uint64_t>(_bindableMap) & ~static_cast<uint64_t>(_mouseBindings[buttonMask])) |
			    // Add modded action
			    static_cast<uint64_t>(_mouseModBindings[buttonMask].second));
		}
		else if (_mouseBindings.contains(buttonMask))
		{
			_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) |
			                                              static_cast<uint64_t>(_mouseBindings[buttonMask]));
		}
		_currentMouseButtons |= buttonMask;
	}
	else if (event.type == SDL_MOUSEWHEEL)
	{
		if (event.wheel.y > 0)
		{
			_bindableMap =
			    static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) |
			                                   static_cast<uint64_t>(_mouseWheelBinding[0].value_or(BindableActionMap::NONE)));
		}
		else if (event.wheel.y < 0)
		{
			_bindableMap =
			    static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) |
			                                   static_cast<uint64_t>(_mouseWheelBinding[1].value_or(BindableActionMap::NONE)));
		}
	}
	else if (event.type == SDL_KEYUP)
	{
		_bindableMap = static_cast<BindableActionMap>(
		    static_cast<uint64_t>(_bindableMap) & ~static_cast<uint64_t>(ActionsForKey(_bindings, event.key.keysym.scancode)));
	}
	else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT && event.button.clicks == 2)
	{
		_unbindableMap = static_cast<UnbindableActionMap>(static_cast<uint8_t>(_unbindableMap) &
		                                                  ~static_cast<uint8_t>(UnbindableActionMap::DOUBLE_CLICK));
	}
	else if (event.type == SDL_MOUSEBUTTONUP)
	{
		const uint8_t buttonMask = SDL_BUTTON(event.button.button);
		if (_mouseBindings.contains(buttonMask))
		{
			_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) &
			                                              ~static_cast<uint64_t>(_mouseModBindings[buttonMask].second));
			_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) &
			                                              ~static_cast<uint64_t>(_mouseBindings[buttonMask]));
		}
		// Remove combo keys
		for (uint8_t i = 0; i < SDL_BUTTON_X2; ++i)
		{
			const uint8_t keyMask = (1 << i);
			const uint8_t comboMask = keyMask | buttonMask;
			if (buttonMask != comboMask && _mouseBindings.contains(comboMask))
			{
				_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) &
				                                              ~static_cast<uint64_t>(_mouseModBindings[comboMask].second));
				_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) &
				                                              ~static_cast<uint64_t>(_mouseBindings[comboMask]));
			}
		}
		_currentMouseButtons &= ~buttonMask;
	}
	else if (event.type == SDL_MOUSEMOTION)
	{
		// Accumulate: several motion events can arrive in one frame (Frame() resets the delta).
		_mouseDelta += glm::ivec2(event.motion.xrel, event.motion.yrel);
	}
}

std::array<std::optional<glm::vec3>, 2> GameActionMap::GetHandPositions() const
{
	auto handPositions = Locator::handSystem::value().GetPlayerHandPositions();

	return {{
	    handPositions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)],
	    handPositions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Right)],
	}};
}
