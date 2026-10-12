/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ControlIcons.h"

#include <algorithm>

namespace openblack::help::control_icons
{

namespace
{
using input::BindableActionMap;
using input::KeyBinding;
using input::MouseInput;

/// The icons' right edge is this far in from the screen's
constexpr int k_RightInset = 4;
/// The gap the icon keeps from its (empty) label
constexpr float k_Gap = 2.0f;
/// The glow's strength at full fade
constexpr int k_GlowAlpha = 0x80;
/// The mice picture is 4 by 4
constexpr float k_Cell = 0.25f;
/// The wheel turns a frame every tenth of a second, four frames and a rest
constexpr uint32_t k_WheelStep = 100;
constexpr uint32_t k_WheelSteps = 5;
constexpr uint32_t k_WheelFrames = 4;
/// The wheel's cells: the wheel lit at rest, then its four frames turning
constexpr int k_WheelRest = 11;
constexpr int k_WheelFirstFrame = 12;
constexpr int k_WheelLastFrame = 15;
/// The bevelled key on atmos.raw (256 pixels square): its left end, middle and right end, half a pixel in
constexpr float k_Atmos = 256.0f;
constexpr std::array<float, 4> k_KeyU = {128.5f / k_Atmos, 144.5f / k_Atmos, 175.5f / k_Atmos, 191.5f / k_Atmos};
constexpr float k_KeyVTop = 0.5f / k_Atmos;
constexpr float k_KeyVBottom = 63.5f / k_Atmos;
/// The words on a key cap are four tenths of its height
constexpr float k_KeyTextShare = 0.4f;
/// The separators between the top icon's keys: the second, then every later one
constexpr std::u16string_view k_SecondKey = u" + ";
constexpr std::u16string_view k_LaterKey = u" / ";

[[nodiscard]] std::optional<BindableActionMap> ActionOf(int action) noexcept
{
	if (action < 0 || action >= static_cast<int>(input::k_KeyBindingCount))
	{
		return std::nullopt;
	}
	return static_cast<BindableActionMap>(uint64_t {1} << static_cast<unsigned>(action));
}

[[nodiscard]] const KeyBinding* Find(int action, std::span<const KeyBinding> bindings) noexcept
{
	const auto mapped = ActionOf(action);
	if (!mapped.has_value())
	{
		return nullptr;
	}
	const auto index = input::IndexOf(bindings, *mapped);
	return index.has_value() ? &bindings[*index] : nullptr;
}

[[nodiscard]] MouseInput MouseOf(BindableActionMap action, std::span<const KeyBinding> bindings) noexcept
{
	const auto index = input::IndexOf(bindings, action);
	return index.has_value() ? bindings[*index].mouse : MouseInput::None;
}

/// The zooms take the two buttons together when they have no mouse binding of their own
[[nodiscard]] bool TakesBothButtons(int action) noexcept
{
	const auto mapped = ActionOf(action);
	return mapped == BindableActionMap::ZOOM_OUT || mapped == BindableActionMap::ZOOM_IN ||
	       mapped == BindableActionMap::ZOOM_ON;
}

/// The bound key's own name, none when the action has no key
[[nodiscard]] std::optional<std::u16string> KeyName(const KeyBinding& binding, const KeyNameFn& keyName)
{
	if (!binding.key.has_value() || binding.key->key == SDL_SCANCODE_UNKNOWN)
	{
		return std::nullopt;
	}
	return keyName(binding.key->key);
}

/// The bound key with the modifier held with it: "Ctrl + S"
[[nodiscard]] std::optional<std::u16string> KeyText(const KeyBinding& binding, const KeyNameFn& keyName)
{
	auto name = KeyName(binding, keyName);
	if (!name.has_value())
	{
		return std::nullopt;
	}
	SDL_Scancode modifier = SDL_SCANCODE_UNKNOWN;
	switch (binding.key->modifier)
	{
	case input::Modifier::None:
		return name;
	case input::Modifier::Ctrl:
		modifier = SDL_SCANCODE_LCTRL;
		break;
	case input::Modifier::Shift:
		modifier = SDL_SCANCODE_LSHIFT;
		break;
	case input::Modifier::Alt:
		modifier = SDL_SCANCODE_LALT;
		break;
	}
	return keyName(modifier) + std::u16string(k_SecondKey) + *name;
}

[[nodiscard]] Box ToBox(int x0, int y0, int x1, int y1)
{
	return {{static_cast<float>(x0), static_cast<float>(y0)}, {static_cast<float>(x1), static_cast<float>(y1)}};
}
} // namespace

bool IsCameraKey(int action) noexcept
{
	return action >= k_FirstCameraKey && action <= k_LastCameraKey;
}

std::optional<std::u16string> KeyIconText(std::span<const int> codes, std::span<const KeyBinding> bindings,
                                          const KeyNameFn& keyName)
{
	std::optional<int> madeBy;
	std::u16string text;
	int count = 0;
	for (const int code : codes)
	{
		if (!IsCameraKey(code))
		{
			continue;
		}
		const auto* binding = Find(code, bindings);
		const auto name = binding != nullptr ? KeyName(*binding, keyName) : std::nullopt;
		if (!name.has_value())
		{
			continue;
		}
		// The action that made the icon starts its words again
		if (!madeBy.has_value() || code == *madeBy)
		{
			madeBy = madeBy.value_or(code);
			text.clear();
			count = 0;
		}
		++count;
		if (count == 2)
		{
			text += k_SecondKey;
		}
		else if (count > 2)
		{
			text += k_LaterKey;
		}
		text += *name;
	}
	if (!madeBy.has_value())
	{
		return std::nullopt;
	}
	return text;
}

std::optional<MouseClick> ClickFor(int action, std::span<const KeyBinding> bindings)
{
	const auto* binding = Find(action, bindings);
	const MouseInput mouse = binding != nullptr ? binding->mouse : MouseInput::None;
	switch (mouse)
	{
	case MouseInput::LeftButton:
		return MouseClick::LeftButton;
	case MouseInput::RightButton:
		return MouseClick::RightButton;
	case MouseInput::MiddleButton:
		return MouseClick::MiddleButton;
	case MouseInput::WheelDown:
		return MouseClick::WheelDown;
	case MouseInput::WheelUp:
		return MouseClick::WheelUp;
	case MouseInput::None:
		break;
	}
	if (binding == nullptr || !TakesBothButtons(action))
	{
		return std::nullopt;
	}
	const auto move = MouseOf(BindableActionMap::MOVE, bindings);
	const auto act = MouseOf(BindableActionMap::ACTION, bindings);
	const bool apart = (move == MouseInput::LeftButton && act == MouseInput::RightButton) ||
	                   (move == MouseInput::RightButton && act == MouseInput::LeftButton);
	return apart ? std::optional(MouseClick::BothButtons) : std::nullopt;
}

std::optional<ActionPicture> ActionIcon(std::span<const int> codes, std::span<const KeyBinding> bindings,
                                        const KeyNameFn& keyName)
{
	std::optional<ActionPicture> picture;
	for (const int code : codes)
	{
		if (IsCameraKey(code))
		{
			continue;
		}
		if (auto shown = PictureFor(code, bindings, keyName); shown.has_value())
		{
			picture = std::move(*shown);
		}
	}
	return picture;
}

std::optional<ActionPicture> PictureFor(int action, std::span<const KeyBinding> bindings, const KeyNameFn& keyName)
{
	if (const auto click = ClickFor(action, bindings); click.has_value())
	{
		return *click;
	}
	if (const auto* binding = Find(action, bindings); binding != nullptr)
	{
		if (auto text = KeyText(*binding, keyName); text.has_value())
		{
			return std::move(*text);
		}
	}
	return std::nullopt;
}

MouseCell CellFor(MouseClick click, int row, uint32_t tickMs)
{
	const auto frame = (tickMs / k_WheelStep) % k_WheelSteps;
	const int rowStart = row * 4;
	int cell = rowStart;
	bool mirrored = false;
	switch (click)
	{
	case MouseClick::None:
		break;
	case MouseClick::LeftButton: // the right button's picture, mirrored
		cell = rowStart + 1;
		mirrored = true;
		break;
	case MouseClick::RightButton:
		cell = rowStart + 1;
		break;
	case MouseClick::BothButtons:
		cell = rowStart + 2;
		break;
	case MouseClick::MiddleButton:
		cell = rowStart + 3;
		break;
	case MouseClick::WheelDown:
		cell = frame < k_WheelFrames ? k_WheelLastFrame - static_cast<int>(frame) : k_WheelRest;
		break;
	case MouseClick::WheelUp:
		cell = frame < k_WheelFrames ? k_WheelFirstFrame + static_cast<int>(frame) : k_WheelRest;
		break;
	}
	const auto column = static_cast<float>(cell % 4);
	const auto cellRow = static_cast<float>(cell / 4);
	MouseCell out;
	out.uvMin = {(mirrored ? column + 1.0f : column) * k_Cell, cellRow * k_Cell};
	out.uvMax = {(mirrored ? column : column + 1.0f) * k_Cell, (cellRow + 1.0f) * k_Cell};
	return out;
}

int IconSize(int top, int bottom)
{
	return (bottom - top + 1) / 3;
}

int KeyIconTop(int top, int bottom)
{
	// Given by its middle row, half an icon below the box's top
	const int size = IconSize(top, bottom);
	return (top + (size / 2)) - (size / 2);
}

int ActionIconTop(int top, int bottom)
{
	const int size = IconSize(top, bottom);
	return (bottom - (size / 2) + 1) - (size / 2);
}

float KeyTextSize(int size)
{
	return static_cast<float>(size) * k_KeyTextShare;
}

float KeyCapWidth(int size, float textWidth)
{
	const float width = textWidth + static_cast<float>((size * 2) / 3);
	return std::max(width, static_cast<float>(size));
}

std::optional<Placed> Place(int screenWidth, int y, int size, float iconWidth, float share)
{
	const float fade = std::clamp(share, 0.0f, 1.0f);
	const int alpha = static_cast<int>(fade * 255.0f);
	if (alpha < 4)
	{
		return std::nullopt;
	}
	const int glowAlpha = static_cast<int>(static_cast<float>(k_GlowAlpha) * fade);
	// Right-aligned after its empty label and the gap
	const int x = static_cast<int>(static_cast<float>(screenWidth - k_RightInset) - (iconWidth + k_Gap));
	const int iconX = static_cast<int>(static_cast<float>(x) + k_Gap);
	const int right = static_cast<int>(static_cast<float>(iconX) + iconWidth);
	return Placed {
	    .icon = ToBox(iconX, y, right, y + size),
	    .glowColour = {1.0f, 1.0f, 1.0f, static_cast<float>(glowAlpha) / 255.0f},
	    .alpha = static_cast<float>(alpha) / 255.0f,
	    .size = size,
	};
}

KeyCap LayoutKeyCap(const Placed& placed)
{
	const auto x0 = static_cast<int>(placed.icon.min.x);
	const auto y0 = static_cast<int>(placed.icon.min.y);
	const auto x1 = static_cast<int>(placed.icon.max.x);
	const auto y1 = static_cast<int>(placed.icon.max.y);
	const int height = y1 - y0;
	const int end = height / 4;
	KeyCap cap;
	const std::array<int, 4> x = {x0, x0 + end, x1 - end, x1};
	for (size_t i = 0; i < cap.slices.size(); ++i)
	{
		cap.slices.at(i) = ToBox(x.at(i), y0, x.at(i + 1), y1);
		cap.uvs.at(i) = {{k_KeyU.at(i), k_KeyVTop}, {k_KeyU.at(i + 1), k_KeyVBottom}};
	}
	cap.textAt = {static_cast<float>(x0 + (height / 5)), static_cast<float>(y0 + (height / 7))};
	cap.textSize = KeyTextSize(height);
	cap.textColour = {0.0f, 0.0f, 0.0f, placed.alpha};
	return cap;
}

} // namespace openblack::help::control_icons
