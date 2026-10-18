/******************************************************************************
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
#include <optional>
#include <span>
#include <string>
#include <vector>

/// The package file of the game (.lhp), for example creature.lhp, which the game writes for the creature upload tool.
///
/// The file has three parts:
/// - A header of 36 bytes: the text "LHNILD", the version 1 as a 16-bit number at byte 6, a 16-bit stamp at byte 24,
///   and zero bytes in all other places.
/// - The blocks, encrypted with TEA in parts of 8 bytes. The key changes after each part. The last part is filled with
///   zero bytes to 8 bytes.
/// - The length of the blocks before the encryption, as a 32-bit number.
///
/// All numbers are little-endian. Each block has a record of 172 bytes, then its data. The record has the name (150
/// bytes), the category (10 bytes), the link to the next block, the length of the data and the location of the data.
/// The game writes its memory addresses in the two links. This writer writes the offset in the blocks of the next
/// block (zero for the last block) and of the block's data.
namespace openblack::lhp
{

constexpr std::array<char, 6> k_Signature {'L', 'H', 'N', 'I', 'L', 'D'};
constexpr uint16_t k_Version = 1;
constexpr size_t k_HeaderSize = 36;
constexpr size_t k_StampOffset = 24;
constexpr size_t k_RecordSize = 172;
constexpr size_t k_NameSize = 150;
constexpr size_t k_CategorySize = 10;
/// The key of the first part of 8 bytes
constexpr std::array<uint32_t, 4> k_Key {0x429C, 0x1D83, 0x7BCA, 0x5D10};
/// What the game adds to each word of the key after each part of 8 bytes
constexpr std::array<uint32_t, 4> k_KeyStep {4, static_cast<uint32_t>(-0x4A), 0xAC, static_cast<uint32_t>(-0x1A)};

/// One block of a package
struct Block
{
	/// At most 9 characters: a longer category is cut
	std::string category;
	/// At most 149 characters: a longer name is cut
	std::string name;
	std::vector<uint8_t> data;
};

struct Package
{
	/// A random number that the game puts in the header each time that it writes the file
	uint16_t stamp {0};
	std::vector<Block> blocks;
};

/// Encrypts or decrypts the bytes in place, in parts of 8 bytes. The length must be a multiple of 8.
void Encrypt(std::span<uint8_t> bytes);
void Decrypt(std::span<uint8_t> bytes);

/// The bytes of the file
[[nodiscard]] std::vector<uint8_t> Write(const Package& package);

/// The package in the bytes of a file, none if the bytes are not a package
[[nodiscard]] std::optional<Package> Read(std::span<const uint8_t> bytes);

} // namespace openblack::lhp
