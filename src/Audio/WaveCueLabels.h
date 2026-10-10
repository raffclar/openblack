/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <span>
#include <string>
#include <vector>

namespace openblack::audio
{

/// A labelled cue point of a WAV recording
struct WaveCueLabel
{
	std::string text;
	/// Seconds from the start: the cue's sample position over the recording's sample rate
	float time;
};

/// The labels of a RIFF WAVE recording's cue points, as the game reads them: the format chunk's sample rate, then the
/// "cue " chunk after it, then the "adtl" list after that, whose "labl" chunks are read in order, one for each cue
/// point. A label is matched to its cue point by id; a label without one is left out. Anything missing or out of
/// order gives no labels.
[[nodiscard]] std::vector<WaveCueLabel> ReadWaveCueLabels(std::span<const uint8_t> riff);

} // namespace openblack::audio
