/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// help::spirits (src/Help/Spirits.h): AdvisorSpiritController, the spirit script calls and AdvisorSpirit's hover motion (the
// hover splines, SetState and the update) as pure logic, with the random streams and the camera given by the test. No .hd is
// needed: the dudes have no clips, so only the motion runs.

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <string>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "Common/Zoomer.h"
#include "Help/Spirits.h"

using namespace openblack;
using namespace openblack::help::spirits;

namespace
{
constexpr Screen k_Screen {640, 480};

struct Fixture
{
	bool randomMax {false};
	std::vector<int32_t> randCalls;
	std::vector<glm::vec2> randomCalls; ///< the (a, b) of every Random
	DudeData good;
	DudeData evil;
	AdvisorSpiritController control;

	Fixture()
	    : control(good, evil, MakeQueries(), k_Screen)
	{
	}

	Queries MakeQueries()
	{
		Queries q;
		q.localRand = [this](int32_t n) {
			randCalls.push_back(n);
			return randomMax && n > 0 ? static_cast<uint32_t>(n - 1) : 0u;
		};
		q.localFloatRand = [](float) { return 0.0f; };
		q.random = [this](float a, float b) {
			randomCalls.emplace_back(a, b);
			return a;
		};
		return q;
	}

	void Frames(int count, float dt = 0.1f)
	{
		FrameInput input;
		input.dt = dt;
		input.frameMs = static_cast<int32_t>(dt * 1000.0f + 0.5f);
		input.screen = k_Screen;
		input.mouse = {320, 240};
		for (int i = 0; i < count; ++i)
		{
			input.tickMs += static_cast<uint32_t>(input.frameMs);
			control.Update(input);
		}
	}
};

// hover space: hx = (px - W/2) / (W/2), hy = (py - H/2) / (W/2)
float Hx(int px)
{
	return static_cast<float>(px - 320) / 320.0f;
}
float Hy(int py)
{
	return static_cast<float>(py - 240) / 320.0f;
}
} // namespace

TEST(Spirits, StartAtHome)
{
	Fixture f;
	for (int d = 0; d < k_Dudes; ++d)
	{
		EXPECT_EQ(f.control.State(d), ControlState::Home);
		EXPECT_EQ(f.control.Dude(d).State(), dude_state::k_Hover);
	}
	// the off-screen home anchors
	EXPECT_EQ(f.control.Anchor(0), glm::ivec2(320, 720));
	EXPECT_EQ(f.control.Anchor(1), glm::ivec2(-320, 240));
	EXPECT_EQ(f.control.Anchor(2), glm::ivec2(320, -240));
	EXPECT_EQ(f.control.Anchor(3), glm::ivec2(960, 240));
}

TEST(Spirits, EjectFromHomeFliesIntoTheMiddle)
{
	for (const bool high : {false, true})
	{
		Fixture f;
		f.randomMax = high;
		f.control.SpiritEject(1, false); // CHL 7 from a challenge script: Eject
		EXPECT_EQ(f.control.State(0), ControlState::Out);
		EXPECT_EQ(f.control.Timer(0), 0.0f);
		// LocalRand(W/2) + W/4 then LocalRand(H/2) + H/4, in that order
		ASSERT_GE(f.randCalls.size(), 2u);
		EXPECT_EQ(f.randCalls[0], 320);
		EXPECT_EQ(f.randCalls[1], 240);
		const AdvisorSpirit& d = f.control.Dude(0);
		const float x = d.HoverX().GetDestination();
		const float y = d.HoverY().GetDestination();
		EXPECT_GE(x, Hx(160));
		EXPECT_LE(x, Hx(160 + 319));
		EXPECT_GE(y, Hy(120));
		EXPECT_LE(y, Hy(120 + 239));
		EXPECT_FLOAT_EQ(x, high ? Hx(479) : Hx(160));
		EXPECT_FLOAT_EQ(y, high ? Hy(359) : Hy(120));
		EXPECT_FLOAT_EQ(d.HoverX().GetDuration(), 1.0f);
		EXPECT_EQ(d.AlphaTarget(), 1.0f);
		EXPECT_EQ(d.EmotionTarget(), 0u);
		EXPECT_EQ(d.EmotionPeak(), 1.0f);
	}
}

TEST(Spirits, EjectFromClingStaysInTheMiddleThird)
{
	Fixture f;
	f.control.SpiritCling(1, 0.0f, 0.0f);
	ASSERT_EQ(f.control.State(0), ControlState::Clinging);
	f.randCalls.clear();
	f.randomMax = true;
	f.control.SpiritEject(1, false);
	EXPECT_EQ(f.control.State(0), ControlState::Clinging); // Eject does not change a clinging state
	ASSERT_GE(f.randCalls.size(), 2u);
	EXPECT_EQ(f.randCalls[0], 213); // W/3
	EXPECT_EQ(f.randCalls[1], 160); // H/3
}

TEST(Spirits, AppearSpots)
{
	Fixture f;
	f.control.SpiritEject(1, true); // CHL 300 / CHL 7 from a help script: Appear
	f.control.SpiritEject(2, true);
	const AdvisorSpirit& good = f.control.Dude(0);
	const AdvisorSpirit& evil = f.control.Dude(1);
	EXPECT_EQ(f.control.State(0), ControlState::Out);
	EXPECT_EQ(f.control.State(1), ControlState::Out);
	EXPECT_FLOAT_EQ(good.Hover().x, Hx(160)); // (W/4, H/2)
	EXPECT_FLOAT_EQ(good.Hover().y, 0.0f);
	EXPECT_FLOAT_EQ(evil.Hover().x, Hx(480)); // (3W/4, H/2)
	EXPECT_FLOAT_EQ(evil.Hover().y, 0.0f);
	EXPECT_EQ(good.Alpha(), 0.0f);
	EXPECT_EQ(good.AlphaTarget(), 1.0f);
	EXPECT_TRUE(good.PuffRunning());
	// fades in at 3/s
	f.Frames(1);
	EXPECT_NEAR(f.control.Dude(0).Alpha(), 0.3f, 1e-5f);
}

TEST(Spirits, PuffParticlesDrawRandomInTheExeOrder)
{
	// grey, vx, vy, vz, size, spin: the order of the random draws (at the puff's start and at the particles' creation)
	const std::array<glm::vec2, 6> order = {glm::vec2(116.0f, 250.0f), glm::vec2(-0.8f, 0.8f), glm::vec2(-1.9f, 1.5f),
	                                        glm::vec2(-1.0f, 1.0f),    glm::vec2(4.0f, 8.0f),  glm::vec2(-2.0f, 2.0f)};
	Fixture f;
	AdvisorSpirit& d = f.control.Dude(0);
	f.control.SpiritEject(1, true); // Appear: StartPuff before the particles exist draws nothing
	EXPECT_TRUE(d.PuffRunning());
	EXPECT_FALSE(d.PuffParticles().has_value());
	f.randomCalls.clear();
	f.Frames(1); // the first draw of the puff makes them, after the dude's update
	ASSERT_TRUE(d.PuffParticles().has_value());
	ASSERT_GE(f.randomCalls.size(), 6 * k_PuffParticles);
	const size_t first = f.randomCalls.size() - 6 * k_PuffParticles;
	for (size_t i = 0; i < 6 * k_PuffParticles; ++i)
	{
		EXPECT_EQ(f.randomCalls[first + i], order.at(i % 6)) << i;
	}
	const PuffParticle& p = d.PuffParticles()->at(0);
	EXPECT_EQ(p.grey, 0x747474u); // 116 truncated, three bytes
	// drawVelocityY is the vy before this frame's drift, which has already moved velocity.y (UpdateDraw)
	EXPECT_FLOAT_EQ(p.drawVelocityY, -1.9f);
	EXPECT_EQ(p.k, (1.0f - -1.9f) + 1.0f);
	EXPECT_GT(p.age, 0.0f); // stepped by this frame's draw
	// a second puff draws them again at once, in StartPuff itself
	f.randomCalls.clear();
	d.StartPuff();
	ASSERT_EQ(f.randomCalls.size(), 6 * k_PuffParticles);
	for (size_t i = 0; i < f.randomCalls.size(); ++i)
	{
		EXPECT_EQ(f.randomCalls[i], order.at(i % 6)) << i;
	}
	EXPECT_EQ(d.PuffParticles()->at(0).age, 0.0f);
}

TEST(Spirits, HomeTakesOneSecond)
{
	Fixture f;
	f.control.SpiritEject(1, false);
	f.control.SpiritHome(1, false); // CHL 8 from a challenge script: Home
	EXPECT_EQ(f.control.State(0), ControlState::GoingHome);
	f.Frames(9);
	EXPECT_EQ(f.control.State(0), ControlState::GoingHome);
	f.Frames(2);
	EXPECT_EQ(f.control.State(0), ControlState::Home);
	EXPECT_EQ(f.control.Dude(0).State(), dude_state::k_Hover);
}

TEST(Spirits, VanishWaitsForThePuff)
{
	Fixture f;
	f.control.SpiritEject(2, false);
	f.control.SpiritHome(2, true); // CHL 301: Vanish
	const AdvisorSpirit& d = f.control.Dude(1);
	EXPECT_EQ(f.control.State(1), ControlState::GoingHome);
	EXPECT_EQ(d.AlphaTarget(), 0.0f);
	EXPECT_TRUE(d.PuffRunning());
	f.Frames(5);
	EXPECT_NEAR(d.Alpha(), 0.0f, 1e-6f); // fades out at 2/s
	f.Frames(6);                         // 1.1 s: the timer is past 1 s but the puff still runs
	EXPECT_EQ(f.control.State(1), ControlState::GoingHome);
	f.Frames(20); // the puff ends after (255 / 32) / 0.0032 ms of time units
	EXPECT_FALSE(d.PuffRunning());
	EXPECT_EQ(f.control.State(1), ControlState::Home);
}

TEST(Spirits, ClingEdges)
{
	Fixture f;
	f.control.SpiritCling(1, 0.0f, 0.0f); // (0, 0): the default edge, good on the right
	f.control.SpiritCling(2, 0.0f, 0.0f); // evil on the left
	const AdvisorSpirit& good = f.control.Dude(0);
	const AdvisorSpirit& evil = f.control.Dude(1);
	EXPECT_EQ(f.control.State(0), ControlState::Clinging);
	EXPECT_EQ(good.ClingEdge(), Edge::Right);
	EXPECT_FLOAT_EQ(good.ClingTarget().x, 1.04f);
	EXPECT_EQ(evil.ClingEdge(), Edge::Left);
	EXPECT_FLOAT_EQ(evil.ClingTarget().x, -1.04f);
	// from home: snapped to the edge's off-screen anchor first, then arriving
	EXPECT_FLOAT_EQ(good.Hover().x, Hx(960));
	EXPECT_FLOAT_EQ(evil.Hover().x, Hx(-320));
	EXPECT_EQ(good.State(), dude_state::k_ClingArrive);
	EXPECT_FLOAT_EQ(good.HoverX().GetDestination(), 1.04f);

	// |x| > 1.28205 |y| -> left / right, else bottom / top
	f.control.SpiritCling(1, 0.5f, 1.0f); // hx 0, hy 0.75
	EXPECT_EQ(good.ClingEdge(), Edge::Bottom);
	EXPECT_FLOAT_EQ(good.ClingTarget().y, 0.78f);
	f.control.SpiritCling(1, 0.2f, 1.0f); // hx -0.6, hy 0.75
	EXPECT_EQ(good.ClingEdge(), Edge::Bottom);
	f.control.SpiritCling(1, 0.0f, 0.5f); // hx -1, hy 0
	EXPECT_EQ(good.ClingEdge(), Edge::Left);
	f.control.SpiritCling(1, 0.5f, 0.0f); // hx 0, hy -0.75
	EXPECT_EQ(good.ClingEdge(), Edge::Top);
	EXPECT_FLOAT_EQ(good.ClingTarget().y, -0.78f);
}

TEST(Spirits, FlyReachesItsTargetInOneSecond)
{
	Fixture f;
	f.control.SpiritEject(1, false);
	f.control.SpiritEject(2, false);
	// as in Land 1's FollowUs challenge script
	f.control.SpiritFly(1, 0.8f, 0.75f);
	f.control.SpiritFly(2, 0.2f, 0.75f);
	const glm::vec2 goodStart = f.control.Dude(0).Hover();
	f.Frames(5);
	const glm::vec2 goodMid = f.control.Dude(0).Hover();
	EXPECT_GT(goodMid.x, std::min(goodStart.x, Hx(512)));
	EXPECT_LT(goodMid.x, std::max(goodStart.x, Hx(512)));
	f.Frames(5);
	EXPECT_FLOAT_EQ(f.control.Dude(0).Hover().x, Hx(512)); // 0.8 W
	EXPECT_FLOAT_EQ(f.control.Dude(0).Hover().y, Hy(360)); // 0.75 H
	EXPECT_FLOAT_EQ(f.control.Dude(1).Hover().x, Hx(128));
	EXPECT_FLOAT_EQ(f.control.Dude(1).Hover().y, Hy(360));
	EXPECT_EQ(f.control.Dude(0).HoverX().GetSpeed(), 0.0f);
}

TEST(Spirits, FlyIgnoredAtHome)
{
	Fixture f;
	const glm::vec2 before = f.control.Dude(0).Hover();
	f.control.SpiritFly(1, 0.8f, 0.75f);
	EXPECT_EQ(f.control.Dude(0).HoverX().GetDestination(), before.x);
}

TEST(Spirits, HoverClamps)
{
	Fixture f;
	AdvisorSpirit& d = f.control.Dude(0);
	d.SetHoverX(2.0f, 1.0f, true);
	EXPECT_FLOAT_EQ(d.HoverX().GetDestination(), 0.75f);
	d.SetHoverX(-2.0f, 1.0f, true);
	EXPECT_FLOAT_EQ(d.HoverX().GetDestination(), -0.75f);
	d.SetHoverY(1.0f, 1.0f, true);
	EXPECT_FLOAT_EQ(d.HoverY().GetDestination(), 0.6f);
	d.SetHoverY(-1.0f, 1.0f, true);
	EXPECT_FLOAT_EQ(d.HoverY().GetDestination(), -0.6f);
	d.SetHoverX(2.0f, 1.0f, false);
	EXPECT_FLOAT_EQ(d.HoverX().GetDestination(), 2.0f);
	// below 0.001 s the spline snaps
	d.SetHoverY(0.25f, 0.0005f, true);
	EXPECT_FLOAT_EQ(d.HoverY().GetValue(), 0.25f);
}

TEST(Spirits, FocusScale)
{
	Fixture f;
	f.Frames(1);
	// 0.8 e^(-+0.5 (0.4 - 0.5))
	EXPECT_NEAR(f.control.Dude(0).ModelScale(), 0.841, 5e-4);
	EXPECT_NEAR(f.control.Dude(1).ModelScale(), 0.761, 5e-4);
	// the original's values with the FPU at 24 bits (the exp rounded per step): 0x3F574CE2 / 0x3F42CFD2; the double
	// 0.8 e^-0.05 would be one ulp below for dude 1
	EXPECT_EQ(f.control.Dude(0).ModelScale(), 0.841016889f);
	EXPECT_EQ(f.control.Dude(1).ModelScale(), 0.760983586f);
	EXPECT_EQ(f.control.Focus(), 0);
}

TEST(Spirits, PointSide)
{
	Fixture f;
	f.control.SpiritEject(1, false);
	AdvisorSpirit& d = f.control.Dude(0);
	d.SetPosition({480, 240}); // hx 0.5
	// LocalRand(2) = 1 gives the random default side 1: the rule must override it
	f.randomMax = true;
	f.randCalls.clear();
	d.ScreenPoint({320, 240}); // target hx 0, left of the dude: side 2, the right arm, x + 0.2
	// the LocalRand(2) is drawn before the rules
	EXPECT_NE(std::find(f.randCalls.begin(), f.randCalls.end(), 2), f.randCalls.end());
	EXPECT_EQ(d.State(), dude_state::k_PointIntroR);
	EXPECT_FLOAT_EQ(d.HoverX().GetDestination(), 0.2f);
	d.SetState(dude_state::k_Hover, true);
	// LocalRand(2) = 0 gives the random default side 2: the rule must override it again
	f.randomMax = false;
	d.SetPosition({160, 240}); // hx -0.5: target on its right: side 1, x - 0.2
	d.ScreenPoint({320, 240});
	EXPECT_EQ(d.State(), dude_state::k_PointIntroL);
	EXPECT_FLOAT_EQ(d.HoverX().GetDestination(), -0.2f);
}

TEST(Spirits, PlayAnimAndPlayed)
{
	Fixture f;
	EXPECT_FALSE(f.control.SpiritPlayingAnim(1));
	f.control.SpiritPlayAnim(1, 0.5f, 0.5f, 24, 1.0f); // ejects first
	EXPECT_EQ(f.control.State(0), ControlState::Out);
	EXPECT_EQ(f.control.Dude(0).State(), dude_state::k_FlyToAnim);
	EXPECT_TRUE(f.control.SpiritPlayingAnim(1));      // SPIRIT_PLAYED pushes the negation
	f.control.SpiritPlayAnim(1, 0.5f, 0.5f, 0, 1.0f); // anim 0 stops it
	EXPECT_FALSE(f.control.SpiritPlayingAnim(1));
}

TEST(Spirits, AudioTags)
{
	int errors = 0;
	std::string word;
	auto tags = ParseAudioTags("[TE pleased TA knockscreen]", 1.5f, word, &errors);
	ASSERT_EQ(tags.size(), 2u);
	EXPECT_EQ(errors, 0);
	EXPECT_EQ(tags[0].who, 0);
	EXPECT_EQ(tags[0].action, 4);
	EXPECT_EQ(tags[0].index, 1); // Pleased
	EXPECT_EQ(tags[0].value, 100);
	EXPECT_EQ(tags[0].time, 1.5f);
	EXPECT_EQ(tags[1].action, 1);
	EXPECT_EQ(tags[1].index, 58); // KnockScreen
	tags = ParseAudioTags("[TA pray OA dismiss]", 0.0f, word, &errors);
	ASSERT_EQ(tags.size(), 2u);
	EXPECT_EQ(tags[0].index, 51);
	EXPECT_EQ(tags[1].who, 1);
	EXPECT_EQ(tags[1].index, 55);
	tags = ParseAudioTags("[TE sad40]", 0.0f, word, &errors);
	ASSERT_EQ(tags.size(), 1u);
	EXPECT_EQ(tags[0].index, 3);
	EXPECT_EQ(tags[0].value, 40);
	tags = ParseAudioTags("[GLS camera]", 0.0f, word, &errors);
	ASSERT_EQ(tags.size(), 1u);
	EXPECT_EQ(tags[0].who, 2);
	EXPECT_EQ(tags[0].action, 6);
	EXPECT_EQ(tags[0].index, 2);
	EXPECT_EQ(errors, 0);

	// FireTag: an emotion tag on the speaker
	Fixture f;
	AdvisorSpirit& d = f.control.Dude(1);
	d.FireTag(ParseAudioTags("[TE furious50]", 0.0f, word)[0], true, true);
	EXPECT_EQ(d.EmotionTarget(), 7u);
	EXPECT_FLOAT_EQ(d.EmotionPeak(), 0.5f);
	// "G" from the evil spirit goes to its partner
	d.partner = &f.control.Dude(0);
	d.FireTag(ParseAudioTags("[GA shrug]", 0.0f, word)[0], true, true);
	EXPECT_EQ(f.control.Dude(0).SlotMode(64), 1);
	EXPECT_EQ(d.SlotMode(64), 0);
}

TEST(Spirits, AudioTagsFromEveryBracket)
{
	int errors = 0;
	std::string word;
	// Text outside the brackets is skipped, and every bracket is read
	const auto tags = ParseAudioTags("well [TE sad] then [TA pray]", 0.0f, word, &errors);
	ASSERT_EQ(tags.size(), 2u);
	EXPECT_EQ(errors, 0);
	EXPECT_EQ(tags[0].action, 4);
	EXPECT_EQ(tags[1].index, 51);
	EXPECT_TRUE(ParseAudioTags("no tags here", 0.0f, word, &errors).empty());
	// An unclosed bracket is an error
	errors = 0;
	std::ignore = ParseAudioTags("[TE sad", 0.0f, word, &errors);
	EXPECT_EQ(errors, 1);
}

TEST(Spirits, OneBadLabelDropsTheSentencesTags)
{
	std::string word;
	const std::array good {TagLabel {"[TE sad]", 0.5f}, TagLabel {"[TA pray]", 1.0f}};
	const auto tags = BuildSentenceTags(good, word);
	ASSERT_EQ(tags.size(), 2u);
	EXPECT_EQ(tags[0].time, 0.5f);
	EXPECT_EQ(tags[1].time, 1.0f);
	const std::array bad {TagLabel {"[TE sad]", 0.5f}, TagLabel {"[TA pray", 1.0f}};
	EXPECT_TRUE(BuildSentenceTags(bad, word).empty());
}

TEST(Spirits, AnimSoundsPlayAsTheirPointsArePassed)
{
	const std::array events {helpdude::SoundEvent {1, 1, 0.1f}, helpdude::SoundEvent {2, 1, 0.5f},
	                         helpdude::SoundEvent {3, 1, 0.9f}};
	float last = -1.0f;
	// Not heard before: from where it is now
	EXPECT_TRUE(CrossedSounds(events, last, 0.0f).empty());
	auto crossed = CrossedSounds(events, last, 0.2f);
	ASSERT_EQ(crossed.size(), 1u);
	EXPECT_EQ(crossed[0].sample, 1u);
	// A jump of more than half the anim plays nothing, but is remembered
	EXPECT_TRUE(CrossedSounds(events, last, 0.95f).empty());
	EXPECT_FLOAT_EQ(last, 0.95f);
	// Round the end of the loop: both the end's and the start's points
	last = 0.85f;
	crossed = CrossedSounds(events, last, 1.15f);
	ASSERT_EQ(crossed.size(), 2u);
	EXPECT_EQ(crossed[0].sample, 1u);
	EXPECT_EQ(crossed[1].sample, 3u);
	EXPECT_FLOAT_EQ(last, 0.15f);
	// Both ends are included: held on a point, it plays every time
	last = 0.5f;
	EXPECT_EQ(CrossedSounds(events, last, 0.5f).size(), 1u);
	EXPECT_EQ(CrossedSounds(events, last, 0.5f).size(), 1u);
}

TEST(Spirits, TrailTakesAPointEveryFifthAndSparksDrawRandoms)
{
	Fixture f;
	AdvisorSpirit& d = f.control.Dude(0);
	d.ResetTrail();
	const glm::vec3 start = d.GetTrail().points[0];
	EXPECT_TRUE(std::ranges::all_of(d.GetTrail().points, [&](const glm::vec3& p) { return p == start; }));
	f.randomCalls.clear();
	d.UpdateTrail(0.1f);
	d.UpdateTrail(0.1f);
	EXPECT_EQ(d.GetTrail().head, 0u); // 0.2 is not past 0.2
	d.UpdateTrail(0.1f);
	EXPECT_EQ(d.GetTrail().head, 1u);
	EXPECT_TRUE(f.randomCalls.empty());
	// The sparks respawn every 16, each with five draws in this order
	d.UpdateTrail(16.0f);
	ASSERT_EQ(f.randomCalls.size(), 5u * Trail::k_Sparks);
	EXPECT_EQ(f.randomCalls[0], glm::vec2(-0.5f, 0.0f));
	EXPECT_EQ(f.randomCalls[1], glm::vec2(-0.1f, 0.1f));
	EXPECT_EQ(f.randomCalls[2], glm::vec2(-0.1f, 0.1f));
	EXPECT_EQ(f.randomCalls[3], glm::vec2(-0.3f, 0.3f));
	EXPECT_EQ(f.randomCalls[4], glm::vec2(-0.1f, 0.1f));
	// A reset leaves where the next point goes
	d.ResetTrail();
	EXPECT_EQ(d.GetTrail().head, 2u);
}

TEST(Spirits, TrailStripNarrowAndFaintWhenStill)
{
	Fixture f;
	AdvisorSpirit& d = f.control.Dude(1);
	d.ResetTrail();
	const auto strip = d.TrailStrip();
	// Still points: no width, the faintest alpha, both sides on the point
	EXPECT_EQ(strip[0].argb, (20u << 24u) | 0xFFFFFFu);
	EXPECT_EQ(strip[0].hover, strip[1].hover);
	// The evil band's edge is at the bottom of the texture, the middle at a half
	EXPECT_EQ(strip[2].uv, glm::vec2(0.03125f, 0.0f));
	EXPECT_EQ(strip[3].uv, glm::vec2(0.03125f, 0.5f));
	const auto indices = TrailIndices();
	EXPECT_EQ(indices[0], 60);
	EXPECT_EQ(indices[1], 61);
	EXPECT_EQ(indices[2], 63);
}

TEST(Spirits, ZoomerSolvesAsTheGameDoes)
{
	Zoomer z;
	z.Reset(0.0f);
	z.SetDestination(1.0f, 1.0f);
	z.Update(0.5f);
	EXPECT_GT(z.GetValue(), 0.0f);
	EXPECT_LT(z.GetValue(), 1.0f);
	z.Update(0.5f);
	EXPECT_EQ(z.GetValue(), 1.0f);
	// No time, or not a number, puts it there
	z.SetDestination(2.0f, std::nanf(""));
	EXPECT_EQ(z.GetValue(), 2.0f);
	// A move shorter than about 0.05 s barely moves, then jumps at its end
	z.SetDestination(3.0f, 0.02f);
	z.Update(0.01f);
	EXPECT_LT(z.GetValue() - 2.0f, 0.01f);
	z.Update(0.01f);
	EXPECT_EQ(z.GetValue(), 3.0f);
}
