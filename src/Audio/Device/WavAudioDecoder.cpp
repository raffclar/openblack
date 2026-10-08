/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WavAudioDecoder.h"

#include <stdexcept>
#include <utility>

#include "Audio/Codec/WaveFile.h"

using namespace openblack::audio;

int WavAudioDecoder::GetSampleRate() const
{
	return _audio ? static_cast<int>(_audio->sampleRate) : 0;
}

bool WavAudioDecoder::Open(const std::vector<uint8_t>& buffer)
{
	_audio = codec::DecodeWaveFile(buffer);
	return _audio.has_value();
}

void WavAudioDecoder::Read(std::vector<int16_t>& buffer)
{
	buffer = _audio ? std::move(_audio->samples) : std::vector<int16_t> {};
}

ChannelLayout WavAudioDecoder::GetChannelLayout()
{
	switch (_audio ? _audio->channels : 0)
	{
	case 1:
		return ChannelLayout::Mono;
	case 2:
		return ChannelLayout::Stereo;
	default:
		throw std::runtime_error("Unsupported channel layout");
	}
}
