/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AdvisorVoices.h"

#include <cmath>

#include <utility>

namespace openblack::help
{

namespace
{
/// Inside this far towards an edge an advisor says its line at once
constexpr float k_NoDelayHover = 0.95f;
constexpr float k_DelayMsPerHover = 250.0f;
constexpr float k_MaxDelayMs = 500.0f;
/// The interruption lines an advisor picks from when cut short
constexpr int32_t k_InterruptionLines = 5;
} // namespace

AdvisorVoices::AdvisorVoices(Audio audio)
    : _audio(std::move(audio))
{
}

uint32_t AdvisorVoices::SayDelayMs(float hoverX)
{
	const float beyond = std::fabs(hoverX) - k_NoDelayHover;
	if (!(beyond >= 0.0f))
	{
		return 0;
	}
	float delay = (beyond + 1.0f) * k_DelayMsPerHover;
	if (delay > k_MaxDelayMs)
	{
		delay = k_MaxDelayMs;
	}
	return static_cast<uint32_t>(static_cast<int32_t>(delay));
}

void AdvisorVoices::Say(int advisor, uint32_t line, bool onlyIfSilent, float hoverX)
{
	SaySentence(advisor, line, onlyIfSilent, SayDelayMs(hoverX));
	_advisors.at(static_cast<size_t>(advisor)).active = true;
}

void AdvisorVoices::SaySentence(int advisor, uint32_t line, bool onlyIfSilent, uint32_t delayMs)
{
	_speaker = advisor;
	if (_sentence != 0)
	{
		if (onlyIfSilent)
		{
			if (IsTalking(advisor))
			{
				return;
			}
		}
		else
		{
			StopSentence(advisor);
		}
	}
	const uint32_t count = _audio.lineCount ? _audio.lineCount() : 0;
	if (line < 1 || line > count)
	{
		return;
	}
	auto& state = _advisors.at(static_cast<size_t>(advisor));
	state.line = line;
	state.startTick = Now() + delayMs;
	UpdateSaySentence(state);
}

void AdvisorVoices::UpdateSaySentence(Advisor& advisor)
{
	if (advisor.startTick == 0)
	{
		return;
	}
	// The clock compared as signed, so that it may wrap
	if (static_cast<int32_t>(Now()) < static_cast<int32_t>(advisor.startTick))
	{
		return;
	}
	advisor.startTick = 0;
	const bool started = _audio.start && _audio.start(advisor.line);
	_sentence = started ? advisor.line : 0;
}

void AdvisorVoices::Update()
{
	for (int i = 0; i < k_Advisors; ++i)
	{
		UpdateSaySentence(_advisors.at(static_cast<size_t>(i)));
		static_cast<void>(TalkingOrJustStopped(i));
	}
}

bool AdvisorVoices::IsTalking(int advisor)
{
	if (_speaker != advisor)
	{
		return false;
	}
	auto& state = _advisors.at(static_cast<size_t>(advisor));
	if (state.startTick == 0)
	{
		if (_sentence == 0)
		{
			return false;
		}
		if (!(_audio.isPlaying && _audio.isPlaying(_sentence)))
		{
			StopSentence(advisor);
			return false;
		}
	}
	state.lastTalkTick = Now();
	return true;
}

bool AdvisorVoices::TalkingOrJustStopped(int advisor)
{
	if (IsTalking(advisor))
	{
		return true;
	}
	return Now() - _advisors.at(static_cast<size_t>(advisor)).lastTalkTick < k_JustStoppedMs;
}

float AdvisorVoices::PercentageDone(int advisor)
{
	if (_speaker != advisor)
	{
		return 1.0f;
	}
	auto& state = _advisors.at(static_cast<size_t>(advisor));
	if (state.startTick != 0)
	{
		state.lastTalkTick = Now();
		return 0.0f;
	}
	if (_sentence == 0)
	{
		return 1.0f;
	}
	const float done = _audio.percentageDone ? _audio.percentageDone(_sentence) : 1.0f;
	if (done < 1.0f)
	{
		state.lastTalkTick = Now();
		return done;
	}
	return 1.0f;
}

void AdvisorVoices::StopSentence(int advisor)
{
	_advisors.at(static_cast<size_t>(advisor)).startTick = 0;
	if (_sentence == 0 || _speaker != advisor)
	{
		return;
	}
	if (_audio.stop)
	{
		_audio.stop(_sentence);
	}
	_sentence = 0;
	_speaker = -1;
}

void AdvisorVoices::Stop(int advisor)
{
	StopSentence(advisor);
	_advisors.at(static_cast<size_t>(advisor)).active = false;
}

bool AdvisorVoices::AnyTalking()
{
	return (IsActive(0) && TalkingOrJustStopped(0)) || (IsActive(1) && TalkingOrJustStopped(1));
}

void AdvisorVoices::Interrupt(int advisor)
{
	if (!IsActive(advisor) || !IsTalking(advisor))
	{
		return;
	}
	// The interruption line is picked, then the stop leaves no speaker, so the line is never started
	static_cast<void>(_audio.localRand ? _audio.localRand(k_InterruptionLines) : 0);
	Stop(advisor);
}

} // namespace openblack::help
