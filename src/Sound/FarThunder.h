/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <numbers>
#include <optional>
#include <span>
#include <utility>

/// How far thunder is heard. This is a deviation from the game, which the user asked for: the whole land hears
/// thunder. Up to the maximum distance of its sample, thunder plays with the volume and the fall-off of the game. Past
/// it, the thunder gets quieter as real thunder does, and air takes its high sounds first. Thus the crack becomes a
/// rumble, and the rumble goes quiet far away.
///
/// The numbers come from real thunder, in metres:
/// - Thunder is heard sharp to about 1 to 2 km, and as a rumble past that (M. A. Uman, "The Lightning Discharge",
///   1987; A. A. Few, "Thunder", Scientific American 233, 1975).
/// - Thunder is seldom heard past about 15 to 25 km (Uman 1987; the U.S. National Weather Service tells people that
///   thunder is heard to about 10 miles, 16 km).
/// - A lightning channel is a line about 5 km long. Near a line, the sound gets quieter by 3 dB each time the distance
///   doubles. Far from it, the sound gets quieter by 6 dB each time, as from a point (Few 1975).
/// - Air takes the high sounds, by the absorption of ISO 9613-1 (ISO 9613-2:1996, table 2, at 20 degrees C and 70 %
///   relative humidity).
namespace openblack::audio::far_thunder
{

/// Up to this distance, thunder sounds sharp: the air takes nothing from its crack. Past it, the air path that
/// softens the crack starts to grow slowly.
inline constexpr float k_SharpDistance = 550.0f;
/// For each metre past the sharp distance, the air path that softens the crack grows by this part of a metre. Thus the
/// crack is softer at 1 km, but the thunder is not yet a rumble.
inline constexpr float k_SoftAirPathPerMetre = 0.36f;
/// Past this distance, each metre adds one metre of air path, and the crack becomes a rumble. The air path is the
/// larger of the two.
inline constexpr float k_RumbleDistance = 1500.0f;
/// The length of a lightning channel. Nearer than this, thunder falls off as from a line; further, as from a point.
inline constexpr float k_ChannelLength = 5000.0f;
/// From this distance, thunder gets quieter to silence
inline constexpr float k_FadeDistance = 15000.0f;
/// Past this distance, nobody hears thunder
inline constexpr float k_SilentDistance = 25000.0f;
/// The cut-off of the filter is the pitch where the air takes this much of the sound, in dB
inline constexpr float k_CutOffAbsorption = 3.0f;

/// The absorption of the air at the centre of each octave band: the pitch in Hz, and the loss in dB each kilometre
inline constexpr std::array<std::pair<float, float>, 8> k_AirAbsorption {{
    {63.0f, 0.1f},
    {125.0f, 0.3f},
    {250.0f, 1.1f},
    {500.0f, 2.8f},
    {1000.0f, 5.0f},
    {2000.0f, 9.0f},
    {4000.0f, 22.9f},
    {8000.0f, 76.6f},
}};

/// How the listener hears thunder at a distance
struct Hearing
{
	/// The part of the volume that this model gives, on top of the fall-off of the sample's own distances
	float gain {1.0f};
	/// The cut-off of the low-pass filter in Hz. None when the thunder is sharp.
	std::optional<float> cutOff;

	bool operator==(const Hearing&) const = default;
};

/// The pitch in Hz where the air takes the given loss in dB each kilometre. The value is between the octave bands of
/// the table, on a log scale. None for a loss above the highest band: then only pitches above 8 kHz go.
[[nodiscard]] inline std::optional<float> PitchOfAbsorption(float lossPerKilometre)
{
	if (lossPerKilometre >= k_AirAbsorption.back().second)
	{
		return std::nullopt;
	}
	if (lossPerKilometre <= k_AirAbsorption.front().second)
	{
		return k_AirAbsorption.front().first;
	}
	const auto upper =
	    std::ranges::find_if(k_AirAbsorption, [lossPerKilometre](const auto& band) { return band.second >= lossPerKilometre; });
	const auto lower = std::prev(upper);
	const float part = std::log(lossPerKilometre / lower->second) / std::log(upper->second / lower->second);
	return lower->first * std::pow(upper->first / lower->first, part);
}

/// How the listener hears thunder at this distance. The sample's own distances give the volume up to its maximum
/// distance, `sampleReach`. Past it, the thunder falls off as from a line to the channel length, then as from a point,
/// and fades to silence between the fade and the silent distances. Past the sharp distance, a low-pass filter takes
/// the high sounds that the air takes over the air path. The filter does not change the true pitch or the speed of the
/// sample.
[[nodiscard]] inline Hearing HearingAt(float distance, float sampleReach)
{
	if (!(distance < k_SilentDistance))
	{
		return {.gain = 0.0f, .cutOff = std::nullopt};
	}
	Hearing hearing;
	if (distance > sampleReach && sampleReach > 0.0f)
	{
		const float line = std::min(distance, k_ChannelLength);
		hearing.gain = std::sqrt(sampleReach / std::max(line, sampleReach));
		if (distance > k_ChannelLength)
		{
			hearing.gain *= k_ChannelLength / distance;
		}
	}
	if (distance > k_FadeDistance)
	{
		hearing.gain *= (k_SilentDistance - distance) / (k_SilentDistance - k_FadeDistance);
	}
	if (distance > k_SharpDistance)
	{
		const float soft = (distance - k_SharpDistance) * k_SoftAirPathPerMetre;
		const float rumble = distance - k_RumbleDistance;
		const float kilometres = std::max(soft, rumble) * 0.001f;
		hearing.cutOff = PitchOfAbsorption(k_CutOffAbsorption / kilometres);
	}
	return hearing;
}

/// The coefficients of a second-order low-pass filter with a flat pass band (Butterworth), as the
/// Audio EQ Cookbook of R. Bristow-Johnson gives them
struct LowPassCoefficients
{
	float b0 {1.0f};
	float b1 {0.0f};
	float b2 {0.0f};
	float a1 {0.0f};
	float a2 {0.0f};
};

[[nodiscard]] inline LowPassCoefficients LowPassOf(float cutOff, int sampleRate)
{
	const float omega = 2.0f * std::numbers::pi_v<float> * cutOff / static_cast<float>(sampleRate);
	// The quality factor of a flat pass band is 1 / sqrt(2)
	const float alpha = std::sin(omega) / std::numbers::sqrt2_v<float>;
	const float cosine = std::cos(omega);
	const float a0 = 1.0f + alpha;
	return {
	    .b0 = (1.0f - cosine) * 0.5f / a0,
	    .b1 = (1.0f - cosine) / a0,
	    .b2 = (1.0f - cosine) * 0.5f / a0,
	    .a1 = -2.0f * cosine / a0,
	    .a2 = (1.0f - alpha) / a0,
	};
}

/// Filters interleaved samples with the low-pass filter. Each channel has its own filter state. A cut-off at or
/// above half the sample rate changes nothing.
inline void LowPass(std::span<int16_t> samples, size_t channels, int sampleRate, float cutOff)
{
	if (channels == 0 || sampleRate <= 0 || !(cutOff < static_cast<float>(sampleRate) * 0.5f))
	{
		return;
	}
	const auto filter = LowPassOf(cutOff, sampleRate);
	for (size_t channel = 0; channel < channels; ++channel)
	{
		float in1 = 0.0f;
		float in2 = 0.0f;
		float out1 = 0.0f;
		float out2 = 0.0f;
		for (size_t i = channel; i < samples.size(); i += channels)
		{
			const float in = samples[i];
			const float out = filter.b0 * in + filter.b1 * in1 + filter.b2 * in2 - filter.a1 * out1 - filter.a2 * out2;
			in2 = in1;
			in1 = in;
			out2 = out1;
			out1 = out;
			samples[i] = static_cast<int16_t>(std::clamp(std::lround(out), -32768L, 32767L));
		}
	}
}

} // namespace openblack::audio::far_thunder
