/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The advisors' area on screens of any shape (src/Help/SpiritStage.h, SpiritView.h): on a screen wider than 4:3 the
// advisors move in a 4:3 area as tall as the screen, in its middle, and are drawn as a 4:3 screen of that height
// draws them: the same place, size, perspective, depth and light. Screens 4:3 or taller are left exactly as the game
// has them.

#include <cmath>
#include <cstdint>

#include <array>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>
#include <gtest/gtest.h>

#include "Help/SpiritStage.h"
#include "Help/SpiritView.h"
#include "Help/Spirits.h"

using namespace openblack;
using namespace openblack::help::spirits;

namespace
{
/// The game's horizontal field of view, 70 degrees
constexpr float k_FieldOfView = 1.2217305f;
constexpr float k_NearClip = 3.5f;

struct StageCase
{
	Screen screen;
	uint16_t layoutWidth;
	int32_t left;
};

constexpr std::array<StageCase, 9> k_Stages {
    StageCase {{640, 480}, 640, 0},      StageCase {{1024, 768}, 1024, 0},    StageCase {{1600, 1200}, 1600, 0},
    StageCase {{1280, 1024}, 1280, 0},   StageCase {{1280, 720}, 960, 160},   StageCase {{1920, 1080}, 1440, 240},
    StageCase {{2560, 1440}, 1920, 320}, StageCase {{3440, 1440}, 1920, 760}, StageCase {{5120, 1440}, 1920, 1600},
};

/// A camera over the land looking down at it, its field of view across the screen, as the game makes its view
SpiritView MakeView(Screen screen)
{
	const float aspect = static_cast<float>(screen.width) / static_cast<float>(screen.height);
	const float yFov = std::atan(std::tan(k_FieldOfView * 0.5f) / aspect) * 2.0f;
	const glm::mat4 projection = glm::perspective(yFov, aspect, k_NearClip, 65536.0f);
	const glm::vec3 eye(1200.0f, 140.0f, 900.0f);
	const glm::mat4 viewMatrix = glm::lookAt(eye, glm::vec3(1260.0f, 60.0f, 1010.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	const glm::mat4 toWorld = glm::inverse(viewMatrix);
	SpiritView view;
	view.eye = glm::vec3(toWorld[3]);
	view.right = glm::vec3(toWorld[0]);
	view.up = glm::vec3(toWorld[1]);
	view.forward = glm::vec3(toWorld[2]) * projection[2][3];
	view.halfExtent = {1.0f / projection[0][0], 1.0f / projection[1][1]};
	view.nearClip = k_NearClip;
	view.viewProjection = projection * viewMatrix;
	view.screen = screen;
	return view;
}

/// A turned and scaled set of axes for an advisor, as its hover turns it
glm::mat3 AdvisorAxes(const SpiritView& view)
{
	const glm::mat3 camera(view.right, view.up, view.forward);
	const glm::mat3 turn = glm::mat3(glm::rotate(glm::mat4(1.0f), 0.7f, glm::vec3(0.3f, 1.0f, 0.2f)));
	return camera * turn * 0.0215f;
}

/// Points of a model in its own units, around its root
constexpr std::array<glm::vec3, 6> k_ModelPoints {glm::vec3(0.0f),
                                                  glm::vec3(30.0f, 0.0f, 0.0f),
                                                  glm::vec3(0.0f, 63.0f, 0.0f),
                                                  glm::vec3(0.0f, 0.0f, 40.0f),
                                                  glm::vec3(-20.0f, 35.0f, -25.0f),
                                                  glm::vec3(15.0f, -10.0f, 38.0f)};

/// A pixel of a real screen, unclamped
glm::vec2 PixelOf(const SpiritView& view, const glm::vec3& point)
{
	const auto pixel = WorldToPixel(view, point, true);
	EXPECT_TRUE(pixel.has_value());
	return pixel.value_or(glm::vec2(0.0f));
}

struct Controller
{
	SpiritView view;
	DudeData good;
	DudeData evil;
	AdvisorSpiritController control;

	explicit Controller(const SpiritView& stageView)
	    : view(stageView)
	    , control(MakeData(), MakeData(), MakeQueries(), stageView.screen)
	{
	}

	static DudeData MakeData()
	{
		DudeData data;
		data.nearDepth = 8.733334f;
		data.farDepth = 7.232f;
		data.scale = 0.02552f;
		data.modelSize = 63.42295f;
		return data;
	}

	Queries MakeQueries()
	{
		Queries q;
		q.localRand = [](int32_t) { return 0u; };
		q.localFloatRand = [](float) { return 0.0f; };
		q.random = [](float a, float) { return a; };
		q.nearClip = [this]() { return view.nearClip; };
		q.pointFromScreen = [this](glm::vec2 pixel, float depth) { return PointFromScreen(view, pixel, depth); };
		q.worldToPixel = [this](const glm::vec3& point, bool force) { return WorldToPixel(view, point, force); };
		q.projectPoint = [this](const glm::vec3& point) { return ProjectPoint(view, point); };
		q.cameraAxes = [this]() { return glm::mat3(view.right, view.up, view.forward); };
		q.cameraPosition = [this]() { return view.eye; };
		return q;
	}

	void Frames(int count, float share)
	{
		FrameInput input;
		input.dt = 0.1f;
		input.frameMs = 100;
		input.screen = view.screen;
		input.mouse = {view.screen.HalfWidth(), view.screen.HalfHeight()};
		input.share = share;
		for (int i = 0; i < count; ++i)
		{
			input.tickMs += static_cast<uint32_t>(input.frameMs);
			control.Update(input);
		}
	}
};
} // namespace

TEST(SpiritStage, A4By3AreaInTheMiddleOfWiderScreens)
{
	for (const auto& c : k_Stages)
	{
		SCOPED_TRACE(c.screen.width);
		const Stage stage = StageOf(c.screen);
		EXPECT_EQ(stage.screen, c.screen);
		EXPECT_EQ(stage.layout.width, c.layoutWidth);
		EXPECT_EQ(stage.layout.height, c.screen.height);
		EXPECT_EQ(stage.left, c.left);
		EXPECT_FLOAT_EQ(stage.share, static_cast<float>(c.layoutWidth) / static_cast<float>(c.screen.width));
		// Centred
		EXPECT_EQ(stage.left * 2 + stage.layout.width, c.screen.width);
	}
}

TEST(SpiritStage, ScreensUpTo4By3AreLeftAsTheGameHasThem)
{
	for (const auto& screen : {Screen {640, 480}, Screen {1600, 1200}, Screen {1280, 1024}, Screen {1024, 1024}})
	{
		SCOPED_TRACE(screen.width);
		const SpiritView view = MakeView(screen);
		const SpiritView staged = OnStage(view, StageOf(screen));
		EXPECT_EQ(staged.screen, view.screen);
		EXPECT_EQ(staged.halfExtent, view.halfExtent);
		EXPECT_EQ(staged.viewProjection, view.viewProjection);
		const glm::mat3 axes = AdvisorAxes(view);
		EXPECT_EQ(ShrinkAcrossScreen(axes, view.forward, 1.0f), axes);
	}
}

TEST(SpiritStage, AreaPixelsLieInTheMiddleOfTheScreen)
{
	for (const auto& c : k_Stages)
	{
		SCOPED_TRACE(c.screen.width);
		const Stage stage = StageOf(c.screen);
		const SpiritView real = MakeView(c.screen);
		const SpiritView staged = OnStage(real, stage);
		for (const glm::vec2 pixel :
		     {glm::vec2(0.0f, 0.0f), glm::vec2(100.0f, 300.0f), glm::vec2(stage.layout.width, stage.layout.height) * 0.5f,
		      glm::vec2(stage.layout.width, stage.layout.height)})
		{
			const glm::vec3 point = PointFromScreen(staged, pixel, 8.0f);
			// On the real screen it is the area's pixel moved over by the area's left edge
			const glm::vec2 onScreen = PixelOf(real, point);
			EXPECT_NEAR(onScreen.x, pixel.x + static_cast<float>(stage.left), 0.25f);
			EXPECT_NEAR(onScreen.y, pixel.y, 0.25f);
			// and back in the area's pixels
			const glm::vec2 back = PixelOf(staged, point);
			EXPECT_NEAR(back.x, pixel.x, 0.25f);
			EXPECT_NEAR(back.y, pixel.y, 0.25f);
			const auto projected = ProjectPoint(staged, point);
			ASSERT_TRUE(projected.has_value());
			EXPECT_NEAR(projected->depth, 8.0f, 1e-3f);
		}
	}
}

TEST(SpiritStage, TheRestZoneIsAsFarOutAsOnA4By3Screen)
{
	// Each advisor rests 0.66 half widths of the area out from the middle: the same share of the screen's height as on
	// a 4:3 screen, 0.66 x 2/3 of it, whatever the screen's width
	for (const auto& c : k_Stages)
	{
		SCOPED_TRACE(c.screen.width);
		const Stage stage = StageOf(c.screen);
		const float out = 0.66f * static_cast<float>(stage.layout.HalfWidth());
		const float shareOfHeight = out / static_cast<float>(c.screen.height);
		if (stage.share < 1.0f)
		{
			EXPECT_NEAR(shareOfHeight, 0.66f * 2.0f / 3.0f, 1e-3f);
		}
	}
}

TEST(SpiritStage, DrawnAsA4By3ScreenOfTheSameHeightDrawsThem)
{
	for (const auto& c : k_Stages)
	{
		SCOPED_TRACE(c.screen.width);
		const Stage stage = StageOf(c.screen);
		const SpiritView wide = OnStage(MakeView(c.screen), stage);
		const SpiritView fourByThree = MakeView(stage.layout);
		for (const glm::vec2 pixel : {glm::vec2(stage.layout.width, stage.layout.height) * 0.5f,
		                              glm::vec2(stage.layout.width * 0.83f, stage.layout.height * 0.5f),
		                              glm::vec2(stage.layout.width * 0.1f, stage.layout.height * 0.9f)})
		{
			const glm::vec3 root = PointFromScreen(wide, pixel, 6.2f);
			const glm::vec3 root43 = PointFromScreen(fourByThree, pixel, 6.2f);
			const glm::mat3 axes43 = AdvisorAxes(fourByThree);
			const glm::mat3 axes = ShrinkAcrossScreen(axes43, wide.forward, stage.share);
			for (const auto& v : k_ModelPoints)
			{
				const glm::vec3 point = root + axes * v;
				const glm::vec3 point43 = root43 + axes43 * v;
				// Every point of it lands on the same pixel of the area as on the 4:3 screen (to within the float precision of
				// points this far out in the world): its place, size and
				// perspective are the 4:3 screen's
				const glm::vec2 drawn = PixelOf(wide, point);
				const glm::vec2 drawn43 = PixelOf(fourByThree, point43);
				EXPECT_NEAR(drawn.x, drawn43.x, 0.25f);
				EXPECT_NEAR(drawn.y, drawn43.y, 0.25f);
				// at the same depth, so no nearer the near plane than on the 4:3 screen
				EXPECT_NEAR(glm::dot(point - wide.eye, wide.forward), glm::dot(point43 - fourByThree.eye, fourByThree.forward),
				            1e-3f);
			}
			// Lit from the same side: its light, beside the camera at the area's left edge, in its own axes
			const glm::vec2 lightPixel(0.0f, static_cast<float>(stage.layout.HalfHeight()));
			const glm::vec3 light = glm::normalize(glm::inverse(axes) * (PointFromScreen(wide, lightPixel, -10.0f) - root));
			const glm::vec3 light43 =
			    glm::normalize(glm::inverse(axes43) * (PointFromScreen(fourByThree, lightPixel, -10.0f) - root43));
			EXPECT_NEAR(light.x, light43.x, 1e-4f);
			EXPECT_NEAR(light.y, light43.y, 1e-4f);
			EXPECT_NEAR(light.z, light43.z, 1e-4f);
		}
	}
}

TEST(SpiritStage, AnAdvisorIsTheSameShareOfTheScreensHeightOnEveryScreen)
{
	// Its height is ~13.2% of a 4:3 screen's width, so ~17.6% of its height; on any screen it stays that share of the
	// height
	for (const auto& c : k_Stages)
	{
		SCOPED_TRACE(c.screen.width);
		const Stage stage = StageOf(c.screen);
		const SpiritView real = MakeView(c.screen);
		const SpiritView wide = OnStage(real, stage);
		const glm::vec2 middle(static_cast<float>(stage.layout.HalfWidth()), static_cast<float>(stage.layout.HalfHeight()));
		const glm::vec3 root = PointFromScreen(wide, middle, 8.733334f);
		const float height = 0.0268267f * 79.15389f * 0.761f;
		const glm::vec3 top = root + ShrinkAcrossScreen(wide.up * height, wide.forward, stage.share);
		const float drawn = PixelOf(real, root).y - PixelOf(real, top).y;
		const float tallest = std::min(static_cast<float>(c.screen.width), static_cast<float>(c.screen.height) * 4.0f / 3.0f);
		EXPECT_NEAR(drawn / tallest, 0.132f, 0.002f);
	}
}

TEST(SpiritStage, TheControllerShrinksTheAdvisorAcrossTheScreenNearIt)
{
	const Stage stage = StageOf({2560, 1440});
	const SpiritView view = OnStage(MakeView(stage.screen), stage);
	Controller narrow(view);
	Controller shrunk(view);
	narrow.control.SpiritEject(1, false);
	shrunk.control.SpiritEject(1, false);
	narrow.Frames(5, 1.0f);
	shrunk.Frames(5, stage.share);
	const AdvisorSpirit& full = narrow.control.Dude(0);
	const AdvisorSpirit& dude = shrunk.control.Dude(0);
	EXPECT_EQ(full.DrawShare(), 1.0f);
	EXPECT_FLOAT_EQ(dude.DrawShare(), 0.75f);
	// The same place on the screen; the axes shrunk across it, their depth kept
	EXPECT_EQ(dude.Position(), full.Position());
	const glm::mat3 expected = ShrinkAcrossScreen(full.Rows(), view.forward, stage.share);
	for (glm::length_t i = 0; i < 3; ++i)
	{
		for (glm::length_t j = 0; j < 3; ++j)
		{
			EXPECT_NEAR(dude.Rows()[i][j], expected[i][j], 1e-6f);
		}
		EXPECT_NEAR(glm::dot(dude.Rows()[i], view.forward), glm::dot(full.Rows()[i], view.forward), 1e-6f);
	}
}
