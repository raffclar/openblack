/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <queue>
#include <type_traits>

#include "Sound/LoadedSound.h"

extern "C" {
#include <AL/al.h>
#include <AL/alc.h>
}

namespace openblack::audio
{
static_assert(std::is_same_v<BufferId, ALuint>, "A sound keeps the buffer number of the audio device");
static_assert(std::is_same_v<SourceId, ALuint>, "An emitter keeps the source number of the audio device");
} // namespace openblack::audio
