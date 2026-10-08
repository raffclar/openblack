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

#include <array>
#include <vector>

/// Synthetic MPEG audio layer II frames for the decoder tests, written field by field from the format: a header, the
/// bit allocation, the scale factor selection, the scale factors and the samples, all from a small seeded generator.
namespace openblack::test
{

/// Most significant bit first, as MPEG audio frames are written
class MsbBitWriter
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
	[[nodiscard]] size_t Bits() const { return _bits; }
	[[nodiscard]] std::vector<uint8_t> Bytes(size_t size) const
	{
		auto bytes = _bytes;
		bytes.resize(size, 0);
		return bytes;
	}

private:
	std::vector<uint8_t> _bytes;
	size_t _bits {0};
};

struct Layer2FrameSpec
{
	bool mpeg1 {false};    ///< MPEG-1 (else MPEG-2, half the sample rates)
	uint8_t rateIndex {0}; ///< 0, 1, 2: 44100/48000/32000 (MPEG-1) or 22050/24000/16000 (MPEG-2)
	uint8_t bitrateIndex {8};
	bool stereo {false}; ///< stereo, else mono
	uint32_t seed {1};
	uint32_t largestCode {1}; ///< the bit allocation codes are drawn from 0..largestCode (clamped to each band's width)
};

/// The frame's size in bytes (no padding)
inline size_t Layer2FrameBytes(const Layer2FrameSpec& spec)
{
	static constexpr uint16_t k_Mpeg1[15] = {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384};
	static constexpr uint16_t k_Mpeg2[15] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160};
	static constexpr uint32_t k_Hz[3] = {44100, 48000, 32000};
	const uint32_t kbps = spec.mpeg1 ? k_Mpeg1[spec.bitrateIndex] : k_Mpeg2[spec.bitrateIndex];
	const uint32_t hz = k_Hz[spec.rateIndex] >> (spec.mpeg1 ? 0 : 1);
	return 1152u * kbps * 125u / hz;
}

/// A whole frame as the spec says, zero-filled to its size (the generator's choices may not fit: then the frame is
/// one a decoder must refuse)
inline std::vector<uint8_t> MakeLayer2Frame(const Layer2FrameSpec& spec)
{
	// The format's tables of bit allocation codes, and which rows the bands use
	static constexpr std::array<uint8_t, 76> k_Codes = {
	    0, 17, 3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 0,  17, 18, 3,  19, 4,  5,  6, 7, 8,
	    9, 10, 11, 12, 13, 16, 0,  17, 18, 3,  19, 4,  5,  16, 0,  17, 18, 16, 0,  17, 18, 19, 4,  5, 6, 7,
	    8, 9,  10, 11, 12, 13, 14, 15, 0,  17, 18, 3,  19, 4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14};
	struct Row
	{
		uint8_t offset;
		uint8_t width;
		uint8_t count;
	};
	std::vector<Row> rows;
	int bands = 30;
	if (!spec.mpeg1)
	{
		rows = {{60, 4, 4}, {44, 3, 7}, {44, 2, 19}};
	}
	else
	{
		static constexpr uint16_t k_Mpeg1[15] = {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384};
		const uint32_t perChannel = k_Mpeg1[spec.bitrateIndex] >> (spec.stereo ? 1 : 0);
		rows = {{0, 4, 3}, {16, 4, 8}, {32, 3, 12}, {40, 2, 7}};
		bands = 27;
		if (perChannel < 56)
		{
			rows = {{44, 4, 2}, {44, 3, 10}};
			bands = spec.rateIndex == 2 ? 12 : 8;
		}
		else if (perChannel >= 96 && spec.rateIndex != 1)
		{
			bands = 30;
		}
	}
	const int channels = spec.stereo ? 2 : 1;
	uint32_t state = spec.seed;
	const auto draw = [&state](uint32_t below) {
		state = state * 1664525u + 1013904223u;
		return (state >> 8) % below;
	};

	MsbBitWriter w;
	w.Put(0xFFF, 12);
	w.Put(spec.mpeg1 ? 1 : 0, 1);
	w.Put(2, 2); // layer II
	w.Put(1, 1); // no CRC
	w.Put(spec.bitrateIndex, 4);
	w.Put(spec.rateIndex, 2);
	w.Put(0, 2); // no padding, private bit
	w.Put(spec.stereo ? 0 : 3, 2);
	w.Put(0, 6);

	std::array<uint8_t, 64> allocation {};
	size_t row = 0;
	int rowEnd = rows[0].count;
	for (int band = 0; band < bands; ++band)
	{
		if (band == rowEnd)
		{
			++row;
			rowEnd += rows[row].count;
		}
		for (int c = 0; c < channels; ++c)
		{
			const uint32_t code = draw(std::min((1u << rows[row].width) - 1, spec.largestCode) + 1);
			w.Put(code, rows[row].width);
			allocation[static_cast<size_t>(2 * band + c)] = k_Codes[rows[row].offset + code];
		}
	}
	std::array<uint8_t, 64> scfsi {};
	for (int i = 0; i < 2 * bands; ++i)
	{
		if (allocation[static_cast<size_t>(i)] != 0)
		{
			scfsi[static_cast<size_t>(i)] = static_cast<uint8_t>(draw(4));
			w.Put(scfsi[static_cast<size_t>(i)], 2);
		}
	}
	for (int i = 0; i < 2 * bands; ++i)
	{
		if (allocation[static_cast<size_t>(i)] != 0)
		{
			static constexpr int k_Count[4] = {3, 2, 1, 2};
			for (int k = 0; k < k_Count[scfsi[static_cast<size_t>(i)]]; ++k)
			{
				w.Put(draw(63), 6);
			}
		}
	}
	for (int part = 0; part < 12; ++part)
	{
		for (int i = 0; i < 2 * bands; ++i)
		{
			const int ba = allocation[static_cast<size_t>(i)];
			if (ba == 0)
			{
				continue;
			}
			if (ba < 17)
			{
				for (int k = 0; k < 3; ++k)
				{
					w.Put(draw(1u << ba), ba);
				}
			}
			else
			{
				const uint32_t levels = (2u << (ba - 17)) + 1;
				w.Put(draw(levels * levels * levels), static_cast<int>(levels + 2 - (levels >> 3)));
			}
		}
	}
	return w.Bytes(Layer2FrameBytes(spec));
}

} // namespace openblack::test
