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
#include <numbers>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>

#include "Enums.h"

/// How villagers make children. A couple at home at bed time may make one when the town feels sleep with all its heart
/// and wants children past what its people already do for it, while the home has room for another child. Out of doors
/// two villagers of an age and of opposite sexes meet, kiss, and the woman falls pregnant three times in ten. She goes
/// home and stays there until her time comes, when she gives birth wherever she is: a boy of one of the six jobs of her
/// tribe half the time, else a girl like her, who joins her home and follows her about. A breeder disciple walks about a
/// home of its town drawing villagers of the other sex to it, until the town is too far over full.
namespace openblack::ecs::villager_breeding
{

/// Random numbers on the game's shared stream: a float from 0 up to a limit, and a whole number below a limit
using FloatRandom = std::function<float(float)>;
using IntRandom = std::function<uint32_t(uint32_t)>;

/// Two villagers meeting to make love walk to stand this far apart, and kiss once nearer than this edge to edge
constexpr float k_MeetingGap = 0.562f;
constexpr float k_KissingGap = 0.57f;
/// The game's turns in one of its days, which set how long a birth takes
constexpr float k_TurnsPerDay = 36000.0f / 365.25f;
/// A birth takes a quarter of a day, then up to a day more
constexpr float k_BirthLeastShare = 0.25f;
/// A newborn is a year old
constexpr uint32_t k_NewbornAge = 1;
/// A newborn is a boy when a draw below this comes no higher than its mother's kind's boy chance
constexpr uint32_t k_NewbornDraw = 100;
/// A boy takes one of the jobs after the housewife's: forester, fisherman, farmer, shepherd, leader or trader
constexpr uint32_t k_NewbornJobs = 6;
/// A baby cries one of so many cries as it is born
constexpr uint32_t k_BabyCries = 10;
/// A breeder breeds while its town's adults are fewer over its places than this many times the children's allowance
constexpr float k_BreederAdultShare = 4.0f;
/// A breeder walks about the nearest home of its town within this many metres, else about its town's food store
constexpr float k_BreederHomeReach = 300.0f;
/// About a home, out from its door by an eighth of a turn either side, half its size to its size away
constexpr float k_BreederFan = std::numbers::pi_v<float> / 4.0f;
constexpr float k_BreederHomeShare = 0.5f;
/// About a store, three to five metres away
constexpr float k_BreederStoreLeast = 3.0f;
constexpr float k_BreederStoreRange = 2.0f;
/// A breeder ambles up to this far from its spot, pausing 30 to 69 turns
constexpr float k_BreederAmble = 8.0f;
constexpr uint32_t k_BreederPauseLeast = 30;
constexpr uint32_t k_BreederPauseRange = 40;
/// A child following its mother makes for a point this far from her
constexpr float k_NearMother = 5.0f;

/// Whether a villager of an age is of an age to make love, from the start age up to the stop age
[[nodiscard]] bool IsSexuallyActive(uint32_t age, uint32_t startAge, uint32_t stopAge);
/// Whether a villager is pregnant: a female with turns of pregnancy left
[[nodiscard]] bool IsPregnant(bool female, int16_t pregnancy);
/// A woman is a female who has grown up
[[nodiscard]] bool IsWoman(bool female, bool child);

/// What decides whether a woman conceives at home
struct HomeConception
{
	bool pregnant {false};
	bool hasTown {false};
	bool hasAbode {false};
	bool sexuallyActive {false};
	/// How much her town wants children past what makes its people act on it, none or more
	float childrenWanted {0.0f};
	/// The children of her home and its pregnant women, against the children it has room for
	uint32_t children {0};
	uint32_t pregnantWomen {0};
	uint32_t childRoom {0};
};
/// A woman conceives at home when she isn't already pregnant, has a town and a home, is of an age, her town wants
/// children, and her home has room for one more child counting those on the way
[[nodiscard]] bool ConceivesAtHome(const HomeConception& facts);

/// Going to bed may make a child only once the town feels sleep with all its heart and the villager is of an age
[[nodiscard]] bool BedTimeMayConceive(bool hasTown, float townSleep, bool sexuallyActive);
/// One of the people of a home, in the home's order (the newest first): a male or not, and whether in at home
struct Housemate
{
	bool male {false};
	bool atHome {false};
};
/// Who gets a chance to conceive as a villager goes to bed, by place in its home: a woman herself, once, when any male
/// is at home; a man, every female at home. Males and females here are of any age, as in the game.
[[nodiscard]] std::vector<size_t> BedTimeConceptions(bool bedderFemale, size_t bedder, std::span<const Housemate> home);

/// A pregnancy after turns gone by, and whether the birth has come
struct Countdown
{
	int16_t pregnancy {0};
	bool birthDue {false};
};
[[nodiscard]] Countdown CountDown(int16_t pregnancy, uint32_t turnsGone);
/// The turns a birth takes once it starts: a quarter of a day, plus a whole number of turns up to a day, plus one
[[nodiscard]] uint16_t BirthTurns(float turnsPerDay, const IntRandom& random);
/// The job a newborn boy takes, none for a girl, who is her mother's kind
[[nodiscard]] std::optional<VillagerNumber> NewbornJob(uint32_t boyChance, const IntRandom& random);

/// The turns two villagers make love for: half of the mate's time and twice the villager's own, and one more
[[nodiscard]] uint16_t LoveTurns(uint32_t mateTime, uint32_t ownTime);
/// Where a villager walks to meet another: towards it, stopping a gap short of halfway between them
[[nodiscard]] glm::vec2 MeetingPoint(glm::vec2 from, glm::vec2 to, float gap);
/// Whether two things touch within a gap, edge to edge
[[nodiscard]] bool Touching(float centreDistance, float radiusA, float radiusB, float gap);
/// Whether a villager that has reached its meeting point waits for its partner instead of starting the kiss: when the
/// partner, near enough to kiss, is still walking to its own point. The game started the kiss at once, the partner
/// stopping short wherever it was, often too far off for the kiss to meet (deviations list)
[[nodiscard]] bool WaitsForPartner(bool partnerTouching, bool partnerWalkingToMeet);
/// Whether a villager leaving one of the love-making states stops making love with its partner: only on going on to a
/// state that is an end in itself (not on its way somewhere first, as when it walks to meet the partner again) and
/// isn't one of the love-making states
[[nodiscard]] bool LeavesLove(bool nextIsLoveState, bool nextIsFinal);

/// What a breeder weighs of its town: its children and adults against their places in its homes
struct TownHeadcount
{
	uint32_t children {0};
	uint32_t childPlaces {0};
	uint32_t adults {0};
	uint32_t adultPlaces {0};
};
/// A breeder breeds while its town's children are fewer over their places than the allowance, and its adults fewer
/// over theirs than four times it
[[nodiscard]] bool BreederMayBreed(const TownHeadcount& town, float overAllowance);
/// A breeder's spot by a home: out from the home's door, its way from the home turned by up to an eighth either side
[[nodiscard]] glm::vec2 BreederSpotByHome(glm::vec2 home, glm::vec2 door, float homeRadius, const IntRandom& intRandom,
                                          const FloatRandom& floatRandom);
/// A breeder's spot without a home near: three to five metres from its town's food store
[[nodiscard]] glm::vec2 BreederSpotByStore(glm::vec2 store, const FloatRandom& random);
/// A breeder's amble about its spot, and the turns it then pauses
struct Amble
{
	glm::vec2 goal {0.0f};
	uint16_t pause {0};
};
[[nodiscard]] Amble BreederAmble(glm::vec2 spot, const FloatRandom& floatRandom, const IntRandom& intRandom);
/// A point a child makes for near its mother, or its home without her
[[nodiscard]] glm::vec2 NearMother(glm::vec2 centre, const FloatRandom& random);

} // namespace openblack::ecs::villager_breeding
