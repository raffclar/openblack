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

#include <string_view>

union SDL_Event;

namespace openblack::input
{

/// Whether the player's mouse and keyboard are kept out while an agent drives the game through the debug inspector
enum class LockMode : uint8_t
{
	/// Never kept out: the game without the inspector, or a human taking over
	Unlocked,
	/// Kept out while an inspector client is connected, and a few seconds after the last one leaves
	Auto,
	/// Always kept out, as for a headless scenario run
	Locked,
};

[[nodiscard]] std::string_view Name(LockMode mode);

/// What becomes of an event under the lock
enum class LockVerdict : uint8_t
{
	/// It reaches the game
	Pass,
	/// The player's input while locked: it never reaches the game, the debug windows or the camera
	Drop,
	/// The emergency release: the lock opens for good and the human has the game
	Release,
};

/// The lock on the player's mouse and keyboard. Only the game's own input (the testbed's and the inspector's, which
/// they mark) gets through while it is locked; the window's own events (quitting, closing, resizing, focus, redrawing)
/// always do, and so does the emergency release, Ctrl+Alt+Shift+F12 pressed on the keyboard.
class InputLock
{
public:
	/// How long it stays locked after the last client leaves, so that a tool that connects for each call keeps it
	static constexpr float k_LingerSeconds = 3.0f;

	void SetMode(LockMode mode) { _mode = mode; }
	[[nodiscard]] LockMode Mode() const { return _mode; }

	/// Once a frame: whether an inspector client is connected, and the seconds since the last frame
	void Update(bool clientConnected, float seconds);

	[[nodiscard]] bool Locked() const;

	/// What becomes of an event; the emergency release opens the lock
	LockVerdict Filter(const SDL_Event& event);

private:
	LockMode _mode {LockMode::Unlocked};
	/// Seconds since a client was last connected; none while one is
	float _sinceClient {k_LingerSeconds};
};

/// Whether an event is the emergency release: Ctrl+Alt+Shift+F12 going down
[[nodiscard]] bool IsEmergencyRelease(const SDL_Event& event);

} // namespace openblack::input
