/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureCastMoves.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <limits>
#include <numbers>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Common/X87Angle.h"
#include "Common/X87Vector.h"
#include "Creature/CreaturePlayerAttention.h"

using namespace openblack;
using namespace openblack::creature_cast_moves;

namespace
{
/// Walking up to something much lower than this share of its own height, the creature keeps clear of it by most of its
/// own radius less; up to that height less and less, and past it by this much
constexpr float k_LowThingShare = 0.8f;
constexpr float k_LowThingKeep = 0.7f;
constexpr float k_TallerThingKeep = 0.3f;
constexpr float k_TallThingKeep = 0.4f;
/// A tree is walked up to within this share of its radius, at most this close
constexpr float k_TreeShare = 0.1f;
constexpr float k_TreeMost = 0.25f;
/// A thing longer than this many times its width keeps a row of circles clear
constexpr float k_LongThing = 1.4f;
/// Where a worship site's ten balls stand in its model, across, up and along; each is as wide as 1.3 times its height
constexpr std::array<glm::vec3, creature_cast_moves::k_WorshipSiteSpheres> k_WorshipSiteSpherePoints {{
    {2.448f, 1.4f, -34.171f},
    {3.348f, 1.4f, -36.551f},
    {24.71f, 1.4f, -24.133f},
    {26.359f, 1.4f, -25.951f},
    {3.93f, 2.0f, -39.084f},
    {27.452f, 2.0f, -28.503f},
    {7.405f, 3.0f, -40.707f},
    {12.317f, 3.0f, -39.988f},
    {22.537f, 3.0f, -35.073f},
    {26.905f, 3.0f, -33.063f},
}};
constexpr float k_WorshipSiteSphereWidth = 1.3f;
} // namespace

float creature_cast_moves::BoneReach(const skeletal_animation::Animation& animation,
                                     const skeletal_animation::Skeleton& skeleton)
{
	if (skeleton.Empty())
	{
		return 0.0f;
	}
	const auto poses = skeletal_animation::SampleCycle(animation, animation, 0, skeleton);
	const auto bones = skeletal_animation::ComposeBoneMatrices(poses, skeleton.parents);
	float reach = 0.0f;
	for (const auto& bone : bones)
	{
		reach = std::max(reach, std::sqrt(bone[3].x * bone[3].x + bone[3].z * bone[3].z));
	}
	return reach;
}

float creature_cast_moves::CreatureRadius(float modelScale, float boneReach)
{
	return modelScale * boneReach;
}

float creature_cast_moves::RoutePlanRadius(float radius, float height, bool tree, float creatureHeight, float creatureRadius)
{
	if (tree)
	{
		return std::min(k_TreeShare * radius, k_TreeMost);
	}
	const float low = creatureHeight * k_LowThingShare;
	const float keep = height <= low ? k_LowThingKeep - k_TallerThingKeep * height / low : k_TallThingKeep;
	return radius - keep * creatureRadius;
}

float creature_cast_moves::MapPointDistance(glm::vec2 a, glm::vec2 b)
{
	return glm::distance(map_coords::ToMetres(map_coords::FromMetres(a)), map_coords::ToMetres(map_coords::FromMetres(b)));
}

bool creature_cast_moves::SameDestination(glm::vec2 before, glm::vec2 now)
{
	const auto offset = now - before;
	return (offset.x * offset.x) + (offset.y * offset.y) < k_SameDestinationSquared;
}

bool creature_cast_moves::HeadsFor(glm::vec2 creature, glm::vec2 point, GameHeading heading)
{
	// The same test as a creature that follows the hand makes
	return creature_player_attention::IsDirectionClose(map_coords::FromMetres(creature), map_coords::FromMetres(point),
	                                                   heading.GameAngle());
}

bool creature_cast_moves::Arrived(float distance, float creatureRadius, float routeRadius, float keep)
{
	return distance < k_ArrivalMargin * (creatureRadius + routeRadius + keep);
}

float creature_cast_moves::GetAwayDistance(float keep, std::optional<float> thingRadius)
{
	return keep + (thingRadius.has_value() ? *thingRadius * k_GetAwayRadii : 0.0f);
}

glm::vec3 creature_cast_moves::GetAwayPoint(const glm::vec3& creature, const glm::vec3& thing, float apart, float distance)
{
	// The game adds the squares of the way z, y, x
	const auto away = creature - thing;
	glm::vec3 direction {1.0f, 0.0f, 0.0f};
	if (apart > k_GetAwayApart)
	{
		direction = away;
		if (away != glm::vec3(0.0f))
		{
			direction *= 1.0f / x87::LengthZyx(away);
		}
	}
	return (direction * distance) + creature;
}

std::optional<glm::vec2> creature_cast_moves::FindClearArea(glm::vec2 point, float width, const ClearCell& clear)
{
	const auto centreX = static_cast<int32_t>(std::floor(point.x / k_CellSize));
	const auto centreZ = static_cast<int32_t>(std::floor(point.y / k_CellSize));
	const int32_t originX = centreX - k_ClearAreaCells / 2;
	const int32_t originZ = centreZ - k_ClearAreaCells / 2;
	std::vector<bool> open(static_cast<size_t>(k_ClearAreaCells * k_ClearAreaCells));
	for (int32_t z = 0; z < k_ClearAreaCells; ++z)
	{
		for (int32_t x = 0; x < k_ClearAreaCells; ++x)
		{
			open.at(static_cast<size_t>(x + z * k_ClearAreaCells)) = clear(originX + x, originZ + z);
		}
	}
	// A square of cells as wide as the area, rounded up to whole cells
	const auto whole = static_cast<int32_t>(width);
	const int32_t cells = static_cast<int32_t>(width / k_CellSize) + (whole % static_cast<int32_t>(k_CellSize) != 0 ? 1 : 0);
	const float half = width * 0.5f;
	const auto inDisc = [&](int32_t i, int32_t j) {
		const float dx = half - (static_cast<float>(i) * k_CellSize + k_CellSize * 0.5f);
		const float dz = half - (static_cast<float>(j) * k_CellSize + k_CellSize * 0.5f);
		return dx * dx + dz * dz <= 0.25f * width * width;
	};
	std::optional<glm::vec2> best;
	float nearest = std::numeric_limits<float>::max();
	for (int32_t a = 0; a < k_ClearAreaCells - cells; ++a)
	{
		for (int32_t b = 0; b < k_ClearAreaCells - cells; ++b)
		{
			bool fits = true;
			for (int32_t i = 0; i < cells && fits; ++i)
			{
				for (int32_t j = 0; j < cells && fits; ++j)
				{
					fits = !inDisc(i, j) || open.at(static_cast<size_t>(a + i + (b + j) * k_ClearAreaCells));
				}
			}
			if (!fits)
			{
				continue;
			}
			const glm::vec2 candidate {static_cast<float>(originX + a) * k_CellSize + half,
			                           static_cast<float>(originZ + b) * k_CellSize + half};
			const float distance = glm::distance(candidate, point);
			if (distance < nearest)
			{
				nearest = distance;
				best = candidate;
			}
		}
	}
	return best;
}

bool creature_cast_moves::Facing(glm::vec2 creature, GameHeading heading, glm::vec2 point)
{
	if (MapPointDistance(point, creature) < k_OnTopDistance)
	{
		return true;
	}
	const float off = HeadingTo(creature, point).TurnFrom(heading);
	return !(std::abs(off) > k_FacingRadians);
}

namespace
{
/// The way from a creature's map position to a point's, each made metres first
glm::vec2 MapOffset(glm::vec2 creature, glm::vec2 point)
{
	return map_coords::ToMetres(map_coords::FromMetres(point)) - map_coords::ToMetres(map_coords::FromMetres(creature));
}
} // namespace

GameHeading creature_cast_moves::HeadingTo(glm::vec2 creature, glm::vec2 point)
{
	const auto offset = MapOffset(creature, point);
	return GameHeading::Of(offset.x, offset.y);
}

GameHeading creature_cast_moves::HeadingAwayFrom(glm::vec2 creature, glm::vec2 thing)
{
	// The half turn is added to the heading before the heading is made a float, and the sum is rounded one time
	const auto offset = MapOffset(creature, thing);
	return GameHeading(x87::Add(x87::GameHeadingOf(offset.x, offset.y), std::numbers::pi_v<float>));
}

std::vector<CollideCircle> creature_cast_moves::CollideCircles(glm::vec2 halfSize, glm::vec2 centre, const glm::mat2& turn)
{
	const auto half = glm::max(halfSize, glm::vec2(1.0f));
	const float longer = std::max(half.x, half.y);
	const float shorter = std::min(half.x, half.y);
	if (!(longer / shorter > k_LongThing))
	{
		return {{.centre = centre, .radius = longer}};
	}
	// A row of circles as wide as its shorter side, spread evenly along its longer
	const auto count = static_cast<int32_t>(longer / shorter) + 1;
	const float spacing = 2.0f * longer / static_cast<float>(count);
	std::vector<CollideCircle> circles;
	circles.reserve(static_cast<size_t>(count));
	for (int32_t i = 0; i < count; ++i)
	{
		const float along = (static_cast<float>(i) + 0.5f) * spacing - longer;
		const glm::vec2 local = half.x > half.y ? glm::vec2(along, 0.0f) : glm::vec2(0.0f, along);
		circles.push_back({.centre = centre + turn * local, .radius = shorter});
	}
	return circles;
}

bool creature_cast_moves::BlocksCell(const CollideCircle& circle, int32_t x, int32_t z)
{
	const glm::vec2 middle {static_cast<float>(x) * k_CellSize + k_CellSize * 0.5f,
	                        static_cast<float>(z) * k_CellSize + k_CellSize * 0.5f};
	return glm::distance(middle, circle.centre) - circle.radius < k_ClearOfThings;
}

bool creature_cast_moves::BlocksCell(const CollideSphere& sphere, int32_t x, int32_t z)
{
	const glm::vec3 middle {static_cast<float>(x) * k_CellSize + k_CellSize * 0.5f, 0.0f,
	                        static_cast<float>(z) * k_CellSize + k_CellSize * 0.5f};
	// The squares are added from z to x
	return x87::Distance<x87::SumOrder::Zyx>(middle, sphere.centre) - sphere.radius < k_ClearOfThings;
}

std::array<CollideSphere, creature_cast_moves::k_WorshipSiteSpheres>
creature_cast_moves::WorshipSiteSpheres(const glm::vec3& position, float yAngle, float scale)
{
	std::array<CollideSphere, k_WorshipSiteSpheres> spheres {};
	// The site's turn and scale as the game builds them: the cosine and the sine each times the scale
	const float c = x87::Mul(x87::Cos(yAngle), scale);
	const float s = x87::Mul(x87::Sin(yAngle), scale);
	const std::array<glm::vec3, 3> rows {glm::vec3(c, 0.0f, s), glm::vec3(0.0f, scale, 0.0f), glm::vec3(-s, 0.0f, c)};
	std::ranges::transform(k_WorshipSiteSpherePoints, spheres.begin(), [&](const glm::vec3& point) {
		// Each part adds the products of z, then y, then x, and then the place
		const auto part = [&](int axis) {
			return (((rows[2][axis] * point.z) + (rows[1][axis] * point.y)) + (rows[0][axis] * point.x)) + position[axis];
		};
		return CollideSphere {.centre = {part(0), part(1), part(2)}, .radius = k_WorshipSiteSphereWidth * point.y};
	});
	return spheres;
}

bool creature_cast_moves::BlocksCell(const ThingInCell& thing, int32_t x, int32_t z, bool livingBlocks)
{
	if (thing.temple || (livingBlocks && thing.living))
	{
		return true;
	}
	return thing.tested &&
	       std::ranges::any_of(thing.spheres, [x, z](const CollideSphere& sphere) { return BlocksCell(sphere, x, z); });
}

creature_cast_moves::Stall creature_cast_moves::CheckStall(uint32_t turns, float moved, bool scripted)
{
	if (turns % k_StallTurns == 0)
	{
		return Stall::Note;
	}
	if (turns % k_StallTurns == k_StallTurns - 1 && !scripted && moved < k_StallDistance)
	{
		return Stall::GiveUp;
	}
	return Stall::None;
}

creature_cast_moves::WalkStart creature_cast_moves::WalkStartOf(bool started, bool destinationValid)
{
	if (!destinationValid)
	{
		return WalkStart::NoWay;
	}
	return started ? WalkStart::Walking : WalkStart::TryAgain;
}
