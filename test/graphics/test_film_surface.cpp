/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <array>

#include <gtest/gtest.h>

#include "Graphics/FilmSurface.h"

using namespace openblack::graphics::film_surface;

namespace
{
constexpr OfferedFormat k_X1R5G5B5 {.bits = 16, .rgb = true, .redMask = 0x7C00, .greenMask = 0x03E0, .blueMask = 0x001F};
constexpr OfferedFormat k_A1R5G5B5 {
    .bits = 16, .rgb = true, .alpha = true, .redMask = 0x7C00, .greenMask = 0x03E0, .blueMask = 0x001F};
constexpr OfferedFormat k_A4R4G4B4 {
    .bits = 16, .rgb = true, .alpha = true, .redMask = 0x0F00, .greenMask = 0x00F0, .blueMask = 0x000F};
constexpr OfferedFormat k_R5G6B5 {.bits = 16, .rgb = true, .redMask = 0xF800, .greenMask = 0x07E0, .blueMask = 0x001F};
constexpr OfferedFormat k_X8R8G8B8 {.bits = 32, .rgb = true, .redMask = 0xFF0000, .greenMask = 0xFF00, .blueMask = 0xFF};
constexpr OfferedFormat k_A8R8G8B8 {
    .bits = 32, .rgb = true, .alpha = true, .redMask = 0xFF0000, .greenMask = 0xFF00, .blueMask = 0xFF};
constexpr OfferedFormat k_Compressed {};

/// The nearest byte to n / (2^bits - 1), from floating-point maths
uint8_t NearestByte(uint32_t level, uint32_t bits)
{
	const double top = static_cast<double>((1u << bits) - 1u);
	return static_cast<uint8_t>(std::floor((static_cast<double>(level) * 255.0 / top) + 0.5));
}
} // namespace

TEST(FilmSurface, ACardOfTodayGivesFiveBitsAChannel)
{
	// The texture formats in the order that a GeForce RTX 4070 Ti offers them to the game, on Windows 10
	constexpr std::array k_Offered = {k_X1R5G5B5, k_A1R5G5B5, k_A4R4G4B4, k_R5G6B5, k_X8R8G8B8, k_A8R8G8B8, k_Compressed};
	EXPECT_EQ(Choose(k_Offered), Format::Rgb555);
	EXPECT_EQ(Choose(k_Offered), k_TodaysCards);
}

TEST(FilmSurface, TheFiveBitFormatStopsTheSearch)
{
	constexpr std::array k_SixFirst = {k_R5G6B5, k_X1R5G5B5};
	EXPECT_EQ(Choose(k_SixFirst), Format::Rgb555);
	constexpr std::array k_FiveFirst = {k_X1R5G5B5, k_R5G6B5};
	EXPECT_EQ(Choose(k_FiveFirst), Format::Rgb555);
}

TEST(FilmSurface, ACardWithNoFiveBitFormatGivesSixBitsOfGreen)
{
	constexpr std::array k_Offered = {k_A1R5G5B5, k_A4R4G4B4, k_R5G6B5, k_X8R8G8B8};
	EXPECT_EQ(Choose(k_Offered), Format::Rgb565);
}

TEST(FilmSurface, FormatsWithAlphaOrOtherSizesDoNotCount)
{
	constexpr std::array k_Offered = {k_A1R5G5B5, k_A4R4G4B4, k_X8R8G8B8, k_A8R8G8B8, k_Compressed};
	EXPECT_EQ(Choose(k_Offered), Format::Rgb555);
	EXPECT_EQ(Choose({}), Format::Rgb555);
}

TEST(FilmSurface, TheLastOpaqueFormatIsKept)
{
	// An opaque 16-bit format that is neither 555 nor 565 comes after the 565 format: the game keeps it, so 555
	constexpr OfferedFormat k_Other {.bits = 16, .rgb = true, .redMask = 0xFC00, .greenMask = 0x03E0, .blueMask = 0x001F};
	constexpr std::array k_Offered = {k_R5G6B5, k_Other};
	EXPECT_EQ(Choose(k_Offered), Format::Rgb555);
	constexpr std::array k_Reversed = {k_Other, k_R5G6B5};
	EXPECT_EQ(Choose(k_Reversed), Format::Rgb565);
}

TEST(FilmSurface, EachChannelKeepsItsTopBits)
{
	EXPECT_EQ(Pack({.r = 0xFF, .g = 0xFF, .b = 0xFF}, Format::Rgb555), 0x7FFF);
	EXPECT_EQ(Pack({.r = 0xFF, .g = 0xFF, .b = 0xFF}, Format::Rgb565), 0xFFFF);
	EXPECT_EQ(Pack({.r = 0x07, .g = 0x80, .b = 0xFF}, Format::Rgb555), (0u << 10) | (16u << 5) | 31u);
	EXPECT_EQ(Pack({.r = 0x07, .g = 0x80, .b = 0xFF}, Format::Rgb565), (0u << 11) | (32u << 5) | 31u);
	// Truncation, not rounding: 7 is just below the next level of 8
	EXPECT_EQ(Pack({.r = 0x0F, .g = 0x07, .b = 0x08}, Format::Rgb555), (1u << 10) | (0u << 5) | 1u);
	EXPECT_EQ(Pack({.r = 0x0F, .g = 0x07, .b = 0x08}, Format::Rgb565), (1u << 11) | (1u << 5) | 1u);
}

TEST(FilmSurface, TheCardReadsTheNearestByte)
{
	for (uint32_t level = 0; level < 32; ++level)
	{
		EXPECT_EQ(Widen(level, 5), NearestByte(level, 5)) << level;
	}
	for (uint32_t level = 0; level < 64; ++level)
	{
		EXPECT_EQ(Widen(level, 6), NearestByte(level, 6)) << level;
	}
	// The levels where the nearest byte is not the level with its top bits repeated
	EXPECT_EQ(Widen(3, 5), 25);
	EXPECT_EQ(Widen(7, 5), 58);
	EXPECT_EQ(Widen(24, 5), 197);
	EXPECT_EQ(Widen(28, 5), 230);
	EXPECT_EQ(Widen(11, 6), 45);
	EXPECT_EQ(Widen(52, 6), 210);
}

TEST(FilmSurface, KnownPixels)
{
	EXPECT_EQ(Quantise({.r = 0, .g = 0, .b = 0}, Format::Rgb555), (Rgb8 {.r = 0, .g = 0, .b = 0}));
	EXPECT_EQ(Quantise({.r = 255, .g = 255, .b = 255}, Format::Rgb555), (Rgb8 {.r = 255, .g = 255, .b = 255}));
	EXPECT_EQ(Quantise({.r = 255, .g = 255, .b = 255}, Format::Rgb565), (Rgb8 {.r = 255, .g = 255, .b = 255}));
	EXPECT_EQ(Quantise({.r = 31, .g = 128, .b = 200}, Format::Rgb555), (Rgb8 {.r = 25, .g = 132, .b = 206}));
	EXPECT_EQ(Quantise({.r = 31, .g = 128, .b = 200}, Format::Rgb565), (Rgb8 {.r = 25, .g = 130, .b = 206}));
	EXPECT_EQ(Quantise({.r = 7, .g = 6, .b = 8}, Format::Rgb555), (Rgb8 {.r = 0, .g = 0, .b = 8}));
	EXPECT_EQ(Quantise({.r = 7, .g = 6, .b = 8}, Format::Rgb565), (Rgb8 {.r = 0, .g = 4, .b = 8}));
}

TEST(FilmSurface, EveryChannelValue)
{
	for (uint32_t value = 0; value < 256; ++value)
	{
		const auto byte = static_cast<uint8_t>(value);
		const auto five = Quantise({.r = byte, .g = byte, .b = byte}, Format::Rgb555);
		EXPECT_EQ(five,
		          (Rgb8 {.r = NearestByte(value >> 3u, 5), .g = NearestByte(value >> 3u, 5), .b = NearestByte(value >> 3u, 5)}))
		    << value;
		const auto six = Quantise({.r = byte, .g = byte, .b = byte}, Format::Rgb565);
		EXPECT_EQ(six,
		          (Rgb8 {.r = NearestByte(value >> 3u, 5), .g = NearestByte(value >> 2u, 6), .b = NearestByte(value >> 3u, 5)}))
		    << value;
	}
}

TEST(FilmSurface, TheShaderMode)
{
	EXPECT_EQ(ShaderMode(std::nullopt), 0.0f);
	EXPECT_EQ(ShaderMode(Format::Rgb555), 1.0f);
	EXPECT_EQ(ShaderMode(Format::Rgb565), 2.0f);
}
