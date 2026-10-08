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

#include <optional>
#include <span>
#include <vector>

/// Uncompressed and ADPCM wave files, decoded to 16-bit samples exactly as the decoder openblack used before (dr_wav)
/// did, so that whatever played then plays the same:
///
/// - containers: RIFF, RIFX (big-endian RIFF), RF64, Sony Wave64, AIFF and AIFC;
/// - formats: integer PCM of 1 to 8 bytes a sample (8-bit unsigned), IEEE float of 4 and 8 bytes, A-law, mu-law,
///   MS-ADPCM, IMA ADPCM, and WAVE_FORMAT_EXTENSIBLE carrying any of them.
///
/// Its length rules are kept too, with one fix: an MS-ADPCM wave ends at its "fact" length (the old library played the
/// padding of its last block). A wave of any other format opens as silence of the length its data would have.
namespace openblack::audio::codec
{

inline constexpr uint16_t k_FormatPcm = 0x0001;
inline constexpr uint16_t k_FormatMsAdpcm = 0x0002;
inline constexpr uint16_t k_FormatIeeeFloat = 0x0003;
inline constexpr uint16_t k_FormatALaw = 0x0006;
inline constexpr uint16_t k_FormatMuLaw = 0x0007;
inline constexpr uint16_t k_FormatImaAdpcm = 0x0011;
inline constexpr uint16_t k_FormatExtensible = 0xFFFE;

enum class WaveContainer : uint8_t
{
	Riff,
	Rifx,
	Wave64,
	Rf64,
	Aiff,
};

/// A wave file's layout, as read from its header and chunks
struct WaveFile
{
	WaveContainer container {WaveContainer::Riff};
	/// The format of the samples: an extensible format's sub-format; an AIFF's compression as the matching tag
	uint16_t format {0};
	uint16_t channels {0};
	uint32_t sampleRate {0};
	uint16_t blockAlign {0};
	uint16_t bitsPerSample {0};
	size_t dataOffset {0};
	/// The data's size, cut to the end of the file and, for uncompressed formats, to whole frames
	uint64_t dataSize {0};
	/// The frames the wave holds
	uint64_t frames {0};
	bool aiffLittleEndian {false}; ///< AIFC "sowt"
	bool aiffUnsigned {false};     ///< AIFC "raw " 8-bit
};

/// Decoded audio: interleaved 16-bit samples
struct DecodedAudio
{
	std::vector<int16_t> samples;
	uint16_t channels {0};
	uint32_t sampleRate {0};
};

/// The wave's layout, or nullopt when it is not a wave file the decoder opens (no container, no format or data chunk,
/// a rate, channel count, sample size or block size out of range)
[[nodiscard]] std::optional<WaveFile> ParseWaveFile(std::span<const uint8_t> file) noexcept;

/// The whole wave as 16-bit samples: always WaveFile::frames frames, those the data does not reach left at 0. Nullopt
/// when ParseWaveFile refuses it, or when its length would be unreasonable (more than 2^28 samples)
[[nodiscard]] std::optional<DecodedAudio> DecodeWaveFile(std::span<const uint8_t> file);

} // namespace openblack::audio::codec
