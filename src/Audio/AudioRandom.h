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

namespace openblack::audio
{

/// The random generator of the sound library.
///
/// The sound library has its own generator, which the game does not share. The generator gets a new seed from the
/// clock at these times: each time that an atmosphere bank is registered, at the first sample that plays, and at the
/// first pick among the samples of an animation effect. Each sample that plays takes one number for the change of its
/// pitch, and each pick among two or more samples of an animation effect takes one. Thus the atmosphere, the samples
/// and the animation effects take their numbers from one stream.
class AudioRandom
{
public:
	static constexpr uint32_t k_RandMax = 0x7FFF;

	void Seed(uint32_t seed) noexcept { _state = seed; }

	uint32_t Next() noexcept
	{
		_state = (_state * 214013u) + 2531011u;
		return (_state >> 16) & k_RandMax;
	}

private:
	uint32_t _state {1};
};

/// The draw of the sound library that picks one sample of an animation effect. It halves a number of the stream. On
/// the first draw and on each second draw after it, it moves the half to the upper half of the range. Thus with two
/// samples, the picks change from one sample to the other.
class AnimEffectDraw
{
public:
	/// The largest number that a half of the stream gives
	static constexpr uint32_t k_HalfMax = AudioRandom::k_RandMax / 2;

	/// A number from 0 to 2 * k_HalfMax, from a number of the stream from 0 to AudioRandom::k_RandMax
	[[nodiscard]] uint32_t Next(uint32_t number) noexcept
	{
		auto half = number / 2;
		if (_upper)
		{
			half += k_HalfMax;
		}
		_upper = !_upper;
		return half;
	}

private:
	bool _upper {true};
};

/// The sample of an animation effect with count samples that a draw picks, from 0 to count - 1
[[nodiscard]] constexpr uint32_t AnimEffectSampleIndex(uint32_t draw, uint32_t count) noexcept
{
	return (draw * count) / AudioRandom::k_RandMax;
}

} // namespace openblack::audio
