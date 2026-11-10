/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// One coordinate of the camera gliding to where it is sent, as the game's camera paths move it: on a curve of the
// fourth degree in time that starts from where it is at the speed it has, and arrives at the destination at the speed
// asked, no longer speeding up or slowing down, after the time given. Sent somewhere new, it starts afresh from where it
// is. The path is solved and moved on with the game's float operations in the game's order. Pure, tested without
// the game.

namespace openblack::camera
{

class Zoomer
{
public:
	/// Puts it at a value, still
	void SetPosition(float value);
	/// Sends it to a destination, to arrive at a speed after a number of seconds; in under a thousandth of a second it
	/// is there at once, still
	void SetDestination(float destination, float speed, float seconds);
	/// It moves on by some seconds, as the game moves the camera's position and focus on
	void Update(float seconds);
	/// It moves on by some seconds, as the game moves the camera's lens on. The value is the same as with `Update`; the
	/// speed adds its terms in a different order, so it can be different in its last bit.
	void UpdateLens(float seconds);

	[[nodiscard]] float Value() const { return _value; }
	[[nodiscard]] float Speed() const { return _speed; }
	/// Where it is going, or where it is when still
	[[nodiscard]] float Destination() const { return _destination; }

private:
	/// Moves its time on. It gives false when the time is up: then it is at its destination, at the speed asked.
	bool Step(float seconds);

	float _value {0.0f};
	float _speed {0.0f};
	float _destination {0.0f};
	float _destinationSpeed {0.0f};
	float _time {0.0f};
	float _duration {0.0f};
	float _startValue {0.0f};
	float _startSpeed {0.0f};
	/// The acceleration at the start, and how it changes and how that changes
	float _c1 {0.0f};
	float _c2 {0.0f};
	float _c3 {0.0f};

public:
	/// Gives the values that a saved game keeps to a visitor
	template <typename Self, typename Visitor>
	static void VisitSaved(Self& self, Visitor&& visit)
	{
		visit(self._value, self._speed, self._destination, self._destinationSpeed, self._time, self._duration, self._startValue,
		      self._startSpeed, self._c1, self._c2, self._c3);
	}
};

} // namespace openblack::camera
