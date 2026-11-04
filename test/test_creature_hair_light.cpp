/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureHairLight.h"

using namespace openblack::creature_hair_light;

namespace
{
constexpr BodyDraw k_UsualWithHair {.environmentMapped = false, .keepsHairLight = true};
constexpr BodyDraw k_UsualWithoutHair {.environmentMapped = false, .keepsHairLight = false};
constexpr BodyDraw k_Mapped {.environmentMapped = true, .keepsHairLight = false};
constexpr Colours k_Film {.light = glm::vec3(200.0f, 180.0f, 160.0f), .added = glm::vec3(10.0f, 20.0f, 30.0f)};
} // namespace

TEST(CreatureHairLight, TheStartColoursAreAWhiteLightAndNoAddedColour)
{
	EXPECT_EQ(k_Start.light, glm::vec3(255.0f));
	EXPECT_EQ(k_Start.added, glm::vec3(0.0f));
}

TEST(CreatureHairLight, AMappedBodyTakesTheColoursOfTheLastUsualHairBeforeIt)
{
	const std::vector<BodyDraw> draws {k_UsualWithHair, k_UsualWithHair, k_UsualWithoutHair, k_Mapped, k_UsualWithHair};
	const auto plan = PlanFrame(draws);
	ASSERT_EQ(plan.hair.size(), draws.size());
	EXPECT_FALSE(plan.hair[0].has_value());
	EXPECT_FALSE(plan.hair[2].has_value());
	ASSERT_TRUE(plan.hair[3].has_value());
	// The body without hair keeps no colours, so the second body gives them
	EXPECT_EQ(*plan.hair[3], Source {FromDraw {1}});
	EXPECT_EQ(plan.last, 4u);
}

TEST(CreatureHairLight, AMappedBodyBeforeAllUsualHairTakesTheColoursOfAnEarlierFrame)
{
	const std::vector<BodyDraw> draws {k_UsualWithoutHair, k_Mapped, k_UsualWithHair, k_Mapped};
	const auto plan = PlanFrame(draws);
	ASSERT_TRUE(plan.hair[1].has_value());
	EXPECT_EQ(*plan.hair[1], Source {FromFrameBefore {}});
	ASSERT_TRUE(plan.hair[3].has_value());
	EXPECT_EQ(*plan.hair[3], Source {FromDraw {2}});
}

TEST(CreatureHairLight, AMappedBodyKeepsNoColoursForTheBodiesAfterIt)
{
	const std::vector<BodyDraw> draws {k_UsualWithHair, k_Mapped, k_Mapped};
	const auto plan = PlanFrame(draws);
	EXPECT_EQ(*plan.hair[1], Source {FromDraw {0}});
	EXPECT_EQ(*plan.hair[2], Source {FromDraw {0}});
	EXPECT_EQ(plan.last, 0u);
}

TEST(CreatureHairLight, AFrameWithNoUsualHairKeepsWhatTheFrameBeforeKept)
{
	const std::vector<BodyDraw> draws {k_Mapped, k_UsualWithoutHair};
	const auto plan = PlanFrame(draws);
	EXPECT_FALSE(plan.last.has_value());
	const std::vector<std::optional<Colours>> known(draws.size());
	EXPECT_EQ(Carry(plan, known, Carried {k_Start}), Carried {k_Start});
	EXPECT_EQ(Carry(plan, known, Carried {Kept {}}), Carried {Kept {}});
	EXPECT_EQ(Carry(plan, known, Carried {k_Film}), Carried {k_Film});
}

TEST(CreatureHairLight, AFrameKeepsTheColoursOfItsLastUsualHair)
{
	const std::vector<BodyDraw> draws {k_UsualWithHair, k_UsualWithHair};
	const auto plan = PlanFrame(draws);
	// The renderer finds the colours of a hair at its place on the GPU, so it keeps them there
	const std::vector<std::optional<Colours>> placed(draws.size());
	EXPECT_EQ(Carry(plan, placed, Carried {k_Start}), Carried {Kept {}});
	// A hair with colours of its own gives those colours
	std::vector<std::optional<Colours>> known(draws.size());
	known[1] = k_Film;
	EXPECT_EQ(Carry(plan, known, Carried {Kept {}}), Carried {k_Film});
	known[1].reset();
	known[0] = k_Film;
	EXPECT_EQ(Carry(plan, known, Carried {k_Start}), Carried {Kept {}});
}

TEST(CreatureHairLight, TheLookOfAMappedHairFollowsItsSource)
{
	std::vector<std::optional<Colours>> known(3);
	known[2] = k_Film;
	// A usual hair earlier in the frame: its colours at its place, or its own colours
	EXPECT_EQ(LookOf(Source {FromDraw {1}}, known, Carried {k_Start}), Look {AtPlace {1}});
	EXPECT_EQ(LookOf(Source {FromDraw {2}}, known, Carried {k_Start}), Look {k_Film});
	// An earlier frame: the colours that it kept
	EXPECT_EQ(LookOf(Source {FromFrameBefore {}}, known, Carried {k_Start}), Look {k_Start});
	EXPECT_EQ(LookOf(Source {FromFrameBefore {}}, known, Carried {k_Film}), Look {k_Film});
	EXPECT_EQ(LookOf(Source {FromFrameBefore {}}, known, Carried {Kept {}}), Look {Kept {}});
}
