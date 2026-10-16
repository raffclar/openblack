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

#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "Creature/CreatureIdleMind.h"
#include "Creature/CreaturePlanner.h"

/// The creature keeps one thing to do when it completes its current work, in place of the next decision. A magic
/// mushroom, a stroke and a slap each put a thing there.
namespace openblack::creature_follow_up
{

enum class FollowUp : uint8_t
{
	/// It ate a magic mushroom and becomes high
	BecomeHigh,
	/// It was stroked before it was old enough to learn, and shows that it is happy
	ShowHappy,
	/// It was slapped before it was old enough to learn, and is frightened of the player
	FrightenedOfPlayer,
	/// It was slapped, and shows how the slap made it feel
	ShowSlapFeeling,
	/// It learnt to eat what it holds when it was stroked. When the meal is done, it does again what it was stroked for,
	/// to the most useful thing of the same kind.
	DoLessonAgain,
	/// It cleared a thing out of its way, and walks on to where it went
	WalkOn,
	/// On the leash, it saw a nasty miracle that it knows. It casts that miracle at the thing that was nearest to the
	/// hand.
	CastOnLeash,
};

/// The creature eats a magic mushroom: it becomes high only when nothing else waits
[[nodiscard]] std::optional<FollowUp> AfterMushroom(std::optional<FollowUp> waiting);

/// Before the creature is old enough to learn, one stroke or slap in two puts a reaction in place of what waits. The roll
/// is from 0 to 1.
[[nodiscard]] std::optional<FollowUp> AfterYoungFeedback(std::optional<FollowUp> waiting, float feedback, uint32_t roll);

/// The action that a creature frightened of the player does: it runs away from the player one time in three, and it is
/// frightened on the spot two times in three. The roll is from 0 to 2.
[[nodiscard]] std::string_view FrightenedAction(uint32_t roll);

/// The clips of the reactions
constexpr size_t k_Angry = 53;
constexpr size_t k_Happy = 55;
constexpr size_t k_Sad = 56;
constexpr size_t k_Frightened = 61;
constexpr size_t k_Cry = 219;
constexpr size_t k_RudeGesture = 222;

/// A stroke before it can learn: it plays the happy clip
[[nodiscard]] std::vector<creature_mind::Step> ShowHappy();

/// What the creature feels about the player, which the slap reaction reads
struct Feelings
{
	float compassion {0.0f};
	float anger {0.0f};
};

/// A slap: one of four reactions, each as likely:
/// - when it feels no more anger than compassion, it is frightened; else it is angry or makes a rude gesture, each one
///   time in two;
/// - it is sad;
/// - it cries;
/// - it looks at the player for one to two seconds, with darting eyes and a sad face.
/// The random function gives a whole number below its argument. The chance is from 0 to 1.
[[nodiscard]] std::vector<creature_mind::Step> ShowSlapFeeling(const Feelings& feelings,
                                                               const std::function<uint32_t(uint32_t)>& random, float chance,
                                                               uint32_t turnsPerSecond);

/// The action that the creature does again after the meal: the action that it was stroked for, but it eats a thing alive
/// in place of a thing that it holds in its hand
[[nodiscard]] std::string_view ActionToDoAgain(std::string_view stroked);

/// Whether the creature can do an action again: the action had a thing that was not the creature itself, and the
/// creature does not copy the player. The thing can be gone.
[[nodiscard]] bool MayDoAgain(std::optional<uint32_t> actedOn, uint32_t self, bool copying);

/// The thing to do the action again to: the creature itself for an action that acts on nothing, else the most useful
/// thing of the same kind, the first of equals, and only a thing that is worth more than nothing
[[nodiscard]] std::optional<uint32_t> ThingToDoAgainTo(bool actsOnThing, uint32_t self,
                                                       std::span<const creature_planner::ObjectCandidate> sameKind);

/// A thing that the creature believes in, as the creature looks for a thing to do an action again to
struct DoAgainThing
{
	/// It is of the same kind as the thing that the creature acted on
	bool sameKind {false};
	/// The result of the action's own test of the thing; none when the action has no test
	std::optional<bool> passesTest;
	/// The action must not act on a thing that the creature holds
	bool notWhatItHolds {false};
	/// The creature holds the thing
	bool heldByItself {false};
};

/// Whether the creature can do the action again to the thing: a thing of the same kind that the action's test accepts.
/// An action with no test accepts all things of that kind.
[[nodiscard]] bool SuitsDoAgain(const DoAgainThing& thing);

} // namespace openblack::creature_follow_up
