/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the shadows of the sun and the moon

#include <cmath>

#include <glm/geometric.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "Graphics/SunShadows.h"

namespace shadows = openblack::graphics::sun_shadows;

TEST(SunShadows, TheMapsAreMadeAboutEveryThreeSecondsAndBlend)
{
	shadows::ShadowClock clock;
	const glm::vec3 light {0.3f, 0.8f, 0.2f};
	const glm::vec2 focus {1000.0f, 1000.0f};
	// The first frame makes the maps and shows them at once
	auto frame = clock.Advance(0.0f, light, focus);
	EXPECT_TRUE(frame.update);
	EXPECT_FLOAT_EQ(frame.blend, 1.0f);
	const auto first = frame.newest;
	// Nothing changes for a while
	int updates = 0;
	for (int i = 1; i <= 1000; ++i)
	{
		frame = clock.Advance(static_cast<float>(i) * 0.01f, light, focus);
		updates += frame.update ? 1 : 0;
	}
	EXPECT_EQ(updates, 3);
	// Each update goes to the other set, and blends in over the blend time
	frame = clock.Advance(12.0f, light, focus);
	ASSERT_TRUE(frame.update);
	EXPECT_FLOAT_EQ(frame.blend, 0.0f);
	const auto half = clock.Advance(12.0f + shadows::k_BlendSeconds * 0.5f, light, focus);
	EXPECT_NEAR(half.blend, 0.5f, 1e-4f);
	EXPECT_EQ(half.newest, frame.newest);
	(void)first;
}

TEST(SunShadows, ALightThatTurnsOrAFocusThatMovesMakesTheMapsSooner)
{
	shadows::ShadowClock clock;
	const glm::vec3 light {0.0f, 1.0f, 0.3f};
	(void)clock.Advance(0.0f, light, {0.0f, 0.0f});
	// After the blend time, a turn of a degree makes the maps again
	const glm::vec3 turned {0.0f, 1.0f, 0.3f + 0.025f};
	EXPECT_TRUE(clock.Advance(1.0f, turned, {0.0f, 0.0f}).update);
	// A focus that leaves the middle of the near cascade too
	EXPECT_TRUE(clock.Advance(2.0f, turned, {shadows::k_CascadeWidths.front() * 0.3f, 0.0f}).update);
	// But never before the last maps blend in
	EXPECT_FALSE(clock.Advance(2.1f, light, {500.0f, 500.0f}).update);
}

TEST(SunShadows, TheCascadeLiesOnWholeTexels)
{
	const glm::vec3 right {1.0f, 0.0f, 0.0f};
	const glm::vec3 up {0.0f, 0.0f, 1.0f};
	const float width = 480.0f;
	const float texel = width / shadows::k_MapTexels;
	const auto snapped = shadows::SnapToTexels({123.456f, 7.0f, 987.654f}, right, up, width);
	EXPECT_NEAR(std::remainder(snapped.x, texel), 0.0f, 1e-3f);
	EXPECT_NEAR(std::remainder(snapped.z, texel), 0.0f, 1e-3f);
	EXPECT_FLOAT_EQ(snapped.y, 7.0f);
	EXPECT_LE(std::abs(snapped.x - 123.456f), texel);
}

TEST(SunShadows, TheLightsViewPutsTheNearestThingsDeepest)
{
	const glm::vec3 light = glm::normalize(glm::vec3 {0.4f, 1.0f, -0.2f});
	const glm::vec3 middle {500.0f, 20.0f, 700.0f};
	const auto view = shadows::CascadeView(light, middle, 480.0f, false);
	const auto viewProj = view.projection * view.view;
	const auto at = [&viewProj](glm::vec3 p) {
		const auto clip = viewProj * glm::vec4(p, 1.0f);
		return glm::vec3(clip) / clip.w;
	};
	// The middle is in the middle of the map
	const auto centre = at(middle);
	EXPECT_NEAR(centre.x, 0.0f, 0.01f);
	EXPECT_NEAR(centre.y, 0.0f, 0.01f);
	// A point nearer the light has a larger depth, as the scene's reversed depth has
	const auto nearer = at(middle + light * 50.0f);
	EXPECT_GT(nearer.z, centre.z);
	EXPECT_GT(centre.z, 0.0f);
	EXPECT_LT(nearer.z, 1.0f);
	// The edge of the cascade is at the edge of the map
	const glm::vec3 right {view.view[0][0], view.view[1][0], view.view[2][0]};
	EXPECT_NEAR(std::abs(at(middle + right * 240.0f).x), 1.0f, 0.01f);
}

TEST(SunShadows, AHillHidesTheLowSun)
{
	// A ridge 100 high at x = 500, flat ground elsewhere
	const auto heights = [](glm::vec2 p) { return std::abs(p.x - 500.0f) < 20.0f ? 100.0f : 0.0f; };
	// The sun low in the +x direction: the ground just west of the ridge is in its shadow, the far ground lit
	const auto low = glm::normalize(glm::vec3 {1.0f, 0.1f, 0.0f});
	EXPECT_LT(shadows::VisibilityAt({300.0f, 0.0f}, low, heights), 0.05f);
	EXPECT_GT(shadows::VisibilityAt({-1200.0f, 0.0f}, low, heights), 0.95f);
	// East of the ridge, the light comes from the side away from it
	EXPECT_GT(shadows::VisibilityAt({700.0f, 0.0f}, low, heights), 0.95f);
	// A high sun lights it all, and a sun under the horizon none
	const auto high = glm::normalize(glm::vec3 {1.0f, 3.0f, 0.0f});
	EXPECT_GT(shadows::VisibilityAt({300.0f, 0.0f}, high, heights), 0.95f);
	EXPECT_EQ(shadows::VisibilityAt({300.0f, 0.0f}, {1.0f, -0.1f, 0.0f}, heights), 0.0f);
}
