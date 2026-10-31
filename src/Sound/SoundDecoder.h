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

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Sound/LoadedSound.h"

namespace openblack::audio
{

/// What a sample's bytes contain. Lionhead banks hold raw MPEG audio and WAV files with PCM, MS-ADPCM or MPEG data.
enum class SoundContainer
{
	Unknown,
	RawMpeg,
	WavPcm,
	WavAdpcm,
	WavMpeg,
	WavOther,
};

struct DecodedSound
{
	std::vector<int16_t> samples;
	ChannelLayout channelLayout;
	int sampleRate;
	uint64_t frames;
	[[nodiscard]] float Duration() const
	{
		return sampleRate != 0 ? static_cast<float>(frames) / static_cast<float>(sampleRate) : 0.0f;
	}
};

struct DecodeResult
{
	SoundContainer container {SoundContainer::Unknown};
	/// Empty when the sample could not be decoded at all, has no frames when the container declares no audio
	std::optional<DecodedSound> sound;
	/// Length the container declares, when it does
	std::optional<float> expectedDuration;
	/// Exact length in frames the container declares, for formats where it is known
	std::optional<uint64_t> expectedFrames;
	/// Padding frames removed from the end of the last compressed block
	uint64_t trimmedFrames {0};
	/// Problems found while decoding, the sound may still be usable
	std::vector<std::string> warnings;
	std::string error;
};

[[nodiscard]] const char* ToString(SoundContainer container);

/// Decodes one sample with the decoder its contents call for and checks the result against what the container
/// declares. expectedSampleRate is the bank header's rate, 0 to skip that check.
[[nodiscard]] DecodeResult DecodeSound(std::span<const uint8_t> data, int expectedSampleRate = 0);

/// The frames of sound in raw MPEG audio, which the decoder counts from the frames' headers without the work to decode
/// them. None when the decoder does not accept the data.
[[nodiscard]] std::optional<uint64_t> CountMpegFrames(std::span<const uint8_t> data);

/// The frames and the rate of one sample
struct SoundLength
{
	uint64_t frames {0};
	int sampleRate {0};
	[[nodiscard]] float Seconds() const
	{
		return sampleRate != 0 ? static_cast<float>(frames) / static_cast<float>(sampleRate) : 0.0f;
	}
};

/// The length of one sample as DecodeSound gives it, from the headers of its container, without the work to decode it.
/// None when DecodeSound cannot decode the data.
[[nodiscard]] std::optional<SoundLength> MeasureSound(std::span<const uint8_t> data);

/// The length in seconds of a sample made of parts, as the parts are put together to play: the frames of each part that
/// can be decoded, at the rate of the last of them. The length is the same before and after the sample is first played.
[[nodiscard]] float MeasureSoundParts(std::span<const std::vector<uint8_t>> parts);

} // namespace openblack::audio
