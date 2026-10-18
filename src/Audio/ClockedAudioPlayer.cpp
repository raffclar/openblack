/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ClockedAudioPlayer.h"

#include <cmath>

#include <algorithm>
#include <numeric>
#include <utility>

using namespace openblack::audio;

namespace
{
constexpr double k_MillisecondsPerSecond = 1000.0;
constexpr size_t k_BytesPerSample = sizeof(int16_t);
} // namespace

double ClockedAudioPlayer::Buffer::Milliseconds() const
{
	return static_cast<double>(frames) * k_MillisecondsPerSecond / static_cast<double>(std::max(sampleRate, 1));
}

ClockedAudioPlayer::ClockedAudioPlayer(Clock clock)
    : _clock(std::move(clock))
{
}

BufferId ClockedAudioPlayer::CreateBuffer(ChannelLayout layout, const std::vector<int16_t>& buffer, int sampleRate)
{
	const size_t channels = layout == ChannelLayout::Stereo ? 2 : 1;
	const auto id = _nextBuffer++;
	_buffers[id] = {.frames = buffer.size() / channels, .channels = channels, .sampleRate = sampleRate};
	return id;
}

void ClockedAudioPlayer::QueueBuffer(SourceId sourceId, BufferId buffer)
{
	if (auto* source = Find(sourceId); source != nullptr)
	{
		Advance(*source);
		source->queue.push_back(buffer);
	}
}

void ClockedAudioPlayer::DeleteBuffer(BufferId id)
{
	_buffers.erase(id);
}

SourceId ClockedAudioPlayer::CreateSource(float pitch, bool /*relative*/)
{
	const auto id = _nextSource++;
	_sources[id] = {.pitch = pitch};
	return id;
}

void ClockedAudioPlayer::DeleteSource(SourceId id)
{
	_sources.erase(id);
}

void ClockedAudioPlayer::UpdateSource(SourceId id, glm::vec3 /*pos*/, float /*volume*/, bool loop)
{
	SetLooping(id, loop);
}

void ClockedAudioPlayer::UpdateSource(SourceId id, float /*volume*/, bool loop)
{
	SetLooping(id, loop);
}

float ClockedAudioPlayer::GetDuration(BufferId id)
{
	const auto found = _buffers.find(id);
	return found == _buffers.end() ? 0.0f : static_cast<float>(found->second.Milliseconds() / k_MillisecondsPerSecond);
}

void ClockedAudioPlayer::PlaySource(SourceId id, glm::vec3 /*pos*/, float /*volume*/, bool loop)
{
	Play(id, loop);
}

void ClockedAudioPlayer::PlaySource(SourceId id, float /*volume*/, bool loop)
{
	Play(id, loop);
}

void ClockedAudioPlayer::Play(SourceId id, bool loop)
{
	SetLooping(id, loop);
	StartSource(id);
}

void ClockedAudioPlayer::StartSource(SourceId id)
{
	auto* source = Find(id);
	if (source == nullptr)
	{
		return;
	}
	Advance(*source);
	// A paused source goes on from where it is; any other starts again from the start of its queue
	if (source->status != AudioStatus::Paused)
	{
		source->offsetMs = 0.0;
	}
	source->status = AudioStatus::Playing;
	source->lastTick = _clock();
}

void ClockedAudioPlayer::PauseSource(SourceId id) const
{
	if (auto* source = Find(id); source != nullptr)
	{
		Advance(*source);
		if (source->status == AudioStatus::Playing)
		{
			source->status = AudioStatus::Paused;
		}
	}
}

void ClockedAudioPlayer::StopSource(SourceId id) const
{
	if (auto* source = Find(id); source != nullptr)
	{
		Advance(*source);
		source->status = AudioStatus::Stopped;
		source->offsetMs = QueueMilliseconds(*source);
	}
}

void ClockedAudioPlayer::SetPitch(SourceId id, float pitch)
{
	if (auto* source = Find(id); source != nullptr)
	{
		Advance(*source);
		source->pitch = pitch;
	}
}

void ClockedAudioPlayer::SetLooping(SourceId id, bool loop)
{
	if (auto* source = Find(id); source != nullptr)
	{
		Advance(*source);
		source->loop = loop;
	}
}

AudioStatus ClockedAudioPlayer::GetStatus(SourceId id) const
{
	auto* source = Find(id);
	if (source == nullptr)
	{
		return AudioStatus::Stopped;
	}
	Advance(*source);
	return source->status;
}

float ClockedAudioPlayer::GetProgress(size_t sizeInBytes, SourceId sourceId) const
{
	auto* source = Find(sourceId);
	if (source == nullptr || source->queue.empty() || sizeInBytes == 0)
	{
		return 0.0f;
	}
	Advance(*source);
	const auto found = _buffers.find(source->queue.front());
	if (found == _buffers.end())
	{
		return 0.0f;
	}
	const auto& buffer = found->second;
	const auto frames = source->offsetMs * static_cast<double>(buffer.sampleRate) / k_MillisecondsPerSecond;
	const auto bytes = frames * static_cast<double>(buffer.channels * k_BytesPerSample);
	return static_cast<float>(bytes / static_cast<double>(sizeInBytes));
}

std::vector<BufferId> ClockedAudioPlayer::UnqueueProcessedBuffers(SourceId id)
{
	std::vector<BufferId> processed;
	auto* source = Find(id);
	if (source == nullptr || source->loop)
	{
		return processed;
	}
	Advance(*source);
	while (!source->queue.empty())
	{
		const auto found = _buffers.find(source->queue.front());
		const auto length = found == _buffers.end() ? 0.0 : found->second.Milliseconds();
		if (source->status != AudioStatus::Stopped && source->offsetMs < length)
		{
			break;
		}
		processed.push_back(source->queue.front());
		source->queue.pop_front();
		source->offsetMs = std::max(source->offsetMs - length, 0.0);
	}
	return processed;
}

uint32_t ClockedAudioPlayer::GetSampleOffset(SourceId id) const
{
	auto* source = Find(id);
	if (source == nullptr || source->queue.empty())
	{
		return 0;
	}
	Advance(*source);
	const auto found = _buffers.find(source->queue.front());
	if (found == _buffers.end())
	{
		return 0;
	}
	return static_cast<uint32_t>(source->offsetMs * static_cast<double>(found->second.sampleRate) / k_MillisecondsPerSecond);
}

void ClockedAudioPlayer::Advance(Source& source) const
{
	const auto now = _clock();
	// The difference as a signed number, so that a clock that goes back counts as no time
	const auto step = static_cast<int32_t>(now - source.lastTick);
	source.lastTick = now;
	if (source.status != AudioStatus::Playing || step <= 0)
	{
		return;
	}
	source.offsetMs += static_cast<double>(step) * static_cast<double>(source.pitch);
	const auto length = QueueMilliseconds(source);
	if (source.loop)
	{
		source.offsetMs = length > 0.0 ? std::fmod(source.offsetMs, length) : 0.0;
		return;
	}
	if (source.offsetMs >= length)
	{
		source.offsetMs = length;
		source.status = AudioStatus::Stopped;
	}
}

double ClockedAudioPlayer::QueueMilliseconds(const Source& source) const
{
	return std::accumulate(source.queue.begin(), source.queue.end(), 0.0, [this](double sum, BufferId id) {
		const auto found = _buffers.find(id);
		return sum + (found == _buffers.end() ? 0.0 : found->second.Milliseconds());
	});
}

ClockedAudioPlayer::Source* ClockedAudioPlayer::Find(SourceId id) const
{
	const auto found = _sources.find(id);
	return found == _sources.end() ? nullptr : &found->second;
}
