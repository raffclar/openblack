/*****************************************************************************
 * Copyright (c) 2018-2020 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "AudioLoader.h"

using namespace openblack::audio;

void MockAudioLoader::ToPCM16(Sound& sound) {
	sound.loaded = true;
}
