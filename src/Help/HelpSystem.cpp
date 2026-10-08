/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpSystem.h"

#include <cstdlib>

#include <algorithm>
#include <memory>
#include <utility>

#include <spdlog/spdlog.h>

#include "Camera/FieldOfView.h"
#include "Debug/DebugEnv.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "GameClock.h"
#include "HelpMessageSets.h"
#include "InputPromptIcon.h"
#include "Locator.h"
#include "SpiritsRuntime.h"
#include "TextSplitter.h"
#include "ToolTips.h"

namespace openblack::help
{

namespace
{
/// What this module keeps between calls (Locator::scriptState)
struct HelpSystemState
{
	std::unique_ptr<HelpSystem> helpSystem {};
};

HelpSystemState& HelpSystemData()
{
	return openblack::Locator::scriptState::value().Get<HelpSystemState>();
}

bool Tracing()
{
	static const bool k_Trace = debug_env::TextTrace();
	return k_Trace && spdlog::get("game") != nullptr;
}

std::string Utf8(std::u16string_view text)
{
	std::string out;
	out.reserve(text.size());
	for (const char16_t c : text)
	{
		if (c < 0x80)
		{
			out.push_back(c == u'\n' ? '|' : static_cast<char>(c));
		}
		else if (c < 0x800)
		{
			out.push_back(static_cast<char>(0xC0 | (c >> 6)));
			out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
		}
		else
		{
			out.push_back(static_cast<char>(0xE0 | (c >> 12)));
			out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
			out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
		}
	}
	return out;
}
} // namespace

uint32_t CountWords(std::u16string_view text)
{
	// Count the non-empty pieces the splitter returns. No $M icon; the colour and font codes only change a state nobody
	// reads here.
	text_splitter::Splitter splitter(text);
	text_splitter::DrawState state;
	std::u16string word;
	uint32_t words = 0;
	while (!splitter.AtEnd())
	{
		splitter.Next(word, state, false);
		if (!word.empty())
		{
			++words;
		}
	}
	return words;
}

float ReadSpeedFactor(float readSpeed)
{
	// Below, equal or unordered (NaN) -> the first branch
	if (!(static_cast<double>(readSpeed) > 0.5))
	{
		const float scaled = readSpeed * 4.0f;
		return 3.0f - scaled;
	}
	// Each result rounded to 24 bits
	float value = static_cast<float>(static_cast<double>(readSpeed) - 0.5);
	value = value + value;
	value = static_cast<float>(1.0 - static_cast<double>(value));
	value = static_cast<float>(static_cast<double>(value) * 0.80000000000000004);
	return static_cast<float>(static_cast<double>(value) + 0.20000000000000001);
}

VoiceRoute RouteOf(int32_t narrator, audio::TextVoice voice)
{
	if (voice.bank == audio::SfxBank::HelpSprites)
	{
		if (narrator == helptext::k_NarratorGoodSpirit)
		{
			return VoiceRoute::GoodSpirit;
		}
		if (narrator == helptext::k_NarratorEvilSpirit)
		{
			return VoiceRoute::EvilSpirit;
		}
	}
	return voice.sample != 0 && voice.bank != audio::SfxBank::None ? VoiceRoute::Narration : VoiceRoute::None;
}

int32_t SpiritWhoTalks(int32_t narrator)
{
	if (narrator == helptext::k_NarratorGoodSpirit)
	{
		return 1;
	}
	return narrator == helptext::k_NarratorEvilSpirit ? 2 : 0;
}

Speaker ApplySpeakerOverride(SpeakerOverride override, int32_t narrator)
{
	const bool good = narrator == helptext::k_NarratorGoodSpirit;
	const bool evil = narrator == helptext::k_NarratorEvilSpirit;
	switch (override)
	{
	case SpeakerOverride::None:
		break;
	case SpeakerOverride::SilenceGood:
		return {.narrator = narrator, .silenced = good};
	case SpeakerOverride::SilenceEvil:
		return {.narrator = narrator, .silenced = evil};
	case SpeakerOverride::ForceGood:
		return {.narrator = evil ? helptext::k_NarratorGoodSpirit : narrator, .silenced = false};
	case SpeakerOverride::ForceEvil:
		return {.narrator = good ? helptext::k_NarratorEvilSpirit : narrator, .silenced = false};
	}
	return {.narrator = narrator, .silenced = false};
}

int32_t ResolveScriptAdvisor(int32_t type, int discreteAlignment, const std::function<int()>& rand100)
{
	switch (type)
	{
	case 2:
		return 2;
	case 3:
		return discreteAlignment < 3 ? 2 : 1;
	case 4:
		return discreteAlignment >= 3 ? 2 : 1;
	case 5:
		return (rand100 ? rand100() : 0) > 50 ? 2 : 1;
	default:
		return 1;
	}
}

HelpSystem::HelpSystem(Info info, Queries queries, Hooks hooks)
    : _info(info)
    , _queries(std::move(queries))
    , _hooks(std::move(hooks))
    // The line height is H / 28 when the language needs bigger text, else H / 30. (pending) re-initialise the text after
    // a resolution change
    , _display(_queries.screenHeight ? _queries.screenHeight() : 480, helptext::NeedsBiggerText())
{
}

namespace
{
/// Delete the icon, then the slot is empty
void DeleteIcon(input_prompt::Handle& icon)
{
	input_prompt::Destroy(icon);
	icon = input_prompt::k_None;
}
} // namespace

void HelpSystem::Draw3D(float frameMs, const TextRegion& region, int screenWidth)
{
	_display.Advance(frameMs, _textDraw);
	if (!_waitClick)
	{
		return;
	}
	const int size = ClickCueHeight(region);
	const int y = ClickCueY(region);
	if (auto* icon = input_prompt::Get(_clickIcon); icon != nullptr)
	{
		icon->y = y;
		return;
	}
	input_prompt::Desc desc;
	// No icon when the action cannot be resolved
	if (!input_prompt::ResolveAction(1, desc.animType, desc.clickType, desc.row, desc.keyName))
	{
		return;
	}
	if (desc.animType == 0)
	{
		desc.animType = 1; // the button blinks
	}
	// HELP_TEXT_TOOLTIP_07
	desc.text = helptext::Get(helptext::k_ToolTipContinue);
	desc.start = 0.0f;
	desc.target = 1.0f;
	desc.duration = 1.0f;
	desc.x = screenWidth - 4;
	desc.y = y;
	desc.size = size;
	desc.align = 0x11;
	desc.c1 = input_prompt::k_Yellow;
	desc.c2 = input_prompt::k_White;
	desc.alpha8 = 0x80;
	_clickIcon = input_prompt::Create(desc);
}

void HelpSystem::ProcessIcons()
{
	if (!_display.HasNewestText())
	{
		DeleteIcon(_clickIcon);
		DeleteIcon(_controlKeyIcon);
	}
}

void HelpSystem::ResetIcons()
{
	DeleteIcon(_clickIcon);
	help::tooltips::ResetIcon();
	DeleteIcon(_controlKeyIcon);
}

void HelpSystem::RunText(bool singleLine, uint32_t textId, int32_t withInteraction)
{
	RunTextWithNumber(singleLine, textId, 0.0f, withInteraction); // number 0
}

void HelpSystem::RunTextWithNumber(bool singleLine, uint32_t textId, float number, int32_t withInteraction)
{
	if (textId >= helptext::k_TextCount) // an invalid text reports an error, then text 0
	{
		if (const auto logger = spdlog::get("game"); logger != nullptr)
		{
			SPDLOG_LOGGER_WARN(logger, "Invalid text {}", textId);
		}
		textId = 0;
	}
	if (singleLine || _display.IsSingleLine())
	{
		ClearAllText();
	}
	_display.SetSingleLine(singleLine);
	SayText(textId, withInteraction, number);
}

void HelpSystem::TempText(bool singleLine, std::u16string_view text, int32_t withInteraction)
{
	TempTextWithNumber(singleLine, text, 0.0f, withInteraction); // number 0
}

void HelpSystem::TempTextWithNumber(bool singleLine, std::u16string_view text, float number, int32_t withInteraction)
{
	// "*" + the string. TempText also reports "Development text being used in game!" and the string (debug output, not
	// ported)
	const std::u16string shown = u"*" + std::u16string(text);
	if (singleLine || _display.IsSingleLine())
	{
		ClearAllText();
	}
	_display.SetSingleLine(singleLine);
	AddText(shown, withInteraction, number, 1, 0); // narrator 1, text id 0
}

void HelpSystem::ClearDialogue()
{
	ClearAllText();
}

void HelpSystem::CloseDialogue()
{
	_display.Close();
	ClearAllText();
}

void HelpSystem::SayText(uint32_t textId, int32_t withInteraction, float number)
{
	if (textId >= helptext::k_TextCount)
	{
		textId = 0;
	}
	const auto entry = _queries.textEntry ? _queries.textEntry(textId) : helptext::GetEntry(textId);
	const auto voice = _queries.textVoice ? _queries.textVoice(textId) : audio::voices::Table().Get(textId);
	const auto speaker = ApplySpeakerOverride(_speakerOverride, entry.narrator);
	AddText(entry.text, withInteraction, number, speaker.narrator, textId);
	AddHistory(textId, withInteraction, number, speaker.narrator);
	const auto route = speaker.silenced ? VoiceRoute::None : RouteOf(speaker.narrator, voice);
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: {} {} narrator {} voice bank {} sample {} route {}", textId, entry.name,
		                   entry.narrator, static_cast<int>(voice.bank), voice.sample, static_cast<int>(route));
	}
	if (route != VoiceRoute::None && _hooks.sayVoice)
	{
		_hooks.sayVoice(textId, route, voice);
	}
}

void HelpSystem::AddText(const std::u16string& text, int32_t withInteraction, float number, int32_t narrator, uint32_t textId)
{
	// The TEXT_DRAW gate reads the entry of the text being made current here (inferred: TEMP_TEXT's id 0 reads entry 0)
	const auto entry = _queries.textEntry ? _queries.textEntry(textId) : helptext::GetEntry(textId);
	_display.Add(text, number, narrator, entry.arg0 != 0);
	if (_hooks.showText)
	{
		_hooks.showText(text, number, narrator);
	}
	if (_hooks.textStarted)
	{
		_hooks.textStarted(textId);
	}
	DeleteIcon(_clickIcon);
	DeleteIcon(_controlKeyIcon);
	StartReadingTime(text);
	_waitClick = withInteraction == 1;
	_noClick = withInteraction == 2;
	for (size_t i = _texts.size() - 1; i > 0; --i) // shift the five older texts down
	{
		_texts[i] = _texts[i - 1];
	}
	_texts[0] = textId;
	// The key combination icon is not ported
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: show {} narrator {} interaction {} words {} turns {}..{} \"{}\"", textId,
		                   narrator, withInteraction, CountWords(text), _startTurn, _endTurn, Utf8(text));
	}
}

void HelpSystem::StartReadingTime(std::u16string_view text)
{
	const uint32_t words = CountWords(text);
	const float factor = ReadSpeedFactor(_readSpeed);
	const auto gameTurns = static_cast<int32_t>(_info.readDefaultWordGTTime * words + _info.readDefaultAdjustGTTime);
	const uint32_t turn = Turn();
	_startTurn = turn;
	// Each step rounded to a float (the FPU at 24 bits)
	const float seconds =
	    static_cast<float>(gameTurns) * static_cast<float>(game_clock::MsPerTurn()) * game_clock::k_SecondsPerMs * factor;
	// The seconds back to game turns
	_endTurn = turn + static_cast<uint32_t>(game_clock::TicksForSeconds(seconds));
	const int32_t now = NowMs();
	_startMs = now;
	// Each step rounded to a float (the FPU at 24 bits: past 2^24 ms the sum loses its last bits, as in the original)
	_endMs = static_cast<int32_t>(static_cast<float>(static_cast<double>(seconds * 1000.0f) + static_cast<double>(now)));
}

bool HelpSystem::HasVoice(uint32_t textId) const
{
	if (textId >= helptext::k_TextCount)
	{
		return false;
	}
	const auto voice = _queries.textVoice ? _queries.textVoice(textId) : audio::voices::Table().Get(textId);
	return voice.HasVoice() && _queries.voiceBankLoaded && _queries.voiceBankLoaded(voice.bank);
}

bool HelpSystem::IsTextRead() const
{
	if (_waitClick)
	{
		return false;
	}
	const uint32_t textId = _texts[0];
	if (HasVoice(textId))
	{
		const auto voice = _queries.textVoice ? _queries.textVoice(textId) : audio::voices::Table().Get(textId);
		if (voice.bank == audio::SfxBank::HelpSprites) // the advisors
		{
			return !(_queries.advisorsTalking && _queries.advisorsTalking());
		}
		if (_queries.isPlaying && _queries.isPlaying(voice.bank, audio::VoiceOwner::Narration, voice.sample))
		{
			_endMs = NowMs() + 450;
			return false;
		}
		return static_cast<uint32_t>(NowMs()) >= static_cast<uint32_t>(_endMs);
	}
	if (CitadelClock())
	{
		return static_cast<uint32_t>(_endMs) < static_cast<uint32_t>(NowMs());
	}
	return _endTurn < Turn();
}

void HelpSystem::ClearAllText()
{
	_display.Reset(true); // also clears the single line flag
	_texts.fill(0);
	ClearTextDisplayed();
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: clear all");
	}
}

void HelpSystem::Reset()
{
	ClearAllText();
	_messageSets.sentTurn.fill(0);
	_messageSets.sentCount.fill(0);
	_categoryTurns.fill(0);
	_messageSets.banterCount = 0;
	SetWideScreen(0, 0);
	ResetIcons();
	_historyCount = 0;
	_historyNext = 0;
	_helpOn = 1;
	_speakerOverride = SpeakerOverride::None;
}

void HelpSystem::TriggerCategory(int32_t category)
{
	// The category's turn is the game turn (unchecked in the original)
	if (category >= 0 && static_cast<size_t>(category) < _categoryTurns.size())
	{
		_categoryTurns.at(static_cast<size_t>(category)) = Turn();
	}
}

uint32_t HelpSystem::GetCategoryTurn(int32_t category) const
{
	return category >= 0 && static_cast<size_t>(category) < _categoryTurns.size()
	           ? _categoryTurns.at(static_cast<size_t>(category))
	           : 0;
}

bool HelpSystem::DialogueControlRequest(uint32_t task)
{
	if (IsDialogueControlled())
	{
		return false;
	}
	SetCurrentControl(task);
	ClearAllText();
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: dialogue control to task {}", task);
	}
	return true;
}

void HelpSystem::ClearDialogueControl()
{
	_dialogueOwner = 0;
	_display.Close();
}

void HelpSystem::ReleaseDialogueControl(uint32_t task)
{
	if (_dialogueOwner != task)
	{
		return;
	}
	// The advisors go home at once (arg 1) when the task is a Help script (VMScriptType 2)
	const uint32_t type = _queries.taskScriptType ? _queries.taskScriptType(task) : 1;
	const int32_t helpScript = type == 2 ? 1 : 0;
	ClearDialogueControl();
	// Both spirits sent home first
	SpiritHome(1, 0);
	SpiritHome(2, 0);
	SetWideScreen(0, 0);
	if (_hooks.spiritStop)
	{
		_hooks.spiritStop(1, 1);
		_hooks.spiritStop(2, 1);
	}
	SpiritHome(1, helpScript);
	SpiritHome(2, helpScript);
	ClearAllText();
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: dialogue control of task {} released", task);
	}
}

void HelpSystem::ReleaseWideScreenOf(uint32_t task)
{
	if (_wideScreen != 0 && task == _wideScreenOwner)
	{
		SetWideScreen(0, 0);
	}
}

void HelpSystem::SpiritHome(int32_t spirit, int32_t arg)
{
	if (_hooks.spiritHome)
	{
		_hooks.spiritHome(spirit, arg);
	}
}

void HelpSystem::SetWideScreen(int32_t on, uint32_t owner)
{
	if (_wideScreen == on)
	{
		return;
	}
	_wideScreen = on;
	_wideScreenOwner = on != 0 ? owner : 0;
	if (_hooks.wideScreen)
	{
		_hooks.wideScreen(on != 0);
	}
	if (Tracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: wide screen {} (task {})", on, _wideScreenOwner);
	}
}

void HelpSystem::ClearTextDisplayed()
{
	if (_waitClick)
	{
		DeleteIcon(_clickIcon);
	}
	DeleteIcon(_controlKeyIcon);
	_noClick = false;
	_waitClick = false;
	_endTurn = 0;
	_endMs = 0;
}

bool HelpSystem::ShownLongEnough() const
{
	// 1 s while it waits for the click, 0.5 s otherwise
	const float limit = _waitClick ? 1.0f : 0.5f;
	// the FPU at 24 bits: float
	float seconds;
	if (CitadelClock())
	{
		seconds = static_cast<float>(static_cast<uint32_t>(NowMs() - _startMs)) * game_clock::k_SecondsPerMs;
	}
	else
	{
		seconds = static_cast<float>(static_cast<int32_t>(Turn() - _startTurn)) *
		          static_cast<float>(static_cast<int32_t>(game_clock::MsPerTurn())) * game_clock::k_SecondsPerMs;
	}
	return !(seconds < limit);
}

int HelpSystem::ProcessInterface(bool click)
{
	const bool key = _queries.skipKey && _queries.skipKey();
	if (_noClick)
	{
		return 1;
	}
	if (!click && !key)
	{
		return 1;
	}
	if (_queries.playBack && _queries.playBack())
	{
		return 1;
	}
	if (IsTextRead())
	{
		return 1;
	}
	if (!ShownLongEnough() && !key)
	{
		return 1;
	}
	if (_clickPending)
	{
		_clickPending = false;
		return k_ClickTaken;
	}
	if (IsScriptWideScreen() || key)
	{
		if (_texts[0] != 0)
		{
			if (_hooks.spiritStop)
			{
				_hooks.spiritStop(1, 1);
				_hooks.spiritStop(2, 1);
			}
			if (_hooks.stopVoicesOnClick)
			{
				_hooks.stopVoicesOnClick();
			}
		}
		ClearTextDisplayed();
		if (Tracing())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: click cuts {}", _texts[0]);
		}
		return k_ClickTaken;
	}
	if (_waitClick)
	{
		_waitClick = false;
		DeleteIcon(_clickIcon);
		if (Tracing())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: click ends the wait of {}", _texts[0]);
		}
	}
	return 1;
}

void HelpSystem::AddHistory(uint32_t textId, int32_t withInteraction, float number, int32_t narrator)
{
	if (++_historyCount > static_cast<int32_t>(k_HistorySize))
	{
		_historyCount = static_cast<int32_t>(k_HistorySize);
	}
	_history[static_cast<size_t>(_historyNext)] = {textId, withInteraction, number, narrator};
	_historyNext = (_historyNext + 1) % static_cast<int32_t>(k_HistorySize);
}

const HistoryEntry* HelpSystem::GetHistory(int32_t i) const
{
	if (i < 0 || i >= _historyCount)
	{
		return nullptr;
	}
	const auto index = (_historyNext - i + 0x3FF) & 0x3FF;
	return &_history[static_cast<size_t>(index)];
}

HelpSystem* Get()
{
	// nothing without the script state (the unit tests that make no services)
	return openblack::Locator::scriptState::has_value() ? HelpSystemData().helpSystem.get() : nullptr;
}

void Process(const script_control::Vm& vm, uint32_t turn)
{
	// The interface's hand state. (pending) the help system's field-of-view object is not reset: its reader is not ported
	if (Locator::handSystem::has_value())
	{
		Locator::handSystem::value().UpdateInterfaceHandState();
	}
	if (auto* helpSystem = Get(); helpSystem != nullptr)
	{
		helpSystem->ProcessIcons();
	}
	// Both spirits' turn, the evil one first
	if (auto* spirits = spirits::Get(); spirits != nullptr)
	{
		spirits->ProcessTurn();
	}
	tooltips::ProcessTurn();
	// The did-you-know bubble's thing gone or off the screen (no mesh counts as off; no temple test) closes the bubble;
	// else the bubble's life is reset to 3.0
	if (auto* helpSystem = Get(); helpSystem != nullptr && helpSystem->GetBubbleThing().has_value())
	{
		const auto thing = static_cast<entt::entity>(*helpSystem->GetBubbleThing());
		auto& registry = Locator::entitiesRegistry::value();
		const bool shown = registry.Valid(thing) && field_of_view::ObjectOnScreen(thing);
		helpSystem->ProcessBubble(!shown);
	}
	// With the help on: the conditions and the banter
	if (auto* helpSystem = Get(); helpSystem != nullptr && helpSystem->IsHelpSystemOn())
	{
		message_sets::ProcessConditions(*helpSystem);
		message_sets::ProcessBanter(*helpSystem, vm, turn);
	}
}

void Start(HelpSystem::Info info, HelpSystem::Queries queries, HelpSystem::Hooks hooks)
{
	HelpSystemData().helpSystem = std::make_unique<HelpSystem>(info, std::move(queries), std::move(hooks));
}

void Shutdown()
{
	HelpSystemData().helpSystem.reset();
}

void HelpSystem::SetBubbleProperties(uint32_t thing, uint32_t text)
{
	if (_bubbleThing == thing) // the same thing again closes the bubble
	{
		_bubbleThing.reset();
		return;
	}
	_bubbleText = text;
	_bubbleThing = thing;
	_bubble.Restart();
}

void HelpSystem::ProcessBubble(bool thingGoneOrOffScreen)
{
	if (!_bubbleThing.has_value())
	{
		return;
	}
	if (thingGoneOrOffScreen)
	{
		_bubbleThing.reset();
		return;
	}
	_bubble.KeepAlive();
}

} // namespace openblack::help
