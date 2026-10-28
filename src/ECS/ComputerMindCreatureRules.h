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

#include <glm/vec3.hpp>

#include "Enums.h"

/// The rules by which a computer god finds how much it wants to look after its creature. Pure functions of what the god
/// sees.
namespace openblack::computer_mind_rules
{

/// What the god knows of its creature when it finds how free the creature is for the god
struct CreatureFreedomFacts
{
	/// The god holds the creature on its leash
	bool leashed {false};
	/// The game turns since the god last tied the leash
	uint32_t turnsSinceTied {0};
	bool fighting {false};
	float life {1.0f};
	float exhaustion {0.0f};
	float energy {1.0f};
	/// The plan of the creature is for its desire to get its health back
	bool restoringHealth {false};
};

/// The turns after a tie of the leash during which the creature is not free for the god
constexpr float k_LeashBusyTurns = 450.0f;
/// The part of full freedom that each turn after those turns adds
constexpr float k_LeashFreedomPerTurn = 0.0022222223f;
/// Above this share of its life, energy or rest, the creature is fully fit for the god
constexpr float k_FitShare = 0.5f;

/// How free the creature is for the god to interact with, from 0 to 1. A creature off the leash is fully free. On the
/// leash it is free only some time after the last tie, when it does not fight, and less when it is weak, tired or
/// hungry. It is not free while it tries to get its health back.
[[nodiscard]] float CreatureFreedom(const CreatureFreedomFacts& facts);

/// The god sees that its creature starves: how much, from 0 to 1, by the creature's hunger
[[nodiscard]] float StarvingWish(float hunger);
/// The number that the god keeps for the starving creature: the wish in thousandths, made whole toward 0
[[nodiscard]] int32_t StarvingAmount(float wish);
/// At this share of its life or less, the creature is badly hurt
constexpr float k_DamagedLife = 0.2f;
/// At this share of illness or more, the creature is ill
constexpr float k_IllShare = 0.5f;
/// The wish to make the creature warmer or cooler is its desire times this
constexpr float k_WarmerShare = 0.5f;
constexpr float k_CoolerShare = 0.1f;

/// A journey further than this in metres is long
constexpr float k_LongJourney = 100.0f;
/// The creature runs faster than its run speed by this part on a long journey
constexpr float k_LongJourneyRun = 1.5f;
/// The range of the random number that the game draws for a short journey
constexpr float k_ShortJourneyDraw = 0.4f;

/// The journey of the creature from its place to a place is long
[[nodiscard]] bool LongJourney(glm::vec3 creature, glm::vec3 place);
/// The speed that the creature keeps for a journey, as a part of its top speed from 0 to 1: one and a half times its
/// run speed on a long journey, else the mean of its walk speed and its run speed
[[nodiscard]] float JourneySpeed(bool longJourney, float walk, float run);

/// How much the god wants to play with its creature. Each wish is a random number in a range, plus a least wish.
/// - To throw food at the creature
constexpr float k_ThrowFoodWishRange = 0.3f;
constexpr float k_ThrowFoodLeastWish = 0.2f;
/// - To slap the creature, with no least wish
constexpr float k_SlapWishRange = 0.1f;
/// - To stroke the creature
constexpr float k_StrokeWishRange = 0.1f;
constexpr float k_StrokeLeastWish = 0.1f;
/// - To tie the leash of the creature to a thing that moves, while the creature wears no leash
constexpr float k_LeashWishRange = 0.3f;
constexpr float k_LeashLeastWish = 0.2f;
/// - To cast a miracle on the creature for a laugh, while no miracle acts on it. This wish is not random.
constexpr float k_LaughWish = 0.035937004f;
static_assert(std::bit_cast<uint32_t>(k_LaughWish) == 0x3D1332AEu);

/// The hand throws food at the creature from this distance in metres
constexpr float k_ThrowFoodDistance = 20.0f;
/// The food falls at a random place in a square of this side in metres round the creature
constexpr float k_ThrowFoodScatter = 4.0f;
/// The god aims the food this many metres above the level of the sea for each unit of the size of the creature
constexpr float k_ThrowFoodHeightPerSize = 15.0f;

/// Where the hand goes before it throws food at the creature: on the line from the hand to the creature, at the throw
/// distance from the creature. A hand that is nearer than that distance goes back by that distance, away from the
/// creature. A hand at the place of the creature stays.
[[nodiscard]] glm::vec3 ThrowFoodHandPlace(glm::vec3 hand, glm::vec3 creature);
/// Where the god aims the food: the place of the creature, moved across by two random numbers in the scatter range,
/// at the height that the size of the creature gives above the level of the sea
[[nodiscard]] glm::vec3 ThrowFoodAim(glm::vec3 creature, float size, float firstDraw, float secondDraw);

/// The feedback that the creature feels when the god slaps it, and when the god strokes it
constexpr float k_SlapFeedback = -1.0f;
constexpr float k_StrokeFeedback = 1.0f;
/// The hand stays on the creature after a slap or a stroke for the least seconds plus a random number in the range
constexpr float k_FeedbackLeastSeconds = 3.0f;
constexpr float k_FeedbackSecondsRange = 2.0f;

/// The turns that the hand stays on the creature after a slap or a stroke, from the random number in the range
[[nodiscard]] uint32_t FeedbackTurns(float draw);

/// After a tie of the leash, the hand follows the thing for these turns, and then until it is this near in metres
constexpr uint32_t k_TieFollowTurns = 20;
constexpr float k_TieNearDistance = 30.0f;

/// The god looks at the hand this many times the height of the creature, plus the distance, for a leash to let go
constexpr float k_LetGoHeights = 10.0f;
constexpr float k_LetGoDistance = 200.0f;

/// The wish of the god to keep the leash of its creature tied at the start of a cast: the freedom of the creature
/// times the part of the miracle that the creature must still learn. The part that the creature knows is the times that
/// it saw the miracle over the times that it must see the miracle, but not more than 1.
[[nodiscard]] float CastLetGoWish(float freedom, float seen, float needed);
/// At the start of a cast, the god lets go of the thing that the leash of its creature is tied to:
/// - The freedom must be less than the wish, or the wish is not a number. A wish that is a number is never more than the
///   freedom, so this almost never occurs.
/// - The distance in metres from the hand to the creature must be less than the limit that the size gives.
[[nodiscard]] bool LetsGoAtCast(float freedom, float wish, float distance, float size);

/// The god takes off the evil leash of its creature, tied to another creature, when the other creature is out of the
/// influence of the god or its own creature tries to get its health back
[[nodiscard]] bool UntiesEvilLeash(float influence, bool restoringHealth);

/// How much the god wants to teach its creature an ordinary skill, while the creature does not know a skill that the god
/// teaches
constexpr float k_SkillLessonWish = 0.3f;
static_assert(std::bit_cast<uint32_t>(k_SkillLessonWish) == 0x3E99999Au);
/// For each kind of miracle, each time that the god sent its creature while the creature did not know the miracles of
/// that kind adds this much to the wish to teach a miracle of that kind
constexpr float k_UntaughtSendWish = 0.7f;
static_assert(std::bit_cast<uint32_t>(k_UntaughtSendWish) == 0x3F333333u);

/// How much the god wants to teach its creature a kind of miracle: the times that it sent the creature while the
/// creature did not know that kind, times the wish of one send, from 0 to 1
[[nodiscard]] float MiracleLessonWish(uint32_t untaughtSends);

/// What the god knows of an ordinary skill or a miracle when it looks for one to teach
struct LessonFacts
{
	/// The game's table lets a computer god teach it
	bool teachable {false};
	/// The creature knows it
	bool known {false};
	/// For a miracle: what the creature thinks that its player wants with the miracle
	CreatureDesires perceived {CreatureDesires::ToImpress};
	/// For a miracle: the seed of the miracle is a real one
	bool seedExists {false};
};

/// The kinds of miracle that the god teaches its creature
enum class LessonKind : uint8_t
{
	/// A miracle that the creature feels as angry
	Aggressive,
	/// A miracle that the creature feels as kind
	Compassionate,
	/// Any miracle
	Impressive,
};

/// The first ordinary skill, in the order of the game's table, that the god teaches and the creature does not know
[[nodiscard]] std::optional<size_t> FirstSkillToTeach(std::span<const LessonFacts> skills);
/// The first miracle, in the order of the magic types, of the kind that the god teaches, that the creature does not know
/// and that has a real seed
[[nodiscard]] std::optional<size_t> FirstMiracleToTeach(std::span<const LessonFacts> miracles, LessonKind kind);

/// The ordinary skill that has a lesson after the god teaches it: the use of the totem of a town
constexpr size_t k_TotemSkill = 2;
/// The god looks for a totem this many metres round the creature for the lesson of the totem
constexpr float k_LessonTotemReach = 250.0f;
/// The god raises the totem to this share of the people at worship for the lesson of the totem
constexpr float k_LessonTotemShare = 1.0f;
/// The actions of the game's table that the creature does in a lesson: it looks at the camera of its god, and it looks
/// at the totem
constexpr uint32_t k_LessonLookAtGodAction = 82;
constexpr uint32_t k_LessonLookAtTotemAction = 20;

} // namespace openblack::computer_mind_rules
