/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <bit>
#include <limits>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindCreatureRules.h"
#include "ECS/ComputerMindDomains.h"
#include "ECS/ComputerMindRules.h"
#include "ECS/Systems/Implementations/ComputerMindRuleTable.h"

using namespace openblack;
using namespace openblack::computer_mind;
using namespace openblack::computer_mind_rules;
namespace computer_world = openblack::ecs::systems::computer_world;

TEST(ComputerMindCreaturePlay, OwnsItsKindsOfNodes)
{
	EXPECT_EQ(DomainOf(NodeKind::ActionSlapCreature), MindDomain::CreaturePlay);
	EXPECT_EQ(DomainOf(NodeKind::ActionTeachCreatureNormalAction), MindDomain::CreaturePlay);
	EXPECT_EQ(DomainOf(NodeKind::ObjectObjectToLeashTo), MindDomain::CreaturePlay);
}

TEST(ComputerMindCreaturePlay, WishesHaveTheValuesOfTheGame)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(k_ThrowFoodWishRange), 0x3E99999Au);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_ThrowFoodLeastWish), 0x3E4CCCCDu);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_SlapWishRange), 0x3DCCCCCDu);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_LaughWish), 0x3D1332AEu);
}

TEST(ComputerMindCreaturePlay, HandGoesToTheThrowDistanceFromTheCreature)
{
	// A far hand goes along the line to 20 metres from the creature
	const auto far = ThrowFoodHandPlace({0.0f, 10.0f, 0.0f}, {100.0f, 10.0f, 0.0f});
	EXPECT_FLOAT_EQ(far.x, 80.0f);
	EXPECT_FLOAT_EQ(far.y, 10.0f);
	EXPECT_FLOAT_EQ(far.z, 0.0f);
	// A near hand goes back by 20 metres, away from the creature
	const auto near = ThrowFoodHandPlace({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 5.0f});
	EXPECT_FLOAT_EQ(near.z, -20.0f);
	// A hand at the place of the creature stays
	const auto same = ThrowFoodHandPlace({3.0f, 4.0f, 5.0f}, {3.0f, 4.0f, 5.0f});
	EXPECT_EQ(same, glm::vec3(3.0f, 4.0f, 5.0f));
	// The game makes the sums in its order
	const glm::vec3 hand {12.3f, 45.6f, 78.9f};
	const glm::vec3 creature {101.7f, 22.2f, 33.3f};
	const float dx = creature.x - hand.x;
	const float dy = creature.y - hand.y;
	const float dz = creature.z - hand.z;
	const float distance = std::sqrt((dy * dy + dx * dx) + dz * dz);
	const float inverse = 1.0f / distance;
	const float along = distance - 20.0f;
	const auto place = ThrowFoodHandPlace(hand, creature);
	EXPECT_EQ(place.x, dx * inverse * along + hand.x);
	EXPECT_EQ(place.z, dz * inverse * along + hand.z);
}

TEST(ComputerMindCreaturePlay, FoodFallsNearTheCreatureAtAHeightFromItsSize)
{
	const auto aim = ThrowFoodAim({100.0f, 30.0f, 200.0f}, 2.0f, 0.5f, 3.5f);
	EXPECT_FLOAT_EQ(aim.x, 98.5f);
	// The height does not come from the land: it is 15 metres for each unit of size
	EXPECT_FLOAT_EQ(aim.y, 30.0f);
	EXPECT_FLOAT_EQ(aim.z, 201.5f);
}

TEST(ComputerMindCreaturePlay, TouchLastsThreeToFiveSeconds)
{
	EXPECT_EQ(FeedbackTurns(0.0f), 30u);
	EXPECT_EQ(FeedbackTurns(1.99f), 49u);
	EXPECT_EQ(FeedbackTurns(2.0f), 50u);
}

TEST(ComputerMindCreaturePlay, CastAlmostNeverLetsGoOfTheLeash)
{
	// The wish is the freedom times the part that the creature must still learn
	EXPECT_EQ(CastLetGoWish(0.5f, 1.0f, 4.0f), 0.5f * (1.0f - 0.25f));
	EXPECT_EQ(CastLetGoWish(0.5f, 3.0f, 0.0f), 0.0f);
	EXPECT_EQ(CastLetGoWish(0.5f, 9.0f, 4.0f), 0.0f);
	// No sightings of a miracle that needs none gives a wish that is not a number
	EXPECT_TRUE(std::isnan(CastLetGoWish(0.5f, 0.0f, 0.0f)));
	// A wish that is a number never lets go
	EXPECT_FALSE(LetsGoAtCast(0.5f, 0.375f, 0.0f, 1.0f));
	EXPECT_FALSE(LetsGoAtCast(0.0f, 0.0f, 0.0f, 1.0f));
	// A wish that is not a number lets go when the hand is near enough
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_TRUE(LetsGoAtCast(0.5f, nan, 349.0f, 1.0f));
	EXPECT_FALSE(LetsGoAtCast(0.5f, nan, 350.0f, 1.0f));
	EXPECT_TRUE(LetsGoAtCast(0.5f, 0.6f, 0.0f, 1.0f));
}

TEST(ComputerMindCreaturePlay, EvilLeashComesOffOutOfInfluenceOrForHealth)
{
	EXPECT_FALSE(UntiesEvilLeash(0.5f, false));
	EXPECT_TRUE(UntiesEvilLeash(0.0f, false));
	EXPECT_TRUE(UntiesEvilLeash(std::numeric_limits<float>::quiet_NaN(), false));
	EXPECT_TRUE(UntiesEvilLeash(0.5f, true));
}

TEST(ComputerMindCreaturePlay, EachKindOfThePartHasItsRules)
{
	computer_world::RuleTable table {};
	computer_world::RegisterCreaturePlayRules(table);
	const auto rule = [&table](NodeKind kind) { return table.at(static_cast<size_t>(kind)); };
	for (const auto kind : {NodeKind::ActionThrowFoodatCreature, NodeKind::ActionSlapCreature, NodeKind::ActionStrokeCreature,
	                        NodeKind::ActionAttachLeashtoMovingObject, NodeKind::ObjectCastSpellOnMyCreatureForALaugh})
	{
		EXPECT_NE(rule(kind).intensity, nullptr) << static_cast<int>(kind);
		EXPECT_NE(rule(kind).perform, nullptr) << static_cast<int>(kind);
	}
	// The laugh action has only a wish, and its object carries out the cast. No node lists the object to leash to.
	for (const auto kind : {NodeKind::ActionCastSpellOnMyCreatureForALaugh, NodeKind::ObjectObjectToLeashTo})
	{
		EXPECT_NE(rule(kind).intensity, nullptr) << static_cast<int>(kind);
		EXPECT_EQ(rule(kind).perform, nullptr) << static_cast<int>(kind);
	}
}

TEST(ComputerMindCreatureTeach, WishesHaveTheValuesOfTheGame)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(k_SkillLessonWish), 0x3E99999Au);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_UntaughtSendWish), 0x3F333333u);
	EXPECT_EQ(k_TotemSkill, 2u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_LessonTotemReach), 0x437A0000u);
	EXPECT_EQ(k_LessonLookAtGodAction, 82u);
	EXPECT_EQ(k_LessonLookAtTotemAction, 20u);
}

TEST(ComputerMindCreatureTeach, EachSendAddsToTheWishForAMiracleUpToOne)
{
	EXPECT_EQ(MiracleLessonWish(0), 0.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(MiracleLessonWish(1)), 0x3F333333u);
	EXPECT_EQ(MiracleLessonWish(2), 1.0f);
	EXPECT_EQ(MiracleLessonWish(std::numeric_limits<uint32_t>::max()), 1.0f);
}

TEST(ComputerMindCreatureTeach, TheGodTeachesTheFirstSkillThatTheCreatureDoesNotKnow)
{
	std::vector<LessonFacts> skills(6);
	EXPECT_FALSE(FirstSkillToTeach(skills).has_value());
	// Only a skill that the table lets the god teach counts
	skills[2].teachable = true;
	EXPECT_EQ(FirstSkillToTeach(skills), 2u);
	skills[4].teachable = true;
	EXPECT_EQ(FirstSkillToTeach(skills), 2u);
	skills[2].known = true;
	EXPECT_EQ(FirstSkillToTeach(skills), 4u);
	skills[4].known = true;
	EXPECT_FALSE(FirstSkillToTeach(skills).has_value());
}

TEST(ComputerMindCreatureTeach, TheGodTeachesTheFirstMiracleOfTheKindWithARealSeed)
{
	const LessonFacts angry {.teachable = true, .perceived = CreatureDesires::Anger, .seedExists = true};
	const LessonFacts kind {.teachable = true, .perceived = CreatureDesires::Compassion, .seedExists = true};
	std::vector<LessonFacts> miracles {angry, kind, angry, kind};
	EXPECT_EQ(FirstMiracleToTeach(miracles, LessonKind::Aggressive), 0u);
	EXPECT_EQ(FirstMiracleToTeach(miracles, LessonKind::Compassionate), 1u);
	EXPECT_EQ(FirstMiracleToTeach(miracles, LessonKind::Impressive), 0u);
	// A known miracle, a miracle that the table does not let the god teach, and a miracle with no real seed do not count
	miracles[0].known = true;
	miracles[1].teachable = false;
	EXPECT_EQ(FirstMiracleToTeach(miracles, LessonKind::Aggressive), 2u);
	EXPECT_EQ(FirstMiracleToTeach(miracles, LessonKind::Compassionate), 3u);
	EXPECT_EQ(FirstMiracleToTeach(miracles, LessonKind::Impressive), 2u);
	miracles[3].seedExists = false;
	EXPECT_FALSE(FirstMiracleToTeach(miracles, LessonKind::Compassionate).has_value());
	// A miracle that the creature feels as impressive is only an impressive lesson
	miracles = {{.teachable = true, .perceived = CreatureDesires::ToImpress, .seedExists = true}};
	EXPECT_FALSE(FirstMiracleToTeach(miracles, LessonKind::Aggressive).has_value());
	EXPECT_FALSE(FirstMiracleToTeach(miracles, LessonKind::Compassionate).has_value());
	EXPECT_EQ(FirstMiracleToTeach(miracles, LessonKind::Impressive), 0u);
}

TEST(ComputerMindCreatureTeach, EachKindOfTheLessonsHasItsRules)
{
	computer_world::RuleTable table {};
	computer_world::RegisterCreaturePlayRules(table);
	const auto rule = [&table](NodeKind kind) { return table.at(static_cast<size_t>(kind)); };
	// The ordinary skill and the kind miracle start lessons
	for (const auto kind : {NodeKind::ActionTeachCreatureNormalAction, NodeKind::ObjectTeachCreatureCompassionateMagic})
	{
		EXPECT_NE(rule(kind).intensity, nullptr) << static_cast<int>(kind);
		EXPECT_NE(rule(kind).perform, nullptr) << static_cast<int>(kind);
	}
	// The angry and the impressive miracles are always 0 under the lesson, so only their wishes are made
	for (const auto kind : {NodeKind::ActionTeachCreatureAggressiveMagic, NodeKind::ActionTeachCreatureCompassionateMagic,
	                        NodeKind::ActionTeachCreatureImpressiveMagic, NodeKind::ObjectTeachCreatureAggressiveMagic,
	                        NodeKind::ObjectTeachCreatureImpressiveMagic})
	{
		EXPECT_NE(rule(kind).intensity, nullptr) << static_cast<int>(kind);
		EXPECT_EQ(rule(kind).perform, nullptr) << static_cast<int>(kind);
	}
}
