/*****************************************************************************
 * Copyright (c) 2018-2020 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "Sound.h"
#include "Sound/SoundPack.h"

namespace openblack::audio
{
class AudioLoader
{
public:
	[[nodiscard]] virtual std::string GetName() const = 0;
	virtual void ToPCM16(Sound& sound) = 0;
};

class MockAudioLoader : public AudioLoader
{
public:
	std::string GetName() const { return "Mock Player"; };
	void ToPCM16(Sound& sound);
};
} // namespace openblack::audio
