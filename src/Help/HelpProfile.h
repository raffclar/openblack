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

#include <array>
#include <optional>
#include <vector>

/// The help system's profile of the player: how often and how lately the player has done each of the things the help
/// keeps count of (tapped a scroll, turned the camera, ...). The scripts read it to judge whether the player has
/// learnt a control yet.
namespace openblack::help::profile
{

/// The things counted, numbered as the scripts number them; 0 is never asked for by a script
constexpr uint32_t k_EventCount = 49;
/// The interface reads a new place of the pointer, after the mouse moved
constexpr uint32_t k_MouseMoved = 1;
/// The hand takes a thing into it. The hand lets go of a thing that it holds: a throw, a drop or a pour.
constexpr uint32_t k_PickUp = 2;
constexpr uint32_t k_Throw = 4;
/// A new miracle seed goes into the hand of this computer's player, before the hand counts it as a thing it took
constexpr uint32_t k_SeedInHand = 12;
/// The hand of this computer's player gives what it holds to a creature
constexpr uint32_t k_Give = 5;
/// A store takes a thing that the hand of this computer's player threw into it
constexpr uint32_t k_ThrownIntoStore = 6;
/// The hand of this computer's player casts a miracle. A spell for creatures has its own count.
constexpr uint32_t k_CastMiracle = 9;
constexpr uint32_t k_CastCreatureMiracle = 10;
/// The hand holds the leash over nothing and the tooltip offers to focus on the creature
constexpr uint32_t k_FocusCreatureTip = 47;
/// The gestures that the help counts. All of them also count as the event for any gesture.
/// - The hand powers up the miracle that it holds.
/// - The player calls the special move of the creature in a fight.
/// - A scribble stops the power-up of the miracle in the hand, or drops the miracle when it has no power-up.
/// - The hand casts a miracle that has a power-up.
constexpr uint32_t k_GesturePowerUp = 18;
constexpr uint32_t k_GestureSpecialMove = 19;
constexpr uint32_t k_GestureStopPowerUp = 20;
constexpr uint32_t k_GestureDropMiracle = 22;
constexpr uint32_t k_GestureCastPoweredUp = 23;
/// The world camera's controls: turning it (and which way: positive is the way the turn-right key turns it), tilting,
/// zooming, a double click on the land or on an object, flying to the temple or over the realm, and dragging it across
/// the land
constexpr uint32_t k_CameraTurn = 25;
constexpr uint32_t k_CameraTurnPositive = 26;
constexpr uint32_t k_CameraTurnNegative = 27;
constexpr uint32_t k_CameraTilt = 28;
constexpr uint32_t k_CameraZoom = 29;
constexpr uint32_t k_CameraDoubleClickLand = 30;
constexpr uint32_t k_CameraDoubleClickObject = 31;
constexpr uint32_t k_CameraFlyToTemple = 32;
constexpr uint32_t k_CameraDrag = 33;
/// The hand of this computer's player tapped a thing that takes a tap
constexpr uint32_t k_TapThing = 8;
/// The hand tapped a scroll or sign with a challenge or tip, or one without
constexpr uint32_t k_TapWithText = 34;
constexpr uint32_t k_TapWithoutText = 35;
/// The player's creature starts a move in a fight from its stance: a block, an attack (a blow or the special move, also
/// each turn that a blow waits to be let go), a miracle, or a step or other animation
constexpr uint32_t k_FightBlock = 36;
constexpr uint32_t k_FightAttack = 37;
constexpr uint32_t k_FightCast = 38;
constexpr uint32_t k_FightStep = 39;
/// The world camera's view, which the help counts each turn: the land shows at the middle of the screen, the land
/// shows near and the camera looks steeply down at it, and no land shows and the camera looks up at the sky
constexpr uint32_t k_CameraSeesLand = 44;
constexpr uint32_t k_CameraNearAndSteep = 45;
constexpr uint32_t k_CameraSeesSky = 46;
/// The camera is near the land when the land at the middle of the screen is nearer than this distance
constexpr float k_NearDistance = 15.0f;
/// The camera looks steeply down above this pitch, and up at the sky below the second pitch, in radians
constexpr float k_SteepPitch = 0.55f;
constexpr float k_SkyPitch = -0.3f;
/// Events that also count as another: 14 to 23 count as 24, 9 to 11 as 11, 1 to 42 as 43, and every one as 48
constexpr uint32_t k_AnyEvent = 48;

/// The profile's clock runs 100 ms a game turn while it is kept
constexpr uint32_t k_MillisecondsPerTurn = 100;
/// The latest times of an event kept, for its rate
constexpr uint32_t k_TimesKept = 64;
/// How quickly the smoothed rate follows the turns the event happens on
constexpr float k_RateFollow = 0.005f;

/// One event's count: its total, the times it last happened, and a smoothed share of the turns it happens on. It
/// counts at most once a turn.
class EventCount
{
public:
	/// It happens at the clock's time, unless it already has this turn
	void Trigger(uint32_t clock);
	/// The turn ends: the smoothed share follows whether it happened, and it may happen again
	void EndTurn();

	[[nodiscard]] uint32_t Total() const { return _total; }
	[[nodiscard]] float SmoothedRate() const { return _smoothedRate; }
	/// The seconds since it last happened, 0 if it never has. A clock behind its times (a game loaded from an earlier
	/// point) pulls them back to the clock and reads 0.
	[[nodiscard]] float SecondsSince(uint32_t clock);
	/// How many times a second it happened, over the times kept: the gaps between them over the time since the oldest.
	/// 0 with fewer than two kept or none of the clock passed since the oldest; a clock behind its times as above.
	[[nodiscard]] float PerSecond(uint32_t clock);

	/// All that the count keeps, as a player's profile stores it
	struct Kept
	{
		uint32_t total {0};
		float smoothedRate {0.0f};
		/// The slot of the next time to keep, and how many slots have a time
		uint32_t head {0};
		uint32_t kept {0};
		bool triggeredThisTurn {false};
		std::array<uint32_t, k_TimesKept> times {};
	};
	[[nodiscard]] Kept GetKept() const;
	/// The count as a profile stored it. A slot or a number of times past the last is kept to the slots there are.
	void SetKept(const Kept& kept);

private:
	void PullTimesBackTo(uint32_t clock);

	uint32_t _total {0};
	float _smoothedRate {0.0f};
	uint32_t _head {0};
	uint32_t _kept {0};
	bool _triggeredThisTurn {false};
	std::array<uint32_t, k_TimesKept> _times {};
};

/// The player's world camera, as the help sees it each turn
struct WorldCameraView
{
	/// The distance from the camera to the land at the middle of the screen, none when that line touches no land
	std::optional<float> centreDistance;
	/// How steeply the camera looks down, in radians: below zero, it looks up
	float pitch {0.0f};
};

/// The events that the view of the world camera gives each turn, in the order that they count:
/// - The land shows at the middle of the screen: k_CameraSeesLand. Then, if the land is near and the camera looks
///   steeply down, also k_CameraNearAndSteep.
/// - No land shows there and the camera looks up at the sky: k_CameraSeesSky.
[[nodiscard]] std::vector<uint32_t> CameraViewEvents(const WorldCameraView& view);

/// Every event's count and the profile's clock
class HelpProfile
{
public:
	/// An event happens; with it the events it also counts as. Nothing for a number past the last.
	void Trigger(uint32_t event);
	/// A game turn ends: each event may happen again, and the clock moves on
	void EndTurn();

	[[nodiscard]] uint32_t Clock() const { return _clock; }
	[[nodiscard]] const EventCount& Count(uint32_t event) const { return _counts.at(event); }
	[[nodiscard]] EventCount& Count(uint32_t event) { return _counts.at(event); }

	/// Whether the scripts may ask about an event: 1 to 48
	[[nodiscard]] static constexpr bool ScriptMayAsk(int32_t event) { return event > 0 && event < k_EventCount; }

private:
	std::array<EventCount, k_EventCount> _counts {};
	uint32_t _clock {0};
};

} // namespace openblack::help::profile
