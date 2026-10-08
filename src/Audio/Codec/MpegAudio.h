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

#include "WaveFile.h"

/// MPEG-1, MPEG-2 and MPEG-2.5 audio, layers II and III: layer II holds the voices and the music of the game's sound
/// banks; layer III, the .mp3 format, the game does not use. The sample values are those of the decoder openblack used
/// before (dr_mp3), which decodes in single precision floating point: the decoding below does the same operations in
/// the same order, and rounds to 16 bits the same way.
namespace openblack::audio::codec
{

/// What one call of MpegFrameDecoder::Decode did
struct MpegFrame
{
	/// Frames of samples (1152, or 576 for MPEG-2 and 2.5 layer III), or 0 when nothing was decoded
	uint32_t samples {0};
	/// The bytes used: the frame (and any bytes skipped before it), or the bytes skipped when no frame was found
	size_t bytes {0};
	uint16_t channels {0};
	uint32_t sampleRate {0};
	uint8_t layer {0};
};

/// Decodes one frame at a time from a buffer, keeping what a stream needs from one frame to the next: the last
/// header, to find the next frame quickly, the synthesis filter's history and, for layer III, the bit reservoir and
/// the overlap of the inverse MDCT
class MpegFrameDecoder
{
public:
	/// Up to 1152 frames of 2 channels
	static constexpr size_t k_MaxSamples = 1152 * 2;

	/// Forgets the stream: the next frame is searched for from scratch
	void Reset() noexcept { _header[0] = 0; }

	/// Finds the next frame at the start of `data` and decodes it into `pcm` (interleaved). With an empty `pcm` the
	/// frame is only measured (its samples counted, nothing decoded; a layer III frame still fills the reservoir). A
	/// layer III frame whose main data starts in bytes the reservoir does not have is found but gives no samples
	MpegFrame Decode(std::span<const uint8_t> data, std::span<int16_t> pcm) noexcept;

private:
	static constexpr size_t k_MaxReservoirBytes = 511;
	static constexpr size_t k_MaxPayloadBytes = 2304;

	MpegFrame DecodeLayer3(const uint8_t* header, int frameSize, std::span<int16_t> pcm, MpegFrame result) noexcept;

	std::array<uint8_t, 4> _header {};
	int32_t _freeFormatBytes {0};
	std::array<float, 15 * 64> _filter {};                  ///< the synthesis filter's history
	std::array<float, 2 * 9 * 32> _overlap {};              ///< each channel's second half of the last inverse MDCT
	std::array<uint8_t, k_MaxReservoirBytes> _reservoir {}; ///< the end of the last frame's main data
	int32_t _reservoirBytes {0};
	// Scratch of one frame that the reference decoder does not clear between frames: a damaged frame that reads past
	// what it wrote reads what the previous frame left. The granule's samples, the bands' gains and the synthesis
	// buffer follow each other as there, since short blocks at 8 kHz reorder past the samples' end
	static constexpr size_t k_GranuleFloats = 2 * 576;
	static constexpr size_t k_ScalefactorFloats = 40;
	static constexpr size_t k_SynthesisFloats = (18 + 15) * 64;
	std::array<uint8_t, k_MaxReservoirBytes + k_MaxPayloadBytes> _mainData {};
	std::array<float, k_GranuleFloats + k_ScalefactorFloats + k_SynthesisFloats> _scratch {};
	std::array<std::array<uint8_t, 39>, 2> _intensityPositions {};
};

/// A whole MPEG stream in memory, as the sound banks' MPEG waves are read: tags skipped (ID3v2 at the start, ID3v1 and
/// APE at the end), the frames counted, then decoded from the start into that many frames (those that fail to decode
/// leave 0 at the end). A first frame with a Xing or Info header is not audio: its frame count gives the stream's
/// length, and its LAME tag the encoder's delay and padding, which are cut from the start and the end. Nullopt when
/// it has no frame
[[nodiscard]] std::optional<DecodedAudio> DecodeMpegStream(std::span<const uint8_t> stream);

} // namespace openblack::audio::codec
