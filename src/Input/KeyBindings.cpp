/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "KeyBindings.h"

#include <algorithm>
#include <iterator>
#include <string>

#include <SDL_scancode.h>

#include "InputDeviceInterface.h"

namespace openblack::input
{
namespace
{
[[nodiscard]] BindableActionMap Combine(BindableActionMap a, BindableActionMap b) noexcept
{
	return static_cast<BindableActionMap>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
}

[[nodiscard]] bool SameKey(SDL_Scancode a, SDL_Scancode b) noexcept
{
	return CanonicalKey(a) == CanonicalKey(b);
}
} // namespace

SDL_Scancode CanonicalKey(SDL_Scancode key) noexcept
{
	switch (key)
	{
	case SDL_SCANCODE_RCTRL:
		return SDL_SCANCODE_LCTRL;
	case SDL_SCANCODE_RSHIFT:
		return SDL_SCANCODE_LSHIFT;
	case SDL_SCANCODE_RALT:
		return SDL_SCANCODE_LALT;
	default:
		return key;
	}
}

bool ModifierHeld(Modifier modifier, uint16_t sdlModifiers) noexcept
{
	switch (modifier)
	{
	case Modifier::None:
		return true;
	case Modifier::Ctrl:
		return (sdlModifiers & KMOD_CTRL) != 0;
	case Modifier::Shift:
		return (sdlModifiers & KMOD_SHIFT) != 0;
	case Modifier::Alt:
		return (sdlModifiers & KMOD_ALT) != 0;
	}
	return false;
}

Modifier ModifierOfKey(SDL_Scancode key) noexcept
{
	switch (CanonicalKey(key))
	{
	case SDL_SCANCODE_LCTRL:
		return Modifier::Ctrl;
	case SDL_SCANCODE_LSHIFT:
		return Modifier::Shift;
	case SDL_SCANCODE_LALT:
		return Modifier::Alt;
	default:
		return Modifier::None;
	}
}

BindableActionMap ActionsForKeyDown(std::span<const KeyBinding> bindings, SDL_Scancode key, uint16_t sdlModifiers) noexcept
{
	auto withModifier = BindableActionMap::NONE;
	auto plain = BindableActionMap::NONE;
	for (const auto& binding : bindings)
	{
		if (!binding.key.has_value() || !SameKey(binding.key->key, key))
		{
			continue;
		}
		if (binding.key->modifier == Modifier::None)
		{
			plain = Combine(plain, binding.action);
		}
		else if (ModifierHeld(binding.key->modifier, sdlModifiers))
		{
			withModifier = Combine(withModifier, binding.action);
		}
	}
	return withModifier != BindableActionMap::NONE ? withModifier : plain;
}

namespace
{
// The hand and camera actions from moving the hand to turning around the mouse, but talking
constexpr auto k_HeldActions =
    static_cast<uint64_t>(BindableActionMap::MOVE) | static_cast<uint64_t>(BindableActionMap::ACTION) |
    static_cast<uint64_t>(BindableActionMap::ZOOM_OUT) | static_cast<uint64_t>(BindableActionMap::ZOOM_IN) |
    static_cast<uint64_t>(BindableActionMap::ZOOM_ON) | static_cast<uint64_t>(BindableActionMap::MOVE_LEFT) |
    static_cast<uint64_t>(BindableActionMap::MOVE_RIGHT) | static_cast<uint64_t>(BindableActionMap::MOVE_FORWARDS) |
    static_cast<uint64_t>(BindableActionMap::MOVE_BACKWARDS) | static_cast<uint64_t>(BindableActionMap::TILT_UP) |
    static_cast<uint64_t>(BindableActionMap::TILT_DOWN) | static_cast<uint64_t>(BindableActionMap::ROTATE_LEFT) |
    static_cast<uint64_t>(BindableActionMap::ROTATE_RIGHT) | static_cast<uint64_t>(BindableActionMap::ROTATE_ON) |
    static_cast<uint64_t>(BindableActionMap::ROTATE_AROUND_MOUSE_ON);
} // namespace

bool IsHeldAction(BindableActionMap action) noexcept
{
	return (static_cast<uint64_t>(action) & k_HeldActions) != 0;
}

BindableActionMap OnePressActions(BindableActionMap actions) noexcept
{
	return static_cast<BindableActionMap>(static_cast<uint64_t>(actions) & ~k_HeldActions);
}

BindableActionMap ActionsPressedByKeyDown(std::span<const KeyBinding> bindings, SDL_Scancode key, uint16_t sdlModifiers,
                                          BindableActionMap blocked) noexcept
{
	auto actions = BindableActionMap::NONE;
	// An action that acts while it is held is held while its key is, with its modifier when it has one
	for (const auto& binding : bindings)
	{
		if (IsHeldAction(binding.action) && binding.key.has_value() && SameKey(binding.key->key, key) &&
		    ModifierHeld(binding.key->modifier, sdlModifiers))
		{
			actions = Combine(actions, binding.action);
		}
	}
	// A one-press action needs the key with the press's one modifier
	if (const auto index = BindingForKeyDown(bindings, key, sdlModifiers, blocked);
	    index.has_value() && !IsHeldAction(bindings[*index].action))
	{
		actions = Combine(actions, bindings[*index].action);
	}
	return actions;
}

Modifier KeyDownModifier(uint16_t sdlModifiers) noexcept
{
	if ((sdlModifiers & KMOD_SHIFT) != 0)
	{
		return Modifier::Shift;
	}
	if ((sdlModifiers & KMOD_CTRL) != 0)
	{
		return Modifier::Ctrl;
	}
	if ((sdlModifiers & KMOD_ALT) != 0)
	{
		return Modifier::Alt;
	}
	return Modifier::None;
}

std::optional<size_t> BindingForKeyDown(std::span<const KeyBinding> bindings, SDL_Scancode key, uint16_t sdlModifiers,
                                        BindableActionMap blocked) noexcept
{
	const auto modifier = KeyDownModifier(sdlModifiers);
	const auto found = std::ranges::find_if(bindings, [key, modifier](const KeyBinding& binding) {
		return binding.key.has_value() && SameKey(binding.key->key, key) && binding.key->modifier == modifier;
	});
	if (found == bindings.end() || (static_cast<uint64_t>(found->action) & static_cast<uint64_t>(blocked)) != 0)
	{
		return std::nullopt;
	}
	return static_cast<size_t>(std::distance(bindings.begin(), found));
}

KeyDownBinding KeyDownBindingFor(std::span<const KeyBinding> bindings, SDL_Scancode key, uint16_t sdlModifiers,
                                 BindableActionMap blocked, bool scriptHoldsScreen) noexcept
{
	const auto index = BindingForKeyDown(bindings, key, sdlModifiers, blocked);
	if (!index.has_value())
	{
		return KeyDownBinding::None;
	}
	if (scriptHoldsScreen || IsHeldAction(bindings[*index].action))
	{
		return KeyDownBinding::LetsKeyThrough;
	}
	return KeyDownBinding::TakesKey;
}

BindableActionMap MousePressAction(std::span<const KeyBinding> bindings, MouseInput mouse, BindableActionMap blocked,
                                   bool scriptHoldsScreen) noexcept
{
	if (mouse == MouseInput::None || scriptHoldsScreen)
	{
		return BindableActionMap::NONE;
	}
	const auto found = std::ranges::find(bindings, mouse, &KeyBinding::mouse);
	if (found == bindings.end() || (static_cast<uint64_t>(found->action) & static_cast<uint64_t>(blocked)) != 0 ||
	    IsHeldAction(found->action))
	{
		return BindableActionMap::NONE;
	}
	return found->action;
}

bool MousePressActs(std::span<const KeyBinding> bindings, MouseInput mouse, BindableActionMap blocked,
                    bool scriptHoldsScreen) noexcept
{
	return MousePressAction(bindings, mouse, blocked, scriptHoldsScreen) != BindableActionMap::NONE;
}

BindableActionMap ActionsForKey(std::span<const KeyBinding> bindings, SDL_Scancode key) noexcept
{
	auto actions = BindableActionMap::NONE;
	for (const auto& binding : bindings)
	{
		if (binding.key.has_value() && SameKey(binding.key->key, key))
		{
			actions = Combine(actions, binding.action);
		}
	}
	return actions;
}

BindableActionMap ActionsForMouse(std::span<const KeyBinding> bindings, MouseInput mouse) noexcept
{
	auto actions = BindableActionMap::NONE;
	if (mouse == MouseInput::None)
	{
		return actions;
	}
	for (const auto& binding : bindings)
	{
		if (binding.mouse == mouse)
		{
			actions = Combine(actions, binding.action);
		}
	}
	return actions;
}

std::optional<size_t> IndexOf(std::span<const KeyBinding> bindings, BindableActionMap action) noexcept
{
	const auto found = std::ranges::find(bindings, action, &KeyBinding::action);
	if (found == bindings.end())
	{
		return std::nullopt;
	}
	return static_cast<size_t>(std::distance(bindings.begin(), found));
}

std::vector<std::pair<size_t, size_t>> FindConflicts(std::span<const KeyBinding> bindings)
{
	std::vector<std::pair<size_t, size_t>> conflicts;
	for (size_t i = 0; i < bindings.size(); ++i)
	{
		for (size_t j = i + 1; j < bindings.size(); ++j)
		{
			const auto& a = bindings[i];
			const auto& b = bindings[j];
			const bool sameKey =
			    a.key.has_value() && b.key.has_value() && SameKey(a.key->key, b.key->key) && a.key->modifier == b.key->modifier;
			const bool sameMouse = a.mouse != MouseInput::None && a.mouse == b.mouse;
			if (sameKey || sameMouse)
			{
				conflicts.emplace_back(i, j);
			}
		}
	}
	return conflicts;
}

std::string KeyChordName(const KeyChord& chord)
{
	std::string name;
	switch (chord.modifier)
	{
	case Modifier::None:
		break;
	case Modifier::Ctrl:
		name = "Ctrl+";
		break;
	case Modifier::Shift:
		name = "Shift+";
		break;
	case Modifier::Alt:
		name = "Alt+";
		break;
	}
	// The modifier keys stand for both sides, so they go by their plain names
	switch (ModifierOfKey(chord.key))
	{
	case Modifier::Ctrl:
		return name + "Ctrl";
	case Modifier::Shift:
		return name + "Shift";
	case Modifier::Alt:
		return name + "Alt";
	case Modifier::None:
		break;
	}
	const auto keyName = Device().ScancodeName(chord.key);
	return name + (!keyName.empty() ? std::string(keyName) : std::string("?"));
}

std::string_view MouseInputName(MouseInput mouse) noexcept
{
	switch (mouse)
	{
	case MouseInput::None:
		return "";
	case MouseInput::LeftButton:
		return "LMB";
	case MouseInput::MiddleButton:
		return "MMB";
	case MouseInput::WheelUp:
		return "Wheel Up";
	case MouseInput::WheelDown:
		return "Wheel Down";
	case MouseInput::RightButton:
		return "RMB";
	}
	return "";
}

std::string_view CategoryName(BindCategory category) noexcept
{
	switch (category)
	{
	case BindCategory::Hand:
		return "Hand";
	case BindCategory::Camera:
		return "Camera";
	case BindCategory::Places:
		return "Places";
	case BindCategory::Creature:
		return "Creature";
	case BindCategory::Villagers:
		return "Villagers";
	case BindCategory::Interface:
		return "Interface";
	case BindCategory::Game:
		return "Game";
	}
	return "";
}

std::string_view StatusName(BindStatus status) noexcept
{
	switch (status)
	{
	case BindStatus::Implemented:
		return "Implemented";
	case BindStatus::NotYetImplemented:
		return "Not yet implemented";
	case BindStatus::OwnedByLeashWork:
		return "Leash work";
	case BindStatus::OwnedByCreatureModeWork:
		return "Creature mode work";
	}
	return "";
}

} // namespace openblack::input
