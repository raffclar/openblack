/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "Audio/Codec/WaveFile.h"
#include "Audio/Device/AudioDecoderInterface.h"

namespace openblack::audio
{

class MpegAudioDecoder final: public AudioDecoderInterface
{
public:
	MpegAudioDecoder() = default;
	MpegAudioDecoder(const MpegAudioDecoder&) = delete;
	MpegAudioDecoder& operator=(const MpegAudioDecoder&) = delete;
	~MpegAudioDecoder() override = default;
	bool Open(const std::vector<uint8_t>& buffer) override;
	void Read(std::vector<int16_t>& buffer) override;
	[[nodiscard]] ChannelLayout GetChannelLayout() override;
	/// The decoded rate in Hz (0 before a successful Open)
	[[nodiscard]] int GetSampleRate() const;

private:
	std::optional<codec::DecodedAudio> _audio;
};

} // namespace openblack::audio
