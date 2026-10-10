/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Damaged and malformed videos fail cleanly: no crash and no read outside the data, and a frame that fails leaves the
// last good picture. Deterministic fuzzing of synthetic packets everywhere, and of the game's own packets when
// OPENBLACK_GAME_PATH names the game's folder.

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <memory>
#include <random>
#include <vector>

#include <BinkDecoder.h>
#include <BinkFile.h>
#include <gtest/gtest.h>

#include "BinkTestFiles.h"
#include "Crc32.h"

using namespace openblack::bink;
using namespace openblack::bink::test;

namespace
{
/// OPENBLACK_BINK_FUZZ_SCALE multiplies the rounds, for longer runs by hand (under a sanitizer, say)
int Scale()
{
	const char* scale = std::getenv("OPENBLACK_BINK_FUZZ_SCALE");
	return scale != nullptr ? std::max(1, std::atoi(scale)) : 1;
}

std::vector<std::vector<uint8_t>> SyntheticPackets()
{
	return {FillFrame(90, 100, 110), RawFrame(), MotionFrame(-8), SkipFrame(), MotionFrame(3)};
}

uint32_t PictureCrc(const Picture& picture)
{
	return Crc32().AddU32(PlaneCrc(picture.y)).AddU32(PlaneCrc(picture.u)).AddU32(PlaneCrc(picture.v)).Value();
}

/// Decodes `packet` over a decoder holding a known picture: whatever it returns, a failure must leave that picture
void DecodeDamaged(Decoder& decoder, std::span<const uint8_t> packet)
{
	const uint32_t before = PictureCrc(decoder.GetPicture());
	if (!decoder.Decode(packet))
	{
		ASSERT_EQ(PictureCrc(decoder.GetPicture()), before);
	}
}

void Fuzz(const Header& header, const std::vector<std::vector<uint8_t>>& packets, std::span<const uint8_t> reference,
          uint32_t seed, int rounds)
{
	std::mt19937 random(seed);
	auto decoder = Decoder::Create(header);
	ASSERT_TRUE(decoder.has_value());
	ASSERT_TRUE(decoder->Decode(reference));
	for (const auto& packet : packets)
	{
		// Every truncation of short packets, a spread of them for long ones
		const size_t step = std::max<size_t>(1, packet.size() / 64);
		for (size_t size = 0; size < packet.size(); size += step)
		{
			DecodeDamaged(*decoder, std::span(packet).first(size));
		}
		for (int round = 0; round < rounds; ++round)
		{
			auto damaged = packet;
			const int flips = 1 + static_cast<int>(random() % 8);
			for (int i = 0; i < flips; ++i)
			{
				damaged[random() % damaged.size()] ^= static_cast<uint8_t>(1u << (random() % 8));
			}
			if (random() % 4 == 0)
			{
				damaged.resize(random() % damaged.size());
			}
			DecodeDamaged(*decoder, damaged);
		}
		// Random bytes
		std::vector<uint8_t> noise(packet.size());
		std::ranges::generate(noise, [&random]() { return static_cast<uint8_t>(random()); });
		DecodeDamaged(*decoder, noise);
	}
	// It still decodes good data afterwards
	EXPECT_TRUE(decoder->Decode(reference));
}
} // namespace

TEST(BinkRobustness, DamagedSyntheticPackets)
{
	const auto packets = SyntheticPackets();
	Fuzz({.revision = 'i', .width = 16, .height = 16}, packets, packets[0], 1234, 400 * Scale());
}

TEST(BinkRobustness, EmptyAndTinyPackets)
{
	auto decoder = Decoder::Create({.revision = 'i', .width = 16, .height = 16});
	ASSERT_TRUE(decoder.has_value());
	EXPECT_FALSE(decoder->Decode({}));
	const std::vector<uint8_t> tiny = {4, 0, 0, 0};
	EXPECT_FALSE(decoder->Decode(tiny));
	EXPECT_FALSE(decoder->HasPicture());
}

TEST(BinkRobustness, ImpossibleSizesAreRefused)
{
	EXPECT_FALSE(Decoder::Create({.revision = 'i', .width = 0xFFFFFFFF, .height = 16}).has_value());
	EXPECT_FALSE(Decoder::Create({.revision = 'i', .width = 16, .height = 100000}).has_value());
	// One pixel is the smallest video
	auto one = Decoder::Create({.revision = 'i', .width = 1, .height = 1});
	ASSERT_TRUE(one.has_value());
	std::mt19937 random(7);
	for (int i = 0; i < 200; ++i)
	{
		std::vector<uint8_t> noise(4 + random() % 64);
		std::ranges::generate(noise, [&random]() { return static_cast<uint8_t>(random()); });
		noise[0] = static_cast<uint8_t>(random() % 16);
		noise[1] = noise[2] = noise[3] = 0;
		DecodeDamaged(*one, noise);
	}
}

TEST(BinkRobustness, MalformedContainers)
{
	const auto good = MakeBik(SyntheticPackets());
	std::mt19937 random(99);
	for (int round = 0; round < 2000 * Scale(); ++round)
	{
		auto damaged = good;
		const int flips = 1 + static_cast<int>(random() % 4);
		for (int i = 0; i < flips; ++i)
		{
			// Mostly the header and the tables
			const size_t at = random() % 2 == 0 ? random() % 72 : random() % damaged.size();
			damaged[at] ^= static_cast<uint8_t>(1u << (random() % 8));
		}
		auto file = BinkFile::Parse(std::move(damaged));
		if (!file)
		{
			continue;
		}
		auto reader = FrameReader::Create(std::make_shared<const BinkFile>(std::move(*file)));
		if (!reader)
		{
			continue;
		}
		for (uint32_t frame = 0; frame < reader->GetFile().FrameCount() && frame < 8; ++frame)
		{
			[[maybe_unused]] const bool decoded = reader->DecodeFrame(frame);
		}
	}
}

TEST(BinkRobustness, DamagedGamePackets)
{
	const auto game = GamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	uint32_t seed = 1;
	for (const char* name : {"Data/INTRO.bik", "Data/tips.bik"})
	{
		const auto path = *game / name;
		if (!std::filesystem::exists(path))
		{
			GTEST_SKIP() << path << " not found";
		}
		const auto file = BinkFile::Open(path);
		ASSERT_TRUE(file.has_value());
		std::vector<std::vector<uint8_t>> packets;
		for (const uint32_t frame : {0u, 1u, 30u, 200u, 600u, 1000u})
		{
			const auto packet = file->VideoPacket(frame % file->FrameCount());
			packets.emplace_back(packet.begin(), packet.end());
		}
		Fuzz(file->GetHeader(), packets, packets[0], seed++, 40 * Scale());
	}
}
