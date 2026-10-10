/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "HelpTextDisplay.h"

namespace openblack::help
{

/// How many help texts the scripts can refer to by number
constexpr uint32_t k_HelpTextCount = 6974;
/// The narrators the help texts name that are the advisors
constexpr int32_t k_NarratorGoodAdvisor = 2;
constexpr int32_t k_NarratorEvilAdvisor = 3;

/// The factor the player's reading speed (0 to 1, 0.5 by default) puts on a text's reading time: 3 - 4r up to 0.5,
/// then falling from 1 to 0.2. Not a number counts as slow.
[[nodiscard]] float ReadSpeedFactor(float readSpeed);

/// A help text's spoken line
struct DialogueVoice
{
	/// In the advisors' sound bank. Their lines are said by the advisor the text's narrator is; any other line is heard
	/// everywhere as narration.
	bool advisorsBank {false};
};

/// The dialogue the scripts show at the bottom of the screen, a line at a time: which texts are shown, how long each
/// takes to read or whether it waits for a click, the voice saying it, and the player clicking it on.
///
/// Up to six texts are kept, the newest at the bottom; a text shown alone ("single line") clears the ones before it. A
/// text is read once its voice has finished, or without a voice once its reading time has passed: 5 game turns a word
/// and 8 more, longer or shorter with the player's reading speed. A text that waits for a click is not read until the
/// player clicks. While a script holds the cinema bars, a click (or the keypad's Enter) cuts the text short and stops
/// its voice.
class DialogueText
{
public:
	/// A help text as the scripts number them
	struct Text
	{
		int32_t narrator {0};
		/// Shown even when the player has only the important texts shown
		bool important {false};
		std::u16string_view text;
	};

	/// What the dialogue reads from the rest of the game; unset reads as the value next to each
	struct Queries
	{
		/// A help text by number. Unset: an empty text
		std::function<Text(uint32_t number)> text;
		/// The text's spoken line, nothing when it has none or its sound bank is missing. Unset: none
		std::function<std::optional<DialogueVoice>(uint32_t number)> voice;
		/// The game turn. Unset: 0
		std::function<uint32_t()> turn;
		/// The game clock in milliseconds, which runs at the game's speed. Unset: 0
		std::function<int32_t()> nowMs;
		/// Inside the temple times are in milliseconds rather than turns. Unset: false
		std::function<bool()> inTemple;
		/// Either advisor is saying a line it was given, or stopped less than 200 ms ago. Unset: false
		std::function<bool()> advisorsTalking;
		/// The narration of a text is still sounding. Unset: false
		std::function<bool(uint32_t number)> narrationSounding;
		/// A script holds the cinema bars. Unset: false
		std::function<bool()> scriptWideScreen;
	};

	/// What the dialogue has the rest of the game do
	struct Hooks
	{
		/// Both advisors stop what they say, then the advisor of the narrator (2 good, 3 evil) says the text's line
		std::function<void(int32_t narrator, uint32_t number)> advisorSays;
		/// The text's line is heard as narration
		std::function<void(uint32_t number)> narrate;
		/// Both advisors are cut short
		std::function<void()> interruptAdvisors;
		/// The story's narration in the villagers' voices stops
		std::function<void()> stopNarration;
	};

	/// The player's settings for the texts
	struct Settings
	{
		/// 0 shows no texts, 1 only the important ones, anything else all of them
		int textDraw {1};
		/// The newest text at the top of the box rather than the bottom
		bool topToBottom {false};
		/// From 0 (slow) to 1 (fast)
		float readSpeed {0.5f};
		/// The reading time: so many game turns a word, and so many more
		uint32_t turnsPerWord {5};
		uint32_t extraTurns {8};
		/// The length of a game turn
		uint32_t msPerTurn {100};
	};

	DialogueText(int screenHeight, bool biggerText, Settings settings, Queries queries, Hooks hooks);

	/// A script shows a help text: with interaction 1 it waits for a click, with 2 a click does nothing. A single line
	/// clears the texts before it, and so does any text after a single line. A number past the last text shows the
	/// first, and false is returned for the script's error.
	bool RunText(bool singleLine, uint32_t number, int32_t withInteraction);
	/// The text the scripts last showed has been read
	[[nodiscard]] bool IsTextRead() const;
	/// Every text is taken away
	void ClearAllText();
	/// The texts are taken away and the box closed
	void CloseDialogue();
	/// The player clicked (or holds the keypad's Enter): it may end the wait for a click or cut the text short
	void ProcessClick(bool click, bool skipKey);

	/// Once a frame: the newest text slides in by the frame's game milliseconds (inside the temple its real ones)
	void Update(float frameMs);

	/// Whether the texts may be drawn: by the player's setting, and the important flag of the text last shown
	[[nodiscard]] bool IsDrawn() const;
	/// The frame's box and words for a screen with cinema bars of `barPixels`
	[[nodiscard]] const TextFrame& Layout(int width, int height, int barPixels, const WidthFn& widthFn) const;

	/// The text last shown, 0 once cleared
	[[nodiscard]] uint32_t GetCurrentText() const { return _texts[0]; }
	[[nodiscard]] bool IsWaitingForClick() const { return _waitClick; }
	[[nodiscard]] uint32_t GetEndTurn() const { return _endTurn; }
	[[nodiscard]] int32_t GetEndMs() const { return _endMs; }
	[[nodiscard]] const HelpTextDisplay& GetDisplay() const { return _display; }
	[[nodiscard]] const Settings& GetSettings() const { return _settings; }

private:
	void AddText(const Text& text, uint32_t number, int32_t withInteraction);
	void StartReadingTime(std::u16string_view text);
	/// Waits are over: the click wait and the reading time
	void ClearTextDisplayed();
	[[nodiscard]] bool ShownLongEnough() const;

	[[nodiscard]] uint32_t Turn() const { return _queries.turn ? _queries.turn() : 0; }
	[[nodiscard]] int32_t NowMs() const { return _queries.nowMs ? _queries.nowMs() : 0; }
	[[nodiscard]] bool InTemple() const { return _queries.inTemple && _queries.inTemple(); }

	Settings _settings;
	Queries _queries;
	Hooks _hooks;
	HelpTextDisplay _display;
	/// The text last shown, then the five before it
	std::array<uint32_t, 6> _texts {};
	bool _waitClick {false};
	bool _noClick {false};
	uint32_t _startTurn {0};
	uint32_t _endTurn {0};
	int32_t _startMs {0};
	/// Moved on while the narration is still sounding
	mutable int32_t _endMs {0};
};

} // namespace openblack::help
