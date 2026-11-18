/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <vector>

#include <Graphics/LakeMirrorOptions.h>
#include <Graphics/LakeMirrors.h>
#include <Graphics/ViewFrustum.h>
#include <Visual/RenderPass.h>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

using namespace openblack;
using namespace openblack::graphics;
using namespace openblack::graphics::lake_mirrors;

namespace
{
constexpr glm::u16vec2 k_Screen {800, 600};
constexpr PlanSettings k_Settings {.slots = 16, .margin = 2, .rects = true, .shareLayers = true};

/// A camera high over the middle of a square of 1000 units, looking straight down
glm::mat4 OverheadViewProjection()
{
	const auto projection = glm::perspective(glm::radians(60.0f), 800.0f / 600.0f, 1.0f, 10000.0f);
	const auto view =
	    glm::lookAt(glm::vec3(500.0f, 1000.0f, 500.0f), glm::vec3(500.0f, 0.0f, 500.0f), glm::vec3(0.0f, 0.0f, -1.0f));
	return projection * view;
}
constexpr glm::vec3 k_Eye {500.0f, 1000.0f, 500.0f};

/// The water of one level in one square piece
Level SquareLevel(float level, glm::vec2 from, glm::vec2 to)
{
	return {.level = level, .firstIndex = 0, .indexCount = 3, .pieces = {{.minima = from, .maxima = to}}};
}

lake_surface::Vertex VertexAt(float x, float z, float level)
{
	return {.x = x, .y = level, .z = z, .landHeight = level - 1.0f, .level = level, .fade = level - 1.0f};
}
} // namespace

TEST(LakeMirrors, GroupByLevelPutsTheTrianglesOfEachLevelTogether)
{
	lake_surface::Surface surface;
	surface.vertices = {VertexAt(0, 0, 30),     VertexAt(10, 0, 30),    VertexAt(0, 10, 30),
	                    VertexAt(200, 200, 12), VertexAt(210, 200, 12), VertexAt(200, 210, 12),
	                    VertexAt(20, 0, 30),    VertexAt(20, 10, 30),   VertexAt(10, 10, 30)};
	surface.indices = {0, 1, 2, 3, 4, 5, 6, 7, 8};
	const auto levels = GroupByLevel(surface);
	ASSERT_EQ(levels.size(), 2u);
	EXPECT_FLOAT_EQ(levels[0].level, 12.0f);
	EXPECT_EQ(levels[0].firstIndex, 0u);
	EXPECT_EQ(levels[0].indexCount, 3u);
	EXPECT_FLOAT_EQ(levels[1].level, 30.0f);
	EXPECT_EQ(levels[1].firstIndex, 3u);
	EXPECT_EQ(levels[1].indexCount, 6u);
	EXPECT_EQ(surface.indices, (std::vector<uint32_t> {3, 4, 5, 0, 1, 2, 6, 7, 8}));
	// The two triangles of the higher level are in one square of 80 units
	ASSERT_EQ(levels[1].pieces.size(), 1u);
	EXPECT_EQ(levels[1].pieces[0].minima, glm::vec2(0.0f, 0.0f));
	EXPECT_EQ(levels[1].pieces[0].maxima, glm::vec2(20.0f, 10.0f));
}

TEST(LakeMirrors, ScreenRectHoldsTheWaterAndItsMargin)
{
	const auto viewProjection = OverheadViewProjection();
	const auto rect = ScreenRect(std::vector<Piece> {{.minima = {450.0f, 450.0f}, .maxima = {550.0f, 550.0f}}}, 0.0f,
	                             viewProjection, k_Screen, 2);
	ASSERT_TRUE(rect.has_value());
	// The square is at the middle of the screen
	EXPECT_NEAR(rect->x + (rect->width * 0.5f), 400.0f, 1.0f);
	EXPECT_NEAR(rect->y + (rect->height * 0.5f), 300.0f, 1.0f);
	// The projected square is about 52 pixels across, with 2 pixels more on each side
	EXPECT_NEAR(rect->width, 52.0f + 4.0f, 2.0f);
	EXPECT_NEAR(rect->height, 52.0f + 4.0f, 2.0f);
}

TEST(LakeMirrors, ScreenRectIsNoneOffTheScreenOrBehindTheCamera)
{
	const auto viewProjection = OverheadViewProjection();
	EXPECT_FALSE(ScreenRect(std::vector<Piece> {{.minima = {5000.0f, 5000.0f}, .maxima = {5100.0f, 5100.0f}}}, 0.0f,
	                        viewProjection, k_Screen, 2)
	                 .has_value());
	// Water over the camera is behind it
	EXPECT_FALSE(ScreenRect(std::vector<Piece> {{.minima = {450.0f, 450.0f}, .maxima = {550.0f, 550.0f}}}, 2000.0f,
	                        viewProjection, k_Screen, 2)
	                 .has_value());
}

TEST(LakeMirrors, ScreenRectOfWaterThatCrossesTheCameraPlaneReachesTheEdge)
{
	// A camera that looks along the ground, with the water reaching back past it
	const auto projection = glm::perspective(glm::radians(60.0f), 800.0f / 600.0f, 1.0f, 10000.0f);
	const auto view = glm::lookAt(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, 0.0f, -100.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	const auto rect = ScreenRect(std::vector<Piece> {{.minima = {-50.0f, -200.0f}, .maxima = {50.0f, 50.0f}}}, 0.0f,
	                             projection * view, k_Screen, 2);
	ASSERT_TRUE(rect.has_value());
	EXPECT_EQ(rect->y + rect->height, k_Screen.y);
}

TEST(LakeMirrors, PlanSkipsLevelsAtOrOverTheCameraHiddenOrOffScreen)
{
	const auto viewProjection = OverheadViewProjection();
	const std::vector<Level> levels {
	    SquareLevel(10.0f, {450.0f, 450.0f}, {550.0f, 550.0f}),     // in view
	    SquareLevel(1000.0f, {100.0f, 100.0f}, {200.0f, 200.0f}),   // at the camera's height
	    SquareLevel(20.0f, {5000.0f, 5000.0f}, {5100.0f, 5100.0f}), // off the screen
	    SquareLevel(30.0f, {300.0f, 300.0f}, {350.0f, 350.0f}),     // in view but hidden
	};
	const std::vector<uint8_t> hidden {0, 0, 0, 1};
	const auto plan = Plan(levels, viewProjection, k_Eye, k_Screen, hidden, {}, k_Settings);
	ASSERT_EQ(plan.passes.size(), 1u);
	EXPECT_EQ(plan.passes[0].level, 0u);
	EXPECT_FALSE(plan.passes[0].kept);
	EXPECT_EQ(plan.passes[0].rect, plan.passes[0].drawn);
	ASSERT_EQ(plan.passOfLevel.size(), 4u);
	EXPECT_EQ(plan.passOfLevel[0], std::optional<uint8_t>(0));
	EXPECT_FALSE(plan.passOfLevel[1].has_value());
	EXPECT_FALSE(plan.passOfLevel[2].has_value());
	EXPECT_FALSE(plan.passOfLevel[3].has_value());
}

TEST(LakeMirrors, PlanSharesALayerUntilTheRectanglesOverlap)
{
	const auto viewProjection = OverheadViewProjection();
	const std::vector<Level> levels {
	    SquareLevel(10.0f, {300.0f, 300.0f}, {400.0f, 400.0f}),
	    SquareLevel(20.0f, {600.0f, 600.0f}, {700.0f, 700.0f}),
	    SquareLevel(30.0f, {350.0f, 350.0f}, {420.0f, 420.0f}),
	};
	const auto plan = Plan(levels, viewProjection, k_Eye, k_Screen, {}, {}, k_Settings);
	ASSERT_EQ(plan.passes.size(), 3u);
	const auto layerOf = [&plan](size_t level) { return plan.passes[*plan.passOfLevel[level]].layer; };
	EXPECT_EQ(layerOf(0), 0);
	EXPECT_EQ(layerOf(1), 0);
	EXPECT_EQ(layerOf(2), 1);
	// With no shared layers, each mirror has its own
	auto alone = k_Settings;
	alone.shareLayers = false;
	const auto own = Plan(levels, viewProjection, k_Eye, k_Screen, {}, {}, alone);
	EXPECT_EQ(own.passes[0].layer, 0);
	EXPECT_EQ(own.passes[1].layer, 1);
	EXPECT_EQ(own.passes[2].layer, 2);
}

TEST(LakeMirrors, PlanGivesTheSlotsToTheLargestRectangles)
{
	const auto viewProjection = OverheadViewProjection();
	const std::vector<Level> levels {
	    SquareLevel(10.0f, {300.0f, 300.0f}, {310.0f, 310.0f}),
	    SquareLevel(20.0f, {400.0f, 400.0f}, {500.0f, 500.0f}),
	    SquareLevel(30.0f, {600.0f, 600.0f}, {650.0f, 650.0f}),
	};
	auto two = k_Settings;
	two.slots = 2;
	const auto plan = Plan(levels, viewProjection, k_Eye, k_Screen, {}, {}, two);
	ASSERT_EQ(plan.passes.size(), 2u);
	EXPECT_EQ(plan.passes[0].level, 1u);
	EXPECT_EQ(plan.passes[1].level, 2u);
	EXPECT_FALSE(plan.passOfLevel[0].has_value());
}

TEST(LakeMirrors, PlanWithNoRectanglesDrawsTheWholeScreen)
{
	auto whole = k_Settings;
	whole.rects = false;
	const std::vector<Level> levels {SquareLevel(10.0f, {450.0f, 450.0f}, {550.0f, 550.0f})};
	const auto plan = Plan(levels, OverheadViewProjection(), k_Eye, k_Screen, {}, {}, whole);
	ASSERT_EQ(plan.passes.size(), 1u);
	EXPECT_EQ(plan.passes[0].rect, (PixelRect {.x = 0, .y = 0, .width = 800, .height = 600}));
}

TEST(LakeMirrors, PlanJoinsNearLevelsAndHalvesSmallMirrors)
{
	const std::vector<Level> levels {
	    SquareLevel(10.0f, {300.0f, 300.0f}, {400.0f, 400.0f}),
	    SquareLevel(10.5f, {600.0f, 600.0f}, {650.0f, 650.0f}),
	};
	auto joined = k_Settings;
	joined.joinWithin = 1.0f;
	const auto plan = Plan(levels, OverheadViewProjection(), k_Eye, k_Screen, {}, {}, joined);
	ASSERT_EQ(plan.passes.size(), 1u);
	EXPECT_EQ(plan.passOfLevel[0], plan.passOfLevel[1]);
	// The joined rectangle holds both
	const auto first = ScreenRect(levels[0].pieces, 10.0f, OverheadViewProjection(), k_Screen, 2);
	const auto second = ScreenRect(levels[1].pieces, 10.5f, OverheadViewProjection(), k_Screen, 2);
	EXPECT_TRUE(Overlap(plan.passes[0].rect, *first));
	EXPECT_TRUE(Overlap(plan.passes[0].rect, *second));

	auto halved = k_Settings;
	halved.halfSizeBelow = 0.5f;
	const auto small = Plan(levels, OverheadViewProjection(), k_Eye, k_Screen, {}, {}, halved);
	ASSERT_EQ(small.passes.size(), 2u);
	EXPECT_EQ(small.passes[0].drawn.width, (small.passes[0].rect.width + 1) / 2);
	EXPECT_EQ(small.passes[0].drawn.x, small.passes[0].rect.x);
}

TEST(LakeMirrors, PlanKeepsSmallMirrorsOnEverySecondFrame)
{
	const std::vector<Level> levels {SquareLevel(10.0f, {300.0f, 300.0f}, {400.0f, 400.0f})};
	auto alternate = k_Settings;
	alternate.everySecondFrameBelow = 0.5f;
	const auto first = Plan(levels, OverheadViewProjection(), k_Eye, k_Screen, {}, {}, alternate);
	ASSERT_EQ(first.passes.size(), 1u);
	EXPECT_FALSE(first.passes[0].kept);
	alternate.drawsAll = false;
	const auto second = Plan(levels, OverheadViewProjection(), k_Eye, k_Screen, {}, first.passes, alternate);
	ASSERT_EQ(second.passes.size(), 1u);
	EXPECT_TRUE(second.passes[0].kept);
	EXPECT_EQ(second.passes[0].rect, first.passes[0].rect);
	// A kept mirror is not kept again
	const auto third = Plan(levels, OverheadViewProjection(), k_Eye, k_Screen, {}, second.passes, alternate);
	EXPECT_FALSE(third.passes[0].kept);
}

TEST(LakeMirrors, RectTransformGivesEachPointTheSamePixel)
{
	const auto viewProjection = OverheadViewProjection();
	const PixelRect rect {.x = 300, .y = 200, .width = 120, .height = 90};
	const auto inRect = RectTransform(rect, k_Screen) * viewProjection;
	const glm::vec4 point {480.0f, 0.0f, 510.0f, 1.0f};
	const auto whole = viewProjection * point;
	const auto part = inRect * point;
	const glm::vec2 wholePixel {(whole.x / whole.w + 1.0f) * 0.5f * k_Screen.x, (1.0f - whole.y / whole.w) * 0.5f * k_Screen.y};
	const glm::vec2 partPixel {rect.x + (part.x / part.w + 1.0f) * 0.5f * rect.width,
	                           rect.y + (1.0f - part.y / part.w) * 0.5f * rect.height};
	EXPECT_NEAR(wholePixel.x, partPixel.x, 1e-3f);
	EXPECT_NEAR(wholePixel.y, partPixel.y, 1e-3f);
	EXPECT_FLOAT_EQ(whole.z, part.z);
	EXPECT_FLOAT_EQ(whole.w, part.w);
}

TEST(LakeMirrors, ParseOptions)
{
	const auto defaults = ParseOptions("");
	ASSERT_TRUE(defaults.has_value());
	EXPECT_TRUE(defaults->mirrors && defaults->floor && defaults->rects && defaults->shareLayers);
	EXPECT_FALSE(defaults->occlusion);
	const auto changed = ParseOptions("no-floor,no-rects,occlusion,no-share,half=0.1,join=1.5,alternate=0.05");
	ASSERT_TRUE(changed.has_value());
	EXPECT_FALSE(changed->floor || changed->rects || changed->shareLayers);
	EXPECT_TRUE(changed->occlusion);
	EXPECT_FLOAT_EQ(changed->halfSizeBelow, 0.1f);
	EXPECT_FLOAT_EQ(changed->joinWithin, 1.5f);
	EXPECT_FLOAT_EQ(changed->everySecondFrameBelow, 0.05f);
	EXPECT_FALSE(ParseOptions("off")->mirrors);
	EXPECT_FALSE(ParseOptions("fast").has_value());
	EXPECT_FALSE(ParseOptions("half=x").has_value());
}

TEST(LakeMirrors, PassesOfEachSlot)
{
	for (uint8_t slot = 0; slot < k_LakeMirrorSlots; ++slot)
	{
		const auto scene = LakeMirrorPass(slot);
		EXPECT_TRUE(IsMirrorScene(scene));
		EXPECT_TRUE(IsMirrorView(scene));
		EXPECT_TRUE(HasSkyPass(scene));
		EXPECT_TRUE(IsMirrorSky(SkyPassOf(scene)));
		EXPECT_FALSE(IsMirrorView(SkyPassOf(scene)));
		EXPECT_TRUE(IsMirrorView(TranslucentPassOf(scene)));
		EXPECT_TRUE(IsMirrorView(MirroredPassOf(scene)));
		EXPECT_FALSE(IsMirrorScene(MirroredPassOf(scene)));
		for (const auto pass : {SkyPassOf(scene), scene, TranslucentPassOf(scene), MirroredPassOf(scene)})
		{
			EXPECT_EQ(LakeMirrorSlotOf(pass), std::optional<uint8_t>(slot));
		}
	}
	EXPECT_EQ(MirroredPassOf(RenderPass::Reflection), RenderPass::ReflectionMirrored);
	EXPECT_EQ(MirroredPassOf(RenderPass::Main), RenderPass::Main);
	EXPECT_FALSE(LakeMirrorSlotOf(RenderPass::Reflection).has_value());
	EXPECT_FALSE(LakeMirrorSlotOf(RenderPass::Sky).has_value());
	EXPECT_TRUE(IsMirrorSky(RenderPass::ReflectionSky));
}

TEST(LakeMirrors, FloorLeavesOutWhatIsUnderTheLevel)
{
	const auto frustum = view_frustum::WithFloor(view_frustum::FromViewProjection(OverheadViewProjection()), 50.0f);
	EXPECT_TRUE(view_frustum::SeesSphere(frustum, {500.0f, 60.0f, 500.0f}, 1.0f));
	EXPECT_FALSE(view_frustum::SeesSphere(frustum, {500.0f, 40.0f, 500.0f}, 1.0f));
	EXPECT_TRUE(view_frustum::SeesWorldBox(frustum, {490.0f, 0.0f, 490.0f}, {510.0f, 55.0f, 510.0f}));
	EXPECT_FALSE(view_frustum::SeesWorldBox(frustum, {490.0f, 0.0f, 490.0f}, {510.0f, 45.0f, 510.0f}));
	// With no floor, the same box is in view
	EXPECT_TRUE(view_frustum::SeesWorldBox(view_frustum::FromViewProjection(OverheadViewProjection()), {490.0f, 0.0f, 490.0f},
	                                       {510.0f, 45.0f, 510.0f}));
}
