/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TextureSize.h"

#include <algorithm>
#include <array>

namespace openblack::graphics
{
namespace
{
/// How a format keeps its texels: in blocks of texels with a size in bytes, and with a smallest number of blocks on
/// each side
struct BlockInfo
{
	uint8_t bitsPerPixel;
	uint8_t blockWidth;
	uint8_t blockHeight;
	uint8_t blockSize;
	uint8_t minBlocksX;
	uint8_t minBlocksY;
};

constexpr BlockInfo k_Bc1 {
    .bitsPerPixel = 4, .blockWidth = 4, .blockHeight = 4, .blockSize = 8, .minBlocksX = 1, .minBlocksY = 1};
constexpr BlockInfo k_Bc2 {
    .bitsPerPixel = 8, .blockWidth = 4, .blockHeight = 4, .blockSize = 16, .minBlocksX = 1, .minBlocksY = 1};

/// A format whose blocks are one texel of this many bytes
constexpr BlockInfo Texel(uint8_t bytes)
{
	return {.bitsPerPixel = static_cast<uint8_t>(bytes * 8),
	        .blockWidth = 1,
	        .blockHeight = 1,
	        .blockSize = bytes,
	        .minBlocksX = 1,
	        .minBlocksY = 1};
}

/// The block of each format, in the order of TextureFormat
constexpr std::array<BlockInfo, static_cast<size_t>(TextureFormat::_COUNT)> k_Blocks {{
    k_Bc1,     // BlockCompression1
    k_Bc2,     // BlockCompression2
    k_Bc2,     // BlockCompression3
    Texel(4),  // Depth24Stencil8
    Texel(2),  // DepthComponent16
    Texel(3),  // DepthComponent24
    Texel(2),  // R16F
    Texel(2),  // R16I
    Texel(2),  // R16SNorm
    Texel(2),  // R16UI
    Texel(4),  // R32F
    Texel(4),  // R32I
    Texel(4),  // R32UI
    Texel(1),  // A8
    Texel(1),  // R8
    Texel(1),  // R8I
    Texel(1),  // R8SNorm
    Texel(1),  // R8UI
    Texel(4),  // RG16
    Texel(4),  // RG16F
    Texel(4),  // RG16SNorm
    Texel(8),  // RG32F
    Texel(8),  // RG32I
    Texel(8),  // RG32UI
    Texel(2),  // RG8
    Texel(2),  // RG8I
    Texel(2),  // RG8SNorm
    Texel(2),  // RG8UI
    Texel(4),  // RGB10A2
    Texel(2),  // B5G6R5
    Texel(2),  // R5G6B5
    Texel(2),  // BGR5A1
    Texel(2),  // RGB5A1
    Texel(3),  // RGB8
    Texel(3),  // RGB8I
    Texel(3),  // RGB8UI
    Texel(4),  // RGB9E5
    Texel(4),  // RGBA8
    Texel(4),  // RGBA8I
    Texel(4),  // RGBA8UI
    Texel(4),  // RGBA8SNorm
    Texel(4),  // BGRA8
    Texel(8),  // RGBA16
    Texel(8),  // RGBA16F
    Texel(8),  // RGBA16I
    Texel(8),  // RGBA16UI
    Texel(8),  // RGBA16SNorm
    Texel(16), // RGBA32F
    Texel(16), // RGBA32I
    Texel(16), // RGBA32UI
    Texel(2),  // BGRA4
    Texel(2),  // RGBA4
}};
} // namespace

TextureSize CalculateTextureSize(uint16_t width, uint16_t height, uint16_t layers, TextureFormat format) noexcept
{
	const auto& block = k_Blocks.at(static_cast<size_t>(format));
	const uint32_t texelsX = std::max<uint32_t>(1, width);
	const uint32_t texelsY = std::max<uint32_t>(1, height);
	// A side keeps full blocks, and at least the smallest number of blocks
	const uint32_t sideX = std::max<uint32_t>(block.blockWidth * block.minBlocksX,
	                                          ((texelsX + block.blockWidth - 1) / block.blockWidth) * block.blockWidth);
	const uint32_t sideY = std::max<uint32_t>(block.blockHeight * block.minBlocksY,
	                                          ((texelsY + block.blockHeight - 1) / block.blockHeight) * block.blockHeight);
	const uint64_t layerBytes = uint64_t(sideX / block.blockWidth) * (sideY / block.blockHeight) * block.blockSize;
	return {.width = static_cast<uint16_t>(texelsX),
	        .height = static_cast<uint16_t>(texelsY),
	        .layers = layers,
	        .bitsPerPixel = block.bitsPerPixel,
	        .storageSize = static_cast<uint32_t>(layerBytes * layers)};
}

} // namespace openblack::graphics
