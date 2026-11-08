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

#include <algorithm>

#include "Common/X87.h"

/// How the sound library mixes a volume from 0 to 127 with the master volume of the effects or the music. All the steps
/// are whole numbers, so a master volume changes the sound in steps of 1/127.
namespace openblack::audio::mix_volume
{

/// The most volume of a voice, a bank and a master volume
inline constexpr uint32_t k_MaxVolume = 127;
/// The mixer's volume for each step of a volume from 0 to 127
inline constexpr uint32_t k_MixerVolumePerStep = 258;
/// The mixer's volume is linear, and this volume is full volume
inline constexpr float k_MixerFullVolume = 32767.0f;
/// The options slider value that gives the most master volume
inline constexpr float k_SliderSteps = 127.0f;

/// The master volume that an options slider gives: the slider value from 0 to 1 times 127, cut to a whole number
[[nodiscard]] inline uint32_t MasterVolumeOfSlider(float slider) noexcept
{
	// The slider gives only values from 0 to 1
	const auto steps = x87::Mul(std::clamp(slider, 0.0f, 1.0f), k_SliderSteps);
	return static_cast<uint32_t>(steps);
}

/// The mixer's volume of a sample voice: the voice's volume times the master volume, divided by 127, then in mixer
/// steps
[[nodiscard]] constexpr uint32_t SampleMixerVolume(uint32_t volume, uint32_t master) noexcept
{
	return (std::min(volume, k_MaxVolume) * master / k_MaxVolume) * k_MixerVolumePerStep;
}

/// The mixer's volume of a music channel: the channel's volume times the bank's volume in mixer steps, divided by 127,
/// then times the master volume, divided by 127
[[nodiscard]] constexpr uint32_t MusicMixerVolume(uint32_t volume, uint32_t bankVolume, uint32_t master) noexcept
{
	return (volume * bankVolume * k_MixerVolumePerStep / k_MaxVolume) * std::min(master, k_MaxVolume) / k_MaxVolume;
}

/// The linear gain that the mixer plays a mixer volume at
[[nodiscard]] constexpr float MixerGain(uint32_t mixerVolume) noexcept
{
	return static_cast<float>(mixerVolume) / k_MixerFullVolume;
}

} // namespace openblack::audio::mix_volume
