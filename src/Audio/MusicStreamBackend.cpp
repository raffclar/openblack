/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MusicStreamBackend.h"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "SoundDecoder.h"

using namespace openblack::audio;

namespace
{
constexpr float k_PercentToRate = 0.01f;
} // namespace

MusicStreamBackend::MusicStreamBackend(AudioPlayerInterface& player, Decoder decoder, Clock clock)
    : _player(player)
    , _decoder(std::move(decoder))
    , _clock(std::move(clock))
{
}

MusicStreamBackend::~MusicStreamBackend()
{
	while (!_streams.empty())
	{
		Destroy(_streams.begin()->first);
	}
}

MusicBackend::Stream MusicStreamBackend::CreateStream(int32_t pitchPercent)
{
	const auto rate = static_cast<float>(pitchPercent) * k_PercentToRate;
	const auto source = _player.CreateSource(rate, true);
	const auto stream = _nextStream++;
	_streams.emplace(stream, StreamState {.source = source, .rate = rate});
	return stream;
}

// A bank's chunks are cut from one MPEG stream at frame boundaries. A decoder starting on a chunk has nothing in its
// synthesis filter, so the first few hundred samples it gives are wrong, mostly silence, which clicks against the end
// of the chunk before. Decoding the chunk after the one that plays before it, and dropping that one's samples, gives
// exactly what decoding the whole stream would.
MusicStreamBackend::DecodedChunk MusicStreamBackend::Decode(const Decoder& decoder, const ChunkReader& read, int sampleRate,
                                                            std::optional<uint32_t> before, uint64_t beforeFrames,
                                                            uint32_t chunk, uint32_t skipFrames)
{
	const auto decode = [&decoder, &read, sampleRate](std::optional<uint32_t> first, uint32_t second) {
		if (!first)
		{
			const auto data = read(second);
			return decoder(std::span<const uint8_t>(data.data(), data.size()), sampleRate);
		}
		auto data = read(*first);
		const auto more = read(second);
		data.insert(data.end(), more.begin(), more.end());
		return decoder(std::span<const uint8_t>(data.data(), data.size()), sampleRate);
	};

	// What plays before: the chunk queued last, or the one before in the bank when starting part way through
	if (!before && chunk > 0)
	{
		if (const auto alone = decode(std::nullopt, chunk - 1); alone.sound)
		{
			before = chunk - 1;
			beforeFrames = alone.sound->frames;
		}
	}

	auto result = decode(before, chunk);
	if (result.sound && before && result.sound->frames <= beforeFrames)
	{
		result.sound.reset();
	}
	if (!result.sound)
	{
		// Fall back on the chunk by itself
		before.reset();
		beforeFrames = 0;
		result = decode(std::nullopt, chunk);
	}
	DecodedChunk decoded {.number = chunk};
	if (!result.sound)
	{
		decoded.error = result.error;
		return decoded;
	}
	auto& sound = *result.sound;
	decoded.chunk = chunk;
	decoded.chunkFrames = sound.frames - beforeFrames;

	// Leave out the chunk decoded first, and when starting in time with another channel, the part of this one it has
	// played already
	const auto channels = sound.channelLayout == ChannelLayout::Stereo ? 2u : 1u;
	const auto skipSamples = std::min<size_t>(static_cast<size_t>(beforeFrames + skipFrames) * channels, sound.samples.size());
	sound.samples.erase(sound.samples.begin(), sound.samples.begin() + static_cast<std::ptrdiff_t>(skipSamples));
	decoded.sound = std::move(sound);
	return decoded;
}

bool MusicStreamBackend::QueueDecoded(StreamState& state, DecodedChunk decoded)
{
	state.lastChunk = decoded.chunk;
	state.lastChunkFrames = decoded.chunkFrames;
	if (!decoded.sound)
	{
		if (const auto logger = spdlog::get("audio"); logger != nullptr)
		{
			SPDLOG_LOGGER_WARN(logger, "Could not decode chunk {} of {}: {}", decoded.number, decoded.bank, decoded.error);
		}
		return false;
	}
	auto& sound = *decoded.sound;
	const auto channels = sound.channelLayout == ChannelLayout::Stereo ? 2u : 1u;
	if (decoded.skipFrames > 0)
	{
		// A chunk that starts in time with another channel also leaves out what the other channel played during the
		// decode
		const auto waited = std::chrono::duration<float>(_clock() - decoded.requested).count();
		const auto frames = static_cast<size_t>(waited * static_cast<float>(sound.sampleRate) * state.rate);
		const auto samples = std::min(frames * channels, sound.samples.size());
		sound.samples.erase(sound.samples.begin(), sound.samples.begin() + static_cast<std::ptrdiff_t>(samples));
	}
	if (sound.samples.empty())
	{
		return false;
	}
	const auto buffer = _player.CreateBuffer(sound.channelLayout, sound.samples, sound.sampleRate);
	_player.QueueBuffer(state.source, buffer);
	state.buffers.push_back(buffer);
	state.bufferFrames.push_back(sound.samples.size() / channels);
	return true;
}

void MusicStreamBackend::QueueFinishedDecodes(StreamState& state)
{
	while (!state.decoding.empty() && state.decoding.front().wait_for(std::chrono::seconds(0)) == std::future_status::ready)
	{
		auto decoded = state.decoding.front().get();
		state.decoding.pop_front();
		if (!QueueDecoded(state, std::move(decoded)))
		{
			++state.failed;
		}
	}
}

bool MusicStreamBackend::QueueChunk(Stream stream, const MusicBank& bank, uint32_t chunk, uint32_t skipFrames)
{
	const auto found = _streams.find(stream);
	if (found == _streams.end() || chunk >= bank.GetChunkCount())
	{
		return false;
	}
	auto& state = found->second;
	const auto sampleRate = chunk < bank.chunkSampleRates.size() ? static_cast<int>(bank.chunkSampleRates[chunk]) : 0;
	// The reader keeps its own copy of how to read the bank: the bank can go before the decode ends
	ChunkReader read = [readChunk = bank.readChunk, count = bank.GetChunkCount()](uint32_t index) {
		return readChunk && index < count ? readChunk(index) : std::vector<uint8_t> {};
	};

	// Another thread decodes the chunk after the chunk before it, so that a frame of the game does not wait for the
	// decode. The chunk plays after the chunks queued before it.
	const auto requested = _clock();
	std::shared_future<DecodedChunk> previous;
	if (!state.decoding.empty())
	{
		previous = state.decoding.back();
	}
	auto job = [decoder = _decoder, read = std::move(read), sampleRate, previous, before = state.lastChunk,
	            beforeFrames = state.lastChunkFrames, chunk, skipFrames, path = bank.path, requested]() mutable {
		if (previous.valid())
		{
			const auto& last = previous.get();
			before = last.chunk;
			beforeFrames = last.chunkFrames;
		}
		auto decoded = Decode(decoder, read, sampleRate, before, beforeFrames, chunk, skipFrames);
		decoded.bank = std::move(path);
		decoded.skipFrames = skipFrames;
		decoded.requested = requested;
		return decoded;
	};
	state.decoding.push_back(std::async(std::launch::async, std::move(job)).share());
	return true;
}

uint32_t MusicStreamBackend::ReleasePlayedChunks(Stream stream)
{
	const auto found = _streams.find(stream);
	if (found == _streams.end())
	{
		return 0;
	}
	auto& state = found->second;
	QueueFinishedDecodes(state);
	std::erase_if(_abandoned, [](const std::shared_future<DecodedChunk>& decode) {
		return decode.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
	});
	const auto played = _player.UnqueueProcessedBuffers(state.source);
	for (const auto buffer : played)
	{
		_player.DeleteBuffer(buffer);
		if (!state.buffers.empty())
		{
			state.buffers.pop_front();
			state.releasedFrames += state.bufferFrames.front();
			state.bufferFrames.pop_front();
		}
	}
	// A chunk that could not be decoded counts as played, so that the player stops to wait for it
	const auto failed = std::exchange(state.failed, 0u);
	return static_cast<uint32_t>(played.size()) + failed;
}

uint32_t MusicStreamBackend::GetPlayPosition(Stream stream) const
{
	const auto found = _streams.find(stream);
	return found != _streams.end() ? _player.GetSampleOffset(found->second.source) : 0;
}

uint64_t MusicStreamBackend::GetFramesPlayed(Stream stream) const
{
	const auto found = _streams.find(stream);
	if (found == _streams.end())
	{
		return 0;
	}
	const auto& state = found->second;
	switch (_player.GetStatus(state.source))
	{
	case AudioStatus::Playing:
	case AudioStatus::Paused:
		// The offset counts from the first buffer still on the source
		return state.releasedFrames + _player.GetSampleOffset(state.source);
	case AudioStatus::Stopped:
		// Run out: every buffer on it has played
		return std::accumulate(state.bufferFrames.begin(), state.bufferFrames.end(), state.releasedFrames);
	case AudioStatus::Initial:
		break;
	}
	return state.releasedFrames;
}

void MusicStreamBackend::Play(Stream stream)
{
	const auto found = _streams.find(stream);
	if (found != _streams.end() && !found->second.buffers.empty())
	{
		_player.SetVolume(found->second.source, found->second.volume * _outputVolume);
		_player.StartSource(found->second.source);
	}
}

bool MusicStreamBackend::IsPlaying(Stream stream) const
{
	const auto found = _streams.find(stream);
	return found != _streams.end() && _player.GetStatus(found->second.source) == AudioStatus::Playing;
}

void MusicStreamBackend::SetVolume(Stream stream, float volume)
{
	const auto found = _streams.find(stream);
	if (found != _streams.end())
	{
		found->second.volume = volume;
		_player.SetVolume(found->second.source, volume * _outputVolume);
	}
}

void MusicStreamBackend::SetPlacement(Stream stream, const std::optional<MusicPlacement>& placement)
{
	const auto found = _streams.find(stream);
	if (found == _streams.end())
	{
		return;
	}
	found->second.placement = placement;
	if (placement)
	{
		// QSound's distance mapping is OpenAL's inverse clamped model on positions stretched by the distance scale
		_player.SetDistanceAttenuation(found->second.source, placement->minDistance, placement->maxDistance, 1.0f);
	}
	else
	{
		_player.SetDistanceAttenuation(found->second.source, 1.0f, 1.0f, 0.0f);
		_player.SetPosition(found->second.source, glm::vec3(0.0f));
	}
}

void MusicStreamBackend::ForEachPlaced(const std::function<void(SourceId source, const MusicPlacement& placement)>& place) const
{
	for (const auto& [stream, state] : _streams)
	{
		if (state.placement)
		{
			place(state.source, *state.placement);
		}
	}
}

void MusicStreamBackend::Destroy(Stream stream)
{
	const auto found = _streams.find(stream);
	if (found == _streams.end())
	{
		return;
	}
	auto& state = found->second;
	// The decodes still running end on their own threads; the frame does not wait for them
	std::ranges::move(state.decoding, std::back_inserter(_abandoned));
	// A stopped source has played all of its buffers, so they can all be taken off it
	_player.StopSource(state.source);
	for (const auto buffer : _player.UnqueueProcessedBuffers(state.source))
	{
		_player.DeleteBuffer(buffer);
	}
	_player.DeleteSource(state.source);
	_streams.erase(found);
}

void MusicStreamBackend::WaitForDecodes()
{
	for (auto& [stream, state] : _streams)
	{
		for (const auto& decode : state.decoding)
		{
			decode.wait();
		}
		QueueFinishedDecodes(state);
	}
}

void MusicStreamBackend::SetOutputVolume(float volume)
{
	if (volume == _outputVolume)
	{
		return;
	}
	_outputVolume = volume;
	for (const auto& [stream, state] : _streams)
	{
		_player.SetVolume(state.source, state.volume * _outputVolume);
	}
}
