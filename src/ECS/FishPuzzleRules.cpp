/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FishPuzzleRules.h"

#include "Common/PortableMath.h"

namespace openblack::ecs::fish_puzzle
{

namespace
{
/// The seconds of a step of the game time, as the game gets them from its milliseconds
constexpr float k_SecondsPerMillisecond = 0.001f;
/// The shoals take their steps in this order: the west shoal, which the puzzle made last, first
constexpr std::array<size_t, 2> k_StepOrder {1, 0};
} // namespace

FishNet Make(glm::vec3 middle, const fish_shoal::Random& random)
{
	FishNet puzzle {.net = {.middle = middle}};
	puzzle.shoals.at(0) = fish_shoal::MakeShoal(middle + glm::vec3(k_ShoalOffset, 0.0f, k_ShoalOffset), random);
	puzzle.shoals.at(1) = fish_shoal::MakeShoal(middle + glm::vec3(-k_ShoalOffset, 0.0f, k_ShoalOffset), random);
	return puzzle;
}

uint32_t FishInNet(const fish_shoal::Shoal& shoal, const Net& net)
{
	uint32_t count = 0;
	const auto shown = fish_shoal::ShownCount(shoal.fullness);
	for (uint32_t i = 0; i < shown; ++i)
	{
		const auto& position = shoal.fish.at(i).position;
		const float x = position.x - net.middle.x;
		const float z = position.z - net.middle.z;
		if (x * x + z * z < k_NetRadius * k_NetRadius)
		{
			++count;
		}
	}
	return count;
}

void StepNet(Net& net, float seconds)
{
	if (net.caught && net.closing != 0.0f)
	{
		net.closing -= seconds + seconds;
		if (net.closing < 0.0f)
		{
			net.closing = 0.0f;
		}
	}
	net.bobTime += seconds * k_TimePerStep;
}

std::vector<glm::vec3> StepTurn(FishNet& puzzle, uint32_t milliseconds, std::optional<glm::vec3> scare,
                                const fish_shoal::Random& random)
{
	const float seconds = static_cast<float>(milliseconds) * k_SecondsPerMillisecond;
	auto& net = puzzle.net;
	uint32_t count = 0;
	std::vector<glm::vec3> caught;
	for (const auto index : k_StepOrder)
	{
		auto& shoal = puzzle.shoals.at(index);
		// A shoal whose fish were caught does not swim
		if (!net.caught)
		{
			fish_shoal::Step(shoal, seconds, scare, random);
			count += FishInNet(shoal, net);
		}
		StepNet(net, seconds);
		if (count >= k_FishToCatch && !net.caught)
		{
			net.heldMilliseconds += milliseconds;
			if (net.heldMilliseconds >= k_HoldMilliseconds)
			{
				net.caught = true;
				// Each fish of both shoals makes a ring on the water, also those that do not show
				for (const auto& each : puzzle.shoals)
				{
					for (const auto& fish : each.fish)
					{
						caught.push_back(fish.position);
					}
				}
			}
		}
	}
	net.fishInRing = count;
	if (count < k_FishToCatch)
	{
		net.heldMilliseconds = 0;
	}
	return caught;
}

std::array<glm::vec3, k_FloatCount> FloatPoints(const Net& net)
{
	const float radius = net.closing * k_ClosingReach + k_ClosedRadius;
	std::array<glm::vec3, k_FloatCount> points {};
	for (size_t i = 0; i < points.size(); ++i)
	{
		const float angle = static_cast<float>(i) * k_FloatAngleStep;
		const float bob = portable_math::Cos(static_cast<float>(i * i) + net.bobTime) * k_BobHeight;
		points.at(i) = {portable_math::Cos(angle) * radius + net.middle.x, net.middle.y + bob,
		                portable_math::Sin(angle) * radius + net.middle.z};
	}
	return points;
}

} // namespace openblack::ecs::fish_puzzle
