/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The Creature Cave's pure logic: what its scrolls are told of the creature, the pages of its screen, the lessons it
// has learnt and the tattoo editor's edits

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "3D/TempleScrolls.h"
#include "Creature/CreatureCave.h"

using namespace openblack;

TEST(CreatureCave, FactsCutPercentagesShort)
{
	creature_cave::Snapshot snapshot;
	snapshot.name = u"Bob";
	snapshot.needs.age = 42;
	snapshot.needs.life = 0.879f;
	snapshot.needs.energy = 0.0399f;
	snapshot.needs.exhaustion = 0.5f;
	snapshot.needs.dehydration = 0.25f;
	snapshot.needs.poo = 0.999f;
	snapshot.strength = 0.42f;
	snapshot.fatness = 0.35f;
	snapshot.alignment = -3.0f;
	const auto facts = creature_cave::FactsOf(snapshot);
	EXPECT_EQ(facts.name, u"Bob");
	EXPECT_EQ(facts.age, 42);
	EXPECT_EQ(facts.health, 87);
	EXPECT_EQ(facts.energy, 3);
	EXPECT_EQ(facts.exhaustion, 50);
	EXPECT_EQ(facts.dehydration, 25);
	EXPECT_EQ(facts.poo, 99);
	EXPECT_EQ(facts.strength, 42);
	EXPECT_EQ(facts.fatness, 35);
	EXPECT_FLOAT_EQ(facts.alignment, -1.0f);
}

TEST(CreatureCave, FactsTellOfTheMind)
{
	using creature_desires::Desire;
	creature_cave::Snapshot snapshot;
	snapshot.likes = {
	    {.desire = Desire::Play, .level = 4}, {.desire = Desire::Hunger, .level = 1}, {.desire = Desire::Poo, .level = 0}};
	snapshot.attitudeToPlayer = 0.5f;
	snapshot.secondsAlone = creature_cave::k_AttentionFadeSeconds / 4.0f;
	snapshot.creaturesKnown = 2;
	const auto facts = creature_cave::FactsOf(snapshot);
	// The personality in the scroll's order, and only the desires it tells of
	ASSERT_EQ(facts.desires.size(), 2u);
	EXPECT_EQ(facts.desires[0].text, "HELP_TEXT_ROOM_PERSONALITY_GREEDY");
	EXPECT_EQ(facts.desires[0].attitude, 1);
	EXPECT_EQ(facts.desires[1].text, "HELP_TEXT_ROOM_PERSONALITY_PLAYFUL");
	EXPECT_EQ(facts.desires[1].attitude, 4);
	EXPECT_FLOAT_EQ(*facts.opinionOfGod, 0.5f);
	EXPECT_FLOAT_EQ(facts.attention, 0.75f);
	EXPECT_EQ(facts.known[0], 2);
	EXPECT_FALSE(creature_cave::PersonalityText(Desire::Poo).has_value());
}

TEST(CreatureCave, FactsTellOfSkillsAndMiracles)
{
	creature_cave::Snapshot snapshot;
	// The scroll tells of the skills after building
	snapshot.skills = {{"build", true}, {"field", true}, {"totem", false}, {"store", true}, {"fish", false}, {"dance", true}};
	snapshot.miracles = {{"none", 0}, {"fireball", 40}, {"lightning", 0}, {"heal", 100}};
	const auto facts = creature_cave::FactsOf(snapshot);
	EXPECT_EQ(facts.actionsKnown, (std::array<bool, 5> {true, false, true, false, true}));
	ASSERT_EQ(facts.miracles.size(), 2u);
	EXPECT_EQ(facts.miracles[0].text, "HELP_TEXT_CREATURE_LESSON_LEARN_MAGIC_ACTION_02");
	EXPECT_EQ(facts.miracles[0].percent, 40);
	EXPECT_EQ(facts.miracles[1].text, "HELP_TEXT_CREATURE_LESSON_LEARN_MAGIC_ACTION_04");
}

TEST(CreatureCave, PagesGoRound)
{
	using creature_cave::Page;
	EXPECT_EQ(creature_cave::Next(Page::Attributes), Page::ActionsLearnt);
	EXPECT_EQ(creature_cave::Next(Page::Tattoos), Page::Attributes);
	EXPECT_EQ(creature_cave::Previous(Page::Attributes), Page::Tattoos);
	EXPECT_EQ(creature_cave::ContentOf(Page::Attributes), TempleScrolls::Content::CreatureAttributes);
	EXPECT_EQ(creature_cave::ContentOf(Page::Mind), TempleScrolls::Content::CreatureMind);
	EXPECT_FALSE(creature_cave::ContentOf(Page::Tattoos).has_value());
	EXPECT_EQ(creature_cave::TitleName(Page::Tattoos), "HELP_TEXT_DIALOG_CREATURETATOO");
	for (size_t i = 0; i < creature_cave::k_PageCount; ++i)
	{
		EXPECT_FALSE(creature_cave::TitleName(static_cast<Page>(i)).empty());
	}
}

TEST(CreatureCave, LessonsAreTheStrongestOpinionsEachWay)
{
	const std::vector<creature_cave::Snapshot::Opinion> opinions {
	    {"eat villager", -0.8f}, {"dance", 0.3f}, {"sleep", 0.01f}, {"fish", 0.9f}, {"throw rock", -0.2f}, {"poo", 0.5f},
	};
	const auto lessons = creature_cave::LessonsOf(opinions, 2);
	ASSERT_EQ(lessons.toDo.size(), 2u);
	EXPECT_EQ(lessons.toDo[0].action, "fish");
	EXPECT_EQ(lessons.toDo[1].action, "poo");
	ASSERT_EQ(lessons.notToDo.size(), 2u);
	EXPECT_EQ(lessons.notToDo[0].action, "eat villager");
	EXPECT_EQ(lessons.notToDo[1].action, "throw rock");
}

TEST(CreatureCave, TattoosGoOnAndComeOff)
{
	creature_tattoo::Slots slots {};
	const glm::u8vec3 red {200, 0, 0};
	auto edit = creature_cave::Apply(slots, 2, 7, red);
	ASSERT_TRUE(edit.has_value());
	EXPECT_EQ(edit->slot, 0u);
	EXPECT_EQ(edit->tattoo.site, 2);
	EXPECT_EQ(edit->tattoo.design, 7);
	slots.at(edit->slot) = edit->tattoo;
	// The same symbol on the same place keeps its slot, recoloured
	edit = creature_cave::Apply(slots, 2, 7, glm::u8vec3 {0, 0, 200});
	ASSERT_TRUE(edit.has_value());
	EXPECT_EQ(edit->slot, 0u);
	// Another goes in the first empty slot
	edit = creature_cave::Apply(slots, 5, 3, red);
	ASSERT_TRUE(edit.has_value());
	EXPECT_EQ(edit->slot, 1u);
	// No such place or symbol
	EXPECT_FALSE(creature_cave::Apply(slots, 8, 3, red).has_value());
	EXPECT_FALSE(creature_cave::Apply(slots, 1, 16, red).has_value());
	// Taking it off empties its slot
	const auto removed = creature_cave::Remove(slots, 2);
	ASSERT_TRUE(removed.has_value());
	EXPECT_EQ(removed->slot, 0u);
	EXPECT_TRUE(removed->tattoo.Empty());
	EXPECT_FALSE(creature_cave::Remove(slots, 4).has_value());
}
