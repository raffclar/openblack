/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LipSync.h"

#include <cmath>

#include <algorithm>
#include <utility>

namespace openblack::audio::lip_sync
{

namespace
{
constexpr double k_TwoPi = 6.2831853071795898;
/// A 16-bit sample's full scale
constexpr float k_SampleScale = 32768.0f;
/// The weights' rise and fall limit per second, per unit of rate
constexpr float k_RateScale = 0.18f;
} // namespace

void Fft(std::span<float> data, int n, bool forward)
{
	// One-based indices, as the algorithm is written
	auto at = [&data](int i) -> float& { return data[static_cast<size_t>(i - 1)]; };
	const int size = n * 2;
	// Bit reversal
	int j = 1;
	for (int i = 1; i < size; i += 2)
	{
		if (j > i)
		{
			std::swap(at(j), at(i));
			std::swap(at(j + 1), at(i + 1));
		}
		int m = n;
		while (m >= 2 && j > m)
		{
			j -= m;
			m >>= 1;
		}
		j += m;
	}
	// The butterflies. The game rounds every step to single precision except the two sines, so the angle is a float
	// and the sines' products are rounded once.
	const int sign = forward ? 1 : -1;
	int mmax = 2;
	while (size > mmax)
	{
		const int step = mmax * 2;
		const auto theta = static_cast<float>(k_TwoPi / static_cast<double>(mmax * sign));
		const double halfSine = std::sin(static_cast<double>(theta * 0.5f));
		const float wpr = static_cast<float>(halfSine * halfSine) * -2.0f;
		const double wpi = std::sin(static_cast<double>(theta));
		float wr = 1.0f;
		float wi = 0.0f;
		for (int m = 1; m < mmax; m += 2)
		{
			for (int i = m; i <= size; i += step)
			{
				const int k = i + mmax;
				const float tempr = at(k) * wr - at(k + 1) * wi;
				const float tempi = at(k) * wi + at(k + 1) * wr;
				at(k) = at(i) - tempr;
				at(k + 1) = at(i + 1) - tempi;
				at(i) = tempr + at(i);
				at(i + 1) = tempi + at(i + 1);
			}
			const float previous = wr;
			wr = (previous * wpr - static_cast<float>(static_cast<double>(wi) * wpi)) + previous;
			wi = wi * (wpr + 1.0f) + static_cast<float>(wpi * static_cast<double>(previous));
		}
		mmax = step;
	}
}

void MagnitudeSpectrum(std::span<const int16_t> samples, std::span<float> out, int n)
{
	// The triangle window rises by this much a sample up to the middle, then falls back
	const float step = 1.0f / (static_cast<float>(n) * k_SampleScale);
	float window = 0.0f;
	const int half = n / 2;
	for (int i = 0; i < n; ++i)
	{
		window = i < half ? window + step : window - step;
		const auto index = static_cast<size_t>(i);
		out[2 * index] = static_cast<float>(samples[index]) * window;
		out[2 * index + 1] = 0.0f;
	}
	Fft(out, n, true);
	const auto count = static_cast<float>(n);
	for (size_t i = 0; i < static_cast<size_t>(n); ++i)
	{
		// Packed at the front, which only overwrites pairs already read
		const float re = out[2 * i];
		const float im = out[2 * i + 1];
		out[i] = std::sqrt((re * re + im * im) / count);
	}
}

float BandLevel(std::span<const float> spectrum, int n, float rate, float low, float high)
{
	const auto bins = static_cast<float>(n);
	const int from = std::min(static_cast<int>(low / rate * bins), n);
	const int to = std::min(static_cast<int>(high / rate * bins), n);
	if (from >= to)
	{
		return 0.0f;
	}
	float sum = 0.0f;
	for (int i = from; i < to; ++i)
	{
		sum += spectrum[static_cast<size_t>(i)];
	}
	return sum / static_cast<float>(to - from);
}

Levels UpdateKey(const Settings& settings, Key& key, float dt, float time, std::span<const int16_t> samples, int frames,
                 int sampleRate, std::span<float> spectrum, int window)
{
	Levels levels;
	const auto rate = static_cast<float>(sampleRate);
	const auto centre = static_cast<int>((settings.offsetMs * 0.001f + dt + time) * rate);
	const int start = std::min(std::max(centre - window / 2, 0), frames - window);
	if (start < 0 || static_cast<size_t>(start + window) > samples.size())
	{
		// Too short a recording for the window: the game would read before it
		return levels;
	}
	MagnitudeSpectrum(samples.subspan(static_cast<size_t>(start)), spectrum, window);

	float total = 0.0f;
	for (size_t i = 0; i < settings.bands.size(); ++i)
	{
		const Band& band = settings.bands.at(i);
		levels.bands.at(i) = BandLevel(spectrum, window, rate, band.low, band.high) * band.gain;
		total += levels.bands.at(i);
	}
	if (total < settings.threshold)
	{
		total = 0.0f;
	}
	levels.total = total;
	const float squared = total * total;
	if (squared != 0.0f)
	{
		for (float& level : levels.bands)
		{
			level = level * level / squared;
		}
	}

	// The loudest band, the last one on a tie
	const auto& b = levels.bands;
	size_t loudest = 2;
	if (b[0] > b[1] && b[0] > b[2])
	{
		loudest = 0;
	}
	else if (b[1] > b[2] && b[1] > b[0])
	{
		loudest = 1;
	}
	const float level = std::min(settings.level * total, 1.0f);

	float sum = 0.0f;
	for (size_t i = 0; i < settings.bands.size(); ++i)
	{
		float& weight = key.weights.at(static_cast<size_t>(settings.bands.at(i).weight));
		const float ratio = b.at(i) / b.at(loudest);
		float target = ratio * ratio * level;
		const float fall = dt * settings.rate * k_RateScale;
		if (weight - target > fall)
		{
			target = weight - fall;
		}
		const float rise = fall + fall;
		if (target - weight > rise)
		{
			target = rise + weight;
		}
		weight = target;
		sum += weight;
	}
	if (sum > 1.0f)
	{
		const float scale = 1.0f / sum;
		for (const Band& band : settings.bands)
		{
			key.weights.at(static_cast<size_t>(band.weight)) *= scale;
		}
	}
	return levels;
}

} // namespace openblack::audio::lip_sync
