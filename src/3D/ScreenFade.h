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

namespace openblack
{

/// The original's full-screen fade and cinema bars.
///
/// Fade (script state): SET_FADE / SET_FADE_IN move the alpha linearly once per game turn; the colour is drawn over
/// everything at the end of the frame.
/// Cinema bars (help system state, SET_WIDESCREEN): a fraction 0..1 that slides linearly in
/// HelpSystemInfo.wideScreenTime seconds of game time; bars of (H - 0.5625 W) f / 2 pixels.
class ScreenFade
{
public:
	/// SET_FADE: fade from transparent to the colour in `seconds` (instant if <= 0)
	void FadeTo(uint8_t red, uint8_t green, uint8_t blue, float seconds);
	/// SET_FADE_IN: from opaque back to transparent in `seconds` (instant if <= 0)
	void FadeBackToNormal(float seconds);
	/// FADE_FINISHED: no fade in progress
	[[nodiscard]] bool IsFinished() const { return _rate == 0.0f; }
	/// Once per game turn, from the script's turn
	void ProcessTurn();
	/// The fade colour as ARGB: alpha 0 means nothing is drawn
	[[nodiscard]] uint32_t GetColour() const { return _colour; }
	/// Set the colour directly, as the temple writes it every frame it runs (the falling spell's white fade,
	/// video/FallingSpellVideo.h); the rate and the current alpha of ProcessTurn stay
	void SetColour(uint32_t argb) { _colour = argb; }

	/// SET_WIDESCREEN
	void SetWideScreen(bool on, float transitionSeconds);
	/// Every frame with the game-time milliseconds of this frame (0 while the game is paused)
	void UpdateWideScreen(float gameMilliseconds);
	/// Before a full-screen movie (after SetWideScreen(1, 0)): the timer = -FLT_MAX, so the bars' percentage is 1 at once
	/// (with the bars on) and stays there while 0 ms are added
	void SnapWideScreen();
	/// 0 (no bars) .. 1 (the picture is 16:9)
	[[nodiscard]] float GetWideScreenFraction() const { return _wideFraction; }
	/// The bars are on or coming
	[[nodiscard]] bool IsWideScreenOn() const { return _wideOn; }
	/// WIDESCREEN_TRANSISTION_FINISHED
	[[nodiscard]] bool IsWideScreenTransitionFinished() const
	{
		return _wideOn ? _wideFraction >= 1.0f : _wideFraction <= 0.0f;
	}
	/// Height of each bar in pixels
	[[nodiscard]] static int LetterboxHeight(int width, int height, float fraction);

private:
	/// |timer * 0.001 / wideScreenTime|, 1 - that with the bars off, in [0, 1]
	[[nodiscard]] float WideScreenPercentage() const;

	float _rate {0.0f};    ///< alpha per turn, 0 when idle
	float _current {0.0f}; ///< alpha 0..255
	uint32_t _colour {0};  ///< ARGB
	bool _wideOn {false};
	float _wideTimer {0.0f}; ///< milliseconds
	float _wideTime {2.0f};  ///< HelpSystemInfo.wideScreenTime
	float _wideFraction {0.0f};
};

} // namespace openblack
