/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The Bink 1 decoder: synthetic 16x16 videos written bit by bit, and, when OPENBLACK_GAME_PATH names the game's
// folder, every frame of the game's five videos against checksums of the game's own Bink library (BinkGolden.h).

#include <cstdint>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <BinkDecoder.h>
#include <BinkFile.h>
#include <BinkYuv.h>
#include <gtest/gtest.h>

#include "BinkGolden.h"
#include "BinkTestFiles.h"
#include "Crc32.h"

using namespace openblack::bink;
using namespace openblack::bink::test;

namespace
{
std::shared_ptr<const BinkFile> Make(const std::vector<std::vector<uint8_t>>& packets, const BikOptions& options = {})
{
	auto file = BinkFile::Parse(MakeBik(packets, options));
	EXPECT_TRUE(file.has_value());
	return std::make_shared<const BinkFile>(std::move(*file));
}

FrameReader Reader(const std::vector<std::vector<uint8_t>>& packets, const BikOptions& options = {})
{
	auto reader = FrameReader::Create(Make(packets, options));
	EXPECT_TRUE(reader.has_value());
	return std::move(*reader);
}

/// Every visible pixel of a plane is `value`
void ExpectPlane(const PlaneView& plane, uint8_t value)
{
	for (uint32_t row = 0; row < plane.height; ++row)
	{
		for (uint32_t x = 0; x < plane.width; ++x)
		{
			ASSERT_EQ(plane.pixels[row * plane.stride + x], value) << x << "," << row;
		}
	}
}

void ExpectFill(const Picture& picture, uint8_t y, uint8_t u, uint8_t v)
{
	ExpectPlane(picture.y, y);
	ExpectPlane(picture.u, u);
	ExpectPlane(picture.v, v);
}
} // namespace

TEST(BinkDecoder, RefusesWhatTheGameNeverPlays)
{
	std::string error;
	EXPECT_FALSE(Decoder::Create({.revision = 'i', .width = 0, .height = 16}, &error).has_value());
	EXPECT_FALSE(Decoder::Create({.revision = 'h', .width = 16, .height = 16}, &error).has_value());
	EXPECT_NE(error.find("revision"), std::string::npos);
	EXPECT_FALSE(Decoder::Create({.revision = 'i', .width = 16, .height = 16, .videoFlags = BinkFile::k_VideoFlagAlpha}, &error)
	                 .has_value());
	EXPECT_NE(error.find("alpha"), std::string::npos);
	EXPECT_FALSE(FrameReader::Create(nullptr).has_value());
}

TEST(BinkDecoder, PlaneSizesArePaddedToWholeBlocks)
{
	const auto decoder = Decoder::Create({.revision = 'i', .width = 640, .height = 360});
	ASSERT_TRUE(decoder.has_value());
	const auto picture = decoder->GetPicture();
	EXPECT_EQ(picture.y.width, 640u);
	EXPECT_EQ(picture.y.height, 360u);
	EXPECT_EQ(picture.u.width, 320u);
	EXPECT_EQ(picture.u.height, 180u);
	// Luma 80 x 45 blocks kept as 80 x 46; chroma 40 x 23 blocks on the luma's 16x16 grid kept as 40 x 24
	EXPECT_EQ(picture.y.stride, 640u);
	EXPECT_EQ(picture.y.pixels.size(), 640u * 368);
	EXPECT_EQ(picture.v.stride, 320u);
	EXPECT_EQ(picture.v.pixels.size(), 320u * 192);
	EXPECT_FALSE(decoder->HasPicture());
	ExpectPlane(picture.y, 0);
}

TEST(BinkDecoder, FillFrameAndPlaneOrder)
{
	auto reader = Reader({FillFrame(200, 70, 30)});
	ASSERT_TRUE(reader.DecodeFrame(0));
	// Coded Y, then V, then U
	ExpectFill(reader.GetPicture(), 200, 70, 30);
	std::vector<uint8_t> rgba(16 * 16 * 4);
	ConvertPicture(reader.GetPicture(), PixelLayout::Rgba8, rgba);
	const auto colour = ToRgb(200, 70, 30);
	EXPECT_EQ(rgba[0], colour.r);
	EXPECT_EQ(rgba[1], colour.g);
	EXPECT_EQ(rgba[2], colour.b);
	EXPECT_EQ(rgba[3], 0xFF);
}

TEST(BinkDecoder, SkipKeepsThePreviousPicture)
{
	auto reader = Reader({FillFrame(90, 100, 110), SkipFrame(), SkipFrame()});
	ASSERT_TRUE(reader.DecodeFrame(0));
	ASSERT_TRUE(reader.DecodeFrame(1));
	ASSERT_TRUE(reader.DecodeFrame(2));
	ExpectFill(reader.GetPicture(), 90, 100, 110);
}

TEST(BinkDecoder, TheFirstFrameIsItsOwnReference)
{
	// Skip blocks before any picture copy the picture being decoded: still black
	auto reader = Reader({SkipFrame()});
	ASSERT_TRUE(reader.DecodeFrame(0));
	ExpectFill(reader.GetPicture(), 0, 0, 0);
}

TEST(BinkDecoder, RawAndMotion)
{
	auto reader = Reader({RawFrame(), MotionFrame(-8), MotionFrame(-9)});
	ASSERT_TRUE(reader.DecodeFrame(0));
	const auto y = reader.GetPicture().y;
	for (size_t i = 0; i < 64; ++i)
	{
		ASSERT_EQ(y.pixels[(i / 8) * y.stride + i % 8], i);
	}
	EXPECT_EQ(y.pixels[8], 50);
	// Block (1, 0) takes the raw block from 8 pixels to its left
	ASSERT_TRUE(reader.DecodeFrame(1));
	const auto moved = reader.GetPicture().y;
	for (size_t i = 0; i < 64; ++i)
	{
		ASSERT_EQ(moved.pixels[(i / 8) * moved.stride + 8 + i % 8], i);
	}
	// A vector reaching before the plane's start is damaged data: the frame fails and the picture stays
	EXPECT_FALSE(reader.DecodeFrame(2));
	EXPECT_EQ(reader.GetPicture().y.pixels[8], 0);
}

TEST(BinkDecoder, ADamagedFrameKeepsTheReference)
{
	auto broken = FillFrame(10, 20, 30);
	broken.resize(8);
	auto reader = Reader({FillFrame(90, 100, 110), broken, SkipFrame()});
	ASSERT_TRUE(reader.DecodeFrame(0));
	EXPECT_FALSE(reader.DecodeFrame(1));
	ExpectFill(reader.GetPicture(), 90, 100, 110);
	ASSERT_TRUE(reader.DecodeFrame(2));
	ExpectFill(reader.GetPicture(), 90, 100, 110);
}

TEST(BinkDecoder, ChromaStartsWhereThePacketSays)
{
	// Padding between the luma and the chroma planes, with the packet's leading offset past it
	auto packet = FillFrame(40, 50, 60);
	const uint32_t chroma = static_cast<uint32_t>(packet[0]) | static_cast<uint32_t>(packet[1]) << 8;
	packet.insert(packet.begin() + chroma, 8, 0xFF);
	PutU32(packet, 0, chroma + 8);
	auto reader = Reader({packet});
	ASSERT_TRUE(reader.DecodeFrame(0));
	ExpectFill(reader.GetPicture(), 40, 50, 60);

	// An offset inside the field itself or past the packet is damage
	auto inside = FillFrame(40, 50, 60);
	PutU32(inside, 0, 2);
	auto past = FillFrame(40, 50, 60);
	PutU32(past, 0, static_cast<uint32_t>(past.size() + 1));
	auto damaged = Reader({inside, past});
	EXPECT_FALSE(damaged.DecodeFrame(0));
	EXPECT_FALSE(damaged.DecodeFrame(1));
}

TEST(BinkDecoder, GoesBackToTheKeyFrame)
{
	const std::vector<std::vector<uint8_t>> packets = {FillFrame(10, 11, 12), FillFrame(20, 21, 22), SkipFrame(),
	                                                   FillFrame(40, 41, 42), SkipFrame()};
	auto reader = Reader(packets, {.keyFrames = {0, 3}});
	ASSERT_TRUE(reader.DecodeFrame(2));
	EXPECT_EQ(reader.NextFrame(), 3u);
	ExpectFill(reader.GetPicture(), 20, 21, 22);
	ASSERT_TRUE(reader.DecodeFrame(4));
	ExpectFill(reader.GetPicture(), 40, 41, 42);
	ASSERT_TRUE(reader.DecodeFrame(1));
	ExpectFill(reader.GetPicture(), 20, 21, 22);
	ASSERT_TRUE(reader.DecodeFrame(0));
	ExpectFill(reader.GetPicture(), 10, 11, 12);
	// Again the same frame decodes it from its key frame
	ASSERT_TRUE(reader.DecodeFrame(0));
	ExpectFill(reader.GetPicture(), 10, 11, 12);
	EXPECT_FALSE(reader.DecodeFrame(5));
	ExpectFill(reader.GetPicture(), 10, 11, 12);
}

// Every frame of the game's five videos, in order, as the game's own Bink library decodes them; then the logo and the
// tips in an order going backwards and forwards, as the loading screen asks for them
TEST(BinkDecoder, MatchesTheGamesLibraryOnEveryFrame)
{
	const auto game = GamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	for (const auto& video : golden::k_Videos)
	{
		const auto path = *game / video.path;
		if (!std::filesystem::exists(path))
		{
			GTEST_SKIP() << path << " not found";
		}
		auto file = BinkFile::Open(path);
		ASSERT_TRUE(file.has_value()) << path;
		ASSERT_EQ(file->FrameCount(), video.frameCount);
		auto reader = FrameReader::Create(std::make_shared<const BinkFile>(std::move(*file)));
		ASSERT_TRUE(reader.has_value());
		std::vector<uint8_t> bgrx(static_cast<size_t>(reader->GetFile().Width()) * reader->GetFile().Height() * 4);
		Crc32 digest;
		auto expected = video.frames.begin();
		for (uint32_t frame = 0; frame < video.frameCount; ++frame)
		{
			ASSERT_TRUE(reader->DecodeFrame(frame)) << video.path << " frame " << frame;
			const auto picture = reader->GetPicture();
			ConvertPicture(picture, PixelLayout::Bgrx8, bgrx);
			const uint32_t pictureCrc = Crc32().Add(bgrx).Value();
			const uint32_t y = PlaneCrc(picture.y);
			const uint32_t u = PlaneCrc(picture.u);
			const uint32_t v = PlaneCrc(picture.v);
			digest.AddU32(pictureCrc).AddU32(y).AddU32(u).AddU32(v);
			if (expected != video.frames.end() && expected->frame == frame)
			{
				EXPECT_EQ(pictureCrc, expected->picture) << video.path << " frame " << frame;
				EXPECT_EQ(y, expected->y) << video.path << " frame " << frame;
				EXPECT_EQ(u, expected->u) << video.path << " frame " << frame;
				EXPECT_EQ(v, expected->v) << video.path << " frame " << frame;
				++expected;
			}
		}
		EXPECT_EQ(digest.Value(), video.digest) << video.path;

		if (reader->GetFile().KeyFrameCount() == video.frameCount)
		{
			for (uint32_t k = 0; k < video.frameCount; ++k)
			{
				const uint32_t frame = (k * 11 + 3) % video.frameCount;
				ASSERT_TRUE(reader->DecodeFrame(frame));
				const auto it = std::ranges::find(video.frames, frame, &golden::Frame::frame);
				if (it != video.frames.end())
				{
					EXPECT_EQ(PlaneCrc(reader->GetPicture().y), it->y) << video.path << " frame " << frame;
				}
			}
		}
	}
}
