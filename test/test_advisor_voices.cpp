/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// help::AdvisorVoices (src/Help/AdvisorVoices.h): which advisor speaks, the delay before its line, talking and just
// stopped, and being cut short, with the sound and the clock faked by the test.

#include <cstdint>

#include <set>
#include <vector>

#include <gtest/gtest.h>

#include "Help/AdvisorVoices.h"

using namespace openblack::help;

namespace
{
constexpr int k_Good = 0;
constexpr int k_Evil = 1;

struct Fixture
{
	uint32_t lines {100};
	uint32_t now {1000};
	std::set<uint32_t> playing;
	float done {0.5f};
	std::vector<uint32_t> started;
	std::vector<uint32_t> stopped;
	std::vector<int32_t> rands;
	AdvisorVoices voices;

	Fixture()
	    : voices(MakeAudio())
	{
	}

	AdvisorVoices::Audio MakeAudio()
	{
		AdvisorVoices::Audio audio;
		audio.lineCount = [this]() { return lines; };
		audio.start = [this](uint32_t line) {
			started.push_back(line);
			playing.insert(line);
			return true;
		};
		audio.isPlaying = [this](uint32_t line) { return playing.contains(line); };
		audio.percentageDone = [this](uint32_t) { return done; };
		audio.stop = [this](uint32_t line) {
			stopped.push_back(line);
			playing.erase(line);
		};
		audio.tickMs = [this]() { return now; };
		audio.localRand = [this](int32_t n) {
			rands.push_back(n);
			return 0u;
		};
		return audio;
	}
};
} // namespace

TEST(AdvisorVoices, DelayNearTheEdges)
{
	EXPECT_EQ(AdvisorVoices::SayDelayMs(0.0f), 0u);
	EXPECT_EQ(AdvisorVoices::SayDelayMs(0.9f), 0u);
	EXPECT_EQ(AdvisorVoices::SayDelayMs(-0.9f), 0u);
	EXPECT_EQ(AdvisorVoices::SayDelayMs(0.95f), 250u);
	EXPECT_EQ(AdvisorVoices::SayDelayMs(-1.0f), 262u);
	EXPECT_EQ(AdvisorVoices::SayDelayMs(1.95f), 500u);
	EXPECT_EQ(AdvisorVoices::SayDelayMs(5.0f), 500u);
}

TEST(AdvisorVoices, ALineStartsAtOnceInTheMiddle)
{
	Fixture f;
	f.voices.Say(k_Good, 7, false, 0.0f);
	EXPECT_EQ(f.started, (std::vector<uint32_t> {7}));
	EXPECT_EQ(f.voices.GetSpeaker(), k_Good);
	EXPECT_EQ(f.voices.GetSentence(), 7u);
	EXPECT_TRUE(f.voices.IsActive(k_Good));
	EXPECT_TRUE(f.voices.IsTalking(k_Good));
	EXPECT_FALSE(f.voices.IsTalking(k_Evil));
	EXPECT_FLOAT_EQ(f.voices.PercentageDone(k_Good), 0.5f);
	EXPECT_FLOAT_EQ(f.voices.PercentageDone(k_Evil), 1.0f);
}

TEST(AdvisorVoices, ALineAtTheEdgeWaitsItsDelay)
{
	Fixture f;
	f.voices.Say(k_Evil, 9, false, 1.0f);
	EXPECT_TRUE(f.started.empty());
	// Waiting counts as talking, and nothing of it is done
	EXPECT_TRUE(f.voices.IsTalking(k_Evil));
	EXPECT_FLOAT_EQ(f.voices.PercentageDone(k_Evil), 0.0f);
	f.now += 261;
	f.voices.Update();
	EXPECT_TRUE(f.started.empty());
	f.now += 1;
	f.voices.Update();
	EXPECT_EQ(f.started, (std::vector<uint32_t> {9}));
}

TEST(AdvisorVoices, LinesTheBankLacksAreNotSaid)
{
	Fixture f;
	f.lines = 5;
	f.voices.Say(k_Good, 6, false, 0.0f);
	f.voices.Say(k_Good, 0, false, 0.0f);
	EXPECT_TRUE(f.started.empty());
	EXPECT_EQ(f.voices.GetSentence(), 0u);
	// It still became the speaker
	EXPECT_EQ(f.voices.GetSpeaker(), k_Good);
}

TEST(AdvisorVoices, TheNewSpeakerStopsTheOther)
{
	Fixture f;
	f.voices.Say(k_Good, 7, false, 0.0f);
	f.voices.Say(k_Evil, 8, false, 0.0f);
	EXPECT_EQ(f.stopped, (std::vector<uint32_t> {7}));
	EXPECT_EQ(f.started, (std::vector<uint32_t> {7, 8}));
	EXPECT_EQ(f.voices.GetSentence(), 8u);
	// Stopping the other's line leaves no speaker, as in the original, so neither counts as talking
	EXPECT_EQ(f.voices.GetSpeaker(), -1);
	EXPECT_FALSE(f.voices.IsTalking(k_Good));
	EXPECT_FALSE(f.voices.IsTalking(k_Evil));
}

TEST(AdvisorVoices, OnlyIfSilentWaitsForItsOwnLine)
{
	Fixture f;
	f.voices.Say(k_Good, 7, false, 0.0f);
	f.voices.Say(k_Good, 8, true, 0.0f);
	EXPECT_EQ(f.started, (std::vector<uint32_t> {7}));
	EXPECT_EQ(f.voices.GetSentence(), 7u);
	// Once its line is over the next one is said
	f.playing.clear();
	f.voices.Say(k_Good, 8, true, 0.0f);
	EXPECT_EQ(f.started, (std::vector<uint32_t> {7, 8}));
}

TEST(AdvisorVoices, AFinishedLineIsStoppedAndJustStoppedLasts200Ms)
{
	Fixture f;
	f.voices.Say(k_Good, 7, false, 0.0f);
	EXPECT_TRUE(f.voices.TalkingOrJustStopped(k_Good));
	EXPECT_TRUE(f.voices.AnyTalking());
	f.playing.clear();
	f.now += 199;
	EXPECT_FALSE(f.voices.IsTalking(k_Good));
	EXPECT_EQ(f.stopped, (std::vector<uint32_t> {7}));
	EXPECT_EQ(f.voices.GetSpeaker(), -1);
	EXPECT_TRUE(f.voices.TalkingOrJustStopped(k_Good));
	EXPECT_TRUE(f.voices.AnyTalking());
	f.now += 1;
	EXPECT_FALSE(f.voices.TalkingOrJustStopped(k_Good));
	EXPECT_FALSE(f.voices.AnyTalking());
}

TEST(AdvisorVoices, StoppedAdvisorsDoNotCountAsTalking)
{
	Fixture f;
	f.voices.Say(k_Good, 7, false, 0.0f);
	f.voices.Stop(k_Good);
	EXPECT_FALSE(f.voices.IsActive(k_Good));
	EXPECT_FALSE(f.voices.AnyTalking());
	EXPECT_EQ(f.stopped, (std::vector<uint32_t> {7}));
}

TEST(AdvisorVoices, StoppingBeforeTheStartCallsItOff)
{
	Fixture f;
	f.voices.Say(k_Good, 7, false, 1.0f);
	f.voices.StopSentence(k_Good);
	f.now += 1000;
	f.voices.Update();
	EXPECT_TRUE(f.started.empty());
}

TEST(AdvisorVoices, InterruptDrawsOneLineAndStops)
{
	Fixture f;
	f.voices.Say(k_Good, 7, false, 0.0f);
	f.voices.Interrupt(k_Evil);
	EXPECT_TRUE(f.rands.empty());
	f.voices.Interrupt(k_Good);
	EXPECT_EQ(f.rands, (std::vector<int32_t> {5}));
	EXPECT_EQ(f.stopped, (std::vector<uint32_t> {7}));
	EXPECT_FALSE(f.voices.IsActive(k_Good));
	// The interruption line is never heard
	EXPECT_EQ(f.started, (std::vector<uint32_t> {7}));
	// A silent advisor is not interrupted
	f.voices.Interrupt(k_Good);
	EXPECT_EQ(f.rands.size(), 1u);
}
