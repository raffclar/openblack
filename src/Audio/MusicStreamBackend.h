/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "AudioPlayerInterface.h"
#include "MusicPlayer.h"
#include "SoundDecoder.h"

namespace openblack::audio
{

/// Streams music channels with OpenAL: each stream is a source centred on the listener with the decoded chunks of
/// its bank queued on it. Other threads decode the chunks in their sequence, and the stream gets them as they are
/// ready. Thus a stream starts to play a short time after its first chunk is queued.
class MusicStreamBackend final: public MusicBackend
{
public:
	/// Decodes a chunk of a bank, or chunks joined end to end, given the bank header's sample rate
	using Decoder = std::function<DecodeResult(std::span<const uint8_t> data, int expectedSampleRate)>;

	/// The time now, which tells how long the decode of a chunk took
	using Clock = std::function<std::chrono::steady_clock::time_point()>;

	explicit MusicStreamBackend(AudioPlayerInterface& player, Decoder decoder = DecodeSound,
	                            Clock clock = std::chrono::steady_clock::now);
	~MusicStreamBackend() override;
	MusicStreamBackend(const MusicStreamBackend&) = delete;
	MusicStreamBackend& operator=(const MusicStreamBackend&) = delete;

	[[nodiscard]] Stream CreateStream(int32_t pitchPercent) override;
	bool QueueChunk(Stream stream, const MusicBank& bank, uint32_t chunk, uint32_t skipFrames) override;
	[[nodiscard]] uint32_t ReleasePlayedChunks(Stream stream) override;
	[[nodiscard]] uint32_t GetPlayPosition(Stream stream) const override;
	[[nodiscard]] uint64_t GetFramesPlayed(Stream stream) const override;
	void Play(Stream stream) override;
	[[nodiscard]] bool IsPlaying(Stream stream) const override;
	void SetVolume(Stream stream, float volume) override;
	void SetPlacement(Stream stream, const std::optional<MusicPlacement>& placement) override;
	void Destroy(Stream stream) override;

	/// Waits for the chunks that other threads decode, and queues them on their streams
	void WaitForDecodes();
	/// The music and master volumes, applied on top of each stream's own
	void SetOutputVolume(float volume);
	/// Each placed stream's source and placement, for positioning it relative to the listener as the camera moves
	void ForEachPlaced(const std::function<void(SourceId source, const MusicPlacement& placement)>& place) const;

private:
	/// Reads a chunk's data from its bank
	using ChunkReader = std::function<std::vector<uint8_t>(uint32_t chunk)>;
	/// A chunk decoded after the chunk that plays before it
	struct DecodedChunk
	{
		/// The frames that the chunk plays; none if it could not be decoded
		std::optional<DecodedSound> sound;
		/// The chunk decoded and its length in frames, which the next chunk is decoded after; none after a failure
		std::optional<uint32_t> chunk;
		uint64_t chunkFrames {0};
		/// The frames left out at the start, and when the chunk was queued: a chunk that starts in time with another
		/// channel also leaves out the time that the decode took
		uint32_t skipFrames {0};
		std::chrono::steady_clock::time_point requested;
		/// The number of the chunk in its bank, the bank's path and why the decode failed, for the log
		uint32_t number {0};
		std::string bank;
		std::string error;
	};

	struct StreamState
	{
		SourceId source;
		/// The speed that the source plays at, 1 for the music's own speed
		float rate {1.0f};
		std::deque<BufferId> buffers;
		/// Frames in each queued buffer, and in all the buffers already played and taken off
		std::deque<uint64_t> bufferFrames;
		uint64_t releasedFrames {0};
		float volume {0.0f};
		std::optional<MusicPlacement> placement;
		/// The chunk queued last and its length in frames, which the next chunk is decoded after
		std::optional<uint32_t> lastChunk;
		uint64_t lastChunkFrames {0};
		/// The chunks that other threads decode, in the sequence that they play
		std::deque<std::shared_future<DecodedChunk>> decoding;
		/// The chunks that other threads could not decode, which count as played
		uint32_t failed {0};
	};

	/// Decodes a chunk after the chunk before it, and leaves out the frames to skip
	[[nodiscard]] static DecodedChunk Decode(const Decoder& decoder, const ChunkReader& read, int sampleRate,
	                                         std::optional<uint32_t> before, uint64_t beforeFrames, uint32_t chunk,
	                                         uint32_t skipFrames);
	/// Queues a decoded chunk on the stream's source; false if it has nothing to play
	bool QueueDecoded(StreamState& state, DecodedChunk decoded);
	/// Queues the chunks that other threads finished, in their sequence
	void QueueFinishedDecodes(StreamState& state);

	AudioPlayerInterface& _player;
	Decoder _decoder;
	Clock _clock;
	std::map<Stream, StreamState> _streams;
	/// The decodes of the streams that were destroyed, kept until they end so that no frame waits for them
	std::vector<std::shared_future<DecodedChunk>> _abandoned;
	Stream _nextStream {1};
	float _outputVolume {1.0f};
};

} // namespace openblack::audio
