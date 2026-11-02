/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The custom music player decodes its MP3 tracks on other threads. These tests play a track of silent MPEG frames on a
// fake audio device, which plays each buffer as soon as the player queues it.

#include <cstdint>

#include <chrono>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <numeric>
#include <thread>
#include <vector>

#include <Audio/AudioPlayerInterface.h>
#include <Audio/CustomMusicPlayer.h>
#include <gtest/gtest.h>

using namespace openblack;
using namespace openblack::audio;

namespace
{
constexpr size_t k_MpegFrames = 50;
constexpr uint64_t k_MpegFrameSamples = 576;
constexpr uint32_t k_SampleRate = 22050;
/// The frames of a part: a quarter of a second
constexpr uint64_t k_PartFrames = k_SampleRate / 4;
/// The parts that the player keeps queued: a second
constexpr size_t k_QueuedParts = 4;

/// Mono MPEG-2 layer III frames of silence at 22050 Hz
std::vector<uint8_t> SilentTrack()
{
	constexpr size_t k_FrameBytes = 104;
	std::vector<uint8_t> data(k_MpegFrames * k_FrameBytes, 0);
	for (size_t i = 0; i < k_MpegFrames; ++i)
	{
		data[i * k_FrameBytes + 0] = 0xFF;
		data[i * k_FrameBytes + 1] = 0xF3;
		data[i * k_FrameBytes + 2] = 0x40;
		data[i * k_FrameBytes + 3] = 0xC0;
	}
	return data;
}

/// Keeps the buffers queued on each source. A source that plays gives back all of its buffers as played on each look.
class FakeDevice final: public AudioPlayerInterface
{
public:
	void Initialize() override {}
	void UpdateListener(glm::vec3, glm::vec3, glm::vec3) const override {}
	BufferId CreateBuffer(ChannelLayout, const std::vector<int16_t>& buffer, int) override
	{
		buffers[nextBuffer] = buffer;
		created.push_back(buffer.size());
		return nextBuffer++;
	}
	void QueueBuffer(SourceId sourceId, BufferId buffer) override { queued[sourceId].push_back(buffer); }
	void DeleteBuffer(BufferId id) override { buffers.erase(id); }
	SourceId CreateSource(float, bool) override
	{
		++sourcesMade;
		return nextSource++;
	}
	void DeleteSource(SourceId) override {}
	void UpdateSource(SourceId, glm::vec3, float, bool) override {}
	void UpdateSource(SourceId, float, bool) override {}
	float GetDuration(BufferId) override { return 0.0f; }
	void PlaySource(SourceId, glm::vec3, float, bool) override {}
	void PlaySource(SourceId, float, bool) override {}
	void PauseSource(SourceId) const override {}
	void StopSource(SourceId) const override { stopped = true; }
	void SetVolume(SourceId, float) override {}
	void SetPitch(SourceId, float) override {}
	void SetPosition(SourceId, glm::vec3) override {}
	void SetDistanceAttenuation(SourceId, float, float, float) override {}
	void SetLooping(SourceId, bool) override {}
	void StartSource(SourceId) override {}
	[[nodiscard]] float GetVolume() const override { return 1.0f; }
	[[nodiscard]] AudioStatus GetStatus(SourceId) const override { return AudioStatus::Playing; }
	[[nodiscard]] float GetProgress(size_t, SourceId) const override { return 0.0f; }
	std::vector<BufferId> UnqueueProcessedBuffers(SourceId sourceId) override
	{
		// A stopped source has played all of its buffers
		if (!playing && !stopped)
		{
			return {};
		}
		stopped = false;
		auto& source = queued[sourceId];
		std::vector<BufferId> played(source.begin(), source.end());
		source.clear();
		return played;
	}
	[[nodiscard]] uint32_t GetSampleOffset(SourceId) const override { return 0; }

	std::map<BufferId, std::vector<int16_t>> buffers;
	std::map<SourceId, std::deque<BufferId>> queued;
	/// The samples of each buffer in the sequence that the player made them
	std::vector<size_t> created;
	int sourcesMade {0};
	bool playing {false};
	mutable bool stopped {false};
	BufferId nextBuffer {1};
	SourceId nextSource {1};
};

std::unique_ptr<CustomMusicPlayer> MakePlayer(FakeDevice& device)
{
	auto player = std::make_unique<CustomMusicPlayer>(
	    device, [](const std::filesystem::path&) { return SilentTrack(); }, [](uint32_t) { return 0u; });
	const std::vector<std::filesystem::path> files {"Tone.mp3"};
	player->SetFiles(files);
	return player;
}

/// Updates the player until the condition is true, or a few seconds pass
bool UpdateUntil(CustomMusicPlayer& player, const std::function<bool()>& condition)
{
	const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
	while (std::chrono::steady_clock::now() < end)
	{
		player.Update();
		if (condition())
		{
			return true;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return false;
}

/// The frames of all of the track, as the player gives them when it plays the track from its start to its end
uint64_t PlayToTheEnd(CustomMusicPlayer& player, FakeDevice& device, size_t firstPart = 0)
{
	device.playing = true;
	EXPECT_TRUE(UpdateUntil(player, [&player]() { return player.GetPlayState() == CustomMusicPlayer::PlayState::Stopped; }));
	return std::accumulate(device.created.begin() + static_cast<std::ptrdiff_t>(firstPart), device.created.end(), uint64_t {0});
}
} // namespace

TEST(CustomMusicDecode, TheFirstSecondIsQueuedWhenTheTrackOpens)
{
	FakeDevice device;
	auto player = MakePlayer(device);
	player->Play();
	ASSERT_TRUE(UpdateUntil(*player, [&device]() { return device.sourcesMade > 0; }));
	// The update that makes the source also queues the first second, as when the player decoded it in that update
	ASSERT_EQ(device.created.size(), k_QueuedParts);
	for (const auto samples : device.created)
	{
		EXPECT_EQ(samples, k_PartFrames);
	}
}

TEST(CustomMusicDecode, TheTrackPlaysInQuarterSecondsToItsEnd)
{
	FakeDevice device;
	auto player = MakePlayer(device);
	player->Play();
	const auto total = PlayToTheEnd(*player, device);
	ASSERT_GT(device.created.size(), k_QueuedParts);
	for (size_t i = 0; i + 1 < device.created.size(); ++i)
	{
		EXPECT_EQ(device.created[i], k_PartFrames) << "part " << i;
	}
	EXPECT_LE(device.created.back(), k_PartFrames);
	EXPECT_GT(device.created.back(), 0u);
	// The frames played are the frames of the track, which are silent
	EXPECT_EQ(total, k_MpegFrames * k_MpegFrameSamples);
	for (const auto& [id, samples] : device.buffers)
	{
		for (const auto sample : samples)
		{
			ASSERT_EQ(sample, 0);
		}
	}
}

TEST(CustomMusicDecode, AMoveOfThePositionDecodesFromTheNewFrame)
{
	FakeDevice whole;
	auto reference = MakePlayer(whole);
	reference->Play();
	const auto total = PlayToTheEnd(*reference, whole);

	FakeDevice device;
	auto player = MakePlayer(device);
	player->Play();
	ASSERT_TRUE(UpdateUntil(*player, [&device]() { return device.created.size() == k_QueuedParts; }));
	constexpr uint32_t k_MoveMilliseconds = 500;
	constexpr uint64_t k_MoveFrame = k_SampleRate * k_MoveMilliseconds / 1000;
	player->SetPosition(k_MoveMilliseconds);
	ASSERT_TRUE(UpdateUntil(*player, [&device]() { return device.created.size() == 2 * k_QueuedParts; }));
	EXPECT_EQ(player->GetPosition(), k_MoveMilliseconds);
	// From the move to the end the player gives the frames of the track after the new frame
	const auto afterMove = PlayToTheEnd(*player, device, k_QueuedParts);
	EXPECT_EQ(afterMove, total - k_MoveFrame);
}

TEST(CustomMusicDecode, AStoppedTrackLeavesItsDecodesToEndOnTheirOwn)
{
	FakeDevice device;
	auto player = MakePlayer(device);
	player->Play();
	device.playing = true;
	ASSERT_TRUE(UpdateUntil(*player, [&device]() { return device.created.size() > k_QueuedParts; }));
	player->Stop();
	const auto made = device.created.size();
	for (int i = 0; i < 20; ++i)
	{
		player->Update();
	}
	EXPECT_EQ(device.created.size(), made);
	EXPECT_EQ(player->GetPlayState(), CustomMusicPlayer::PlayState::Stopped);
}
