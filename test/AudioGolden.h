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
#include <string_view>

/// The golden decodes of some of the game's sound bank samples: frames, channels, rate and the CRC-32 (zlib's) of the
/// 16-bit samples as little-endian bytes, as openblack decoded them with dr_wav and dr_mp3 (which our codecs match on
/// every sample of every bank). Numbers only: no sound is in the repo.
namespace openblack::test::audio_golden
{

struct Golden
{
	std::string_view bank; ///< from the game folder
	uint32_t sample;       ///< the sample's index in the bank
	uint32_t frames;
	uint16_t channels;
	uint32_t rate;
	uint32_t crc;
};

/// RIFF waves: PCM 16-bit (11025, 22050 mono and stereo, 44100, 48000 stereo), PCM 8-bit, MS-ADPCM mono and stereo
/// (ending at their "fact" length), MPEG layer II mono and stereo
inline constexpr std::array<Golden, 10> k_Waves = {{
    {"Audio/SFX/Game/InGame.sad", 172, 1328, 1, 11025, 0xBCA6E5D5},
    {"Audio/Dialogue/Guidance.sad", 129, 32089, 1, 22050, 0xFD0F95D0},
    {"Audio/SFX/Atmos/country.sad", 0, 68607, 2, 22050, 0x9A3F0856},
    {"Audio/SFX/Script/Scriptsfx.sad", 123, 19712, 1, 44100, 0xF493DCB1},
    {"CreatureIsle/Audio/Dialogue/CI_SpellDialogue.sad", 14, 50437, 2, 48000, 0x1DC36500},
    {"CreatureIsle/Audio/SFX/Script/Scriptsfx.sad", 158, 453931, 1, 22050, 0x69CE7BA4},
    {"Audio/Dialogue/VillagersBanter.sad", 0, 57855, 1, 22050, 0x7A978B1C},
    {"Audio/SFX/Atmos/high.sad", 0, 117503, 2, 22050, 0x78E3C6FB},
    {"Audio/Dialogue/Guidance.sad", 0, 23040, 1, 22050, 0x18E5E831},
    {"Audio/SFX/Game/InGame.sad", 159, 690048, 2, 22050, 0xEE8F8309},
}};

/// Music segments decoded in order with one decoder from segment 0: MPEG-2 22050 Hz stereo; MPEG-1 48000 Hz and
/// 44100 Hz (CreatureIsle)
inline constexpr std::array<Golden, 5> k_Segments = {{
    {"Audio/Music/align/Aztc_Good.sad", 0, 24192, 2, 22050, 0x927AF9D6},
    {"Audio/Music/align/Aztc_Good.sad", 1, 24192, 2, 22050, 0x5A0312B4},
    {"Audio/Music/align/Aztc_Good.sad", 2, 24192, 2, 22050, 0xB86E9EFF},
    {"CreatureIsle/Audio/Music/epics/EpicBlackhole.sad", 0, 24192, 2, 48000, 0x5F0863A5},
    {"CreatureIsle/Audio/Music/CHANT/marauder_chant.sad", 0, 24192, 2, 44100, 0x21A4F5E2},
}};

/// zlib's CRC-32 of 16-bit samples as little-endian bytes
[[nodiscard]] inline uint32_t Crc(std::span<const int16_t> samples)
{
	uint32_t crc = 0xFFFFFFFFu;
	for (const int16_t s : samples)
	{
		for (const auto byte : {static_cast<uint8_t>(s), static_cast<uint8_t>(static_cast<uint16_t>(s) >> 8)})
		{
			crc ^= byte;
			for (int k = 0; k < 8; ++k)
			{
				crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
			}
		}
	}
	return ~crc;
}

} // namespace openblack::test::audio_golden
