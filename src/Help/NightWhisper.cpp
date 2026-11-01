/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "NightWhisper.h"

#include <algorithm>
#include <array>
#include <bit>
#include <iterator>

#include "Common/X87.h"

using namespace openblack;
using namespace openblack::night_whisper;

namespace
{
constexpr char16_t k_Space = u' ';
/// The value that tells that a letter gives its own character as its code
constexpr uint8_t k_OwnCode = 0xFF;
/// The sound codes of the letters a to z
constexpr std::array<uint8_t, 26> k_LetterCodes {
    0, 1, 2, 3, 0, 1, 2, k_OwnCode, 0, 2, 2, 4, 5, 5, 0, 1, 2, 6, 2, 3, 0, 1, k_OwnCode, 2, k_OwnCode, 2,
};

/// The 0.65 limit of the random number of the playback rate, from the game's bits
constexpr float k_PitchDrawLimit = std::bit_cast<float>(0x3F266666u);
/// The 0.8 limit of the random number of the volume, from the game's bits
constexpr float k_VolumeDrawLimit = std::bit_cast<float>(0x3F4CCCCDu);
/// The pan is a random number below this
constexpr int32_t k_PanChoices = 180;

/// The character at a place of a text, 0 past its end
[[nodiscard]] char16_t At(std::u16string_view text, size_t place)
{
	return place < text.size() ? text[place] : u'\0';
}

/// The next sound code of a word from a place, which moves past the letters that it reads. A space or the end of the
/// text gives 0 and does not move the place.
[[nodiscard]] uint32_t NextCode(std::u16string_view text, size_t& place)
{
	for (;;)
	{
		const auto character = At(text, place);
		if (character == u'\0' || character == k_Space)
		{
			return 0;
		}
		++place;
		const auto code = SoundCode(character);
		if (code != 0)
		{
			// A letter with the same code as the letter after it gives one more, and the next call reads that letter again
			return SoundCode(At(text, place)) == code ? code + 1 : code;
		}
	}
}

/// The factor 1 + d cubed, or 1 divided by it when the coin is 0, as the game computes it in float
[[nodiscard]] float Factor(float draw, uint32_t coin)
{
	const float factor = draw * draw * draw + 1.0f;
	return coin == 0 ? 1.0f / factor : factor;
}
} // namespace

uint32_t night_whisper::SoundCode(char16_t character)
{
	const bool upper = character >= u'A' && character <= u'Z';
	const bool lower = character >= u'a' && character <= u'z';
	if (!upper && !lower)
	{
		return 0;
	}
	const auto letter = static_cast<size_t>(upper ? character - u'A' : character - u'a');
	const auto code = k_LetterCodes.at(letter);
	return code == k_OwnCode ? static_cast<uint32_t>(character) : code;
}

bool night_whisper::SoundsAlike(std::u16string_view name, std::u16string_view words)
{
	const auto first = At(name, 0);
	if (first != At(words, 0) || first == u'\0')
	{
		return false;
	}
	size_t namePlace = 1;
	size_t wordPlace = 1;
	constexpr int k_Codes = 3;
	for (int i = 0; i < k_Codes; ++i)
	{
		const auto nameCode = NextCode(name, namePlace);
		if (nameCode != NextCode(words, wordPlace))
		{
			return false;
		}
	}
	return true;
}

bool night_whisper::AnyWordSoundsAlike(std::u16string_view name, std::u16string_view words)
{
	size_t place = 0;
	if (At(words, place) == u'\0')
	{
		return false;
	}
	for (;;)
	{
		if (SoundsAlike(name, words.substr(place)))
		{
			return true;
		}
		while (At(words, place) != u'\0' && At(words, place) != k_Space)
		{
			++place;
		}
		if (At(words, place) == u'\0')
		{
			return false;
		}
		// Only one space is skipped
		++place;
		if (At(words, place) == u'\0')
		{
			return false;
		}
	}
}

std::optional<uint32_t> night_whisper::FindNameSample(std::span<const std::u16string> names, std::u16string_view words)
{
	if (words.empty() || words.front() == u'\0')
	{
		return std::nullopt;
	}
	for (size_t i = 0; i < names.size() && i < k_NameCount; ++i)
	{
		if (AnyWordSoundsAlike(names[i], words))
		{
			return NameSample(i);
		}
	}
	return std::nullopt;
}

std::u16string night_whisper::SecondName(std::u16string_view profileName)
{
	std::u16string kept;
	std::ranges::copy_if(profileName, std::back_inserter(kept),
	                     [](char16_t character) { return character >= u' ' && character <= u'~'; });
	if (kept.size() > k_SecondNameLength)
	{
		kept.resize(k_SecondNameLength);
	}
	return kept;
}

std::optional<uint32_t> night_whisper::LookUpSample(std::span<const std::u16string> names, std::u16string_view profileName)
{
	if (const auto sample = FindNameSample(names, profileName); sample.has_value())
	{
		return sample;
	}
	return FindNameSample(names, SecondName(profileName));
}

bool night_whisper::CheckWhispers(float draw, uint32_t count)
{
	const float share = (1.0f - draw * draw * draw) * 1000.0f;
	return static_cast<uint32_t>(x87::ToInteger(share)) < count;
}

Whisper night_whisper::MakeWhisper(uint32_t sample, uint32_t& volume, const Draws& draws)
{
	// The draws come in this order: the rate, its coin, the pan, the volume and its coin
	const float pitchDraw = draws.floatRand(k_PitchDrawLimit);
	const auto pitchCoin = draws.rand(2);
	const float pitchFactor = Factor(pitchDraw, pitchCoin);
	const auto pan = draws.rand(k_PanChoices);
	const float volumeDraw = draws.floatRand(k_VolumeDrawLimit);
	const auto volumeCoin = draws.rand(2);
	const float volumeFactor = Factor(volumeDraw, volumeCoin);

	const auto pitchPercent = static_cast<uint32_t>(x87::ToInteger(pitchFactor * 100.0f));
	// The volume of the last whisper changes by the factor, so the volume wanders from whisper to whisper
	volume = static_cast<uint32_t>(x87::ToInteger(x87::Mul(static_cast<double>(volume), volumeFactor)));
	return {.sample = sample, .pitchPercent = pitchPercent, .volume = volume, .pan = pan};
}

TurnResult Whisperer::Turn(int32_t landNumber, const std::function<bool()>& clockSaysNight, const Draws& draws)
{
	if (landNumber == 1 || landNumber == 2 || !_sample.has_value())
	{
		return std::monostate {};
	}
	if (_countdown != 0)
	{
		--_countdown;
		return std::monostate {};
	}
	_countdown = k_CheckEvery - 1;
	if (!clockSaysNight())
	{
		return std::monostate {};
	}
	TurnResult result = NoWhisper {};
	if (CheckWhispers(draws.floatRand(1.0f), _count))
	{
		result = MakeWhisper(*_sample, _volume, draws);
		_count = 0;
	}
	++_count;
	return result;
}
