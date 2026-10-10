/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <span>
#include <string_view>

/// Checksums of the game's videos as the game's own Bink library decodes them, taken with a separate harness that runs
/// the library outside this repository. No picture from the game is kept here, only zlib CRC-32s:
///
/// - per frame: the 32-bit picture the library copies out (B, G, R, 0 per pixel, rows without padding) and its Y, U and
///   V planes (visible pixels only, row by row);
/// - per video, a digest of every frame: the CRC-32 of each frame's four CRCs (picture, Y, U, V) as little-endian
///   words, in frame order;
/// - the library's colour for every (Y, U, V): the CRC-32 of R, G, B for V, then U, then Y from 0 to 255 (V outermost).
namespace openblack::bink::test::golden
{

struct Frame
{
	uint32_t frame;
	uint32_t picture;
	uint32_t y;
	uint32_t u;
	uint32_t v;
};

struct Video
{
	std::string_view path; ///< under the game's folder
	uint32_t frameCount;
	uint32_t digest;
	std::span<const Frame> frames;
};

inline constexpr std::array<Frame, 2> k_LogoFrames = {{
    {0, 0x42D7F4B8, 0x0DA77D70, 0xA78D31D4, 0xA78D31D4},
    {1, 0x81E604A6, 0x64CF226E, 0xA78D31D4, 0xA78D31D4},
}};
inline constexpr std::array<Frame, 3> k_TipsFrames = {{
    {0, 0x09E1F0DC, 0x248BC97F, 0x68D48364, 0x1EE51051},
    {20, 0x26B2AB71, 0x6435F8EF, 0xD19906D3, 0xDDA9C2FB},
    {34, 0x3BE0C83D, 0x70C7801E, 0x8BDED9DD, 0xBE86145A},
}};
inline constexpr std::array<Frame, 6> k_IntroFrames = {{
    {0, 0xFD15E9AA, 0xC3102810, 0x77609D18, 0x77609D18},
    {18, 0x7C98544B, 0xD43FEAEC, 0x77609D18, 0x77609D18},
    {100, 0xAF2EE3F0, 0x3D1F3FAD, 0x77609D18, 0x77609D18},
    {800, 0xCCE161B8, 0x1D7416C3, 0x1E57B04C, 0x1CBF2E1D},
    {1392, 0x7D58D760, 0x22B5FEA6, 0xF73E721E, 0xCFF36886},
    {1600, 0x9FA77679, 0x40322B55, 0x0EEBAA2E, 0x59D4CC18},
}};
inline constexpr std::array<Frame, 4> k_FallFrames = {{
    {0, 0x9296F542, 0x9E6DB4E5, 0x89DF4AB2, 0xB3EADC63},
    {1, 0x6C190163, 0x1F720CEA, 0xEFFD139D, 0x4BAD2DEF},
    {100, 0xE99B7EF7, 0x50321823, 0x6FA0DAA1, 0x050A80D8},
    {1199, 0x9D251296, 0x8808F8BF, 0x2E93CB77, 0xE2C757E6},
}};
inline constexpr std::array<Frame, 4> k_PreIntroFrames = {{
    {0, 0xE11A68FC, 0xAAB18F0E, 0xE6A8D5F2, 0xEDD7F76A},
    {1, 0x897A7121, 0x215D24C6, 0xE6A8D5F2, 0xF8C1FC08},
    {100, 0x1CD324E0, 0x38E36D56, 0x21D6D4BB, 0x7DB46990},
    {2514, 0xCDE1CEA7, 0x0A7B4C0A, 0xCE8DC7BC, 0xA1F22544},
}};

inline constexpr std::array<Video, 5> k_Videos = {{
    {"Data/logo.bik", 2, 0x54181162, k_LogoFrames},
    {"Data/tips.bik", 35, 0xB896E7EA, k_TipsFrames},
    {"Data/INTRO.bik", 1601, 0xCB8AEB25, k_IntroFrames},
    {"Data/Spells/fall/fall.bik", 1200, 0xFC5327C5, k_FallFrames},
    {"Data/pre_intro.bik", 2515, 0x86F8BF29, k_PreIntroFrames},
}};

inline constexpr uint32_t k_EveryColour = 0x247978CA;

} // namespace openblack::bink::test::golden
