/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DialogueText.h"

#include <algorithm>
#include <utility>

#include "TextSplitter.h"

namespace openblack::help
{

namespace
{
/// The time the narration may fall silent before its text counts as read
constexpr int32_t k_NarrationGapMs = 450;
/// How long a text must have been shown before a click counts: longer while it waits for the click
constexpr float k_ClickWaitSeconds = 1.0f;
constexpr float k_ClickSeconds = 0.5f;
/// The display's gate for a text drawn or not
constexpr int k_DrawAlways = 2;
constexpr int k_DrawNever = 0;
} // namespace

float ReadSpeedFactor(float readSpeed)
{
	// Up to a half, or not a number: from 3 down to 1
	if (!(static_cast<double>(readSpeed) > 0.5))
	{
		const float scaled = readSpeed * 4.0f;
		return 3.0f - scaled;
	}
	// Above, from 1 down to 0.2, each step rounded to a float
	float value = static_cast<float>(static_cast<double>(readSpeed) - 0.5);
	value = value + value;
	value = static_cast<float>(1.0 - static_cast<double>(value));
	value = static_cast<float>(static_cast<double>(value) * 0.8);
	return static_cast<float>(static_cast<double>(value) + 0.2);
}

DialogueText::DialogueText(int screenHeight, bool biggerText, Settings settings, Queries queries, Hooks hooks)
    : _settings(settings)
    , _queries(std::move(queries))
    , _hooks(std::move(hooks))
    , _display(screenHeight, biggerText)
{
}

bool DialogueText::RunText(bool singleLine, uint32_t number, int32_t withInteraction)
{
	const bool valid = number < k_HelpTextCount;
	if (!valid)
	{
		number = 0;
	}
	if (singleLine || _display.IsSingleLine())
	{
		ClearAllText();
	}
	_display.SetSingleLine(singleLine);

	const auto text = _queries.text ? _queries.text(number) : Text {};
	AddText(text, number, withInteraction);

	const auto voice = _queries.voice ? _queries.voice(number) : std::nullopt;
	if (!voice)
	{
		return valid;
	}
	if (voice->advisorsBank && (text.narrator == k_NarratorGoodAdvisor || text.narrator == k_NarratorEvilAdvisor))
	{
		if (_hooks.advisorSays)
		{
			_hooks.advisorSays(text.narrator, number);
		}
	}
	else if (_hooks.narrate)
	{
		_hooks.narrate(number);
	}
	return valid;
}

void DialogueText::AddText(const Text& text, uint32_t number, int32_t withInteraction)
{
	// The display's own important flag is not used: the gate is worked out here from the text last shown
	_display.Add(std::u16string(text.text), 0.0f, text.narrator, true);
	StartReadingTime(text.text);
	_waitClick = withInteraction == 1;
	_noClick = withInteraction == 2;
	std::shift_right(_texts.begin(), _texts.end(), 1);
	_texts[0] = number;
}

void DialogueText::StartReadingTime(std::u16string_view text)
{
	const uint32_t words = CountWords(text);
	const float factor = ReadSpeedFactor(_settings.readSpeed);
	const auto turns = static_cast<int32_t>((_settings.turnsPerWord * words) + _settings.extraTurns);
	const uint32_t turn = Turn();
	_startTurn = turn;
	// Each step rounded to a float
	float seconds = static_cast<float>(turns * static_cast<int32_t>(_settings.msPerTurn)) * 0.001f;
	seconds *= factor;
	const auto turnsPerSecond = static_cast<float>(1000 / _settings.msPerTurn);
	_endTurn = turn + static_cast<uint32_t>(static_cast<int32_t>(turnsPerSecond * seconds));
	const int32_t now = NowMs();
	_startMs = now;
	_endMs = static_cast<int32_t>((seconds * 1000.0f) + static_cast<float>(now));
}

bool DialogueText::IsTextRead() const
{
	if (_waitClick)
	{
		return false;
	}
	const uint32_t number = _texts[0];
	const auto voice = number < k_HelpTextCount && _queries.voice ? _queries.voice(number) : std::nullopt;
	if (voice)
	{
		if (voice->advisorsBank)
		{
			return !(_queries.advisorsTalking && _queries.advisorsTalking());
		}
		if (_queries.narrationSounding && _queries.narrationSounding(number))
		{
			_endMs = NowMs() + k_NarrationGapMs;
			return false;
		}
		return static_cast<uint32_t>(NowMs()) >= static_cast<uint32_t>(_endMs);
	}
	if (InTemple())
	{
		return static_cast<uint32_t>(_endMs) < static_cast<uint32_t>(NowMs());
	}
	return _endTurn < Turn();
}

void DialogueText::ClearTextDisplayed()
{
	_noClick = false;
	_waitClick = false;
	_endTurn = 0;
	_endMs = 0;
}

void DialogueText::ClearAllText()
{
	_display.Reset(true);
	_texts.fill(0);
	ClearTextDisplayed();
}

void DialogueText::CloseDialogue()
{
	_display.Close();
	ClearAllText();
}

bool DialogueText::ShownLongEnough() const
{
	const float limit = _waitClick ? k_ClickWaitSeconds : k_ClickSeconds;
	float shown = 0.0f;
	if (InTemple())
	{
		shown = static_cast<float>(NowMs() - _startMs) * 0.001f;
	}
	else
	{
		shown = static_cast<float>((Turn() - _startTurn) * _settings.msPerTurn) * 0.001f;
	}
	return !(shown < limit);
}

void DialogueText::ProcessClick(bool click, bool skipKey)
{
	if (_noClick || (!click && !skipKey))
	{
		return;
	}
	if (IsTextRead())
	{
		return;
	}
	if (!ShownLongEnough() && !skipKey)
	{
		return;
	}
	// In a script's cut scene, or with the key, the text is cut short
	if ((_queries.scriptWideScreen && _queries.scriptWideScreen()) || skipKey)
	{
		if (_texts[0] != 0)
		{
			if (_hooks.interruptAdvisors)
			{
				_hooks.interruptAdvisors();
			}
			if (_hooks.stopNarration)
			{
				_hooks.stopNarration();
			}
		}
		ClearTextDisplayed();
		return;
	}
	// Otherwise the click only ends the wait for it
	_waitClick = false;
}

bool DialogueText::IsDrawn() const
{
	if (_settings.textDraw == 0)
	{
		return false;
	}
	if (_settings.textDraw == 1 && _texts[0] != 0)
	{
		return _queries.text ? _queries.text(_texts[0]).important : false;
	}
	return true;
}

void DialogueText::Update(float frameMs)
{
	_display.Advance(frameMs, IsDrawn() ? k_DrawAlways : k_DrawNever);
}

const TextFrame& DialogueText::Layout(int width, int height, int barPixels, const WidthFn& widthFn) const
{
	return _display.CachedLayout(width, height, barPixels, IsDrawn() ? k_DrawAlways : k_DrawNever, _settings.topToBottom,
	                             widthFn);
}

} // namespace openblack::help
