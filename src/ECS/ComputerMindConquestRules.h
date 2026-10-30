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
#include <span>

/// The rules by which a computer god chooses a town to win from another player and finds how much it wants each way to
/// win it. Pure functions of what the god sees.
namespace openblack::computer_mind_rules
{

/// The players of a town's belief on a land, the neutral player last
constexpr size_t k_BeliefPlayers = 8;

/// The god holds the town less when the town believes in another player: the belief is multiplied by this
constexpr float k_HeldBeliefShare = 0.2f;
/// The hold of a town of the god's own is larger by this
constexpr float k_OwnTownHold = 0.75f;
/// The hold is never less than this
constexpr float k_LeastHold = 0.1f;
/// The lowest belief difference that the search for the strongest rival starts with
constexpr float k_NoRivalBelief = -9999.0f;
/// Each miracle of a town that the god does not have adds this to the wish for the town, and as much again when the god
/// asked for that miracle
constexpr float k_MissingMiracleWish = 0.5f;
/// A player that has more power than the god cuts the wish for its towns by this share at most
constexpr double k_RivalPowerShare = 0.85;
/// The distance from the hand at which a town is no longer near, in metres
constexpr float k_TownNearDistance = 250.0f;
/// The nearness of a town is half its own value plus this
constexpr float k_NearHalf = 0.5f;
constexpr double k_NearBase = 0.5;
/// In a town of its own, the god wants to feed it more the stronger its best rival is there: the ratio times this
constexpr float k_OwnTownFeedScale = 10.0f;

/// The town keeps the god's hold on it: from what the town believes in the strongest player against the god (the
/// player that owns it, or the best rival when the god owns it). A town of the god's own holds more. From 0.1 to 1.
[[nodiscard]] float TownHold(float rivalBelief, bool ownTown);
/// What the town believes in the strongest player against the god. In a town of another player, its belief in that
/// player. In a town of the god's own, its belief in the other player whose belief is the most above the god's, the
/// neutral player too; 0 when none is above -9999. The belief has a place for each player of the game, by number.
[[nodiscard]] float RivalBelief(std::span<const float> belief, size_t god, size_t owner);

/// What the god knows of a miracle that a town offers
struct TownMiracleFacts
{
	/// The god can cast the miracle already
	bool enabled {false};
	/// The god asked for the miracle at least one time
	bool requested {false};
};
/// How much the god wants the miracles of a town: 0.5 for each one it cannot cast, and 0.5 more when it asked for it
[[nodiscard]] float TownMiracleWish(std::span<const TownMiracleFacts> miracles);
/// The share of the god's people that the town has. When the god has no people, the miracle wish plus 1.
[[nodiscard]] float TownPeopleShare(uint32_t townPeople, float godPeople, float miracleWish);
/// The god's people as the god counts them
[[nodiscard]] float PeopleCount(uint32_t people);
/// How much a stronger owner of the town cuts the wish: 0 for the neutral player and for a god with no power, else the
/// owner's power over the god's power, not more than 1, times 0.85
[[nodiscard]] float RivalPowerCut(bool neutralOwner, float ownerPower, float godPower);
/// How near the town is to the hand: from 0.5 to 1
[[nodiscard]] float TownNearness(float distanceFromHand);
/// How much the god wants to win the town
[[nodiscard]] float TownWorth(float peopleShare, float miracleWish, float hold, float powerCut, float nearness);
/// The town that the god wants most: the first of the largest wishes above 0. None when no wish is above 0.
[[nodiscard]] std::optional<size_t> MostWantedTown(std::span<const float> worths);
/// The wish for the most wanted town, not more than 1
[[nodiscard]] float CappedWorth(float worth);

/// The strongest belief of the town in a player that is not the god and not an ally of the god, over the town's belief
/// in the god. 0 when the town has no belief in the god.
[[nodiscard]] float RivalBeliefRatio(float strongestRival, float godBelief);
/// How much the god wants to meet the needs of the town. In a town of another player, 1. In a town of its own, the
/// rival belief ratio times 10, not more than 1.
[[nodiscard]] float FeedFactor(bool ownTown, float rivalRatio);
/// How much the god wants to win the town by meeting its food or wood need: the town's raw desire times the feed
/// factor, not more than 1
[[nodiscard]] float NeedWish(float rawDesire, float feedFactor);

/// The god's creature walks this far for each unit of its size, in metres, before the reach constant
constexpr float k_CreatureHeightPerSize = 15.0f;
constexpr float k_CreatureReachPerHeight = 12.0f;
constexpr float k_CreatureLeastReach = 1000.0f;
/// The distance at which the god no longer sends its creature: from the creature's grown size
[[nodiscard]] float CreatureReach(float size);
/// How much the god wants its creature to go to a place: its freedom, its rest and the distance
[[nodiscard]] float CreatureHelpWish(float freedom, float exhaustion, float distance, float size);

/// The share of the miracles of a kind that the creature knows: 1 when there is none of the kind. The game divides
/// whole numbers, so the share is 0 only when the creature knows none of them, else 1.
[[nodiscard]] float KnownMiracleShare(uint32_t miracles, uint32_t unknown);

/// Which miracle the god casts to meet a town's needs
enum class NeedsMiracle : uint8_t
{
	None,
	Food,
	Wood,
};
/// The food miracle when its wish is larger than the wood wish, else the wood miracle when its wish is not 0
[[nodiscard]] NeedsMiracle ChooseNeedsMiracle(float foodWish, float woodWish);

/// The radius of the search for a thing to throw, in metres
constexpr float k_ThrowSearchRadius = 250.0f;
/// A place for a throw further than this from the hand is too far, in metres
constexpr float k_ThrowReach = 700.0f;
/// A second look at the impressive place must find more than this
constexpr float k_LeastImpressValue = 0.01f;
/// The hand goes this many times its speed toward the edge of the influence before it measures the pull
constexpr float k_ThrowPullSteps = 2.0f;
/// How much the god wants to throw a thing to impress the town: the value of the place, the town's boredom with things
/// that fly, and the pull of the place
[[nodiscard]] float ThrowWish(float pull, float placeValue, float boredom);
/// How much a thing impresses when it is thrown: its impressiveness plus its artefact modifier
[[nodiscard]] float ThrowScore(float impressiveness, float artefactModifier);
/// A rock, a bonfire or a dead tree wider than this cannot be thrown
constexpr float k_ThrowableRockRadius = 3.6f;

/// What kind of thing the god may throw, by its own rule
enum class ThrowKind : uint8_t
{
	/// Never thrown: the living, buildings, features and the like
	None,
	/// Loose things, standing trees and statics other than rocks: always
	Always,
	/// A pile: while it holds something
	Pile,
	/// A rock, a bonfire or a dead tree: while it is not too wide
	RockLike,
};
/// The player can throw the thing
[[nodiscard]] bool CanBeThrownByPlayer(ThrowKind kind, float radius, uint32_t amount);

/// The first and the last cell of a search around a map position along one axis, as the game finds them
struct CellSpan
{
	int16_t first {0};
	int16_t last {0};
};
[[nodiscard]] CellSpan SearchCells(int32_t fixed, float radius);
/// The search keeps a thing when its score is not less than the best, and its score times its nearness is more than the
/// best. The new best, or none.
[[nodiscard]] std::optional<float> SearchKeeps(float score, float distance, float radius, float best);

} // namespace openblack::computer_mind_rules
