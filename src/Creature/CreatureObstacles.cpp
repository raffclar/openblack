/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureObstacles.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/geometric.hpp>

using namespace openblack;
using namespace openblack::creature_obstacles;
using creature_route::Circle;

namespace
{
/// Things at least this share of the creature's height are walked round
constexpr float k_LowShare = 0.1f;
constexpr float k_TreeShare = 1.5f;

/// The share of the creature's radius taken off what it keeps clear: four tenths for things taller than four fifths of
/// it, rising to seven tenths for the lowest
constexpr float k_TallLimit = 0.8f;
constexpr float k_TallShare = 0.4f;
constexpr float k_LowestShare = 0.7f;
constexpr float k_ShareRange = 0.3f;

/// A box this much longer than wide is a row of circles, at most five of them, each this much wider than half the row
constexpr float k_LongBox = 1.2f;
constexpr int32_t k_MostInRow = 5;
constexpr double k_RowWidening = 1.15;

/// A trunk is a tenth of the tree's width, a quarter of a unit at most
constexpr float k_TrunkShare = 0.1f;
constexpr float k_MostTrunk = 0.25f;
/// Animated scenery is seven tenths of its model's half diagonal
constexpr float k_SceneryShare = 0.7f;
/// Another creature is this much wider than the two of them together
constexpr float k_CreatureWidening = 1.15f;
/// The creature walks up to a third of the temple's width
constexpr float k_TempleReach = 0.33f;

// The temple's circles, all in proportion to one size; worked out to more than float precision, as the game does
constexpr double k_TempleSize = 21.5;
constexpr double k_TempleMiddle = static_cast<double>(0.64f);
constexpr double k_TempleMiddleExtra = static_cast<double>(1.1f);
constexpr double k_SpokeCircle = static_cast<double>(0.11f);
constexpr double k_SpokeStart = static_cast<double>(0.1f);
constexpr double k_SpokeStep = static_cast<double>(0.2f);
constexpr double k_BendStep = static_cast<double>(0.1f);
constexpr double k_SecondBendStep = static_cast<double>(0.07f);
constexpr float k_FirstSpoke = 3.83f;
constexpr float k_SpokeTurn = 0.8975979f;
constexpr std::array<float, 3> k_Bends {0.06f, 0.16f, 0.27f};
/// The other spokes have two circles for a player more evil than this, three for one more evil than the next, else four
constexpr float k_EvilTemple = -0.7f;
constexpr float k_MiddlingTemple = -0.3f;
constexpr float k_SiteTurn = 0.5f;
constexpr float k_SiteDistance = 24.0f;
constexpr float k_SiteRadius = 12.0f;

float Clamped(float radius)
{
	return std::max(radius, k_MinRadius);
}

float FireMargin(const Thing& thing)
{
	return thing.hasFire ? k_FireMargin : 0.0f;
}

glm::vec2 Turned(glm::vec2 v, float angle)
{
	const auto c = std::cos(angle);
	const auto s = std::sin(angle);
	return {(v.x * c) - (v.y * s), (v.x * s) + (v.y * c)};
}

/// The outline of a model's box: one circle at its middle, or a row along a long box
std::vector<Circle> BoxCircles(const Thing& thing, const Walker& walker)
{
	const auto margin = walker.radius - (RadiusShare(thing.height, walker.height) * walker.radius) + FireMargin(thing);
	const auto centre = thing.position + Turned(glm::vec2(thing.boxCentre.x, thing.boxCentre.z) * thing.scale, thing.yAngle);
	auto sizeX = (thing.boxHalfSize.x * thing.scale) + margin;
	auto sizeZ = (thing.boxHalfSize.z * thing.scale) + margin;

	int32_t count = 1;
	bool alongX = true;
	float spacing = 0.0f;
	float width = 0.0f;
	if (sizeX > sizeZ * k_LongBox)
	{
		count = static_cast<int32_t>(sizeX / sizeZ) + 1;
		if (count > k_MostInRow)
		{
			count = k_MostInRow;
			sizeZ = sizeX / static_cast<float>(k_MostInRow);
		}
		spacing = sizeX / static_cast<float>(count);
		width = sizeZ;
	}
	else if (sizeX * k_LongBox < sizeZ)
	{
		count = static_cast<int32_t>(sizeZ / sizeX) + 1;
		if (count > k_MostInRow)
		{
			count = k_MostInRow;
			sizeX = sizeZ / static_cast<float>(k_MostInRow);
		}
		alongX = false;
		spacing = sizeZ / static_cast<float>(count);
		width = sizeX;
	}
	if (count == 1)
	{
		return {{.centre = centre, .radius = Clamped(thing.width + margin)}};
	}

	// Along the long side, the circles' middles evenly spaced over its length
	const auto angle = thing.yAngle + (alongX ? std::numbers::pi_v<float> / 2.0f : 0.0f);
	const auto step = glm::vec2(std::sin(angle) * spacing, -std::cos(angle) * spacing);
	auto at = centre - (step * static_cast<float>(count - 1));
	const auto radius = Clamped(static_cast<float>(static_cast<double>(width) * k_RowWidening));
	std::vector<Circle> circles;
	circles.reserve(static_cast<size_t>(count));
	for (int32_t i = 0; i < count; ++i)
	{
		circles.push_back({.centre = at, .radius = radius});
		at += step * 2.0f;
	}
	return circles;
}
} // namespace

float creature_obstacles::ModelHeight(glm::vec3 boxHalfSize, float scale)
{
	return boxHalfSize.y * scale * 2.0f;
}

float creature_obstacles::ModelWidth(glm::vec3 boxHalfSize, float scale)
{
	return std::max(boxHalfSize.x, boxHalfSize.z) * scale;
}

bool creature_obstacles::MustAvoid(const Thing& thing, const Walker& walker)
{
	const auto tallEnough = [&](float share) { return thing.height >= walker.height * share; };
	switch (thing.avoidance)
	{
	case Avoidance::Never:
		return false;
	case Avoidance::Always:
		return true;
	case Avoidance::Building:
		return (!thing.justBegun && tallEnough(k_LowShare)) || thing.burning;
	case Avoidance::Object:
		return tallEnough(k_LowShare) || thing.burning;
	case Avoidance::Tree:
		return tallEnough(k_TreeShare) || thing.burning;
	case Avoidance::Forest:
	case Avoidance::Burning:
		return thing.burning;
	case Avoidance::BurningUnlessScripted:
		return !walker.scriptControlled && thing.burning;
	case Avoidance::Creature:
		return !thing.walking;
	}
	return false;
}

float creature_obstacles::RadiusShare(float height, float walkerHeight)
{
	const auto limit = walkerHeight * k_TallLimit;
	if (height > limit)
	{
		return k_TallShare;
	}
	return k_LowestShare - ((height / limit) * k_ShareRange);
}

float creature_obstacles::ReachRadius(const Thing& thing, const Walker& walker)
{
	switch (thing.shape)
	{
	case Shape::Trunk:
		return std::min(thing.width * k_TrunkShare, k_MostTrunk);
	case Shape::Temple:
		return thing.width * k_TempleReach;
	default:
		return thing.width - (walker.radius * RadiusShare(thing.height, walker.height));
	}
}

std::vector<Circle> creature_obstacles::CirclesOf(const Thing& thing, const Walker& walker)
{
	switch (thing.shape)
	{
	case Shape::ModelBox:
		return BoxCircles(thing, walker);
	case Shape::AtPlace:
	case Shape::Trunk:
		return {{.centre = thing.position, .radius = Clamped(ReachRadius(thing, walker) + walker.radius + FireMargin(thing))}};
	case Shape::Scenery:
		return {{.centre = thing.position, .radius = glm::length(thing.boxHalfSize) * k_SceneryShare}};
	case Shape::Temple:
		return TempleCircles(thing.position, thing.yAngle, thing.playerAlignment, thing.worshipSites);
	case Shape::Creature:
		return {{.centre = thing.position, .radius = (thing.width + walker.radius) * k_CreatureWidening}};
	}
	return {};
}

std::vector<Circle> creature_obstacles::TempleCircles(glm::vec2 centre, float yAngle, float playerAlignment,
                                                      const std::array<bool, 6>& worshipSites)
{
	std::vector<Circle> circles;
	const auto middle = static_cast<float>(k_TempleSize * k_TempleMiddle);
	circles.push_back({.centre = centre, .radius = static_cast<float>(middle + k_TempleMiddleExtra)});

	int32_t shortSpoke = 4;
	if (playerAlignment < k_EvilTemple)
	{
		shortSpoke = 2;
	}
	else if (playerAlignment < k_MiddlingTemple)
	{
		shortSpoke = 3;
	}
	const auto radius = static_cast<float>(k_TempleSize * k_SpokeCircle);
	const auto at = [&](float distance, double angle) {
		return glm::vec2(static_cast<float>((std::cos(angle) * distance) + centre.x),
		                 static_cast<float>((std::sin(angle) * distance) + centre.y));
	};
	auto angle = static_cast<float>(static_cast<double>(yAngle) + k_FirstSpoke);
	for (int32_t spoke = 0; spoke < 7; ++spoke)
	{
		auto distance = static_cast<float>((k_TempleSize * k_SpokeStart) + middle);
		const double towards = spoke == 0 ? 1.0 : -1.0;
		for (int32_t i = 0; i < 6; ++i)
		{
			if (spoke < 2)
			{
				// The first two spokes run on and bend towards each other at their ends
				const auto bend = i < 3 ? 0.0 : towards * k_Bends.at(static_cast<size_t>(i - 3));
				circles.push_back({.centre = at(distance, static_cast<double>(angle) + bend), .radius = radius});
				if (i < 3)
				{
					distance = static_cast<float>((k_TempleSize * k_SpokeStep) + distance);
				}
				else if (i == 3)
				{
					distance = static_cast<float>((k_TempleSize * k_BendStep) + distance);
				}
				else if (i == 4)
				{
					distance = static_cast<float>((k_TempleSize * k_SecondBendStep) + distance);
				}
			}
			else if (i < shortSpoke)
			{
				circles.push_back({.centre = at(distance, angle), .radius = radius});
				distance = static_cast<float>((k_TempleSize * k_SpokeStep) + distance);
			}
		}
		if (spoke != 0 && worshipSites.at(static_cast<size_t>(spoke - 1)))
		{
			circles.push_back({.centre = at(k_SiteDistance, static_cast<double>(angle) + k_SiteTurn), .radius = k_SiteRadius});
		}
		angle += k_SpokeTurn;
	}
	return circles;
}

float creature_obstacles::BodyRadius(std::span<const glm::mat4> boneMatrices)
{
	float furthest = 0.0f;
	for (const auto& bone : boneMatrices)
	{
		furthest = std::max(furthest, std::sqrt((bone[3].z * bone[3].z) + (bone[3].x * bone[3].x)));
	}
	return furthest;
}

float creature_obstacles::YAngleOf(const glm::mat3& rotation)
{
	return std::atan2(rotation[0].z, rotation[0].x);
}
