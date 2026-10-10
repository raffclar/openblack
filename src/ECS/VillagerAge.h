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

#include <algorithm>
#include <array>
#include <functional>
#include <optional>
#include <span>

namespace openblack::ecs::components
{
struct Villager;
}

/// How villagers age: their years counted in game turns, their size as they grow, growing up and dying of old age
namespace openblack::ecs::villager_age
{

/// A villager's year lasts this many turns, two and a half minutes at the game's ten turns a second
constexpr uint32_t k_TurnsPerYear = 1500;
/// A child's size is looked at four times a year
constexpr uint32_t k_GrowthTurns = k_TurnsPerYear / 4;
/// No adult is younger than this
constexpr uint32_t k_YoungestAdult = 18;
/// A newly made adult's size, before its own draw
constexpr float k_AdultStartScale = 0.9f;
/// An adult's size is drawn up to this far below its largest
constexpr float k_AdultScaleRange = 0.1f;
constexpr float k_AdultLargestScale = 1.05f;
/// A child grows this share of the way to its age's size at most each time it grows
constexpr float k_ChildGrowthShare = 0.75f;
/// The old are looked at for dying of old age once in this many turns, each at its own turn
constexpr uint32_t k_OldAgeCheckTurns = 800;
/// A new villager's food is drawn this far above the hungry mark
constexpr float k_NewbornFoodRange = 0.6f;
/// A new villager waits up to this many turns before it first decides what to do
constexpr uint32_t k_FirstDecisionTurns = 500;

/// Random numbers on the game's shared stream: a float from 0 up to a limit, and a whole number below a limit
using FloatRandom = std::function<float(float)>;
using IntRandom = std::function<uint32_t(uint32_t)>;

/// The ages of a kind of villager: a child under the first, old past the second, and none lives past the third
struct Ages
{
	uint32_t grownUp {0};
	uint32_t old {0};
	uint32_t oldest {0};
};

/// Its age in whole years: the turns since it was born over the turns of a year. The turn counts wrap, so a villager
/// made older than the game is born before turn 0.
[[nodiscard]] constexpr uint32_t AgeOf(uint32_t turn, uint32_t birthTurn)
{
	return (turn - birthTurn) / k_TurnsPerYear;
}

/// The turn it was born on to be so many years old now
[[nodiscard]] constexpr uint32_t BirthTurnFor(uint32_t turn, uint32_t age)
{
	return turn - (k_TurnsPerYear * age);
}

/// The age a villager is given: a child keeps its age, and an adult is at least the youngest adult's age
[[nodiscard]] constexpr uint32_t GivenAge(uint32_t age, uint32_t grownUp)
{
	return age < grownUp ? age : std::max(age, k_YoungestAdult);
}

[[nodiscard]] constexpr bool IsChildAge(uint32_t age, uint32_t grownUp)
{
	return age < grownUp;
}

/// What giving a villager a new age changes: the age it is given, whether that makes it a child, and the model it
/// changes to when it crosses between child and adult (none when it stays on the same side)
struct AgeSetting
{
	uint32_t age {0};
	bool child {false};
	/// Set when its model changes: true for the child's model, false for its adult one
	std::optional<bool> childModel;
};

/// A villager given an age: an adult is at least the youngest adult's age; its model changes only when the age it had
/// was on the other side of growing up
[[nodiscard]] constexpr AgeSetting SetAge(uint32_t age, uint32_t currentAge, uint32_t grownUp)
{
	AgeSetting setting {.age = GivenAge(age, grownUp)};
	setting.child = IsChildAge(setting.age, grownUp);
	if (setting.child != IsChildAge(currentAge, grownUp))
	{
		setting.childModel = setting.child;
	}
	return setting;
}

/// The size a villager starts at: a child its age's size from the table, an adult a little under full size
[[nodiscard]] float StartScale(uint32_t age, uint32_t grownUp, std::span<const float, 20> ageToScale);

/// Its size after it grows: a child a random way up to three quarters of the way to its age's size; an adult draws a
/// size, and if that is bigger than it is, it takes a second draw instead
[[nodiscard]] float GrownScale(uint32_t age, uint32_t grownUp, std::span<const float, 20> ageToScale, float scale,
                               const FloatRandom& random);

/// Whether a child grows on this turn
[[nodiscard]] constexpr bool IsGrowthTurn(uint32_t turn)
{
	return turn % k_GrowthTurns == 0;
}

/// Whether a villager is looked at for dying of old age in the checks it has now: its own turn of the eight hundred
/// came in the turns since its last checks. Its own turn is set by its number.
[[nodiscard]] constexpr bool IsOldAgeCheckDue(uint32_t turn, uint32_t number, uint32_t turnsSinceChecked)
{
	return (turn + number) % k_OldAgeCheckTurns < turnsSinceChecked;
}

/// Whether an old villager dies of old age now. Past old age, a draw from 0 to 1 cubed gives how much of the years
/// between old age and the oldest it may add to its own; it dies when that takes it past the oldest.
[[nodiscard]] bool DiesOfOldAge(uint32_t age, const Ages& ages, const FloatRandom& floatRandom, const IntRandom& intRandom);

/// What a villager is given as it is made
struct Newborn
{
	/// Its age once adults are raised to the youngest adult's
	uint32_t age {0};
	bool child {false};
	float scale {1.0f};
	/// How full it starts: a little over half full to a little over full
	float food {1.0f};
	/// The turn its needs count as last looked at: up to a check's worth of turns ago, so villagers made together are
	/// looked at on different turns
	uint32_t lastCheckTurn {0};
	/// The turns it stands before its first decision, 1 to 500
	uint16_t turnsUntilFirstDecision {1};
};

/// What the making of a villager needs from its kind
struct NewbornKind
{
	uint32_t grownUp {0};
	float hungryForFood {0.5f};
	uint32_t processChecksEvery {8};
	std::span<const float, 20> ageToScale;
};

/// Makes a villager of an age on a turn, with the game's draws in the game's order: its size, its food (drawn twice
/// unless the first comes out full), its last check (drawn twice unless the game is younger than the first draw) and
/// its wait before its first decision
[[nodiscard]] Newborn MakeNewborn(uint32_t age, uint32_t turn, const NewbornKind& kind, const FloatRandom& floatRandom,
                                  const IntRandom& intRandom);

/// A villager's age in whole years by the game's clock now
[[nodiscard]] uint32_t AgeNow(const components::Villager& villager);
/// Makes the villager so many years old by the game's clock now
void SetBirthTurnForAge(components::Villager& villager, uint32_t age);

} // namespace openblack::ecs::villager_age
