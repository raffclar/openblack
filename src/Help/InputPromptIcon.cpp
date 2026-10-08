/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InputPromptIcon.h"

#include <algorithm>
#include <array>
#include <vector>

#include <SDL_keyboard.h>

#include "ECS/Systems/ScriptStateInterface.h"
#include "Locator.h"

namespace openblack::help::input_prompt
{
namespace
{

struct State
{
	std::vector<InputPromptIcon> icons; ///< the linked list, the head first
	Handle next {1};
};

/// The input prompt icons (Locator::scriptState)
State& GetState()
{
	return openblack::Locator::scriptState::value().Get<State>();
}

/// The per-frame update of one icon
void Update(InputPromptIcon& icon, float dt)
{
	// elapsed += dt, then below 0 it is 0 and past the duration it is the duration
	icon.elapsed += dt;
	if (icon.elapsed < 0.0f)
	{
		icon.elapsed = 0.0f;
	}
	if (icon.elapsed > icon.duration)
	{
		icon.elapsed = icon.duration;
	}
	// current = target; with a duration > 0, start + (target - start) * (elapsed / duration)
	icon.current = icon.target;
	if (icon.duration > 0.0f)
	{
		icon.current = icon.elapsed / icon.duration * (icon.target - icon.start) + icon.start;
	}
	// c1.a = c2.a = int(a * 255.0)
	const auto alpha = static_cast<uint32_t>(static_cast<int32_t>(Alpha(icon) * 255.0f)) & 0xFFu;
	icon.c1 = (icon.c1 & 0x00FFFFFFu) | (alpha << 24);
	icon.c2 = (icon.c2 & 0x00FFFFFFu) | (alpha << 24);
}

/// The mouse code of a binding as the drawn click type
int32_t ClickTypeOfMouseCode(int32_t code)
{
	// 1 LMB -> 1, 2 MMB -> 4, 3 wheel up -> 0x10, 4 wheel down -> 8, 5 RMB -> 2, anything else -1
	// (not ported) code 0 for the actions 3 / 4 / 6
	switch (code)
	{
	case 1:
		return 1;
	case 2:
		return 4;
	case 3:
		return 0x10;
	case 4:
		return 8;
	case 5:
		return 2;
	default:
		return -1;
	}
}

/// A binding of the control map: the action, its mouse code (0: none) and its key (SDL_SCANCODE_UNKNOWN:
/// none)
struct Binding
{
	int32_t action;
	int32_t mouseCode;
	SDL_Scancode key;
};

/// The control map's defaults: action 1 -> the mouse code 1 (LMB), action 2 -> the mouse code 5 (RMB), action 0 -> the
/// key F1. (pending) the other defaults are not decoded, and openblack has no control map: its GameActionMap
/// (Input/GameActionMap.cpp) binds the same three, so the table stands for it until the bindings can be asked
constexpr std::array<Binding, 3> k_Defaults {{
    {1, 1, SDL_SCANCODE_UNKNOWN},
    {2, 5, SDL_SCANCODE_UNKNOWN},
    {0, 0, SDL_SCANCODE_F1},
}};

} // namespace

Handle Create(const Desc& desc)
{
	auto& state = GetState();
	InputPromptIcon icon;
	icon.id = state.next++;
	if (state.next == k_None)
	{
		state.next = 1;
	}
	icon.start = desc.start;
	icon.current = desc.start;
	icon.target = desc.target;
	icon.duration = desc.duration;
	icon.elapsed = 0.0f;
	icon.animType = desc.animType;
	icon.clickType = desc.clickType;
	icon.row = desc.row;
	icon.keyName = desc.keyName;         // (approximate) a copy: the original shares one buffer, rewritten by every
	                                     // key resolve
	icon.text = desc.text.value_or(u""); // L"" for a null text
	icon.x = desc.x;
	icon.y = desc.y;
	icon.size = desc.size;
	icon.align = desc.align;
	icon.c1 = desc.c1.value_or(k_White); // white when null
	icon.c2 = desc.c2.value_or(icon.c1); // c1 when null
	icon.alpha8 = desc.alpha8;
	// linked at the head of the list
	state.icons.insert(state.icons.begin(), std::move(icon));
	return state.icons.front().id;
}

void Destroy(Handle handle)
{
	// unlinked from the list (the head, or its predecessor's link)
	if (handle == k_None)
	{
		return;
	}
	auto& icons = GetState().icons;
	std::erase_if(icons, [handle](const InputPromptIcon& icon) { return icon.id == handle; });
}

InputPromptIcon* Get(Handle handle)
{
	if (handle == k_None)
	{
		return nullptr;
	}
	auto& icons = GetState().icons;
	const auto found = std::ranges::find(icons, handle, &InputPromptIcon::id);
	return found != icons.end() ? &*found : nullptr;
}

void Frame(float realSeconds, size_t pass)
{
	// (not ported) the stand-alone mouse picture: nothing sets it, so it draws nothing (inferred). Then the update on
	// each icon from the head. Twice a frame, from the help system's draw and the help text's end of frame callback,
	// with the same dt both times: the fades take half their nominal time
	for (auto& icon : GetState().icons)
	{
		Update(icon, realSeconds);
		icon.passAlpha.at(pass) = Alpha(icon);
	}
}

float Alpha(const InputPromptIcon& icon)
{
	// current <= 0 gives 0, current < 1 gives current, else 1
	if (icon.current <= 0.0f)
	{
		return 0.0f;
	}
	return icon.current < 1.0f ? icon.current : 1.0f;
}

std::span<const InputPromptIcon> All()
{
	return GetState().icons;
}

void Snapshot(std::vector<graphics::InputPromptDraw>& out)
{
	out.clear();
	for (const auto& icon : GetState().icons)
	{
		out.push_back({icon.animType, icon.clickType, icon.row, icon.keyName, icon.text, icon.x, icon.y, icon.size, icon.align,
		               icon.c1, icon.c2, icon.alpha8, icon.passAlpha});
	}
}

bool ResolveAction(int32_t action, int32_t& animType, int32_t& clickType, int32_t& row, std::u16string& keyName)
{
	// -1 and 0x21 have no picture
	if (action == -1 || action == 0x21)
	{
		return false;
	}
	const auto binding = std::ranges::find(k_Defaults, action, &Binding::action);
	if (binding == k_Defaults.end())
	{
		return false; // (inferred) no binding: the caller falls back
	}
	// a mouse binding: animType 0, its click type, the mouse row
	if (binding->mouseCode != 0)
	{
		animType = 0;
		clickType = ClickTypeOfMouseCode(binding->mouseCode);
		row = MouseRow();
		keyName.clear();
		return true;
	}
	// a key binding: animType -1, clickType 0, row = its name.
	// (approximate) the name is SDL's; the original asks Windows for it (inferred), which may give another spelling in
	// another language
	animType = -1;
	clickType = 0;
	row = 0;
	keyName.clear();
	for (const char* c = SDL_GetScancodeName(binding->key); c != nullptr && *c != '\0'; ++c)
	{
		keyName.push_back(static_cast<char16_t>(static_cast<unsigned char>(*c)));
	}
	return true;
}

int32_t MouseRow()
{
	// 2 with a wheel, 1 with more than 2 buttons, else 0. (not verified) openblack cannot ask DirectInput's mouse caps:
	// a wheel mouse is assumed
	return 2;
}

} // namespace openblack::help::input_prompt
