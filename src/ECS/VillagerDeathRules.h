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

#include "3D/AllMeshes.h"
#include "Enums.h"

/// The rules of what follows a villager's death: who hears of it, how its player's alignment moves, how its town
/// counts it, how the people about it notice, point, go to look and mourn, and how its soul rises and fades. Free of
/// the game's state so they can be tested on their own.
namespace openblack::villager_death
{

/// What the players at this machine hear of a death by each cause
struct DeathGuidance
{
	/// The advisors speak up when the player here caused it: a miracle, a throw or a drowning
	bool killerHears {false};
	/// The advisors warn of losing people when another player caused it and the town still has many
	bool warnsOfLosses {false};
	/// The guidance's line of a death in the village when it was the player's own villager and another caused it
	bool ownerHears {false};
};

[[nodiscard]] constexpr DeathGuidance GuidanceOf(DeathReason reason)
{
	switch (reason)
	{
	case DeathReason::Starving:
	case DeathReason::Chant:
	case DeathReason::Sacrifice:
	case DeathReason::Exhaustion:
		return {.killerHears = false, .warnsOfLosses = false, .ownerHears = true};
	case DeathReason::Spell:
		return {.killerHears = true, .warnsOfLosses = true, .ownerHears = true};
	case DeathReason::Animal:
		return {.killerHears = false, .warnsOfLosses = true, .ownerHears = true};
	case DeathReason::PlayerInteraction:
		return {.killerHears = true, .warnsOfLosses = true, .ownerHears = false};
	case DeathReason::PlayerInteractionDrown:
		return {.killerHears = true, .warnsOfLosses = false, .ownerHears = false};
	default:
		return {};
	}
}

/// Who at this machine hears of a death
enum class DeathVoice : uint8_t
{
	Nobody,
	/// The player here caused it: the advisors speak of it
	KillerAdvisors,
	/// Its villager was the player's here and the player here didn't cause it: the guidance's line
	OwnerLine,
};

/// The player put down for the death is heard first: when that is the player here, the owner's line never plays
[[nodiscard]] constexpr DeathVoice WhoHears(DeathReason reason, bool killerIsHere, bool ownerIsHere)
{
	const auto guidance = GuidanceOf(reason);
	if (killerIsHere)
	{
		return guidance.killerHears ? DeathVoice::KillerAdvisors : DeathVoice::Nobody;
	}
	return ownerIsHere && guidance.ownerHears ? DeathVoice::OwnerLine : DeathVoice::Nobody;
}

/// The guidance's lines of a death in the village: five samples of its bank, one picked at random
inline constexpr uint32_t k_DeathLineFirstSample = 1;
inline constexpr uint32_t k_DeathLineCount = 5;
inline constexpr uint32_t k_DeathLineVolume = 127;
inline constexpr uint32_t k_DeathLinePitch = 100;

/// How often a kind of guidance may be heard: at least so many turns apart, and more apart at random; the help level it
/// needs; and whether it is heard on the first land of a game for one player
struct GuidanceKind
{
	uint32_t leastGap {0};
	uint32_t helpLevel {0};
	bool onFirstLand {false};
};
/// The guidance's line of a death in the village: any turn but the one it was last heard, from the lowest help level on
inline constexpr GuidanceKind k_DeathLineKind {.leastGap = 0, .helpLevel = 1, .onFirstLand = true};

/// The spread of the random part of the gap between two of a kind: five times the least gap, shrunk by the cube of a
/// draw of 0 to 1, so that long gaps are rarer
[[nodiscard]] uint32_t GuidanceGapSpread(uint32_t leastGap, float draw);

/// Whether a kind of guidance may be heard now: the help level the player chose is high enough (none while the scripts
/// keep the help system off), it isn't kept quiet on the first land, and it was last heard longer ago than the gap
[[nodiscard]] bool GuidanceMayPlay(const GuidanceKind& kind, uint32_t helpLevel, bool firstLandForOnePlayer, uint32_t gap,
                                   uint32_t turnsSinceLastHeard);

/// How a death moves the alignment of the player whose villager it was: by that player's change for the cause, twice
/// as much for a child
[[nodiscard]] constexpr float AlignmentChange(float causeChange, bool child)
{
	return child ? causeChange * 2.0f : causeChange;
}

/// What a town keeps of its people's deaths
struct DeathCounts
{
	/// The turn of the latest death
	uint32_t lastDeathTurn {0};
	uint32_t total {0};
	/// By the player put down for each, by player number
	std::array<uint32_t, 8> byPlayer {};
	/// By what killed them
	std::array<uint32_t, static_cast<size_t>(DeathReason::_COUNT)> byReason {};
};

/// A town counts one more death, at a turn, put down to a player (by number) and a cause
void CountDeath(DeathCounts& counts, uint32_t turn, size_t killer, DeathReason reason);

/// No more than this many of the living react to one death at once
inline constexpr uint32_t k_MostMourners = 10;
/// A mourner walks to within this share past how far it keeps from the body, and looks from there
inline constexpr float k_CloseEnoughShare = 1.2f;
/// A walk shorter than this isn't taken: it looks from where it is
inline constexpr float k_LeastWalk = 1.0f;

/// How much a living thing cares about a death: all of the reaction's priority for a dead creature; for a person, none
/// to the people of a town that has a working graveyard, and none to the dead one itself or once enough react already
[[nodiscard]] constexpr uint8_t DeathPriority(bool deadIsCreature, bool reactorsTownHasWorkingGraveyard, bool reactorIsTheDead,
                                              uint32_t reactingNow, uint8_t priority)
{
	if (deadIsCreature)
	{
		return priority;
	}
	if (reactorsTownHasWorkingGraveyard)
	{
		return 0;
	}
	return !reactorIsTheDead && reactingNow < k_MostMourners ? priority : 0;
}

/// What a villager does first on noticing a death
enum class FirstNotice : uint8_t
{
	/// A dead creature is looked at as anything else is
	LookAtCreature,
	/// It points at the dead person, talking
	Point,
	/// It goes towards the dead person
	GoTowards,
};

/// A dead creature is looked at; a dead person is pointed at or gone to, even odds, by the game's draw of 0 or 1
[[nodiscard]] constexpr FirstNotice FirstNoticeOf(bool deadIsCreature, uint32_t coin)
{
	if (deadIsCreature)
	{
		return FirstNotice::LookAtCreature;
	}
	return coin == 0 ? FirstNotice::Point : FirstNotice::GoTowards;
}

/// A villager pointing at the dead goes towards it once it has faced it for more turns than two to twenty-two seconds
/// make, drawn afresh each turn from a draw of 0 to 20
[[nodiscard]] int32_t PointingLimit(float drawUpToTwenty, uint32_t turnsPerSecond);
/// A villager mourning stops once it has mourned for more turns than four to seven seconds make, drawn afresh each turn
/// from a draw of 0 to 3
[[nodiscard]] int32_t MourningLimit(float drawUpToThree, uint32_t turnsPerSecond);

/// How a villager going towards the dead goes on: it looks once close enough, else walks straight towards the body by
/// the distance past how far it keeps from it
struct Approach
{
	bool looks {false};
	float walk {0.0f};
};
[[nodiscard]] Approach ApproachToBody(float distance, float keepAway);

/// The clip a soul rises with: to heaven or to hell at even odds on a draw of 0 to 100, but always to heaven for a child
[[nodiscard]] constexpr AnimId SoulClip(float drawUpToHundred, bool child)
{
	if (child)
	{
		return AnimId::PDead1GotoHeaven;
	}
	return drawUpToHundred < 50.0f ? AnimId::PDead1GotoHeaven : AnimId::PDead1GotoHell;
}

/// A soul is drawn white and this see-through, fading out over its clip's last half second
inline constexpr uint8_t k_SoulAlpha = 105;
inline constexpr uint32_t k_SoulFadeMs = 500;
/// A soul goes this long before its clip ends
inline constexpr uint32_t k_SoulEndsEarlyMs = 110;

/// Whether a soul has gone, so far into its clip
[[nodiscard]] constexpr bool SoulGone(uint32_t timeMs, uint32_t clipMs)
{
	return timeMs + k_SoulEndsEarlyMs > clipMs;
}
/// How see-through a soul is, so far into its clip
[[nodiscard]] uint8_t SoulAlphaAt(uint32_t timeMs, uint32_t clipMs);

} // namespace openblack::villager_death
