/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InputLock.h"

#include <SDL_events.h>

#include "InjectedInput.h"

using namespace openblack::input;

std::string_view openblack::input::Name(LockMode mode)
{
	switch (mode)
	{
	case LockMode::Unlocked:
		return "unlocked";
	case LockMode::Auto:
		return "auto";
	case LockMode::Locked:
		return "locked";
	}
	return "unknown";
}

void InputLock::Update(bool clientConnected, float seconds)
{
	_sinceClient = clientConnected ? 0.0f : _sinceClient + seconds;
}

bool InputLock::Locked() const
{
	return _mode == LockMode::Locked || (_mode == LockMode::Auto && _sinceClient < k_LingerSeconds);
}

LockVerdict InputLock::Filter(const SDL_Event& event)
{
	if (!Locked() || !IsPlayerDeviceEvent(event) || IsInjected(event))
	{
		return LockVerdict::Pass;
	}
	if (IsEmergencyRelease(event))
	{
		_mode = LockMode::Unlocked;
		return LockVerdict::Release;
	}
	return LockVerdict::Drop;
}

bool openblack::input::IsEmergencyRelease(const SDL_Event& event)
{
	if (event.type != SDL_KEYDOWN || event.key.keysym.scancode != SDL_SCANCODE_F12)
	{
		return false;
	}
	const auto mod = event.key.keysym.mod;
	return (mod & KMOD_CTRL) != 0 && (mod & KMOD_ALT) != 0 && (mod & KMOD_SHIFT) != 0;
}
