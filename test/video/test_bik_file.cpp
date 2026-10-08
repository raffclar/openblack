/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// video::BikFile (src/Video/BikFile.h): the Bink 1 container. Synthetic files for the layout and the checks, and the
// five .bik of the game
// when OPENBLACK_TEST_GAME_PATH (or OPENBLACK_TEST_BW_ROOT) is the game's folder; without it those tests skip.

#include <cctype>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

// the tests build their own file system
#define LOCATOR_IMPLEMENTATIONS
#include "FileSystem/DefaultFileSystem.h"
#include "Locator.h"
#include "Video/BikFile.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::video;

namespace
{
void PutU32(std::vector<uint8_t>& data, size_t at, uint32_t v)
{
	data[at + 0] = static_cast<uint8_t>(v);
	data[at + 1] = static_cast<uint8_t>(v >> 8);
	data[at + 2] = static_cast<uint8_t>(v >> 16);
	data[at + 3] = static_cast<uint8_t>(v >> 24);
}

void PutU16(std::vector<uint8_t>& data, size_t at, uint16_t v)
{
	data[at + 0] = static_cast<uint8_t>(v);
	data[at + 1] = static_cast<uint8_t>(v >> 8);
}

/// A Bink 1 file with `packets` as the frames (frame 0 a key frame) and `tracks` audio tracks of the header. The
/// packets must have even sizes, as in a real file: bit 0 of an offset is the key frame flag
std::vector<uint8_t> MakeBik(const std::vector<std::vector<uint8_t>>& packets, uint32_t tracks = 0, uint32_t width = 16,
                             uint32_t height = 8, uint32_t fpsNum = 24, uint32_t fpsDen = 1)
{
	const auto frames = static_cast<uint32_t>(packets.size());
	const size_t trackTable = BikFile::k_HeaderSize;
	const size_t frameTable = trackTable + BikFile::k_AudioTrackSize * tracks;
	const size_t tableEnd = frameTable + 4 * (static_cast<size_t>(frames) + 1);
	std::vector<uint8_t> data(tableEnd, 0);
	data[0] = 'B';
	data[1] = 'I';
	data[2] = 'K';
	data[3] = 'i';
	size_t largest = 0;
	for (uint32_t i = 0; i < frames; ++i)
	{
		PutU32(data, frameTable + 4 * i, static_cast<uint32_t>(data.size()) | (i == 0 ? 1u : 0u));
		data.insert(data.end(), packets[i].begin(), packets[i].end());
		largest = std::max(largest, packets[i].size());
	}
	PutU32(data, frameTable + 4 * static_cast<size_t>(frames), static_cast<uint32_t>(data.size()));
	PutU32(data, 4, static_cast<uint32_t>(data.size() - 8));
	PutU32(data, 8, frames);
	PutU32(data, 12, static_cast<uint32_t>(largest));
	PutU32(data, 16, frames);
	PutU32(data, 20, width);
	PutU32(data, 24, height);
	PutU32(data, 28, fpsNum);
	PutU32(data, 32, fpsDen);
	PutU32(data, 36, 0);
	PutU32(data, 40, tracks);
	for (uint32_t t = 0; t < tracks; ++t)
	{
		PutU32(data, trackTable + 4 * t, 0x1000 + t);
		PutU16(data, trackTable + 4 * (tracks + t), static_cast<uint16_t>(22050 + t));
		PutU16(data, trackTable + 4 * (tracks + t) + 2, BikFile::k_AudioFlagStereo);
		PutU32(data, trackTable + 4 * (2 * tracks + t), 7 + t);
	}
	return data;
}

std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

std::optional<std::filesystem::path> GameRoot()
{
	for (const char* name : {"OPENBLACK_TEST_GAME_PATH", "OPENBLACK_TEST_BW_ROOT"})
	{
		const char* root = std::getenv(name);
		if (root != nullptr && *root != '\0' && std::filesystem::is_directory(root))
		{
			return std::filesystem::path(root);
		}
	}
	return std::nullopt;
}

// The original runs on a case-insensitive file system (INTRO.bik is opened as data\intro.bik)
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

struct GameBik
{
	const char* path;
	uint32_t width;
	uint32_t height;
	uint32_t fps;
	uint32_t frames;
	uint32_t keyFrames;
	size_t size;
};

// the installed files: all BIKi, fps x / 1, no audio track
constexpr GameBik k_GameFiles[] = {
    {"Data/INTRO.bik", 640, 360, 24, 1601, 1, 0xF78E0C},
    {"Data/logo.bik", 768, 512, 15, 2, 2, 0xF400},
    {"Data/pre_intro.bik", 768, 512, 25, 2515, 1, 0x1CCE0F8},
    {"Data/tips.bik", 768, 512, 15, 35, 35, 0x473334},
    {"Data/Spells/fall/fall.bik", 640, 360, 24, 1200, 1, 0x8C0388},
};
} // namespace

TEST(BikFile, ParsesTheLayout)
{
	BikFile file;
	ASSERT_TRUE(file.Parse(MakeBik({{1, 2, 3, 4}, {4, 5}, {6, 7}}))) << file.GetError();
	EXPECT_TRUE(file.IsOpen());
	EXPECT_EQ(file.Revision(), 'i');
	EXPECT_EQ(file.FrameCount(), 3u);
	EXPECT_EQ(file.Width(), 16u);
	EXPECT_EQ(file.Height(), 8u);
	EXPECT_EQ(file.FpsNumerator(), 24u);
	EXPECT_EQ(file.FpsDenominator(), 1u);
	EXPECT_EQ(file.Fps(), 24u);
	EXPECT_EQ(file.LargestFrameSize(), 4u);
	EXPECT_EQ(file.HeaderFileSize(), file.Size() - 8);
	EXPECT_TRUE(file.AudioTracks().empty());
	EXPECT_TRUE(file.IsKeyFrame(0));
	EXPECT_FALSE(file.IsKeyFrame(1));
	EXPECT_FALSE(file.IsKeyFrame(3));
	EXPECT_EQ(file.KeyFrameCount(), 1u);
	EXPECT_EQ(std::vector<uint8_t>(file.FrameData(0).begin(), file.FrameData(0).end()), (std::vector<uint8_t> {1, 2, 3, 4}));
	EXPECT_EQ(std::vector<uint8_t>(file.VideoData(1).begin(), file.VideoData(1).end()), (std::vector<uint8_t> {4, 5}));
	EXPECT_EQ(file.FrameData(2).size(), 2u);
	EXPECT_TRUE(file.FrameData(3).empty());
	EXPECT_TRUE(file.VideoData(3).empty());
	EXPECT_EQ(file.FrameOffset(0), BikFile::k_HeaderSize + 4 * 4);
	EXPECT_EQ(file.FrameOffset(3), file.Size());
	EXPECT_EQ(file.FrameOffset(4), 0u);
}

TEST(BikFile, FpsIsTruncated)
{
	// integer division: 30000 / 1001 -> 29
	BikFile file;
	ASSERT_TRUE(file.Parse(MakeBik({{0, 0}}, 0, 16, 8, 30000, 1001))) << file.GetError();
	EXPECT_EQ(file.Fps(), 29u);
}

TEST(BikFile, SplitsTheAudioPackets)
{
	// per track a 32-bit size and the bytes, then the video
	const std::vector<uint8_t> frame0 {2, 0, 0, 0, 0xA1, 0xA2, 0, 0, 0, 0, 0x11, 0x12, 0x13, 0x14};
	const std::vector<uint8_t> frame1 {0, 0, 0, 0, 1, 0, 0, 0, 0xB1, 0x21};
	BikFile file;
	ASSERT_TRUE(file.Parse(MakeBik({frame0, frame1}, 2))) << file.GetError();
	ASSERT_EQ(file.AudioTracks().size(), 2u);
	EXPECT_EQ(file.AudioTracks()[0].maxBuffer, 0x1000u);
	EXPECT_EQ(file.AudioTracks()[1].sampleRate, 22051u);
	EXPECT_EQ(file.AudioTracks()[0].flags, BikFile::k_AudioFlagStereo);
	EXPECT_EQ(file.AudioTracks()[1].id, 8u);
	EXPECT_EQ(std::vector<uint8_t>(file.AudioData(0, 0).begin(), file.AudioData(0, 0).end()),
	          (std::vector<uint8_t> {0xA1, 0xA2}));
	EXPECT_TRUE(file.AudioData(0, 1).empty());
	EXPECT_EQ(std::vector<uint8_t>(file.VideoData(0).begin(), file.VideoData(0).end()),
	          (std::vector<uint8_t> {0x11, 0x12, 0x13, 0x14}));
	EXPECT_TRUE(file.AudioData(1, 0).empty());
	EXPECT_EQ(std::vector<uint8_t>(file.AudioData(1, 1).begin(), file.AudioData(1, 1).end()), (std::vector<uint8_t> {0xB1}));
	EXPECT_EQ(std::vector<uint8_t>(file.VideoData(1).begin(), file.VideoData(1).end()), (std::vector<uint8_t> {0x21}));
	EXPECT_TRUE(file.AudioData(0, 2).empty());
}

TEST(BikFile, RejectsBadFiles)
{
	BikFile file;
	EXPECT_FALSE(file.Parse({}));
	EXPECT_FALSE(file.GetError().empty());
	EXPECT_FALSE(file.IsOpen());

	auto data = MakeBik({{1, 1}, {2, 2}});
	data[0] = 'K'; // "KB2": Bink 2
	EXPECT_FALSE(file.Parse(data));

	data = MakeBik({{1, 1}, {2, 2}});
	PutU32(data, 8, 0); // no frames
	EXPECT_FALSE(file.Parse(data));

	EXPECT_FALSE(file.Parse(MakeBik({{1, 1}}, 0, 0, 8)));     // no picture
	EXPECT_FALSE(file.Parse(MakeBik({{1, 1}}, 0, 16, 8, 0))); // no frame rate
	EXPECT_FALSE(file.Parse(MakeBik({{1, 1}}, 0, 16, 8, 24, 0)));

	data = MakeBik({{1, 1}, {2, 2}});
	data.push_back(0); // the last offset is not the file size
	EXPECT_FALSE(file.Parse(data));

	data = MakeBik({{1, 1}, {2, 2}});
	data.pop_back();
	EXPECT_FALSE(file.Parse(data));

	data = MakeBik({{1, 2}, {3, 4}});
	PutU32(data, BikFile::k_HeaderSize + 4, BikFile::k_HeaderSize + 12); // frame 1 before frame 0
	EXPECT_FALSE(file.Parse(data));

	data = MakeBik({{1, 1}, {2, 2}});
	PutU32(data, BikFile::k_HeaderSize, 0x1); // frame 0 inside the tables
	EXPECT_FALSE(file.Parse(data));

	data = MakeBik({{1, 1}, {2, 2}});
	PutU32(data, 8, 1000); // a table past the end of the file
	EXPECT_FALSE(file.Parse(data));

	// an audio packet longer than its frame
	EXPECT_FALSE(file.Parse(MakeBik({{9, 0, 0, 0, 1, 0}}, 1)));
	// no room for the size
	EXPECT_FALSE(file.Parse(MakeBik({{9, 0}}, 1)));

	// a good one after the bad ones
	EXPECT_TRUE(file.Parse(MakeBik({{1, 1}})));
	EXPECT_TRUE(file.GetError().empty());
	// the file system a test gives the files it opens, restored afterwards
	const test::RestoreService<Locator::filesystem> restoreFilesystem;
	Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
	EXPECT_FALSE(file.Open(std::filesystem::path("this file does not exist.bik")));
	EXPECT_FALSE(file.IsOpen());
}

TEST(BikFile, OpenReadsWhatParseReads)
{
	// Open keeps the stream and reads each packet when asked: the same tables, packets, audio and errors as Parse of the
	// same bytes, in any order of access
	const test::RestoreService<Locator::filesystem> restoreFilesystem;
	Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
	const auto folder = std::filesystem::temp_directory_path() / "openblack_test_bik_file";
	std::filesystem::create_directories(folder);
	const std::vector<uint8_t> frame0 {2, 0, 0, 0, 0xA1, 0xA2, 0, 0, 0, 0, 0x11, 0x12, 0x13, 0x14};
	const std::vector<uint8_t> frame1 {0, 0, 0, 0, 1, 0, 0, 0, 0xB1, 0x21};
	std::vector<std::vector<uint8_t>> files {MakeBik({{1, 2, 3, 4}, {4, 5}, {6, 7}}),
	                                         MakeBik({frame0, frame1}, 2),
	                                         MakeBik({{9, 0, 0, 0, 1, 0}}, 1),
	                                         MakeBik({{9, 0}}, 1),
	                                         {},
	                                         {'B', 'I', 'K'}};
	auto truncated = MakeBik({{1, 1}, {2, 2}});
	truncated.pop_back();
	files.push_back(truncated);
	auto pastEnd = MakeBik({{1, 1}, {2, 2}});
	PutU32(pastEnd, 8, 1000); // a table past the end of the file
	files.push_back(pastEnd);
	for (size_t i = 0; i < files.size(); ++i)
	{
		const auto path = folder / ("film" + std::to_string(i) + ".bik");
		{
			std::ofstream out(path, std::ios::binary | std::ios::trunc);
			out.write(reinterpret_cast<const char*>(files[i].data()), static_cast<std::streamsize>(files[i].size()));
		}
		BikFile parsed;
		BikFile opened;
		const bool parsedOk = parsed.Parse(files[i]);
		ASSERT_EQ(opened.Open(path), parsedOk) << i << ": " << opened.GetError();
		EXPECT_EQ(opened.GetError(), parsed.GetError()) << i;
		EXPECT_EQ(opened.IsOpen(), parsed.IsOpen()) << i;
		if (!parsedOk)
		{
			continue;
		}
		EXPECT_EQ(opened.Size(), parsed.Size()) << i;
		ASSERT_EQ(opened.FrameCount(), parsed.FrameCount()) << i;
		ASSERT_EQ(opened.AudioTracks().size(), parsed.AudioTracks().size()) << i;
		const auto bytes = [](std::span<const uint8_t> span) { return std::vector<uint8_t>(span.begin(), span.end()); };
		// backwards, then forwards, each packet asked for more than once
		for (int pass = 0; pass < 2; ++pass)
		{
			for (uint32_t k = 0; k < opened.FrameCount(); ++k)
			{
				const uint32_t frame = pass == 0 ? opened.FrameCount() - 1 - k : k;
				EXPECT_EQ(bytes(opened.FrameData(frame)), bytes(parsed.FrameData(frame))) << i << " frame " << frame;
				EXPECT_EQ(bytes(opened.VideoData(frame)), bytes(parsed.VideoData(frame))) << i << " frame " << frame;
				for (uint32_t t = 0; t <= opened.AudioTracks().size(); ++t)
				{
					EXPECT_EQ(bytes(opened.AudioData(frame, t)), bytes(parsed.AudioData(frame, t))) << i << " frame " << frame;
				}
				EXPECT_EQ(bytes(opened.FrameData(frame)), bytes(parsed.FrameData(frame))) << i << " frame " << frame;
			}
		}
		EXPECT_TRUE(opened.FrameData(opened.FrameCount()).empty());
		opened.Close();
		EXPECT_FALSE(opened.IsOpen());
	}
	std::error_code ignored;
	std::filesystem::remove_all(folder, ignored);
}

// Integration test: needs the original game data (OPENBLACK_TEST_GAME_PATH); skipped without it
TEST(BikFile, GameFiles)
{
	const test::RestoreService<Locator::filesystem> restoreFilesystem;
	Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH not set";
	}
	for (const auto& expected : k_GameFiles)
	{
		const auto path = FindNoCase(*root, expected.path);
		ASSERT_TRUE(path.has_value()) << expected.path;
		BikFile file;
		ASSERT_TRUE(file.Open(*path)) << expected.path << ": " << file.GetError();
		EXPECT_EQ(file.Revision(), 'i') << expected.path;
		EXPECT_EQ(file.Width(), expected.width) << expected.path;
		EXPECT_EQ(file.Height(), expected.height) << expected.path;
		EXPECT_EQ(file.FpsNumerator(), expected.fps) << expected.path;
		EXPECT_EQ(file.FpsDenominator(), 1u) << expected.path;
		EXPECT_EQ(file.Fps(), expected.fps) << expected.path;
		EXPECT_EQ(file.FrameCount(), expected.frames) << expected.path;
		EXPECT_EQ(file.AudioTracks().size(), 0u) << expected.path;
		EXPECT_EQ(file.VideoFlags(), 0u) << expected.path;
		EXPECT_EQ(file.Size(), expected.size) << expected.path;
		EXPECT_EQ(file.HeaderFileSize(), expected.size - 8) << expected.path;
		EXPECT_EQ(file.KeyFrameCount(), expected.keyFrames) << expected.path;
		EXPECT_TRUE(file.IsKeyFrame(0)) << expected.path;
		size_t largest = 0;
		for (uint32_t i = 0; i < file.FrameCount(); ++i)
		{
			// no audio: the video is the whole packet
			ASSERT_EQ(file.VideoData(i).size(), file.FrameData(i).size()) << expected.path << " frame " << i;
			ASSERT_FALSE(file.FrameData(i).empty()) << expected.path << " frame " << i;
			largest = std::max(largest, file.FrameData(i).size());
		}
		EXPECT_EQ(largest, file.LargestFrameSize()) << expected.path;
		// the first frame right after the table of frames + 1 offsets, the last offset at the file size
		EXPECT_EQ(file.FrameOffset(0), BikFile::k_HeaderSize + 4 * (static_cast<size_t>(file.FrameCount()) + 1))
		    << expected.path;
		EXPECT_EQ(file.FrameOffset(file.FrameCount()), file.Size()) << expected.path;
	}
}

/// The checks of GameFiles on two small files built here: one with a single key frame (like the intro films) and one
/// where every frame is a key frame (like logo.bik and tips.bik)
TEST(BikFile, GameFilesSynthetic)
{
	struct Synthetic
	{
		const char* name;
		std::vector<std::vector<uint8_t>> packets;
		bool allKeyFrames;
		uint32_t width;
		uint32_t height;
		uint32_t fps;
	};
	const Synthetic files[] = {
	    {"one key frame", {{1, 2, 3, 4}, {5, 6}, {7, 8, 9, 10, 11, 12}}, false, 64, 36, 24},
	    {"all key frames", {{1, 2}, {3, 4, 5, 6}}, true, 32, 16, 15},
	};
	for (const auto& synthetic : files)
	{
		auto data = MakeBik(synthetic.packets, 0, synthetic.width, synthetic.height, synthetic.fps, 1);
		if (synthetic.allKeyFrames)
		{
			// set the key frame bit on every frame's offset (the table follows the header when there is no audio)
			for (size_t i = 0; i < synthetic.packets.size(); ++i)
			{
				data[BikFile::k_HeaderSize + 4 * i] |= static_cast<uint8_t>(BikFile::k_KeyFrameFlag);
			}
		}
		const auto frames = static_cast<uint32_t>(synthetic.packets.size());
		const size_t size = data.size();
		size_t expectedLargest = 0;
		for (const auto& packet : synthetic.packets)
		{
			expectedLargest = std::max(expectedLargest, packet.size());
		}

		BikFile file;
		ASSERT_TRUE(file.Parse(std::move(data))) << synthetic.name << ": " << file.GetError();
		EXPECT_EQ(file.Revision(), 'i') << synthetic.name;
		EXPECT_EQ(file.Width(), synthetic.width) << synthetic.name;
		EXPECT_EQ(file.Height(), synthetic.height) << synthetic.name;
		EXPECT_EQ(file.FpsNumerator(), synthetic.fps) << synthetic.name;
		EXPECT_EQ(file.FpsDenominator(), 1u) << synthetic.name;
		EXPECT_EQ(file.Fps(), synthetic.fps) << synthetic.name;
		EXPECT_EQ(file.FrameCount(), frames) << synthetic.name;
		EXPECT_EQ(file.AudioTracks().size(), 0u) << synthetic.name;
		EXPECT_EQ(file.VideoFlags(), 0u) << synthetic.name;
		EXPECT_EQ(file.Size(), size) << synthetic.name;
		EXPECT_EQ(file.HeaderFileSize(), size - 8) << synthetic.name;
		EXPECT_EQ(file.KeyFrameCount(), synthetic.allKeyFrames ? frames : 1u) << synthetic.name;
		EXPECT_TRUE(file.IsKeyFrame(0)) << synthetic.name;
		size_t largest = 0;
		for (uint32_t i = 0; i < file.FrameCount(); ++i)
		{
			EXPECT_EQ(file.IsKeyFrame(i), synthetic.allKeyFrames || i == 0) << synthetic.name << " frame " << i;
			// no audio: the video is the whole packet, and the key frame bit does not change the packet's bounds
			ASSERT_EQ(file.VideoData(i).size(), file.FrameData(i).size()) << synthetic.name << " frame " << i;
			ASSERT_EQ(file.FrameData(i).size(), synthetic.packets[i].size()) << synthetic.name << " frame " << i;
			largest = std::max(largest, file.FrameData(i).size());
		}
		EXPECT_EQ(largest, expectedLargest) << synthetic.name;
		EXPECT_EQ(largest, file.LargestFrameSize()) << synthetic.name;
		// the first frame right after the table of frames + 1 offsets, the last offset at the file size
		EXPECT_EQ(file.FrameOffset(0), BikFile::k_HeaderSize + 4 * (static_cast<size_t>(frames) + 1)) << synthetic.name;
		EXPECT_EQ(file.FrameOffset(frames), file.Size()) << synthetic.name;
	}
}

// Integration test: needs the original game data (OPENBLACK_TEST_GAME_PATH); skipped without it
TEST(BikFile, IntroSizes)
{
	const test::RestoreService<Locator::filesystem> restoreFilesystem;
	Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH not set";
	}
	const auto path = FindNoCase(*root, "Data/INTRO.bik");
	ASSERT_TRUE(path.has_value());
	BikFile file;
	ASSERT_TRUE(file.Open(*path)) << file.GetError();
	// the smallest packet 100 bytes, the largest 31656; the first frame at 0x1934
	size_t smallest = SIZE_MAX;
	for (uint32_t i = 0; i < file.FrameCount(); ++i)
	{
		smallest = std::min(smallest, file.FrameData(i).size());
	}
	EXPECT_EQ(smallest, 100u);
	EXPECT_EQ(file.LargestFrameSize(), 31656u);
	EXPECT_EQ(file.FrameOffset(0), 0x1934u);
}
