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

#include <optional>

#include <glm/vec2.hpp>

/// Which press of a mouse button is a double click, as the game gets it from Windows. The game reads the double click
/// messages of its window. Windows sends a double click when a press of a button comes soon after the last press of
/// the same button, near its place. After a double click, the next press starts a new pair. Thus the third quick press
/// is a plain press and the fourth is a double click again.
namespace openblack::input::mouse_clicks
{

/// How soon and how near the second press must come to make a double click
struct Settings
{
	/// The second press must come before this time from the first press
	uint32_t milliseconds {0};
	/// The width and the height of the box on the place of the first press. The second press must be in the box.
	glm::ivec2 box {0, 0};
};

/// The values of a Windows system that the player did not change
inline constexpr Settings k_WindowsDefaults {.milliseconds = 500, .box = {4, 4}};

/// The values of this system. On Windows, they are the player's own values, which the game uses. On the other systems,
/// they are the Windows defaults.
[[nodiscard]] Settings SystemSettings();

/// The count of clicks that a mouse event gives for a plain press and for the second press of a double click
inline constexpr uint8_t k_PlainPress = 1;
inline constexpr uint8_t k_DoublePress = 2;

/// Whether a count of clicks tells the second press of a double click. Every reader of a mouse event uses this rule.
[[nodiscard]] constexpr bool IsDoublePress(uint8_t clicks) noexcept
{
	return clicks == k_DoublePress;
}

/// The count of clicks for a press that a tool made, from the count that the tool gives. A tool counts the presses as the
/// window system does (1, 2, 3, ...). Thus an even count is the second press of a pair.
[[nodiscard]] constexpr uint8_t ClicksOfToolPress(uint8_t clicks) noexcept
{
	return clicks != 0 && clicks % 2 == 0 ? k_DoublePress : k_PlainPress;
}

/// A press of a mouse button: which button, when, and where on the screen in pixels
struct Press
{
	uint8_t button {0};
	uint32_t milliseconds {0};
	glm::ivec2 position {0, 0};
};

/// Tells the double clicks from the presses of all the buttons, and keeps the buttons that the game holds down. A plain
/// press holds its button until the button goes up. The second press of a double click does not hold the button.
class Clicks
{
public:
	explicit Clicks(Settings settings = k_WindowsDefaults) noexcept
	    : _settings(settings)
	{
	}

	/// A button goes down. Gives the count of clicks: k_DoublePress for the second press of a double click, else
	/// k_PlainPress.
	[[nodiscard]] uint8_t Down(const Press& press) noexcept;
	/// A press that a tool made, with the count of clicks that the tool gives. It ends the pair that waits.
	[[nodiscard]] uint8_t ToolDown(uint8_t button, uint8_t clicks) noexcept;
	/// A button goes up. Gives the count of clicks of its last press.
	[[nodiscard]] uint8_t Up(uint8_t button) noexcept;

	/// The buttons that plain presses hold, one bit for each button: bit (button - 1), as SDL_BUTTON gives it
	[[nodiscard]] uint32_t HeldButtons() const noexcept { return _held; }
	[[nodiscard]] const Settings& GetSettings() const noexcept { return _settings; }

private:
	void Hold(uint8_t button, uint8_t clicks) noexcept;
	[[nodiscard]] static uint32_t Bit(uint8_t button) noexcept;

	Settings _settings;
	/// The first press of the pair that can become a double click
	std::optional<Press> _first;
	uint32_t _held {0};
	/// The buttons whose last press was the second press of a double click, one bit for each button
	uint32_t _lastDouble {0};
};

} // namespace openblack::input::mouse_clicks
