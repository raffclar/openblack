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
#include <optional>
#include <span>

#include "BinkBitReader.h"

/// The 8x8 block operations of Bink 1 video: reading DCT coefficients and residues, the inverse DCT, and copying,
/// filling and scaling blocks inside a plane. Pixel arithmetic wraps modulo 256 as the format expects (no clamping).
namespace openblack::video::bink
{

/// A plane of pixels: `stride` bytes per row
struct Plane
{
	std::span<uint8_t> pixels;
	size_t stride;
};

/// The DCT coefficients of a block, in pixel order (row * 8 + x)
using Coefficients = std::array<int32_t, 64>;

/// The coefficients a block coded: their positions in coding order (BinkTables.h k_CoefficientScan)
struct CodedCoefficients
{
	std::array<uint8_t, 64> positions;
	size_t count {0};
};

/// Reads the coefficients after the first (which comes from a bundle) into `block`, and the quantiser index (0 to 15)
/// that follows them. Nullopt: the data is damaged
[[nodiscard]] std::optional<uint32_t> ReadDctCoefficients(BitReader& reader, Coefficients& block,
                                                          CodedCoefficients& coded) noexcept;
/// Each coded coefficient and the first times its quantiser, with 11 fraction bits
void Dequantise(Coefficients& block, std::span<const uint32_t, 64> quantiser, const CodedCoefficients& coded) noexcept;
/// The inverse DCT, in place: the pixel values (or differences) of the block
void InverseDct(Coefficients& block) noexcept;

/// Reads the difference a residue block adds after its motion copy: `masks` + 1 bits set at most, by bit planes
[[nodiscard]] std::array<int16_t, 64> ReadResidue(BitReader& reader, int32_t masks) noexcept;

/// The 8x8 block at `offset` set to `block` (modulo 256)
void PutBlock(const Plane& plane, size_t offset, const Coefficients& block) noexcept;
/// `block` added to the 8x8 block at `offset` (modulo 256)
void AddBlock(const Plane& plane, size_t offset, std::span<const int32_t, 64> block) noexcept;
void AddBlock(const Plane& plane, size_t offset, std::span<const int16_t, 64> block) noexcept;
/// The 8x8 block at `from` of `source` copied to `offset`, row by row
void CopyBlock(const Plane& plane, size_t offset, const Plane& source, size_t from) noexcept;
/// A `size` x `size` square at `offset` filled with `value`
void FillBlock(const Plane& plane, size_t offset, uint8_t value, size_t size) noexcept;
/// The 16x16 square at `offset` made from an 8x8 block, each pixel doubled both ways
void ScaleBlock(const Plane& plane, size_t offset, std::span<const uint8_t, 64> block) noexcept;

} // namespace openblack::video::bink
