/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Moon.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>

using namespace openblack::graphics;

namespace
{
constexpr float k_BasisScale = 4.0f;
/// The moon leans back a little
constexpr float k_Tilt = -0.13089970f;
constexpr float k_MeshScale = 0.65f;
constexpr float k_GlowHalfSize = 500.0f;
constexpr float k_GlowUvMinimum = 0.25f;
constexpr float k_GlowUvMaximum = 0.49375f;
constexpr double k_FullTurn = 6.2831854820251465;
constexpr float k_HalfTurn = 3.14159274f;
constexpr float k_OverHalfTurn = 0.318309873f;
/// Days either side of a date searched for a point of the moon month
constexpr int64_t k_HalfMonthDays = 15;

/// Turns two of a matrix's axes into each other by an angle, as the game's matrices turn their rows
void TurnAxes(glm::mat3& m, int i, int j, float c, float s)
{
	const auto a = m[i];
	const auto b = m[j];
	m[i] = (c * a) + (s * b);
	m[j] = (c * b) - (s * a);
}
} // namespace

glm::vec3 moon::Offset(float scriptHour)
{
	// An hour of the day is a twelfth of half a turn
	const float angle = scriptHour * 0.2617993950843811f;
	return {4000.0f, (1100.0f * std::cos(angle)) - 150.0f, 800.0f * std::sin(angle)};
}

std::optional<moon::Placement> moon::Place(float scriptHour)
{
	const auto offset = Offset(scriptHour);
	const float alpha = std::min(200.0f, std::floor((0.5f * offset.y) - 110.0f));
	if (alpha <= 0.0f)
	{
		return std::nullopt;
	}
	return Placement {.offset = offset, .alpha = alpha};
}

double moon::MonthFraction(int64_t unixTime)
{
	// Whole days from a new moon, in moon months of about 29.5 days, kept at full precision until the phase is made
	const auto days = static_cast<int32_t>((unixTime / k_SecondsPerDay) - k_NewMoonDay);
	const double months = static_cast<double>(days) * k_MonthsPerDay;
	return months - static_cast<double>(static_cast<int32_t>(months));
}

float moon::Phase(int64_t unixTime)
{
	return static_cast<float>((1.0 - MonthFraction(unixTime)) * k_FullTurn);
}

float moon::ScriptPercentage(float phase)
{
	if (phase >= k_HalfTurn)
	{
		return (phase - k_HalfTurn) * k_OverHalfTurn;
	}
	return 1.0f - (phase * k_OverHalfTurn);
}

int64_t moon::DateAtFraction(int64_t unixTime, double fraction)
{
	const int64_t today = unixTime / k_SecondsPerDay;
	const auto distance = [fraction](int64_t day) {
		const double apart = std::abs(MonthFraction(day * k_SecondsPerDay) - fraction);
		const double wrapped = apart - std::floor(apart);
		return std::min(wrapped, 1.0 - wrapped);
	};
	int64_t best = today;
	for (int64_t day = today - k_HalfMonthDays; day <= today + k_HalfMonthDays; ++day)
	{
		if (distance(day) < distance(best))
		{
			best = day;
		}
	}
	return (best * k_SecondsPerDay) + (k_SecondsPerDay / 2);
}

moon::SkyAngles moon::Angles(const glm::vec3& offset)
{
	const float across = std::hypot(offset.x, offset.z);
	return {
	    .azimuth = glm::degrees(std::atan2(offset.x, offset.z)),
	    .elevation = glm::degrees(std::atan2(offset.y, across)),
	};
}

glm::mat3 moon::Basis(const glm::mat4& view, const glm::mat4& inverseView, const glm::vec3& position)
{
	// Its face square to the line from the camera, and upright
	const glm::vec3 inView(view * glm::vec4(position, 1.0f));
	const glm::vec3 facing = inView != glm::vec3(0.0f) ? glm::normalize(inView) : inView;
	glm::vec3 across {facing.z, 0.0f, -facing.x};
	if (across != glm::vec3(0.0f))
	{
		across = glm::normalize(across);
	}
	const glm::vec3 up = glm::cross(facing, across);
	return glm::mat3(inverseView) * glm::mat3(across, up, facing) * k_BasisScale;
}

glm::mat4 moon::Model(const glm::mat3& basis, const glm::vec3& position, float phase)
{
	glm::mat3 axes = basis;
	TurnAxes(axes, 0, 1, std::cos(k_Tilt), -std::sin(k_Tilt));
	const float turn = phase + glm::pi<float>();
	TurnAxes(axes, 0, 2, std::cos(turn), std::sin(turn));
	glm::mat4 model(axes * k_MeshScale);
	model[3] = glm::vec4(position, 1.0f);
	return model;
}

moon::Glow moon::MakeGlow(const glm::mat3& basis, const glm::vec3& position)
{
	const auto across = basis[0] * k_GlowHalfSize;
	const auto up = basis[1] * k_GlowHalfSize;
	return {
	    .corners = {position - up - across, position + across - up, position + up - across, position + up + across},
	    .uvs = {glm::vec2(k_GlowUvMinimum, k_GlowUvMinimum), glm::vec2(k_GlowUvMaximum, k_GlowUvMinimum),
	            glm::vec2(k_GlowUvMinimum, k_GlowUvMaximum), glm::vec2(k_GlowUvMaximum, k_GlowUvMaximum)},
	};
}

moon::Glow moon::SeaGlow(const glm::mat4& view, const glm::mat4& inverseView, const glm::vec3& position)
{
	const glm::vec3 inTheSea {position.x, -position.y, position.z};
	auto glow = MakeGlow(Basis(view, inverseView, inTheSea), inTheSea);
	for (auto& corner : glow.corners)
	{
		corner.y = -corner.y;
	}
	return glow;
}

glm::vec3 moon::GlowColour(const glm::vec3& moonColour)
{
	return {moonColour.r / 6.0f, moonColour.g / 5.0f, moonColour.b / 4.0f};
}
