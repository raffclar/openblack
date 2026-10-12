/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFeedback.h"

#include <cmath>

#include <algorithm>
#include <limits>
#include <utility>

#include <glm/geometric.hpp>

using namespace openblack;
using namespace openblack::creature_feedback;

BodyPart creature_feedback::NearestPart(const glm::vec3& touch, std::span<const glm::vec3, k_BodyPartCount> parts)
{
	size_t best = 0;
	float bestDistance = std::numeric_limits<float>::max();
	for (size_t i = 0; i < parts.size(); ++i)
	{
		const auto distance = glm::distance(touch, parts[i]);
		if (distance < bestDistance)
		{
			best = i;
			bestDistance = distance;
		}
	}
	return static_cast<BodyPart>(best);
}

std::optional<float> creature_feedback::Pace(float& sinceStep, float frameMs)
{
	sinceStep += frameMs;
	if (sinceStep < 1000.0f / k_StepsPerSecond)
	{
		return std::nullopt;
	}
	return std::exchange(sinceStep, 0.0f);
}

float creature_feedback::StrokeWaitMs(float sum)
{
	return (1.0f - sum) * k_StrokeWaitMs;
}

bool creature_feedback::StrokeDue(const StrokeClocks& clocks, std::optional<BodyPart> last, BodyPart part, float sum)
{
	// The game compares the wait in whole milliseconds
	return last != part && std::trunc(StrokeWaitMs(sum)) <= clocks.sinceTouch && clocks.ready >= k_ReadyForStrokeMs;
}

bool creature_feedback::SlapConsidered(bool onBody, bool wasOnBody, float cursorX, float lastCursorX, float screenMiddle)
{
	const bool crossesMiddle = (cursorX - screenMiddle) * (lastCursorX - screenMiddle) < 0.0f;
	return (onBody && !wasOnBody) || crossesMiddle;
}

std::optional<Slap> creature_feedback::ClassifySlap(float handHeight, float speed, float creatureHeight, bool sweepsLeft)
{
	if (creatureHeight <= 0.0f || speed <= k_SlapSpeed * creatureHeight)
	{
		return std::nullopt;
	}
	const auto height = handHeight / creatureHeight;
	if (height <= 0.0f || height >= k_SlapAbove)
	{
		return std::nullopt;
	}
	const auto band = height < k_FeetBelow ? SlapBand::Feet : height < k_WaistBelow ? SlapBand::Waist : SlapBand::Head;
	auto animation = band == SlapBand::Feet ? k_SlapFeet : band == SlapBand::Waist ? k_SlapWaist : k_SlapHead;
	const bool gentle = speed < k_HardSlapSpeed * creatureHeight;
	if (gentle)
	{
		animation += k_GentleOffset;
	}
	return Slap {.animation = animation, .band = band, .gentle = gentle, .mirrored = sweepsLeft};
}

bool creature_feedback::CountSlap(HardSlaps& slaps, const Slap& slap, bool onBody)
{
	if (slap.band != slaps.band)
	{
		slaps = {.band = slap.band, .count = 0};
	}
	if (slap.gentle)
	{
		return false;
	}
	++slaps.count;
	if (!onBody || slaps.count < k_HardSlapsToBruise)
	{
		return false;
	}
	slaps.count = 0;
	return true;
}

uint8_t creature_feedback::BruiseColumn(float speed, float creatureHeight)
{
	const auto hard = k_HardSlapSpeed * creatureHeight;
	if (hard <= 0.0f)
	{
		return 7;
	}
	const auto past = std::clamp(static_cast<int>((speed - hard) * 5.0f / hard), 0, 7);
	return static_cast<uint8_t>(7 - past);
}

float creature_feedback::LifeAfterBruise(float life)
{
	return life > k_BruiseLife ? std::max(life - k_BruiseLife, k_BruiseLife) : life;
}

TouchCycle creature_feedback::NextCycle(TouchCycle cycle, bool fast, bool stroking, bool onBody)
{
	if (fast)
	{
		cycle = TouchCycle::Slap;
	}
	if (stroking)
	{
		cycle = TouchCycle::Tickle;
	}
	if (!onBody && cycle == TouchCycle::Tickle)
	{
		cycle = TouchCycle::Wiggle;
	}
	return cycle;
}

float creature_feedback::AfterStroke(float sum)
{
	return std::min(sum + k_StrokeAmount, 1.0f);
}

float creature_feedback::AfterSlap(float sum, bool gentle)
{
	auto amount = gentle ? k_GentleSlapAmount : k_HardSlapAmount;
	if (sum > k_EnjoyingSum)
	{
		amount *= 2.0f;
	}
	return std::max(sum - amount, -1.0f);
}

float creature_feedback::Delivered(float sum)
{
	return std::clamp(sum, -1.0f, 1.0f);
}

float creature_feedback::AttitudeAfter(float attitude, float feedback)
{
	return (0.7f * attitude) + (0.05f * feedback);
}

float creature_feedback::AverageAfter(float average, float feedback)
{
	return (0.6f * feedback) + (0.4f * average);
}

std::optional<float> creature_feedback::RayHit(const glm::vec3& origin, const glm::vec3& direction,
                                               std::span<const Capsule> capsules)
{
	const auto length = glm::length(direction);
	if (length <= 0.0f)
	{
		return std::nullopt;
	}
	const auto ray = direction / length;
	std::optional<float> nearest;
	for (const auto& capsule : capsules)
	{
		// The point of the capsule's line nearest the line of sight
		const auto axis = capsule.to - capsule.from;
		const auto axisSquared = glm::dot(axis, axis);
		float onAxis = 0.0f;
		if (axisSquared > 0.0f)
		{
			const auto w = origin - capsule.from;
			const auto b = glm::dot(ray, axis);
			const auto denominator = axisSquared - (b * b);
			if (denominator > 1e-6f * axisSquared)
			{
				onAxis = std::clamp((glm::dot(axis, w) - (b * glm::dot(ray, w))) / denominator, 0.0f, 1.0f);
			}
		}
		const auto centre = capsule.from + (axis * onAxis);
		// Where the line of sight enters the sphere round that point
		const auto toCentre = centre - origin;
		const auto middle = glm::dot(toCentre, ray);
		const auto missSquared = glm::dot(toCentre, toCentre) - (middle * middle);
		const auto radiusSquared = capsule.radius * capsule.radius;
		if (missSquared > radiusSquared)
		{
			continue;
		}
		const auto entry = middle - std::sqrt(radiusSquared - missSquared);
		if (entry >= 0.0f && (!nearest || entry < *nearest))
		{
			nearest = entry;
		}
	}
	if (nearest)
	{
		*nearest /= length;
	}
	return nearest;
}

std::vector<Capsule> creature_feedback::BodyCapsules(std::span<const uint32_t> parents, std::span<const glm::mat4> boneMatrices,
                                                     const glm::mat4& placement, float radius)
{
	// Where each joint is in the world: only the placement of the bone's origin is needed, once for each bone
	std::vector<glm::vec3> joints(boneMatrices.size());
	std::ranges::transform(boneMatrices, joints.begin(),
	                       [&placement](const glm::mat4& bone) { return glm::vec3(placement * bone[3]); });
	std::vector<Capsule> capsules;
	capsules.reserve(std::min(parents.size(), boneMatrices.size()));
	for (uint32_t bone = 0; bone < parents.size() && bone < boneMatrices.size(); ++bone)
	{
		const auto parent = parents[bone];
		const auto to = joints[bone];
		const auto from = parent < boneMatrices.size() ? joints[parent] : to;
		capsules.push_back({.from = from, .to = to, .radius = radius});
	}
	return capsules;
}

float creature_feedback::DistanceOutside(const glm::vec3& point, std::span<const Capsule> capsules)
{
	float best = std::numeric_limits<float>::max();
	for (const auto& capsule : capsules)
	{
		const auto axis = capsule.to - capsule.from;
		const auto length = glm::dot(axis, axis);
		const auto t = length > 0.0f ? std::clamp(glm::dot(point - capsule.from, axis) / length, 0.0f, 1.0f) : 0.0f;
		best = std::min(best, glm::distance(point, capsule.from + (axis * t)) - capsule.radius);
	}
	return best;
}
