/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The Bink 1 container: synthetic files for the layout and every check, and the game's five videos when
// OPENBLACK_GAME_PATH names the game's folder (those tests skip without it).

#include <cstdint>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <BinkFile.h>
#include <gtest/gtest.h>

#include "BinkGolden.h"
#include "BinkTestFiles.h"

using namespace openblack::bink;
using namespace openblack::bink::test;

namespace
{
std::vector<std::vector<uint8_t>> Packets(std::initializer_list<size_t> sizes)
{
	std::vector<std::vector<uint8_t>> packets;
	uint8_t value = 1;
	for (const size_t size : sizes)
	{
		packets.emplace_back(size, value++);
	}
	return packets;
}

std::string ParseError(std::vector<uint8_t> data)
{
	std::string error;
	EXPECT_FALSE(BinkFile::Parse(std::move(data), &error).has_value());
	return error;
}
} // namespace

TEST(BinkFile, ReadsTheLayout)
{
	const auto file = BinkFile::Parse(MakeBik(Packets({8, 4, 6}), {.width = 640, .height = 360, .keyFrames = {0, 2}}));
	ASSERT_TRUE(file.has_value());
	const auto& header = file->GetHeader();
	EXPECT_EQ(header.revision, 'i');
	EXPECT_EQ(file->FrameCount(), 3u);
	EXPECT_EQ(file->Width(), 640u);
	EXPECT_EQ(file->Height(), 360u);
	EXPECT_EQ(header.largestPacket, 8u);
	EXPECT_EQ(header.fileSizeLess8, file->Size() - 8);
	EXPECT_TRUE(file->AudioTracks().empty());
	EXPECT_TRUE(file->IsKeyFrame(0));
	EXPECT_FALSE(file->IsKeyFrame(1));
	EXPECT_TRUE(file->IsKeyFrame(2));
	EXPECT_FALSE(file->IsKeyFrame(3));
	EXPECT_EQ(file->KeyFrameCount(), 2u);
	EXPECT_EQ(file->KeyFrameAtOrBefore(1), 0u);
	EXPECT_EQ(file->KeyFrameAtOrBefore(2), 2u);
	EXPECT_EQ(file->KeyFrameAtOrBefore(99), 2u);
	const size_t tables = BinkFile::k_HeaderSize + 4 * 4;
	EXPECT_EQ(file->FrameOffset(0), tables);
	EXPECT_EQ(file->FrameOffset(3), file->Size());
	EXPECT_EQ(file->VideoPacket(0).size(), 8u);
	EXPECT_EQ(file->VideoPacket(1).size(), 4u);
	EXPECT_EQ(file->VideoPacket(1)[0], 2);
	EXPECT_TRUE(file->VideoPacket(3).empty());
	EXPECT_TRUE(file->FramePacket(3).empty());
}

TEST(BinkFile, FrameRateIsRoundedDown)
{
	// 30000 / 1001 is 29.97 frames a second, played at 29
	const auto file = BinkFile::Parse(MakeBik(Packets({4}), {.fpsNumerator = 30000, .fpsDenominator = 1001}));
	ASSERT_TRUE(file.has_value());
	EXPECT_EQ(file->IntegerFps(), 29u);
}

TEST(BinkFile, SplitsTheAudioPackets)
{
	// Two tracks: frame 0 has 4 bytes of the first and 2 of the second, then 6 bytes of video
	std::vector<uint8_t> packet = {4, 0, 0, 0, 0xA, 0xA, 0xA, 0xA, 2, 0, 0, 0, 0xB, 0xB, 1, 2, 3, 4, 5, 6};
	const auto file = BinkFile::Parse(MakeBik({packet}, {.audioTracks = 2}));
	ASSERT_TRUE(file.has_value());
	ASSERT_EQ(file->AudioTracks().size(), 2u);
	EXPECT_EQ(file->AudioTracks()[1].sampleRate, 22051);
	EXPECT_EQ(file->AudioTracks()[1].id, 8u);
	EXPECT_EQ(file->AudioPacket(0, 0).size(), 4u);
	EXPECT_EQ(file->AudioPacket(0, 1).size(), 2u);
	EXPECT_EQ(file->AudioPacket(0, 1)[0], 0xB);
	EXPECT_TRUE(file->AudioPacket(0, 2).empty());
	ASSERT_EQ(file->VideoPacket(0).size(), 6u);
	EXPECT_EQ(file->VideoPacket(0)[0], 1);
}

TEST(BinkFile, RejectsBadFiles)
{
	const auto good = MakeBik(Packets({4, 4}));
	ASSERT_TRUE(BinkFile::Parse(good).has_value());

	EXPECT_NE(ParseError({}).find("shorter"), std::string::npos);
	EXPECT_NE(ParseError(std::vector<uint8_t>(good.begin(), good.begin() + 43)).find("shorter"), std::string::npos);

	auto bink2 = good;
	bink2[0] = 'K';
	bink2[1] = 'B';
	bink2[2] = '2';
	EXPECT_NE(ParseError(bink2).find("signature"), std::string::npos);

	auto noFrames = good;
	PutU32(noFrames, 8, 0);
	EXPECT_NE(ParseError(noFrames).find("no frames"), std::string::npos);

	auto noPicture = good;
	PutU32(noPicture, 24, 0);
	EXPECT_NE(ParseError(noPicture).find("no picture"), std::string::npos);

	auto noRate = good;
	PutU32(noRate, 32, 0);
	EXPECT_NE(ParseError(noRate).find("frame rate"), std::string::npos);

	// Counts far too big for the file must not wrap around
	auto hugeFrames = good;
	PutU32(hugeFrames, 8, 0xFFFFFFFF);
	EXPECT_NE(ParseError(hugeFrames).find("past the"), std::string::npos);
	auto hugeTracks = good;
	PutU32(hugeTracks, 40, 0xFFFFFFFF);
	EXPECT_NE(ParseError(hugeTracks).find("past the"), std::string::npos);

	auto insideTables = good;
	PutU32(insideTables, BinkFile::k_HeaderSize, 8);
	EXPECT_NE(ParseError(insideTables).find("inside the tables"), std::string::npos);

	auto backwards = good;
	PutU32(backwards, BinkFile::k_HeaderSize + 4, static_cast<uint32_t>(BinkFile::k_HeaderSize + 12));
	EXPECT_NE(ParseError(backwards).find("is not before"), std::string::npos);

	auto shortTable = good;
	PutU32(shortTable, BinkFile::k_HeaderSize + 8, static_cast<uint32_t>(good.size() - 2));
	EXPECT_NE(ParseError(shortTable).find("ends at"), std::string::npos);

	// An audio size reaching past its packet
	std::vector<uint8_t> packet = {200, 0, 0, 0, 1, 2};
	EXPECT_NE(ParseError(MakeBik({packet}, {.audioTracks = 1})).find("past the packet"), std::string::npos);
	std::vector<uint8_t> tooSmall = {1, 2};
	EXPECT_NE(ParseError(MakeBik({tooSmall}, {.audioTracks = 1})).find("no room"), std::string::npos);
}

TEST(BinkFile, EveryTruncationIsRejected)
{
	const auto good = MakeBik(Packets({6, 4, 8}), {.audioTracks = 1});
	for (size_t size = 0; size < good.size(); ++size)
	{
		EXPECT_FALSE(BinkFile::Parse(std::vector<uint8_t>(good.begin(), good.begin() + static_cast<ptrdiff_t>(size)))) << size;
	}
}

TEST(BinkFile, OpenReadsWhatParseReads)
{
	const auto data = MakeBik(Packets({4, 6}));
	const auto path = std::filesystem::temp_directory_path() / "openblack_test_bink_file.bik";
	{
		std::ofstream stream(path, std::ios::binary);
		stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
	}
	const auto opened = BinkFile::Open(path);
	std::filesystem::remove(path);
	ASSERT_TRUE(opened.has_value());
	EXPECT_EQ(opened->FrameCount(), 2u);
	EXPECT_EQ(opened->VideoPacket(1).size(), 6u);

	std::string error;
	EXPECT_FALSE(BinkFile::Open(path, &error).has_value());
	EXPECT_NE(error.find("cannot open"), std::string::npos);
}

TEST(BinkFile, TheGamesVideos)
{
	const auto game = GamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	struct Expected
	{
		uint32_t width;
		uint32_t height;
		uint32_t fps;
		uint32_t keyFrames;
	};
	constexpr std::array<Expected, 5> k_Expected = {{
	    {768, 512, 15, 2},  // logo
	    {768, 512, 15, 35}, // tips
	    {640, 360, 24, 1},  // INTRO
	    {640, 360, 24, 1},  // fall
	    {768, 512, 25, 1},  // pre_intro
	}};
	for (size_t i = 0; i < golden::k_Videos.size(); ++i)
	{
		const auto& video = golden::k_Videos[i];
		const auto path = *game / video.path;
		if (!std::filesystem::exists(path))
		{
			GTEST_SKIP() << path << " not found";
		}
		std::string error;
		const auto file = BinkFile::Open(path, &error);
		ASSERT_TRUE(file.has_value()) << path << ": " << error;
		EXPECT_EQ(file->GetHeader().revision, 'i');
		EXPECT_EQ(file->FrameCount(), video.frameCount);
		EXPECT_EQ(file->Width(), k_Expected[i].width);
		EXPECT_EQ(file->Height(), k_Expected[i].height);
		EXPECT_EQ(file->IntegerFps(), k_Expected[i].fps);
		EXPECT_EQ(file->KeyFrameCount(), k_Expected[i].keyFrames);
		EXPECT_TRUE(file->AudioTracks().empty());
		EXPECT_EQ(file->GetHeader().videoFlags & BinkFile::k_VideoFlagAlpha, 0u);
	}
}
