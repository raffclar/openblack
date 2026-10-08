/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "Animals/BirdRules.h"

using namespace openblack;
using namespace openblack::animals::birds;

namespace
{
/// A roll that always gives the same whole number, below whatever it is asked for
auto Always(uint32_t value)
{
	return [value](uint32_t below) { return below == 0 ? 0 : value % below; };
}
} // namespace

TEST(BirdRules, OnlyTheLandsOwnKindsAreLandBirds)
{
	for (const auto type :
	     {AnimalInfo::Crow, AnimalInfo::Dove, AnimalInfo::Swallow, AnimalInfo::Pigeon, AnimalInfo::Seagull, AnimalInfo::Bat})
	{
		EXPECT_TRUE(IsLandBird(type));
	}
	// The vulture's kind is never made, and the miracles' and the unused temple rows aren't the land's
	for (const auto type : {AnimalInfo::Vulture, AnimalInfo::CitadelDove, AnimalInfo::CitadelBat, AnimalInfo::SpellDove,
	                        AnimalInfo::SpellBat, AnimalInfo::Sheep})
	{
		EXPECT_FALSE(IsLandBird(type));
	}
}

TEST(BirdRules, MostKindsFlapOnAZeroAndGlideOtherwise)
{
	EXPECT_EQ(FlyingClip(AnimalInfo::Crow, Always(0)), AnimId::CrowFlap);
	EXPECT_EQ(FlyingClip(AnimalInfo::Crow, Always(1)), AnimId::CrowGlide);
	EXPECT_EQ(FlyingClip(AnimalInfo::Dove, Always(0)), AnimId::DoveFlap);
	EXPECT_EQ(FlyingClip(AnimalInfo::Dove, Always(1)), AnimId::DoveGlide);
	EXPECT_EQ(FlyingClip(AnimalInfo::Pigeon, Always(0)), AnimId::PigeonFlap);
	EXPECT_EQ(FlyingClip(AnimalInfo::Pigeon, Always(1)), AnimId::PigeonGlide);
	EXPECT_EQ(FlyingClip(AnimalInfo::Seagull, Always(0)), AnimId::SeagullFlap);
	EXPECT_EQ(FlyingClip(AnimalInfo::Seagull, Always(1)), AnimId::SeagullGlide);
}

TEST(BirdRules, ASwallowChoosesAmongThreeAndABatAlwaysFlaps)
{
	EXPECT_EQ(FlyingClip(AnimalInfo::Swallow, Always(0)), AnimId::SwallowFlap);
	EXPECT_EQ(FlyingClip(AnimalInfo::Swallow, Always(1)), AnimId::SwallowEraticglide);
	EXPECT_EQ(FlyingClip(AnimalInfo::Swallow, Always(2)), AnimId::SwallowCalmglide);
	// A bat doesn't roll at all
	std::vector<uint32_t> asked;
	const auto clip = FlyingClip(AnimalInfo::Bat, [&asked](uint32_t below) {
		asked.push_back(below);
		return 0u;
	});
	EXPECT_EQ(clip, AnimId::BatFlap);
	EXPECT_TRUE(asked.empty());
}

TEST(BirdRules, EachKindLiesDeadInItsOwnClip)
{
	EXPECT_EQ(DeadClip(AnimalInfo::Crow), AnimId::CrowGlide);
	EXPECT_EQ(DeadClip(AnimalInfo::Dove), AnimId::DoveFlap);
	EXPECT_EQ(DeadClip(AnimalInfo::Pigeon), AnimId::PigeonGlide);
	EXPECT_EQ(DeadClip(AnimalInfo::Seagull), AnimId::SeagullGentleflap);
	EXPECT_EQ(DeadClip(AnimalInfo::Swallow), AnimId::SwallowEraticglide);
	EXPECT_EQ(DeadClip(AnimalInfo::Bat), AnimId::BatGlide);
	EXPECT_FALSE(DeadClip(AnimalInfo::Cow).has_value());
}

TEST(BirdRules, LandBirdsBankSlowerThanTheMiraclesBirds)
{
	EXPECT_FLOAT_EQ(BankOf(AnimalInfo::Seagull).angle, 0.5f);
	EXPECT_FLOAT_EQ(BankOf(AnimalInfo::Seagull).seconds, 2.0f);
	EXPECT_FLOAT_EQ(BankOf(AnimalInfo::SpellDove).angle, 0.5f);
	EXPECT_FLOAT_EQ(BankOf(AnimalInfo::SpellDove).seconds, 0.5f);
	EXPECT_FLOAT_EQ(BankOf(AnimalInfo::SpellBat).seconds, 0.5f);
}

TEST(BirdRules, TheLeaderGivesUpItsLegOnceItsStayTimeIsUp)
{
	EXPECT_FALSE(LeaderPicksNewLeg(99, 100));
	EXPECT_TRUE(LeaderPicksNewLeg(100, 100));
	EXPECT_TRUE(LeaderPicksNewLeg(101, 100));
}

TEST(BirdRules, ABirdWithNoAgeGetsARandomOne)
{
	EXPECT_EQ(ScriptBirdAge(13, true, Always(7)), 13u);
	std::vector<uint32_t> asked;
	const auto roll = [&asked](uint32_t below) {
		asked.push_back(below);
		return below - 1;
	};
	EXPECT_EQ(ScriptBirdAge(0, true, roll), 24u);
	EXPECT_EQ(ScriptBirdAge(0, false, roll), 44u);
	EXPECT_EQ(asked, (std::vector<uint32_t> {20, 40}));
	EXPECT_EQ(ScriptBirdAge(0, true, Always(0)), 5u);
}

TEST(BirdRules, AFlocksOwnHeightGoesBeforeItsKinds)
{
	EXPECT_FLOAT_EQ(BaseHeight(0.0f, 40.0f), 40.0f);
	EXPECT_FLOAT_EQ(BaseHeight(25.0f, 40.0f), 25.0f);
}

TEST(BirdRules, AGoodTempleHasDovesAndAnEvilOneBats)
{
	EXPECT_EQ(TempleBirdKind(0.3f), AnimalInfo::Dove);
	EXPECT_EQ(TempleBirdKind(0.0f), AnimalInfo::Dove);
	EXPECT_EQ(TempleBirdKind(-0.01f), AnimalInfo::Bat);
}

TEST(BirdRules, ATemplesBirdsGrowWithHowFarItsPlayerIsFromNeutral)
{
	EXPECT_EQ(TempleBirdCount(1.0f, 20, true), 20u);
	EXPECT_EQ(TempleBirdCount(-1.0f, 20, true), 20u);
	EXPECT_EQ(TempleBirdCount(0.5f, 20, true), 10u);
	// Cut down to a whole bird: none until a twentieth of the way
	EXPECT_EQ(TempleBirdCount(0.049f, 20, true), 0u);
	EXPECT_EQ(TempleBirdCount(-0.07f, 20, true), 1u);
	EXPECT_EQ(TempleBirdCount(1.0f, 20, false), 0u);
}

TEST(BirdRules, TheTemplesFlockMovesOneBirdAtATime)
{
	EXPECT_EQ(StepTowards(3, 5), TempleFlockStep::AddOne);
	EXPECT_EQ(StepTowards(5, 5), TempleFlockStep::None);
	EXPECT_EQ(StepTowards(6, 0), TempleFlockStep::RemoveOne);
}

TEST(BirdRules, TheTemplesBirdsFlyTenMetresOverItsModel)
{
	EXPECT_FLOAT_EQ(TempleFlockHeight(1.0f, 30.0f), 40.0f);
	EXPECT_FLOAT_EQ(TempleFlockHeight(1.5f, 20.0f), 40.0f);
}
