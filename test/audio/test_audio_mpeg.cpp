/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// audio::codec's MPEG audio layer II (src/Audio/Codec/MpegAudio.h): the frame search, the streams and the decoding,
// with synthetic frames written field by field (support/MpegTestFrames.h) whose decodes are pinned to the CRC-32 of
// what dr_mp3 gave for them.

#include <cstdint>

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Audio/Codec/MpegAudio.h"
#include "AudioGolden.h"
#include "support/MpegTestFrames.h"

using namespace openblack::audio::codec;
using namespace openblack::test;

namespace
{
std::vector<uint8_t> Stream(Layer2FrameSpec spec, int frames)
{
	std::vector<uint8_t> stream;
	for (int i = 0; i < frames; ++i)
	{
		const auto frame = MakeLayer2Frame(spec);
		stream.insert(stream.end(), frame.begin(), frame.end());
		++spec.seed;
	}
	return stream;
}

Layer2FrameSpec Mono22() // MPEG-2, 22050 Hz, 64 kbps, mono: the voices' format
{
	return {.mpeg1 = false, .rateIndex = 0, .bitrateIndex = 8, .stereo = false, .seed = 1, .largestCode = 1};
}
} // namespace

TEST(AudioMpeg, AFrameOfSilence)
{
	// Every band without bits: 1152 samples of 0
	auto spec = Mono22();
	spec.largestCode = 0;
	const auto frame = MakeLayer2Frame(spec);
	ASSERT_EQ(frame.size(), 417u); // 1152 * 64000 / 8 / 22050
	MpegFrameDecoder decoder;
	std::array<int16_t, MpegFrameDecoder::k_MaxSamples> pcm;
	pcm.fill(1);
	const auto result = decoder.Decode(frame, pcm);
	EXPECT_EQ(result.samples, 1152u);
	EXPECT_EQ(result.bytes, frame.size());
	EXPECT_EQ(result.channels, 1);
	EXPECT_EQ(result.sampleRate, 22050u);
	EXPECT_EQ(result.layer, 2);
	for (size_t i = 0; i < 1152; ++i)
	{
		ASSERT_EQ(pcm[i], 0) << i;
	}
}

TEST(AudioMpeg, MeasuringDoesNotDecode)
{
	const auto stream = Stream(Mono22(), 2);
	MpegFrameDecoder decoder;
	const auto result = decoder.Decode(stream, {});
	EXPECT_EQ(result.samples, 1152u);
	EXPECT_EQ(result.bytes, 417u);
}

TEST(AudioMpeg, GarbageBeforeTheFirstFrame)
{
	auto stream = Stream(Mono22(), 3);
	stream.insert(stream.begin(), {0x00, 0xFF, 0x12, 0x34, 0xFF});
	MpegFrameDecoder decoder;
	std::array<int16_t, MpegFrameDecoder::k_MaxSamples> pcm {};
	const auto result = decoder.Decode(stream, pcm);
	EXPECT_EQ(result.samples, 1152u);
	EXPECT_EQ(result.bytes, 5u + 417u); // the skipped bytes and the frame
}

TEST(AudioMpeg, AFrameMustBeFollowedByTheSameStream)
{
	// A lone frame followed by a few bytes that are no frame header: not a frame
	auto stream = MakeLayer2Frame(Mono22());
	stream.insert(stream.end(), {1, 2, 3, 4, 5, 6});
	MpegFrameDecoder decoder;
	std::array<int16_t, MpegFrameDecoder::k_MaxSamples> pcm {};
	const auto lone = decoder.Decode(stream, pcm);
	EXPECT_EQ(lone.samples, 0u);
	// A lone frame that is the whole buffer is one
	const auto exact = MakeLayer2Frame(Mono22());
	decoder.Reset();
	EXPECT_EQ(decoder.Decode(exact, pcm).samples, 1152u);
}

TEST(AudioMpeg, StreamsMatchTheReference)
{
	// The CRC-32 of each stream's samples as dr_mp3 decoded them
	struct Case
	{
		Layer2FrameSpec spec;
		uint32_t frames;
		uint16_t channels;
		uint32_t rate;
		uint32_t crc;
	};
	const std::array<Case, 3> cases = {{
	    {Mono22(), 3456, 1, 22050, 0x72BC06F1},
	    // MPEG-1 48000 Hz 192 kbps stereo
	    {{.mpeg1 = true, .rateIndex = 1, .bitrateIndex = 10, .stereo = true, .seed = 7, .largestCode = 1},
	     3456,
	     2,
	     48000,
	     0x98921B7E},
	    // MPEG-2 22050 Hz 128 kbps stereo with grouped samples (3, 5 and 9 levels)
	    {{.mpeg1 = false, .rateIndex = 0, .bitrateIndex = 12, .stereo = true, .seed = 11, .largestCode = 7},
	     3456,
	     2,
	     22050,
	     0x4D115F26},
	}};
	for (const auto& c : cases)
	{
		const auto audio = DecodeMpegStream(Stream(c.spec, 3));
		ASSERT_TRUE(audio.has_value());
		EXPECT_EQ(audio->channels, c.channels);
		EXPECT_EQ(audio->sampleRate, c.rate);
		EXPECT_EQ(audio->samples.size(), size_t {c.frames} * c.channels);
		EXPECT_EQ(audio_golden::Crc(audio->samples), c.crc);
	}
}

TEST(AudioMpeg, AFrameTooBigForItsBitsIsSkipped)
{
	// Seeds 5, 6, 7 with every allocation allowed: one of the frames does not fit its bits and gives nothing; the
	// stream keeps its counted length and ends with 0
	auto spec = Mono22();
	spec.seed = 5;
	spec.largestCode = 15;
	const auto audio = DecodeMpegStream(Stream(spec, 3));
	ASSERT_TRUE(audio.has_value());
	ASSERT_EQ(audio->samples.size(), 3456u);
	EXPECT_EQ(audio_golden::Crc(audio->samples), 0x9AD049C9u);
	for (size_t i = 2304; i < 3456; ++i)
	{
		ASSERT_EQ(audio->samples[i], 0) << i;
	}
}

TEST(AudioMpeg, NoFrameNoStream)
{
	EXPECT_FALSE(DecodeMpegStream(std::vector<uint8_t>(100, 0)).has_value());
	EXPECT_FALSE(DecodeMpegStream(std::vector<uint8_t> {}).has_value());
	// Every frame too big for its bits
	const auto audio = DecodeMpegStream(
	    Stream({.mpeg1 = true, .rateIndex = 1, .bitrateIndex = 10, .stereo = true, .seed = 7, .largestCode = 3}, 3));
	EXPECT_FALSE(audio.has_value());
}

TEST(AudioMpeg, TagsAreSkipped)
{
	const auto plain = DecodeMpegStream(Stream(Mono22(), 3));
	ASSERT_TRUE(plain.has_value());
	// An ID3v2 tag of 20 bytes in front, an ID3v1 tag at the end
	auto tagged = Stream(Mono22(), 3);
	std::vector<uint8_t> id3 = {'I', 'D', '3', 3, 0, 0, 0, 0, 0, 20};
	id3.resize(30, 0xFF);
	tagged.insert(tagged.begin(), id3.begin(), id3.end());
	std::vector<uint8_t> v1(128, 0xFF);
	v1[0] = 'T';
	v1[1] = 'A';
	v1[2] = 'G';
	tagged.insert(tagged.end(), v1.begin(), v1.end());
	const auto audio = DecodeMpegStream(tagged);
	ASSERT_TRUE(audio.has_value());
	EXPECT_EQ(audio->samples, plain->samples);
}

TEST(AudioMpeg, FramesInSequenceKeepTheFilter)
{
	// Decoding frame by frame with one decoder gives the stream's samples; a reset before the second frame does not
	const auto stream = Stream(Mono22(), 3);
	const auto whole = DecodeMpegStream(stream);
	ASSERT_TRUE(whole.has_value());
	MpegFrameDecoder decoder;
	std::vector<int16_t> pcm;
	std::array<int16_t, MpegFrameDecoder::k_MaxSamples> frame {};
	size_t at = 0;
	while (at < stream.size())
	{
		const auto result = decoder.Decode(std::span(stream).subspan(at), frame);
		ASSERT_GT(result.bytes, 0u);
		at += result.bytes;
		pcm.insert(pcm.end(), frame.begin(), frame.begin() + result.samples);
	}
	EXPECT_EQ(pcm, whole->samples);

	decoder.Reset();
	std::array<int16_t, MpegFrameDecoder::k_MaxSamples> first {};
	ASSERT_EQ(decoder.Decode(stream, first).samples, 1152u);
	decoder.Reset();
	std::array<int16_t, MpegFrameDecoder::k_MaxSamples> second {};
	ASSERT_EQ(decoder.Decode(std::span(stream).subspan(417), second).samples, 1152u);
	EXPECT_FALSE(std::equal(second.begin(), second.begin() + 1152, whole->samples.begin() + 1152));
}
