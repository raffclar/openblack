/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Music banks are one MPEG stream cut into chunks. A decoder starting on a chunk gets its first few hundred samples
// wrong, so decoding the chunks one by one leaves a seam between each. These fake chunks hold PCM, and the fake
// decoder silences the start of whatever it decodes the way a cold MPEG decoder does.

#include <cstring>

#include <algorithm>
#include <chrono>
#include <map>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include <Audio/AudioPlayerInterface.h>
#include <Audio/MusicStreamBackend.h>
#include <Audio/SoundDecoder.h>
#include <gtest/gtest.h>

using namespace openblack::audio;

namespace
{
constexpr uint32_t k_ChunkFrames = 1152;
constexpr uint32_t k_ColdFrames = 481;
constexpr int k_SampleRate = 22050;

/// Mono PCM samples of a chunk: a ramp that carries on from one chunk to the next, with no zeros in it
std::vector<uint8_t> MakeChunk(uint32_t index)
{
	std::vector<int16_t> samples(k_ChunkFrames);
	for (uint32_t i = 0; i < k_ChunkFrames; ++i)
	{
		samples[i] = static_cast<int16_t>(1 + ((index * k_ChunkFrames + i) % 30000));
	}
	std::vector<uint8_t> bytes(samples.size() * sizeof(int16_t));
	std::memcpy(bytes.data(), samples.data(), bytes.size());
	return bytes;
}

/// Decodes the PCM of the fake chunks, silencing what a cold decoder would get wrong at the start
DecodeResult ColdDecoder(std::span<const uint8_t> data, int expectedSampleRate)
{
	DecodeResult result;
	DecodedSound sound;
	sound.samples.resize(data.size() / sizeof(int16_t));
	std::memcpy(sound.samples.data(), data.data(), sound.samples.size() * sizeof(int16_t));
	std::fill_n(sound.samples.begin(), std::min<size_t>(k_ColdFrames, sound.samples.size()), int16_t {0});
	sound.channelLayout = ChannelLayout::Mono;
	sound.sampleRate = expectedSampleRate;
	sound.frames = sound.samples.size();
	result.sound = std::move(sound);
	return result;
}

/// A clock that does not move, so that a decode takes no time
std::chrono::steady_clock::time_point StillClock()
{
	return {};
}

/// Keeps what is queued on each source
class RecordingPlayer final: public AudioPlayerInterface
{
public:
	void Initialize() override {}
	void UpdateListener(glm::vec3, glm::vec3, glm::vec3) const override {}
	BufferId CreateBuffer(ChannelLayout, const std::vector<int16_t>& buffer, int) override
	{
		buffers[nextBuffer] = buffer;
		return nextBuffer++;
	}
	void QueueBuffer(SourceId sourceId, BufferId buffer) override
	{
		auto& queued = sources[sourceId];
		queued.insert(queued.end(), buffers[buffer].begin(), buffers[buffer].end());
	}
	void DeleteBuffer(BufferId id) override { buffers.erase(id); }
	SourceId CreateSource(float, bool) override { return nextSource++; }
	void DeleteSource(SourceId) override {}
	void UpdateSource(SourceId, glm::vec3, float, bool) override {}
	void UpdateSource(SourceId, float, bool) override {}
	float GetDuration(BufferId) override { return 0.0f; }
	void PlaySource(SourceId, glm::vec3, float, bool) override {}
	void PlaySource(SourceId, float, bool) override {}
	void PauseSource(SourceId) const override {}
	void StopSource(SourceId) const override {}
	void SetVolume(SourceId, float) override {}
	void SetPitch(SourceId, float) override {}
	void SetPosition(SourceId, glm::vec3) override {}
	void SetDistanceAttenuation(SourceId, float, float, float) override {}
	void SetLooping(SourceId, bool) override {}
	void StartSource(SourceId) override {}
	[[nodiscard]] float GetVolume() const override { return 1.0f; }
	[[nodiscard]] AudioStatus GetStatus(SourceId) const override { return AudioStatus::Playing; }
	[[nodiscard]] float GetProgress(size_t, SourceId) const override { return 0.0f; }
	std::vector<BufferId> UnqueueProcessedBuffers(SourceId) override { return {}; }
	[[nodiscard]] uint32_t GetSampleOffset(SourceId) const override { return 0; }

	std::map<BufferId, std::vector<int16_t>> buffers;
	std::map<SourceId, std::vector<int16_t>> sources;
	BufferId nextBuffer {1};
	SourceId nextSource {1};
};

MusicBank MakeBank(uint32_t chunks)
{
	MusicBank bank;
	bank.path = "fake.sad";
	auto data = std::make_shared<std::vector<std::vector<uint8_t>>>();
	for (uint32_t i = 0; i < chunks; ++i)
	{
		data->push_back(MakeChunk(i));
		bank.chunkSampleRates.push_back(k_SampleRate);
	}
	bank.chunkCount = chunks;
	bank.readChunk = [data](uint32_t chunk) { return data->at(chunk); };
	return bank;
}

/// Chunks first to last as the one stream they were cut from
std::vector<int16_t> DecodeWhole(const MusicBank& bank, const std::vector<uint32_t>& chunks)
{
	std::vector<uint8_t> stream;
	for (const auto chunk : chunks)
	{
		const auto data = bank.ReadChunk(chunk);
		stream.insert(stream.end(), data.begin(), data.end());
	}
	auto decoded = ColdDecoder(stream, k_SampleRate);
	if (!decoded.sound)
	{
		ADD_FAILURE() << "the stream didn't decode";
		return {};
	}
	return decoded.sound->samples;
}

std::vector<int16_t> Queue(const MusicBank& bank, const std::vector<uint32_t>& chunks, uint32_t skipFrames = 0)
{
	RecordingPlayer player;
	MusicStreamBackend backend(player, ColdDecoder, StillClock);
	const auto stream = backend.CreateStream(100);
	bool first = true;
	for (const auto chunk : chunks)
	{
		EXPECT_TRUE(backend.QueueChunk(stream, bank, chunk, first ? skipFrames : 0));
		first = false;
	}
	backend.WaitForDecodes();
	auto queued = player.sources.begin()->second;
	backend.Destroy(stream);
	return queued;
}
} // namespace

TEST(MusicDecoding, ChunksJoinAsTheStreamTheyWereCutFrom)
{
	const auto bank = MakeBank(6);
	const auto queued = Queue(bank, {0, 1, 2, 3, 4, 5});
	EXPECT_EQ(queued, DecodeWhole(bank, {0, 1, 2, 3, 4, 5}));
	// Only the very start of the music is silent
	EXPECT_EQ(std::count(queued.begin(), queued.end(), int16_t {0}), k_ColdFrames);
}

TEST(MusicDecoding, LoopingBackCarriesOnFromTheLastChunk)
{
	const auto bank = MakeBank(3);
	const auto queued = Queue(bank, {0, 1, 2, 0, 1});
	EXPECT_EQ(queued, DecodeWhole(bank, {0, 1, 2, 0, 1}));
}

TEST(MusicDecoding, StartingPartWayThroughCarriesOnFromTheChunkBefore)
{
	const auto bank = MakeBank(20);
	const auto queued = Queue(bank, {10, 11, 12});
	const auto whole = DecodeWhole(bank, {9, 10, 11, 12});
	ASSERT_EQ(queued.size(), whole.size() - k_ChunkFrames);
	EXPECT_TRUE(std::equal(queued.begin(), queued.end(), whole.begin() + k_ChunkFrames));
	EXPECT_EQ(std::count(queued.begin(), queued.end(), int16_t {0}), 0);
}

TEST(MusicDecoding, StartingInTimeLeavesOutWhatHasPlayed)
{
	const auto bank = MakeBank(20);
	const auto queued = Queue(bank, {10, 11}, 300);
	const auto whole = DecodeWhole(bank, {9, 10, 11});
	ASSERT_EQ(queued.size(), whole.size() - k_ChunkFrames - 300);
	EXPECT_TRUE(std::equal(queued.begin(), queued.end(), whole.begin() + k_ChunkFrames + 300));
}

TEST(MusicDecoding, ChunksAreQueuedInSequenceWhenTheyAreDecoded)
{
	const auto bank = MakeBank(6);
	RecordingPlayer player;
	MusicStreamBackend backend(player, ColdDecoder, StillClock);
	const auto stream = backend.CreateStream(100);
	for (uint32_t chunk = 0; chunk < 4; ++chunk)
	{
		EXPECT_TRUE(backend.QueueChunk(stream, bank, chunk, 0));
	}
	// Other threads decode the chunks: none is on the source before the stream gets them
	EXPECT_TRUE(player.buffers.empty());
	backend.WaitForDecodes();
	EXPECT_EQ(player.buffers.size(), 4u);
	EXPECT_EQ(player.sources.begin()->second, DecodeWhole(bank, {0, 1, 2, 3}));
	// A chunk queued after the others were decoded continues from them
	EXPECT_TRUE(backend.QueueChunk(stream, bank, 4, 0));
	backend.WaitForDecodes();
	EXPECT_EQ(player.sources.begin()->second, DecodeWhole(bank, {0, 1, 2, 3, 4}));
	backend.Destroy(stream);
}

TEST(MusicDecoding, StartingInTimeAlsoLeavesOutTheTimeOfTheDecode)
{
	const auto bank = MakeBank(20);
	RecordingPlayer player;
	// Each look at the clock is ten milliseconds after the one before
	auto now = std::chrono::steady_clock::time_point {};
	MusicStreamBackend backend(player, ColdDecoder, [&now]() {
		now += std::chrono::milliseconds(10);
		return now;
	});
	const auto stream = backend.CreateStream(100);
	EXPECT_TRUE(backend.QueueChunk(stream, bank, 10, 300));
	backend.WaitForDecodes();
	// The other channel played 10 ms more, 220 frames at 22050 frames a second
	const auto whole = DecodeWhole(bank, {9, 10});
	const auto& queued = player.sources.begin()->second;
	constexpr size_t k_Waited = 220;
	ASSERT_EQ(queued.size(), whole.size() - k_ChunkFrames - 300 - k_Waited);
	EXPECT_TRUE(std::equal(queued.begin(), queued.end(), whole.begin() + k_ChunkFrames + 300 + k_Waited));
	backend.Destroy(stream);
}

TEST(MusicDecoding, AChunkThatCannotBeDecodedCountsAsPlayed)
{
	auto bank = MakeBank(3);
	bank.readChunk = [good = bank.readChunk](uint32_t chunk) { return chunk == 1 ? std::vector<uint8_t> {} : good(chunk); };
	RecordingPlayer player;
	const auto decoder = [](std::span<const uint8_t> data, int sampleRate) {
		return data.empty() ? DecodeResult {} : ColdDecoder(data, sampleRate);
	};
	MusicStreamBackend backend(player, decoder, StillClock);
	const auto stream = backend.CreateStream(100);
	EXPECT_TRUE(backend.QueueChunk(stream, bank, 0, 0));
	EXPECT_TRUE(backend.QueueChunk(stream, bank, 1, 0));
	backend.WaitForDecodes();
	// The player hears of the chunk that failed as of a played one
	EXPECT_EQ(backend.ReleasePlayedChunks(stream), 1u);
	EXPECT_EQ(backend.ReleasePlayedChunks(stream), 0u);
	backend.Destroy(stream);
}
