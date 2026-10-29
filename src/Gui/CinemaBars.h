/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <limits>

namespace openblack::gui
{

/// Black bars across the top and bottom of the screen that the scripts slide in for their cut scenes, turning the picture
/// to 16:9. They slide in a set time of game time, from wherever they are. A new game starts with them sliding out.
/// The time they take is the game's, passed in each time.
///
/// The bars have two clocks. The frames move the drawn bars by the game time of each frame. The game turns move the bars
/// that the scripts wait for by the time of each turn, so that the world does not change with the frame rate.
class CinemaBars
{
public:
	/// Slides the bars in or out
	void Set(bool on, float transitionSeconds);
	/// Brings the bars all the way in at once, as a full-screen video does
	void Snap();
	/// Slides the drawn bars on by the game time of the frame, none while the game is paused
	void Update(float gameMilliseconds, float transitionSeconds);
	/// Slides the bars that the scripts wait for on by the time of a game turn
	void ProcessTurn(float turnMilliseconds, float transitionSeconds);
	/// The drawn bars start from the bars of the turns again, as after a load
	void DrawFromTurns();
	/// How far in the drawn bars are, 0 for none and 1 for a 16:9 picture
	[[nodiscard]] float GetFraction() const { return _drawnFraction; }
	/// Whether the bars are in or coming in
	[[nodiscard]] bool IsOn() const { return _on; }
	/// Whether the bars of the turns have finished sliding
	[[nodiscard]] bool IsTransitionFinished() const { return _on ? _fraction >= 1.0f : _fraction <= 0.0f; }
	/// The height of each bar in pixels on a screen, none on screens 16:9 or wider
	[[nodiscard]] static int BarHeight(int width, int height, float fraction);

private:
	/// How far in the bars are after a slide of this time, for a slide of this length in seconds
	[[nodiscard]] float Fraction(float timer, float transitionSeconds) const;

	bool _on {false};
	/// Game time since they started sliding, in milliseconds. A new game resets the bars to off at once, with no slide.
	float _timer {-std::numeric_limits<float>::max()};
	float _transitionSeconds {0.0f};
	float _fraction {0.0f};
	/// The time and the fraction of the drawn bars, which the frames move. A save does not keep them.
	float _drawnTimer {-std::numeric_limits<float>::max()};
	float _drawnFraction {0.0f};

public:
	/// Gives the values that a saved game keeps to a visitor
	template <typename Self, typename Visitor>
	static void VisitSaved(Self& self, Visitor&& visit)
	{
		visit(self._on, self._timer, self._transitionSeconds, self._fraction);
	}
};

} // namespace openblack::gui
