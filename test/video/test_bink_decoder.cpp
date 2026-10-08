/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// video::BinkDecoder (src/Video/BinkDecoder.h): synthetic 16x16 films written bit by bit, and the game's five films
// against the golden frames of the original's Bink library (BinkGolden.h) when OPENBLACK_GAME_PATH is the game's
// folder; without it those tests skip.

#include <cctype>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "BinkGolden.h"
// the tests build their own file system
#define LOCATOR_IMPLEMENTATIONS
#include "FileSystem/DefaultFileSystem.h"
#include "Locator.h"
#include "Video/BikFile.h"
#include "Video/BinkBundles.h"
#include "Video/BinkDecoder.h"
#include "Video/BinkYuv.h"
#include "support/BinkBitWriter.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::video;
using namespace openblack::test::bink_golden;
using bink::BlockType;
using openblack::test::BinkBitWriter;

namespace
{
void PutU32(std::vector<uint8_t>& data, size_t at, uint32_t v)
{
	for (size_t i = 0; i < 4; ++i)
	{
		data[at + i] = static_cast<uint8_t>(v >> (8 * i));
	}
}

/// A 16x16 Bink 1 film of `packets` (frame 0 the key frame, no audio)
std::vector<uint8_t> MakeBik(const std::vector<std::vector<uint8_t>>& packets)
{
	const auto frames = static_cast<uint32_t>(packets.size());
	const size_t frameTable = BikFile::k_HeaderSize;
	std::vector<uint8_t> data(frameTable + 4 * (static_cast<size_t>(frames) + 1), 0);
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
	PutU32(data, 20, 16);
	PutU32(data, 24, 16);
	PutU32(data, 28, 24);
	PutU32(data, 32, 1);
	return data;
}

/// Writes the planes of a 16x16 frame: luma 2x2 blocks, each chroma plane 1 block. All trees are tree 0 (plain 4-bit
/// symbols); every chunk count is 10 bits except the chroma's sub-block types (9)
class Frame
{
public:
	Frame() { _w.Put(0, 32); }

	/// Starts a plane: its trees
	Frame& Plane(bool chroma)
	{
		_chroma = chroma;
		for (int i = 0; i < 7 + 16; ++i)
		{
			_w.Put(0, 4);
		}
		return *this;
	}
	/// `count` copies of `value` (block types, runs)
	Frame& Repeat(uint32_t count, uint32_t value)
	{
		_w.Put(count, 10).Bit(true).Put(value, 4);
		return *this;
	}
	/// `count` block types coded one by one
	Frame& Types(std::initializer_list<BlockType> types)
	{
		_w.Put(static_cast<uint32_t>(types.size()), 10).Bit(false);
		for (const auto type : types)
		{
			_w.Put(static_cast<uint32_t>(type), 4);
		}
		return *this;
	}
	/// `count` copies of a colour
	Frame& Colour(uint32_t count, uint8_t colour)
	{
		_w.Put(count, 10).Bit(true).Put(colour >> 4, 4).Put(colour & 15, 4);
		return *this;
	}
	/// Colours one by one
	Frame& Colours(std::span<const uint8_t> colours)
	{
		_w.Put(static_cast<uint32_t>(colours.size()), 10).Bit(false);
		for (const uint8_t c : colours)
		{
			_w.Put(c >> 4, 4).Put(c & 15, 4);
		}
		return *this;
	}
	/// `count` copies of a motion offset
	Frame& Offset(uint32_t count, int32_t offset)
	{
		_w.Put(count, 10).Bit(true).Put(static_cast<uint32_t>(std::abs(offset)), 4);
		if (offset != 0)
		{
			_w.Bit(offset < 0);
		}
		return *this;
	}
	/// A bundle with nothing more in this plane
	Frame& End() { return EndSub(false); }
	/// The sub-block types' end, 9 bits in a chroma plane
	Frame& EndSub(bool sub = true)
	{
		_w.Put(0, sub && _chroma ? 9 : 10);
		return *this;
	}
	Frame& Ends(int count)
	{
		for (int i = 0; i < count; ++i)
		{
			End();
		}
		return *this;
	}
	Frame& EndPlane()
	{
		_w.Align32();
		return *this;
	}
	/// A whole plane of one fill colour: luma two rows of two blocks, chroma one block
	Frame& FillPlane(bool chroma, uint8_t colour)
	{
		Plane(chroma);
		const uint32_t blocks = chroma ? 1 : 2;
		Repeat(blocks, static_cast<uint32_t>(BlockType::Fill)).EndSub().Colour(blocks, colour).Ends(6);
		if (!chroma)
		{
			Repeat(2, static_cast<uint32_t>(BlockType::Fill)).Colour(2, colour);
		}
		return EndPlane();
	}
	/// A whole plane of skip blocks
	Frame& SkipPlane(bool chroma)
	{
		Plane(chroma);
		Repeat(chroma ? 1 : 2, static_cast<uint32_t>(BlockType::Skip)).EndSub().Ends(7);
		if (!chroma)
		{
			Repeat(2, static_cast<uint32_t>(BlockType::Skip));
		}
		return EndPlane();
	}

	[[nodiscard]] std::vector<uint8_t> Bytes() const { return _w.Bytes(); }

private:
	BinkBitWriter _w;
	bool _chroma {false};
};

/// A frame of one colour: Y, then V, then U
std::vector<uint8_t> FillFrame(uint8_t y, uint8_t u, uint8_t v)
{
	return Frame().FillPlane(false, y).FillPlane(true, v).FillPlane(true, u).Bytes();
}

std::vector<uint8_t> SkipFrame()
{
	return Frame().SkipPlane(false).SkipPlane(true).SkipPlane(true).Bytes();
}

/// Block (0, 0) raw with the values 0..63, the other three filled with 50; grey chroma
std::vector<uint8_t> RawFrame()
{
	std::vector<uint8_t> colours(64);
	std::iota(colours.begin(), colours.end(), uint8_t {0});
	colours.push_back(50);
	Frame f;
	f.Plane(false).Types({BlockType::Raw, BlockType::Fill}).EndSub().Colours(colours).Ends(6);
	f.Repeat(2, static_cast<uint32_t>(BlockType::Fill)).Colour(2, 50).EndPlane();
	return f.FillPlane(true, 128).FillPlane(true, 128).Bytes();
}

/// Block (1, 0) moved from 8 pixels to its left in the previous frame, the rest skipped
std::vector<uint8_t> MotionFrame(int32_t dx)
{
	Frame f;
	f.Plane(false).Types({BlockType::Skip, BlockType::Motion}).EndSub().Ends(2).Offset(1, dx).Offset(1, 0).Ends(3);
	// The offsets have been taken: they read their next chunk, empty
	f.Repeat(2, static_cast<uint32_t>(BlockType::Skip)).Ends(2).EndPlane();
	return f.SkipPlane(true).SkipPlane(true).Bytes();
}

BikFile Parse(const std::vector<std::vector<uint8_t>>& packets)
{
	BikFile file;
	EXPECT_TRUE(file.Parse(MakeBik(packets))) << file.GetError();
	return file;
}

void ExpectColour(std::span<const uint8_t> rgba, uint8_t y, uint8_t u, uint8_t v)
{
	ASSERT_EQ(rgba.size(), size_t {16} * 16 * 4);
	const auto c = bink_yuv::ToRgb(y, u, v);
	for (size_t i = 0; i < rgba.size(); i += 4)
	{
		ASSERT_EQ(rgba[i + 0], c.r) << i / 4;
		ASSERT_EQ(rgba[i + 1], c.g) << i / 4;
		ASSERT_EQ(rgba[i + 2], c.b) << i / 4;
		ASSERT_EQ(rgba[i + 3], 0xFF) << i / 4;
	}
}
} // namespace

TEST(BinkDecoder, RefusesNoFilm)
{
	BinkDecoder decoder;
	const BikFile file;
	EXPECT_FALSE(decoder.Open(file));
	EXPECT_FALSE(decoder.GetError().empty());
	EXPECT_TRUE(decoder.DecodeNext(0).empty());
}

TEST(BinkDecoder, RefusesOtherRevisionsAndAlpha)
{
	auto data = MakeBik({FillFrame(80, 100, 160)});
	data[3] = 'h';
	BikFile older;
	ASSERT_TRUE(older.Parse(data));
	BinkDecoder decoder;
	EXPECT_FALSE(decoder.Open(older));
	data[3] = 'i';
	PutU32(data, 36, BikFile::k_VideoFlagAlpha);
	BikFile alpha;
	ASSERT_TRUE(alpha.Parse(data));
	EXPECT_FALSE(decoder.Open(alpha));
}

TEST(BinkDecoder, FillFrameAndPlaneOrder)
{
	const auto file = Parse({FillFrame(80, 100, 160)});
	BinkDecoder decoder;
	ASSERT_TRUE(decoder.Open(file)) << decoder.GetError();
	const auto rgba = decoder.DecodeNext(0);
	ExpectColour(rgba, 80, 100, 160);
	// The packet holds V before U
	EXPECT_EQ(decoder.PlanePixels(1)[0], 100);
	EXPECT_EQ(decoder.PlanePixels(2)[0], 160);
	EXPECT_TRUE(decoder.DecodeNext(1).empty()); // past the last frame
}

TEST(BinkDecoder, SkipKeepsThePreviousPicture)
{
	const auto file = Parse({FillFrame(80, 100, 160), SkipFrame()});
	BinkDecoder decoder;
	ASSERT_TRUE(decoder.Open(file));
	ASSERT_FALSE(decoder.DecodeNext(0).empty());
	ExpectColour(decoder.DecodeNext(1), 80, 100, 160);
}

TEST(BinkDecoder, RawAndMotion)
{
	const auto file = Parse({RawFrame(), MotionFrame(-8)});
	BinkDecoder decoder;
	ASSERT_TRUE(decoder.Open(file));
	ASSERT_FALSE(decoder.DecodeNext(0).empty()) << decoder.GetError();
	const auto y = decoder.PlanePixels(0);
	const size_t stride = decoder.PlaneStride(0);
	EXPECT_EQ(y[7 * stride + 7], 63);
	EXPECT_EQ(y[8], 50);
	EXPECT_EQ(y[15 * stride + 15], 50);
	ASSERT_FALSE(decoder.DecodeNext(1).empty()) << decoder.GetError();
	const auto moved = decoder.PlanePixels(0);
	for (size_t row = 0; row < 8; ++row)
	{
		for (size_t x = 0; x < 8; ++x)
		{
			EXPECT_EQ(moved[row * stride + 8 + x], row * 8 + x) << row << "," << x;
			EXPECT_EQ(moved[row * stride + x], row * 8 + x) << row << "," << x;
		}
	}
	EXPECT_EQ(moved[8 * stride], 50);
}

TEST(BinkDecoder, ADamagedFrameKeepsTheReference)
{
	// Frame 1 moves block (1, 0) from 15 pixels to its left, 7 before the plane: it fails, and frame 2 skips from
	// frame 0
	const auto file = Parse({RawFrame(), MotionFrame(-15), SkipFrame()});
	BinkDecoder decoder;
	ASSERT_TRUE(decoder.Open(file));
	ASSERT_FALSE(decoder.DecodeNext(0).empty());
	EXPECT_TRUE(decoder.DecodeNext(1).empty());
	EXPECT_FALSE(decoder.GetError().empty());
	ASSERT_FALSE(decoder.DecodeNext(2).empty());
	const auto y = decoder.PlanePixels(0);
	EXPECT_EQ(y[7 * decoder.PlaneStride(0) + 7], 63);
	EXPECT_EQ(y[8], 50);
}

TEST(BinkDecoder, GoesBackToTheKeyFrame)
{
	const auto file = Parse({FillFrame(80, 100, 160), SkipFrame(), SkipFrame()});
	BinkDecoder decoder;
	ASSERT_TRUE(decoder.Open(file));
	// Straight to frame 2: frames 0 and 1 are decoded first
	ExpectColour(decoder.DecodeNext(2), 80, 100, 160);
	// Back to frame 1: again from frame 0
	ExpectColour(decoder.DecodeNext(1), 80, 100, 160);
}

namespace
{
std::string Lower(std::string s)
{
	for (auto& c : s)
	{
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return s;
}

// The original runs on a case-insensitive file system
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

/// Decodes `film` of the game in order up to each golden frame and checks its CRC
void ExpectGoldenInOrder(std::string_view film, std::span<const Golden> goldens)
{
	const test::RestoreService<Locator::filesystem> restoreFilesystem;
	Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	ASSERT_NE(game, nullptr);
	const auto path = FindNoCase(game, film);
	ASSERT_TRUE(path.has_value()) << film;
	BikFile file;
	ASSERT_TRUE(file.Open(*path)) << file.GetError();
	BinkDecoder decoder;
	ASSERT_TRUE(decoder.Open(file)) << decoder.GetError();
	uint32_t next = 0;
	for (const auto& golden : goldens)
	{
		std::span<const uint8_t> rgba;
		for (; next <= golden.frame; ++next)
		{
			rgba = decoder.DecodeNext(next);
			ASSERT_FALSE(rgba.empty()) << film << " frame " << next << ": " << decoder.GetError();
		}
		EXPECT_EQ(Crc555(rgba), golden.crc) << film << " frame " << golden.frame;
	}
}
} // namespace

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(BinkDecoder, IntroMatchesTheOriginal)
{
	if (std::getenv("OPENBLACK_GAME_PATH") == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	ExpectGoldenInOrder("Data/INTRO.bik", k_Intro);
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(BinkDecoder, FallMatchesTheOriginal)
{
	if (std::getenv("OPENBLACK_GAME_PATH") == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	ExpectGoldenInOrder("Data/Spells/fall/fall.bik", k_Fall);
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(BinkDecoder, PreIntroMatchesTheOriginal)
{
	if (std::getenv("OPENBLACK_GAME_PATH") == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	ExpectGoldenInOrder("Data/pre_intro.bik", k_PreIntro);
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(BinkDecoder, LogoAndTipsMatchTheOriginalInAnyOrder)
{
	const test::RestoreService<Locator::filesystem> restoreFilesystem;
	Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto logoPath = FindNoCase(game, "Data/logo.bik");
	ASSERT_TRUE(logoPath.has_value());
	BikFile logo;
	ASSERT_TRUE(logo.Open(*logoPath)) << logo.GetError();
	BinkDecoder decoder;
	ASSERT_TRUE(decoder.Open(logo)) << decoder.GetError();
	for (const auto& golden : k_Logo)
	{
		EXPECT_EQ(Crc555(decoder.DecodeNext(golden.frame)), golden.crc) << "logo.bik frame " << golden.frame;
	}
	EXPECT_TRUE(decoder.DecodeNext(2).empty());
	// Back to frame 0 (logo.bik: both frames are key frames)
	EXPECT_EQ(Crc555(decoder.DecodeNext(0)), k_Logo[0].crc);

	const auto tipsPath = FindNoCase(game, "Data/tips.bik");
	ASSERT_TRUE(tipsPath.has_value());
	BikFile tips;
	ASSERT_TRUE(tips.Open(*tipsPath)) << tips.GetError();
	ASSERT_TRUE(decoder.Open(tips)) << decoder.GetError();
	// The tip video goes straight to a frame, backwards and forwards
	for (const auto& golden : k_Tips)
	{
		EXPECT_EQ(Crc555(decoder.DecodeNext(golden.frame)), golden.crc) << "tips.bik frame " << golden.frame;
	}
	EXPECT_EQ(Crc555(decoder.DecodeNext(k_Tips0.frame)), k_Tips0.crc);
}
