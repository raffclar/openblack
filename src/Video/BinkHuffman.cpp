/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BinkHuffman.h"

#include <cstddef>

#include <span>
#include <utility>

#include "BinkTables.h"

using namespace openblack::video::bink;

namespace
{
struct Leaf
{
	uint8_t leaf;
	uint8_t length;
};

constexpr size_t k_LookupSize = size_t {1} << k_MaxCodeLength;
using Lookup = std::array<std::array<Leaf, k_LookupSize>, 16>;

/// For each tree, the leaf the next k_MaxCodeLength bits start with: every entry whose low bits are a leaf's code
constexpr Lookup MakeLookup() noexcept
{
	Lookup lookup {};
	for (size_t tree = 0; tree < 16; ++tree)
	{
		for (size_t leaf = 0; leaf < 16; ++leaf)
		{
			const uint32_t length = k_TreeLengths[tree][leaf];
			const uint32_t mask = (1u << length) - 1;
			for (size_t bits = 0; bits < k_LookupSize; ++bits)
			{
				if ((bits & mask) == k_TreeCodes[tree][leaf])
				{
					lookup[tree][bits] = {static_cast<uint8_t>(leaf), static_cast<uint8_t>(length)};
				}
			}
		}
	}
	return lookup;
}

constexpr Lookup k_Lookup = MakeLookup();

/// One pass of the merge sort: the two halves of `in` become `out`, each bit choosing whether the next symbol comes
/// from the first half (0) or the second (1)
void Merge(BitReader& reader, std::span<uint8_t> out, std::span<const uint8_t> in) noexcept
{
	const size_t size = in.size() / 2;
	size_t first = 0;
	size_t second = size;
	size_t next = 0;
	while (first < size && second < in.size())
	{
		out[next++] = reader.ReadBit() ? in[second++] : in[first++];
	}
	while (first < size)
	{
		out[next++] = in[first++];
	}
	while (second < in.size())
	{
		out[next++] = in[second++];
	}
}
} // namespace

std::optional<Tree> openblack::video::bink::ReadTree(BitReader& reader) noexcept
{
	if (reader.BitsLeft() < 4)
	{
		return std::nullopt;
	}
	Tree tree;
	tree.index = static_cast<uint8_t>(reader.Read(4));
	if (tree.index == 0)
	{
		return tree;
	}
	if (reader.ReadBit())
	{
		// The first symbols listed, then the others in increasing order
		std::array<bool, 16> listed {};
		uint32_t last = reader.Read(3);
		for (uint32_t i = 0; i <= last; ++i)
		{
			tree.symbols[i] = static_cast<uint8_t>(reader.Read(4));
			listed[tree.symbols[i]] = true;
		}
		for (uint8_t symbol = 0; symbol < 16 && last < 15; ++symbol)
		{
			if (!listed[symbol])
			{
				tree.symbols[++last] = symbol;
			}
		}
	}
	else
	{
		// Merge passes over runs of 1, 2, 4 and 8 symbols, as many as the 2-bit count says (1 to 4)
		std::array<uint8_t, 16> in = tree.symbols;
		std::array<uint8_t, 16> out {};
		const uint32_t passes = reader.Read(2) + 1;
		for (uint32_t pass = 0; pass < passes; ++pass)
		{
			const size_t size = size_t {1} << pass;
			for (size_t start = 0; start < 16; start += size * 2)
			{
				Merge(reader, std::span(out).subspan(start, size * 2), std::span(in).subspan(start, size * 2));
			}
			std::swap(in, out);
		}
		tree.symbols = in;
	}
	return tree;
}

uint8_t openblack::video::bink::ReadSymbol(BitReader& reader, const Tree& tree) noexcept
{
	const Leaf leaf = k_Lookup[tree.index & 15][reader.Peek(k_MaxCodeLength)];
	reader.Skip(leaf.length);
	return tree.symbols[leaf.leaf];
}
