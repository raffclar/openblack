/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "IconChargeLooks.h"

#include <cmath>

#include "Common/X87.h"

namespace openblack::magic::icon_charge_looks
{

namespace
{
constexpr float k_SecondsPerMillisecond = 0.001f;
/// The seconds of one round of the clock
constexpr float k_ClockRound = 3.33f;
/// The ring's points are this far round from each other, as a share of a turn
constexpr float k_Turn = 6.2831855f;
constexpr float k_TurnShare = 0.11111111f;
/// While the icon charges, the ring starts this far up and fills the rest of the way
constexpr float k_ChargeStart = 0.2f;
/// The throb goes round two times in each round of the clock
constexpr float k_ThrobSpeed = 12.566371f;
constexpr float k_ThrobDark = 0.1f;
constexpr float k_ThrobBright = 0.6f;
constexpr float k_Full = 255.0f;
/// The flash at the start of a charge: its speed in radians a second, how long it lasts, its most light over the light
/// that stays, and the light that stays, in 256ths
constexpr float k_FlashSpeed = 8.0f;
constexpr float k_FlashSeconds = 1.5f;
constexpr float k_FlashLight = 205.0f;
constexpr uint32_t k_ChargeLight = 50;
constexpr int32_t k_MillisecondsPerTurn = 100;
/// The pulse model is this many times as large as the icon at the start of a pulse, and none at the end
constexpr float k_PulseLargest = 3.0f;
constexpr float k_PulseSmallest = 0.0f;
/// The pulse model goes up the icon by this share of the icon's height at the end of a pulse, and this much more at the
/// start (none)
constexpr float k_PulseRise = 0.1f;
constexpr float k_PulseRiseAtStart = 0.0f;
/// The pulse model's alpha at the start and the end of a pulse
constexpr float k_PulseAlphaAtStart = 0.0f;
constexpr float k_PulseAlphaAtEnd = 255.0f;
constexpr uint32_t k_ByteMask = 0xFFu;

/// The light of a throb at a phase of the clock, 0 to 255
[[nodiscard]] uint32_t ThrobLight(float clockPhase)
{
	const float wave = x87::Add(x87::Sin(clockPhase * k_ThrobSpeed), 1.0f) * 0.5f;
	const float light = ((wave * (k_ThrobBright - k_ThrobDark)) + k_ThrobDark) * k_Full;
	return static_cast<uint32_t>(std::lrint(light)) & k_ByteMask;
}

/// The light of a charge at so many seconds after it started, 0 to 255
[[nodiscard]] uint32_t ChargeLight(float seconds)
{
	if (!(seconds < k_FlashSeconds))
	{
		return k_ChargeLight;
	}
	const float flash = x87::Sub(1.0f, x87::Cos(k_FlashSpeed * seconds)) * 0.5f;
	float light = (1.0f - (seconds / k_FlashSeconds)) * flash;
	if (light <= 0.0f)
	{
		light = 0.0f;
	}
	else if (!(light < 1.0f))
	{
		light = 1.0f;
	}
	return ((static_cast<uint32_t>(std::lrint(light * k_FlashLight)) & k_ByteMask) + k_ChargeLight) & k_ByteMask;
}

/// Each channel of a colour times a light in 256ths
[[nodiscard]] uint32_t Lit(uint32_t rgb, uint32_t light)
{
	const uint32_t red = ((rgb & 0xFF0000u) * light) & 0xFF0000FFu;
	const uint32_t green = ((rgb & 0xFF00u) * light) & 0xFF0000u;
	const uint32_t blue = ((rgb & 0xFFu) * light) & 0xFF00u;
	return ((red | green | blue) >> 8u) & 0xFFFFFFu;
}
} // namespace

DecayClock Advance(DecayClock clock, int32_t frameMilliseconds)
{
	float phase = ((static_cast<float>(frameMilliseconds) * k_SecondsPerMillisecond) / k_ClockRound) + clock.phase;
	while (phase > 1.0f)
	{
		phase -= 1.0f;
		++clock.wraps;
	}
	clock.phase = phase;
	return clock;
}

float SecondsSince(float phase, float mark)
{
	const float since = phase <= mark ? phase + 1.0f : phase;
	return (since - mark) * k_ClockRound;
}

std::vector<glm::vec3> RingPoints(const glm::mat4& iconModel)
{
	const glm::vec3 across {iconModel[0]};
	const glm::vec3 up {iconModel[1]};
	const glm::vec3 along {iconModel[2]};
	const glm::vec3 foot {iconModel[3]};
	std::vector<glm::vec3> points;
	points.reserve(k_RingPoints);
	for (size_t i = 0; i < k_RingPoints; ++i)
	{
		const float angle = static_cast<float>(i) * k_Turn * k_TurnShare;
		// The game keeps the cosine as a float and the sine wide until its product with the radius
		const float cosine = x87::RoundToFloat(x87::Cos(angle)) * k_RingRadius;
		const float sine = x87::Mul(x87::Sin(angle), k_RingRadius);
		const auto axis = [&](int c) { return (((sine * along[c]) + (0.0f * up[c])) + (cosine * across[c])) + foot[c]; };
		points.emplace_back(axis(0), axis(1), axis(2));
	}
	return points;
}

Shown ShownCharge(const IconState& icon)
{
	Shown shown;
	if (icon.seedHeld)
	{
		shown = {.fraction = 1.0f, .throbs = true};
	}
	else if (icon.store != 0.0f)
	{
		shown.fraction = icon.store / icon.required;
		shown.throbs = !((icon.required - icon.store) > 0.0f);
	}
	if (icon.charging)
	{
		shown.fraction = (k_ChargeStart + shown.fraction) / (k_ChargeStart + 1.0f);
	}
	return shown;
}

float ChargeSeconds(uint32_t turn, uint32_t startTurn, float turnFraction)
{
	const auto turns = static_cast<float>(static_cast<int32_t>(turn - startTurn));
	return ((turns + turnFraction) * static_cast<float>(k_MillisecondsPerTurn)) * k_SecondsPerMillisecond;
}

uint32_t Tint(uint32_t playerRgb, const Shown& shown, bool charging, float clockPhase, float chargeSeconds)
{
	if (shown.throbs)
	{
		return Lit(playerRgb, ThrobLight(clockPhase));
	}
	if (charging)
	{
		return Lit(playerRgb, ChargeLight(chargeSeconds));
	}
	return 0;
}

std::optional<float> PulseShare(float clockPhase)
{
	const float share = SecondsSince(clockPhase, k_PulseMark) / 1.0f;
	if (share < 0.0f || share > 1.0f)
	{
		return std::nullopt;
	}
	return share;
}

glm::mat4 PulseModel(const glm::mat4& iconModel, float iconModelHeight, float share)
{
	const float left = 1.0f - share;
	const float scale = (((1.0f - k_PulseSmallest) * left) + k_PulseSmallest) * k_PulseLargest;
	const float rise = ((left * k_PulseRiseAtStart) + k_PulseRise) * iconModelHeight;
	glm::mat4 model = iconModel;
	for (int c = 0; c < 3; ++c)
	{
		model[3][c] = (rise * iconModel[1][c]) + iconModel[3][c];
	}
	for (int axis = 0; axis < 3; ++axis)
	{
		for (int c = 0; c < 3; ++c)
		{
			model[axis][c] = scale * iconModel[axis][c];
		}
	}
	return model;
}

uint8_t PulseAlpha(float share)
{
	const float left = 1.0f - share;
	const float alpha = ((k_PulseAlphaAtStart - k_PulseAlphaAtEnd) * left) + k_PulseAlphaAtEnd;
	return static_cast<uint8_t>(static_cast<int32_t>(alpha) & static_cast<int32_t>(k_ByteMask));
}

} // namespace openblack::magic::icon_charge_looks
