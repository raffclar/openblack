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

#include <optional>
#include <span>
#include <vector>

/// The rules of worship for a town's people: how many of them the town's worship share wants at its worship site, who
/// goes first and who is sent home first, whether they dance or hide on arriving, what the
/// chanting costs them, when they eat at the site, when they go home, and how much food the site wants from the town.
/// Free of the game's state so they can be tested on their own.
namespace openblack::ecs::worshipper_rules
{

/// A worshipper this near the site's gate, in metres, has arrived
inline constexpr float k_ArriveDistance = 10.0f;
/// The candidates' town is judged from this much further out than its own distance to the site, in metres
inline constexpr float k_TownDistanceSlack = 100.0f;
/// A worship disciple sent off raises its town's share to take in itself and a little more
inline constexpr float k_DiscipleShareSlack = 1.4f;
/// A hiding worshipper is at the site's door this near it, in metres
inline constexpr float k_HidingReach = 1.0f;

/// The counts a town's worship is judged by
struct Headcount
{
	/// Its adults and children
	uint32_t population {0};
	/// Its people counted at worship (dancing, hiding, eating or resting at the site)
	uint32_t worshipping {0};
	/// Its people on their way to the site
	uint32_t onTheWay {0};
	/// Its people at the site asking to go home
	uint32_t askingToGoHome {0};
	/// The share of its people its totem wants at worship, 0 to 1
	float share {0.0f};
};

/// How many more of its people a town wants at worship (fewer than none: too many are there), and whether it already
/// has as many as its share asks for although it wants more, which happens only when some ask to go home
struct Needed
{
	int32_t count {0};
	bool alreadyEnough {false};
};

/// How many people the share asks for: none without a share, at least one with any
[[nodiscard]] int32_t Target(uint32_t population, float share);
/// How many more people a town wants at worship. Those on their way are counted with the worshippers only when asked,
/// and those asking to go home are made up for only when asked.
[[nodiscard]] Needed WorshippersNeeded(const Headcount& town, bool countOnTheWay, bool makeUpForThoseGoingHome);

/// The share a worship disciple's town takes once the disciple is sent off, so that the share keeps it: none while the
/// town still wants more, or when it has no people
[[nodiscard]] std::optional<float> ShareAfterDisciple(int32_t needed, uint32_t worshipping, uint32_t onTheWay,
                                                      uint32_t population);

/// How fit a villager is to be sent to worship, or sent home first when too many are there: those near the site and
/// those full of life first
[[nodiscard]] float CandidateKey(float villagerToSite, float townToSite, float life);
/// The order candidates are tried in, by their keys: fittest first when sending people to worship, least fit first when
/// sending them home. Equal keys keep the order they were found in.
[[nodiscard]] std::vector<size_t> CandidateOrder(std::span<const float> keys, bool fittestFirst);
/// Whether a candidate may be sent to worship: any when low life is ignored, otherwise only those above the threshold
[[nodiscard]] bool LifeAllowsWorship(bool ignoreLowLife, float life, float damageThresholdToGoHome);

/// Whether a villager can get to its site: one further than villagers go to worship gets there only through a teleport
/// stone of its player's, and one of no player's not at all checks
[[nodiscard]] bool CanReachSite(float distance, float maxDistance, bool hasPlayer, bool stoneFound);

/// What a worshipper does on reaching the site's gate
enum class Arrival
{
	/// Still too far: it walks on to the gate
	WalkOn,
	/// It joins the dance
	Dance,
	/// The dance has as many dancers as can be seen: it worships hidden in the site
	Hide,
};
[[nodiscard]] Arrival OnReachingGate(float distanceToGate, uint32_t dancers, uint32_t maxDancersVisible);

/// The life a turn's worship costs a worshipper: what each dancer chanted last turn at its site, at the villager's rate
[[nodiscard]] float ChantDamage(float chantsPerDancer, float chantLifeRate);

/// Whether a hungry worshipper eats at its site: hungry, with a meal's worth of food in the site's pot
[[nodiscard]] bool EatsAtSite(float belly, float hungryForFood, uint32_t siteFood, uint32_t meal);

/// What a worshipper does about going home, each turn it worships
enum class GoingBack
{
	/// It worships on
	Stay,
	/// Its town can't worship there any more (no site, no standing town centre, or the site isn't its player's): it
	/// decides afresh what to do
	Decide,
	/// More of its town worship than the share wants, and the site lets it go: it goes home
	GoHome,
};
[[nodiscard]] GoingBack CheckGoingBack(bool townCanWorship, int32_t neededNotCountingOnTheWay, bool mayGoHome);

/// The food the dancers want for their dinner: what each dancing villager's belly lacks, at the villager's dinner size
[[nodiscard]] float DancersFoodNeed(std::span<const float> bellies, std::span<const int32_t> dinnerSizes);
/// The food a site wants its town to bring: what its dancers want less what it has and what is on its way, at least none
[[nodiscard]] int32_t FoodNeededFromTown(float dancersNeed, uint32_t inPot, int32_t onTheWay);
/// The food a town can spare for worship: its storage pit's food beyond its people's dinner, at least none
[[nodiscard]] int32_t FoodSpareForWorship(std::optional<uint32_t> pitFood, float foodForDinner);
/// The food a town sends its site: what the site wants, but none when the town can't spare it all
[[nodiscard]] int32_t FoodForSiteIfEnough(int32_t wanted, int32_t spare);

} // namespace openblack::ecs::worshipper_rules
