/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <variant>

#include <gtest/gtest.h>

#include "Help/HelpQuery.h"

using namespace openblack;
using namespace openblack::help_query;
namespace messages = openblack::help_messages;

namespace
{
size_t SetOf(const Answer& answer)
{
	const auto* set = std::get_if<SaySet>(&answer);
	return set != nullptr ? set->set : 0;
}

SayTexts TextsOfAnswer(const Answer& answer)
{
	const auto* say = std::get_if<SayTexts>(&answer);
	return say != nullptr ? *say : SayTexts {.texts = {.first = 0, .last = 0}};
}

Person Believer()
{
	return {.players = true};
}

Thing PersonWith(Person person, Texts texts = {.first = 3015, .last = 3015})
{
	return {.person = person, .texts = texts};
}
} // namespace

TEST(HelpQuery, ScriptControlledPersonFirst)
{
	auto person = Believer();
	person.scriptControlled = true;
	person.dead = true;
	const auto say = TextsOfAnswer(About(PersonWith(person)));
	EXPECT_EQ(say.texts.first, k_ScriptControlledText);
	EXPECT_EQ(say.texts.last, k_ScriptControlledText);
}

TEST(HelpQuery, PeopleStockAnswersInOrder)
{
	auto person = Believer();
	person.dead = true;
	person.child = true;
	EXPECT_EQ(TextsOfAnswer(About(PersonWith(person))).texts.first, k_DeadText);
	person.dead = false;
	person.vagrant = true;
	EXPECT_EQ(SetOf(About(PersonWith(person))), messages::k_Child);
	person.child = false;
	person.players = false;
	EXPECT_EQ(SetOf(About(PersonWith(person))), messages::k_Vagrant);
	person.vagrant = false;
	EXPECT_EQ(SetOf(About(PersonWith(person))), messages::k_NonBeliever);
	person.players = true;
	const auto say = TextsOfAnswer(About(PersonWith(person)));
	EXPECT_EQ(say.texts.first, 3015u);
	EXPECT_EQ(say.script, messages::Script::JustTalkWithText);
}

TEST(HelpQuery, WorkingDiscipleIsToldByItsTexts)
{
	auto person = Believer();
	person.workingDisciple = true;
	person.players = false;
	person.child = true;
	EXPECT_EQ(TextsOfAnswer(About(PersonWith(person, {.first = 3130, .last = 3130}))).texts.first, 3130u);
}

TEST(HelpQuery, OthersWonderAndNoTexts)
{
	EXPECT_EQ(SetOf(About({.othersWonder = true, .texts = {.first = 2829, .last = 2829}})), messages::k_OthersWonder);
	EXPECT_EQ(SetOf(About({})), messages::k_NotInterested);
}

TEST(HelpQuery, TownCentreAndNeeds)
{
	const auto town = TextsOfAnswer(About({.texts = {.first = 2816, .last = 2826}, .role = Role::Town}));
	EXPECT_EQ(town.script, messages::Script::Town);
	EXPECT_EQ(town.texts.last, 2826u);

	const auto need = TextsOfAnswer(
	    About({.texts = {.first = 3083, .last = 3083}, .role = Role::TownNeed, .needText = 3006, .rawDesire = 2.5f}));
	EXPECT_EQ(need.script, messages::Script::TownNeeds);
	ASSERT_TRUE(need.townNeed.has_value());
	EXPECT_EQ(need.townNeed->first, 3006u);
	EXPECT_FLOAT_EQ(need.townNeed->second, 100.0f);

	const auto noTown =
	    TextsOfAnswer(About({.texts = {.first = 3083, .last = 3083}, .role = Role::TownNeed, .needText = 3006}));
	EXPECT_EQ(noTown.texts.first, 3006u);
	EXPECT_EQ(noTown.script, messages::Script::JustTalkWithText);
}

TEST(HelpQuery, Ground)
{
	// The height is kept as a float but compared with the shallow water height as a double, so 0.15f is just above it
	EXPECT_EQ(SetOf(AboutGround({.hasPoint = true, .altitude = 0.15f, .onLand = true})), messages::k_NotInterested);
	EXPECT_EQ(SetOf(AboutGround({})), messages::k_NotInterested);
	EXPECT_EQ(TextsOfAnswer(AboutGround({.hasPoint = true, .fishFarm = true, .altitude = 0.0f, .onLand = true})).texts.first,
	          k_FishFarmText);
	EXPECT_EQ(TextsOfAnswer(AboutGround({.hasPoint = true, .altitude = 0.1f, .onLand = true})).texts.first, k_ShallowWaterText);
	// Off the land's blocks low ground is told by its material
	EXPECT_EQ(
	    TextsOfAnswer(AboutGround({.hasPoint = true, .altitude = 0.0f, .material = {.first = 2948, .last = 2948}})).texts.first,
	    2948u);
	EXPECT_EQ(TextsOfAnswer(
	              AboutGround({.hasPoint = true, .altitude = 5.0f, .onLand = true, .material = {.first = 2961, .last = 2961}}))
	              .texts.first,
	          2961u);
	EXPECT_EQ(SetOf(AboutGround({.hasPoint = true, .altitude = 5.0f, .onLand = true})), messages::k_NotInterested);
}

TEST(HelpQuery, VillagerTexts)
{
	const Texts kind {.first = 2767, .last = 2767};
	EXPECT_EQ(VillagerTexts(3024, VillagerDisciple::Farmer, kind).first, 3024u);
	EXPECT_EQ(VillagerTexts(0, VillagerDisciple::Farmer, kind).first, 3130u);
	EXPECT_EQ(VillagerTexts(0, VillagerDisciple::Missionary, kind).first, 3136u);
	EXPECT_EQ(VillagerTexts(0, VillagerDisciple::Craftsman, kind).first, 3129u);
	EXPECT_EQ(VillagerTexts(0, VillagerDisciple::Trader, kind).first, 2770u);
	EXPECT_EQ(VillagerTexts(0, VillagerDisciple::Worship, kind).first, 2767u);
	EXPECT_EQ(VillagerTexts(0, std::nullopt, kind).first, 2767u);
}

TEST(HelpQuery, CreatureTexts)
{
	EXPECT_EQ(CreatureTexts({.first = 3096, .last = 3096}, 2, true).first, 3096u);
	const auto others = CreatureTexts({.first = 3096, .last = 3096}, 2, false);
	EXPECT_EQ(others.first, 3097u);
	EXPECT_EQ(others.last, 3097u);
	EXPECT_EQ(CreatureTexts({.first = 2425, .last = 2425}, 13, false).first, 2425u);
	EXPECT_EQ(CreatureTexts({.first = 3092, .last = 3092}, 12, false).first, 3092u);
}

TEST(HelpQuery, FeaturePileAndSeedTexts)
{
	EXPECT_EQ(FeatureTexts(FeatureInfo::ArkDryDock, 1.0f, {.first = 1, .last = 1}).first, 2865u);
	EXPECT_EQ(FeatureTexts(FeatureInfo::ArkDryDock, 0.5f, {.first = 1, .last = 1}).first, 1u);
	EXPECT_EQ(FoodPileTexts(false, {.first = 2870, .last = 2870}).first, k_FoodPileText);
	EXPECT_EQ(FoodPileTexts(true, {.first = 2870, .last = 2870}).first, 2870u);
	const auto seed = SeedTexts({.first = 2711, .last = 0}, {.first = 5961, .last = 5961});
	EXPECT_EQ(seed.first, 2711u);
	EXPECT_EQ(seed.last, 5961u);
}
