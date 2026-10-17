/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FilmCreature.h"

#include <cmath>

#include <array>

#include "Common/ModelInverseSquareRoot.h"

using namespace openblack;

std::optional<int32_t> film_creature::Step(Clock& clock, int32_t filmMs)
{
	const auto elapsed = filmMs - clock.lastMs;
	clock.lastMs = filmMs;
	return elapsed > 0 ? std::optional(elapsed) : std::nullopt;
}

bool film_creature::Shown(int32_t filmMs)
{
	return filmMs < k_HiddenFromMs || filmMs > k_HiddenToMs;
}

int32_t film_creature::Brightness(int32_t filmMs)
{
	int32_t brightness = 0;
	if (filmMs < k_FirstRiseMs)
	{
		brightness = (filmMs * 255) / k_FirstRiseMs;
	}
	else if (filmMs < k_SecondRiseEndMs)
	{
		brightness = ((filmMs - k_FirstRiseMs) * 255) / (k_SecondRiseEndMs - k_FirstRiseMs);
	}
	if (brightness <= 0)
	{
		return 0;
	}
	return brightness >= 255 ? 255 : brightness;
}

film_creature::Look film_creature::LookAt(int32_t brightness, uint32_t red, uint32_t green, uint32_t blue)
{
	const auto k = static_cast<uint32_t>(brightness);
	// The added colour: a dark red that flickers, each channel scaled by the brightness, of 256
	const uint32_t flicker = ((((red << 8u) + green + 0xFF2008u) << 8u) + blue);
	const uint32_t added = ((((flicker & 0xFF0000u) * k) & 0xFF0000FFu) | (((flicker & 0xFF00u) * k) & 0xFF0000u) |
	                        (((flicker & 0xFFu) * k) & 0xFF00u)) >>
	                           8u |
	                       (flicker & 0xFF000000u);

	// The light's colour falls from white as the brightness rises: the red by 127/256 of it, the green by 191/256 and
	// the blue by 255/256, each step in whole numbers
	const uint32_t redLight = ((((0u - (k * 127u)) << 16u) >> 8u) - 0x10000u) & 0xFFFF0000u;
	const uint32_t greenLight = ((((((0u - (k * 191u)) << 8u) >> 8u) - 0x100u) >> 8u) & 0xFFu) << 8u;
	const uint32_t blueLight = (((0u - (k * 255u)) >> 8u) - 1u) & 0xFFu;
	return {.colour = redLight | greenLight | blueLight | 0xFF000000u, .added = added};
}

std::optional<float> film_creature::GlowPower(creature_hand_glow::Place hand, int32_t filmMs)
{
	// When a glow starts to rise, starts to fall, ends its fall, and rises again for a second
	struct Ramp
	{
		int32_t riseFromMs;
		int32_t fallFromMs;
		int32_t fallToMs;
		int32_t againFromMs;
	};
	constexpr Ramp k_Left {.riseFromMs = 16350, .fallFromMs = 17350, .fallToMs = 19350, .againFromMs = 31650};
	constexpr Ramp k_Right {.riseFromMs = 17200, .fallFromMs = 18200, .fallToMs = 20200, .againFromMs = 36500};
	constexpr int32_t k_RampMs = 1000;
	if (hand != creature_hand_glow::Place::LeftHand && hand != creature_hand_glow::Place::RightHand)
	{
		return std::nullopt;
	}
	const auto& ramp = hand == creature_hand_glow::Place::LeftHand ? k_Left : k_Right;
	int32_t level = 0;
	if (filmMs >= ramp.riseFromMs && filmMs < ramp.fallFromMs)
	{
		level = filmMs - ramp.riseFromMs;
	}
	else if (filmMs >= ramp.fallFromMs && filmMs < ramp.fallToMs)
	{
		level = k_RampMs - ((filmMs - ramp.fallFromMs) / 2);
	}
	if (filmMs > ramp.againFromMs - 1 && filmMs < ramp.againFromMs + k_RampMs)
	{
		level = filmMs - ramp.againFromMs;
	}
	// A level of 0 leaves the glow as it is
	if (level == 0)
	{
		return std::nullopt;
	}
	const float scaled = static_cast<float>(level) * 0.001f;
	float part = 0.0f;
	if (scaled > 0.0f)
	{
		part = scaled >= 1.0f ? 1.0f : scaled;
	}
	const float doubled = part + part;
	return doubled + 1.0f;
}

glm::mat4 film_creature::View(const FilmCameraPath::Point& point)
{
	const auto& m = point.frame;
	// The rows of the matrix that takes a point from the world to the camera, the path's axes turned on their side
	std::array<std::array<float, 3>, 3> rows {{
	    {m[0], m[3], -m[6]},
	    {m[1], m[4], -m[7]},
	    {m[2], m[5], -m[8]},
	}};
	for (auto& row : rows)
	{
		const float x = row[0] * row[0];
		const float y = row[1] * row[1];
		const float z = row[2] * row[2];
		const float xy = x + y;
		const float inverse = gutils::ModelInverseSquareRoot(xy + z);
		row[0] = inverse * row[0];
		row[1] = inverse * row[1];
		row[2] = inverse * row[2];
	}
	const glm::vec3 eye {point.position.x * k_PathScale, point.position.y * k_PathScale, point.position.z * k_PathScale};

	glm::mat4 view {1.0f};
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			view[i][j] = rows.at(i).at(j);
		}
	}
	for (int j = 0; j < 3; ++j)
	{
		const float first = -(rows[0].at(j) * eye.x);
		const float second = rows[1].at(j) * eye.y;
		const float third = rows[2].at(j) * eye.z;
		const float firstTwo = first - second;
		view[3][j] = firstTwo - third;
	}
	return view;
}

glm::mat4 film_creature::Projection(const glm::mat4& worldProjection)
{
	// The lens's width across sets the scale across; the 4:3 screen's shape sets the scale up and down
	const double tangent = std::tan(static_cast<double>(k_FieldOfView) * 0.5);
	const auto across = static_cast<float>(1.0 / tangent);
	auto projection = worldProjection;
	projection[0][0] = across;
	projection[1][1] = static_cast<float>(static_cast<double>(k_ScreenAspect) * static_cast<double>(across));
	return projection;
}
