/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MusicPlayer.h"

#include <algorithm>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>

using namespace openblack::audio;

MusicPlayer::MusicPlayer(MusicBackend& backend)
    : _backend(backend)
{
}

MusicPlayer::~MusicPlayer()
{
	Stop(false);
}

bool MusicPlayer::Play(const MusicPlayOptions& options)
{
	if (!options.bank || options.bank->GetChunkCount() == 0)
	{
		Stop(true);
		return false;
	}

	// A channel already playing the bank is told its new volume, and becomes the one heard if it was fading out
	for (size_t i = 0; i < _channels.size(); ++i)
	{
		auto& channel = _channels.at(i);
		if (!channel.active || channel.bank != options.bank)
		{
			continue;
		}
		channel.fadeIn = options.fadeIn;
		channel.targetVolume = options.volume;
		channel.sync = options.sync;
		if (channel.placement && options.position)
		{
			channel.placement->position = *options.position;
			_backend.SetPlacement(channel.stream, channel.placement);
		}
		_current = static_cast<int>(i);
		return true;
	}

	const auto free = std::ranges::find_if(_channels, [](const Channel& c) { return !c.active; });
	if (free == _channels.end())
	{
		return false;
	}
	auto& channel = *free;
	channel = Channel {};
	channel.active = true;
	channel.bank = options.bank;
	channel.groupId = options.bank->groupId;
	channel.targetVolume = options.volume;
	channel.fadeIn = options.fadeIn;
	channel.volume = options.fadeIn ? 0 : options.volume;
	channel.loops = options.bank->loopOverride.value_or(options.loops);
	channel.sync = options.sync;
	channel.pitchPercent = options.pitchPercent;
	channel.onFinished = options.onFinished;
	channel.onCue = options.onCue;
	if (options.position)
	{
		const auto& bank = *options.bank;
		channel.placement = MusicPlacement {
		    .position = *options.position,
		    .minDistance = bank.minDistance.value_or(options.minDistance),
		    .maxDistance = bank.maxDistance.value_or(options.maxDistance),
		    .distanceScale = bank.distanceScale.value_or(options.distanceScale),
		};
	}
	Start(static_cast<size_t>(free - _channels.begin()), options.startChunk);
	return true;
}

void MusicPlayer::StopBank(const std::shared_ptr<const MusicBank>& bank, bool fadeOut)
{
	const auto found = std::ranges::find_if(_channels, [&bank](const Channel& c) { return c.active && c.bank == bank; });
	if (found == _channels.end())
	{
		return;
	}
	const auto index = static_cast<size_t>(found - _channels.begin());
	if (std::cmp_equal(_current, index))
	{
		Stop(fadeOut);
	}
	else if (!fadeOut)
	{
		Release(index);
	}
}

// The music thread starting a channel: from the requested chunk, or where a channel of the same music group has got
// to, and from then on it is the channel heard
void MusicPlayer::Start(size_t index, uint32_t startChunk)
{
	auto& channel = _channels.at(index);
	const auto chunkCount = channel.bank->GetChunkCount();
	auto chunk = (startChunk >= 1 && startChunk <= chunkCount) ? startChunk : 1;
	uint32_t skipFrames = 0;
	if (channel.sync && channel.groupId != -1)
	{
		for (size_t i = 0; i < _channels.size(); ++i)
		{
			auto& other = _channels.at(i);
			if (i == index || !other.active || other.groupId != channel.groupId || other.queued.empty())
			{
				continue;
			}
			// Only what has not been heard yet is still queued
			for (auto played = _backend.ReleasePlayedChunks(other.stream); played > 0 && !other.queued.empty(); --played)
			{
				other.queued.pop_front();
			}
			if (!other.queued.empty() && other.queued.front() < chunkCount)
			{
				chunk = other.queued.front() + 1;
				skipFrames = _backend.GetPlayPosition(other.stream);
			}
			break;
		}
	}

	if (const auto logger = spdlog::get("audio"); logger != nullptr)
	{
		SPDLOG_LOGGER_DEBUG(logger, "Music {} (group {}) starts at chunk {} of {}, {} frames in", channel.bank->path,
		                    channel.groupId, chunk, chunkCount, skipFrames);
	}
	channel.stream = _backend.CreateStream(channel.pitchPercent);
	if (channel.placement && channel.stream != MusicBackend::k_InvalidStream)
	{
		_backend.SetPlacement(channel.stream, channel.placement);
	}
	channel.nextChunk = chunk - 1;
	channel.playingChunk = chunk;
	// The cues' clock starts at the start of the chunk playback starts in, and no cue has been passed yet: cues in
	// chunks before it are all passed at the first check
	channel.cueStartChunk = chunk;
	channel.cueStartFrames = skipFrames;
	channel.cueChecked = {};
	if (channel.stream != MusicBackend::k_InvalidStream &&
	    _backend.QueueChunk(channel.stream, *channel.bank, channel.nextChunk, skipFrames))
	{
		channel.queued.push_back(channel.nextChunk);
	}
	++channel.nextChunk;
	Feed(channel);
	ApplyVolume(channel);
	_backend.Play(channel.stream);
	_current = static_cast<int>(index);
}

// Keeps four chunks queued, going round the bank again while it has loops left
void MusicPlayer::Feed(Channel& channel)
{
	const auto chunkCount = channel.bank->GetChunkCount();
	while (!channel.ending && channel.queued.size() < k_QueuedChunks)
	{
		if (channel.nextChunk >= chunkCount)
		{
			if (channel.loops == 0)
			{
				channel.ending = true;
				break;
			}
			--channel.loops;
			channel.nextChunk = 0;
		}
		if (_backend.QueueChunk(channel.stream, *channel.bank, channel.nextChunk, 0))
		{
			channel.queued.push_back(channel.nextChunk);
		}
		++channel.nextChunk;
	}
}

void MusicPlayer::Release(size_t index)
{
	auto& channel = _channels.at(index);
	if (channel.stream != MusicBackend::k_InvalidStream)
	{
		_backend.Destroy(channel.stream);
	}
	channel = Channel {};
	if (std::cmp_equal(_current, index))
	{
		_current = k_NoChannel;
	}
}

void MusicPlayer::Stop(bool fadeOut)
{
	for (size_t i = 0; i < _channels.size(); ++i)
	{
		if (!_channels.at(i).active)
		{
			continue;
		}
		if (fadeOut)
		{
			_channels.at(i).targetVolume = 0;
		}
		else
		{
			Release(i);
		}
	}
}

bool MusicPlayer::IsActive() const
{
	return std::ranges::any_of(_channels, [](const Channel& c) { return c.active; });
}

void MusicPlayer::Update(std::chrono::microseconds dt)
{
	for (size_t i = 0; i < _channels.size(); ++i)
	{
		auto& channel = _channels.at(i);
		if (!channel.active)
		{
			continue;
		}
		for (auto played = _backend.ReleasePlayedChunks(channel.stream); played > 0 && !channel.queued.empty(); --played)
		{
			channel.queued.pop_front();
		}
		Feed(channel);
		if (!channel.queued.empty())
		{
			channel.playingChunk = channel.queued.front() + 1;
			if (!_backend.IsPlaying(channel.stream))
			{
				_backend.Play(channel.stream);
			}
			continue;
		}
		if (channel.ending && !_backend.IsPlaying(channel.stream))
		{
			// Played to the end
			auto onFinished = std::move(channel.onFinished);
			Release(i);
			if (onFinished)
			{
				onFinished();
			}
		}
	}

	_tickTime += dt;
	while (_tickTime >= k_Tick)
	{
		_tickTime -= k_Tick;
		CueTick();
		VolumeTick();
	}
}

// Only the channel heard passes its cues; a channel that isn't current keeps its clock running and catches up on the
// cues it missed once it is current again
void MusicPlayer::CueTick()
{
	if (_current == k_NoChannel)
	{
		return;
	}
	auto& channel = _channels.at(static_cast<size_t>(_current));
	if (!channel.active || !channel.onCue || channel.bank->cues.empty() || channel.stream == MusicBackend::k_InvalidStream)
	{
		return;
	}
	const auto now = CuePositionAfter(channel.cueStartChunk, channel.cueStartFrames + _backend.GetFramesPlayed(channel.stream));
	// The handler may play or stop music and so free the channel: the bank and handler are held for the calls
	const auto bank = channel.bank;
	const auto onCue = channel.onCue;
	const auto passed = CuesPassed(bank->cues, channel.cueChecked, now);
	channel.cueChecked = now;
	for (const auto cue : passed)
	{
		if (const auto logger = spdlog::get("audio"); logger != nullptr)
		{
			SPDLOG_LOGGER_DEBUG(logger, "Music {} passes cue {} (now at chunk {}, frame {})", bank->path, cue, now.chunk,
			                    now.frame);
		}
		onCue(cue);
	}
}

MusicCuePosition openblack::audio::CuePositionAfter(uint32_t startChunk, uint64_t framesHeard)
{
	return {
	    .chunk = startChunk + static_cast<uint32_t>(framesHeard / k_CueChunkFrames),
	    .frame = static_cast<uint32_t>(framesHeard % k_CueChunkFrames),
	};
}

std::vector<std::string_view> openblack::audio::CuesPassed(std::span<const MusicCue> cues, MusicCuePosition from,
                                                           MusicCuePosition to)
{
	std::vector<std::string_view> passed;
	for (const auto& cue : cues)
	{
		const auto at = MusicCuePosition {.chunk = cue.chunk, .frame = cue.frame};
		if (at > from && at <= to)
		{
			passed.emplace_back(cue.text);
		}
	}
	return passed;
}

// The current channel rises to its target, at once unless it fades in. Every other channel, and the current one
// above its target, falls by 3 a tick, and is released when it reaches silence.
void MusicPlayer::VolumeTick()
{
	bool changed = false;
	for (size_t i = 0; i < _channels.size(); ++i)
	{
		auto& channel = _channels.at(i);
		if (!channel.active)
		{
			continue;
		}
		const bool current = std::cmp_equal(_current, i);
		if (channel.volume < channel.targetVolume && current)
		{
			if (!channel.fadeIn)
			{
				channel.volume = channel.targetVolume;
				channel.fadeIn = true;
			}
			else
			{
				channel.volume = std::min(channel.volume + k_FadeInStep, channel.targetVolume);
			}
			ApplyVolume(channel);
			changed = true;
		}
		else if (channel.volume > channel.targetVolume || !current)
		{
			channel.volume = std::max(channel.volume - k_FadeOutStep, 0);
			ApplyVolume(channel);
			changed = true;
			if (channel.volume == 0)
			{
				Release(i);
			}
		}
	}
	// The mix as it fades, each bank playing with its volume out of 127
	if (const auto logger = spdlog::get("audio"); changed && logger != nullptr && logger->should_log(spdlog::level::debug))
	{
		std::string mix;
		for (const auto& channel : _channels)
		{
			if (channel.active)
			{
				mix += fmt::format("{}{} {}", mix.empty() ? "" : ", ", channel.bank->path, channel.volume);
			}
		}
		SPDLOG_LOGGER_DEBUG(logger, "Music mix: {}", mix.empty() ? "silent" : mix);
	}
}

void MusicPlayer::ApplyVolume(const Channel& channel)
{
	const auto bankVolume = static_cast<float>(channel.bank->bankVolume) / static_cast<float>(k_MaxVolume);
	_backend.SetVolume(channel.stream, static_cast<float>(channel.volume) / static_cast<float>(k_MaxVolume) * bankVolume);
}
