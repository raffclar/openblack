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

#include <optional>

#include <PackFile.h>

namespace openblack::audio
{

/// The play settings that a bank header gives a sample, and the override flags that tell which of them apply
struct HeaderPlaySettings
{
	uint32_t overrideFlags {0};
	uint16_t volume {0};
	uint32_t pitchPercent {0};
	/// The largest random change of the playback rate, in percent of the rate, up or down
	uint32_t pitchDeviation {0};
	float minDistance {0.0f};
	float maxDistance {0.0f};
	float distanceScale {0.0f};
};

/// The play settings that a caller gives. An empty value keeps the default of the sound library.
struct CallerPlaySettings
{
	std::optional<uint32_t> volume;
	std::optional<uint32_t> pitchPercent;
	std::optional<float> minDistance;
	std::optional<float> maxDistance;
	/// The settings (bits of pack::AudioBankOverride) for which the caller's value wins over the bank header
	uint32_t callerWins {0};
};

/// The settings that a sound starts with
struct StartPlaySettings
{
	uint32_t volume;
	uint32_t pitchPercent;
	float minDistance;
	float maxDistance;
	float distanceScale;
	/// The settings (bits of pack::AudioBankOverride) that the bank header set. Later changes of these do nothing.
	uint32_t headerSet;

	bool operator==(const StartPlaySettings&) const = default;
};

inline constexpr uint32_t k_PlayMaxVolume = 127;
inline constexpr uint32_t k_PlayDefaultPitchPercent = 100;
inline constexpr float k_PlayDefaultMinDistance = 1.0f;
inline constexpr float k_PlayDefaultMaxDistance = 9999.0f;
inline constexpr float k_PlayDefaultDistanceScale = 0.3f;
/// The largest value of the random number that gives the change of the playback rate
inline constexpr uint32_t k_PlayRandomMax = 0x7FFF;

/// The caller's minimum and maximum distances win over the bank header's, as for a placed guidance voice
inline constexpr uint32_t k_CallerDistancesWin =
    static_cast<uint32_t>(pack::AudioBankOverride::MinDist) | static_cast<uint32_t>(pack::AudioBankOverride::MaxDist);

/// The settings that the bank header sets: each setting that the header has and for which the caller's value does not
/// win
[[nodiscard]] constexpr uint32_t HeaderSetSettings(uint32_t overrideFlags, uint32_t callerWins)
{
	return overrideFlags & ~callerWins;
}

/// Whether a setting is one of a mask of pack::AudioBankOverride bits
[[nodiscard]] constexpr bool HasSetting(uint32_t settings, pack::AudioBankOverride setting)
{
	return (settings & static_cast<uint32_t>(setting)) != 0;
}

/// The settings that a sample of a bank starts with. The bank header's value replaces the caller's value of each
/// setting that the header has, unless the caller's value wins. Then the playback rate changes by a random amount, up
/// to the header's deviation up or down, whether the rate came from the header or from the caller. random is a number
/// from 0 to k_PlayRandomMax. All the arithmetic is unsigned integer arithmetic.
[[nodiscard]] constexpr StartPlaySettings ResolveStartSettings(const HeaderPlaySettings& header,
                                                               const CallerPlaySettings& caller, uint32_t random)
{
	const auto set = HeaderSetSettings(header.overrideFlags, caller.callerWins);
	const auto fromHeader = [set](pack::AudioBankOverride setting) { return HasSetting(set, setting); };

	auto volume = fromHeader(pack::AudioBankOverride::Volume) ? header.volume : caller.volume.value_or(k_PlayMaxVolume);
	if (volume > k_PlayMaxVolume)
	{
		volume = k_PlayMaxVolume;
	}

	auto pitch = fromHeader(pack::AudioBankOverride::Pitch) ? header.pitchPercent
	                                                        : caller.pitchPercent.value_or(k_PlayDefaultPitchPercent);
	if (pitch == 0)
	{
		pitch = k_PlayDefaultPitchPercent;
	}
	const auto deviation = (header.pitchDeviation * pitch) / 100;
	pitch -= deviation;
	pitch += (random * deviation * 2) / k_PlayRandomMax;
	if (pitch == 0)
	{
		pitch = k_PlayDefaultPitchPercent;
	}

	return {
	    .volume = volume,
	    .pitchPercent = pitch,
	    .minDistance = fromHeader(pack::AudioBankOverride::MinDist) ? header.minDistance
	                                                                : caller.minDistance.value_or(k_PlayDefaultMinDistance),
	    .maxDistance = fromHeader(pack::AudioBankOverride::MaxDist) ? header.maxDistance
	                                                                : caller.maxDistance.value_or(k_PlayDefaultMaxDistance),
	    .distanceScale = fromHeader(pack::AudioBankOverride::Scale) ? header.distanceScale : k_PlayDefaultDistanceScale,
	    .headerSet = set,
	};
}

/// Whether a later change of a playing sound's volume does something. It does nothing when the bank header set the
/// volume.
[[nodiscard]] constexpr bool VolumeChangeApplies(uint32_t headerSet)
{
	return !HasSetting(headerSet, pack::AudioBankOverride::Volume);
}

/// Whether a later change of a playing sound's playback rate does something. It does nothing when the bank header set
/// the rate, and a rate of 0 does nothing.
[[nodiscard]] constexpr bool PitchChangeApplies(uint32_t headerSet, uint32_t pitchPercent)
{
	return pitchPercent != 0 && !HasSetting(headerSet, pack::AudioBankOverride::Pitch);
}

} // namespace openblack::audio
