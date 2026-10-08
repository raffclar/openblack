/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstring>

#include <array>
#include <deque>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "Audio/Engine/MusicBank.h"
#include "Audio/Engine/MusicEngine.h"
#include "support/TestServices.h"

// MusicEngine over a fake mixer. Expected values follow the original music player: +4 / -3 per pass, entry
// without fade, sync on the same chunk and sample, 6 channels, main channel -1 at the end. The banks are small .sad files
// written here with the music bank layout (no game data needed).

using namespace openblack::audio;

namespace
{
/// MusicBank::Register reads through the file system: a DefaultFileSystem for the call, the one before put back
/// after it (the bank keeps its own open file)
std::unique_ptr<MusicBank> RegisterMusicBank(const std::filesystem::path& path)
{
	const openblack::test::ScopedDefaultFileSystem fileSystem;
	return MusicBank::Register(path);
}

/// The running test's suite and name and the process id: parallel test processes never share a temporary file
std::string UniqueSuffix()
{
	const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
#ifdef _WIN32
	const auto pid = _getpid();
#else
	const auto pid = getpid();
#endif
	return std::string(info->test_suite_name()) + "_" + info->name() + "_" + std::to_string(pid);
}

struct BankSpec
{
	int segments {8};
	int group {0};
	uint32_t flags {0};
	int loops {0};
	int volume {127};
	std::vector<std::string> descriptions; // the description text of each segment (markers)
	int badOffsetSegment {-1};             // a segment whose offset points past LHAudioWaveData
};

void WriteBlock(std::ofstream& out, const char* name, const std::vector<uint8_t>& data)
{
	std::array<char, 32> blockName {};
	std::strncpy(blockName.data(), name, blockName.size() - 1);
	out.write(blockName.data(), blockName.size());
	const auto size = static_cast<uint32_t>(data.size());
	out.write(reinterpret_cast<const char*>(&size), sizeof(size));
	out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

void Put32(std::vector<uint8_t>& v, size_t offset, uint32_t value)
{
	std::memcpy(&v[offset], &value, sizeof(value));
}

// LiOnHeAd with LHFileSegmentBankInfo (3rd u32 = 1: music), LHAudioWaveData (4 bytes per segment: its index) and
// LHAudioBankSampleTable (n records of MusicBank::k_RecordSize)
std::filesystem::path WriteBank(const std::string& name, const BankSpec& spec)
{
	const auto path = std::filesystem::path(TEST_BINARY_DIR) / ("music_engine_" + name + "_" + UniqueSuffix() + ".sad");
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write("LiOnHeAd", 8);
	WriteBlock(out, "LHFileSegmentBankInfo", {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0});
	std::vector<uint8_t> wave(static_cast<size_t>(spec.segments) * 4);
	for (int i = 0; i < spec.segments; ++i)
	{
		Put32(wave, static_cast<size_t>(i) * 4, static_cast<uint32_t>(i));
	}
	WriteBlock(out, "LHAudioWaveData", wave);
	std::vector<uint8_t> table(4 + static_cast<size_t>(spec.segments) * MusicBank::k_RecordSize);
	Put32(table, 0, static_cast<uint32_t>(spec.segments));
	for (int i = 0; i < spec.segments; ++i)
	{
		const size_t r = 4 + static_cast<size_t>(i) * MusicBank::k_RecordSize;
		Put32(table, r + 0x10C, 4);
		Put32(table, r + 0x110, i == spec.badOffsetSegment ? 0x100000 : static_cast<uint32_t>(i) * 4);
		Put32(table, r + 0x128, 22050);
		if (static_cast<size_t>(i) < spec.descriptions.size())
		{
			std::memcpy(&table[r + 0x140], spec.descriptions[static_cast<size_t>(i)].c_str(),
			            spec.descriptions[static_cast<size_t>(i)].size());
		}
		if (i == 0)
		{
			Put32(table, r + 0x118, static_cast<uint32_t>(spec.group));
			Put32(table, r + 0x244, spec.flags);
			Put32(table, r + 0x248, static_cast<uint32_t>(spec.loops));
			Put32(table, r + 0x25C, static_cast<uint32_t>(spec.volume));
		}
	}
	WriteBlock(out, "LHAudioBankSampleTable", table);
	return path;
}

// the mixer without sound: the queue of each channel (the segment index is the data), volumes, flushes
class FakeSink final: public IMusicSink
{
public:
	struct Chunk
	{
		uint32_t segment;
		uint32_t startSample;
		bool last;
		bool reset;
	};

	void SetListener(IMusicChunkListener* listener) override { _listener = listener; }
	void EnableChannel(int channel, bool is3D, const MusicDistanceMapping& mapping, glm::vec3) override
	{
		enabled3D[static_cast<size_t>(channel)] = is3D;
		mappings[static_cast<size_t>(channel)] = mapping;
	}
	void QueueChunk(int channel, const std::vector<uint8_t>& segment, bool resetDecoder, uint32_t startSample,
	                bool last) override
	{
		uint32_t index = 0;
		std::memcpy(&index, segment.data(), sizeof(index));
		queues[static_cast<size_t>(channel)].push_back({index, startSample, last, resetDecoder});
		history[static_cast<size_t>(channel)].push_back({index, startSample, last, resetDecoder});
	}
	void SetCentred(int) override {}
	void SetFrequency(int channel, uint32_t hz) override { frequency[static_cast<size_t>(channel)] = hz; }
	void SetVolume(int channel, uint32_t volume) override { volumes[static_cast<size_t>(channel)] = volume; }
	bool IsChannelDone(int channel) override { return queues[static_cast<size_t>(channel)].empty(); }
	void FlushChannel(int channel) override
	{
		++flushes[static_cast<size_t>(channel)];
		auto flushed = std::move(queues[static_cast<size_t>(channel)]);
		queues[static_cast<size_t>(channel)].clear();
		for (const auto& chunk : flushed)
		{
			_listener->OnChunkDone(channel, chunk.last);
		}
	}
	uint32_t GetPlayPosition(int channel) override { return playPosition[static_cast<size_t>(channel)]; }
	void SetSourcePosition(int, glm::vec3) override {}
	void PauseChannel(int channel) override { paused[static_cast<size_t>(channel)] = true; }
	void RestartChannel(int channel) override { paused[static_cast<size_t>(channel)] = false; }

	/// The oldest queued chunk of a channel has played
	void PlayOne(int channel)
	{
		auto& queue = queues[static_cast<size_t>(channel)];
		ASSERT_FALSE(queue.empty());
		const bool last = queue.front().last;
		queue.pop_front();
		_listener->OnChunkDone(channel, last);
	}

	std::array<std::deque<Chunk>, k_MusicChannelCount> queues;
	std::array<std::vector<Chunk>, k_MusicChannelCount> history;
	std::array<uint32_t, k_MusicChannelCount> volumes {};
	std::array<uint32_t, k_MusicChannelCount> frequency {};
	std::array<uint32_t, k_MusicChannelCount> playPosition {};
	std::array<int, k_MusicChannelCount> flushes {};
	std::array<bool, k_MusicChannelCount> paused {};
	std::array<bool, k_MusicChannelCount> enabled3D {};
	std::array<MusicDistanceMapping, k_MusicChannelCount> mappings {};

private:
	IMusicChunkListener* _listener {nullptr};
};

class MusicEngineTest: public ::testing::Test
{
protected:
	std::unique_ptr<MusicBank> Bank(const std::string& name, const BankSpec& spec = {})
	{
		_files.push_back(WriteBank(name, spec));
		auto bank = RegisterMusicBank(_files.back());
		EXPECT_NE(bank, nullptr);
		return bank;
	}
	// the banks of a test are gone (and their files closed) when its body ends
	void TearDown() override
	{
		for (const auto& file : _files)
		{
			std::error_code ec;
			std::filesystem::remove(file, ec);
		}
	}
	MusicPlayOptions Options(MusicBank* bank, int volume, int fade, int sync = 0)
	{
		MusicPlayOptions options;
		options.bank = bank;
		options.volume = volume;
		options.fade = fade;
		options.sync = sync;
		return options;
	}
	/// One pass of the thread, 120 ms after the previous one
	bool Pass()
	{
		_now += k_MusicPassSleepMs;
		return engine.Process(_now);
	}

	FakeSink sink;
	MusicEngine engine {sink};
	uint32_t _now {1000};
	std::vector<std::filesystem::path> _files;
};
} // namespace

TEST_F(MusicEngineTest, InitialState)
{
	// main volume 127, no main channel, all free and installed/active
	EXPECT_TRUE(engine.IsInstalled());
	EXPECT_TRUE(engine.IsActive());
	EXPECT_EQ(engine.GetMainVolume(), 127);
	EXPECT_EQ(engine.GetMainChannel(), k_NoMusicChannel);
	for (int i = 0; i < k_MusicChannelCount; ++i)
	{
		EXPECT_EQ(engine.GetStatus(i), MusicStatus::Free);
		EXPECT_FALSE(sink.enabled3D[static_cast<size_t>(i)]);
	}
}

TEST_F(MusicEngineTest, PlayOptionsDefaults)
{
	const MusicPlayOptions options;
	EXPECT_EQ(options.bank, nullptr);
	EXPECT_EQ(options.volume, 127);
	EXPECT_EQ(options.startChunk, 1);
	EXPECT_EQ(options.loops, 0);
	EXPECT_EQ(options.sync, 0);
	EXPECT_EQ(options.fade, 0);
	EXPECT_EQ(options.is3D, 0);
	EXPECT_EQ(options.pitch, 100);
	EXPECT_EQ(options.distance.minDistance, 10.0f);
	EXPECT_EQ(options.distance.maxDistance, 100.0f);
	EXPECT_EQ(options.distance.scale, 1.0f);
}

TEST_F(MusicEngineTest, FadeInTakes32Passes)
{
	auto bank = Bank("fadein");
	const int ch = engine.Play(Options(bank.get(), 127, 1));
	ASSERT_EQ(ch, 0);
	EXPECT_EQ(engine.GetChannel(ch).current, 0);
	int passes = 0;
	while (engine.GetChannel(ch).current < 127 && passes < 100)
	{
		ASSERT_TRUE(Pass());
		++passes;
	}
	EXPECT_EQ(passes, 32); // 127 / 4 rounded up, the last step clamped
	EXPECT_EQ(engine.GetMainChannel(), ch);
	EXPECT_EQ(engine.GetQueued(ch), k_MusicQueueDepth);
	// 32766 = floor(127 * 127 * 258 / 127) * 127 / 127
	EXPECT_EQ(sink.volumes[0], 32766u);
}

TEST_F(MusicEngineTest, NoFadeStartsAtTheVolume)
{
	auto bank = Bank("nofade");
	const int ch = engine.Play(Options(bank.get(), 80, 0));
	EXPECT_EQ(engine.GetChannel(ch).current, 80);
	ASSERT_TRUE(Pass());
	EXPECT_EQ(engine.GetChannel(ch).current, 80);
	EXPECT_EQ(engine.GetChannel(ch).fade, 0);

	// re-triggered as the main channel with a higher volume and no fade: straight there, and fade becomes 1
	EXPECT_EQ(engine.Play(Options(bank.get(), 127, 0)), ch);
	ASSERT_TRUE(Pass());
	EXPECT_EQ(engine.GetChannel(ch).current, 127);
	EXPECT_EQ(engine.GetChannel(ch).fade, 1);
}

TEST_F(MusicEngineTest, FadeOutTakes43And27Passes)
{
	for (const auto& [volume, expected] : {std::pair {127, 43}, std::pair {80, 27}})
	{
		auto bank = Bank("fadeout" + std::to_string(volume));
		const int ch = engine.Play(Options(bank.get(), volume, 0));
		ASSERT_NE(ch, k_NoMusicChannel);
		ASSERT_TRUE(Pass());
		ASSERT_EQ(engine.GetMainChannel(), ch);
		engine.Stop(1); // targets to 0
		int passes = 0;
		while (engine.GetStatus(ch) != MusicStatus::Free && passes < 100)
		{
			ASSERT_TRUE(Pass());
			++passes;
		}
		EXPECT_EQ(passes, expected) << "from " << volume;
		EXPECT_EQ(engine.GetMainChannel(), k_NoMusicChannel);
		EXPECT_EQ(engine.GetChannel(ch).bank, nullptr);
		EXPECT_EQ(engine.GetQueued(ch), 0); // the flush notified every queued chunk
	}
}

TEST_F(MusicEngineTest, SeventhTrackIsRefused)
{
	std::vector<std::unique_ptr<MusicBank>> banks;
	for (int i = 0; i < 7; ++i)
	{
		banks.push_back(Bank("seven" + std::to_string(i)));
	}
	for (int i = 0; i < 6; ++i)
	{
		EXPECT_EQ(engine.Play(Options(banks[static_cast<size_t>(i)].get(), 127, 0)), i);
	}
	EXPECT_EQ(engine.Play(Options(banks[6].get(), 127, 0)), k_NoMusicChannel);
	// the same bank again is the channel it already has
	EXPECT_EQ(engine.Play(Options(banks[2].get(), 127, 0)), 2);
}

TEST_F(MusicEngineTest, NotAMusicBankStopsWithFade)
{
	auto bank = Bank("stopfade");
	const int ch = engine.Play(Options(bank.get(), 127, 0));
	ASSERT_TRUE(Pass());
	EXPECT_EQ(engine.Play(Options(nullptr, 127, 0)), k_NoMusicChannel); // Stop(1)
	EXPECT_EQ(engine.GetChannel(ch).target, 0);
	EXPECT_EQ(engine.GetStatus(ch), MusicStatus::Playing);
}

TEST_F(MusicEngineTest, SyncStartsOnTheSameChunkAndSample)
{
	BankSpec spec;
	spec.segments = 20;
	spec.group = 1;
	auto good = Bank("good", spec);
	auto evil = Bank("evil", spec);
	const int a = engine.Play(Options(good.get(), 80, 0));
	ASSERT_TRUE(Pass());
	for (int i = 0; i < 4; ++i)
	{
		sink.PlayOne(a);
		ASSERT_TRUE(Pass());
	}
	ASSERT_EQ(engine.GetCurrentChunk(a), 5u);
	sink.playPosition[static_cast<size_t>(a)] = 12345;

	const int b = engine.Play(Options(evil.get(), 80, 1, 1));
	ASSERT_EQ(b, 1);
	ASSERT_TRUE(Pass());
	// the chunk the other channel plays, starting at its play position
	ASSERT_FALSE(sink.history[static_cast<size_t>(b)].empty());
	EXPECT_EQ(sink.history[static_cast<size_t>(b)][0].segment, 4u); // chunk 5
	EXPECT_EQ(sink.history[static_cast<size_t>(b)][0].startSample, 12345u);
	EXPECT_TRUE(sink.history[static_cast<size_t>(b)][0].reset);
	EXPECT_EQ(sink.history[static_cast<size_t>(b)][1].startSample, 0u);
	EXPECT_EQ(engine.GetCurrentChunk(b), 5u);
	EXPECT_EQ(engine.GetMainChannel(), b);

	// the old one only fades out from the next pass (it came before in this one) and the new one fades in
	EXPECT_EQ(engine.GetChannel(b).current, 4);
	EXPECT_EQ(engine.GetChannel(a).current, 80);
	ASSERT_TRUE(Pass());
	EXPECT_EQ(engine.GetChannel(a).current, 77);
	EXPECT_EQ(engine.GetChannel(b).current, 8);
}

TEST_F(MusicEngineTest, GroupZeroSyncsAndStartChunk)
{
	BankSpec spec;
	spec.segments = 20;
	auto one = Bank("nogroup1", spec);
	auto two = Bank("nogroup2", spec);
	const int a = engine.Play(Options(one.get(), 127, 0));
	ASSERT_TRUE(Pass());
	sink.PlayOne(a);
	sink.PlayOne(a);
	ASSERT_EQ(engine.GetCurrentChunk(a), 3u);
	MusicPlayOptions options = Options(two.get(), 127, 0, 1);
	options.startChunk = 7;
	const int b = engine.Play(options);
	ASSERT_TRUE(Pass());
	// only group -1 (not a music bank) is left out: two banks of group 0 sync too, and the
	// start chunk is not used
	EXPECT_EQ(sink.history[static_cast<size_t>(b)][0].segment, 2u);

	// a start chunk out of 1..n starts at 1
	auto three = Bank("startchunk", spec);
	MusicPlayOptions late = Options(three.get(), 127, 0, 0);
	late.startChunk = 21;
	const int c = engine.Play(late);
	ASSERT_TRUE(Pass());
	EXPECT_EQ(sink.history[static_cast<size_t>(c)][0].segment, 0u);
}

TEST_F(MusicEngineTest, LoopsAndTheLastChunk)
{
	BankSpec spec;
	spec.segments = 3;
	auto bank = Bank("loops", spec);
	MusicPlayOptions options = Options(bank.get(), 127, 0);
	options.loops = 1;
	int finishedWith = -1;
	options.userData = 42;
	options.finished = [&finishedWith](int data) { finishedWith = data; };
	const int ch = engine.Play(options);
	ASSERT_TRUE(Pass());
	// 4 queued: 1 2 3 1
	ASSERT_EQ(sink.history[static_cast<size_t>(ch)].size(), 4u);
	for (int i = 0; i < 2; ++i)
	{
		sink.PlayOne(ch);
		ASSERT_TRUE(Pass());
	}
	const auto& history = sink.history[static_cast<size_t>(ch)];
	ASSERT_EQ(history.size(), 6u);
	const std::array<uint32_t, 6> expected = {0, 1, 2, 0, 1, 2};
	for (size_t i = 0; i < history.size(); ++i)
	{
		EXPECT_EQ(history[i].segment, expected[i]);
		EXPECT_EQ(history[i].last, i == 5);
	}
	EXPECT_EQ(engine.GetStatus(ch), MusicStatus::LastQueued);
	// the audible chunk wraps after the last segment
	sink.PlayOne(ch);
	sink.PlayOne(ch);
	EXPECT_EQ(engine.GetCurrentChunk(ch), 2u);
	sink.PlayOne(ch);
	sink.PlayOne(ch);
	EXPECT_EQ(engine.GetStatus(ch), MusicStatus::Finished);
	EXPECT_EQ(finishedWith, -1);
	ASSERT_TRUE(Pass());
	// freed, callback with the user data, main channel -1
	EXPECT_EQ(finishedWith, 42);
	EXPECT_EQ(engine.GetStatus(ch), MusicStatus::Free);
	EXPECT_EQ(engine.GetMainChannel(), k_NoMusicChannel);
}

TEST_F(MusicEngineTest, BankOverridesVolumeAndLoops)
{
	BankSpec spec;
	spec.flags = 0x60;
	spec.loops = -1;
	spec.volume = 65;
	auto bank = Bank("overrides", spec);
	const int ch = engine.Play(Options(bank.get(), 127, 0));
	EXPECT_EQ(engine.GetChannel(ch).sadVolume, 65);
	EXPECT_EQ(engine.GetChannel(ch).loops, -1);
	ASSERT_TRUE(Pass());
	// floor(127 * 65 * 258 / 127) = 16770, * 127 / 127
	EXPECT_EQ(sink.volumes[static_cast<size_t>(ch)], 16770u);
}

TEST_F(MusicEngineTest, MainVolume)
{
	auto bank = Bank("master");
	const int ch = engine.Play(Options(bank.get(), 127, 0));
	ASSERT_TRUE(Pass());
	engine.SetMainVolume(64);
	EXPECT_EQ(engine.GetMainVolume(), 64);
	// floor(32766 * 64 / 127) = 16512, reapplied at once
	EXPECT_EQ(sink.volumes[static_cast<size_t>(ch)], 16512u);
	EXPECT_NEAR(static_cast<float>(sink.volumes[static_cast<size_t>(ch)]) / 32767.0f,
	            127.0f * 127.0f * 64.0f / (127.0f * 127.0f * 127.0f), 1e-3f);
	engine.SetMainVolume(200); // above 127: 127
	EXPECT_EQ(engine.GetMainVolume(), 127);
	engine.SetMainVolume(0);
	EXPECT_EQ(sink.volumes[static_cast<size_t>(ch)], 0u);
	engine.SetMainVolume(static_cast<uint32_t>(-1)); // unsigned compare: 127
	EXPECT_EQ(engine.GetMainVolume(), 127);
}

TEST_F(MusicEngineTest, CutStopsAtOnce)
{
	auto bank = Bank("cut");
	const int ch = engine.Play(Options(bank.get(), 127, 0));
	ASSERT_TRUE(Pass());
	ASSERT_EQ(engine.GetQueued(ch), 4);
	engine.Stop(0);
	EXPECT_EQ(engine.GetStatus(ch), MusicStatus::Free);
	EXPECT_EQ(engine.GetQueued(ch), 0);
	EXPECT_EQ(sink.flushes[static_cast<size_t>(ch)], 1);
	EXPECT_EQ(sink.volumes[static_cast<size_t>(ch)], 0u);
	EXPECT_EQ(engine.GetMainChannel(), k_NoMusicChannel);
}

TEST_F(MusicEngineTest, StopOneChannel)
{
	auto one = Bank("stopone1");
	auto two = Bank("stopone2");
	const int a = engine.Play(Options(one.get(), 127, 0));
	ASSERT_TRUE(Pass());
	const int b = engine.Play(Options(two.get(), 127, 0));
	ASSERT_TRUE(Pass());
	ASSERT_EQ(engine.GetMainChannel(), b);
	engine.Stop(a, 1); // not the main channel: a fade does nothing
	EXPECT_EQ(engine.GetStatus(a), MusicStatus::Playing);
	engine.Stop(a, 0);
	EXPECT_EQ(engine.GetStatus(a), MusicStatus::Free);
	EXPECT_EQ(engine.GetStatus(b), MusicStatus::Playing);
	engine.Stop(b, 1); // the main channel stops them all
	EXPECT_EQ(engine.GetChannel(b).target, 0);
}

TEST_F(MusicEngineTest, Markers)
{
	BankSpec spec;
	spec.segments = 4;
	spec.descriptions = {"!0=L1!12096=P", "!100=L2"};
	auto bank = Bank("markers", spec);
	std::vector<std::string> fired;
	MusicPlayOptions options = Options(bank.get(), 127, 0);
	options.marker = [&fired](std::string_view label) { fired.emplace_back(label); };
	const int ch = engine.Play(options);
	ASSERT_TRUE(Pass()); // queues; the clock starts now, not the main channel yet when the markers ran
	EXPECT_TRUE(fired.empty());
	const uint32_t start = _now;
	ASSERT_EQ(engine.GetChannel(ch).markers->startTick, start);
	ASSERT_TRUE(Pass()); // 120 ms = 2646 samples
	ASSERT_EQ(fired.size(), 1u);
	EXPECT_EQ(fired[0], "L1");
	while (_now - start < 2200)
	{
		ASSERT_TRUE(Pass());
	}
	// P at 12096 samples (549 ms) and L2 at 24192 + 100 (1102 ms), each once
	ASSERT_EQ(fired.size(), 3u);
	EXPECT_EQ(fired[1], "P");
	EXPECT_EQ(fired[2], "L2");
}

TEST_F(MusicEngineTest, ReadFailureEndsThePass)
{
	BankSpec spec;
	spec.segments = 8;
	spec.badOffsetSegment = 2;
	auto bank = Bank("readfail", spec);
	const int ch = engine.Play(Options(bank.get(), 127, 1));
	EXPECT_FALSE(Pass()); // segments 1 and 2 queued, the 3rd fails: the thread sleeps 5000 ms
	EXPECT_EQ(engine.GetQueued(ch), 2);
	EXPECT_EQ(engine.GetChannel(ch).current, 0); // the fade step of that pass did not run
}

TEST_F(MusicEngineTest, PauseRestartAndSwitch)
{
	auto bank = Bank("pause");
	const int ch = engine.Play(Options(bank.get(), 127, 0));
	ASSERT_TRUE(Pass());
	engine.Pause();
	EXPECT_EQ(engine.GetStatus(ch), MusicStatus::Paused);
	EXPECT_TRUE(sink.paused[static_cast<size_t>(ch)]);
	engine.Restart();
	EXPECT_EQ(engine.GetStatus(ch), MusicStatus::Playing);
	EXPECT_FALSE(sink.paused[static_cast<size_t>(ch)]);

	engine.Switch(0); // Stop(0), inactive
	EXPECT_FALSE(engine.IsActive());
	EXPECT_EQ(engine.GetStatus(ch), MusicStatus::Free); // GetStatus needs active: 0
	EXPECT_EQ(engine.GetMainVolume(), -1);
	EXPECT_EQ(engine.Play(Options(bank.get(), 127, 0)), 0); // Play only needs installed
	ASSERT_TRUE(Pass());
	EXPECT_EQ(engine.GetQueued(0), 0); // the thread does not queue while inactive
	engine.Switch(1);
	ASSERT_TRUE(Pass());
	EXPECT_EQ(engine.GetQueued(0), 4);
}

TEST_F(MusicEngineTest, PitchAnd3D)
{
	BankSpec spec;
	spec.flags = 0x380;
	auto bank = Bank("pitch", spec);
	MusicPlayOptions options = Options(bank.get(), 127, 0);
	options.is3D = 1;
	options.distance = {30.0f, 120.0f, 2.0f};
	const int ch = engine.Play(options);
	EXPECT_TRUE(sink.enabled3D[static_cast<size_t>(ch)]);
	// flags 0x80/0x100/0x200 take the .sad's own min/max distance and scale (0 in this bank)
	EXPECT_EQ(sink.mappings[static_cast<size_t>(ch)].minDistance, 0.0f);
	ASSERT_TRUE(Pass());
	EXPECT_EQ(sink.frequency[static_cast<size_t>(ch)], 22050u);
	engine.SetPitch(ch, 300); // clamped to 250
	EXPECT_EQ(engine.GetChannel(ch).pitch, 250);
	EXPECT_EQ(sink.frequency[static_cast<size_t>(ch)], 55125u);
	engine.SetPitch(ch, 10); // clamped to 50
	EXPECT_EQ(sink.frequency[static_cast<size_t>(ch)], 11025u);
}

TEST_F(MusicEngineTest, CloseFlushesEverything)
{
	auto bank = Bank("close");
	const int ch = engine.Play(Options(bank.get(), 127, 0));
	ASSERT_TRUE(Pass());
	engine.Close();
	EXPECT_EQ(engine.GetQueued(ch), 0);
	EXPECT_FALSE(engine.IsInstalled());
	EXPECT_EQ(engine.Play(Options(bank.get(), 127, 0)), k_NoMusicChannel);
}
