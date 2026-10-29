/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "ECS/WorshipChantMusic.h"
#include "ECS/WorshipSiteLooks.h"

using namespace openblack::ecs;
namespace looks = openblack::ecs::worship_site_looks;
namespace chant = openblack::ecs::worship_chant_music;

TEST(WorshipSiteLooks, TheHidingFlagCountsTheWorshippersThatDoNotDance)
{
	EXPECT_FLOAT_EQ(looks::HidingIndicator(false, 30, 2), 0.0f);
	EXPECT_FLOAT_EQ(looks::HidingIndicator(true, 30, 30), 0.0f);
	EXPECT_FLOAT_EQ(looks::HidingIndicator(true, 30, 5), 25.0f * 0.01f);
	// It is full at a hundred, and full when the count runs below none
	EXPECT_FLOAT_EQ(looks::HidingIndicator(true, 150, 5), 1.0f);
	EXPECT_FLOAT_EQ(looks::HidingIndicator(true, 1, 5), 1.0f);
}

TEST(WorshipSiteLooks, TheRingIsTenPointsRoundTheDanceTheLastOnTheFirst)
{
	const glm::vec3 centre {100.0f, 20.0f, -50.0f};
	const auto points = looks::RingPoints(centre);
	ASSERT_EQ(points.size(), looks::k_RingPoints);
	for (const auto& point : points)
	{
		EXPECT_FLOAT_EQ(point.y, centre.y);
		EXPECT_NEAR(glm::distance(glm::vec2(point.x, point.z), glm::vec2(centre.x, centre.z)), looks::k_RingRadius, 1e-4f);
	}
	// The first is along x from the centre, the third a ninth of two turns round
	EXPECT_FLOAT_EQ(points.front().x, 103.0f);
	EXPECT_FLOAT_EQ(points.front().z, -50.0f);
	EXPECT_NEAR(points.back().x, points.front().x, 1e-4f);
	EXPECT_NEAR(points.back().z, points.front().z, 1e-4f);
	EXPECT_GT(points.at(2).z, centre.z);
}

TEST(WorshipSiteLooks, TheRingIsAsStrongAsTheBatteryIsFull)
{
	EXPECT_FLOAT_EQ(looks::RingStrength(0.0f, 100.0f), 0.0f);
	EXPECT_FLOAT_EQ(looks::RingStrength(-5.0f, 100.0f), 0.0f);
	EXPECT_FLOAT_EQ(looks::RingStrength(25.0f, 100.0f), 0.25f);
	EXPECT_FLOAT_EQ(looks::RingStrength(150.0f, 100.0f), 1.0f);
}

TEST(WorshipSiteLooks, TheRingThrobsDarkWhileTheSiteStrains)
{
	constexpr uint32_t k_Red = 0xFF4646u;
	EXPECT_EQ(looks::RingRgb(0xFFFF4646u, 0.0f, 0.3f), k_Red);
	// Each channel times the pulse in 256ths: a full pulse is 255 of 256
	EXPECT_EQ(looks::RingRgb(k_Red, 0.5f, 1.0f),
	          ((0xFFu * 255u) >> 8u) << 16u | ((0x46u * 255u) >> 8u) << 8u | ((0x46u * 255u) >> 8u));
	EXPECT_EQ(looks::RingRgb(k_Red, 0.5f, 0.0f), 0u);
	// Half a pulse is 127 of 256, cut down
	EXPECT_EQ(looks::RingRgb(k_Red, 0.5f, 0.5f),
	          ((0xFFu * 127u) >> 8u) << 16u | ((0x46u * 127u) >> 8u) << 8u | ((0x46u * 127u) >> 8u));
	EXPECT_FLOAT_EQ(looks::RingSlideSpeed(0.25f), -0.25f);
}

TEST(WorshipSiteLooks, TheThrobGoesFasterTheMoreTheSiteStrains)
{
	// A site that does not strain keeps its throb
	const looks::Throb still {.phase = 1.0f, .pulse = 0.4f};
	const auto kept = looks::NextThrob(still, 0.0f, 100);
	EXPECT_FLOAT_EQ(kept.phase, 1.0f);
	EXPECT_FLOAT_EQ(kept.pulse, 0.4f);
	// Five radians a second at the least strain, ten at the most and more
	EXPECT_NEAR(looks::NextThrob({}, 0.0001f, 100).phase, 0.5f, 1e-4f);
	EXPECT_NEAR(looks::NextThrob({}, 1.0f, 100).phase, 1.0f, 1e-6f);
	EXPECT_NEAR(looks::NextThrob({}, 3.0f, 100).phase, 1.0f, 1e-6f);
	// The pulse is half of one more than the cosine of the phase
	const auto half = looks::NextThrob({}, 0.5f, 200);
	EXPECT_NEAR(half.phase, 1.5f, 1e-6f);
	EXPECT_NEAR(half.pulse, (std::cos(1.5) + 1.0) * 0.5, 1e-6f);
	// The phase wraps round a whole turn
	const auto wrapped = looks::NextThrob({.phase = 6.0f, .pulse = 0.0f}, 1.0f, 100);
	EXPECT_NEAR(wrapped.phase, 7.0f - 6.2831855f, 1e-5f);
}

TEST(WorshipSiteLooks, TheMotesAreClearUnlessTheSiteStrains)
{
	EXPECT_EQ(looks::MoteAlpha(0.0f, 0.2f), 255u);
	EXPECT_EQ(looks::MoteAlpha(0.5f, 1.0f), 255u);
	EXPECT_EQ(looks::MoteAlpha(0.5f, 0.5f), 127u);
	EXPECT_EQ(looks::MoteAlpha(0.5f, 0.0f), 0u);
	EXPECT_FLOAT_EQ(looks::FrameSeconds(16), 16.0f * 0.001f);
}

TEST(WorshipChantMusic, TheNearestTempleInRangeIsHeard)
{
	const std::array<glm::vec3, 3> temples {glm::vec3 {0.0f, 0.0f, 200.0f}, glm::vec3 {0.0f, 0.0f, 120.0f},
	                                        glm::vec3 {0.0f, 0.0f, -120.0f}};
	// The first of equally near ones
	EXPECT_EQ(chant::NearestTemple(temples, glm::vec3(0.0f)), std::optional<size_t> {1});
	EXPECT_EQ(chant::NearestTemple(std::span(temples).first(1), glm::vec3(0.0f)), std::nullopt);
}

TEST(WorshipChantMusic, TheNearestSiteWithDancersInRangeIsHeard)
{
	std::array<std::optional<chant::Site>, 6> sites;
	EXPECT_EQ(chant::NearestSite(sites, glm::vec3(0.0f)), std::nullopt);
	sites[0] = chant::Site {.dance = {0.0f, 0.0f, 10.0f}, .dancers = 0};
	sites[2] = chant::Site {.dance = {0.0f, 0.0f, 50.0f}, .dancers = 4};
	sites[4] = chant::Site {.dance = {0.0f, 0.0f, 120.0f}, .dancers = 9};
	// The nearest has no dancers, the furthest is too far
	EXPECT_EQ(chant::NearestSite(sites, glm::vec3(0.0f)), std::optional<size_t> {2});
	sites[2]->dancers = 0;
	EXPECT_EQ(chant::NearestSite(sites, glm::vec3(0.0f)), std::nullopt);
}

TEST(WorshipChantMusic, TheCameraMustBeNearTheLandUnderTheDanceInHeight)
{
	EXPECT_TRUE(chant::NearInHeight(150.0f, 60.0f));
	EXPECT_FALSE(chant::NearInHeight(160.0f, 60.0f));
	EXPECT_TRUE(chant::NearInHeight(-30.0f, 60.0f));
	EXPECT_FALSE(chant::NearInHeight(-40.0f, 60.0f));
}
