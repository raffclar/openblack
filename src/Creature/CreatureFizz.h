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

// A creature fizzes out of sight and back in through static: when it is carried home after passing out, when a script
// loads it, fades it in or deletes it, and when a vortex takes it. The fizz is 0 (in full sight) to 1 (gone). Setting
// it going towards a target over some seconds moves it a share of the way each game turn, so it reaches the target
// after that many seconds; over no seconds it is there at once. Setting it going right out of sight sounds the
// teleport's energise. A creature can be made to go for good once it has fizzed right out, and from then on nothing
// changes its fizz again.
namespace openblack::creature_fizz
{

/// A creature's fizz and where it is going
struct Fizz
{
	/// 0 in full sight to 1 gone
	float now {0.0f};
	float target {0.0f};
	/// The share of the way it moves each second, towards the target; 0 when it isn't moving
	float perSecond {0.0f};
	/// Removed from the world once it has fizzed right out; nothing changes its fizz again
	bool goesForGood {false};

	bool operator==(const Fizz&) const = default;
};

/// The fizz set going, and whether that sounds the teleport's energise (it does when set going right out of sight)
struct Set
{
	Fizz fizz;
	bool sound {false};
};

/// Sets the fizz going to the target (kept within 0 to 1) over so many seconds, or there at once over none or when
/// already there. A creature going for good ignores this.
[[nodiscard]] Set SetFizz(const Fizz& fizz, float target, float seconds, bool goesForGood);

/// One game turn of so many milliseconds: the fizz moves on, stopping at its target once past it
[[nodiscard]] Fizz Step(const Fizz& fizz, uint32_t turnMilliseconds);

/// Whether it has fizzed right out and goes for good
[[nodiscard]] bool Gone(const Fizz& fizz);

/// Whether it is in full sight and staying so: nothing left to do
[[nodiscard]] bool Settled(const Fizz& fizz);

/// The number of whole game turns timing something that lasts so many seconds: the turns in a second, a whole number,
/// times the seconds, the fraction dropped
[[nodiscard]] uint16_t TurnsOf(float seconds, uint32_t turnMilliseconds);

/// Carried home, a creature fizzes out over this long, is moved, then fizzes back in over this long
constexpr float k_CarryHomeSeconds = 2.0f;

} // namespace openblack::creature_fizz
