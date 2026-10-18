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

#include "Common/X87.h"
#include "Common/X87Angle.h"
#include "Common/X87Vector.h"

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
constexpr float k_RowShare = 0.2f;
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

// The temple's circles, all in proportion to one size
constexpr float k_TempleSize = 21.5f;
constexpr float k_TempleMiddle = 0.64f;
constexpr float k_TempleMiddleExtra = 1.1f;
constexpr float k_SpokeCircle = 0.11f;
constexpr float k_SpokeStart = 0.1f;
constexpr float k_SpokeStep = 0.2f;
constexpr float k_BendStep = 0.1f;
constexpr float k_SecondBendStep = 0.07f;
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

/// The outline of a model's box: one circle at its middle, or a row along a long box
std::vector<Circle> BoxCircles(const Thing& thing, const Walker& walker)
{
	// The margin and the fire margin are added one after the other
	const auto margin = walker.radius - (RadiusShare(thing.height, walker.height) * walker.radius);
	const auto fire = FireMargin(thing);
	// The middle of the box goes through the thing's turn and scale as the game builds them
	const auto rows = thing.placement == Placement::Leaning ? LeaningPlacement(thing.yAngle, 0.0f, 0.0f, thing.scale)
	                                                        : UprightPlacement(thing.yAngle, thing.scale);
	const auto centre = PlacedOnLand(rows, thing.boxCentre, thing.position);
	auto sizeX = ((thing.boxHalfSize.x * thing.scale) + margin) + fire;
	auto sizeZ = ((thing.boxHalfSize.z * thing.scale) + margin) + fire;

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
			sizeZ = sizeX * k_RowShare;
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
			sizeX = sizeZ * k_RowShare;
		}
		alongX = false;
		spacing = sizeZ / static_cast<float>(count);
		width = sizeX;
	}
	if (count == 1)
	{
		return {{.centre = centre, .radius = Clamped((thing.width + margin) + fire)}};
	}

	// Along the long side, the circles' middles evenly spaced over its length
	const auto angle = thing.yAngle + (alongX ? std::numbers::pi_v<float> / 2.0f : 0.0f);
	// The sine and the cosine go wide into the multiply
	const auto step = glm::vec2(x87::Mul(x87::Sin(angle), spacing), -x87::Mul(x87::Cos(angle), spacing));
	auto at = centre - (step * static_cast<float>(count - 1));
	const auto radius = Clamped(x87::Mul(width, k_RowWidening));
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
	case Avoidance::Shield:
		return !walker.scriptControlled && thing.owner != walker.owner;
	case Avoidance::VortexOutOfTheLand:
		return !walker.mayBeTakenByVortex;
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
		return {{.centre = thing.position, .radius = x87::LengthZyx(thing.boxHalfSize) * k_SceneryShare}};
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
	const float middle = k_TempleSize * k_TempleMiddle;
	circles.push_back({.centre = centre, .radius = middle + k_TempleMiddleExtra});

	int32_t shortSpoke = 4;
	if (playerAlignment < k_EvilTemple)
	{
		shortSpoke = 2;
	}
	else if (playerAlignment < k_MiddlingTemple)
	{
		shortSpoke = 3;
	}
	const float radius = k_TempleSize * k_SpokeCircle;
	// A circle on a turned line: the angle is a float, and its sine and cosine go wide into the multiply
	const auto turnedAt = [&](float distance, float angle) {
		return glm::vec2(x87::Mul(x87::Cos(angle), distance) + centre.x, x87::Mul(x87::Sin(angle), distance) + centre.y);
	};
	float angle = yAngle + k_FirstSpoke;
	for (int32_t spoke = 0; spoke < 7; ++spoke)
	{
		// The straight circles of a spoke use its sine and cosine kept as floats
		const float cosine = x87::RoundToFloat(x87::Cos(angle));
		const float sine = x87::RoundToFloat(x87::Sin(angle));
		const auto straightAt = [&](float distance) {
			return glm::vec2((distance * cosine) + centre.x, (distance * sine) + centre.y);
		};
		float distance = (k_TempleSize * k_SpokeStart) + middle;
		const float towards = spoke == 0 ? 1.0f : -1.0f;
		for (int32_t i = 0; i < 6; ++i)
		{
			if (spoke < 2)
			{
				// The first two spokes run on and bend towards each other at their ends
				if (i < 3)
				{
					circles.push_back({.centre = straightAt(distance), .radius = radius});
					distance = (k_TempleSize * k_SpokeStep) + distance;
					continue;
				}
				const float bend = towards * k_Bends.at(static_cast<size_t>(i - 3));
				circles.push_back({.centre = turnedAt(distance, bend + angle), .radius = radius});
				if (i == 3)
				{
					distance = (k_TempleSize * k_BendStep) + distance;
				}
				else if (i == 4)
				{
					distance = (k_TempleSize * k_SecondBendStep) + distance;
				}
			}
			else if (i < shortSpoke)
			{
				circles.push_back({.centre = straightAt(distance), .radius = radius});
				distance = (k_TempleSize * k_SpokeStep) + distance;
			}
		}
		if (spoke != 0 && worshipSites.at(static_cast<size_t>(spoke - 1)))
		{
			circles.push_back({.centre = turnedAt(k_SiteDistance, angle + k_SiteTurn), .radius = k_SiteRadius});
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

PlacementRows creature_obstacles::UprightPlacement(float yAngle, float scale)
{
	const float c = x87::Mul(x87::Cos(yAngle), scale);
	const float s = x87::Mul(x87::Sin(yAngle), scale);
	return {glm::vec3(c, 0.0f, s), glm::vec3(0.0f, scale, 0.0f), glm::vec3(-s, 0.0f, c)};
}

PlacementRows creature_obstacles::LeaningPlacement(float yAngle, float xAngle, float zAngle, float scale)
{
	auto rows = x87::GameTurnYxz(yAngle, xAngle, zAngle);
	if (scale != 1.0f)
	{
		for (auto& row : rows)
		{
			row *= scale;
		}
	}
	return rows;
}

glm::vec2 creature_obstacles::PlacedOnLand(const PlacementRows& rows, const glm::vec3& point, glm::vec2 position)
{
	const float x = (((rows[1].x * point.y) + (rows[0].x * point.x)) + (rows[2].x * point.z)) + position.x;
	const float z = (((rows[1].z * point.y) + (rows[0].z * point.x)) + (rows[2].z * point.z)) + position.y;
	return {x, z};
}

glm::vec3 creature_obstacles::PlacedInWorld(const PlacementRows& rows, const glm::vec3& point, const glm::vec3& position)
{
	const auto along = [&](int axis) {
		return (((rows[1][axis] * point.y) + (rows[0][axis] * point.x)) + (rows[2][axis] * point.z)) + position[axis];
	};
	return {along(0), along(1), along(2)};
}
