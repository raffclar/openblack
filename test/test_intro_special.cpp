/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The intro's PLAY_JC_SPECIAL pure parts (docs/bw1-notes/intro.md): the light of special 0 (Create, HeadPosition and
// the Draw callback), the hand's clock and the put-down yaw.

#include <cmath>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "ECS/IntroSpecial.h"

namespace intro = openblack::ecs::intro_special;
namespace light = openblack::ecs::intro_special::light;

namespace
{
/// Random(a, b) returning a then b then a ... and counting
struct FakeRandom
{
	int calls = 0;
	float operator()(float a, float b)
	{
		++calls;
		return (calls % 2) == 1 ? a : b;
	}
};

light::Light MakeLight(FakeRandom& random)
{
	return light::Create(intro::k_SonSpot, intro::k_LightDirection, [&random](float a, float b) { return random(a, b); });
}
} // namespace

TEST(IntroSpecial, Constants)
{
	EXPECT_FLOAT_EQ(intro::k_SonSpot.x, 1416.337f);
	EXPECT_FLOAT_EQ(intro::k_SonSpot.z, 2053.503f);
	EXPECT_FLOAT_EQ(intro::k_HandSpot.x, 1494.379f);
	EXPECT_FLOAT_EQ(intro::k_HandSpot.y, -2.5f);
	EXPECT_FLOAT_EQ(intro::k_HandSpot.z, 2091.474f);
	EXPECT_FLOAT_EQ(intro::k_PickUpSpot.y, -1.5f);
	EXPECT_FLOAT_EQ(intro::k_LightDirection.y, -0.24f);
	EXPECT_FLOAT_EQ(intro::k_HandScale, 0.008f);
	EXPECT_EQ(intro::k_SeekMs, 2379);
	// -pi/4 - 15 degrees
	EXPECT_NEAR(intro::PutDownYaw(), -1.0471976f, 1e-6f);
}

TEST(IntroSpecial, AdvanceTime)
{
	bool wrapped = true;
	// hand_intro2.anm: 766 ms, one shot: it stops at 765 and never wraps (so the wrap flag stays 0)
	EXPECT_EQ(intro::AdvanceTime(700, 33, 766, false, wrapped), 733);
	EXPECT_FALSE(wrapped);
	EXPECT_EQ(intro::AdvanceTime(760, 33, 766, false, wrapped), 765);
	EXPECT_FALSE(wrapped);
	EXPECT_EQ(intro::AdvanceTime(765, 33, 766, false, wrapped), 765);
	EXPECT_FALSE(wrapped);
	// hand_intro.anm: 3066 ms, looping: the wrap ends the put-down hand
	EXPECT_EQ(intro::AdvanceTime(3050, 33, 3066, true, wrapped), 17);
	EXPECT_TRUE(wrapped);
	EXPECT_EQ(intro::AdvanceTime(2379, 33, 3066, true, wrapped), 2412);
	EXPECT_FALSE(wrapped);
}

TEST(IntroSpecial, LightCreate)
{
	FakeRandom random;
	const auto made = MakeLight(random);
	EXPECT_EQ(random.calls, 20);                           // one Random(0, 2 pi) per sprite
	EXPECT_NEAR(glm::length(made.direction), 1.0f, 2e-3f); // InverseSquareRoot's table guess
	EXPECT_EQ(made.start.x, intro::k_SonSpot.x + made.direction.x * light::k_Far);
	EXPECT_EQ(made.head, made.start);
	EXPECT_EQ(made.state, light::State::Falling);
	EXPECT_EQ(made.elapsed, 0);
	// the up-beam start is 4000 above-ish: direction is down and towards -x, -z
	EXPECT_GT(made.start.y, 600.0f);
	const auto& s0 = made.sprites.at(0);
	EXPECT_EQ(s0.cell, 48);
	EXPECT_EQ(s0.argb, 0xF2FFFFFFu); // 19 x 255 / 20 = 242
	EXPECT_EQ(s0.size, static_cast<float>(285) * std::bit_cast<float>(0x3D4CCCCDu) + 3.0f);
	EXPECT_EQ(s0.angle, 0.0f); // the fake's first value
	const auto& s5 = made.sprites.at(5);
	EXPECT_EQ(s5.cell, 48);
	const auto& s6 = made.sprites.at(6);
	EXPECT_EQ(s6.cell, 50);
	const float six = static_cast<float>(13 * 15) * std::bit_cast<float>(0x3D4CCCCDu) + 3.0f;
	EXPECT_EQ(s6.size, six + six);
	const float k = static_cast<float>(-6) * 6.0f;
	EXPECT_EQ(s6.position.x, made.direction.x * k + made.start.x);
	const auto& s18 = made.sprites.at(18);
	EXPECT_EQ(s18.argb, 0x28FFFFFFu);
	EXPECT_EQ(s18.size, 80.0f);
	EXPECT_EQ(s18.position, made.start);
	const auto& s19 = made.sprites.at(19);
	EXPECT_EQ(s19.cell, 49);
	EXPECT_EQ(s19.size, 350.0f);
	EXPECT_FLOAT_EQ(s19.height, 0.05f);
	EXPECT_EQ(s19.argb, 0x0EFFFFFFu);
	EXPECT_FLOAT_EQ(s19.angle, -0.39269909f);
	EXPECT_EQ(s19.position.y, made.direction.y * -6.0f + made.start.y);
}

TEST(IntroSpecial, LightHead)
{
	FakeRandom random;
	auto made = MakeLight(random);
	made.elapsed = 8866; // 8866 x 0.45 = 3989.7
	auto head = light::HeadPosition(made);
	EXPECT_FALSE(made.arrived);
	made.elapsed = 8867; // 3990.15: clamped, arrived
	head = light::HeadPosition(made);
	EXPECT_TRUE(made.arrived);
	EXPECT_EQ(head.x, light::k_Travel * made.direction.x + made.start.x);
	// 10 short of the target
	EXPECT_NEAR(glm::length(head - intro::k_SonSpot), 10.0f, 0.1f);
}

TEST(IntroSpecial, LightTimeline)
{
	FakeRandom random;
	auto made = MakeLight(random);
	const light::RandomFn fn = [&random](float a, float b) { return random(a, b); };
	int32_t timer = 0;
	light::Frame frame;
	// paused: no Random in the falling state
	random.calls = 0;
	light::Draw(made, 0, timer, fn, frame);
	EXPECT_EQ(random.calls, 0);
	EXPECT_EQ(frame.drawn.size(), 20u);
	EXPECT_FALSE(frame.depthAlways);
	ASSERT_TRUE(frame.cameraPosition.has_value());
	EXPECT_EQ(frame.cameraPosition->y, (made.start.y - made.direction.y * 150.0f) + 30.0f);
	// a frame of 33 ms: 18 angles and 5 sizes
	light::Draw(made, 33, timer, fn, frame);
	EXPECT_EQ(random.calls, 23);
	EXPECT_EQ(made.elapsed, 33);
	// the five front sprites: (float(15 (19 - i)) x 0.05 + jitter) + 3, the jitter the fake's -2 or 2
	EXPECT_GT(made.sprites.at(0).size, 15.0f);
	// ZFUNC ALWAYS once the fall has run more than 8237 ms
	made.elapsed = 8238;
	light::Draw(made, 33, timer, fn, frame);
	EXPECT_TRUE(frame.depthAlways);
	EXPECT_EQ(made.state, light::State::Falling);
	// it lands: hold 500 ms
	made.elapsed = 8900;
	light::Draw(made, 33, timer, fn, frame);
	EXPECT_EQ(made.state, light::State::Hold);
	EXPECT_EQ(timer, light::k_HoldMs);
	EXPECT_EQ(frame.drawn.size(), 20u); // the landing frame still draws the beam
	// the hold: sprite 0 three times, two Random a frame, Z always
	random.calls = 0;
	light::Draw(made, 100, timer, fn, frame);
	EXPECT_EQ(random.calls, 2);
	EXPECT_EQ(frame.drawn.size(), 3u);
	EXPECT_TRUE(frame.depthAlways);
	EXPECT_EQ(timer, 400);
	light::Draw(made, 0, timer, fn, frame); // paused: still two draws
	EXPECT_EQ(random.calls, 4);
	// 500 ms gone: the fade starts at once, alpha 255 at 1000
	light::Draw(made, 401, timer, fn, frame);
	EXPECT_EQ(made.state, light::State::Fade);
	EXPECT_EQ(frame.drawn.front().argb >> 24, 255u);
	EXPECT_EQ(timer, 1000 - 401);
	// half way: (599 / 1000) x 205 + 50 = 172.795 -> 172
	light::Draw(made, 100, timer, fn, frame);
	EXPECT_EQ(frame.drawn.front().argb >> 24, 172u);
	// the end
	light::Draw(made, 600, timer, fn, frame);
	EXPECT_EQ(made.state, light::State::Done);
	light::Draw(made, 33, timer, fn, frame);
	EXPECT_TRUE(frame.drawn.empty());
}
