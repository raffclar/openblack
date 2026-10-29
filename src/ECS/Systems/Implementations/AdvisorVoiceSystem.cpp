/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "AdvisorVoiceSystem.h"

#include <chrono>
#include <optional>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "Common/GameRandom.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Help/Spirits.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/SoundDecoder.h"
#include "Sound/SoundRequestInterface.h"
#include "Sound/WaveCueLabels.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// The playing sound of an emitter, which has not finished
bool Sounding(entt::entity emitter)
{
	if (emitter == entt::null)
	{
		return false;
	}
	auto& audio = Locator::sounds::value();
	return audio.EmitterExists(emitter) && audio.GetStatus(emitter) != audio::AudioStatus::Stopped;
}

/// The real time since the game started, in milliseconds
uint32_t NowMs()
{
	return static_cast<uint32_t>(Locator::time::value().GetElapsedTime().count());
}

/// Whether the length of its sample times the advisor's line, not the sound device
bool TimedByLength()
{
	return audio::TimedByLength(Locator::sounds::value().IsDeviceLost(),
	                            Locator::time::value().GetFixedFrameTime().has_value());
}

/// A line's recording decoded once more, as the sound plays it, with the cue labels its gestures are read from
help::AdvisorVoices::Recording LineRecording(const audio::Sound& sound)
{
	help::AdvisorVoices::Recording recording;
	for (const auto& part : sound.buffer)
	{
		auto decoded = audio::DecodeSound(part, sound.sampleRate);
		if (!decoded.sound)
		{
			continue;
		}
		recording.channels = decoded.sound->channelLayout == audio::ChannelLayout::Stereo ? 2 : 1;
		recording.sampleRate = decoded.sound->sampleRate;
		recording.samples.insert(recording.samples.end(), decoded.sound->samples.begin(), decoded.sound->samples.end());
	}
	if (!sound.buffer.empty())
	{
		std::vector<help::spirits::TagLabel> labels;
		for (auto& label : audio::ReadWaveCueLabels(sound.buffer.front()))
		{
			labels.push_back({.text = std::move(label.text), .time = label.time});
		}
		recording.labels = std::move(labels);
	}
	return recording;
}
} // namespace

AdvisorVoiceSystem::AdvisorVoiceSystem()
    : _voices(VoiceAudio(), VoiceHooks())
{
}

help::AdvisorVoices::Audio AdvisorVoiceSystem::VoiceAudio()
{
	help::AdvisorVoices::Audio audio;
	audio.lineCount = []() {
		return static_cast<uint32_t>(
		    Locator::helpSpeechSystem::value().GetTable().GetBankCount(audio::SpeechBank::HelpSprites));
	};
	audio.start = [this](uint32_t line) {
		const auto sound = Locator::helpSpeechSystem::value().GetTable().FindSound(audio::SpeechBank::HelpSprites, line);
		if (!sound || !Locator::resources::value().GetSounds().Contains(*sound))
		{
			return false;
		}
		// Heard everywhere, wherever the advisor is. The voice rules know the advisors' lines by their own number.
		auto& sounds = Locator::sounds::value();
		_advisorLine = sounds.StartSoundEffect(
		    *sound, {.voiceOwner = audio::VoiceOwner::Number, .ownerNumber = audio::k_AdvisorOwnerNumber});
		// Its length is known once it has been made ready to play
		_advisorPlayback = {
		    .startMs = NowMs(),
		    .lengthMs = Locator::resources::value().GetSounds().Handle(*sound)->duration * 1000.0f,
		};
		// With the sound device lost, or with a fixed frame time, the line starts and lasts the length of its sample
		return _advisorLine != entt::null || TimedByLength();
	};
	audio.isPlaying = [this](uint32_t) { return AdvisorLinePlays(); };
	audio.percentageDone = [this](uint32_t) {
		if (TimedByLength())
		{
			return _advisorPlayback.Progress(NowMs());
		}
		return Sounding(_advisorLine) ? Locator::sounds::value().GetProgress(_advisorLine) : 1.0f;
	};
	audio.stop = [this](uint32_t) {
		if (_advisorLine != entt::null && Locator::sounds::value().EmitterExists(_advisorLine))
		{
			Locator::sounds::value().StopEmitter(_advisorLine);
		}
		_advisorLine = entt::null;
		_advisorPlayback = {};
	};
	audio.recording = [](uint32_t line) -> std::optional<help::AdvisorVoices::Recording> {
		const auto sound = Locator::helpSpeechSystem::value().GetTable().FindSound(audio::SpeechBank::HelpSprites, line);
		if (!sound || !Locator::resources::value().GetSounds().Contains(*sound))
		{
			return std::nullopt;
		}
		const auto begin = std::chrono::steady_clock::now();
		auto recording = LineRecording(*Locator::resources::value().GetSounds().Handle(*sound));
		if (const auto logger = spdlog::get("audio"); logger != nullptr)
		{
			const std::chrono::duration<float, std::milli> took = std::chrono::steady_clock::now() - begin;
			SPDLOG_LOGGER_DEBUG(logger, "Advisor line {}: {} samples at {} Hz, {} cue labels, read in {:.2f} ms", line,
			                    recording.samples.size(), recording.sampleRate, recording.labels ? recording.labels->size() : 0,
			                    took.count());
		}
		return recording;
	};
	audio.playPositionMs = [this](uint32_t) -> int64_t {
		if (!AdvisorLinePlays())
		{
			return -1;
		}
		if (TimedByLength())
		{
			return static_cast<int64_t>(NowMs() - _advisorPlayback.startMs);
		}
		return static_cast<int64_t>(Locator::sounds::value().GetProgress(_advisorLine) * _advisorPlayback.lengthMs);
	};
	audio.tickMs = []() { return NowMs(); };
	audio.localRand = [](int32_t n) { return Locator::gameRandom::value().LocalRand(n); };
	return audio;
}

bool AdvisorVoiceSystem::AdvisorLinePlays() const
{
	return audio::PlaysOn(Sounding(_advisorLine), TimedByLength(), _advisorPlayback, NowMs());
}

help::AdvisorVoices::Hooks AdvisorVoiceSystem::VoiceHooks()
{
	help::AdvisorVoices::Hooks hooks;
	hooks.setTags = [](int advisor, std::vector<help::spirits::AudioTag> tags) {
		if (Locator::advisorSystem::has_value() && Locator::advisorSystem::value().IsLoaded())
		{
			Locator::advisorSystem::value().GetController().SetSentenceTags(advisor, std::move(tags));
		}
	};
	hooks.stopTags = [](int advisor, float before) {
		if (Locator::advisorSystem::has_value() && Locator::advisorSystem::value().IsLoaded())
		{
			Locator::advisorSystem::value().GetController().StopSentence(advisor, before);
		}
	};
	return hooks;
}
