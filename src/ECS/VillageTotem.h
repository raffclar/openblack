/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <numbers>

/// The rules of a village's totem: the statue on the town centre that rises with the share of the town's people who
/// worship, and that the hand takes hold of and slides up and down to set that share
namespace openblack::ecs::village_totem
{

/// How long the statue takes to ease to a new share: this many milliseconds for the whole way from none to all
constexpr float k_EaseMsPerShare = 5200.0f;
/// An ease shorter than this, in milliseconds, is no ease: the statue is put at its new share at once
constexpr float k_SnapMs = 0.001f;
/// The statue has arrived once it is this close to its share
constexpr float k_ArrivedWithin = 0.005f;
/// How far the statue stands risen with all its town at worship
constexpr float k_RiseAtFullShare = 8.0f;
/// How far above the plinth's place its icon stands
constexpr float k_IconAbovePlinth = 2.729f;
/// The hand holds the icon this much of the icon's height above the icon's place
constexpr float k_GripHeight = 0.7f;
/// Moving the mouse the screen's height slides the hand this many times the icon's height
constexpr float k_SlidePerScreen = 3.0f;
/// The share the held statue moves for each unit the hand slides
constexpr float k_SharePerSlide = 0.1f;
/// The hand's standard size, against which it tips near the land and closes on the icon
constexpr float k_HandSize = 3.2f;
/// Below this height the hand tips over towards the land
constexpr float k_LowHand = 2.5f;
/// How far the holding hand is tipped higher up: seven sixteenths of a half turn
constexpr float k_HandTilt = 7.0f * std::numbers::pi_v<float> / 16.0f;

/// How the statue eases from where it stands, at the speed it moves, to a new share, coming to rest there with no
/// jolt: its share and speed follow a curve of the fourth degree in the time since it set off
struct Ease
{
	/// The share the statue stands at now, and how fast it moves, in share per millisecond
	float share {0.0f};
	float speed {0.0f};
	/// The share it was last set to: where it is going
	float target {0.0f};
	/// Milliseconds since it set off, and how long it takes
	float elapsed {0.0f};
	float duration {0.0f};
	/// Where it set off from and how fast it was going then
	float startShare {0.0f};
	float startSpeed {0.0f};
	/// The curve's acceleration, its change and that one's change at the start
	float acceleration {0.0f};
	float jerk {0.0f};
	float snap {0.0f};
	/// Set when it is given a share, until it has arrived
	bool moving {false};
};

/// The statue is given a new share: it eases there from where it stands, taking the time its last share and the new one
/// are apart; so near that it would take no time, it is put there at once
void SetShare(Ease& ease, float share);
/// The statue moves on by the milliseconds that have passed
void Step(Ease& ease, float milliseconds);
/// Whether the statue has just arrived at its share; the moving flag is cleared, so this answers once per share
[[nodiscard]] bool TakeArrival(Ease& ease);

/// How far the statue stands risen for a share
[[nodiscard]] float RiseOf(float share);

/// How far up the hand slides for the mouse's movement up the screen this frame, in pixels; going down it stops at the
/// land under the hand, which is how far above the land the hand is
[[nodiscard]] float HandSlide(float upPixels, float screenHeight, float iconHeight, float handAboveLand);
/// The share the hand holds the statue at after it slides; between none and all
[[nodiscard]] float SlideShare(float held, float slide);
/// Where the hand holds the statue: above the icon's place by part of its height
[[nodiscard]] float GripY(float iconY, float iconHeight);
/// How far the holding hand is tipped, in radians: more as it comes near the land
[[nodiscard]] float HandTiltAt(float handY);
/// How far the hand closes on the icon: by how wide the icon is against the hand, at most fully
[[nodiscard]] float GripClosure(float iconRadius);

} // namespace openblack::ecs::village_totem
