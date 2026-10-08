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

#include <glm/vec3.hpp>

// The arithmetic of the audio library and QMixer that turns a channel's values into what is heard. Pure functions:
// test_audio_laws checks them against an emulation of the original libraries.

namespace openblack::audio::qmixer
{

/// The sample main volume's range and its default (AudioSampleMasterVolume of the configuration when it has one)
inline constexpr int k_MaxVolume = 127;

/// The gain QMixer applies for a channel volume v (0..127) under the sample main volume m (0..127): the library sends
/// floor(m * v / 127) * 258 (0..32766) to QMixer, which keeps it as vol / 32767. A main volume change re-sends the
/// same product for every channel in use.
[[nodiscard]] float Gain(int volume, int mainVolume);

/// QMixer's distance gain with the channel flags the library uses (neither "clamp at max" nor "linear"): 1 up to min or
/// with scale 0, min / (min + scale (d - min)) up to max, 0 beyond max
[[nodiscard]] float DistanceGain(float minDistance, float maxDistance, float scale, float distance);

/// The listener-space point QMixer hears a relative position at (the library's (x, y, z) -> azimuth atan2(x, y) and
/// elevation atan(z / |(x, y)|) in degrees, atan * 180 * the double 0.31847133757961782 (1 / 3.14), range |(x, y, z)|,
/// all three sent as floats; QMixer then, with pi * 0.0055555557f: right = r cos(el) sin(az), up = r sin(el),
/// ahead = r cos(el) cos(az), stored as floats).
/// The library's half runs on the game's thread with the FPU at 24 bits: every step rounds to a float.
/// QMixer's half is not run when the polar point is set, which only stores it: QMixer's pump reaches it, driven every
/// 20 ms by the library's multimedia timer on its own thread, at that thread's 53 bits (the default control word;
/// (inferred) nothing in that thread changes it; the one pass at play time on the game thread is at 24 bits and is
/// redone by the next pump). So the relative x is right, y ahead and z up. Returns (right, up, ahead).
[[nodiscard]] glm::vec3 PolarRelative(glm::vec3 position);

/// The frequency rate * percent / 100 as a ratio of the wave's rate: the unsigned integer division of a start and of
/// a pitch change
[[nodiscard]] float FrequencyRatio(int sampleRate, int percent);

/// The pitch of a starting sample (unsigned integers): p = pitch (0 -> 100),
/// d = deviation * p / 100, p = p - d + rand * 2d / 32767 (rand = MSVC rand(), 0..32767), and 0 -> 100 again
[[nodiscard]] int StartPitch(int pitch, int deviation, int rand15);

} // namespace openblack::audio::qmixer
