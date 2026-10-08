/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <PackFile.h>
#include <gtest/gtest.h>

#include "Audio/Services/ScriptAudioState.h"
#include "Common/HelpText.h"
#include "Help/HelpSystem.h"
#include "Help/ScriptControl.h"
#include "InfoConstants.h"

// HelpSystem without voices: the text queue, the word splitter, the reading times and the script's dialogue and
// camera control. The installation tests need OPENBLACK_TEST_BW_ROOT; without it they skip.

using namespace openblack;
using namespace openblack::help;

namespace
{
std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

std::optional<std::filesystem::path> GameRoot()
{
	const char* root = std::getenv("OPENBLACK_TEST_BW_ROOT");
	if (root == nullptr || *root == '\0' || !std::filesystem::is_directory(root))
	{
		return std::nullopt;
	}
	return std::filesystem::path(root);
}

std::optional<std::filesystem::path> FindNoCase(const std::filesystem::path& root, std::string_view relative)
{
	auto current = root;
	for (const auto& part : std::filesystem::path(relative))
	{
		const auto wanted = Lower(part.string());
		std::optional<std::filesystem::path> found;
		std::error_code ec;
		for (const auto& entry : std::filesystem::directory_iterator(current, ec))
		{
			if (Lower(entry.path().filename().string()) == wanted)
			{
				found = entry.path();
				break;
			}
		}
		if (!found)
		{
			return std::nullopt;
		}
		current = *found;
	}
	return current;
}

/// A help system over a fixed text list and a clock the test moves
struct Fixture
{
	uint32_t turn {100};
	int32_t now {5000};
	bool bankLoaded {false};
	bool playing {false};
	bool advisors {false};
	std::vector<helptext::Entry> texts;
	std::vector<audio::TextVoice> voices;
	std::vector<std::pair<uint32_t, VoiceRoute>> said;
	int stops {0};
	std::unique_ptr<HelpSystem> help;

	Fixture()
	{
		// id 0: HELP_TEXT_NONE; 1: two words; 2: a spirit's text; 3: a villager's text
		texts = {{0, 0, "HELP_TEXT_NONE", u"Cadena de texto no v\u00E1lida"},
		         {1, 1, "HELP_TEXT_TWO", u"Dos palabras"},
		         {1, helptext::k_NarratorGoodSpirit, "HELP_TEXT_SPIRIT", u"Hola"},
		         {1, 5, "HELP_TEXT_MAN", u"Un hombre habla"}};
		voices = {{}, {}, {audio::SfxBank::HelpSprites, 7}, {audio::SfxBank::Villagers, 9}};
		HelpSystem::Queries queries;
		queries.textEntry = [this](uint32_t id) { return id < texts.size() ? texts[id] : texts[0]; };
		queries.textVoice = [this](uint32_t id) { return id < voices.size() ? voices[id] : audio::TextVoice {}; };
		queries.turn = [this]() { return turn; };
		queries.nowMs = [this]() { return now; };
		queries.voiceBankLoaded = [this](audio::SfxBank) { return bankLoaded; };
		queries.isPlaying = [this](audio::SfxBank, audio::VoiceOwner owner, uint32_t) {
			return playing && owner == audio::VoiceOwner::Narration;
		};
		queries.advisorsTalking = [this]() { return advisors; };
		HelpSystem::Hooks hooks;
		hooks.sayVoice = [this](uint32_t id, VoiceRoute route, audio::TextVoice) { said.emplace_back(id, route); };
		hooks.stopVoicesOnClick = [this]() { ++stops; };
		// HelpSystemInfo of info.dat: readDefaultAdjustGTTime 8, readDefaultWordGTTime 5
		help = std::make_unique<HelpSystem>(HelpSystem::Info {8, 5}, std::move(queries), std::move(hooks));
	}
};
} // namespace

TEST(HelpSystem, CountWords)
{
	// the words of the text splitter
	EXPECT_EQ(CountWords(u""), 0u);
	EXPECT_EQ(CountWords(u"   "), 0u);
	EXPECT_EQ(CountWords(u"Cadena de texto no v\u00E1lida"), 5u);
	EXPECT_EQ(CountWords(u"  dos\tpalabras \r\n"), 2u);
	EXPECT_EQ(CountWords(u"una\n\notra"), 2u);
	EXPECT_EQ(CountWords(u"a\xF8FE"
	                     u"b"),
	          2u);
	// codes are not words: $m2 (an M code), \n written as two characters (N), $1 (a number), $C12x (C and one more)
	EXPECT_EQ(CountWords(u"Haz clic en la Criatura para que podamos acariciarla. $m2"), 9u);
	EXPECT_EQ(CountWords(u"uno\\ndos"), 2u);
	EXPECT_EQ(CountWords(u"$1 uno"), 1u);
	EXPECT_EQ(CountWords(u"$C12xhola"), 1u);
	EXPECT_EQ(CountWords(u"$C"), 0u);
	// not a code: only the '$' or '\' goes, the rest is a word
	EXPECT_EQ(CountWords(u"Descubiertos: $I Retos"), 3u);
	EXPECT_EQ(CountWords(u"Nombre: $s"), 2u);
	EXPECT_EQ(CountWords(u"v\u00E1lido.n\\Int\u00E9ntalo con otro"), 4u);
	// "$$" is a literal '$' that starts a word; a '$' ends the word before it
	EXPECT_EQ(CountWords(u"a$$b"), 2u);
	EXPECT_EQ(CountWords(u"$$"), 1u);
	// a word stops after 47 characters and the rest is another one
	EXPECT_EQ(CountWords(std::u16string(47, u'x')), 1u);
	EXPECT_EQ(CountWords(std::u16string(48, u'x')), 2u);
}

TEST(HelpSystem, ReadSpeedFactor)
{
	EXPECT_EQ(ReadSpeedFactor(0.5f), 1.0f);
	EXPECT_EQ(ReadSpeedFactor(0.0f), 3.0f);
	EXPECT_EQ(ReadSpeedFactor(0.25f), 2.0f);
	EXPECT_EQ(ReadSpeedFactor(1.0f), 0.2f);
	EXPECT_EQ(ReadSpeedFactor(0.75f), 0.6f);
	// each step rounded to 24 bits (the game thread's FPU precision): 3 - 4 * 0.1f is 2.5999999f, not the
	// 2.59999999404 of double; (1 - 2 (0.7f - 0.5)) * 0.8 + 0.2 is 0.68000001f, not 0.68000001907
	EXPECT_EQ(ReadSpeedFactor(0.1f), 0x1.4ccccc0p+1f);
	EXPECT_EQ(ReadSpeedFactor(0.7f), 0x1.5c28f6p-1f);
	EXPECT_EQ(ReadSpeedFactor(0.9f), 0x1.70a3dap-2f);
	EXPECT_EQ(ReadSpeedFactor(0.55f), 0x1.d70a3cp-1f);
}

TEST(HelpSystem, ReadingTimeWithoutVoice)
{
	Fixture f;
	// 2 words with READ_SPEED 0.5: (2 * 5 + 8) turns * 1 = 18
	f.help->RunText(false, 1, 0);
	EXPECT_EQ(f.help->GetStartTurn(), 100u);
	EXPECT_EQ(f.help->GetEndTurn(), 118u);
	EXPECT_EQ(f.help->GetTexts()[0], 1u);
	f.turn = 118;
	EXPECT_FALSE(f.help->IsTextRead()); // read only once the end turn is below the turn
	f.turn = 119;
	EXPECT_TRUE(f.help->IsTextRead());

	// READ_SPEED 0: x3 = 54 turns; 1: x0.2, 18 * 100 * 0.001f * 0.2 = 0.36 s -> trunc(3.6) = 3 turns
	f.turn = 200;
	f.help->SetReadSpeed(0.0f);
	f.help->RunText(false, 1, 0);
	EXPECT_EQ(f.help->GetEndTurn(), 254u);
	f.help->SetReadSpeed(1.0f);
	f.help->RunText(false, 1, 0);
	EXPECT_EQ(f.help->GetEndTurn(), 203u);

	// 5 words: 33 turns
	f.help->SetReadSpeed(0.5f);
	f.help->RunText(false, 0, 0);
	EXPECT_EQ(f.help->GetEndTurn(), 233u);
	// out of range: text 0
	f.help->RunText(false, 7000, 0);
	EXPECT_EQ(f.help->GetTexts()[0], 0u);
}

TEST(HelpSystem, QueueSingleLineAndClear)
{
	Fixture f;
	f.help->RunText(false, 1, 0);
	f.help->RunText(false, 3, 0);
	EXPECT_EQ(f.help->GetTexts()[0], 3u);
	EXPECT_EQ(f.help->GetTexts()[1], 1u);
	// singleLine clears first; the next text clears again because the flag stays
	f.help->RunText(true, 2, 0);
	EXPECT_EQ(f.help->GetTexts()[0], 2u);
	EXPECT_EQ(f.help->GetTexts()[1], 0u);
	f.help->RunText(false, 1, 0);
	EXPECT_EQ(f.help->GetTexts()[0], 1u);
	EXPECT_EQ(f.help->GetTexts()[1], 0u);
	f.help->RunText(false, 3, 0);
	EXPECT_EQ(f.help->GetTexts()[1], 1u);
	// six slots: the oldest goes
	for (uint32_t i = 0; i < 6; ++i)
	{
		f.help->RunText(false, 1, 0);
	}
	EXPECT_EQ(std::count(f.help->GetTexts().begin(), f.help->GetTexts().end(), 1u), 6);

	// GAME_CLEAR_DIALOGUE: no current text, read at once
	f.help->ClearDialogue();
	EXPECT_EQ(f.help->GetTexts()[0], 0u);
	EXPECT_EQ(f.help->GetEndTurn(), 0u);
	EXPECT_TRUE(f.help->IsTextRead());
	f.help->RunText(false, 1, 1);
	f.help->CloseDialogue();
	EXPECT_FALSE(f.help->IsWaitingForClick());
	EXPECT_TRUE(f.help->IsTextRead());
}

TEST(HelpSystem, TempText)
{
	std::u16string shown;
	HelpSystem::Hooks hooks;
	hooks.showText = [&shown](const std::u16string& text, float, int32_t narrator) {
		shown = text;
		EXPECT_EQ(narrator, 1);
	};
	// the display's TEXT_DRAW gate reads the current text's entry: an empty database here
	HelpSystem::Queries queries;
	queries.textEntry = [](uint32_t) { return helptext::Entry {}; };
	HelpSystem help({8, 5}, std::move(queries), std::move(hooks));
	help.TempText(false, u"dev text", 0);
	EXPECT_EQ(shown, u"*dev text");
	EXPECT_EQ(help.GetTexts()[0], 0u);
	EXPECT_EQ(help.GetHistoryCount(), 0); // not added to the history
	EXPECT_EQ(help.GetEndTurn(), 18u);    // "*dev" "text"
}

TEST(HelpSystem, WithInteraction)
{
	Fixture f;
	f.help->RunText(false, 1, 1);
	EXPECT_TRUE(f.help->IsWaitingForClick());
	f.turn = 500;
	EXPECT_FALSE(f.help->IsTextRead());
	EXPECT_EQ(f.help->ProcessInterface(false), 1);
	EXPECT_TRUE(f.help->IsWaitingForClick());
	// a click counts after 1 s while it waits: 10 turns of 100 ms
	f.turn = 100 + 9;
	f.help->RunText(false, 1, 1);
	f.help->ProcessInterface(true);
	EXPECT_TRUE(f.help->IsWaitingForClick());
	f.turn += 9;
	f.help->ProcessInterface(true);
	EXPECT_TRUE(f.help->IsWaitingForClick());
	f.turn += 1;
	EXPECT_EQ(f.help->ProcessInterface(true), 1);
	EXPECT_FALSE(f.help->IsWaitingForClick());
	EXPECT_FALSE(f.help->IsTextRead()); // its reading time still runs
	f.turn += 9;
	EXPECT_TRUE(f.help->IsTextRead());

	// withInteraction 2: a click does nothing
	f.help->RunText(false, 0, 2);
	f.turn += 20;
	EXPECT_EQ(f.help->ProcessInterface(true), 1);
	EXPECT_FALSE(f.help->IsTextRead());

	// with the script's wide screen a click after 0.5 s cuts the text
	f.help->SetWideScreen(1, 7); // a script task (7) holds it
	f.help->RunText(false, 0, 0);
	f.turn += 4;
	EXPECT_EQ(f.help->ProcessInterface(true), 1);
	f.turn += 1;
	EXPECT_EQ(f.help->ProcessInterface(true), k_ClickTaken);
	EXPECT_EQ(f.stops, 0); // text 0: no voice to stop
	EXPECT_TRUE(f.help->IsTextRead());
	f.help->RunText(false, 3, 0);
	f.turn += 5;
	EXPECT_EQ(f.help->ProcessInterface(true), k_ClickTaken);
	EXPECT_EQ(f.stops, 1);
}

TEST(HelpSystem, VoiceHooks)
{
	Fixture f;
	f.help->RunText(false, 1, 0);
	f.help->RunText(false, 2, 0);
	f.help->RunText(false, 3, 0);
	ASSERT_EQ(f.said.size(), 2u);
	EXPECT_EQ(f.said[0], std::make_pair(2u, VoiceRoute::GoodSpirit));
	EXPECT_EQ(f.said[1], std::make_pair(3u, VoiceRoute::Narration));
	EXPECT_EQ(RouteOf(3, {audio::SfxBank::HelpSprites, 0}), VoiceRoute::EvilSpirit);
	EXPECT_EQ(RouteOf(5, {audio::SfxBank::HelpSprites, 4}), VoiceRoute::Narration);
	EXPECT_EQ(RouteOf(2, {audio::SfxBank::Villagers, 4}), VoiceRoute::Narration);
	EXPECT_EQ(RouteOf(2, {}), VoiceRoute::None);

	// history: the last first
	EXPECT_EQ(f.help->GetHistoryCount(), 3);
	EXPECT_EQ(f.help->GetHistory(0)->textId, 3u);
	EXPECT_EQ(f.help->GetHistory(0)->narrator, 5);
	EXPECT_EQ(f.help->GetHistory(2)->textId, 1u);
	EXPECT_EQ(f.help->GetHistory(3), nullptr);
	for (int i = 0; i < 1100; ++i)
	{
		f.help->RunText(false, 1, 0);
	}
	EXPECT_EQ(f.help->GetHistoryCount(), 1024);
	EXPECT_EQ(f.help->GetHistory(1023)->textId, 1u);
}

TEST(HelpSystem, Reset)
{
	// Reset (also run by the script reset): ClearAllText and the history emptied
	Fixture f;
	f.help->RunText(true, 1, 1);
	f.help->RunText(false, 3, 0);
	ASSERT_EQ(f.help->GetHistoryCount(), 2);
	f.help->Reset();
	EXPECT_EQ(f.help->GetHistoryCount(), 0);
	EXPECT_EQ(f.help->GetHistory(0), nullptr);
	EXPECT_EQ(f.help->GetTexts()[0], 0u);
	EXPECT_EQ(f.help->GetTexts()[1], 0u);
	EXPECT_FALSE(f.help->IsWaitingForClick());
	EXPECT_TRUE(f.help->IsTextRead());
	// the single line flag went with the help text reset: the next text does not clear the one before
	f.help->RunText(false, 1, 0);
	f.help->RunText(false, 3, 0);
	EXPECT_EQ(f.help->GetTexts()[1], 1u);
	f.help->RunText(false, 2, 0);
	EXPECT_EQ(f.help->GetHistory(0)->textId, 2u);
	EXPECT_EQ(f.help->GetHistoryCount(), 3);
}

TEST(HelpSystem, ReadWithVoice)
{
	Fixture f;
	f.bankLoaded = true;
	// the narration: +450 ms after it stops
	f.help->RunText(false, 3, 0);
	f.playing = true;
	EXPECT_FALSE(f.help->IsTextRead());
	f.playing = false;
	f.now += 449;
	EXPECT_FALSE(f.help->IsTextRead());
	f.now += 1;
	EXPECT_TRUE(f.help->IsTextRead());
	// the advisors (HelpSprites): read when neither talks
	f.help->RunText(false, 2, 0);
	f.advisors = true;
	EXPECT_FALSE(f.help->IsTextRead());
	f.advisors = false;
	EXPECT_TRUE(f.help->IsTextRead());
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(HelpSystem, InstalledGame)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set";
	}
	// HelpSystemInfo in info.dat: readDefaultAdjustGTTime 8, readDefaultWordGTTime 5
	const auto info = FindNoCase(*root, "Scripts/info.dat");
	ASSERT_TRUE(info);
	std::ifstream stream(*info, std::ios::binary);
	pack::PackFile file;
	ASSERT_EQ(file.ReadFile(stream), pack::PackResult::Success);
	const auto& block = file.GetBlock("Info");
	ASSERT_EQ(block.size(), sizeof(InfoConstants));
	auto constants = std::make_unique<InfoConstants>();
	std::memcpy(constants.get(), block.data(), sizeof(InfoConstants));
	EXPECT_EQ(constants->helpSystem.readDefaultAdjustGTTime, 8u);
	EXPECT_EQ(constants->helpSystem.readDefaultWordGTTime, 5u);
	EXPECT_EQ(constants->helpSystem.wideScreenTime, 2.0f);

	// the words of every text of InfoScript2.txt, 52190 (checked against a separate port of the same splitter)
	const auto script = FindNoCase(*root, "Scripts/InfoScript2.txt");
	ASSERT_TRUE(script);
	std::ifstream text(*script, std::ios::binary);
	const std::vector<uint8_t> bytes {std::istreambuf_iterator<char>(text), std::istreambuf_iterator<char>()};
	const auto entries = helptext::Parse(bytes);
	ASSERT_EQ(entries.size(), helptext::k_TextCount);
	uint64_t words = 0;
	for (const auto& entry : entries)
	{
		words += CountWords(entry.text);
	}
	EXPECT_EQ(words, 52190u);
	EXPECT_EQ(CountWords(entries[1715].text), 11u); // a real line feed ("\n" in the file)
	EXPECT_EQ(CountWords(entries[1153].text), 3u);  // "Descubiertos: $I Retos"
}

namespace
{
/// A script VM: `current` runs now; types[task] is its VMScriptType (1 when it does not exist)
struct VmFixture
{
	uint32_t current {0};
	std::vector<uint32_t> types;
	std::vector<uint32_t> stoppedMasks;
	std::function<void(uint32_t)> onStop;

	script_control::Vm Make()
	{
		script_control::Vm vm;
		vm.taskNumber = [this]() { return current; };
		vm.currentTaskType = [this]() { return TypeOf(current); };
		vm.taskType = [this](uint32_t task) { return TypeOf(task); };
		vm.stopTasksOfType = [this](uint32_t mask) {
			stoppedMasks.push_back(mask);
			for (uint32_t task = 1; task < types.size(); ++task)
			{
				if ((types[task] & mask) != 0 && onStop)
				{
					onStop(task);
				}
			}
		};
		return vm;
	}
	[[nodiscard]] uint32_t TypeOf(uint32_t task) const { return task != 0 && task < types.size() ? types[task] : 1; }
};
} // namespace

TEST(ScriptControl, IsDialogueControlled)
{
	// IsDialogueControlled: a dialogue owner, or a script task's wide screen
	Fixture f;
	EXPECT_FALSE(f.help->IsDialogueControlled());
	EXPECT_TRUE(script_control::IsSpiritReady(*f.help)); // IS_DIALOGUE_READY: the negation
	f.help->SetWideScreen(1, 0);                         // the game's wide screen (no owner)
	EXPECT_FALSE(f.help->IsDialogueControlled());
	f.help->SetWideScreen(0, 0);
	f.help->SetWideScreen(1, 4); // a task's
	EXPECT_TRUE(f.help->IsDialogueControlled());
	EXPECT_FALSE(script_control::IsSpiritReady(*f.help));
	EXPECT_EQ(f.help->GetWideScreenOwner(), 4u);
	f.help->SetWideScreen(1, 9); // the wide screen unchanged: nothing
	EXPECT_EQ(f.help->GetWideScreenOwner(), 4u);
	f.help->SetWideScreen(0, 9); // off: the owner goes
	EXPECT_EQ(f.help->GetWideScreenOwner(), 0u);
	f.help->SetCurrentControl(3);
	EXPECT_TRUE(f.help->IsDialogueControlled());
	f.help->ClearDialogueControl();
	EXPECT_FALSE(f.help->IsDialogueControlled());
	// Reset takes the wide screen away, not the dialogue owner
	f.help->SetWideScreen(1, 4);
	f.help->SetCurrentControl(3);
	f.help->Reset();
	EXPECT_EQ(f.help->GetWideScreen(), 0);
	EXPECT_EQ(f.help->GetDialogueOwner(), 3u);
}

TEST(ScriptControl, StartEndDialogue)
{
	Fixture f;
	audio::ScriptAudioState audio;
	VmFixture vm;
	vm.types = {0, 1, 1, 2}; // tasks 1 and 2: Script; 3: Help

	// START_DIALOGUE: nobody has it -> this task (DialogueControlRequest), true
	vm.current = 1;
	EXPECT_TRUE(script_control::StartDialogue(*f.help, vm.Make()));
	EXPECT_EQ(f.help->GetDialogueOwner(), 1u);
	EXPECT_FALSE(script_control::IsSpiritReady(*f.help));
	// again from the same task: true
	EXPECT_TRUE(script_control::StartDialogue(*f.help, vm.Make()));
	// another Script task: false, without stopping anything
	vm.current = 2;
	EXPECT_FALSE(script_control::StartDialogue(*f.help, vm.Make()));
	EXPECT_TRUE(vm.stoppedMasks.empty());
	// END_DIALOGUE of a task without it: nothing
	audio.musicBeat = 5;
	audio.creatureSound = 0;
	script_control::EndDialogue(*f.help, audio, vm.Make());
	EXPECT_EQ(f.help->GetDialogueOwner(), 1u);
	EXPECT_EQ(audio.musicBeat.load(), 5);
	// of the owner: released, the music beat cleared and the creature sound back on
	vm.current = 1;
	f.help->SetWideScreen(1, 1);
	script_control::EndDialogue(*f.help, audio, vm.Make());
	EXPECT_EQ(f.help->GetDialogueOwner(), 0u);
	EXPECT_EQ(f.help->GetWideScreen(), 0);
	EXPECT_EQ(audio.musicBeat.load(), 0);
	EXPECT_EQ(audio.creatureSound.load(), 1);
	EXPECT_TRUE(script_control::IsSpiritReady(*f.help));

	// a Help task has it; a Script task asks: StopHelpScripts (0x4A) and its stop callback gives it back
	vm.current = 3;
	EXPECT_TRUE(script_control::StartDialogue(*f.help, vm.Make()));
	EXPECT_EQ(f.help->GetDialogueOwner(), 3u);
	script_control::CameraControl camera;
	vm.onStop = [&](uint32_t task) { script_control::OnTaskStopped(task, f.help.get(), camera, audio); };
	vm.current = 2;
	EXPECT_TRUE(script_control::StartDialogue(*f.help, vm.Make()));
	ASSERT_EQ(vm.stoppedMasks.size(), 1u);
	EXPECT_EQ(vm.stoppedMasks[0], script_control::k_HelpScriptTypes);
	EXPECT_EQ(f.help->GetDialogueOwner(), 2u);
	// a Help task does not take it from a Script one
	vm.current = 3;
	EXPECT_FALSE(script_control::StartDialogue(*f.help, vm.Make()));
	EXPECT_EQ(vm.stoppedMasks.size(), 1u);
	// the wide screen of another task refuses the request, but START_DIALOGUE still pushes true
	f.help->ClearDialogueControl();
	f.help->SetWideScreen(1, 7);
	vm.current = 1;
	EXPECT_TRUE(script_control::StartDialogue(*f.help, vm.Make()));
	EXPECT_EQ(f.help->GetDialogueOwner(), 0u);
}

TEST(ScriptControl, SetWideScreen)
{
	// SetWideScreen: only the owner, or anyone while nobody holds it
	Fixture f;
	VmFixture vm;
	vm.types = {0, 1, 1};
	vm.current = 1;
	EXPECT_TRUE(script_control::SetWideScreen(*f.help, 1, vm.Make()));
	EXPECT_EQ(f.help->GetWideScreenOwner(), 1u);
	EXPECT_TRUE(f.help->IsScriptWideScreen());
	vm.current = 2;
	EXPECT_FALSE(script_control::SetWideScreen(*f.help, 0, vm.Make()));
	EXPECT_EQ(f.help->GetWideScreen(), 1);
	vm.current = 1;
	EXPECT_TRUE(script_control::SetWideScreen(*f.help, 1, vm.Make())); // its own: the warning, no change
	EXPECT_TRUE(script_control::SetWideScreen(*f.help, 0, vm.Make()));
	EXPECT_EQ(f.help->GetWideScreen(), 0);
	EXPECT_FALSE(f.help->IsScriptWideScreen());
}

TEST(ScriptControl, CameraControl)
{
	audio::ScriptAudioState audio;
	VmFixture vm;
	vm.types = {0, 1, 0x10, 0x8};
	script_control::CameraControl camera;
	camera.Reset();
	vm.current = 1;
	// START_CAMERA_CONTROL outside the citadel: the script camera mode taken -> owner, highlights and leashes off
	EXPECT_TRUE(script_control::StartCameraControl(camera, vm.Make(), false, true));
	EXPECT_EQ(camera.owner, 1u);
	EXPECT_EQ(camera.drawHighlight, 0);
	EXPECT_EQ(camera.drawLeash, 0);
	// not taken (the camera cannot leave its mode): false, nothing written
	script_control::CameraControl other;
	EXPECT_FALSE(script_control::StartCameraControl(other, vm.Make(), false, false));
	EXPECT_EQ(other.owner, 0u);
	// inside the citadel only TempleHelp / TempleSpecial (0x18), without touching the switches
	EXPECT_FALSE(script_control::StartCameraControl(other, vm.Make(), true, true));
	vm.current = 2;
	EXPECT_TRUE(script_control::StartCameraControl(other, vm.Make(), true, false));
	EXPECT_EQ(other.owner, 2u);
	EXPECT_EQ(other.drawLeash, 1);
	// END_CAMERA_CONTROL: only the owner
	EXPECT_FALSE(script_control::EndCameraControl(camera, audio, vm.Make()));
	EXPECT_EQ(camera.owner, 1u);
	vm.current = 1;
	audio.creatureSound = 0;
	camera.field7C = 5;
	EXPECT_TRUE(script_control::EndCameraControl(camera, audio, vm.Make()));
	EXPECT_EQ(camera.owner, 0u);
	EXPECT_EQ(camera.drawHighlight, 1);
	EXPECT_EQ(camera.drawLeash, 1);
	EXPECT_EQ(camera.field7C, 0);
	EXPECT_EQ(audio.creatureSound.load(), 1);
	// the task-stop callback gives it back too
	EXPECT_TRUE(script_control::StartCameraControl(camera, vm.Make(), false, true));
	EXPECT_FALSE(script_control::ReleaseCameraOf(camera, audio, 2));
	EXPECT_TRUE(script_control::ReleaseCameraOf(camera, audio, 1));
	EXPECT_EQ(camera.owner, 0u);
	EXPECT_EQ(script_control::k_ScriptEndFov, 1.2217305f);
}

TEST(HelpSystem, SpeakerOverrideChangesOnlyTheSpiritsTexts)
{
	constexpr auto k_Good = helptext::k_NarratorGoodSpirit;
	constexpr auto k_Evil = helptext::k_NarratorEvilSpirit;
	constexpr int32_t k_Villager = 5;
	for (const auto narrator : {k_Good, k_Evil, k_Villager})
	{
		const auto none = ApplySpeakerOverride(SpeakerOverride::None, narrator);
		EXPECT_EQ(none.narrator, narrator);
		EXPECT_FALSE(none.silenced);
	}
	EXPECT_TRUE(ApplySpeakerOverride(SpeakerOverride::SilenceGood, k_Good).silenced);
	EXPECT_FALSE(ApplySpeakerOverride(SpeakerOverride::SilenceGood, k_Evil).silenced);
	EXPECT_TRUE(ApplySpeakerOverride(SpeakerOverride::SilenceEvil, k_Evil).silenced);
	EXPECT_FALSE(ApplySpeakerOverride(SpeakerOverride::SilenceEvil, k_Villager).silenced);
	EXPECT_EQ(ApplySpeakerOverride(SpeakerOverride::ForceGood, k_Evil).narrator, k_Good);
	EXPECT_EQ(ApplySpeakerOverride(SpeakerOverride::ForceGood, k_Villager).narrator, k_Villager);
	EXPECT_EQ(ApplySpeakerOverride(SpeakerOverride::ForceEvil, k_Good).narrator, k_Evil);
	EXPECT_EQ(ApplySpeakerOverride(SpeakerOverride::ForceEvil, k_Evil).narrator, k_Evil);
}

TEST(HelpSystem, SpeakerOverrideSilencesOrSwapsTheSpiritsVoice)
{
	Fixture f;
	f.help->SetSpeakerOverride(SpeakerOverride::SilenceGood);
	f.help->RunText(false, 2, 0);
	f.help->RunText(false, 3, 0);
	// The good spirit's text is shown without its voice; the villager's is said as before
	ASSERT_EQ(f.said.size(), 1u);
	EXPECT_EQ(f.said[0], std::make_pair(3u, VoiceRoute::Narration));
	EXPECT_EQ(f.help->GetHistory(1)->textId, 2u);

	f.help->SetSpeakerOverride(SpeakerOverride::ForceEvil);
	f.help->RunText(false, 2, 0);
	ASSERT_EQ(f.said.size(), 2u);
	EXPECT_EQ(f.said[1], std::make_pair(2u, VoiceRoute::EvilSpirit));
	EXPECT_EQ(f.help->GetHistory(0)->narrator, helptext::k_NarratorEvilSpirit);
}

TEST(HelpSystem, ResetClearsTheSpeakerOverride)
{
	Fixture f;
	f.help->SetSpeakerOverride(SpeakerOverride::ForceGood);
	f.help->Reset();
	EXPECT_EQ(f.help->GetSpeakerOverride(), SpeakerOverride::None);
}

TEST(HelpSystem, BubbleOpensWithEachTappedSignsTextAndTheSameSignClosesIt)
{
	Fixture f;
	auto& help = *f.help;
	constexpr uint32_t k_SignA = 7;
	constexpr uint32_t k_SignB = 9;
	constexpr uint32_t k_TextA = 0x101;
	constexpr uint32_t k_TextB = 0x202;
	EXPECT_FALSE(help.GetBubbleThing().has_value());

	// the first tap opens it with the sign's text, the scroll on the last line; the turn keeps it alive
	help.SetBubbleProperties(k_SignA, k_TextA);
	ASSERT_EQ(help.GetBubbleThing().value_or(0u), k_SignA);
	EXPECT_EQ(help.GetBubbleText(), k_TextA);
	EXPECT_FLOAT_EQ(help.GetBubble().Scroll(), 1000.0f);
	help.ProcessBubble(false);
	EXPECT_FLOAT_EQ(help.GetBubble().Life(), 3.0f);

	// a second tap on the same sign closes it, and the turn leaves it closed
	help.SetBubbleProperties(k_SignA, k_TextA);
	EXPECT_FALSE(help.GetBubbleThing().has_value());
	help.ProcessBubble(false);
	EXPECT_FALSE(help.GetBubbleThing().has_value());

	// a third opens it again, restarted
	help.GetBubble().Drag(true, 100, 19.0f);
	help.GetBubble().Drag(true, 81, 19.0f);
	help.SetBubbleProperties(k_SignA, k_TextA);
	ASSERT_EQ(help.GetBubbleThing().value_or(0u), k_SignA);
	EXPECT_EQ(help.GetBubbleText(), k_TextA);
	EXPECT_FLOAT_EQ(help.GetBubble().Scroll(), 1000.0f);

	// another sign takes the open bubble over with its own text
	help.SetBubbleProperties(k_SignB, k_TextB);
	ASSERT_EQ(help.GetBubbleThing().value_or(0u), k_SignB);
	EXPECT_EQ(help.GetBubbleText(), k_TextB);

	// its sign off the screen closes it; a tap on that sign then opens it rather than closing it
	help.ProcessBubble(true);
	EXPECT_FALSE(help.GetBubbleThing().has_value());
	help.SetBubbleProperties(k_SignB, k_TextB);
	ASSERT_EQ(help.GetBubbleThing().value_or(0u), k_SignB);
	EXPECT_EQ(help.GetBubbleText(), k_TextB);
}
