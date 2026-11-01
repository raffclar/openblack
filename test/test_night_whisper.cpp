/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <bit>
#include <deque>
#include <string>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "Audio/PanPlace.h"
#include "Help/NightWhisper.h"

using namespace openblack;
using namespace openblack::night_whisper;

namespace
{
/// Random numbers given in order, which keep the limit of each draw
struct FakeDraws
{
	std::deque<float> floats;
	std::deque<uint32_t> wholes;
	std::vector<float> floatLimits;
	std::vector<int32_t> wholeLimits;

	[[nodiscard]] Draws Get()
	{
		return {.floatRand =
		            [this](float limit) {
			            floatLimits.push_back(limit);
			            const auto value = floats.front();
			            floats.pop_front();
			            return value;
		            },
		        .rand =
		            [this](int32_t limit) {
			            wholeLimits.push_back(limit);
			            const auto value = wholes.front();
			            wholes.pop_front();
			            return value;
		            }};
	}
};

/// A clock that gives night or day, and counts its reads
struct FakeClock
{
	bool night {true};
	int reads {0};

	[[nodiscard]] std::function<bool()> Get()
	{
		return [this]() {
			++reads;
			return night;
		};
	}
};

std::vector<std::u16string> Names(std::initializer_list<const char16_t*> names)
{
	return {names.begin(), names.end()};
}
} // namespace

TEST(NightWhisper, TheNamesAreTheHelpTextsInOrderAndTheLastSampleIsAfterTheTenth)
{
	EXPECT_EQ(NameText(0), 4586u);
	EXPECT_EQ(NameText(99), 4685u);
	EXPECT_EQ(NameSample(0), 30u);
	EXPECT_EQ(NameSample(9), 39u);
	EXPECT_EQ(NameSample(10), 41u);
	EXPECT_EQ(NameSample(98), 129u);
	EXPECT_EQ(NameSample(99), 40u);
}

TEST(NightWhisper, SoundCodesAreTheGamesOwn)
{
	const std::u16string letters = u"abcdefghijklmnopqrstuvwxyz";
	const std::vector<uint32_t> codes {0, 1, 2, 3, 0, 1, 2, u'h', 0, 2, 2, 4, 5, 5, 0, 1, 2, 6, 2, 3, 0, 1, u'w', 2, u'y', 2};
	for (size_t i = 0; i < letters.size(); ++i)
	{
		EXPECT_EQ(SoundCode(letters[i]), codes[i]) << static_cast<char>(letters[i]);
	}
	// Capital letters give the same codes, but h w y give their own characters
	EXPECT_EQ(SoundCode(u'B'), 1u);
	EXPECT_EQ(SoundCode(u'H'), static_cast<uint32_t>(u'H'));
	EXPECT_EQ(SoundCode(u'W'), static_cast<uint32_t>(u'W'));
	EXPECT_EQ(SoundCode(u'Y'), static_cast<uint32_t>(u'Y'));
	// Characters that are not letters of the English alphabet give 0
	EXPECT_EQ(SoundCode(u' '), 0u);
	EXPECT_EQ(SoundCode(u'1'), 0u);
	EXPECT_EQ(SoundCode(u'é'), 0u);
	EXPECT_EQ(SoundCode(u'Ł'), 0u);
}

TEST(NightWhisper, TheFirstLetterAndThreeCodesMustMatch)
{
	EXPECT_TRUE(SoundsAlike(u"James", u"James"));
	EXPECT_TRUE(SoundsAlike(u"James", u"Jamez"));
	// The case of the first letter counts
	EXPECT_FALSE(SoundsAlike(u"James", u"james"));
	// A short name matches on the codes it has: the end of a word gives 0
	EXPECT_TRUE(SoundsAlike(u"Jo", u"Joe"));
	// h gives its own code, so John is not Jon
	EXPECT_FALSE(SoundsAlike(u"John", u"Jon"));
	// A doubled code gives one more and the second letter counts again: Matt is (4, 3, 0) and Mat is (3, 0, 0)
	EXPECT_FALSE(SoundsAlike(u"Matt", u"Mat"));
	EXPECT_TRUE(SoundsAlike(u"Matt", u"Matt"));
	// Only three codes count
	EXPECT_TRUE(SoundsAlike(u"Christopher", u"Christ"));
	EXPECT_FALSE(SoundsAlike(u"", u""));
}

TEST(NightWhisper, EachWordAfterASpaceIsTried)
{
	EXPECT_TRUE(AnyWordSoundsAlike(u"James", u"Mr James Smith"));
	EXPECT_TRUE(AnyWordSoundsAlike(u"James", u"James Smith"));
	// Jones has the same codes as James
	EXPECT_TRUE(AnyWordSoundsAlike(u"James", u"Mr Jones"));
	EXPECT_FALSE(AnyWordSoundsAlike(u"James", u"Mr Jack"));
	// After two spaces a word starts with a space and matches nothing, but the next try starts after the second space
	EXPECT_TRUE(AnyWordSoundsAlike(u"James", u"Mr  James"));
	EXPECT_FALSE(AnyWordSoundsAlike(u"James", u""));
}

TEST(NightWhisper, TheFirstNameThatMatchesGivesItsSample)
{
	const auto names = Names({u"James", u"John", u"Peter", u"Pete"});
	EXPECT_EQ(FindNameSample(names, u"Pete"), NameSample(3));
	EXPECT_EQ(FindNameSample(names, u"Peter Pete"), NameSample(2));
	EXPECT_EQ(FindNameSample(names, u"Jo"), std::nullopt);
	EXPECT_EQ(FindNameSample(names, u""), std::nullopt);
}

TEST(NightWhisper, TheSecondNameKeepsThePrintableLettersOfTheProfileName)
{
	EXPECT_EQ(SecondName(u"Jämes Smith"), u"Jmes Smith");
	EXPECT_EQ(SecondName(u"ÄÖ"), u"");
	EXPECT_EQ(SecondName(std::u16string(60, u'a')).size(), k_SecondNameLength);

	const auto names = Names({u"James"});
	EXPECT_EQ(LookUpSample(names, u"James"), NameSample(0));
	// Only the second name matches when the first character is not a letter of the English alphabet
	EXPECT_EQ(LookUpSample(names, u"ÄJames"), NameSample(0));
	EXPECT_EQ(LookUpSample(names, u"Bob"), std::nullopt);
}

TEST(NightWhisper, ACheckWhispersWhenTheCutShareIsBelowTheCount)
{
	EXPECT_FALSE(CheckWhispers(1.0f, 0));
	EXPECT_TRUE(CheckWhispers(1.0f, 1));
	EXPECT_FALSE(CheckWhispers(0.0f, 1000));
	EXPECT_TRUE(CheckWhispers(0.0f, 1001));
	// (1 - 0.125) x 1000 is 875
	EXPECT_FALSE(CheckWhispers(0.5f, 875));
	EXPECT_TRUE(CheckWhispers(0.5f, 876));
}

TEST(NightWhisper, AWhisperDrawsTheRateThePanAndTheVolumeInOrder)
{
	FakeDraws draws {.floats = {0.5f, 0.5f}, .wholes = {1, 45, 0}};
	uint32_t volume = k_FirstVolume;
	const auto whisper = MakeWhisper(77, volume, draws.Get());
	// The rate is 100 x 1.125, cut; the volume is 127 / 1.125, cut, and is kept
	EXPECT_EQ(whisper, (Whisper {.sample = 77, .pitchPercent = 112, .volume = 112, .pan = 45}));
	EXPECT_EQ(volume, 112u);
	EXPECT_EQ(draws.floatLimits, (std::vector<float> {std::bit_cast<float>(0x3F266666u), std::bit_cast<float>(0x3F4CCCCDu)}));
	EXPECT_EQ(draws.wholeLimits, (std::vector<int32_t> {2, 180, 2}));
}

TEST(NightWhisper, TheVolumeWandersFromTheVolumeTheLastWhisperLeft)
{
	FakeDraws draws {.floats = {0.0f, 0.8f}, .wholes = {1, 0, 1}};
	uint32_t volume = 112;
	const auto whisper = MakeWhisper(30, volume, draws.Get());
	// 112 x (1 + 0.8 cubed) is 169.3; the sound plays at 127 but the 169 is kept
	EXPECT_EQ(whisper.volume, 169u);
	EXPECT_EQ(whisper.pitchPercent, 100u);
	EXPECT_EQ(volume, 169u);
}

TEST(NightWhisper, NoCheckOnTheFirstTwoLandsOrWithNoName)
{
	Whisperer whisperer;
	FakeClock clock;
	FakeDraws draws;
	whisperer.Stage(5);
	EXPECT_TRUE(std::holds_alternative<std::monostate>(whisperer.Turn(3, clock.Get(), draws.Get())));
	EXPECT_EQ(clock.reads, 0);
	whisperer.SetSample(30);
	EXPECT_TRUE(std::holds_alternative<std::monostate>(whisperer.Turn(1, clock.Get(), draws.Get())));
	EXPECT_TRUE(std::holds_alternative<std::monostate>(whisperer.Turn(2, clock.Get(), draws.Get())));
	EXPECT_EQ(clock.reads, 0);
	EXPECT_EQ(whisperer.GetCountdown(), 0u);
}

TEST(NightWhisper, TheClockIsCheckedEveryHundredTurnsFromTheHundredAndFirst)
{
	Whisperer whisperer;
	whisperer.SetSample(30);
	FakeClock clock {.night = false};
	FakeDraws draws;
	for (int turn = 0; turn < 100; ++turn)
	{
		(void)whisperer.Turn(3, clock.Get(), draws.Get());
	}
	EXPECT_EQ(clock.reads, 0);
	(void)whisperer.Turn(3, clock.Get(), draws.Get());
	EXPECT_EQ(clock.reads, 1);
	EXPECT_EQ(whisperer.GetCountdown(), 99u);
	for (int turn = 0; turn < 100; ++turn)
	{
		(void)whisperer.Turn(4, clock.Get(), draws.Get());
	}
	EXPECT_EQ(clock.reads, 2);
	// A day check draws nothing and does not count
	EXPECT_EQ(whisperer.GetCount(), 0u);
	EXPECT_TRUE(draws.floatLimits.empty());
}

TEST(NightWhisper, ANightCheckCountsAndAWhisperStartsTheCountAgainAtOne)
{
	Whisperer whisperer;
	whisperer.SetSample(41);
	FakeClock clock;
	FakeDraws draws {.floats = {0.0f}};
	whisperer.Stage(0);
	// With no checks before, nothing is below the count
	EXPECT_TRUE(std::holds_alternative<NoWhisper>(whisperer.Turn(3, clock.Get(), draws.Get())));
	EXPECT_EQ(whisperer.GetCount(), 1u);

	whisperer.Stage(1000);
	draws.floats = {0.5f, 0.5f, 0.5f};
	draws.wholes = {1, 179, 1};
	const auto result = whisperer.Turn(5, clock.Get(), draws.Get());
	ASSERT_TRUE(std::holds_alternative<Whisper>(result));
	const auto& whisper = std::get<Whisper>(result);
	EXPECT_EQ(whisper.sample, 41u);
	EXPECT_EQ(whisper.pan, 179u);
	EXPECT_EQ(whisper.volume, 142u);
	EXPECT_EQ(whisperer.GetVolume(), 142u);
	EXPECT_EQ(whisperer.GetCount(), 1u);
	EXPECT_EQ(whisperer.GetCountdown(), 99u);
}

TEST(NightWhisper, ThePanGivesADirectionFromTheLeftToTheRight)
{
	EXPECT_EQ(audio::PanAzimuth(90), 0u);
	EXPECT_EQ(audio::PanAzimuth(179), 89u);
	EXPECT_EQ(audio::PanAzimuth(0), 270u);
	EXPECT_EQ(audio::PanAzimuth(89), 359u);
	// A pan past 180 plays at 180
	EXPECT_EQ(audio::PanAzimuth(500), 90u);
	const auto right = audio::PanPlace(180);
	EXPECT_NEAR(right.x, 1.0f, 1e-6f);
	EXPECT_NEAR(right.y, 0.0f, 1e-6f);
	const auto left = audio::PanPlace(0);
	EXPECT_NEAR(left.x, -1.0f, 1e-6f);
	const auto front = audio::PanPlace(90);
	EXPECT_NEAR(front.y, 1.0f, 1e-6f);
	EXPECT_EQ(front.z, 0.0f);
}
