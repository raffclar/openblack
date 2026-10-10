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
namespace lip_sync = openblack::audio::lip_sync;

/// Inside this far towards an edge an advisor says its line at once
constexpr float k_NoDelayHover = 0.95f;
constexpr float k_DelayMsPerHover = 250.0f;
constexpr float k_MaxDelayMs = 500.0f;
/// The interruption lines an advisor picks from when cut short
constexpr int32_t k_InterruptionLines = 5;
constexpr float k_SecondsPerMs = 0.001f;
/// A line whose sound has not started playing this long after it should have is stopped
constexpr float k_NotPlayingStop = 0.5f;
/// A stopping line's tags up to this long past its end are made
constexpr float k_FlushPastEnd = 100.0f;
} // namespace

AdvisorVoices::AdvisorVoices(Audio audio)
    : AdvisorVoices(std::move(audio), Hooks())
{
}

AdvisorVoices::AdvisorVoices(Audio audio, Hooks hooks)
    : _audio(std::move(audio))
    , _hooks(std::move(hooks))
    , _spectrum(2 * static_cast<size_t>(lip_sync::k_Window))
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
	UpdateSaySentence(advisor);
}

void AdvisorVoices::UpdateSaySentence(int index)
{
	auto& advisor = _advisors.at(static_cast<size_t>(index));
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
	if (started)
	{
		KeepRecording(index, advisor.line);
	}
}

void AdvisorVoices::KeepRecording(int advisor, uint32_t line)
{
	_sentenceStartTick = Now();
	std::optional<Recording> recording = _audio.recording ? _audio.recording(line) : std::nullopt;
	_samples.clear();
	_frames = 0;
	_sampleRate = 0;
	_duration = 0.0f;
	if (recording && recording->channels > 0 && recording->sampleRate > 0)
	{
		_samples = std::move(recording->samples);
		_frames = static_cast<int>(_samples.size()) / recording->channels;
		_sampleRate = recording->sampleRate;
		_duration = static_cast<float>(_frames) / static_cast<float>(_sampleRate);
	}
	// A recording without tag data, or a label with a mistake in it, leaves the line without gestures
	std::vector<spirits::AudioTag> tags;
	if (recording && recording->labels)
	{
		tags = spirits::BuildSentenceTags(*recording->labels, _tagWord);
	}
	if (_hooks.setTags)
	{
		_hooks.setTags(advisor, std::move(tags));
	}
}

void AdvisorVoices::Update()
{
	for (int i = 0; i < k_Advisors; ++i)
	{
		UpdateSaySentence(i);
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
	// Whatever gestures the line had left are made now, by the advisor that was saying it
	if (_hooks.stopTags)
	{
		_hooks.stopTags(advisor, _duration + k_FlushPastEnd);
	}
	_sentence = 0;
	_speaker = -1;
}

std::optional<spirits::LipSyncFrame> AdvisorVoices::ApplyLipSync(int advisor, float dt)
{
	if (!IsTalking(advisor) || _sentence == 0)
	{
		return std::nullopt;
	}
	auto& state = _advisors.at(static_cast<size_t>(advisor));
	spirits::LipSyncFrame frame;
	frame.time = static_cast<float>(Now() - _sentenceStartTick) * k_SecondsPerMs;
	const int64_t position = _audio.playPositionMs ? _audio.playPositionMs(_sentence) : -1;
	if (position < 0)
	{
		if (frame.time > k_NotPlayingStop)
		{
			StopSentence(advisor);
		}
	}
	else
	{
		frame.time = static_cast<float>(position) * k_SecondsPerMs;
		frame.playing = true;
		if (!_samples.empty())
		{
			lip_sync::UpdateKey(lip_sync::Settings {}, state.key, dt, frame.time, _samples, _frames, _sampleRate, _spectrum);
		}
	}
	frame.weights = state.key.weights;
	return frame;
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
