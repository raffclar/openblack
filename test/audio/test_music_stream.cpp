/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "Audio/Engine/MusicBank.h"
#include "Audio/Engine/MusicEngine.h"
#include "Audio/Engine/MusicStream.h"
#include "support/TestServices.h"

// The continuous MP2 decoding of the music segments (the bank's decoder) and MusicStream under the engine. Expected
// values from the data (docs/bw1-notes/audio.md): align/good.sad has 430 segments of 21 frames = 24192 samples, the
// last one shorter, 471 s at 22050 Hz, stereo. Needs OPENBLACK_TEST_BW_ROOT; without it the tests skip. No OpenAL
// context here: MusicStream only decodes and queues.
// The *Synthetic tests run the same checks on small banks of silent MPEG-2 layer II frames written by the test.

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

std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

std::optional<std::filesystem::path> GameRoot()
{
	const char* root = std::getenv("OPENBLACK_TEST_BW_ROOT");
	if (root == nullptr || *root == '\0' || !std::filesystem::is_directory(root))
	{
		return std::nullopt;
	}
	return std::filesystem::path(root);
}

std::optional<std::filesystem::path> FindNoCase(const std::filesystem::path& root, std::string_view relative)
{
	auto current = root;
	for (const auto& part : std::filesystem::path(relative))
	{
		const auto wanted = Lower(part.string());
		std::optional<std::filesystem::path> found;
		std::error_code ec;
		for (const auto& entry : std::filesystem::directory_iterator(current, ec))
		{
			if (Lower(entry.path().filename().string()) == wanted)
			{
				found = entry.path();
				break;
			}
		}
		if (!found)
		{
			return std::nullopt;
		}
		current = *found;
	}
	return current;
}

std::unique_ptr<MusicBank> Open(std::string_view relative)
{
	const auto root = GameRoot();
	if (!root)
	{
		return nullptr;
	}
	const auto path = FindNoCase(*root, relative);
	return path ? RegisterMusicBank(*path) : nullptr;
}

// ---- synthetic banks ----------------------------------------------------------------------------------------------

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

/// Removes the file the test wrote when it goes out of scope (declare it before the bank that keeps the file open)
struct TempFile
{
	std::filesystem::path path;
	explicit TempFile(std::filesystem::path p)
	    : path(std::move(p))
	{
	}
	TempFile(const TempFile&) = delete;
	TempFile& operator=(const TempFile&) = delete;
	~TempFile()
	{
		std::error_code ec;
		std::filesystem::remove(path, ec);
	}
};

/// A silent MPEG-2 layer II frame at 22050 Hz with no CRC: the header, then zero bits (no subband is allocated).
/// Bytes = 144 x bitrate / 22050; 8 kbps is bitrate index 1, 64 kbps index 8.
std::vector<uint8_t> SilentFrames(int count, int kbps, bool mono)
{
	const uint8_t index = kbps == 64 ? 8 : 1;
	std::vector<uint8_t> frame(static_cast<size_t>(144 * kbps * 1000 / 22050), 0);
	frame[0] = 0xFF;
	frame[1] = 0xF5;
	frame[2] = static_cast<uint8_t>(index << 4);
	frame[3] = mono ? 0xC0 : 0x00;
	std::vector<uint8_t> bytes;
	for (int i = 0; i < count; ++i)
	{
		bytes.insert(bytes.end(), frame.begin(), frame.end());
	}
	return bytes;
}

void Put32(std::vector<uint8_t>& bytes, size_t offset, uint32_t value)
{
	std::memcpy(&bytes[offset], &value, sizeof(value));
}

void WriteBlock(std::ofstream& out, const char* name, const std::vector<uint8_t>& data)
{
	std::array<char, 32> blockName {};
	std::strncpy(blockName.data(), name, blockName.size() - 1);
	out.write(blockName.data(), blockName.size());
	const auto size = static_cast<uint32_t>(data.size());
	out.write(reinterpret_cast<const char*>(&size), sizeof(size));
	out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

/// A music bank (LiOnHeAd with the bank info, the wave data and the sample table) of these segments, in this group
std::filesystem::path WriteBank(const std::string& name, const std::vector<std::vector<uint8_t>>& segments, uint32_t group)
{
	// byte offsets inside one sample table record
	constexpr size_t k_SegmentSize = 0x10C;
	constexpr size_t k_SegmentOffset = 0x110;
	constexpr size_t k_Group = 0x118;
	constexpr size_t k_Rate = 0x128;
	const auto path = std::filesystem::path(TEST_BINARY_DIR) / ("music_stream_" + name + "_" + UniqueSuffix() + ".sad");
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write("LiOnHeAd", 8);
	WriteBlock(out, "LHFileSegmentBankInfo", {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0});
	std::vector<uint8_t> wave;
	std::vector<uint8_t> table(4 + segments.size() * MusicBank::k_RecordSize, 0);
	Put32(table, 0, static_cast<uint32_t>(segments.size()));
	for (size_t i = 0; i < segments.size(); ++i)
	{
		const size_t r = 4 + i * MusicBank::k_RecordSize;
		Put32(table, r + k_SegmentSize, static_cast<uint32_t>(segments[i].size()));
		Put32(table, r + k_SegmentOffset, static_cast<uint32_t>(wave.size()));
		Put32(table, r + k_Group, group);
		Put32(table, r + k_Rate, 22050);
		wave.insert(wave.end(), segments[i].begin(), segments[i].end());
	}
	WriteBlock(out, "LHAudioWaveData", wave);
	WriteBlock(out, "LHAudioBankSampleTable", table);
	return path;
}

/// `full` segments of 21 frames, then one of `lastFrames` frames when not 0
std::vector<std::vector<uint8_t>> Segments(int full, int lastFrames, int kbps, bool mono)
{
	constexpr int k_FramesPerSegment = k_MusicSamplesPerSegment / 1152;
	std::vector<std::vector<uint8_t>> segments(static_cast<size_t>(full), SilentFrames(k_FramesPerSegment, kbps, mono));
	if (lastFrames > 0)
	{
		segments.push_back(SilentFrames(lastFrames, kbps, mono));
	}
	return segments;
}
} // namespace

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(MusicStream, DecodesGoodContinuously)
{
	auto bank = Open("audio/music/align/good.sad");
	if (!bank)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set or no align/good.sad";
	}
	ASSERT_EQ(bank->GetSegmentCount(), 430u);
	MusicSegmentDecoder decoder;
	std::vector<uint8_t> data;
	std::vector<int16_t> pcm;
	uint64_t total = 0;
	for (size_t i = 0; i < bank->GetSegmentCount(); ++i)
	{
		ASSERT_TRUE(bank->ReadSegment(i, data));
		pcm.clear();
		const auto frames = decoder.Decode(data, pcm);
		if (i + 1 < bank->GetSegmentCount())
		{
			ASSERT_EQ(frames, static_cast<size_t>(k_MusicSamplesPerSegment)) << "segment " << i;
		}
		else
		{
			EXPECT_GT(frames, 0u);
			EXPECT_LT(frames, static_cast<size_t>(k_MusicSamplesPerSegment));
			EXPECT_EQ(frames % 1152, 0u); // whole Layer II frames
		}
		ASSERT_EQ(pcm.size(), frames * 2) << "segment " << i;
		total += frames;
	}
	EXPECT_EQ(decoder.GetChannels(), 2);
	EXPECT_EQ(decoder.GetSampleRate(), 22050);
	EXPECT_GE(total, 429u * static_cast<uint64_t>(k_MusicSamplesPerSegment));
	// 429 x 24192 + the last segment (15865 bytes) = 19 frames of 1152: 10400256 samples, 471.67 s (the 471.1 s of the
	// .sad table is an estimate from the bytes)
	EXPECT_EQ(total, 429u * static_cast<uint64_t>(k_MusicSamplesPerSegment) + 19u * 1152u);
	std::cout << "align/good.sad: " << total << " samples, " << static_cast<double>(total) / 22050.0 << " s\n";
}

TEST(MusicStream, DecodesGoodContinuouslySynthetic)
{
	// Three full segments of stereo 8 kbps frames and a last one of 4 frames, through one decoder
	const auto segments = Segments(3, 4, 8, false);
	const TempFile file {WriteBank("stereo", segments, 1)};
	auto bank = RegisterMusicBank(file.path);
	ASSERT_NE(bank, nullptr);
	ASSERT_EQ(bank->GetSegmentCount(), segments.size());
	MusicSegmentDecoder decoder;
	std::vector<uint8_t> data;
	std::vector<int16_t> pcm;
	uint64_t total = 0;
	for (size_t i = 0; i < bank->GetSegmentCount(); ++i)
	{
		ASSERT_TRUE(bank->ReadSegment(i, data));
		pcm.clear();
		const auto frames = decoder.Decode(data, pcm);
		if (i + 1 < bank->GetSegmentCount())
		{
			ASSERT_EQ(frames, static_cast<size_t>(k_MusicSamplesPerSegment)) << "segment " << i;
		}
		else
		{
			EXPECT_EQ(frames, 4u * 1152u);
		}
		ASSERT_EQ(pcm.size(), frames * 2) << "segment " << i;
		// silence in, silence out
		EXPECT_TRUE(std::all_of(pcm.begin(), pcm.end(), [](int16_t s) { return s == 0; })) << "segment " << i;
		total += frames;
	}
	EXPECT_EQ(decoder.GetChannels(), 2);
	EXPECT_EQ(decoder.GetSampleRate(), 22050);
	EXPECT_EQ(total, 3u * static_cast<uint64_t>(k_MusicSamplesPerSegment) + 4u * 1152u);
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(MusicStream, DecodesMono64k)
{
	auto bank = Open("audio/music/script/gregorian3d.sad");
	if (!bank)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set or no script/Gregorian3D.sad";
	}
	MusicSegmentDecoder decoder;
	std::vector<uint8_t> data;
	std::vector<int16_t> pcm;
	ASSERT_TRUE(bank->ReadSegment(0, data));
	EXPECT_EQ(decoder.Decode(data, pcm), static_cast<size_t>(k_MusicSamplesPerSegment));
	EXPECT_EQ(decoder.GetChannels(), 1);
	EXPECT_EQ(pcm.size(), static_cast<size_t>(k_MusicSamplesPerSegment));
}

TEST(MusicStream, DecodesMono64kSynthetic)
{
	// One full segment of mono 64 kbps frames: one sample per frame
	const TempFile file {WriteBank("mono", Segments(1, 0, 64, true), 0)};
	auto bank = RegisterMusicBank(file.path);
	ASSERT_NE(bank, nullptr);
	MusicSegmentDecoder decoder;
	std::vector<uint8_t> data;
	std::vector<int16_t> pcm;
	ASSERT_TRUE(bank->ReadSegment(0, data));
	EXPECT_EQ(decoder.Decode(data, pcm), static_cast<size_t>(k_MusicSamplesPerSegment));
	EXPECT_EQ(decoder.GetChannels(), 1);
	EXPECT_EQ(pcm.size(), static_cast<size_t>(k_MusicSamplesPerSegment));
}

// Integration test: needs the original game data (OPENBLACK_TEST_BW_ROOT); skipped without it
TEST(MusicStream, EngineQueuesDecodedChunks)
{
	auto good = Open("audio/music/align/good.sad");
	auto evil = Open("audio/music/align/evil.sad");
	if (!good || !evil)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_BW_ROOT not set or no align banks";
	}
	MusicStream stream;
	ASSERT_FALSE(stream.IsAvailable()); // no OpenAL context in the tests
	MusicEngine engine(stream);
	engine.NoteBankRegistered(*good);
	engine.NoteBankRegistered(*evil);
	EXPECT_EQ(engine.GetTotalGroups(), 1u);

	MusicPlayOptions options;
	options.bank = good.get();
	options.volume = 80;
	options.fade = 1;
	const int a = engine.Play(options);
	ASSERT_EQ(a, 0);
	uint32_t now = 1000;
	ASSERT_TRUE(engine.Process(now));
	EXPECT_EQ(engine.GetQueued(a), k_MusicQueueDepth);
	EXPECT_EQ(stream.GetDecodedFrames(a), 4u * k_MusicSamplesPerSegment);

	// the change good -> evil (same group 1, sync): the same chunk, decoded by a fresh decoder
	options.bank = evil.get();
	options.sync = 1;
	const int b = engine.Play(options);
	ASSERT_EQ(b, 1);
	now += k_MusicPassSleepMs;
	ASSERT_TRUE(engine.Process(now));
	EXPECT_EQ(engine.GetCurrentChunk(b), engine.GetCurrentChunk(a));
	EXPECT_EQ(stream.GetDecodedFrames(b), 4u * k_MusicSamplesPerSegment);
	EXPECT_EQ(engine.GetMainChannel(), b);

	engine.Stop(0);
	EXPECT_EQ(engine.GetQueued(a), 0);
	EXPECT_EQ(engine.GetQueued(b), 0);
	EXPECT_TRUE(stream.IsChannelDone(a));
}

TEST(MusicStream, EngineQueuesDecodedChunksSynthetic)
{
	// Two banks of the same group 1 with more segments than the queue holds
	const TempFile goodFile {WriteBank("good", Segments(8, 0, 8, false), 1)};
	const TempFile evilFile {WriteBank("evil", Segments(8, 0, 8, false), 1)};
	auto good = RegisterMusicBank(goodFile.path);
	auto evil = RegisterMusicBank(evilFile.path);
	ASSERT_NE(good, nullptr);
	ASSERT_NE(evil, nullptr);
	MusicStream stream;
	ASSERT_FALSE(stream.IsAvailable()); // no OpenAL context in the tests
	MusicEngine engine(stream);
	engine.NoteBankRegistered(*good);
	engine.NoteBankRegistered(*evil);
	EXPECT_EQ(engine.GetTotalGroups(), 1u);

	const auto queuedFrames = static_cast<uint64_t>(k_MusicQueueDepth) * k_MusicSamplesPerSegment;
	MusicPlayOptions options;
	options.bank = good.get();
	options.volume = 80;
	options.fade = 1;
	const int a = engine.Play(options);
	ASSERT_EQ(a, 0);
	uint32_t now = 1000;
	ASSERT_TRUE(engine.Process(now));
	EXPECT_EQ(engine.GetQueued(a), k_MusicQueueDepth);
	EXPECT_EQ(stream.GetDecodedFrames(a), queuedFrames);

	// the change to the other bank of the group, in sync: the same chunk, decoded by a fresh decoder
	options.bank = evil.get();
	options.sync = 1;
	const int b = engine.Play(options);
	ASSERT_EQ(b, 1);
	now += k_MusicPassSleepMs;
	ASSERT_TRUE(engine.Process(now));
	EXPECT_EQ(engine.GetCurrentChunk(b), engine.GetCurrentChunk(a));
	EXPECT_EQ(stream.GetDecodedFrames(b), queuedFrames);
	EXPECT_EQ(engine.GetMainChannel(), b);

	engine.Stop(0);
	EXPECT_EQ(engine.GetQueued(a), 0);
	EXPECT_EQ(engine.GetQueued(b), 0);
	EXPECT_TRUE(stream.IsChannelDone(a));
}
