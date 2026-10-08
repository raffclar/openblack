/*******************************************************************************
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
#include <vector>

#include "Graphics/Rgb16.h"

/// The golden frames of the game's videos: the CRC-32 (zlib's) of each frame as the original's Bink library copies it
/// to a 555 surface, the texels as little-endian bytes. Shared by the decoder tests; no picture is in the repo.
namespace openblack::test::bink_golden
{

struct Golden
{
	uint32_t frame;
	uint32_t crc; ///< zlib.crc32 of golden/<video>/frame_NNNN.bin
};

/// logo.bik: both frames
inline constexpr std::array<Golden, 2> k_Logo = {{{0, 0xC6AE5801}, {1, 0xF94865E9}}};
/// tips.bik as the tip video asks for them: straight to a frame, backwards and forwards
inline constexpr std::array<Golden, 3> k_Tips = {{{34, 0x5CBF3B81}, {20, 0x3BAF3B3A}, {34, 0x5CBF3B81}}};
/// INTRO.bik frame 10, the 10th delta after the only key frame
inline constexpr Golden k_Intro10 = {10, 0xB4672079};

/// In order from the start, each film's only key frame. INTRO.bik: black to frame 17, then the picture; the last frame
inline constexpr std::array<Golden, 5> k_Intro = {
    {{0, 0xB4672079}, {18, 0x4A34A19E}, {100, 0xD114FE0E}, {800, 0x0AA4D08B}, {1600, 0x8BDD2F50}}};
/// Spells\fall\fall.bik, up to its last frame
inline constexpr std::array<Golden, 4> k_Fall = {{{0, 0xA7751E9A}, {1, 0xE1B7C654}, {100, 0x06323F3B}, {1199, 0x4F1F04BE}}};
/// pre_intro.bik, its start
inline constexpr std::array<Golden, 3> k_PreIntro = {{{0, 0xB62084A9}, {1, 0x399A6577}, {100, 0xB3F35C2F}}};
/// tips.bik frame 0
inline constexpr Golden k_Tips0 = {0, 0xFF7EB330};

/// The golden CRC of a picture: what the video player makes of it for the original's 555 surface, then zlib's CRC-32
/// (reflected 0xEDB88320, init and final xor 0xFFFFFFFF) of the texels as little-endian bytes
[[nodiscard]] inline uint32_t Crc555(std::span<const uint8_t> rgba)
{
	std::vector<uint16_t> texels(rgba.size() / 4);
	graphics::rgb16::Quantize(graphics::rgb16::Format::Rgb555, rgba, texels);
	uint32_t crc = 0xFFFFFFFFu;
	const auto byte = [&crc](uint8_t b) {
		crc ^= b;
		for (int k = 0; k < 8; ++k)
		{
			crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
		}
	};
	for (const uint16_t t : texels)
	{
		byte(static_cast<uint8_t>(t));
		byte(static_cast<uint8_t>(t >> 8));
	}
	return ~crc;
}

} // namespace openblack::test::bink_golden
