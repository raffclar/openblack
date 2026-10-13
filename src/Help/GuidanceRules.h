/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>

/// The guidance: the advisors' running commentary on what happens in the game (towns attacked, buildings wrecked, the
/// player turning from good or evil, the moon) and the villagers' voices of guidance. Each kind of guidance waits a
/// while after it was last heard before it is heard again, longer for most at random, needs a help level, and many are
/// kept quiet on the first land of the story.
namespace openblack::guidance
{

/// The kinds of guidance, each with its own wait since it was last heard
enum class Kind : uint8_t
{
	TownDesireSound = 0,
	ResourceDropSound = 1,
	TownAttackedSound = 2,
	TotemRaisedSound = 3,
	NewDiscipleSound = 4,
	BeliefSound = 5,
	/// Not traced to what plays it
	Sound6 = 6,
	DeathInVillageSound = 7,
	/// Any of the advisors' remarks below: each must wait for this one as well as for its own
	AnyRemark = 8,
	OurTownAttacked = 9,
	OurCreatureAttacked = 10,
	OurCreatureAttackingThem = 11,
	LosingVillagers = 12,
	AttackingTown = 13,
	LowOnFood = 14,
	LowOnWood = 15,
	InjuredPeople = 16,
	LowOnPeople = 17,
	VillagersUnhappy = 18,
	LosingBelief = 19,
	OtherVillages = 20,
	CreatureFight = 21,
	GeneralBad = 22,
	GeneralGood = 23,
	KillingPeople = 24,
	GoodBeingEvil = 25,
	EvilBeingGood = 26,
	WorshippersDying = 27,
	VeryGood = 28,
	VeryEvil = 29,
	DestroyBuilding = 30,
	/// A remark said with no text
	NoText = 31,
	/// The remarks said at most once a land
	OneOff = 32,
};
constexpr size_t k_KindCount = 33;

struct KindRule
{
	/// The least number of game turns between two of this kind
	uint32_t leastGap;
	/// The help level the player must have chosen
	uint32_t helpLevel;
	/// Heard on the first land of the story too
	bool onFirstLand;
};
[[nodiscard]] const KindRule& Rule(Kind kind);

/// The random part of the wait: five times the least gap, shrunk by the cube of a draw of 0 to 1 so long waits are
/// rarer, cut to a whole number of turns. The wait is the least gap plus a local random number below this.
[[nodiscard]] uint32_t GapSpread(uint32_t leastGap, float draw);

/// Whether a kind may be heard now: the player's help level is enough (none while the scripts keep the help system
/// off), it isn't kept quiet on the first land of a one-player story, and it was heard longer ago than the wait
[[nodiscard]] bool MayPlay(const KindRule& rule, uint32_t helpLevel, bool firstLandOfStory, uint32_t wait,
                           uint32_t turnsSinceHeard);

/// When a kind counts as last heard on a new land: a random number of turns (below its least gap) before now, at most
/// back to the land's first turn
[[nodiscard]] constexpr uint32_t FirstStamp(uint32_t turn, uint32_t draw)
{
	return turn > draw ? turn - draw : 0;
}

/// The advisors' remarks a kind says, its texts in the game's info (one row of 34, ended by a 0)
constexpr size_t k_RemarkSlots = 34;
using RemarkTexts = std::array<uint32_t, k_RemarkSlots>;
constexpr size_t k_RemarkRows = 22;
/// The remarks' row of a kind, for the kinds of advisors' remarks
[[nodiscard]] std::optional<size_t> RemarkRow(Kind kind);
/// How many of a row's texts may be picked: those before its first 0, but never the 34th, which is never said even in a
/// full row
[[nodiscard]] size_t RemarkChoices(const RemarkTexts& texts);

/// How far the player's alignment is drifting: it keeps 0.95 of the drift so far and adds this turn's change
[[nodiscard]] constexpr float Drift(float drift, float change)
{
	return 0.95f * drift + change;
}
/// The remark on the player's alignment, if any: once the drift is more than twice the most an alignment can change in
/// a turn, a player already past 0.4 (or 0.75 when drifting the same way) towards good or evil is told off for
/// drifting the other way, or remarked on for going further. Positive alignment is good. Going further picks the
/// remark the other way round: a good player going better hears the very evil remarks.
[[nodiscard]] std::optional<Kind> AlignmentRemark(float drift, float alignment, float maxChangePerTurn);

/// The remarks said at most once a land, each with its chance when its moment comes
enum class OneOff : uint8_t
{
	NotVeryGood = 0,
	UpTooLate = 1,
	Sunrise = 2,
	QuitTheYacking = 3,
	/// The full moon at night, when the computer's clock says it is night as well: "There's a full moon tonight...".
	/// The original game had this remark and the next the wrong way round, the full moon warning of miracles nearby and
	/// another player's hand speaking of the full moon; each now says its own (a deviation the user chose)
	FullMoon = 4,
	/// Another player's hand close to the player's: "I detect Miracles being used near you..."
	HandNearby = 5,
	EnemyLeaving = 6,
};
constexpr size_t k_OneOffCount = 7;
struct OneOffRule
{
	uint32_t text;
	float chance;
};
[[nodiscard]] const OneOffRule& Rule(OneOff oneOff);
/// A one-off is said when a draw of 0 to 1 is at most its chance
[[nodiscard]] constexpr bool OneOffSaid(float draw, float chance)
{
	return draw <= chance;
}

/// Whether the computer's clock says it is night: before six in the morning, from eleven at night, and also the quarter
/// hour from 20:45 (as the game reckons it)
[[nodiscard]] bool ClockSaysNight(int hour, int minute);

/// The moon is looked at again this many turns after it was remarked on
constexpr int32_t k_MoonRemarkWait = 600000;
struct MoonLook
{
	bool remark;
	/// The turns until the moon is looked at again
	int32_t wait;
};
/// A look at the moon during the game's night: with the computer's clock at night and the real moon's phase (0 to 2 pi,
/// pi full) within 0.15 of full, the full moon remark; otherwise looked at again the cube of the phase's distance from
/// full times 12000 turns later (at once for a waxing moon, whose distance is negative)
[[nodiscard]] MoonLook LookAtMoon(float phase, bool clockSaysNight);

} // namespace openblack::guidance
