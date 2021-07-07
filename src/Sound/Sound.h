/*****************************************************************************
 * Copyright (c) 2018-2020 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace openblack::audio
{

using SoundId = std::string;

enum class LoopType
{
	None
};

enum class SoundFormat
{
	Unknown,
	MonoS16,
	StereoS16
};

struct Sound
{
	SoundId id;
	int priority;
	int sampleRate;
	int pitch;
	int pitchDeviation;
	std::vector<uint8_t> bytes;
	SoundFormat format;
	LoopType loopType;
	int channels;
	bool loaded;
	bool sectorMerged;
	int sectorCount;
	float lengthInSeconds;
};

using SoundMap = std::map<SoundId, std::unique_ptr<Sound>>;
} // namespace openblack::audio
