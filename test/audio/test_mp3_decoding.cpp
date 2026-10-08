/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// audio::codec's MPEG audio layer III (src/Audio/Codec/MpegAudio.h), the .mp3 files: the frames, the bit reservoir,
// the Xing and LAME headers and the decoding, with synthetic streams written field by field
// (support/Layer3TestFrames.h) whose decodes are pinned to the CRC-32 of what dr_mp3 gave for them.

#include <cstdint>

#include <algorithm>
#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Audio/Codec/MpegAudio.h"
#include "AudioGolden.h"
#include "support/Layer3TestFrames.h"

using namespace openblack::audio::codec;
using namespace openblack::test::layer3;

namespace
{
StreamSpec Stereo44()
{
	return {.version = Version::Mpeg1, .rateIndex = 0, .bitrateIndex = 14, .channels = Channels::Stereo, .seed = 1};
}

StreamSpec Silent(StreamSpec spec)
{
	spec.silence = true;
	return spec;
}

/// The stream decoded one frame at a time with one decoder, as a music stream is
std::vector<int16_t> DecodeFrameByFrame(std::span<const uint8_t> stream)
{
	MpegFrameDecoder decoder;
	std::vector<int16_t> pcm;
	std::array<int16_t, MpegFrameDecoder::k_MaxSamples> frame {};
	size_t at = 0;
	while (at < stream.size())
	{
		const auto result = decoder.Decode(stream.subspan(at), frame);
		if (result.bytes == 0)
		{
			break;
		}
		at += result.bytes;
		pcm.insert(pcm.end(), frame.begin(), frame.begin() + result.samples * result.channels);
	}
	return pcm;
}
} // namespace

TEST(AudioMp3, AFrameOfSilence)
{
	// Every granule without bits: 1152 samples of 0 in each channel
	const auto frame = MakeStream(Silent({.frames = 1}));
	ASSERT_EQ(frame.size(), 1044u); // 1152 * 320000 / 8 / 44100
	MpegFrameDecoder decoder;
	std::array<int16_t, MpegFrameDecoder::k_MaxSamples> pcm;
	pcm.fill(1);
	const auto result = decoder.Decode(frame, pcm);
	EXPECT_EQ(result.samples, 1152u);
	EXPECT_EQ(result.bytes, frame.size());
	EXPECT_EQ(result.channels, 2);
	EXPECT_EQ(result.sampleRate, 44100u);
	EXPECT_EQ(result.layer, 3);
	EXPECT_TRUE(std::all_of(pcm.begin(), pcm.end(), [](int16_t s) { return s == 0; }));
}

TEST(AudioMp3, EveryVersionRateAndMode)
{
	// MPEG-1 (1152 samples a frame), MPEG-2 and MPEG-2.5 (576), at each of their three rates, mono and stereo
	struct Case
	{
		Version version;
		uint32_t rates[3];
	};
	const std::array<Case, 3> cases = {{
	    {Version::Mpeg1, {44100, 48000, 32000}},
	    {Version::Mpeg2, {22050, 24000, 16000}},
	    {Version::Mpeg25, {11025, 12000, 8000}},
	}};
	for (const auto& c : cases)
	{
		for (uint8_t rate = 0; rate < 3; ++rate)
		{
			for (const auto channels : {Channels::Mono, Channels::JointStereo})
			{
				const StreamSpec spec {.version = c.version, .rateIndex = rate, .channels = channels, .frames = 3};
				const auto audio = DecodeMpegStream(MakeStream(Silent(spec)));
				ASSERT_TRUE(audio.has_value());
				EXPECT_EQ(audio->sampleRate, c.rates[rate]);
				EXPECT_EQ(audio->channels, channels == Channels::Mono ? 1 : 2);
				EXPECT_EQ(audio->samples.size(), size_t {3} * FrameSamples(spec) * audio->channels);
			}
		}
	}
}

TEST(AudioMp3, StreamsMatchTheReference)
{
	// Random side information and main data of every block type, stereo mode and table; the CRC-32 of each stream's
	// samples as dr_mp3 decoded them
	struct Case
	{
		StreamSpec spec;
		uint32_t frames;
		uint32_t crc;
	};
	const std::array<Case, 8> cases = {{
	    {Stereo44(), 4608, 0x1E10D397},
	    {{.version = Version::Mpeg1,
	      .rateIndex = 1,
	      .bitrateIndex = 9,
	      .channels = Channels::JointStereo,
	      .seed = 20,
	      .frames = 6},
	     6912,
	     0x8F136B6B},
	    {{.version = Version::Mpeg1,
	      .rateIndex = 2,
	      .bitrateIndex = 5,
	      .channels = Channels::Mono,
	      .seed = 3,
	      .frames = 5,
	      .maxBigValues = 16},
	     5760,
	     0xA7F59A26},
	    {{.version = Version::Mpeg2,
	      .rateIndex = 0,
	      .bitrateIndex = 14,
	      .channels = Channels::JointStereo,
	      .seed = 4,
	      .frames = 6},
	     3456,
	     0xF42F8B5B},
	    {{.version = Version::Mpeg2,
	      .rateIndex = 2,
	      .bitrateIndex = 8,
	      .channels = Channels::Mono,
	      .seed = 5,
	      .frames = 5,
	      .shortBlocks = false,
	      .maxBigValues = 16},
	     2880,
	     0x89A04F58},
	    {{.version = Version::Mpeg25,
	      .rateIndex = 2,
	      .bitrateIndex = 14,
	      .channels = Channels::JointStereo,
	      .seed = 6,
	      .frames = 5,
	      .shortBlocks = false},
	     2880,
	     0x0DC50D4B},
	    {{.version = Version::Mpeg25,
	      .rateIndex = 0,
	      .bitrateIndex = 10,
	      .channels = Channels::Stereo,
	      .seed = 7,
	      .frames = 4,
	      .maxBigValues = 20},
	     2304,
	     0x5513F51D},
	    // The first frame's main data starts in a reservoir there is not: that frame gives nothing
	    {{.version = Version::Mpeg1,
	      .rateIndex = 0,
	      .bitrateIndex = 14,
	      .channels = Channels::JointStereo,
	      .seed = 8,
	      .frames = 5,
	      .firstBegin = 20},
	     4608,
	     0x422BB1CE},
	}};
	for (const auto& c : cases)
	{
		const auto stream = MakeStream(c.spec);
		const auto audio = DecodeMpegStream(stream);
		ASSERT_TRUE(audio.has_value()) << c.spec.seed;
		EXPECT_EQ(audio->samples.size(), size_t {c.frames} * audio->channels) << c.spec.seed;
		EXPECT_EQ(openblack::test::audio_golden::Crc(audio->samples), c.crc) << c.spec.seed;
		// One frame at a time gives the same samples
		EXPECT_EQ(DecodeFrameByFrame(stream), audio->samples) << c.spec.seed;
	}
}

TEST(AudioMp3, MainDataFromTheReservoir)
{
	// A frame whose main data starts in bytes no frame before it left is found, gives nothing, and still fills the
	// reservoir; measuring the frames counts the same
	auto spec = Stereo44();
	spec.firstBegin = 20;
	const auto stream = MakeStream(spec);
	const auto frameBytes = FrameBytes(spec);
	MpegFrameDecoder decoder;
	std::array<int16_t, MpegFrameDecoder::k_MaxSamples> pcm {};
	const auto first = decoder.Decode(stream, pcm);
	EXPECT_EQ(first.bytes, frameBytes);
	EXPECT_EQ(first.samples, 0u);
	EXPECT_EQ(first.layer, 3);
	EXPECT_EQ(decoder.Decode(std::span(stream).subspan(frameBytes), pcm).samples, 1152u);

	MpegFrameDecoder measuring;
	uint32_t counted = 0;
	for (size_t at = 0; at < stream.size(); at += frameBytes)
	{
		counted += measuring.Decode(std::span(stream).subspan(at), {}).samples;
	}
	EXPECT_EQ(counted, 3u * 1152u);
}

TEST(AudioMp3, AResetForgetsTheReservoir)
{
	// The second frame's main data starts in the first one's; after a reset it is not there
	const auto stream = MakeStream(Stereo44());
	const auto frameBytes = FrameBytes(Stereo44());
	const unsigned begin = static_cast<unsigned>(stream[frameBytes + 4] << 1 | stream[frameBytes + 5] >> 7);
	ASSERT_GT(begin, 0u);
	MpegFrameDecoder decoder;
	std::array<int16_t, MpegFrameDecoder::k_MaxSamples> pcm {};
	EXPECT_EQ(decoder.Decode(stream, pcm).samples, 1152u);
	EXPECT_EQ(decoder.Decode(std::span(stream).subspan(frameBytes), pcm).samples, 1152u);
	decoder.Reset();
	const auto again = decoder.Decode(std::span(stream).subspan(frameBytes), pcm);
	EXPECT_EQ(again.bytes, frameBytes);
	EXPECT_EQ(again.samples, 0u);
}

TEST(AudioMp3, XingFrameCutsTheDelayAndThePadding)
{
	// A LAME header says 1105 samples of delay and 1200 of padding: past the decoder's own 529 that is 1634 cut at
	// the start and 671 at the end; the header frame is not audio
	const auto spec = Stereo44();
	const auto plain = DecodeMpegStream(MakeStream(spec));
	ASSERT_TRUE(plain.has_value());
	auto stream = MakeXingFrame(spec, 4, 1105, 1200);
	const auto frames = MakeStream(spec);
	stream.insert(stream.end(), frames.begin(), frames.end());
	const auto audio = DecodeMpegStream(stream);
	ASSERT_TRUE(audio.has_value());
	constexpr size_t k_Delay = 1105 + 529;
	constexpr size_t k_Padding = 1200 - 529;
	ASSERT_EQ(audio->samples.size(), (4 * 1152 - k_Delay - k_Padding) * 2);
	EXPECT_TRUE(std::equal(audio->samples.begin(), audio->samples.end(), plain->samples.begin() + k_Delay * 2));

	// "Info" (a constant bit rate) reads the same
	auto info = MakeXingFrame(spec, 4, 1105, 1200, false);
	auto xing = MakeXingFrame(spec, 4, 1105, 1200, true);
	info.insert(info.end(), frames.begin(), frames.end());
	xing.insert(xing.end(), frames.begin(), frames.end());
	EXPECT_EQ(DecodeMpegStream(info)->samples, DecodeMpegStream(xing)->samples);
}

TEST(AudioMp3, XingFrameCountSetsTheLength)
{
	const auto spec = Stereo44();
	const auto frames = MakeStream(spec);
	const auto plain = DecodeMpegStream(frames);
	ASSERT_TRUE(plain.has_value());
	const auto withHeader = [&](std::vector<uint8_t> header) {
		header.insert(header.end(), frames.begin(), frames.end());
		return DecodeMpegStream(header);
	};

	// More frames than there are: the length is the header's, the missing end is 0
	const auto longer = withHeader(MakeXingFrame(spec, 6, 0, 529));
	ASSERT_TRUE(longer.has_value());
	ASSERT_EQ(longer->samples.size(), (6 * 1152 - 529) * 2);
	EXPECT_TRUE(std::equal(longer->samples.begin(), longer->samples.begin() + ((4 * 1152 - 529) * 2),
	                       plain->samples.begin() + 529 * 2));
	EXPECT_TRUE(
	    std::all_of(longer->samples.begin() + ((4 * 1152 - 529) * 2), longer->samples.end(), [](int16_t s) { return s == 0; }));

	// Fewer: it stops there
	const auto shorter = withHeader(MakeXingFrame(spec, 2, 0, 529));
	ASSERT_TRUE(shorter.has_value());
	ASSERT_EQ(shorter->samples.size(), (2 * 1152 - 529) * 2);
	EXPECT_TRUE(std::equal(shorter->samples.begin(), shorter->samples.end(), plain->samples.begin() + 529 * 2));

	// No count: every frame is counted and the delay still cut, which leaves 0 at the end
	const auto uncounted = withHeader(MakeXingFrame(spec, 0, 0, 1000, false, false));
	ASSERT_TRUE(uncounted.has_value());
	ASSERT_EQ(uncounted->samples.size(), plain->samples.size());
	EXPECT_TRUE(std::equal(uncounted->samples.begin(), uncounted->samples.end() - 529 * 2, plain->samples.begin() + 529 * 2));
	EXPECT_TRUE(std::all_of(uncounted->samples.end() - 529 * 2, uncounted->samples.end(), [](int16_t s) { return s == 0; }));
}

TEST(AudioMp3, TagsAreSkipped)
{
	const auto frames = MakeStream(Stereo44());
	const auto plain = DecodeMpegStream(frames);
	ASSERT_TRUE(plain.has_value());
	// An ID3v2 tag of 20 bytes in front, an ID3v1 tag at the end
	auto tagged = frames;
	std::vector<uint8_t> id3 = {'I', 'D', '3', 3, 0, 0, 0, 0, 0, 20};
	id3.resize(30, 0xFF);
	tagged.insert(tagged.begin(), id3.begin(), id3.end());
	std::vector<uint8_t> v1(128, 0xFF);
	std::copy_n("TAG", 3, v1.begin());
	tagged.insert(tagged.end(), v1.begin(), v1.end());
	const auto audio = DecodeMpegStream(tagged);
	ASSERT_TRUE(audio.has_value());
	EXPECT_EQ(audio->samples, plain->samples);
}
