/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ForceFeedbackRules.h"

#include <cmath>

#include <algorithm>
#include <numbers>

namespace openblack::force_feedback
{

namespace
{

constexpr double k_MicrosecondsPerSecond = 1000000.0;
constexpr double k_FullStrength = 10000.0;
constexpr uint32_t k_ShortestPeriodMs = 3;

[[nodiscard]] const ifr::Timing& TimingOf(const Part& part)
{
	return std::visit([](const auto& effect) -> const ifr::Timing& { return effect.timing; }, part);
}

/// The wave from -1 to 1 at a place in its cycle, from 0 to 1
[[nodiscard]] double Wave(ifr::Waveform waveform, double cycle)
{
	switch (waveform)
	{
	case ifr::Waveform::Sine:
		return std::sin(2.0 * std::numbers::pi * cycle);
	case ifr::Waveform::Square:
		return cycle < 0.5 ? 1.0 : -1.0;
	case ifr::Waveform::Triangle:
		if (cycle < 0.25)
		{
			return 4.0 * cycle;
		}
		return cycle < 0.75 ? 2.0 - (4.0 * cycle) : (4.0 * cycle) - 4.0;
	case ifr::Waveform::SawtoothUp:
		return (2.0 * cycle) - 1.0;
	case ifr::Waveform::SawtoothDown:
		return 1.0 - (2.0 * cycle);
	}
	return 0.0;
}

/// The strength of the envelope, from 0 to 10000, at a time in microseconds from the start of one play of the effect
[[nodiscard]] double EnvelopeLevel(const ifr::Periodic& periodic, double micros)
{
	const auto magnitude = static_cast<double>(periodic.magnitude);
	if (!periodic.envelope.has_value())
	{
		return magnitude;
	}
	const auto& envelope = *periodic.envelope;
	if (envelope.attackTime > 0 && micros < envelope.attackTime)
	{
		const auto attack = static_cast<double>(envelope.attackLevel);
		return attack + ((magnitude - attack) * micros / envelope.attackTime);
	}
	if (periodic.timing.duration.has_value() && envelope.fadeTime > 0)
	{
		const auto fadeStart = static_cast<double>(*periodic.timing.duration) - envelope.fadeTime;
		if (micros > fadeStart)
		{
			const auto fade = static_cast<double>(envelope.fadeLevel);
			return magnitude + ((fade - magnitude) * (micros - fadeStart) / envelope.fadeTime);
		}
	}
	return magnitude;
}

/// True when the stretch from one place to the next on an axis meets a bump
[[nodiscard]] bool MeetsBump(float from, float to, int32_t offset, uint32_t width, uint32_t space)
{
	if (space == 0 || width == 0)
	{
		return false;
	}
	const auto low = static_cast<double>(std::min(from, to)) - offset;
	const auto high = static_cast<double>(std::max(from, to)) - offset;
	// The bumps start every space pixels from the offset, each as wide as its width
	const auto first = std::floor((low - width) / space);
	for (auto k = first; k * space <= high; k += 1.0)
	{
		const auto start = k * space;
		if (start + width > low && start <= high)
		{
			return true;
		}
	}
	return false;
}

/// The vibration of the bumps on one axis, from 0 to 1
[[nodiscard]] float AxisStrength(const ifr::TextureAxis& axis, float from, float to)
{
	if (to == from)
	{
		return 0.0f;
	}
	const bool positive = to > from;
	const auto magnitude = positive ? axis.positiveMagnitude : axis.negativeMagnitude;
	const auto width = positive ? axis.positiveWidth : axis.negativeWidth;
	const auto space = positive ? axis.positiveSpace : axis.negativeSpace;
	if (!MeetsBump(from, to, axis.offset, width, space))
	{
		return 0.0f;
	}
	return static_cast<float>(std::min(1.0, std::abs(magnitude) / k_FullStrength));
}

} // namespace

const EffectEntry* EntryFor(ImmersionEffectType type)
{
	const auto index = static_cast<int>(type);
	if (index < 0 || index >= static_cast<int>(k_Effects.size()))
	{
		return nullptr;
	}
	return &k_Effects.at(static_cast<size_t>(index));
}

bool LevelAllows(ImmersionEffectType type, int level)
{
	const auto* entry = EntryFor(type);
	return entry != nullptr && entry->level <= level;
}

std::string_view FileFor(DeviceKind kind)
{
	return kind == DeviceKind::FullForce ? "FullFeedback.ifr" : "Tactile.ifr";
}

Effect EffectFrom(const ifr::IFRFile& file, const ifr::Object& object)
{
	Effect effect;
	for (const auto* part : file.Effects(object))
	{
		if (part->type == "Periodic")
		{
			effect.parts.emplace_back(ifr::ReadPeriodic(*part));
		}
		else if (part->type == "Texture")
		{
			effect.parts.emplace_back(ifr::ReadTexture(*part));
		}
		else
		{
			effect.parts.emplace_back(ifr::ReadCondition(*part));
		}
	}
	// The beat of the first part is kept as a period in whole milliseconds and the duration as a number of those periods
	if (!effect.parts.empty())
	{
		if (const auto* periodic = std::get_if<ifr::Periodic>(&effect.parts.front()); periodic != nullptr)
		{
			const auto period = periodic->period;
			if (period > 0)
			{
				const auto duration = periodic->timing.duration.value_or(UINT32_MAX);
				effect.periodMs = period / 1000;
				effect.durationRatio = static_cast<float>(static_cast<double>(duration) / period);
			}
		}
	}
	return effect;
}

std::optional<double> PlaySeconds(const Part& part, std::optional<uint32_t> iterations)
{
	const auto& timing = TimingOf(part);
	if (!timing.duration.has_value())
	{
		return std::nullopt;
	}
	const auto times = iterations.value_or(timing.iterations);
	return (timing.startDelay + (static_cast<double>(*timing.duration) * times)) / k_MicrosecondsPerSecond;
}

bool Plays(const Effect& effect, double seconds, std::optional<uint32_t> iterations)
{
	return std::ranges::any_of(effect.parts, [seconds, iterations](const Part& part) {
		const auto length = PlaySeconds(part, iterations);
		return !length.has_value() || seconds < *length;
	});
}

void QuickenBeat(Effect& effect, float fraction)
{
	if (effect.parts.empty() || !effect.periodMs.has_value() || !effect.durationRatio.has_value() ||
	    *effect.durationRatio <= 0.0f)
	{
		return;
	}
	auto* periodic = std::get_if<ifr::Periodic>(&effect.parts.front());
	if (periodic == nullptr)
	{
		return;
	}
	const auto periodMs = std::max(k_ShortestPeriodMs, static_cast<uint32_t>(*effect.periodMs * (1.0f - fraction)));
	const auto durationMs = static_cast<uint64_t>(static_cast<float>(periodMs) * *effect.durationRatio);
	periodic->period = periodMs * 1000;
	periodic->timing.duration = static_cast<uint32_t>(std::min<uint64_t>(durationMs * 1000, UINT32_MAX - 1));
}

void SetPeriodAndMagnitude(Effect& effect, uint32_t periodMs, uint32_t magnitude)
{
	if (effect.parts.empty())
	{
		return;
	}
	if (auto* periodic = std::get_if<ifr::Periodic>(&effect.parts.front()); periodic != nullptr)
	{
		periodic->period = periodMs * 1000;
		periodic->magnitude = magnitude;
	}
}

HandTextureStep NextHandTexture(HandTextureState& state, std::optional<ImmersionEffectType> wanted,
                                const std::function<bool(ImmersionEffectType)>& isPlaying)
{
	HandTextureStep step;
	if (wanted == state.current)
	{
		if (state.startedElsewhere && state.current.has_value() && !isPlaying(*state.current))
		{
			state.startedElsewhere = false;
			step.start = state.current;
		}
		return step;
	}
	if (state.current.has_value() && !state.startedElsewhere)
	{
		step.stop = state.current;
	}
	if (wanted.has_value())
	{
		state.startedElsewhere = isPlaying(*wanted);
		step.start = wanted;
	}
	state.current = wanted;
	return step;
}

std::optional<bool> FurStep(bool touchesBody, bool furPlays)
{
	if (touchesBody)
	{
		return !furPlays;
	}
	return furPlays ? std::optional(false) : std::nullopt;
}

float PeriodicStrength(const ifr::Periodic& periodic, double seconds, std::optional<uint32_t> iterations)
{
	const auto micros = (seconds * k_MicrosecondsPerSecond) - periodic.timing.startDelay;
	if (micros < 0.0)
	{
		return 0.0f;
	}
	auto local = micros;
	if (periodic.timing.duration.has_value())
	{
		const auto duration = static_cast<double>(*periodic.timing.duration);
		const auto times = iterations.value_or(periodic.timing.iterations);
		if (duration <= 0.0 || micros >= duration * times)
		{
			return 0.0f;
		}
		local = std::fmod(micros, duration);
	}
	const auto level = EnvelopeLevel(periodic, local);
	const auto gain = periodic.timing.gain / k_FullStrength;
	double value = 0.0;
	if (periodic.period == 0 || periodic.period / k_MicrosecondsPerSecond < k_QuickWaveSeconds)
	{
		value = std::abs(periodic.offset) + level;
	}
	else
	{
		const auto cycle = std::fmod((micros / periodic.period) + (periodic.phase / 36000.0), 1.0);
		value = std::abs(periodic.offset + (level * Wave(periodic.waveform, cycle)));
	}
	return static_cast<float>(std::clamp(value * gain / k_FullStrength, 0.0, 1.0));
}

float TextureStrength(const ifr::Texture& texture, glm::vec2 from, glm::vec2 to)
{
	const auto gain = static_cast<float>(texture.timing.gain / k_FullStrength);
	switch (texture.axes)
	{
	case ifr::Axes::X:
		return AxisStrength(texture.first, from.x, to.x) * gain;
	case ifr::Axes::Y:
		return AxisStrength(texture.first, from.y, to.y) * gain;
	case ifr::Axes::Both:
		return std::max(AxisStrength(texture.first, from.x, to.x), AxisStrength(texture.second, from.y, to.y)) * gain;
	}
	return 0.0f;
}

} // namespace openblack::force_feedback
