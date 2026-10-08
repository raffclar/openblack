/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The order the coefficients and residues are read in, and the integer inverse DCT, are fixed by the Bink 1 format and
// follow FFmpeg's Bink video decoder (libavcodec/bink.c and binkdsp.c, Copyright (c) 2009 Konstantin Shishkov,
// Copyright (C) 2011 Peter Ross), licensed under the GNU Lesser General Public License version 2.1 or later and used
// here under the GNU General Public License version 3, as that licence allows.

#include "BinkBlocks.h"

#include <algorithm>

#include "BinkTables.h"

using namespace openblack::video::bink;

namespace
{
/// The coefficients are found by splitting groups of four: the list holds (first coefficient, mode) pairs
enum class Mode : uint8_t
{
	Empty,     ///< a group of 4 not split yet (or a used entry when its coefficient is 0)
	Split,     ///< a group of 16, split into four groups of 4
	Group,     ///< a group of 4 whose coefficients are read one by one
	Candidate, ///< one coefficient, not significant yet
};

/// Room in the list: entries are added below the start (candidates) and above the end (groups)
constexpr int k_ListMiddle = 64;
constexpr int k_ListSize = 128;

struct List
{
	std::array<int32_t, k_ListSize> coefficient {};
	std::array<Mode, k_ListSize> mode {};
	int start {k_ListMiddle};
	int end {k_ListMiddle};

	void Append(int32_t first, Mode m) noexcept
	{
		coefficient[static_cast<size_t>(end)] = first;
		mode[static_cast<size_t>(end)] = m;
		++end;
	}
	void Prepend(int32_t first, Mode m) noexcept
	{
		--start;
		coefficient[static_cast<size_t>(start)] = first;
		mode[static_cast<size_t>(start)] = m;
	}
	/// Whether an entry of `mode` starting at `first` can be expanded: its coefficients are inside the block, and the
	/// list and the `count` coefficients found so far have room for what it adds (only damaged data runs out)
	[[nodiscard]] bool CanExpand(Mode m, int32_t first, size_t count) const noexcept
	{
		switch (m)
		{
		case Mode::Empty:
		case Mode::Group:
			return first >= 0 && first <= 60 && start >= 4 && count + 4 <= 64;
		case Mode::Split:
			return first >= 0 && first <= 48 && end + 3 <= k_ListSize;
		case Mode::Candidate:
			return first >= 0 && first <= 63 && count + 1 <= 64;
		}
		return false;
	}
};

/// The starting list: three groups of 16 and the first three coefficients after the DC
List StartingList(bool residue) noexcept
{
	List list;
	list.Append(4, Mode::Empty);
	list.Append(24, Mode::Empty);
	list.Append(44, Mode::Empty);
	if (residue)
	{
		list.Append(0, Mode::Group);
	}
	else
	{
		list.Append(1, Mode::Candidate);
		list.Append(2, Mode::Candidate);
		list.Append(3, Mode::Candidate);
	}
	return list;
}

/// A coefficient that becomes significant at bit plane `bits`: the top bit set, `bits` more bits, then the sign
int32_t ReadCoefficient(BitReader& reader, uint32_t bits) noexcept
{
	if (bits == 0)
	{
		return reader.ReadBit() ? -1 : 1;
	}
	const auto value = static_cast<int32_t>(reader.Read(bits) | 1u << bits);
	return reader.ReadBit() ? -value : value;
}

/// a * k with 32-bit wrapping, then 11 fraction bits off
constexpr int32_t Multiply(int32_t a, int32_t k) noexcept
{
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(k)) >> 11;
}

constexpr int32_t k_A1 = 2896; // cos(pi / 4), 12 fraction bits
constexpr int32_t k_A2 = 2217;
constexpr int32_t k_A3 = 3784;
constexpr int32_t k_A4 = -5352;

/// The 8-point inverse transform of `in(0)` .. `in(7)`
template <typename In>
constexpr std::array<int32_t, 8> Transform(In in) noexcept
{
	const int32_t a0 = in(0) + in(4);
	const int32_t a1 = in(0) - in(4);
	const int32_t a2 = in(2) + in(6);
	const int32_t a3 = Multiply(k_A1, in(2) - in(6));
	const int32_t a4 = in(5) + in(3);
	const int32_t a5 = in(5) - in(3);
	const int32_t a6 = in(1) + in(7);
	const int32_t a7 = in(1) - in(7);
	const int32_t b0 = a4 + a6;
	const int32_t b1 = Multiply(k_A3, a5 + a7);
	const int32_t b2 = Multiply(k_A4, a5) - b0 + b1;
	const int32_t b3 = Multiply(k_A1, a6 - a4) - b2;
	const int32_t b4 = Multiply(k_A2, a7) + b3 - b1;
	return {a0 + a2 + b0, a1 + a3 - a2 + b2, a1 - a3 + a2 + b3, a0 - a2 - b4,
	        a0 - a2 + b4, a1 - a3 + a2 - b3, a1 + a3 - a2 - b2, a0 + a2 - b0};
}
} // namespace

std::optional<uint32_t> openblack::video::bink::ReadDctCoefficients(BitReader& reader, Coefficients& block,
                                                                    CodedCoefficients& coded) noexcept
{
	if (reader.BitsLeft() < 4)
	{
		return std::nullopt;
	}
	coded.count = 0;
	List list = StartingList(false);
	const auto set = [&](int32_t position, int32_t value) {
		block[k_CoefficientScan[static_cast<size_t>(position)]] = value;
		coded.positions[coded.count++] = static_cast<uint8_t>(position);
	};

	// Bit planes from the highest: each pass finds the coefficients that become significant at that plane
	for (int bits = static_cast<int>(reader.Read(4)) - 1; bits >= 0; --bits)
	{
		int pos = list.start;
		while (pos < list.end)
		{
			const auto index = static_cast<size_t>(pos);
			if ((list.mode[index] == Mode::Empty && list.coefficient[index] == 0) || !reader.ReadBit())
			{
				++pos;
				continue;
			}
			int32_t first = list.coefficient[index];
			const Mode mode = list.mode[index];
			if (!list.CanExpand(mode, first, coded.count))
			{
				return std::nullopt;
			}
			switch (mode)
			{
			case Mode::Empty:
			case Mode::Group:
				if (mode == Mode::Empty)
				{
					// The group of 16 is split: this entry stays as its next group of 4
					list.coefficient[index] = first + 4;
					list.mode[index] = Mode::Split;
				}
				else
				{
					list.coefficient[index] = 0;
					list.mode[index] = Mode::Empty;
					++pos;
				}
				for (int i = 0; i < 4; ++i, ++first)
				{
					if (reader.ReadBit())
					{
						list.Prepend(first, Mode::Candidate);
					}
					else
					{
						set(first, ReadCoefficient(reader, static_cast<uint32_t>(bits)));
					}
				}
				break;
			case Mode::Split:
				list.mode[index] = Mode::Group;
				for (int i = 0; i < 3; ++i)
				{
					first += 4;
					list.Append(first, Mode::Group);
				}
				break;
			case Mode::Candidate:
				set(first, ReadCoefficient(reader, static_cast<uint32_t>(bits)));
				list.coefficient[index] = 0;
				list.mode[index] = Mode::Empty;
				++pos;
				break;
			}
		}
	}
	return reader.Read(4);
}

void openblack::video::bink::Dequantise(Coefficients& block, std::span<const uint32_t, 64> quantiser,
                                        const CodedCoefficients& coded) noexcept
{
	const auto scale = [](int32_t value, uint32_t q) { return static_cast<int32_t>(static_cast<uint32_t>(value) * q) >> 11; };
	block[0] = scale(block[0], quantiser[0]);
	for (size_t i = 0; i < coded.count; ++i)
	{
		const uint8_t position = coded.positions[i];
		auto& value = block[k_CoefficientScan[position]];
		value = scale(value, quantiser[position]);
	}
}

void openblack::video::bink::InverseDct(Coefficients& block) noexcept
{
	// Columns first; a column with only its first coefficient is that value throughout
	Coefficients columns {};
	for (size_t x = 0; x < 8; ++x)
	{
		bool flat = true;
		for (size_t row = 1; row < 8; ++row)
		{
			flat = flat && block[row * 8 + x] == 0;
		}
		if (flat)
		{
			for (size_t row = 0; row < 8; ++row)
			{
				columns[row * 8 + x] = block[x];
			}
			continue;
		}
		const auto out = Transform([&](size_t i) { return block[i * 8 + x]; });
		for (size_t row = 0; row < 8; ++row)
		{
			columns[row * 8 + x] = out[row];
		}
	}
	// Then rows, rounded off to the pixel scale
	for (size_t row = 0; row < 8; ++row)
	{
		const auto out = Transform([&](size_t i) { return columns[row * 8 + i]; });
		for (size_t x = 0; x < 8; ++x)
		{
			block[row * 8 + x] = (out[x] + 0x7F) >> 8;
		}
	}
}

std::array<int16_t, 64> openblack::video::bink::ReadResidue(BitReader& reader, int32_t masks) noexcept
{
	std::array<int16_t, 64> block {};
	std::array<uint8_t, 64> nonZero {};
	size_t nonZeroCount = 0;
	List list = StartingList(true);

	for (int32_t mask = 1 << reader.Read(3); mask != 0; mask >>= 1)
	{
		// Refine the coefficients already significant
		for (size_t i = 0; i < nonZeroCount; ++i)
		{
			if (!reader.ReadBit())
			{
				continue;
			}
			auto& value = block[nonZero[i]];
			value = static_cast<int16_t>(value < 0 ? value - mask : value + mask);
			if (--masks < 0)
			{
				return block;
			}
		}
		// Then find the new ones, as the DCT coefficients
		const auto set = [&](int32_t position) {
			const uint8_t pixel = k_CoefficientScan[static_cast<size_t>(position)];
			nonZero[nonZeroCount++] = pixel;
			block[pixel] = static_cast<int16_t>(reader.ReadBit() ? -mask : mask);
			return --masks >= 0;
		};
		int pos = list.start;
		while (pos < list.end)
		{
			const auto index = static_cast<size_t>(pos);
			if ((list.mode[index] == Mode::Empty && list.coefficient[index] == 0) || !reader.ReadBit())
			{
				++pos;
				continue;
			}
			int32_t first = list.coefficient[index];
			const Mode mode = list.mode[index];
			if (!list.CanExpand(mode, first, nonZeroCount))
			{
				return block;
			}
			switch (mode)
			{
			case Mode::Empty:
			case Mode::Group:
				if (mode == Mode::Empty)
				{
					list.coefficient[index] = first + 4;
					list.mode[index] = Mode::Split;
				}
				else
				{
					list.coefficient[index] = 0;
					list.mode[index] = Mode::Empty;
					++pos;
				}
				for (int i = 0; i < 4; ++i, ++first)
				{
					if (reader.ReadBit())
					{
						list.Prepend(first, Mode::Candidate);
					}
					else if (!set(first))
					{
						return block;
					}
				}
				break;
			case Mode::Split:
				list.mode[index] = Mode::Group;
				for (int i = 0; i < 3; ++i)
				{
					first += 4;
					list.Append(first, Mode::Group);
				}
				break;
			case Mode::Candidate:
				list.coefficient[index] = 0;
				list.mode[index] = Mode::Empty;
				++pos;
				if (!set(first))
				{
					return block;
				}
				break;
			}
		}
	}
	return block;
}

void openblack::video::bink::PutBlock(const Plane& plane, size_t offset, const Coefficients& block) noexcept
{
	for (size_t row = 0; row < 8; ++row)
	{
		for (size_t x = 0; x < 8; ++x)
		{
			plane.pixels[offset + row * plane.stride + x] = static_cast<uint8_t>(block[row * 8 + x]);
		}
	}
}

void openblack::video::bink::AddBlock(const Plane& plane, size_t offset, std::span<const int32_t, 64> block) noexcept
{
	for (size_t row = 0; row < 8; ++row)
	{
		for (size_t x = 0; x < 8; ++x)
		{
			auto& pixel = plane.pixels[offset + row * plane.stride + x];
			pixel = static_cast<uint8_t>(pixel + block[row * 8 + x]);
		}
	}
}

void openblack::video::bink::AddBlock(const Plane& plane, size_t offset, std::span<const int16_t, 64> block) noexcept
{
	for (size_t row = 0; row < 8; ++row)
	{
		for (size_t x = 0; x < 8; ++x)
		{
			auto& pixel = plane.pixels[offset + row * plane.stride + x];
			pixel = static_cast<uint8_t>(pixel + block[row * 8 + x]);
		}
	}
}

void openblack::video::bink::CopyBlock(const Plane& plane, size_t offset, const Plane& source, size_t from) noexcept
{
	for (size_t row = 0; row < 8; ++row)
	{
		std::array<uint8_t, 8> line {};
		std::copy_n(source.pixels.begin() + static_cast<ptrdiff_t>(from + row * source.stride), 8, line.begin());
		std::copy(line.begin(), line.end(), plane.pixels.begin() + static_cast<ptrdiff_t>(offset + row * plane.stride));
	}
}

void openblack::video::bink::FillBlock(const Plane& plane, size_t offset, uint8_t value, size_t size) noexcept
{
	for (size_t row = 0; row < size; ++row)
	{
		std::fill_n(plane.pixels.begin() + static_cast<ptrdiff_t>(offset + row * plane.stride), size, value);
	}
}

void openblack::video::bink::ScaleBlock(const Plane& plane, size_t offset, std::span<const uint8_t, 64> block) noexcept
{
	for (size_t row = 0; row < 16; ++row)
	{
		for (size_t x = 0; x < 16; ++x)
		{
			plane.pixels[offset + row * plane.stride + x] = block[(row / 2) * 8 + x / 2];
		}
	}
}
