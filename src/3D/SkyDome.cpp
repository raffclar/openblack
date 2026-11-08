/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkyDome.h"

#include <cctype>
#include <cmath>
#include <cstddef>

#include <algorithm>
#include <array>
#include <string_view>

#include <glm/gtc/matrix_transform.hpp>

#include "Common/X87.h"

namespace openblack::sky_dome
{

namespace
{
constexpr uint8_t k_Night = 0;
constexpr uint8_t k_Dusk = 1;
constexpr uint8_t k_Day = 2;
/// How fast green and blue of the low resolution colour fall towards night; red falls at 2
constexpr float k_LowGreen = 1.7f;
constexpr float k_LowBlue = 1.5f;

/// A fraction of 255, cut to a whole number
uint8_t Weight(float fraction)
{
	return static_cast<uint8_t>(static_cast<int>(fraction * 255.0f));
}
} // namespace

std::array<std::string, k_Pictures> PictureFiles()
{
	constexpr std::array<std::string_view, 3> k_Alignments = {"good", "Ntrl", "evil"};
	constexpr std::array<std::string_view, 3> k_Times = {"night", "dusk", "day"};
	std::array<std::string, k_Pictures> files;
	size_t index = 0;
	for (const auto alignment : k_Alignments)
	{
		for (const auto time : k_Times)
		{
			std::string prefix = "sky";
			std::string hour(time);
			if (alignment == "Ntrl")
			{
				prefix[0] = 'S';
				hour[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(hour[0])));
			}
			files.at(index++) = prefix + "_" + std::string(alignment) + "_" + hour + ".555";
		}
	}
	return files;
}

Follow::Follow(float skyType)
    : _wholeDome(skyType)
{
}

FrameRows Follow::Advance(float skyType)
{
	FrameRows frame;
	if (_wholeDome)
	{
		frame.rows.at(frame.count++) = {.skyType = *_wholeDome, .first = 0, .count = k_Rows};
		_wholeDome.reset();
	}
	if (_rowsDone >= k_Rows)
	{
		if (std::abs(static_cast<double>(skyType - _following)) <= k_Hysteresis)
		{
			return frame;
		}
		_following = skyType;
		_rowsDone = 0;
	}
	frame.rows.at(frame.count++) = {.skyType = _following, .first = _rowsDone, .count = k_RowsPerFrame};
	_rowsDone += k_RowsPerFrame;
	return frame;
}

void Follow::Jump(float skyType)
{
	_following = skyType;
	_rowsDone = 0;
	_wholeDome = skyType;
}

void Picture::Drew(std::span<const Rows> rows)
{
	for (const auto& band : rows)
	{
		const auto first = std::min<size_t>(band.first, k_Rows);
		const auto last = std::min<size_t>(static_cast<size_t>(band.first) + band.count, k_Rows);
		std::fill(_skyTypes.begin() + static_cast<std::ptrdiff_t>(first), _skyTypes.begin() + static_cast<std::ptrdiff_t>(last),
		          band.skyType);
	}
}

std::vector<Rows> Picture::Again() const
{
	std::vector<Rows> bands;
	for (uint16_t row = 0; row < k_Rows; ++row)
	{
		const auto& skyType = _skyTypes.at(row);
		if (!skyType.has_value())
		{
			continue;
		}
		// The row continues the band above it when that band ends at this row and has the same sky type
		if (!bands.empty() && bands.back().first + bands.back().count == row && bands.back().skyType == *skyType)
		{
			++bands.back().count;
		}
		else
		{
			bands.push_back({.skyType = *skyType, .first = row, .count = 1});
		}
	}
	return bands;
}

Pair TimePair(float skyType)
{
	if (skyType > 1.0f)
	{
		return {.lower = k_Dusk, .upper = k_Night, .weight = Weight(skyType - 1.0f)};
	}
	return {.lower = k_Day, .upper = k_Dusk, .weight = Weight(skyType)};
}

Pair AlignmentPair(float sky)
{
	if (sky > 1.0f)
	{
		return {.lower = 1, .upper = 2, .weight = Weight(std::min(sky, 2.0f) - 1.0f)};
	}
	return {.lower = 0, .upper = 1, .weight = Weight(std::max(sky, 0.0f))};
}

uint8_t BlendChannel(uint8_t lower, uint8_t upper, uint8_t weight)
{
	return static_cast<uint8_t>(((lower * (255 - weight)) >> 8) + ((upper * weight) >> 8));
}

uint8_t Darkness(float sky)
{
	const float good = 1.0f - (sky * 0.5f);
	if (!(good > 0.6f))
	{
		return 0;
	}
	// The product rounds to a float before it is cut to a whole number
	const auto darkness = static_cast<int>((good - 0.6f) * 225.0f);
	return static_cast<uint8_t>(std::clamp(darkness, 0, 90));
}

float ThroughOvercast(float alpha, float overcast, bool fog)
{
	const auto whole = static_cast<int>(alpha);
	if (!fog || !(overcast > 0.0f))
	{
		return static_cast<float>(whole);
	}
	return static_cast<float>(static_cast<int>(static_cast<float>(whole) / ((std::min(overcast, 1.0f) * 8.0f) + 1.0f)));
}

glm::u8vec3 LowResolutionColour(uint32_t landColour, float skyType)
{
	// Half of the sky counted from day: 1 by day, 0 at night. Towards night, blue becomes darker first, then green, then red.
	const float day = (2.0f - skyType) * 0.5f;
	const std::array<float, 3> parts {
	    std::min(1.0f, (day + day) + 0.5f),
	    std::min(1.0f, x87::Add(x87::Mul(day, k_LowGreen), 0.5f)),
	    std::min(1.0f, x87::Add(x87::Mul(day, k_LowBlue), 0.5f)),
	};
	glm::u8vec3 colour;
	for (glm::length_t c = 0; c < 3; ++c)
	{
		const auto channel = static_cast<float>((landColour >> (16u - (8u * static_cast<uint32_t>(c)))) & 0xFFu);
		colour[c] = static_cast<uint8_t>(static_cast<int>(channel * parts.at(static_cast<size_t>(c))));
	}
	return colour;
}

Tint TintOf(const TintInputs& inputs)
{
	const int overcast = static_cast<int>(std::clamp(inputs.overcast, 0.0f, 1.0f) * 255.0f);
	// Both colours lose this much of 256 under an evil sky, rounded up
	const int dark = inputs.weather ? (inputs.darkness * inputs.darkness) / 150 : 0;
	const int halfFlash = inputs.flash / 2;
	const auto first = inputs.lowResolution ? LowResolutionColour(inputs.landColour, inputs.skyType) : glm::u8vec3(255);
	Tint tint;
	for (glm::length_t c = 0; c < 3; ++c)
	{
		int modulate = first[c];
		int add = 0;
		if (inputs.fog)
		{
			// From the first colour towards the haze's colour, and the haze's colour added, by the overcast
			const int haze = static_cast<int>(inputs.hazeColour[c]);
			modulate += ((haze - modulate) * overcast) >> 8;
			add = (haze * overcast) >> 8;
		}
		modulate += (-modulate * dark) >> 8;
		add += (-add * dark) >> 8;
		// A flash takes the first towards white, and the second half as far
		modulate += ((255 - modulate) * inputs.flash) >> 8;
		add += ((255 - add) * halfFlash) >> 8;
		tint.modulate[c] = static_cast<uint8_t>(modulate);
		tint.add[c] = static_cast<uint8_t>(add);
	}
	return tint;
}

glm::mat4 Placement()
{
	return glm::scale(glm::translate(glm::mat4(1.0f), k_Centre), glm::vec3(k_Scale));
}

} // namespace openblack::sky_dome
