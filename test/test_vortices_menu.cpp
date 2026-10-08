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

#include "Debug/VorticesModel.h"

using namespace openblack;
using namespace openblack::debug::vortices;

TEST(VorticesMenu, typeNames)
{
	EXPECT_EQ(TypeName(VortexType::In), "In");
	EXPECT_EQ(TypeName(VortexType::Out), "Out");
	EXPECT_EQ(TypeName(VortexType::Volcano), "Volcano");
	EXPECT_EQ(TypeName(static_cast<VortexType>(7)), "unknown");
}

TEST(VorticesMenu, stateNames)
{
	EXPECT_EQ(StateName(VortexStateType::Inactive), "inactive");
	EXPECT_EQ(StateName(VortexStateType::Active), "active");
	EXPECT_EQ(StateName(VortexStateType::FadeIn), "fading in");
	EXPECT_EQ(StateName(VortexStateType::FadeOut), "fading out");
	EXPECT_EQ(StateName(static_cast<VortexStateType>(9)), "unknown");
}

TEST(VorticesMenu, typesInTheGamesOrder)
{
	ASSERT_EQ(k_Types.size(), 3u);
	EXPECT_EQ(k_Types[0], VortexType::In);
	EXPECT_EQ(k_Types[1], VortexType::Out);
	EXPECT_EQ(k_Types[2], VortexType::Volcano);
}

TEST(VorticesMenu, noVortexNoRow)
{
	EXPECT_TRUE(Rows({}).empty());
}

TEST(VorticesMenu, rowsOldestEntityFirst)
{
	const std::vector<VortexAt> vortices {
	    {.entity = entt::entity {12}, .type = VortexType::Out, .state = VortexStateType::FadeIn, .position = {1, 2, 3}},
	    {.entity = entt::entity {4}, .type = VortexType::Volcano, .state = VortexStateType::Active, .position = {4, 5, 6}},
	    {.entity = entt::entity {8}, .type = VortexType::In, .state = VortexStateType::FadeOut, .position = {7, 8, 9}},
	};
	const auto rows = Rows(vortices);
	ASSERT_EQ(rows.size(), 3u);
	EXPECT_EQ(rows[0].entity, entt::entity {4});
	EXPECT_EQ(rows[0].type, "Volcano");
	EXPECT_EQ(rows[0].state, "active");
	EXPECT_EQ(rows[0].position, glm::vec3(4, 5, 6));
	EXPECT_EQ(rows[1].entity, entt::entity {8});
	EXPECT_EQ(rows[1].type, "In");
	EXPECT_EQ(rows[1].state, "fading out");
	EXPECT_EQ(rows[2].entity, entt::entity {12});
	EXPECT_EQ(rows[2].type, "Out");
	EXPECT_EQ(rows[2].state, "fading in");
}

TEST(VorticesMenu, onlyAVortexNotFadingOutCanFadeOut)
{
	const std::vector<VortexAt> vortices {
	    {.entity = entt::entity {1}, .state = VortexStateType::Inactive},
	    {.entity = entt::entity {2}, .state = VortexStateType::Active},
	    {.entity = entt::entity {3}, .state = VortexStateType::FadeIn},
	    {.entity = entt::entity {4}, .state = VortexStateType::FadeOut},
	};
	const auto rows = Rows(vortices);
	ASSERT_EQ(rows.size(), 4u);
	EXPECT_TRUE(rows[0].canFadeOut);
	EXPECT_TRUE(rows[1].canFadeOut);
	EXPECT_TRUE(rows[2].canFadeOut);
	EXPECT_FALSE(rows[3].canFadeOut);
}

TEST(VorticesMenu, createdMessage)
{
	EXPECT_EQ(CreatedMessage(VortexType::Volcano, {1200.4f, 30.0f, 980.6f}, entt::entity {5}),
	          "Volcano vortex made at (1200, 981)");
	EXPECT_EQ(CreatedMessage(VortexType::Out, {0.0f, 0.0f, 0.0f}, entt::null), "No Out vortex was made");
}

TEST(VorticesMenu, exitVorticesAreInsAtTheScriptsPlaces)
{
	EXPECT_EQ(k_ExitVortexType, VortexType::In);
	ASSERT_EQ(k_ExitVortices.size(), 3u);
	EXPECT_EQ(k_ExitVortices[0].land, 1);
	EXPECT_EQ(k_ExitVortices[0].position, glm::vec3(1700.227539f, 11.400500f, 2520.182861f));
	EXPECT_EQ(k_ExitVortices[1].land, 2);
	EXPECT_EQ(k_ExitVortices[1].position, glm::vec3(1061.107056f, 147.785004f, 3597.965088f));
	EXPECT_EQ(k_ExitVortices[2].land, 4);
	EXPECT_EQ(k_ExitVortices[2].position, glm::vec3(2060.179932f, 11.390000f, 2591.697998f));
}
