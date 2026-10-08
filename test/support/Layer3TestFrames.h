/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <vector>

/// Synthetic MPEG audio layer III streams for the decoder tests, written field by field from the format: a header, the
/// side information drawn from a small seeded generator, and main data of random bits. Each granule's bits end well
/// before the frame's, so that even a granule whose samples run past its own bits stays inside the main data; what is
/// left over is the bit reservoir the next frame may start in.
namespace openblack::test::layer3
{

/// Most significant bit first, as MPEG audio frames are written
class BitWriter
{
public:
	void Put(uint32_t value, int count)
	{
		for (int i = count - 1; i >= 0; --i)
		{
			if ((_bits & 7) == 0)
			{
				_bytes.push_back(0);
			}
			if (((value >> i) & 1) != 0)
			{
				_bytes.back() = static_cast<uint8_t>(_bytes.back() | 0x80u >> (_bits & 7));
			}
			++_bits;
		}
	}
	[[nodiscard]] const std::vector<uint8_t>& Bytes() const { return _bytes; }

private:
	std::vector<uint8_t> _bytes;
	size_t _bits {0};
};

enum class Version : uint8_t
{
	Mpeg1,
	Mpeg2,
	Mpeg25,
};

enum class Channels : uint8_t
{
	Mono,
	Stereo,
	JointStereo, ///< with the mid/side and intensity bits drawn per frame
};

struct StreamSpec
{
	Version version {Version::Mpeg1};
	uint8_t rateIndex {0};     ///< 0, 1, 2 (44100/48000/32000 Hz for MPEG-1, half for MPEG-2, a quarter for 2.5)
	uint8_t bitrateIndex {14}; ///< 1..14
	Channels channels {Channels::Stereo};
	uint32_t seed {1};
	uint32_t frames {4};
	bool shortBlocks {true};   ///< granules may switch to start, short (plain or mixed) and stop blocks
	uint8_t maxBigValues {24}; ///< 0..255 pairs from the big tables; more makes them run past their bits sooner
	bool silence {false};      ///< every granule without bits: frames of 0
	uint8_t firstBegin {0};    ///< where the first frame's main data starts: past 0 nothing has it, and the frame is skipped
};

/// The frame's size in bytes (no padding)
inline size_t FrameBytes(const StreamSpec& spec)
{
	static constexpr uint16_t k_Mpeg1[15] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320};
	static constexpr uint16_t k_Mpeg2[15] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160};
	static constexpr uint32_t k_Hz[3] = {44100, 48000, 32000};
	const bool mpeg1 = spec.version == Version::Mpeg1;
	const uint32_t kbps = mpeg1 ? k_Mpeg1[spec.bitrateIndex] : k_Mpeg2[spec.bitrateIndex];
	const uint32_t hz = k_Hz[spec.rateIndex] >> (mpeg1 ? 0 : 1) >> (spec.version == Version::Mpeg25 ? 1 : 0);
	return (mpeg1 ? 1152u : 576u) * kbps * 125u / hz;
}

/// The frames' samples (per channel)
inline uint32_t FrameSamples(const StreamSpec& spec)
{
	return spec.version == Version::Mpeg1 ? 1152 : 576;
}

/// The whole stream: `spec.frames` frames of the same format
inline std::vector<uint8_t> MakeStream(const StreamSpec& spec)
{
	uint32_t state = spec.seed;
	const auto draw = [&state](uint32_t below) {
		state = state * 1664525u + 1013904223u;
		return (state >> 8) % below;
	};
	const bool mpeg1 = spec.version == Version::Mpeg1;
	const bool mono = spec.channels == Channels::Mono;
	const int channels = mono ? 1 : 2;
	const int granules = mpeg1 ? 2 : 1;
	const size_t frameBytes = FrameBytes(spec);
	const size_t sideBytes = mpeg1 ? (mono ? 17 : 32) : (mono ? 9 : 17);
	// Bytes a granule may read past its own bits: scale factors longer than its bits, the big tables' longest pairs,
	// and the reader's look-ahead
	const size_t margin = static_cast<size_t>(spec.maxBigValues) * 47 / 8 + 48;

	std::vector<uint8_t> stream;
	size_t reservoir = 0; // the bytes after the last frame's granules
	for (uint32_t frame = 0; frame < spec.frames; ++frame)
	{
		BitWriter w;
		w.Put(0x7FF, 11);
		w.Put(mpeg1 ? 3 : spec.version == Version::Mpeg2 ? 2 : 0, 2);
		w.Put(1, 2); // layer III
		w.Put(1, 1); // no CRC
		w.Put(spec.bitrateIndex, 4);
		w.Put(spec.rateIndex, 2);
		w.Put(0, 2); // no padding, private bit
		w.Put(mono ? 3 : spec.channels == Channels::Stereo ? 0 : 1, 2);
		w.Put(spec.channels == Channels::JointStereo ? draw(4) : 0, 2);
		w.Put(0, 4);

		// The main data may start in the reservoir, what the last frame left
		const size_t maxBegin = mpeg1 ? 511 : 255;
		const size_t begin = frame == 0 ? spec.firstBegin : std::min({reservoir, maxBegin, size_t {draw(200)}});
		const size_t available = begin + frameBytes - 4 - sideBytes;
		const size_t usable = available > margin ? (available - margin) * 8 : 0;
		std::vector<uint32_t> lengths(static_cast<size_t>(granules * channels));
		uint32_t used = 0;
		for (auto& length : lengths)
		{
			const uint32_t share = static_cast<uint32_t>(usable / lengths.size());
			length = spec.silence ? 0 : std::min(4095u, share / 2 + draw(share / 2 + 1));
			used += length;
		}
		reservoir = available - (used + 7) / 8;

		if (mpeg1)
		{
			w.Put(static_cast<uint32_t>(begin), 9);
			w.Put(0, mono ? 5 : 3);                          // private bits
			w.Put(draw(1u << (4 * channels)), 4 * channels); // scale factor sharing of the second granule
		}
		else
		{
			w.Put(static_cast<uint32_t>(begin), 8);
			w.Put(0, channels); // private bits
		}
		for (size_t g = 0; g < lengths.size(); ++g)
		{
			w.Put(lengths[g], 12);
			w.Put(spec.silence ? 0 : draw(spec.maxBigValues + 1u), 9);
			w.Put(spec.silence ? 0 : 120 + draw(100), 8); // global gain
			w.Put(draw(mpeg1 ? 16 : 512), mpeg1 ? 4 : 9);
			const bool switched = spec.shortBlocks && draw(3) == 0;
			w.Put(switched ? 1 : 0, 1);
			if (switched)
			{
				w.Put(1 + draw(3), 2); // start, short or stop
				w.Put(draw(2), 1);     // mixed
				w.Put(draw(32), 5);
				w.Put(draw(32), 5);
				for (int i = 0; i < 3; ++i)
				{
					w.Put(draw(8), 3); // subblock gain
				}
			}
			else
			{
				w.Put(draw(32), 5);
				w.Put(draw(32), 5);
				w.Put(draw(32), 5);
				w.Put(draw(16), 4);
				w.Put(draw(8), 3);
			}
			if (mpeg1)
			{
				w.Put(draw(2), 1); // preflag
			}
			w.Put(draw(2), 1); // scalefac scale
			w.Put(draw(2), 1); // count1 table
		}
		auto bytes = w.Bytes();
		bytes.resize(frameBytes);
		for (size_t i = 4 + sideBytes; i < frameBytes; ++i)
		{
			bytes[i] = spec.silence ? 0 : static_cast<uint8_t>(draw(256));
		}
		stream.insert(stream.end(), bytes.begin(), bytes.end());
	}
	return stream;
}

/// A frame that holds a Xing ("Xing" for VBR, else "Info") header with a frame count and a LAME tag with the encoder's
/// delay and padding, in the format of the stream's first frame
inline std::vector<uint8_t> MakeXingFrame(const StreamSpec& spec, uint32_t frames, uint16_t delay, uint16_t padding,
                                          bool vbr = false, bool withFrames = true)
{
	auto silent = spec;
	silent.silence = true;
	silent.frames = 1;
	auto frame = MakeStream(silent);
	const bool mpeg1 = spec.version == Version::Mpeg1;
	const bool mono = spec.channels == Channels::Mono;
	size_t p = 4 + (mpeg1 ? (mono ? 17 : 32) : (mono ? 9 : 17));
	const char* id = vbr ? "Xing" : "Info";
	std::copy_n(id, 4, frame.begin() + static_cast<ptrdiff_t>(p));
	frame[p + 7] = withFrames ? 0x01 : 0x00;
	p += 8;
	if (withFrames)
	{
		frame[p++] = static_cast<uint8_t>(frames >> 24);
		frame[p++] = static_cast<uint8_t>(frames >> 16);
		frame[p++] = static_cast<uint8_t>(frames >> 8);
		frame[p++] = static_cast<uint8_t>(frames);
	}
	std::copy_n("LAME3.100", 9, frame.begin() + static_cast<ptrdiff_t>(p));
	p += 21;
	frame[p] = static_cast<uint8_t>(delay >> 4);
	frame[p + 1] = static_cast<uint8_t>((delay & 0xF) << 4 | (padding >> 8 & 0xF));
	frame[p + 2] = static_cast<uint8_t>(padding);
	return frame;
}

} // namespace openblack::test::layer3
