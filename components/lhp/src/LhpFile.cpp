/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LhpFile.h"

#include <algorithm>
#include <ranges>
#include <string_view>

using namespace openblack::lhp;

namespace
{

constexpr uint32_t k_Delta = 0x9E3779B9;
constexpr int k_Rounds = 32;
constexpr size_t k_PartSize = 8;
constexpr size_t k_NextOffset = k_NameSize + k_CategorySize;
constexpr size_t k_LengthOffset = k_NextOffset + 4;
constexpr size_t k_DataOffset = k_LengthOffset + 4;

[[nodiscard]] uint32_t ReadWord(std::span<const uint8_t> bytes, size_t at)
{
	return static_cast<uint32_t>(bytes[at]) | (static_cast<uint32_t>(bytes[at + 1]) << 8) |
	       (static_cast<uint32_t>(bytes[at + 2]) << 16) | (static_cast<uint32_t>(bytes[at + 3]) << 24);
}

void WriteWord(std::span<uint8_t> bytes, size_t at, uint32_t value)
{
	for (size_t i = 0; i < 4; ++i)
	{
		bytes[at + i] = static_cast<uint8_t>(value >> (8 * i));
	}
}

void WriteText(std::span<uint8_t> field, std::string_view text)
{
	// The last byte of the field stays zero, so the text always ends in the field
	const auto length = std::min(text.size(), field.size() - 1);
	std::ranges::copy(text.substr(0, length), field.begin());
}

[[nodiscard]] std::string ReadText(std::span<const uint8_t> field)
{
	const auto end = std::ranges::find(field, uint8_t {0});
	return {field.begin(), end};
}

template <typename Step>
void EachPart(std::span<uint8_t> bytes, Step step)
{
	auto key = k_Key;
	for (size_t at = 0; at + k_PartSize <= bytes.size(); at += k_PartSize)
	{
		uint32_t v0 = ReadWord(bytes, at);
		uint32_t v1 = ReadWord(bytes, at + 4);
		step(v0, v1, key);
		WriteWord(bytes, at, v0);
		WriteWord(bytes, at + 4, v1);
		for (size_t i = 0; i < key.size(); ++i)
		{
			key.at(i) += k_KeyStep.at(i);
		}
	}
}

} // namespace

void openblack::lhp::Encrypt(std::span<uint8_t> bytes)
{
	EachPart(bytes, [](uint32_t& v0, uint32_t& v1, const std::array<uint32_t, 4>& key) {
		uint32_t sum = 0;
		for (int round = 0; round < k_Rounds; ++round)
		{
			sum += k_Delta;
			v0 += ((v1 << 4) + key[0]) ^ (v1 + sum) ^ ((v1 >> 5) + key[1]);
			v1 += ((v0 << 4) + key[2]) ^ (v0 + sum) ^ ((v0 >> 5) + key[3]);
		}
	});
}

void openblack::lhp::Decrypt(std::span<uint8_t> bytes)
{
	EachPart(bytes, [](uint32_t& v0, uint32_t& v1, const std::array<uint32_t, 4>& key) {
		uint32_t sum = k_Delta * static_cast<uint32_t>(k_Rounds);
		for (int round = 0; round < k_Rounds; ++round)
		{
			v1 -= ((v0 << 4) + key[2]) ^ (v0 + sum) ^ ((v0 >> 5) + key[3]);
			v0 -= ((v1 << 4) + key[0]) ^ (v1 + sum) ^ ((v1 >> 5) + key[1]);
			sum -= k_Delta;
		}
	});
}

std::vector<uint8_t> openblack::lhp::Write(const Package& package)
{
	size_t plainSize = 0;
	for (const auto& block : package.blocks)
	{
		plainSize += k_RecordSize + block.data.size();
	}
	const auto partsSize = (plainSize + k_PartSize - 1) / k_PartSize * k_PartSize;

	std::vector<uint8_t> file(k_HeaderSize + partsSize + 4, 0);
	const std::span bytes {file};
	std::ranges::copy(k_Signature, bytes.begin());
	bytes[6] = static_cast<uint8_t>(k_Version);
	bytes[7] = static_cast<uint8_t>(k_Version >> 8);
	bytes[k_StampOffset] = static_cast<uint8_t>(package.stamp);
	bytes[k_StampOffset + 1] = static_cast<uint8_t>(package.stamp >> 8);

	const auto parts = bytes.subspan(k_HeaderSize, partsSize);
	size_t at = 0;
	for (size_t i = 0; i < package.blocks.size(); ++i)
	{
		const auto& block = package.blocks.at(i);
		const auto record = parts.subspan(at, k_RecordSize);
		WriteText(record.subspan(0, k_NameSize), block.name);
		WriteText(record.subspan(k_NameSize, k_CategorySize), block.category);
		const auto next = at + k_RecordSize + block.data.size();
		WriteWord(record, k_NextOffset, i + 1 < package.blocks.size() ? static_cast<uint32_t>(next) : 0u);
		WriteWord(record, k_LengthOffset, static_cast<uint32_t>(block.data.size()));
		WriteWord(record, k_DataOffset, static_cast<uint32_t>(at + k_RecordSize));
		std::ranges::copy(block.data, parts.begin() + static_cast<std::ptrdiff_t>(at + k_RecordSize));
		at = next;
	}
	Encrypt(parts);
	WriteWord(bytes, k_HeaderSize + partsSize, static_cast<uint32_t>(plainSize));
	return file;
}

std::optional<Package> openblack::lhp::Read(std::span<const uint8_t> bytes)
{
	if (bytes.size() < k_HeaderSize + 4 || !std::ranges::equal(bytes.first(k_Signature.size()), k_Signature,
	                                                           [](uint8_t a, char b) { return a == static_cast<uint8_t>(b); }))
	{
		return std::nullopt;
	}
	const auto partsSize = bytes.size() - k_HeaderSize - 4;
	const auto plainSize = ReadWord(bytes, bytes.size() - 4);
	if (partsSize % k_PartSize != 0 || plainSize > partsSize || partsSize - plainSize >= k_PartSize)
	{
		return std::nullopt;
	}

	Package package {.stamp = static_cast<uint16_t>(bytes[k_StampOffset] | (bytes[k_StampOffset + 1] << 8)), .blocks = {}};
	std::vector<uint8_t> plain(bytes.begin() + k_HeaderSize, bytes.end() - 4);
	Decrypt(plain);
	const std::span<const uint8_t> blocks {plain.data(), plainSize};
	size_t at = 0;
	while (at < blocks.size())
	{
		if (blocks.size() - at < k_RecordSize)
		{
			return std::nullopt;
		}
		const auto record = blocks.subspan(at, k_RecordSize);
		const auto length = ReadWord(record, k_LengthOffset);
		if (blocks.size() - at - k_RecordSize < length)
		{
			return std::nullopt;
		}
		const auto data = blocks.subspan(at + k_RecordSize, length);
		package.blocks.push_back({
		    .category = ReadText(record.subspan(k_NameSize, k_CategorySize)),
		    .name = ReadText(record.subspan(0, k_NameSize)),
		    .data = {data.begin(), data.end()},
		});
		at += k_RecordSize + length;
	}
	return package;
}
