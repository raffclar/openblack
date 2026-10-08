/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// audio::codec on the game's own sound banks: a handful of samples of each format, and the first music segments of
// three banks, against the golden CRCs of AudioGolden.h, when OPENBLACK_GAME_PATH is the game's folder; without it
// these tests skip.

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <PackFile.h>
#include <gtest/gtest.h>

#include "Audio/Codec/MpegAudio.h"
#include "Audio/Codec/WaveFile.h"
#include "AudioGolden.h"

using namespace openblack;
using namespace openblack::audio::codec;
using namespace openblack::test::audio_golden;

namespace
{
std::optional<std::vector<std::vector<uint8_t>>> BankWaves(std::string_view bank)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		return std::nullopt;
	}
	pack::PackFile pack;
	if (pack.Open(std::filesystem::path(game) / std::filesystem::path(bank)) != pack::PackResult::Success)
	{
		return std::nullopt;
	}
	return pack.GetAudioSamplesData();
}

/// As the sound banks' waves are decoded (WaveBuffers): an MPEG RIFF wave's data chunk as an MPEG stream, any other
/// wave as a wave file
std::optional<DecodedAudio> DecodeBankWave(std::span<const uint8_t> wave)
{
	const auto u32 = [&](size_t at) {
		return static_cast<uint32_t>(wave[at] | wave[at + 1] << 8 | wave[at + 2] << 16 | wave[at + 3] << 24);
	};
	uint16_t tag = 0;
	for (size_t at = 12; at + 8 <= wave.size();)
	{
		const size_t size = u32(at + 4);
		if (std::memcmp(&wave[at], "fmt ", 4) == 0)
		{
			tag = static_cast<uint16_t>(wave[at + 8] | wave[at + 9] << 8);
		}
		else if (std::memcmp(&wave[at], "data", 4) == 0 && tag == 0x50)
		{
			return DecodeMpegStream(wave.subspan(at + 8, std::min(size, wave.size() - at - 8)));
		}
		at += 8 + size + (size & 1);
	}
	return DecodeWaveFile(wave);
}
} // namespace

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(AudioGolden, BankWaves)
{
	if (std::getenv("OPENBLACK_GAME_PATH") == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	for (const auto& golden : k_Waves)
	{
		const auto waves = BankWaves(golden.bank);
		ASSERT_TRUE(waves.has_value()) << golden.bank;
		ASSERT_LT(golden.sample, waves->size()) << golden.bank;
		const auto audio = DecodeBankWave((*waves)[golden.sample]);
		ASSERT_TRUE(audio.has_value()) << golden.bank << " " << golden.sample;
		EXPECT_EQ(audio->samples.size(), size_t {golden.frames} * golden.channels) << golden.bank << " " << golden.sample;
		EXPECT_EQ(audio->channels, golden.channels) << golden.bank << " " << golden.sample;
		EXPECT_EQ(audio->sampleRate, golden.rate) << golden.bank << " " << golden.sample;
		EXPECT_EQ(Crc(audio->samples), golden.crc) << golden.bank << " " << golden.sample;
	}
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(AudioGolden, MusicSegmentsInOrder)
{
	if (std::getenv("OPENBLACK_GAME_PATH") == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::string bank;
	std::optional<std::vector<std::vector<uint8_t>>> waves;
	MpegFrameDecoder decoder;
	uint32_t next = 0;
	for (const auto& golden : k_Segments)
	{
		if (golden.bank != bank)
		{
			bank = golden.bank;
			waves = BankWaves(bank);
			ASSERT_TRUE(waves.has_value()) << bank;
			decoder.Reset();
			next = 0;
		}
		// The segments before it, in order, through the same decoder
		std::vector<int16_t> pcm;
		uint16_t channels = 0;
		uint32_t rate = 0;
		for (; next <= golden.sample; ++next)
		{
			pcm.clear();
			const std::span<const uint8_t> segment = (*waves)[next];
			std::array<int16_t, MpegFrameDecoder::k_MaxSamples> frame {};
			size_t at = 0;
			while (at < segment.size())
			{
				const auto result = decoder.Decode(segment.subspan(at), frame);
				if (result.bytes == 0)
				{
					break;
				}
				at += result.bytes;
				if (result.samples > 0)
				{
					channels = result.channels;
					rate = result.sampleRate;
					pcm.insert(pcm.end(), frame.begin(), frame.begin() + result.samples * result.channels);
				}
			}
		}
		EXPECT_EQ(pcm.size(), size_t {golden.frames} * golden.channels) << bank << " " << golden.sample;
		EXPECT_EQ(channels, golden.channels) << bank << " " << golden.sample;
		EXPECT_EQ(rate, golden.rate) << bank << " " << golden.sample;
		EXPECT_EQ(Crc(pcm), golden.crc) << bank << " " << golden.sample;
	}
}
