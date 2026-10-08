/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// audio::codec's RIFF waves (src/Audio/Codec/WaveFile.h): the chunks, PCM 8 and 16-bit and MS-ADPCM, from waves
// written by hand.

#include <cstdint>
#include <cstring>

#include <array>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Audio/Codec/WaveFile.h"

using namespace openblack::audio::codec;

namespace
{
void Put16(std::vector<uint8_t>& b, uint32_t v)
{
	b.push_back(static_cast<uint8_t>(v));
	b.push_back(static_cast<uint8_t>(v >> 8));
}

void Put32(std::vector<uint8_t>& b, uint32_t v)
{
	Put16(b, v & 0xFFFF);
	Put16(b, v >> 16);
}

void Chunk(std::vector<uint8_t>& b, std::string_view id, const std::vector<uint8_t>& body, uint32_t size)
{
	b.insert(b.end(), id.begin(), id.end());
	Put32(b, size);
	b.insert(b.end(), body.begin(), body.end());
	if ((body.size() & 1) != 0)
	{
		b.push_back(0);
	}
}

std::vector<uint8_t> Format(uint16_t tag, uint16_t channels, uint32_t rate, uint16_t blockAlign, uint16_t bits)
{
	std::vector<uint8_t> f;
	Put16(f, tag);
	Put16(f, channels);
	Put32(f, rate);
	Put32(f, rate * blockAlign);
	Put16(f, blockAlign);
	Put16(f, bits);
	return f;
}

/// "RIFF" size "WAVE", the chunks; `dataSize` overrides the data chunk's size field when set
std::vector<uint8_t> Wave(const std::vector<uint8_t>& format, const std::vector<uint8_t>& data,
                          const std::vector<uint8_t>& fact = {}, int64_t dataSize = -1)
{
	std::vector<uint8_t> chunks;
	Chunk(chunks, "fmt ", format, static_cast<uint32_t>(format.size()));
	if (!fact.empty())
	{
		Chunk(chunks, "fact", fact, static_cast<uint32_t>(fact.size()));
	}
	Chunk(chunks, "data", data, dataSize >= 0 ? static_cast<uint32_t>(dataSize) : static_cast<uint32_t>(data.size()));
	std::vector<uint8_t> file = {'R', 'I', 'F', 'F'};
	Put32(file, static_cast<uint32_t>(chunks.size() + 4));
	file.insert(file.end(), {'W', 'A', 'V', 'E'});
	file.insert(file.end(), chunks.begin(), chunks.end());
	return file;
}

/// A mono MS-ADPCM block header: predictor, delta, the last then the older sample
std::vector<uint8_t> AdpcmHeader(uint8_t predictor, int16_t delta, int16_t last, int16_t older)
{
	std::vector<uint8_t> b = {predictor};
	Put16(b, static_cast<uint16_t>(delta));
	Put16(b, static_cast<uint16_t>(last));
	Put16(b, static_cast<uint16_t>(older));
	return b;
}
} // namespace

TEST(AudioWave, NotAWave)
{
	EXPECT_FALSE(ParseWaveFile(std::vector<uint8_t> {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'A', 'V', 'I', ' '}).has_value());
	EXPECT_FALSE(DecodeWaveFile(std::vector<uint8_t> {}).has_value());
	// A data chunk before the format, or no data chunk
	std::vector<uint8_t> noFormat = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E'};
	Chunk(noFormat, "data", {1, 2}, 2);
	EXPECT_FALSE(ParseWaveFile(noFormat).has_value());
	std::vector<uint8_t> noData = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E'};
	Chunk(noData, "fmt ", Format(1, 1, 22050, 2, 16), 16);
	EXPECT_FALSE(ParseWaveFile(noData).has_value());
}

TEST(AudioWave, Pcm16)
{
	const auto file = Wave(Format(k_FormatPcm, 2, 44100, 4, 16), {0x01, 0x00, 0xFF, 0xFF, 0x00, 0x80, 0xFF, 0x7F, 0x55});
	const auto wave = ParseWaveFile(file);
	ASSERT_TRUE(wave.has_value());
	EXPECT_EQ(wave->channels, 2);
	EXPECT_EQ(wave->frames, 2u); // the odd byte is no frame
	const auto audio = DecodeWaveFile(file);
	ASSERT_TRUE(audio.has_value());
	EXPECT_EQ(audio->channels, 2);
	EXPECT_EQ(audio->sampleRate, 44100u);
	EXPECT_EQ(audio->samples, (std::vector<int16_t> {1, -1, -32768, 32767}));
}

TEST(AudioWave, Pcm8IsUnsigned)
{
	const auto audio = DecodeWaveFile(Wave(Format(k_FormatPcm, 1, 22050, 1, 8), {0, 128, 255, 1}));
	ASSERT_TRUE(audio.has_value());
	EXPECT_EQ(audio->samples, (std::vector<int16_t> {-32768, 0, 32512, -32512}));
}

TEST(AudioWave, DataCutToTheFile)
{
	// The size says 100 bytes, the file has 4
	const auto audio = DecodeWaveFile(Wave(Format(k_FormatPcm, 1, 22050, 2, 16), {1, 0, 2, 0}, {}, 100));
	ASSERT_TRUE(audio.has_value());
	EXPECT_EQ(audio->samples, (std::vector<int16_t> {1, 2}));
}

TEST(AudioWave, HeaderOnlyWave)
{
	const auto audio = DecodeWaveFile(Wave(Format(k_FormatPcm, 1, 44100, 2, 16), {}));
	ASSERT_TRUE(audio.has_value());
	EXPECT_TRUE(audio->samples.empty());
}

TEST(AudioWave, BrokenOrUnsupportedWavesAreRefused)
{
	// No bits per sample, MS-ADPCM with 3 channels, no sample rate
	EXPECT_FALSE(DecodeWaveFile(Wave(Format(0x50, 1, 22050, 1, 0), {1, 2, 3, 4})).has_value());
	EXPECT_FALSE(DecodeWaveFile(Wave(Format(k_FormatMsAdpcm, 3, 22050, 64, 4), std::vector<uint8_t>(64))).has_value());
	EXPECT_FALSE(DecodeWaveFile(Wave(Format(k_FormatPcm, 1, 0, 2, 16), {1, 2})).has_value());
}

TEST(AudioWave, AnUnknownFormatIsSilence)
{
	// As before: it opens, with the length its data would have, and no sound
	const auto audio = DecodeWaveFile(Wave(Format(0x55, 1, 22050, 2, 16), {1, 2, 3, 4, 5, 6}));
	ASSERT_TRUE(audio.has_value());
	EXPECT_EQ(audio->samples, (std::vector<int16_t> {0, 0, 0}));
}

TEST(AudioWave, Pcm24And32KeepTheirTop16Bits)
{
	const auto pcm24 =
	    DecodeWaveFile(Wave(Format(k_FormatPcm, 1, 22050, 3, 24), {0, 0, 0x80, 0xFF, 0xFF, 0x7F, 0x34, 0x12, 0}));
	ASSERT_TRUE(pcm24.has_value());
	EXPECT_EQ(pcm24->samples, (std::vector<int16_t> {-32768, 32767, 0x12}));
	const auto pcm32 = DecodeWaveFile(Wave(Format(k_FormatPcm, 1, 22050, 4, 32), {0x78, 0x56, 0x34, 0x12}));
	ASSERT_TRUE(pcm32.has_value());
	EXPECT_EQ(pcm32->samples, (std::vector<int16_t> {0x1234}));
}

TEST(AudioWave, IeeeFloat)
{
	std::vector<uint8_t> data;
	for (const float f : {0.5f, -1.0f, 1.0f, 2.0f})
	{
		uint32_t bits = 0;
		std::memcpy(&bits, &f, 4);
		Put32(data, bits);
	}
	const auto audio = DecodeWaveFile(Wave(Format(k_FormatIeeeFloat, 1, 22050, 4, 32), data));
	ASSERT_TRUE(audio.has_value());
	// (clamp(x) + 1) * 32767.5, truncated, - 32768
	EXPECT_EQ(audio->samples, (std::vector<int16_t> {16383, -32768, 32767, 32767}));
}

TEST(AudioWave, ALawAndMuLaw)
{
	const auto alaw = DecodeWaveFile(Wave(Format(k_FormatALaw, 1, 8000, 1, 8), {0xD5, 0x55, 0x00}));
	ASSERT_TRUE(alaw.has_value());
	EXPECT_EQ(alaw->samples, (std::vector<int16_t> {8, -8, -5504}));
	const auto mulaw = DecodeWaveFile(Wave(Format(k_FormatMuLaw, 1, 8000, 1, 8), {0xFF, 0x00, 0x7F}));
	ASSERT_TRUE(mulaw.has_value());
	EXPECT_EQ(mulaw->samples, (std::vector<int16_t> {0, -32124, 0}));
}

TEST(AudioWave, ImaAdpcm)
{
	// A mono block of 8 bytes: the predictor 100 and step index 0, then 8 nibbles, the low one of each byte first
	std::vector<uint8_t> block;
	Put16(block, 100);
	block.insert(block.end(), {0, 0, 0x21, 0x08, 0x74, 0x0F});
	const auto audio = DecodeWaveFile(Wave(Format(k_FormatImaAdpcm, 1, 22050, 8, 4), block));
	ASSERT_TRUE(audio.has_value());
	EXPECT_EQ(audio->samples, (std::vector<int16_t> {100, 101, 104, 104, 104, 111, 127, 93, 98}));
}

TEST(AudioWave, ExtensibleCarriesItsSubFormat)
{
	auto format = Format(k_FormatExtensible, 1, 22050, 2, 16);
	Put16(format, 22);
	Put16(format, 16);
	Put32(format, 4);
	Put16(format, k_FormatPcm);
	format.insert(format.end(), {0, 0, 0, 0, 0x10, 0, 0x80, 0, 0, 0xAA, 0, 0x38, 0x9B, 0x71});
	const auto audio = DecodeWaveFile(Wave(format, {0x34, 0x12}));
	ASSERT_TRUE(audio.has_value());
	EXPECT_EQ(audio->samples, (std::vector<int16_t> {0x1234}));
}

TEST(AudioWave, Rifx)
{
	// RIFF with every number big-endian, the samples too
	const std::vector<uint8_t> file = {'R', 'I', 'F', 'X', 0,   0,   0,   38,  'W', 'A', 'V',  'E',  'f',  'm', 't',  ' ',
	                                   0,   0,   0,   16,  0,   1,   0,   1,   0,   0,   0x56, 0x22, 0,    0,   0xAC, 0x44,
	                                   0,   2,   0,   16,  'd', 'a', 't', 'a', 0,   0,   0,    2,    0x12, 0x34};
	const auto audio = DecodeWaveFile(file);
	ASSERT_TRUE(audio.has_value());
	EXPECT_EQ(audio->sampleRate, 22050u);
	EXPECT_EQ(audio->samples, (std::vector<int16_t> {0x1234}));
}

TEST(AudioWave, Wave64)
{
	const std::array<uint8_t, 12> tail = {0xF3, 0xAC, 0xD3, 0x11, 0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A};
	const auto guid = [&](std::vector<uint8_t>& b, std::string_view id) {
		b.insert(b.end(), id.begin(), id.end());
		b.insert(b.end(), tail.begin(), tail.end());
	};
	std::vector<uint8_t> file = {'r', 'i', 'f', 'f', 0x2E, 0x91, 0xCF, 0x11, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00};
	Put32(file, 200);
	Put32(file, 0);
	guid(file, "wave");
	guid(file, "fmt ");
	Put32(file, 24 + 16);
	Put32(file, 0);
	const auto format = Format(k_FormatPcm, 1, 22050, 2, 16);
	file.insert(file.end(), format.begin(), format.end());
	guid(file, "data");
	Put32(file, 24 + 4);
	Put32(file, 0);
	file.insert(file.end(), {0x01, 0x00, 0xFF, 0x7F});
	const auto audio = DecodeWaveFile(file);
	ASSERT_TRUE(audio.has_value());
	EXPECT_EQ(audio->samples, (std::vector<int16_t> {1, 32767}));
}

TEST(AudioWave, Rf64TakesItsSizesFromDs64)
{
	std::vector<uint8_t> file = {'R', 'F', '6', '4', 0xFF, 0xFF, 0xFF, 0xFF, 'W', 'A', 'V', 'E', 'd', 's', '6', '4'};
	Put32(file, 28);
	Put32(file, 0); // the RIFF size
	Put32(file, 0);
	Put32(file, 4); // the data size
	Put32(file, 0);
	Put32(file, 0); // the sample count: none, so from the data
	Put32(file, 0);
	Put32(file, 0); // the table length
	std::vector<uint8_t> chunks;
	Chunk(chunks, "fmt ", Format(k_FormatPcm, 1, 22050, 2, 16), 16);
	Chunk(chunks, "data", {5, 0, 6, 0, 7, 0}, 0xFFFFFFFF);
	file.insert(file.end(), chunks.begin(), chunks.end());
	const auto audio = DecodeWaveFile(file);
	ASSERT_TRUE(audio.has_value());
	EXPECT_EQ(audio->samples, (std::vector<int16_t> {5, 6})); // ds64 says 4 bytes
}

namespace
{
/// An AIFF or AIFC file: COMM (1 channel, the frames, the bits, 22050 Hz as an 80-bit float, the AIFC compression) and
/// SSND
std::vector<uint8_t> Aiff(bool aifc, std::string_view compression, uint16_t bits, uint32_t frames,
                          const std::vector<uint8_t>& samples)
{
	const auto be16 = [](std::vector<uint8_t>& b, uint32_t v) {
		b.push_back(static_cast<uint8_t>(v >> 8));
		b.push_back(static_cast<uint8_t>(v));
	};
	const auto be32 = [&](std::vector<uint8_t>& b, uint32_t v) {
		be16(b, v >> 16);
		be16(b, v & 0xFFFF);
	};
	std::vector<uint8_t> comm;
	be16(comm, 1);
	be32(comm, frames);
	be16(comm, bits);
	// 22050 = 0x5622: the exponent 16383 + 14, the significand with its top bit first
	be16(comm, 16383 + 14);
	be32(comm, 0x5622u << 17);
	be32(comm, 0);
	if (aifc)
	{
		comm.insert(comm.end(), compression.begin(), compression.end());
		comm.insert(comm.end(), {0, 0}); // its name, empty, as a Pascal string
	}
	std::vector<uint8_t> file = {'F', 'O', 'R', 'M'};
	be32(file, 100);
	const std::string_view type = aifc ? "AIFC" : "AIFF";
	file.insert(file.end(), type.begin(), type.end());
	file.insert(file.end(), {'C', 'O', 'M', 'M'});
	be32(file, static_cast<uint32_t>(comm.size()));
	file.insert(file.end(), comm.begin(), comm.end());
	file.insert(file.end(), {'S', 'S', 'N', 'D'});
	be32(file, static_cast<uint32_t>(8 + samples.size()));
	be32(file, 0);
	be32(file, 0);
	file.insert(file.end(), samples.begin(), samples.end());
	return file;
}
} // namespace

TEST(AudioWave, Aiff)
{
	// Big-endian 16-bit
	const auto pcm = DecodeWaveFile(Aiff(false, "", 16, 2, {0x12, 0x34, 0xFF, 0xFE}));
	ASSERT_TRUE(pcm.has_value());
	EXPECT_EQ(pcm->sampleRate, 22050u);
	EXPECT_EQ(pcm->samples, (std::vector<int16_t> {0x1234, -2}));
	// Signed 8-bit
	const auto signed8 = DecodeWaveFile(Aiff(false, "", 8, 2, {0x80, 0x00}));
	ASSERT_TRUE(signed8.has_value());
	EXPECT_EQ(signed8->samples, (std::vector<int16_t> {-32768, 0}));
	// AIFC little-endian ("sowt")
	const auto sowt = DecodeWaveFile(Aiff(true, "sowt", 16, 1, {0x34, 0x12}));
	ASSERT_TRUE(sowt.has_value());
	EXPECT_EQ(sowt->samples, (std::vector<int16_t> {0x1234}));
	// The frame count comes from COMM: frames past the data stay 0
	const auto longer = DecodeWaveFile(Aiff(false, "", 16, 3, {0x00, 0x01}));
	ASSERT_TRUE(longer.has_value());
	EXPECT_EQ(longer->samples, (std::vector<int16_t> {1, 0, 0}));
	// AIFC IMA ADPCM is not read
	EXPECT_FALSE(DecodeWaveFile(Aiff(true, "ima4", 16, 1, {0, 0})).has_value());
}

TEST(AudioWave, MsAdpcmMono)
{
	// Predictor 0 (256, 0), delta 16, last 100, older 50; then nibbles 1, 2, -1, -8, 7, 0
	auto block = AdpcmHeader(0, 16, 100, 50);
	block.insert(block.end(), {0x12, 0xF8, 0x70});
	const auto file = Wave(Format(k_FormatMsAdpcm, 1, 22050, 10, 4), block);
	const auto audio = DecodeWaveFile(file);
	ASSERT_TRUE(audio.has_value());
	// The older sample first; each nibble times the delta, which adapts (230/256 at small nibbles, at least 16; 768/256
	// after an 8)
	EXPECT_EQ(audio->samples, (std::vector<int16_t> {50, 100, 116, 148, 132, 4, 340, 340}));
}

TEST(AudioWave, MsAdpcmPredictorsAndClamp)
{
	// Predictor 1 (512, -256): 2 * last - older, clamped to 16 bits
	auto block = AdpcmHeader(1, 1000, 30000, 20000);
	block.push_back(0x70);
	const auto audio = DecodeWaveFile(Wave(Format(k_FormatMsAdpcm, 1, 22050, 8, 4), block));
	ASSERT_TRUE(audio.has_value());
	ASSERT_EQ(audio->samples.size(), 4u);
	EXPECT_EQ(audio->samples[2], 32767); // 40000 + 7000
	// then 2 * 32767 - 30000 = 35534, the delta now 614 * 1000 >> 8 = 2398: also clamped
	EXPECT_EQ(audio->samples[3], 32767);
	// An unknown predictor stops the decoding: the frames stay 0
	auto bad = AdpcmHeader(7, 16, 1, 2);
	bad.push_back(0x11);
	const auto stopped = DecodeWaveFile(Wave(Format(k_FormatMsAdpcm, 1, 22050, 8, 4), bad));
	ASSERT_TRUE(stopped.has_value());
	EXPECT_EQ(stopped->samples, (std::vector<int16_t> {0, 0, 0, 0}));
}

TEST(AudioWave, MsAdpcmStereo)
{
	// Both predictors, both deltas, both last samples, both older ones; then a byte per frame, left high nibble
	std::vector<uint8_t> block = {0, 0};
	Put16(block, 16);
	Put16(block, 16);
	Put16(block, 10);
	Put16(block, static_cast<uint16_t>(-10));
	Put16(block, 5);
	Put16(block, static_cast<uint16_t>(-5));
	block.insert(block.end(), {0x1F, 0x20});
	const auto audio = DecodeWaveFile(Wave(Format(k_FormatMsAdpcm, 2, 22050, 16, 4), block));
	ASSERT_TRUE(audio.has_value());
	EXPECT_EQ(audio->samples, (std::vector<int16_t> {5, -5, 10, -10, 26, -26, 58, -26}));
}

TEST(AudioWave, MsAdpcmEndsAtItsFactLength)
{
	// 2 blocks: one of 10 bytes (8 frames) and a partial one of 4, which counts as a block with its header: the data
	// gives (14 - 2 * 6) * 2 = 4 frames. With a "fact" of 3 the wave ends there; without one, at the 4
	auto data = AdpcmHeader(0, 16, 100, 50);
	data.insert(data.end(), {0x12, 0xF8, 0x70});
	data.insert(data.end(), {0, 0, 0, 0});
	std::vector<uint8_t> fact;
	Put32(fact, 3);
	const auto withFact = DecodeWaveFile(Wave(Format(k_FormatMsAdpcm, 1, 22050, 10, 4), data, fact));
	ASSERT_TRUE(withFact.has_value());
	EXPECT_EQ(withFact->samples, (std::vector<int16_t> {50, 100, 116}));
	const auto wave = ParseWaveFile(Wave(Format(k_FormatMsAdpcm, 1, 22050, 10, 4), data));
	ASSERT_TRUE(wave.has_value());
	EXPECT_EQ(wave->frames, 4u);
	// A "fact" longer than the data does not lengthen it
	std::vector<uint8_t> longFact;
	Put32(longFact, 1000);
	const auto longer = DecodeWaveFile(Wave(Format(k_FormatMsAdpcm, 1, 22050, 10, 4), data, longFact));
	ASSERT_TRUE(longer.has_value());
	EXPECT_EQ(longer->samples.size(), 4u);
}
