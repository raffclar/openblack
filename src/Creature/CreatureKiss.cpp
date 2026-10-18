/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureKiss.h"

#include <glm/geometric.hpp>

using namespace openblack;

glm::vec3 creature_kiss::LipsAt(const KissPoints& points, float highWeight)
{
	return points.low + ((points.high - points.low) * highWeight);
}

std::optional<float> creature_kiss::HighWeight(const KissPoints& self, const KissPoints& other)
{
	const bool selfTaller = self.high.y > other.high.y;
	const auto& taller = selfTaller ? self : other;
	const float shorterHigh = selfTaller ? other.high.y : self.high.y;
	if (shorterHigh < taller.low.y)
	{
		return std::nullopt;
	}
	if (selfTaller)
	{
		return (other.high.y - self.low.y) / (self.high.y - self.low.y);
	}
	return 1.0f;
}

float creature_kiss::KissingDistance(const KissPoints& self, const KissPoints& other)
{
	const auto weight = HighWeight(self, other);
	return weight.has_value() ? -LipsAt(self, *weight).z : 0.0f;
}

float creature_kiss::MeanKissingDistance(const KissPoints& self, const KissPoints& other)
{
	return (KissingDistance(self, other) + KissingDistance(other, self)) * 0.5f;
}

std::pair<glm::vec2, glm::vec2> creature_kiss::KissPlaces(glm::vec2 self, glm::vec2 other, float distance)
{
	const auto middle = (self + other) * 0.5f;
	auto offset = middle - self;
	const float length = glm::length(offset);
	// Two creatures on the same point have no side, and both stand on the point
	offset = length > 0.0f ? offset * (distance / length) : glm::vec2(0.0f);
	return {middle - offset, middle + offset};
}
