/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <array>
#include <span>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Common/CrtQsort.h"
#include "Gui/LogoEffect.h"
#include "Gui/StartupRules.h"

using namespace openblack;
using namespace openblack::logo_effect;

namespace
{
constexpr Screen k_Screen {.width = 800, .height = 600};
constexpr float k_WorldLens = 1.2217305f; // 70 degrees

/// A random number half way between the two numbers
float Middle(float a, float b)
{
	return (a + b) * 0.5f;
}

/// A picture whose pixels have blue that falls from the top left, so that the sort changes their order
Picture TestPicture()
{
	Picture picture {.text = u"  ", .lionhead = true};
	for (size_t i = 0; i < k_LogoPixels; ++i)
	{
		picture.rgb.at(i * 3) = static_cast<uint8_t>(i % 256);
		picture.rgb.at((i * 3) + 1) = static_cast<uint8_t>((i / 64) * 4);
		picture.rgb.at((i * 3) + 2) = static_cast<uint8_t>(255 - (i % 256));
	}
	return picture;
}

Input StillInput(int32_t milliseconds)
{
	return {.milliseconds = milliseconds, .pointer = {400, 300}, .mouseMove = {0, 0}, .wheel = 0, .viewAcross = k_WorldLens};
}
} // namespace

TEST(CrtQsort, SortsAndKeepsTheLibraryOrderOfEqualItems)
{
	struct Item
	{
		int key;
		int tag;
	};
	// A part of eight items or fewer puts the largest last each time, so equal items come out in this order
	std::array<Item, 3> few {{{1, 1}, {1, 2}, {1, 3}}};
	crt_qsort::Sort(std::span(few), [](const Item& a, const Item& b) { return a.key - b.key; });
	EXPECT_EQ(few[0].tag, 2);
	EXPECT_EQ(few[1].tag, 3);
	EXPECT_EQ(few[2].tag, 1);

	std::vector<Item> many;
	for (int i = 0; i < 1000; ++i)
	{
		many.push_back({(i * 7919) % 101, i});
	}
	crt_qsort::Sort(std::span(many), [](const Item& a, const Item& b) { return a.key - b.key; });
	for (size_t i = 1; i < many.size(); ++i)
	{
		EXPECT_LE(many[i - 1].key, many[i].key);
	}
}

TEST(LogoEffect, TimeRules)
{
	EXPECT_EQ(FadeAt(0.0f), 255);
	EXPECT_EQ(FadeAt(6.0f), 127);
	EXPECT_EQ(FadeAt(20.0f), 0);
	EXPECT_EQ(FadeAt(32.0f), 127);
	EXPECT_EQ(FadeAt(33.0f), 255);
	EXPECT_EQ(FadeAt(40.0f), 255);

	EXPECT_EQ(SkyColourAt(0.0f), 0xA0FFFFFFU);
	EXPECT_EQ(SkyColourAt(30.0f), 0xE6414141U);

	EXPECT_FLOAT_EQ(TextShareAt(10.0f), 0.0f);
	EXPECT_FLOAT_EQ(TextShareAt(22.5f), 0.5f);
	EXPECT_FLOAT_EQ(TextShareAt(30.0f), 1.0f);
	EXPECT_FLOAT_EQ(TextShareAt(33.5f), 0.5f);
	EXPECT_FLOAT_EQ(TextShareAt(35.0f), 0.0f);

	EXPECT_FLOAT_EQ(ReturnAt(10.0f), 0.0f);
	EXPECT_NEAR(ReturnAt(20.5f), 0.5f, 1e-6f);
	EXPECT_FLOAT_EQ(ReturnAt(26.0f), 1.0f);

	EXPECT_FLOAT_EQ(FlightAt(30.0f), 1.0f);
	EXPECT_FALSE(EndsAt(35.9f));
	EXPECT_TRUE(EndsAt(36.0f));
}

TEST(LogoEffect, LensAndProjection)
{
	const auto lens = LensOf(k_WorldLens, 0.1f, k_Screen);
	EXPECT_NEAR(lens.extentX, std::tan(k_WorldLens * 0.5f) * 0.1f, 1e-7f);
	EXPECT_NEAR(lens.extentY, lens.extentX * 0.75f, 1e-7f);
	EXPECT_NEAR(lens.clipScaleY, lens.clipScaleX * (4.0f / 3.0f), 1e-6f);

	const auto view = LookAt({0.0f, 0.0f, 100.0f}, glm::vec3(0.0f));
	EXPECT_NEAR(view.forward.z, -1.0f, 1e-6f);
	EXPECT_NEAR(view.up.y, 1.0f, 1e-6f);
	const auto middle = ScreenToWorld(view, lens, k_Screen, {400, 300}, 100.0f);
	EXPECT_NEAR(glm::length(middle), 0.0f, 1e-4f);
	const auto point = ScreenToWorld(view, lens, k_Screen, {600, 150}, 80.0f);
	const auto projected = WorldToScreen(view, lens, k_Screen, point);
	ASSERT_TRUE(projected.has_value());
	EXPECT_NEAR(projected->pixel.x, 600, 1);
	EXPECT_NEAR(projected->pixel.y, 150, 1);
	EXPECT_NEAR(projected->depth, 80.0f, 1e-3f);
	EXPECT_TRUE(projected->onScreen);
	EXPECT_FALSE(WorldToScreen(view, lens, k_Screen, {0.0f, 0.0f, 200.0f}).has_value());
}

TEST(LogoEffect, Pictures)
{
	EXPECT_TRUE(TriesUserLogo(0));
	EXPECT_FALSE(TriesUserLogo(1000));
	EXPECT_TRUE(TriesUserLogo(4999));

	std::vector<uint8_t> file(k_LogoPixels, 7);
	EXPECT_FALSE(ReadUserLogo(file).has_value());
	file.push_back(0);
	file.push_back(0);
	EXPECT_FALSE(ReadUserLogo(file).has_value());
	file.resize(k_LogoPixels);
	for (const char16_t c : std::u16string_view(u"Hi"))
	{
		file.push_back(static_cast<uint8_t>(c & 0xFF));
		file.push_back(static_cast<uint8_t>(c >> 8));
	}
	const auto user = ReadUserLogo(file);
	ASSERT_TRUE(user.has_value());
	EXPECT_EQ(user->text, u"Hi");
	EXPECT_FALSE(user->lionhead);
	EXPECT_EQ(user->rgb.at(0), 7);
	EXPECT_EQ(user->rgb.at(k_LogoBytes - 1), 7);

	EXPECT_FALSE(LionheadLogo(std::vector<uint8_t>(100)).has_value());
	const auto lionhead = LionheadLogo(std::vector<uint8_t>(k_LogoBytes, 3));
	ASSERT_TRUE(lionhead.has_value());
	EXPECT_EQ(lionhead->text, k_LionheadText);
	EXPECT_TRUE(lionhead->lionhead);
}

TEST(LogoEffect, DotsAreSortedByBlueAndMakeTheLogo)
{
	const auto lens = LensOf(k_WorldLens, 1.0f, k_Screen);
	const Effect effect(TestPicture(), k_Screen, lens, Middle);
	const auto dots = effect.GetDots();
	ASSERT_EQ(dots.size(), k_DotCount);
	for (size_t i = 1; i < dots.size(); ++i)
	{
		EXPECT_LE(dots[i - 1].colour & 0xFFU, dots[i].colour & 0xFFU);
	}
	// From the start, each dot is under its pixel of the logo, at the middle depth
	const auto view = LookAt({0.0f, 0.0f, 100.0f}, glm::vec3(0.0f));
	for (const auto& dot : dots)
	{
		const auto projected = WorldToScreen(view, lens, k_Screen, dot.position);
		ASSERT_TRUE(projected.has_value());
		EXPECT_NEAR(projected->depth, 100.0f, 1e-3f);
		EXPECT_NEAR(dot.halfSize, 100.0f / 260.0f, 1e-5f);
		EXPECT_EQ(dot.additive, ((dot.colour >> 8U) & 0xFFU) >= 0x80);
	}
}

TEST(LogoEffect, StepsTheTimeAndTheWheelSlowsIt)
{
	Effect effect(TestPicture(), k_Screen, LensOf(k_WorldLens, 1.0f, k_Screen), Middle);
	auto frame = effect.Step(StillInput(17), Middle);
	EXPECT_FLOAT_EQ(effect.GetTime(), 0.001f);
	// Already a little less than black: the fade drops the fraction
	EXPECT_EQ(frame.fade, 254);
	EXPECT_EQ(frame.logoAlpha, 255);
	EXPECT_EQ(frame.textAlpha, 0);
	frame = effect.Step(StillInput(1000), Middle);
	EXPECT_NEAR(effect.GetTime(), 1.101f, 1e-5f);

	auto slower = StillInput(1);
	slower.wheel = -1000;
	effect.Step(slower, Middle);
	EXPECT_NEAR(effect.GetSpeed(), std::exp(-0.16f), 1e-5f);
	for (int i = 0; i < 20; ++i)
	{
		effect.Step(slower, Middle);
	}
	EXPECT_FLOAT_EQ(effect.GetSpeed(), 0.1f);
	auto faster = StillInput(1);
	faster.wheel = 240;
	for (int i = 0; i < 40; ++i)
	{
		effect.Step(faster, Middle);
	}
	EXPECT_FLOAT_EQ(effect.GetSpeed(), 1.0f);
}

TEST(LogoEffect, EndsAfterTheFlight)
{
	Effect effect(TestPicture(), k_Screen, LensOf(k_WorldLens, 1.0f, k_Screen), Middle);
	Frame frame {};
	int frames = 0;
	while (!effect.Ended() && frames < 10000)
	{
		frame = effect.Step(StillInput(17), Middle);
		++frames;
	}
	EXPECT_TRUE(effect.Ended());
	// About 33 seconds of 17 ms frames
	EXPECT_NEAR(frames * 17, 32727, 40);
	EXPECT_EQ(frame.logoAlpha, 0);
	EXPECT_EQ(frame.fade, 255);
	for (const auto& dot : effect.GetDots())
	{
		EXPECT_EQ(dot.colour >> 24U, 0U);
	}
}

TEST(LogoEffect, TheMouseMovesTheDotsNearThePointer)
{
	Effect still(TestPicture(), k_Screen, LensOf(k_WorldLens, 1.0f, k_Screen), Middle);
	Effect pushed(TestPicture(), k_Screen, LensOf(k_WorldLens, 1.0f, k_Screen), Middle);
	still.Step(StillInput(17), Middle);
	auto input = StillInput(17);
	input.mouseMove = {200, 0};
	pushed.Step(input, Middle);
	size_t moved = 0;
	for (size_t i = 0; i < still.GetDots().size(); ++i)
	{
		if (glm::length(still.GetDots()[i].position - pushed.GetDots()[i].position) > 1e-3f)
		{
			++moved;
		}
	}
	EXPECT_GT(moved, 0U);
	EXPECT_LT(moved, k_DotCount);
}

TEST(LogoEffect, DotCorners)
{
	const auto view = LookAt({0.0f, 0.0f, 100.0f}, glm::vec3(0.0f));
	const Dot dot {.position = glm::vec3(0.0f), .halfSize = 2.0f, .angle = 0.0f, .colour = 0xFFFFFFFFU, .additive = false};
	const auto corners = DotCorners(dot, view, 0.1f);
	ASSERT_TRUE(corners.has_value());
	EXPECT_NEAR(glm::length(corners->at(1) - corners->at(0)), 4.0f, 1e-5f);
	EXPECT_NEAR(glm::dot(corners->at(1) - corners->at(0), view.right), 4.0f, 1e-5f);
	EXPECT_NEAR(glm::dot(corners->at(0) - corners->at(3), view.up), 4.0f, 1e-5f);
	const Dot behind {.position = {0.0f, 0.0f, 150.0f}, .halfSize = 1.0f, .angle = 0.0f, .colour = 0, .additive = false};
	EXPECT_FALSE(DotCorners(behind, view, 0.1f).has_value());
}

TEST(LogoEffect, Layout)
{
	const auto boxes = SponsorPlaces(k_Screen);
	EXPECT_EQ(boxes.logo.min, glm::ivec2(20, 476));
	EXPECT_EQ(boxes.logo.max, glm::ivec2(104, 580));
	EXPECT_EQ(boxes.label.min, glm::ivec2(11, 455));
	EXPECT_EQ(boxes.label.max, glm::ivec2(111, 473));
	const auto tradeMark = TradeMarkPlace(k_Screen);
	EXPECT_EQ(tradeMark.position, glm::vec2(512.0f, 192.0f));
	EXPECT_FLOAT_EQ(tradeMark.size, 11.0f);
	const auto title = TitlePlace(k_Screen, 100.0f);
	EXPECT_FLOAT_EQ(title.position.x, 350.0f);
	EXPECT_NEAR(title.position.y, 428.5714f, 1e-3f);
	EXPECT_FLOAT_EQ(title.size, 30.0f);
}

TEST(LogoEffect, WhoSeesIt)
{
	// A recent AMD processor: family 15, extended; SSE2
	EXPECT_TRUE(ShowsSponsorLogo(0x00B40F40U, 1U << 26U));
	// A recent Intel processor: family 6
	EXPECT_FALSE(ShowsSponsorLogo(0x000906EAU, 1U << 26U));
	EXPECT_FALSE(ShowsSponsorLogo(0x00000F29U, 0U));

	EXPECT_TRUE(startup::PlaysLogoEffect(false, false, false));
	EXPECT_FALSE(startup::PlaysLogoEffect(true, false, false));
	EXPECT_TRUE(startup::PlaysLogoEffect(true, true, false));
	EXPECT_FALSE(startup::PlaysLogoEffect(false, false, true));

	EXPECT_TRUE(GoesOn(false, false, false));
	EXPECT_FALSE(GoesOn(true, false, false));
	EXPECT_FALSE(GoesOn(false, true, false));
	EXPECT_FALSE(GoesOn(false, false, true));
}
