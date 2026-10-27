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

#include <bit>
#include <optional>
#include <span>

#include "3D/MapCoords.h"
#include "Enums.h"

/// The rules by which a computer god finds how much it wants a town's need or a miracle. Pure functions of what the god
/// sees.
namespace openblack::computer_mind_rules
{

/// The worship site's icon of a miracle, as the god sees it
struct IconFacts
{
	/// The icon belongs to a worship site
	bool site {false};
	/// The prayer power that the icon has stored
	float store {0.0f};
	/// The prayer power that the miracle costs to make
	float cost {0.0f};
	/// The site never runs out of prayer power
	bool infinite {false};
	/// The prayer power that the site keeps, and what its dancers give this turn and what was used of it
	float battery {0.0f};
	float available {0.0f};
	float used {0.0f};
	/// The prayer power that the dancers of the site chant each turn
	float capacity {0.0f};
};

/// The prayer power that the site reports when it never runs out
constexpr float k_InfiniteChants = 1.0e6f;
/// A small amount that keeps a share from a division by zero
constexpr float k_SmallChants = 0.001f;
/// How much each request for a miracle adds to the god's wish to charge it
constexpr float k_RequestShare = 0.01f;
/// The turns to charge a miracle at which the god no longer wants it
constexpr float k_MostChargeTurns = 2000.0f;

/// The part of the miracle's cost that the icon and its site could give now. With no dancers and less than the cost, 0.
[[nodiscard]] float ChargeShare(const IconFacts& icon);
/// The turns that the dancers need to charge the rest of the miracle: 0 when the site has enough now, -1 when the icon
/// has no site or its site has no dancers
[[nodiscard]] float TurnsToCharge(const IconFacts& icon);
/// How much the god wants to charge a miracle that it asked for this many times, 0 to 1
[[nodiscard]] float RequestShare(uint32_t requests);
/// How much the god wants to cast a miracle: the share of the charge that is there, less the further the charge is from
/// done, and the place's own pull. Never more than 1.
[[nodiscard]] float SpellValue(float charge, float turnsToCharge, float placePull);

/// The town that wants a thing most: the first with the largest desire over 0
[[nodiscard]] std::optional<size_t> MostWanting(std::span<const float> desires);

/// An icon of a worship site that the god could charge
struct ChargeCandidate
{
	/// The worshippers chant at the icon now
	bool charging {false};
	/// The prayer power that the icon has stored
	float store {0.0f};
	/// How many times the god asked for the miracle of the icon
	uint32_t requests {0};
};

/// The icon that the god wants to charge most, and how much: the first with the largest wish over 0 among the icons that
/// do not charge and have nothing stored
struct ChargeChoice
{
	size_t index {0};
	float value {0.0f};
};
[[nodiscard]] std::optional<ChargeChoice> ChooseCharge(std::span<const ChargeCandidate> icons);

/// The distance in metres at which a field no longer pulls the hand
constexpr float k_FieldReach = 250.0f;
/// A field must be wanted more than this before the god takes food from it
constexpr float k_LeastFieldWish = 0.1f;

/// How much the god wants to take food from a field: the share of the field's full crop that is in it, less the further
/// the field is from the hand, times the pull of the field's place
[[nodiscard]] float FieldWish(float food, float totalFood, float distanceFromHand, float placePull);
/// The field that the god wants most: the first with the largest wish over k_LeastFieldWish
[[nodiscard]] std::optional<size_t> MostWantedField(std::span<const float> wishes);

/// The share of the worship that a town wants filled at which the god sees it as fully short of worshippers, and the
/// share under which the god sees no need
constexpr float k_FullWorshipNeed = 0.7f;
constexpr float k_LeastWorshipNeed = 0.2f;
/// The dancers that each icon of a worship site wants
constexpr float k_DancersPerIcon = 4.0f;
/// A villager must be further than this, in metres, from the gate of the worship site before the god carries it there
constexpr float k_CarryToWorshipDistance = 50.0f;

/// How much a town needs more worshippers, from 0 to 1, from its people and those that worship or go to worship: the
/// cube of the share that does not worship. A need of k_FullWorshipNeed or more is full; under k_LeastWorshipNeed it is
/// none.
[[nodiscard]] float TownWorshipNeed(uint32_t population, uint32_t worshipping, uint32_t onTheWay);
/// How much a built worship site needs more dancers, from 0 to 1: one less the fourth power of the share of its dancers
/// against k_DancersPerIcon for each icon. A site with no icons needs none.
[[nodiscard]] float SiteDancerNeed(uint32_t dancers, uint32_t icons);

/// A circle of a player's own influence: its middle and how far it reaches, in metres
struct InfluenceCircle
{
	map_coords::MapCoords centre;
	float radius {0.0f};
};

/// The god measures its distance to the edge of its influence from this distance at first
constexpr float k_FarFromInfluence = 9999999.0f;
/// Under this pull a place outside the influence counts for nothing
constexpr double k_LeastPlacePull = 0.01;

/// The point on the edge of the player's influence nearest to a place, from the circles of the citadel first and then
/// of the towns in their order. The first circle of the smallest distance wins. None when there are no circles.
[[nodiscard]] std::optional<map_coords::MapCoords> NearestInfluenceEdge(const map_coords::MapCoords& place,
                                                                        std::span<const InfluenceCircle> circles);
/// How much a place outside the god's influence pulls, from 0 to 1: less the further it is from the edge of the
/// influence, out to the reach of the hand past the border. A place that pulls less than a hundredth gives 0.
[[nodiscard]] float OutsidePlacePull(float distanceToEdge, float reach);

/// How much the god wants to take food from a fish farm: the full share of food, less the further the farm is from the
/// hand, times the pull of the farm's place. A farm always gives its food, so its share is always 1.
[[nodiscard]] float FishFarmWish(float distanceFromHand, float placePull);

/// The food that a town wants more than it has, in whole food, below 0 when it has more than it wants
[[nodiscard]] int32_t FoodShortfall(float wanted, uint32_t held);

/// The god asks another town for this much of food or wood for each unit of the need of the town above
constexpr float k_AskPerNeed = 1000.0f;
/// The god asks another town only for more than this
constexpr float k_LeastAsk = 50.0f;

/// The food or the wood that the god asks another town for: the need of the node above times k_AskPerNeed
[[nodiscard]] float OtherTownAsk(float need);

/// Another town of the god with a storage pit, as the god sees it when the town above needs food or wood
struct GivingTown
{
	/// The food or the wood in the storage pit of the town
	uint32_t stock {0};
	/// What the town itself wants of the food or the wood, before its people act on it
	float rawDesire {0.0f};
	/// The pull of the place of the storage pit
	float pull {0.0f};
};

/// The town that the god takes the food or the wood from, and how much the god wants to take it
struct GivingChoice
{
	size_t index {0};
	float value {0.0f};
};

/// The town that the god takes food or wood from for the town above, from the need of the node above. A town gives only
/// when its pit has more than the ask, and when its own desire times its pull is less than the need. The town with the
/// least desire times pull wins, the first of equals. None when the ask is k_LeastAsk or less, or no town gives.
[[nodiscard]] std::optional<GivingChoice> ChooseGivingTown(std::span<const GivingTown> towns, float need);

/// The food or the wood that the hand takes from a container: what the step wants less what the pot in the hand holds,
/// no more than the container has. The hand takes nothing when the result is 0 or less.
[[nodiscard]] int32_t ContainerTake(int32_t wanted, std::optional<uint32_t> inPot, uint32_t inContainer);

/// The kinds of miracle that the god chooses among for an aim
enum class SpellKind : uint8_t
{
	/// A miracle that a creature feels as an angry one, to harm
	Aggressive,
	/// A miracle that a creature feels as a kind one, to help
	Compassionate,
	/// A miracle that impresses the people that see it
	Impressive,
};

/// The magic types that the god looks at when it chooses a miracle, from the first to the last
constexpr uint32_t k_FirstChoiceType = 1;
constexpr uint32_t k_LastChoiceType = 41;

/// How well a magic type suits an aim. An aggressive or a compassionate miracle suits it fully or not at all. The suit
/// of an impressive miracle is its impressive balance for a computer god.
[[nodiscard]] float KindScore(SpellKind kind, CreatureDesires perceived, float impressiveBalance);
/// The wish for a miracle of the kind, from how well it suits the aim and how much the god wants to cast it
[[nodiscard]] float KindWish(float wish, float score);

/// The god sees a creature as this many metres across for each unit of its size
constexpr float k_CreatureReachPerSize = 15.0f;
/// The god never casts an angry miracle at a creature nearer to its own creature than this, in metres
constexpr float k_LeastCreatureClearance = 30.0f;

/// The god does not cast an angry miracle at a creature that is too near to its own creature: nearer than twice their
/// two reaches, and never nearer than k_LeastCreatureClearance
[[nodiscard]] bool TooNearOwnCreature(float distance, float ownSize, float targetSize);

/// A building whose life share is under this is badly damaged
constexpr float k_BadDamage = 0.9f;

/// The building of the god's towns whose life share, against the pull of its place, is the smallest under 1: its index
/// and that share. The first of the smallest wins.
struct DamagedChoice
{
	size_t index {0};
	float share {0.0f};
};
[[nodiscard]] std::optional<DamagedChoice> MostDamaged(std::span<const float> shares);
/// How much the god wants to look at the damage of its towns, from the smallest share: all when the building is badly
/// damaged, else one less the square of the share
[[nodiscard]] float DamageNeed(float share);

/// What a town keeps of the last attack of one player: the turn of the attack, the harm of the player's miracles and
/// the two parts of the harm of the things that hit the town
struct TownAttack
{
	uint32_t turn {0};
	float miracleHarm {0.0f};
	float thingHarm {0.0f};
	float otherThingHarm {0.0f};
};

/// The number of players whose attacks a town keeps
constexpr size_t k_TownAttackPlayers = 7;
/// The share of the memory of an attack that the town loses each turn: it forgets the attack after 600 turns
constexpr float k_AttackMemoryShare = std::bit_cast<float>(0x3ADA740Eu);

/// How much the town fears an attack: the attack that is the most recent, with its harm
struct TownThreat
{
	/// 1 just after the attack, down to 0 when the town forgets it
	float fear {0.0f};
	float miracleHarm {0.0f};
	/// The two parts of the harm of the things that hit the town, added
	float thingHarm {0.0f};
};
/// The threat to a town from the attack that it fears most. None when that attack did no harm.
[[nodiscard]] std::optional<TownThreat> ThreatOf(std::span<const TownAttack, k_TownAttackPlayers> attacks, uint32_t turn);

/// The shield that protects a town against a threat: the miracle shield when the harm of miracles is the larger, else
/// the physical shield
[[nodiscard]] MagicType ShieldAgainst(const TownThreat& threat);
/// The shield that the god looks for when it checks its towns: the physical shield when the harm of things is the larger,
/// else the miracle shield
[[nodiscard]] MagicType ShieldToCheck(const TownThreat& threat);

/// The value of a place of a town for an impressive miracle: the villagers that can see it, less the further it is from
/// the hand
constexpr float k_ImpressReach = 1000.0f;
[[nodiscard]] float ImpressPlaceValue(uint32_t villagers, float distanceFromHand);
/// The place that has the largest value, the first of the largest. None when there are no places.
[[nodiscard]] std::optional<size_t> BestImpressPlace(std::span<const float> values);
/// The town's people are too tired to look at a miracle when their wish to sleep is over this
constexpr double k_TooSleepyToImpress = 0.5;

/// How far from a target the hand puts a thing, in metres: the target's radius times a random 1.25 to 1.5, and the
/// thing's own radius. The random part is from 0 to 0.25.
constexpr double k_LeastCarryFactor = 1.25;
constexpr float k_CarryRandom = 0.25f;
[[nodiscard]] float CarryDistance(float random, float targetRadius, float thingRadius);

/// Two miracles of the same magic type are at the same place when each of their x and z is less than this apart, in map
/// units
constexpr int32_t k_SameCastPlace = 0x2000;
[[nodiscard]] bool SameCastPlace(const map_coords::MapCoords& a, const map_coords::MapCoords& b);

/// The radius of a physical shield that the god casts over a thing, against the thing's own radius
constexpr float k_PhysicalShieldScale = 1.2f;

} // namespace openblack::computer_mind_rules
