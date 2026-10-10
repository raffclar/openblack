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

#include <array>
#include <span>

/// Lip sync from a voice recording: the spectrum of a short window of the sound around the time being shown, split
/// into three frequency bands, gives three mouth shapes' weights.
///
/// The game runs this every frame on the line an advisor is saying. All maths is in single precision, as the game does.
namespace openblack::audio::lip_sync
{

/// The window of sound looked at each time, in samples
inline constexpr int k_Window = 512;

/// One frequency band and the mouth shape it drives
struct Band
{
	float low;  ///< Hz
	float high; ///< Hz
	float gain;
	/// Which of the key's weights the band drives
	int weight;
};

/// How the sound is turned into the mouth's weights; the defaults are the advisors'
struct Settings
{
	/// Added to the time looked at, in ms
	float offsetMs {0.0f};
	/// Below this total level the sound counts as silence
	float threshold {0.05f};
	/// The total level's gain, the result kept at most 1
	float level {2.5f};
	/// How fast a weight may move: falling at most dt x rate x 0.18 a call, rising at most twice that
	float rate {40.0f};
	std::array<Band, 3> bands {{
	    {.low = 200.0f, .high = 400.0f, .gain = 0.85f, .weight = 0},
	    {.low = 400.0f, .high = 700.0f, .gain = 1.1f, .weight = 1},
	    {.low = 700.0f, .high = 10000.0f, .gain = 10.0f, .weight = 2},
	}};
};

/// The mouth's three weights, which carry over from one call to the next
struct Key
{
	std::array<float, 3> weights {};
};

/// Numerical Recipes' complex FFT in place: `data` holds n complex numbers as (real, imaginary) pairs, n a power of
/// two; `forward` uses e^(+2 pi i / n), as the game's call does
void Fft(std::span<float> data, int n, bool forward);

/// The magnitude spectrum of n samples under a triangle window (rising to the middle and back down, scaled so that a
/// full-scale sample reads at most 1 x the window): `out` holds 2n floats, the n magnitudes sqrt((re^2 + im^2) / n)
/// are left at its front
void MagnitudeSpectrum(std::span<const int16_t> samples, std::span<float> out, int n);

/// The mean of the spectrum's bins from low / rate x n up to (not including) high / rate x n, both truncated and kept
/// at most n; 0 when that leaves no bins
[[nodiscard]] float BandLevel(std::span<const float> spectrum, int n, float rate, float low, float high);

/// The band levels of the last call, normalised by the total, and the total; they are only kept for looking at
struct Levels
{
	std::array<float, 3> bands {};
	float total {0.0f};
};

/// Moves the key's weights towards the shape of the sound at `time` (seconds) plus `dt`: the window of `window`
/// samples centred there (kept inside the recording), its three bands' levels, each weight the square of its band
/// over the loudest one times the total level, the change limited by `rate`, and the weights scaled down when they
/// add up past 1.
///
/// `samples` is the recording at `sampleRate`, `frames` its length in samples. Like the game, a silent window whose
/// bands are all exactly 0 gives weights that are not numbers for that call, so no mouth shape is shown; the next call
/// with sound recovers them. A recording shorter than the window leaves the key as it was.
Levels UpdateKey(const Settings& settings, Key& key, float dt, float time, std::span<const int16_t> samples, int frames,
                 int sampleRate, std::span<float> spectrum, int window = k_Window);

} // namespace openblack::audio::lip_sync
