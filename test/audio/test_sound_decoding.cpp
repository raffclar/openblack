/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Decodes every sample of every sound bank of a Black & White installation and checks each against what its
// container declares. The game's data is not distributed, so this only runs when OPENBLACK_GAME_PATH points at an
// installation.

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <PackFile.h>
#include <Sound/SoundDecoder.h>
#include <gtest/gtest.h>

using namespace openblack;

namespace
{
/// A mono 16-bit PCM WAV file of silence
std::vector<uint8_t> SilentWav(uint32_t frames, uint32_t rate)
{
	const uint32_t dataSize = frames * 2;
	std::vector<uint8_t> data(44 + dataSize, 0);
	const auto put = [&data](size_t offset, auto value) { std::memcpy(data.data() + offset, &value, sizeof(value)); };
	std::memcpy(data.data(), "RIFF", 4);
	put(4, static_cast<uint32_t>(36 + dataSize));
	std::memcpy(data.data() + 8, "WAVEfmt ", 8);
	put(16, uint32_t {16});
	put(20, uint16_t {1});
	put(22, uint16_t {1});
	put(24, rate);
	put(28, rate * 2);
	put(32, uint16_t {2});
	put(34, uint16_t {16});
	std::memcpy(data.data() + 36, "data", 4);
	put(40, dataSize);
	return data;
}

/// Raw MPEG audio of silent frames
std::vector<uint8_t> SilentMpeg(size_t frames)
{
	constexpr size_t k_FrameBytes = 104;
	std::vector<uint8_t> data(frames * k_FrameBytes, 0);
	for (size_t i = 0; i < frames; ++i)
	{
		data[i * k_FrameBytes + 0] = 0xFF;
		data[i * k_FrameBytes + 1] = 0xF3;
		data[i * k_FrameBytes + 2] = 0x40;
		data[i * k_FrameBytes + 3] = 0xC0;
	}
	return data;
}
} // namespace

TEST(SoundDecoding, TheMeasuredLengthIsTheDecodedLength)
{
	const auto wav = SilentWav(1000, 22050);
	const auto wavLength = audio::MeasureSound(wav);
	const auto wavSound = audio::DecodeSound(wav);
	ASSERT_TRUE(wavLength.has_value());
	ASSERT_TRUE(wavSound.sound.has_value());
	EXPECT_EQ(wavLength->frames, wavSound.sound->frames);
	EXPECT_EQ(wavLength->sampleRate, wavSound.sound->sampleRate);
	EXPECT_EQ(wavLength->Seconds(), wavSound.sound->Duration());

	const auto mpeg = SilentMpeg(10);
	const auto mpegLength = audio::MeasureSound(mpeg);
	const auto mpegSound = audio::DecodeSound(mpeg);
	ASSERT_TRUE(mpegLength.has_value());
	ASSERT_TRUE(mpegSound.sound.has_value());
	EXPECT_EQ(mpegLength->frames, mpegSound.sound->frames);
	EXPECT_EQ(mpegLength->sampleRate, mpegSound.sound->sampleRate);

	// A sample in parts lasts all of its parts; a part that is not a sound adds nothing
	const std::vector<std::vector<uint8_t>> parts {mpeg, std::vector<uint8_t>(16, 0x11), mpeg};
	EXPECT_EQ(audio::MeasureSoundParts(parts),
	          static_cast<float>(2 * mpegLength->frames) / static_cast<float>(mpegLength->sampleRate));
	EXPECT_FALSE(audio::MeasureSound(std::vector<uint8_t>(16, 0x11)).has_value());
	EXPECT_EQ(audio::MeasureSoundParts({}), 0.0f);
}

TEST(SoundDecoding, EveryGameSampleDecodes)
{
	const auto* gamePath = std::getenv("OPENBLACK_GAME_PATH");
	if (gamePath == nullptr)
	{
		GTEST_SKIP() << "Set OPENBLACK_GAME_PATH to a Black & White installation to decode its sound banks";
	}
	const auto audioPath = std::filesystem::path(gamePath) / "Audio";
	ASSERT_TRUE(std::filesystem::exists(audioPath)) << audioPath;

	size_t decoded = 0;
	size_t trimmed = 0;
	size_t silent = 0;
	size_t gaps = 0;
	std::map<std::string, size_t> containers;
	std::vector<std::string> problems;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(audioPath))
	{
		if (entry.path().extension() != ".sad")
		{
			continue;
		}

		pack::PackFile pack;
		const auto result = pack.Open(entry.path());
		if (result != pack::PackResult::Success)
		{
			problems.push_back(entry.path().filename().string() + ": " + std::string(pack::ResultToStr(result)));
			continue;
		}

		const auto& headers = pack.GetAudioSampleHeaders();
		const auto& samples = pack.GetAudioSamplesData();
		for (size_t i = 0; i < headers.size(); ++i)
		{
			// Banks have gaps between their samples: blank headers with no data, which the game skips when loading
			if (samples[i].empty())
			{
				++gaps;
				continue;
			}
			const auto name = entry.path().filename().string() + "/" + std::to_string(headers[i].id) + " " +
			                  std::filesystem::path(std::string(headers[i].name.data())).filename().string();
			const auto sound = audio::DecodeSound(samples[i], static_cast<int>(headers[i].sampleRate));
			++containers[audio::ToString(sound.container)];
			if (!sound.sound)
			{
				problems.push_back(name + ": " + sound.error);
				continue;
			}
			++decoded;
			// The length read from the headers is the length that the decoder gives
			const auto length = audio::MeasureSound(samples[i]);
			if (!length || length->frames != sound.sound->frames || length->sampleRate != sound.sound->sampleRate)
			{
				problems.push_back(name + ": the measured length is not the decoded length");
			}
			silent += sound.sound->frames == 0 ? 1 : 0;
			trimmed += sound.trimmedFrames != 0 ? 1 : 0;
			for (const auto& warning : sound.warnings)
			{
				problems.push_back(std::string(name).append(": ").append(warning));
			}
		}
	}

	for (const auto& [container, count] : containers)
	{
		std::cout << count << " " << container << " samples\n";
	}
	std::cout << decoded << " samples decoded, " << silent << " silent placeholders, " << trimmed
	          << " with block padding removed, " << gaps << " gaps skipped, " << problems.size() << " problems\n";
	for (const auto& problem : problems)
	{
		ADD_FAILURE() << problem;
	}
}
