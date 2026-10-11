/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerDrawRules.h"

#include <cmath>

#include <glm/common.hpp>
#include <glm/gtx/euler_angles.hpp>

using namespace openblack;
using namespace openblack::ecs;

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;
constexpr float k_TwoPi = 2.0f * k_Pi;
constexpr float k_QuarterTurn = k_Pi / 2.0f;
/// The ordinary drawn turn, in radians a millisecond
constexpr double k_TurnPerMillisecond = 0.003;
/// A high-detail body's further turn, in radians a second
constexpr float k_DetailedTurnPerSecond = 3.927f;
/// Models face a quarter turn off the game's headings
constexpr float k_ModelOffset = k_QuarterTurn;
/// How far from flat a turn round the vertical may be and still give a heading
constexpr float k_UprightTolerance = 1e-4f;
} // namespace

glm::vec3 villager_draw::DrawnPosition(const glm::vec3& turnStart, const glm::vec3& now, float turnFraction, bool glides)
{
	if (!glides)
	{
		return now;
	}
	const float t = glm::clamp(turnFraction, 0.0f, 1.0f);
	return (turnStart * (1.0f - t)) + (now * t);
}

float villager_draw::Wrapped(float angle)
{
	if (angle > k_Pi)
	{
		return angle - k_TwoPi;
	}
	if (angle < -k_Pi)
	{
		return angle + k_TwoPi;
	}
	return angle;
}

float villager_draw::EasedHeading(float drawn, float facing, uint32_t gameMilliseconds)
{
	const float difference = Wrapped(Wrapped(facing) - drawn);
	const float size = std::abs(difference);
	// A turn of more than a quarter is made quicker the bigger it is
	const double perMillisecond = size > k_QuarterTurn
	                                  ? (static_cast<double>(size) * (2.0 / std::numbers::pi) * 2.0) * k_TurnPerMillisecond
	                                  : k_TurnPerMillisecond;
	const auto step = static_cast<float>(static_cast<double>(gameMilliseconds) * perMillisecond);
	if (size < step)
	{
		return facing;
	}
	return Wrapped(difference > 0.0f ? drawn + step : drawn - step);
}

float villager_draw::EasedDetailedHeading(float drawn, float heading, uint32_t gameMilliseconds, bool turnAtOnce)
{
	const float target = Wrapped(heading);
	const float current = Wrapped(drawn);
	if (current == target || turnAtOnce)
	{
		return target;
	}
	const float difference = Wrapped(target - current);
	const float step = static_cast<float>(gameMilliseconds) * 0.001f * k_DetailedTurnPerSecond;
	if (std::abs(difference) <= step)
	{
		return target;
	}
	return difference > 0.0f ? current + step : current - step;
}

villager_draw::DrawnHeadings villager_draw::StepHeadings(std::optional<float> eased, std::optional<float> detailed,
                                                         float facing, uint32_t gameMilliseconds, bool highDetail,
                                                         bool turnAtOnce)
{
	const float turned = EasedHeading(eased.value_or(facing), facing, gameMilliseconds);
	if (!highDetail)
	{
		return {.eased = turned, .detailed = std::nullopt, .drawn = turned};
	}
	const float body = EasedDetailedHeading(detailed.value_or(turned), turned, gameMilliseconds, turnAtOnce);
	return {.eased = turned, .detailed = body, .drawn = body};
}

std::optional<float> villager_draw::HeadingOf(const glm::mat3& rotation)
{
	if (std::abs(rotation[1][1] - 1.0f) > k_UprightTolerance)
	{
		return std::nullopt;
	}
	// The model is turned by the heading negated, less a quarter turn
	const float turn = std::atan2(rotation[2][0], rotation[0][0]);
	return Wrapped(-turn - k_ModelOffset);
}

glm::mat3 villager_draw::RotationOf(float heading)
{
	return glm::mat3(glm::eulerAngleY(-heading - k_ModelOffset));
}

void villager_draw::StepClipBlend(ClipBlendTrack& track, AnimId clip, uint32_t place, uint32_t gameMilliseconds)
{
	if (track.lastClip == AnimId::Invalid)
	{
		track.lastClip = clip;
	}
	else if (track.lastClip == clip)
	{
		if (track.blend.remaining != 0)
		{
			const int32_t left = track.blend.remaining - static_cast<int32_t>(gameMilliseconds);
			if (left < 0)
			{
				track.blend.remaining = 0;
			}
			else
			{
				track.blend.remaining = left;
				track.blend.weight = static_cast<float>(left) / static_cast<float>(k_ClipBlendMilliseconds);
			}
		}
	}
	else
	{
		track.blend = {
		    .from = track.lastClip, .fromPlace = track.lastPlace, .remaining = k_ClipBlendMilliseconds, .weight = 1.0f};
		track.lastClip = clip;
	}
	track.lastPlace = place;
}

void villager_draw::BlendPoses(std::span<glm::mat4> bones, std::span<const glm::mat4> old, float oldWeight)
{
	const float newWeight = 1.0f - oldWeight;
	for (size_t i = 0; i < bones.size() && i < old.size(); ++i)
	{
		bones[i] = (bones[i] * newWeight) + (old[i] * oldWeight);
	}
}
