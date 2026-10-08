/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BinkBundles.h"

#include <algorithm>
#include <bit>

#include "BinkTables.h"

using namespace openblack::video::bink;

namespace
{
/// The bits of a chunk count that can say up to `most` values: floor(log2(most + 511)) + 1
constexpr uint32_t CountBits(uint32_t most) noexcept
{
	return static_cast<uint32_t>(std::bit_width(most + 511));
}

/// The first DC of a chunk has 11 bits (one of them the sign for inter blocks)
constexpr uint32_t k_FirstDcBits = 11;
/// DC deltas come in groups of 8 sharing one bit size
constexpr uint32_t k_DcGroup = 8;

constexpr size_t Index(Source source) noexcept
{
	return static_cast<size_t>(source);
}

/// A 4-bit magnitude, then its sign bit when it is not 0
int16_t ReadSigned(BitReader& reader, uint32_t magnitude) noexcept
{
	const auto value = static_cast<int16_t>(magnitude);
	return magnitude != 0 && reader.ReadBit() ? static_cast<int16_t>(-value) : value;
}
} // namespace

Bundles::Bundles(size_t blocks)
{
	for (size_t i = 0; i < k_SourceCount; ++i)
	{
		// 64 bytes of room per block, as in the reference decoder: the DCs take two bytes each
		const bool dc = i == Index(Source::IntraDc) || i == Index(Source::InterDc);
		_bundles[i].values.assign(dc ? blocks * 32 : blocks * 64, 0);
	}
}

bool Bundles::StartPlane(BitReader& reader, uint32_t width, uint32_t blockWidth)
{
	const uint32_t aligned = (width + 7) & ~7u;
	_bundles[Index(Source::BlockTypes)].countBits = CountBits(aligned >> 3);
	_bundles[Index(Source::SubBlockTypes)].countBits = CountBits(aligned >> 4);
	_bundles[Index(Source::Colours)].countBits = CountBits(blockWidth * 64);
	_bundles[Index(Source::IntraDc)].countBits = CountBits(aligned >> 3);
	_bundles[Index(Source::InterDc)].countBits = CountBits(aligned >> 3);
	_bundles[Index(Source::XOffsets)].countBits = CountBits(aligned >> 3);
	_bundles[Index(Source::YOffsets)].countBits = CountBits(aligned >> 3);
	_bundles[Index(Source::Patterns)].countBits = CountBits(blockWidth * 8);
	_bundles[Index(Source::Runs)].countBits = CountBits(blockWidth * 48);

	for (size_t i = 0; i < k_SourceCount; ++i)
	{
		auto& bundle = _bundles[i];
		if (i == Index(Source::Colours))
		{
			for (auto& tree : _highNibbleTrees)
			{
				const auto read = ReadTree(reader);
				if (!read)
				{
					return false;
				}
				tree = *read;
			}
			_lastHighNibble = 0;
		}
		if (i != Index(Source::IntraDc) && i != Index(Source::InterDc))
		{
			const auto read = ReadTree(reader);
			if (!read)
			{
				return false;
			}
			bundle.tree = *read;
		}
		bundle.decoded = 0;
		bundle.taken = 0;
		bundle.ended = false;
	}
	return true;
}

bool Bundles::ReadRow(BitReader& reader)
{
	auto& b = _bundles;
	return ReadBlockTypes(reader, b[Index(Source::BlockTypes)]) && ReadBlockTypes(reader, b[Index(Source::SubBlockTypes)]) &&
	       ReadColours(reader, b[Index(Source::Colours)]) && ReadPatterns(reader, b[Index(Source::Patterns)]) &&
	       ReadOffsets(reader, b[Index(Source::XOffsets)]) && ReadOffsets(reader, b[Index(Source::YOffsets)]) &&
	       ReadDcs(reader, b[Index(Source::IntraDc)], false) && ReadDcs(reader, b[Index(Source::InterDc)], true) &&
	       ReadRuns(reader, b[Index(Source::Runs)]);
}

int32_t Bundles::Next(Source source) noexcept
{
	auto& bundle = _bundles[Index(source)];
	if (bundle.taken >= bundle.values.size())
	{
		return 0;
	}
	return bundle.values[bundle.taken++];
}

uint32_t Bundles::ChunkSize(BitReader& reader, Bundle& bundle) noexcept
{
	if (bundle.ended || bundle.decoded > bundle.taken)
	{
		return 0;
	}
	const uint32_t count = reader.Read(bundle.countBits);
	if (count == 0)
	{
		bundle.ended = true;
	}
	return count;
}

bool Bundles::ReadBlockTypes(BitReader& reader, Bundle& bundle)
{
	const uint32_t count = ChunkSize(reader, bundle);
	if (count == 0)
	{
		return true;
	}
	const size_t end = bundle.decoded + count;
	if (end > bundle.values.size() || reader.BitsLeft() < 1)
	{
		return false;
	}
	auto* const values = bundle.values.data();
	if (reader.ReadBit())
	{
		std::fill(values + bundle.decoded, values + end, static_cast<int16_t>(reader.Read(4)));
		bundle.decoded = end;
		return true;
	}
	int16_t last = 0;
	while (bundle.decoded < end)
	{
		const uint8_t symbol = ReadSymbol(reader, bundle.tree);
		if (symbol < 12)
		{
			last = symbol;
			values[bundle.decoded++] = symbol;
			continue;
		}
		// 12 to 15: the last type again 4, 8, 12 or 32 times
		const size_t run = k_BlockTypeRuns[symbol - 12];
		if (end - bundle.decoded < run)
		{
			return false;
		}
		std::fill_n(values + bundle.decoded, run, last);
		bundle.decoded += run;
	}
	return true;
}

uint8_t Bundles::ReadColour(BitReader& reader, const Bundle& bundle) noexcept
{
	_lastHighNibble = ReadSymbol(reader, _highNibbleTrees[_lastHighNibble]);
	return static_cast<uint8_t>(_lastHighNibble << 4 | ReadSymbol(reader, bundle.tree));
}

bool Bundles::ReadColours(BitReader& reader, Bundle& bundle)
{
	const uint32_t count = ChunkSize(reader, bundle);
	if (count == 0)
	{
		return true;
	}
	const size_t end = bundle.decoded + count;
	if (end > bundle.values.size() || reader.BitsLeft() < 1)
	{
		return false;
	}
	auto* const values = bundle.values.data();
	if (reader.ReadBit())
	{
		std::fill(values + bundle.decoded, values + end, static_cast<int16_t>(ReadColour(reader, bundle)));
		bundle.decoded = end;
		return true;
	}
	while (bundle.decoded < end)
	{
		if (reader.BitsLeft() < 2)
		{
			return false;
		}
		values[bundle.decoded++] = ReadColour(reader, bundle);
	}
	return true;
}

bool Bundles::ReadPatterns(BitReader& reader, Bundle& bundle)
{
	const uint32_t count = ChunkSize(reader, bundle);
	if (count == 0)
	{
		return true;
	}
	const size_t end = bundle.decoded + count;
	if (end > bundle.values.size())
	{
		return false;
	}
	while (bundle.decoded < end)
	{
		if (reader.BitsLeft() < 2)
		{
			return false;
		}
		const uint8_t low = ReadSymbol(reader, bundle.tree);
		const uint8_t high = ReadSymbol(reader, bundle.tree);
		bundle.values[bundle.decoded++] = static_cast<int16_t>(high << 4 | low);
	}
	return true;
}

bool Bundles::ReadOffsets(BitReader& reader, Bundle& bundle)
{
	const uint32_t count = ChunkSize(reader, bundle);
	if (count == 0)
	{
		return true;
	}
	const size_t end = bundle.decoded + count;
	if (end > bundle.values.size() || reader.BitsLeft() < 1)
	{
		return false;
	}
	auto* const values = bundle.values.data();
	if (reader.ReadBit())
	{
		const int16_t value = ReadSigned(reader, reader.Read(4));
		std::fill(values + bundle.decoded, values + end, value);
		bundle.decoded = end;
		return true;
	}
	while (bundle.decoded < end)
	{
		values[bundle.decoded++] = ReadSigned(reader, ReadSymbol(reader, bundle.tree));
	}
	return true;
}

bool Bundles::ReadDcs(BitReader& reader, Bundle& bundle, bool hasSign)
{
	const uint32_t count = ChunkSize(reader, bundle);
	if (count == 0)
	{
		return true;
	}
	const uint32_t firstBits = k_FirstDcBits - (hasSign ? 1 : 0);
	if (reader.BitsLeft() < firstBits || bundle.decoded >= bundle.values.size())
	{
		return false;
	}
	int32_t value = reader.Read(firstBits);
	if (hasSign && value != 0 && reader.ReadBit())
	{
		value = -value;
	}
	bundle.values[bundle.decoded++] = static_cast<int16_t>(value);
	const uint32_t rest = count - 1;
	for (uint32_t i = 0; i < rest; i += k_DcGroup)
	{
		const uint32_t group = std::min(rest - i, k_DcGroup);
		if (bundle.values.size() - bundle.decoded < group)
		{
			return false;
		}
		const uint32_t deltaBits = reader.Read(4);
		for (uint32_t j = 0; j < group; ++j)
		{
			if (deltaBits != 0)
			{
				int32_t delta = reader.Read(deltaBits);
				if (delta != 0 && reader.ReadBit())
				{
					delta = -delta;
				}
				value += delta;
				if (value < INT16_MIN || value > INT16_MAX)
				{
					return false;
				}
			}
			bundle.values[bundle.decoded++] = static_cast<int16_t>(value);
		}
	}
	return true;
}

bool Bundles::ReadRuns(BitReader& reader, Bundle& bundle)
{
	const uint32_t count = ChunkSize(reader, bundle);
	if (count == 0)
	{
		return true;
	}
	const size_t end = bundle.decoded + count;
	if (end > bundle.values.size() || reader.BitsLeft() < 1)
	{
		return false;
	}
	auto* const values = bundle.values.data();
	if (reader.ReadBit())
	{
		std::fill(values + bundle.decoded, values + end, static_cast<int16_t>(reader.Read(4)));
		bundle.decoded = end;
		return true;
	}
	while (bundle.decoded < end)
	{
		values[bundle.decoded++] = ReadSymbol(reader, bundle.tree);
	}
	return true;
}
