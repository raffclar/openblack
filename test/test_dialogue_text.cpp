/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// help::DialogueText (src/Help/DialogueText.h): the scripts' texts, their reading time, the wait for a click, the voices
// and the draw gate, with the game's clock, texts and voices faked by the test.

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Help/DialogueText.h"

using namespace openblack::help;

namespace
{
struct Fixture
{
	struct FakeText
	{
		int32_t narrator {0};
		bool important {false};
		std::u16string text;
		std::optional<DialogueVoice> voice;
	};

	std::map<uint32_t, FakeText> texts;
	uint32_t turn {100};
	int32_t nowMs {10000};
	bool inTemple {false};
	bool advisorsTalking {false};
	bool narrationSounding {false};
	bool wideScreen {false};
	std::vector<std::pair<int32_t, uint32_t>> advisorSaid;
	std::vector<uint32_t> narrated;
	int interrupts {0};
	int narrationStops {0};

	[[nodiscard]] DialogueText Make(DialogueText::Settings settings = {})
	{
		DialogueText::Queries queries;
		queries.text = [this](uint32_t number) {
			const auto it = texts.find(number);
			if (it == texts.end())
			{
				return DialogueText::Text {};
			}
			return DialogueText::Text {
			    .narrator = it->second.narrator, .important = it->second.important, .text = it->second.text};
		};
		queries.voice = [this](uint32_t number) -> std::optional<DialogueVoice> {
			const auto it = texts.find(number);
			return it == texts.end() ? std::nullopt : it->second.voice;
		};
		queries.turn = [this]() { return turn; };
		queries.nowMs = [this]() { return nowMs; };
		queries.inTemple = [this]() { return inTemple; };
		queries.advisorsTalking = [this]() { return advisorsTalking; };
		queries.narrationSounding = [this](uint32_t) { return narrationSounding; };
		queries.scriptWideScreen = [this]() { return wideScreen; };
		DialogueText::Hooks hooks;
		hooks.advisorSays = [this](int32_t narrator, uint32_t number) { advisorSaid.emplace_back(narrator, number); };
		hooks.narrate = [this](uint32_t number) { narrated.push_back(number); };
		hooks.interruptAdvisors = [this]() { ++interrupts; };
		hooks.stopNarration = [this]() { ++narrationStops; };
		return {480, false, settings, std::move(queries), std::move(hooks)};
	}
};
} // namespace

TEST(DialogueText, ReadSpeedFactor)
{
	EXPECT_FLOAT_EQ(ReadSpeedFactor(0.0f), 3.0f);
	EXPECT_FLOAT_EQ(ReadSpeedFactor(0.25f), 2.0f);
	EXPECT_FLOAT_EQ(ReadSpeedFactor(0.5f), 1.0f);
	EXPECT_FLOAT_EQ(ReadSpeedFactor(0.75f), 0.6f);
	EXPECT_FLOAT_EQ(ReadSpeedFactor(1.0f), 0.2f);
	// Not a number counts as slow
	EXPECT_TRUE(std::isnan(ReadSpeedFactor(std::nanf(""))));
}

TEST(DialogueText, ReadingTimeIsFiveTurnsAWordAndEightMore)
{
	Fixture f;
	f.texts[10] = {.text = u"one two three"};
	auto dialogue = f.Make();
	EXPECT_TRUE(dialogue.RunText(false, 10, 0));
	EXPECT_EQ(dialogue.GetCurrentText(), 10u);
	// 23 turns of 100 ms at the normal reading speed
	EXPECT_EQ(dialogue.GetEndTurn(), 123u);
	EXPECT_EQ(dialogue.GetEndMs(), 12300);
	EXPECT_FALSE(dialogue.IsTextRead());
	f.turn = 123;
	EXPECT_FALSE(dialogue.IsTextRead());
	f.turn = 124;
	EXPECT_TRUE(dialogue.IsTextRead());
}

TEST(DialogueText, SlowReadersGetLonger)
{
	Fixture f;
	f.texts[10] = {.text = u"one two three"};
	auto dialogue = f.Make({.readSpeed = 0.0f});
	dialogue.RunText(false, 10, 0);
	EXPECT_EQ(dialogue.GetEndTurn(), 169u);
	EXPECT_EQ(dialogue.GetEndMs(), 16900);
}

TEST(DialogueText, InTheTempleTheTimeIsInMilliseconds)
{
	Fixture f;
	f.inTemple = true;
	f.texts[10] = {.text = u"one two three"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 10, 0);
	f.nowMs = 12300;
	EXPECT_FALSE(dialogue.IsTextRead());
	f.nowMs = 12301;
	EXPECT_TRUE(dialogue.IsTextRead());
}

TEST(DialogueText, InvalidTextShowsTheFirst)
{
	Fixture f;
	auto dialogue = f.Make();
	EXPECT_FALSE(dialogue.RunText(false, k_HelpTextCount, 0));
	EXPECT_EQ(dialogue.GetCurrentText(), 0u);
	EXPECT_TRUE(dialogue.RunText(false, k_HelpTextCount - 1, 0));
}

TEST(DialogueText, WaitsForAClick)
{
	Fixture f;
	f.texts[10] = {.text = u"click me"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 10, 1);
	EXPECT_TRUE(dialogue.IsWaitingForClick());
	f.turn += 1000;
	EXPECT_FALSE(dialogue.IsTextRead());
	// Shown a second: the click only ends the wait outside the cut scenes
	dialogue.ProcessClick(true, false);
	EXPECT_FALSE(dialogue.IsWaitingForClick());
	EXPECT_TRUE(dialogue.IsTextRead());
	EXPECT_EQ(f.interrupts, 0);
}

TEST(DialogueText, TheContinueCueFadesInWhileTheTextWaits)
{
	Fixture f;
	f.texts[10] = {.text = u"click me"};
	f.texts[11] = {.text = u"no click"};
	auto dialogue = f.Make();
	// Nothing waits: no cue
	dialogue.RunText(false, 11, 0);
	dialogue.AdvanceClickCue(0.5f);
	EXPECT_FALSE(dialogue.GetClickCueShare().has_value());
	// It fades in over a second of real time
	dialogue.RunText(false, 10, 1);
	EXPECT_FLOAT_EQ(dialogue.GetClickCueShare().value_or(-1.0f), 0.0f);
	dialogue.AdvanceClickCue(0.25f);
	EXPECT_FLOAT_EQ(dialogue.GetClickCueShare().value_or(-1.0f), 0.25f);
	dialogue.AdvanceClickCue(2.0f);
	EXPECT_FLOAT_EQ(dialogue.GetClickCueShare().value_or(-1.0f), 1.0f);
	// The next waiting text starts it again
	dialogue.RunText(false, 10, 1);
	EXPECT_FLOAT_EQ(dialogue.GetClickCueShare().value_or(-1.0f), 0.0f);
	// The click takes it away at once
	f.turn += 1000;
	dialogue.ProcessClick(true, false);
	EXPECT_FALSE(dialogue.GetClickCueShare().has_value());
}

TEST(DialogueText, TheControlIconsFadeInOverAThirdOfASecond)
{
	Fixture f;
	f.texts[10] = {.text = u"click me $m2"};
	f.texts[11] = {.text = u"keys $m15 $m7 $m8"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 11, 0);
	EXPECT_FLOAT_EQ(dialogue.GetKeyIconShare(), 0.0f);
	dialogue.AdvanceControlIcons(0.15f);
	EXPECT_FLOAT_EQ(dialogue.GetKeyIconShare(), 0.5f);
	EXPECT_FLOAT_EQ(dialogue.GetActionIconShare().value_or(-1.0f), 0.5f);
	dialogue.AdvanceControlIcons(1.0f);
	EXPECT_FLOAT_EQ(dialogue.GetKeyIconShare(), 1.0f);
	// A new text starts them again; the bottom one waits while the "Continue" cue has its place
	dialogue.RunText(false, 10, 1);
	EXPECT_FLOAT_EQ(dialogue.GetKeyIconShare(), 0.0f);
	dialogue.AdvanceControlIcons(0.3f);
	EXPECT_FALSE(dialogue.GetActionIconShare().has_value());
	EXPECT_FLOAT_EQ(dialogue.GetKeyIconShare(), 1.0f);
	// The click ends the wait: the bottom icon fades in from nothing
	f.turn += 1000;
	dialogue.ProcessClick(true, false);
	EXPECT_FLOAT_EQ(dialogue.GetActionIconShare().value_or(-1.0f), 0.0f);
	dialogue.AdvanceControlIcons(0.1f);
	EXPECT_NEAR(dialogue.GetActionIconShare().value_or(-1.0f), 1.0f / 3.0f, 1e-6f);
	// Taking the texts away starts the key cap again
	dialogue.ClearAllText();
	EXPECT_FLOAT_EQ(dialogue.GetKeyIconShare(), 0.0f);
}

TEST(DialogueText, AClickTooSoonDoesNothing)
{
	Fixture f;
	f.texts[10] = {.text = u"click me"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 10, 1);
	// Waiting for a click it must be shown a second: nine turns are not enough
	f.turn += 9;
	dialogue.ProcessClick(true, false);
	EXPECT_TRUE(dialogue.IsWaitingForClick());
	f.turn += 1;
	dialogue.ProcessClick(true, false);
	EXPECT_FALSE(dialogue.IsWaitingForClick());
}

TEST(DialogueText, TheKeyNeedsNoWait)
{
	Fixture f;
	f.texts[10] = {.text = u"click me"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 10, 1);
	dialogue.ProcessClick(false, true);
	EXPECT_FALSE(dialogue.IsWaitingForClick());
	// The key cuts the text short
	EXPECT_EQ(f.interrupts, 1);
	EXPECT_EQ(f.narrationStops, 1);
	EXPECT_EQ(dialogue.GetEndTurn(), 0u);
}

TEST(DialogueText, NoClickTextsIgnoreClicks)
{
	Fixture f;
	f.texts[10] = {.text = u"one two three"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 10, 2);
	f.turn += 10;
	dialogue.ProcessClick(true, true);
	EXPECT_EQ(f.interrupts, 0);
	EXPECT_EQ(dialogue.GetEndTurn(), 123u);
}

TEST(DialogueText, AClickInACutSceneCutsTheTextShort)
{
	Fixture f;
	f.wideScreen = true;
	f.texts[10] = {.text = u"one two three"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 10, 0);
	// Half a second must pass first
	f.turn += 4;
	dialogue.ProcessClick(true, false);
	EXPECT_EQ(f.interrupts, 0);
	f.turn += 1;
	dialogue.ProcessClick(true, false);
	EXPECT_EQ(f.interrupts, 1);
	EXPECT_EQ(f.narrationStops, 1);
	EXPECT_TRUE(dialogue.IsTextRead());
	// The text stays on screen
	EXPECT_EQ(dialogue.GetCurrentText(), 10u);
}

TEST(DialogueText, AReadTextIgnoresClicks)
{
	Fixture f;
	f.wideScreen = true;
	f.texts[10] = {.text = u"one"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 10, 0);
	f.turn += 100;
	dialogue.ProcessClick(true, false);
	EXPECT_EQ(f.interrupts, 0);
}

TEST(DialogueText, AdvisorTextsAreSaidByTheAdvisor)
{
	Fixture f;
	f.texts[4431] = {.narrator = k_NarratorGoodAdvisor, .text = u"Greetings.", .voice = DialogueVoice {true}};
	f.texts[4432] = {.narrator = k_NarratorEvilAdvisor, .text = u"Hello.", .voice = DialogueVoice {true}};
	auto dialogue = f.Make();
	dialogue.RunText(false, 4431, 0);
	dialogue.RunText(false, 4432, 0);
	ASSERT_EQ(f.advisorSaid.size(), 2u);
	EXPECT_EQ(f.advisorSaid[0], std::make_pair(k_NarratorGoodAdvisor, 4431u));
	EXPECT_EQ(f.advisorSaid[1], std::make_pair(k_NarratorEvilAdvisor, 4432u));
	EXPECT_TRUE(f.narrated.empty());
	// Read once the advisors stop talking, whatever the time
	f.advisorsTalking = true;
	EXPECT_FALSE(dialogue.IsTextRead());
	f.advisorsTalking = false;
	EXPECT_TRUE(dialogue.IsTextRead());
}

TEST(DialogueText, OtherVoicesAreNarration)
{
	Fixture f;
	// A villager's line, and an advisor's line in another bank
	f.texts[5187] = {.narrator = 5, .text = u"Help!", .voice = DialogueVoice {false}};
	f.texts[20] = {.narrator = k_NarratorGoodAdvisor, .text = u"Hi.", .voice = DialogueVoice {false}};
	auto dialogue = f.Make();
	dialogue.RunText(true, 5187, 0);
	dialogue.RunText(true, 20, 0);
	EXPECT_EQ(f.narrated, (std::vector<uint32_t> {5187, 20}));
	EXPECT_TRUE(f.advisorSaid.empty());
}

TEST(DialogueText, NarrationIsReadAShortWhileAfterItStops)
{
	Fixture f;
	f.texts[5187] = {.narrator = 5, .text = u"Help!", .voice = DialogueVoice {false}};
	auto dialogue = f.Make();
	dialogue.RunText(true, 5187, 0);
	f.nowMs = 30000;
	f.narrationSounding = true;
	EXPECT_FALSE(dialogue.IsTextRead());
	EXPECT_EQ(dialogue.GetEndMs(), 30450);
	f.narrationSounding = false;
	f.nowMs = 30449;
	EXPECT_FALSE(dialogue.IsTextRead());
	f.nowMs = 30450;
	EXPECT_TRUE(dialogue.IsTextRead());
}

TEST(DialogueText, WithoutAVoiceTheReadingTimeCounts)
{
	Fixture f;
	f.texts[10] = {.narrator = k_NarratorGoodAdvisor, .text = u"one two three"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 10, 0);
	EXPECT_TRUE(f.advisorSaid.empty());
	EXPECT_TRUE(f.narrated.empty());
	f.advisorsTalking = true;
	f.turn = 124;
	EXPECT_TRUE(dialogue.IsTextRead());
}

TEST(DialogueText, ClearingTakesEveryTextAway)
{
	Fixture f;
	f.texts[10] = {.text = u"one"};
	auto dialogue = f.Make();
	dialogue.RunText(false, 10, 1);
	dialogue.ClearAllText();
	EXPECT_EQ(dialogue.GetCurrentText(), 0u);
	EXPECT_FALSE(dialogue.IsWaitingForClick());
	EXPECT_EQ(dialogue.GetEndTurn(), 0u);
	EXPECT_EQ(dialogue.GetEndMs(), 0);
	dialogue.RunText(false, 10, 0);
	dialogue.CloseDialogue();
	EXPECT_EQ(dialogue.GetCurrentText(), 0u);
	EXPECT_FALSE(dialogue.GetDisplay().IsBoxShown());
}

TEST(DialogueText, DrawGate)
{
	Fixture f;
	f.texts[10] = {.text = u"plain"};
	f.texts[11] = {.important = true, .text = u"important"};
	auto onlyImportant = f.Make({.textDraw = 1});
	// Nothing shown yet is drawn
	EXPECT_TRUE(onlyImportant.IsDrawn());
	onlyImportant.RunText(false, 10, 0);
	EXPECT_FALSE(onlyImportant.IsDrawn());
	onlyImportant.RunText(false, 11, 0);
	EXPECT_TRUE(onlyImportant.IsDrawn());

	auto never = f.Make({.textDraw = 0});
	never.RunText(false, 11, 0);
	EXPECT_FALSE(never.IsDrawn());

	auto always = f.Make({.textDraw = 2});
	always.RunText(false, 10, 0);
	EXPECT_TRUE(always.IsDrawn());
}

namespace
{
/// The words a frame draws, every character as wide as the font size
std::vector<std::u16string> DrawnWords(const DialogueText& dialogue)
{
	const WidthFn width = [](TextFont, std::u16string_view text, float size) { return static_cast<float>(text.size()) * size; };
	std::vector<std::u16string> words;
	for (const auto& run : dialogue.Layout(640, 480, 0, width).runs)
	{
		words.push_back(run.text);
	}
	return words;
}

bool Draws(const std::vector<std::u16string>& words, std::u16string_view word)
{
	return std::ranges::find(words, word) != words.end();
}
} // namespace

TEST(DialogueText, NumbersAreWrittenWithThreeDecimals)
{
	EXPECT_EQ(FormatTextNumber(45.5f, false), u"45.500");
	EXPECT_EQ(FormatTextNumber(45.5f, true), u"45.500%");
	EXPECT_EQ(FormatTextNumber(100.0f, true), u"100.000%");
	// Rounded to the nearest thousandth, the sign kept, no thousands separators
	EXPECT_EQ(FormatTextNumber(2.0004f, false), u"2.000");
	EXPECT_EQ(FormatTextNumber(2.0006f, false), u"2.001");
	EXPECT_EQ(FormatTextNumber(-3.25f, false), u"-3.250");
	EXPECT_EQ(FormatTextNumber(1234567.0f, false), u"1234567.000");
	EXPECT_EQ(FormatTextNumber(0.0f, false), u"0.000");
}

TEST(DialogueText, ATextCarriesItsNumber)
{
	Fixture f;
	f.texts[10] = {.text = u"Need $P now"};
	f.texts[11] = {.text = u"Count $D"};
	auto dialogue = f.Make({.textDraw = 2});
	EXPECT_TRUE(dialogue.RunText(false, 10, 0, 45.5f));
	EXPECT_TRUE(Draws(DrawnWords(dialogue), u"45.500%"));
	// Each text keeps its own number; a text shown without one writes 0
	dialogue.RunText(false, 11, 0);
	const auto words = DrawnWords(dialogue);
	EXPECT_TRUE(Draws(words, u"45.500%"));
	EXPECT_TRUE(Draws(words, u"0.000"));
	// The number is not spoken: the voice is the text's own
	EXPECT_EQ(dialogue.GetCurrentText(), 11u);
}

TEST(DialogueText, ScriptWordsGetAnAsterisk)
{
	EXPECT_EQ(ScriptWordsText("Already done"), u"*Already done");
	EXPECT_EQ(ScriptWordsText(""), u"*");
	// Characters are kept as they are: no line break conversion, bytes above 127 as Latin-1
	EXPECT_EQ(ScriptWordsText("a\nb"), u"*a\nb");
	EXPECT_EQ(ScriptWordsText("caf\xE9"), u"*caf\u00E9");
}

TEST(DialogueText, ScriptWordsAreShownWithoutAVoice)
{
	Fixture f;
	f.texts[0] = {.text = u"Invalid text string."};
	f.texts[10] = {.text = u"one two three"};
	auto dialogue = f.Make({.textDraw = 1});
	dialogue.RunText(false, 10, 0);
	// A single line clears the texts before it
	dialogue.TempText(true, "Already done", 0);
	EXPECT_TRUE(f.advisorSaid.empty());
	EXPECT_TRUE(f.narrated.empty());
	EXPECT_EQ(dialogue.GetCurrentText(), 0u);
	EXPECT_TRUE(dialogue.GetDisplay().IsSingleLine());
	// Shown whatever the player's setting, in the plain font
	EXPECT_TRUE(dialogue.IsDrawn());
	const auto words = DrawnWords(dialogue);
	EXPECT_TRUE(Draws(words, u"*Already"));
	EXPECT_TRUE(Draws(words, u"done"));
	EXPECT_FALSE(Draws(words, u"three"));
	// Two words: read after 5 turns a word and 8 more
	f.turn = 118;
	EXPECT_FALSE(dialogue.IsTextRead());
	f.turn = 119;
	EXPECT_TRUE(dialogue.IsTextRead());
}

TEST(DialogueText, ScriptWordsCanWaitForAClick)
{
	Fixture f;
	auto dialogue = f.Make();
	dialogue.TempText(true, "Sorry, the boss got the cheat removed", 1);
	EXPECT_TRUE(dialogue.IsWaitingForClick());
	EXPECT_FALSE(dialogue.IsTextRead());
}
