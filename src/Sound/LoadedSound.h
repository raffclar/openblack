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
#include <memory>
#include <string>
#include <vector>

#include "Sound/SoundId.h"

namespace openblack::pack
{
enum class AudioBankLoop : uint16_t;
}

namespace openblack::audio
{
struct DecodeResult;

/// The number of the buffer of the audio device that holds the sound. This number names a buffer of this run only.
using BufferId = uint32_t;
/// The number of the source of the audio device that plays a sound. This number names a source of this run only.
using SourceId = uint32_t;

enum class ChannelLayout
{
	Mono,
	Stereo
};

class Sound
{
public:
	std::string name;
	int id;
	int sampleRate;
	int priority;
	int bitRate;
	float volume;
	int pitch;
	int pitchDeviation;
	/// Play parameters of the bank header, used where the game plays the sample (see pack::AudioBankOverride)
	uint32_t overrideFlags;
	uint16_t headerVolume;
	/// The pan of the bank header, from the left (0) to the right (180)
	int32_t headerPan {0};
	/// The world position of the bank header, which gives the value of each axis that the header locks
	std::array<float, 3> headerPosition {};
	/// When the bank lets the sample be heard (see SoundEffectUse)
	uint16_t userParam {0};
	int32_t loop;
	float minDistance;
	float maxDistance;
	float distanceScale;
	ChannelLayout channelLayout;
	/// What playing the sample does about instances of it already playing, applied with AudioBankOverride::LoopType
	pack::AudioBankLoop loopType;
	/// Samples of a bank in the same voice group count as the same sample for loopType, 0 for none
	uint16_t group;
	/// The hashed file name of the bank of the sample, 0 for a sample not from a bank file
	uint32_t bank {0};
	/// The wave format tag of the sample data (0x50 for MPEG)
	uint16_t waveFormat {0};
	/// The first and last sample frames of the part it loops over while it loops, both -1 (or not in order) to loop all
	/// of it; let go of, it plays on from there to its end
	int32_t loopStart {-1};
	int32_t loopEnd {-1};
	BufferId bufferId;
	float duration;
	std::vector<std::vector<uint8_t>> buffer;
	/// The parts of buffer decoded ahead of the sample's first play, on a loading thread; let go once it is played
	std::shared_ptr<std::vector<DecodeResult>> decoded;
	size_t sizeInBytes;
};
} // namespace openblack::audio
