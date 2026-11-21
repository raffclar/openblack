/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MouseClicks.h"

#include <cstdlib>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace openblack::input::mouse_clicks
{

Settings SystemSettings()
{
#if defined(_WIN32)
	return {.milliseconds = GetDoubleClickTime(), .box = {GetSystemMetrics(SM_CXDOUBLECLK), GetSystemMetrics(SM_CYDOUBLECLK)}};
#else
	return k_WindowsDefaults;
#endif
}

uint32_t Clicks::Bit(uint8_t button) noexcept
{
	constexpr uint8_t k_Buttons = 32;
	return button == 0 || button > k_Buttons ? 0u : 1u << (button - 1u);
}

void Clicks::Hold(uint8_t button, uint8_t clicks) noexcept
{
	if (IsDoublePress(clicks))
	{
		_lastDouble |= Bit(button);
		return;
	}
	_lastDouble &= ~Bit(button);
	_held |= Bit(button);
}

uint8_t Clicks::Down(const Press& press) noexcept
{
	// The second press must be of the same button, sooner than the time, and in the box on the first press. The box
	// has the first press at its centre.
	const bool pair = _first.has_value() && _first->button == press.button &&
	                  press.milliseconds - _first->milliseconds < _settings.milliseconds &&
	                  std::abs(press.position.x - _first->position.x) < _settings.box.x / 2 &&
	                  std::abs(press.position.y - _first->position.y) < _settings.box.y / 2;
	// A double click ends the pair. Any other press is the first press of a new pair.
	if (pair)
	{
		_first.reset();
	}
	else
	{
		_first = press;
	}
	const uint8_t clicks = pair ? k_DoublePress : k_PlainPress;
	Hold(press.button, clicks);
	return clicks;
}

uint8_t Clicks::ToolDown(uint8_t button, uint8_t clicks) noexcept
{
	_first.reset();
	const uint8_t paired = ClicksOfToolPress(clicks);
	Hold(button, paired);
	return paired;
}

uint8_t Clicks::Up(uint8_t button) noexcept
{
	_held &= ~Bit(button);
	return (_lastDouble & Bit(button)) != 0 ? k_DoublePress : k_PlainPress;
}

} // namespace openblack::input::mouse_clicks
