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

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace openblack::gui
{

/// A colour over the whole screen that fades towards an amount at a whole of it a second, as the temple's fade does.
/// Fading up, it turns when it goes past the amount and fades back down; fading down past nothing, its colour goes back
/// to black. Beyond a whole amount the screen stays covered, so a fade from more than one waits that long before it
/// shows anything. It steps once each frame that the game draws, by the real time of that draw.
class ScreenFade
{
public:
	/// Covers the screen by an amount and fades it away, in the colour it has
	void FadeFrom(float amount);
	/// Covers the screen by an amount of a colour and fades it away
	void FadeFrom(float amount, glm::vec3 colour);
	/// Fades a colour over the screen, then back away
	void FadeThrough(glm::vec3 colour);
	/// Moves the fade on by the real milliseconds of the draw. Outside the temple, a fade that is at its amount does not
	/// move. Inside the temple, it steps each draw: when it is at its amount, it turns again.
	void Update(uint32_t realMilliseconds, bool inTemple);

	/// The colour over the screen. Its alpha is how much of the screen it covers, in steps of 1/255 that round down.
	[[nodiscard]] glm::vec4 GetColour() const;
	/// How much the fade covers the screen, beyond a whole while it waits to show anything
	[[nodiscard]] float GetAmount() const noexcept { return _amount; }
	/// How many times the fade has reached where it was going
	[[nodiscard]] uint32_t GetTurns() const noexcept { return _turns; }

private:
	float _amount {0.0f};
	float _target {0.0f};
	glm::vec3 _colour {0.0f};
	uint32_t _turns {0};
};

} // namespace openblack::gui
