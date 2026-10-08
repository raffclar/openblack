/******************************************************************************
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

#include "Audio/Services/Voices.h"
#include "Bubble.h"
#include "Common/HelpText.h"
#include "HelpTextDisplay.h"
#include "InputPromptIcon.h"

// The text part of the help system: the current text and the five before it, the history, ClearAllText, IsTextRead,
// the click that ends a text, and the CHL functions RUN_TEXT, RUN_TEXT_WITH_NUMBER, TEMP_TEXT, TEMP_TEXT_WITH_NUMBER,
// TEXT_READ, GAME_CLEAR_DIALOGUE and GAME_CLOSE_DIALOGUE. The text display is HelpTextDisplay, drawn by
// Renderer::DrawHelpText; OPENBLACK_TEXT_TRACE logs each text. The voices plug in through the hooks and queries below
// (Game.cpp: audio::voices and audio::advisor); with Queries::voiceBankLoaded unset no text has a voice, so IsTextRead
// takes the reading-time branch, as the original does when the dialogue banks are not registered.
// See docs/bw1-notes/audio.md.

namespace openblack::help
{

/// The history of texts: 1024 entries
constexpr size_t k_HistorySize = 0x400;
/// What ProcessInterface returns when the click was used by the text
constexpr int k_ClickTaken = 0x14;

/// The number of words of a text: the non-empty pieces the text splitter returns. Words end at a blank (space, tab,
/// CR, LF, U+F8FE), at a '$' or '\' or after 47 characters; "$$" / "\\" (any two of them) is a literal character;
/// '$' or '\' and a code (a digit or one of CDFMNP in either case) with its digits are not words (after C one more
/// character is skipped); any other character after them is the start of a word.
[[nodiscard]] uint32_t CountWords(std::u16string_view text);

/// The factor of READ_SPEED r on the reading time, r <= 0.5 ? 3 - 4r : (1 - 2(r - 0.5)) * 0.8 + 0.2. NaN takes the
/// first branch. The game thread's FPU runs at 24 bits: every step rounds to a float, the double constants used whole
/// (docs/bw1-notes/audio.md)
[[nodiscard]] float ReadSpeedFactor(float readSpeed);

/// Where the voice of a text is sent
enum class VoiceRoute : uint8_t
{
	None,       ///< no bank or no sample: the text is only shown
	GoodSpirit, ///< HelpSprites and narrator 2: both advisors stop their sentence, the good advisor says the sample
	EvilSpirit, ///< HelpSprites and narrator 3: both advisors stop their sentence, the evil advisor says the sample
	Narration,  ///< any other: a 2D sound effect with the narration owner
};
[[nodiscard]] VoiceRoute RouteOf(int32_t narrator, audio::TextVoice voice);

/// The spirit who says a text, from the narrator of its entry (entry 0 when the id is out of range): 2 (the good
/// spirit) -> 1, 3 (the evil one) -> 2, any other -> 0
[[nodiscard]] int32_t SpiritWhoTalks(int32_t narrator);

/// A debug override of the spirits' texts, set from the debug GUI's Consciences window: a spirit silenced keeps its
/// texts but says none of them, a spirit forced says the other one's texts as well. None leaves every text as it is.
enum class SpeakerOverride : uint8_t
{
	None,
	SilenceGood,
	SilenceEvil,
	ForceGood,
	ForceEvil,
};

/// Who says a text under the override, and whether its voice is silenced
struct Speaker
{
	int32_t narrator;
	bool silenced;
};
/// The narrator of a text under the override: only the two spirits' texts change
[[nodiscard]] Speaker ApplySpeakerOverride(SpeakerOverride override, int32_t narrator);

/// SCRIPT_SPIRIT_TYPE (ScriptEnums.h SpiritType) -> the help spirit 1 (good) / 2 (evil): 2 (EVIL) -> 2;
/// 3 (ALIGNMENT) -> discrete alignment < 3 ? 2 : 1; 4 (ANTI_ALIGNMENT) -> >= 3 ? 2 : 1; 5 (RANDOM) -> a local random
/// number below 100 > 50 ? 2 : 1; any other (NONE, GOOD) -> 1. `discreteAlignment`: the local player's discrete
/// alignment; `rand100`: a local random number below 100 (asked only for RANDOM)
[[nodiscard]] int32_t ResolveScriptAdvisor(int32_t type, int discreteAlignment, const std::function<int()>& rand100);

/// One entry of the history
struct HistoryEntry
{
	uint32_t textId;
	int32_t withInteraction;
	float number; ///< the number of RUN_TEXT_WITH_NUMBER (0 for RUN_TEXT)
	int32_t narrator;
};

class HelpSystem
{
public:
	/// The help system's values from info.dat
	struct Info
	{
		uint32_t readDefaultAdjustGTTime; ///< 8 in info.dat
		uint32_t readDefaultWordGTTime;   ///< 5 in info.dat
	};

	/// What the help system reads from the rest of the game; unset gives the value written next to each
	struct Queries
	{
		/// The help text database entry. Unset: helptext::GetEntry.
		std::function<helptext::Entry(uint32_t textId)> textEntry;
		/// The voice table. Unset: audio::voices::Table().
		std::function<audio::TextVoice(uint32_t textId)> textVoice;
		/// The game turn. Unset: 0.
		std::function<uint32_t()> turn;
		/// The scaled clock in milliseconds. Unset: 0.
		std::function<int32_t()> nowMs;
		/// Inside the citadel the times are in milliseconds. Unset: false.
		std::function<bool()> citadelClock;
		/// The VMScriptType of a task (1 Script, 2 Help, ...; 1 when there is no such task). Unset: 1.
		std::function<uint32_t(uint32_t taskNumber)> taskScriptType;
		/// The key that skips a text is down (which key: not read). Unset: false.
		std::function<bool()> skipKey;
		/// The interface is playing back a recording. Unset: false.
		std::function<bool()> playBack;
		/// The bank is registered. Unset: false, so no text has a voice.
		std::function<bool(audio::SfxBank)> voiceBankLoaded;
		/// An advisor talks or stopped less than 200 ms ago. Unset: false.
		std::function<bool()> advisorsTalking;
		/// The sample is playing. Unset: false.
		std::function<bool(audio::SfxBank, audio::VoiceOwner, uint32_t sample)> isPlaying;
		/// The screen height the text display reads once (its line height is H / 30). Unset: 480 (inferred)
		std::function<int()> screenHeight;
	};

	/// Where the parts that are not ported plug in
	struct Hooks
	{
		/// Show the text
		std::function<void(const std::u16string& text, float number, int32_t narrator)> showText;
		/// openblack only (test hooks): a text with this id was made the current one
		std::function<void(uint32_t textId)> textStarted;
		/// The voice branches of SayText
		std::function<void(uint32_t textId, VoiceRoute route, audio::TextVoice voice)> sayVoice;
		/// A click that cuts a text stops the narration (audio::voices::CutByClick), after spiritStop on both spirits
		std::function<void()> stopVoicesOnClick;
		/// Send a spirit home: with arg != 0 the advisor leaves at once, with arg == 0 it flies off first (inferred).
		/// Game.cpp: help::spirits.
		std::function<void(int32_t spirit, int32_t arg)> spiritHome;
		/// Interrupt the spirit's advisor: audio::advisor::Interrupt
		std::function<void(int32_t spirit, int32_t arg)> spiritStop;
		/// The rest of SetWideScreen when the wide screen changes: the dialog boxes hidden (on), the interface made
		/// inactive while a script owns the wide screen, and the bars' timer: openblack's bars are ScreenFade::SetWideScreen
		std::function<void(bool on)> wideScreen;
	};

	HelpSystem(Info info, Queries queries, Hooks hooks);

	/// CHL 13 RUN_TEXT
	void RunText(bool singleLine, uint32_t textId, int32_t withInteraction);
	/// CHL 232 RUN_TEXT_WITH_NUMBER
	void RunTextWithNumber(bool singleLine, uint32_t textId, float number, int32_t withInteraction);
	/// CHL 14 TEMP_TEXT: u"*" + the string, narrator 1, text id 0, no voice, no history
	void TempText(bool singleLine, std::u16string_view text, int32_t withInteraction);
	/// CHL 231 TEMP_TEXT_WITH_NUMBER
	void TempTextWithNumber(bool singleLine, std::u16string_view text, float number, int32_t withInteraction);
	/// CHL 411 GAME_CLEAR_DIALOGUE: ClearAllText
	void ClearDialogue();
	/// CHL 412 GAME_CLOSE_DIALOGUE: closes the text display (display only; its Reset clears that again) and ClearAllText
	void CloseDialogue();

	/// Show a help text (AddText), keep it in the history and say its voice
	void SayText(uint32_t textId, int32_t withInteraction, float number);
	/// Show a text, start its reading time and make it the current one
	void AddText(const std::u16string& text, int32_t withInteraction, float number, int32_t narrator, uint32_t textId);
	/// Reset the text display (which also clears the single line flag), the six text ids and ClearTextDisplayed
	void ClearAllText();
	/// Delete the click icon (while waiting for a click) and the key icon, then clear the no-click and wait flags and
	/// the end times
	void ClearTextDisplayed();
	/// Once a turn from Process: the icons go when the display has no current text
	void ProcessIcons();
	/// Delete the click icon, the tooltips' icon and the key icon. (pending) its callers on load and when the advisors
	/// are shut down
	void ResetIcons();
	/// The text part of the help system's reset (from the script reset): ClearAllText, SetWideScreen(0, 0), ResetIcons
	/// and the history emptied. The rest is not ported. The 9 category turns and the message sets' counters are zeroed and
	/// the help is switched on. It does not touch the dialogue owner
	void Reset();
	/// The debug override of the spirits' texts; Reset puts it back to None
	void SetSpeakerOverride(SpeakerOverride override) { _speakerOverride = override; }
	[[nodiscard]] SpeakerOverride GetSpeakerOverride() const { return _speakerOverride; }
	/// CHL 15 TEXT_READ
	[[nodiscard]] bool IsTextRead() const;
	/// The text part of the help system's 3D draw, once a frame with the frame's game time in ms (the frame delta in the
	/// citadel): the slide-in of the newest text, then while a text waits for a click the "click to continue" icon:
	/// created once (help::input_prompt, a 1.0 s fade), its y moved every frame. `region` and `screenWidth` are this frame's
	void Draw3D(float frameMs, const TextRegion& region, int screenWidth);
	/// The text display
	[[nodiscard]] const HelpTextDisplay& GetDisplay() const { return _display; }
	/// The profile value TEXT_DRAW (1 when the profile has none; openblack has no profiles)
	[[nodiscard]] int GetTextDraw() const { return _textDraw; }
	/// The profile's TEXT_TOPTOBOTTOM (0 when missing)
	[[nodiscard]] bool GetTextTopToBottom() const { return _textTopToBottom; }

	/// Called by the interface on a click (inferred: the left button going down; openblack calls it on that event).
	/// 1, or k_ClickTaken.
	int ProcessInterface(bool click);

	/// A script task has the dialogue or holds the wide screen. IS_DIALOGUE_READY pushes its negation
	[[nodiscard]] bool IsDialogueControlled() const { return _dialogueOwner != 0 || IsScriptWideScreen(); }
	/// The script task that has the dialogue (0: none)
	[[nodiscard]] uint32_t GetDialogueOwner() const { return _dialogueOwner; }
	/// Give the dialogue to a task
	void SetCurrentControl(uint32_t task) { _dialogueOwner = task; }

	/// The same thing again closes the bubble; else it stores the text and the thing and restarts the bubble. The caller
	/// does the rest in the original's order: "FirstDYKExplained" when no did-you-know was read yet
	/// (script_control::StartScriptIfNoDialogue) and the text marked read (did_you_know_read::MarkRead)
	void SetBubbleProperties(uint32_t thing, uint32_t text);
	/// Once a turn from Process: the thing gone or off the screen closes the bubble; else the bubble's life is reset to
	/// 3.0
	void ProcessBubble(bool thingGoneOrOffScreen);
	/// The thing the bubble is about (an entity's integral; nullopt: no bubble)
	[[nodiscard]] std::optional<uint32_t> GetBubbleThing() const { return _bubbleThing; }
	/// The did-you-know text
	[[nodiscard]] uint32_t GetBubbleText() const { return _bubbleText; }
	[[nodiscard]] Bubble& GetBubble() { return _bubble; }
	/// Nothing (false) while IsDialogueControlled; else the task takes the dialogue and ClearAllText (true)
	bool DialogueControlRequest(uint32_t task);
	/// No task has the dialogue and the text display is closed (display only, not ported)
	void ClearDialogueControl();
	/// From END_DIALOGUE and when a task stops: when the task has the dialogue, give it back, take the wide screen away
	/// and send both advisors home
	void ReleaseDialogueControl(uint32_t task);
	/// When a task stops: when the wide screen is on and this task owns it, SetWideScreen(0, 0)
	void ReleaseWideScreenOf(uint32_t task);
	/// Hooks::spiritHome
	void SpiritHome(int32_t spirit, int32_t arg);
	/// When the wide screen changes, store it and its owner (0 when off), then Hooks::wideScreen
	void SetWideScreen(int32_t on, uint32_t owner);
	/// A script task holds the wide screen (also read by the sound effects and the alignment music)
	[[nodiscard]] bool IsScriptWideScreen() const { return _wideScreen != 0 && _wideScreenOwner != 0; }
	/// The wide screen is on
	[[nodiscard]] int32_t GetWideScreen() const { return _wideScreen; }
	/// The script task that set it (0: none, or set by the game)
	[[nodiscard]] uint32_t GetWideScreenOwner() const { return _wideScreenOwner; }

	/// The profile's READ_SPEED (0.5 without a profile)
	void SetReadSpeed(float readSpeed) { _readSpeed = readSpeed; }
	[[nodiscard]] float GetReadSpeed() const { return _readSpeed; }

	/// The help switch (Reset sets 1; SET_HELP_SYSTEM stores the popped value)
	void SetHelpOn(uint32_t on) { _helpOn = on; }
	[[nodiscard]] uint32_t GetHelpOn() const { return _helpOn; }
	/// The profile's HELP_LEVEL, 3 when the profile has none (openblack has no profiles)
	void SetHelpLevel(int32_t level) { _helpLevel = level; }
	[[nodiscard]] int32_t GetHelpLevel() const { return _helpLevel; }
	/// HELP_SYSTEM_ON: the help is on and the level is not 0
	[[nodiscard]] bool IsHelpSystemOn() const { return _helpOn != 0 && _helpLevel != 0; }
	/// The level the guidance reads: the help level while the help is on, else 0
	[[nodiscard]] int32_t GetGuidanceLevel() const { return _helpOn != 0 ? _helpLevel : 0; }
	/// Store the turn the category was triggered (Reset zeroes the 9)
	void TriggerCategory(int32_t category);
	[[nodiscard]] uint32_t GetCategoryTurn(int32_t category) const;
	/// The turn the last help message started (zeroed when the game is reset)
	void SetMessageTurn(uint32_t turn) { _messageTurn = turn; }
	/// The message sets' state (Help/HelpMessageSets.h)
	struct MessageSets
	{
		std::array<uint32_t, 18> conditions {};  ///< this turn's
		std::array<uint32_t, 0x98> sentTurn {};  ///< the turn each set was last sent
		std::array<uint32_t, 0x98> sentCount {}; ///< how often each set was sent
		uint32_t lastSet {0};
		uint16_t banterCount {0}; ///< banter sets sent since the last restart
	};
	[[nodiscard]] MessageSets& GetMessageSets() { return _messageSets; }
	[[nodiscard]] uint32_t GetMessageTurn() const { return _messageTurn; }

	/// The i-th most recent text of the history (0 = the last), nullptr past the ones kept
	[[nodiscard]] const HistoryEntry* GetHistory(int32_t i) const;
	[[nodiscard]] int32_t GetHistoryCount() const { return _historyCount; }

	/// The current text (0 after ClearAllText), then the five before it
	[[nodiscard]] const std::array<uint32_t, 6>& GetTexts() const { return _texts; }
	/// withInteraction == 1, the text waits for a click
	[[nodiscard]] bool IsWaitingForClick() const { return _waitClick; }
	/// The turn the text started and the turn after which it is read
	[[nodiscard]] uint32_t GetStartTurn() const { return _startTurn; }
	[[nodiscard]] uint32_t GetEndTurn() const { return _endTurn; }

private:
	/// The reading time of a text without voice
	void StartReadingTime(std::u16string_view text);
	/// The text has a voice to wait for (table entry, bank, sample and the bank registered)
	[[nodiscard]] bool HasVoice(uint32_t textId) const;
	/// The text has been shown long enough for a click to count
	[[nodiscard]] bool ShownLongEnough() const;
	void AddHistory(uint32_t textId, int32_t withInteraction, float number, int32_t narrator);

	[[nodiscard]] uint32_t Turn() const { return _queries.turn ? _queries.turn() : 0; }
	[[nodiscard]] int32_t NowMs() const { return _queries.nowMs ? _queries.nowMs() : 0; }
	[[nodiscard]] bool CitadelClock() const { return _queries.citadelClock && _queries.citadelClock(); }

	Info _info;
	Queries _queries;
	Hooks _hooks;
	SpeakerOverride _speakerOverride {SpeakerOverride::None};

	HelpTextDisplay _display;
	int _textDraw {1};
	bool _textTopToBottom {false};
	input_prompt::Handle _clickIcon {input_prompt::k_None};      ///< the click cue (or the $M<n> action icon)
	input_prompt::Handle _controlKeyIcon {input_prompt::k_None}; ///< the $M<6..15> key combination (pending)
	bool _waitClick {false};
	bool _noClick {false}; ///< withInteraction == 2, a click does nothing
	std::array<uint32_t, 6> _texts {};
	std::array<HistoryEntry, k_HistorySize> _history {};
	int32_t _historyNext {0};
	int32_t _historyCount {0};
	uint32_t _startTurn {0};
	uint32_t _endTurn {0};
	int32_t _startMs {0};
	mutable int32_t _endMs {0}; ///< IsTextRead moves it while the narration plays
	/// Set from an interface flag when the interface's active state is set, cleared by ProcessInterface. openblack
	/// never sets it, so it stays false (not ported)
	bool _clickPending {false};
	float _readSpeed {0.5f};
	uint32_t _helpOn {1};
	int32_t _helpLevel {3}; ///< 3 without a profile
	std::array<uint32_t, 9> _categoryTurns {};
	uint32_t _messageTurn {0};
	MessageSets _messageSets;
	uint32_t _dialogueOwner {0};
	Bubble _bubble;
	uint32_t _bubbleText {0};
	std::optional<uint32_t> _bubbleThing;
	int32_t _wideScreen {0};
	uint32_t _wideScreenOwner {0};
};

namespace script_control
{
struct Vm;
} // namespace script_control

/// The game's help system: nullptr before Start
[[nodiscard]] HelpSystem* Get();
/// Once a game turn, after the script: the hand state, the icons, both spirits (the evil one first), the tooltips'
/// turn and, with the help on, the conditions and the banter (Help/HelpMessageSets.h); `vm` and `turn` for the
/// banter's scripts
void Process(const script_control::Vm& vm, uint32_t turn);
void Start(HelpSystem::Info info, HelpSystem::Queries queries, HelpSystem::Hooks hooks);
void Shutdown();

} // namespace openblack::help
