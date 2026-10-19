/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FixedOutlines.h"

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <bit>

#include "3D/MapCoords.h"
#include "Common/X87.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// The temple's circles, as the game keeps the numbers
constexpr float k_TempleReach = std::bit_cast<float>(0x41AC0000U);       // 21.5
constexpr float k_TempleMiddleShare = std::bit_cast<float>(0x3F333333U); // 0.7
constexpr float k_TempleFirstTurn = std::bit_cast<float>(0x40751EB8U);   // 3.83
constexpr float k_TempleSpokeShare = std::bit_cast<float>(0x3DE147AEU);  // 0.11
constexpr float k_TempleSpokeWidth = std::bit_cast<float>(0x3FB33333U);  // 1.4
constexpr float k_TempleStartShare = std::bit_cast<float>(0x3F3D70A4U);  // 0.74
constexpr float k_TempleStepShare = std::bit_cast<float>(0x3E4CCCCDU);   // 0.2
constexpr float k_TempleSpokeTurn = std::bit_cast<float>(0x3F65C8FAU);   // a seventh of a turn
constexpr int k_TempleSpokes = 7;
constexpr int k_TempleBentSpokes = 2;
constexpr int k_TempleStraightOfBent = 3;
constexpr int k_TempleStraightOfPlain = 4;
/// The bent circles of the first two spokes: the turn away from the spoke, for the first spoke and the second spoke,
/// and the share of the reach to the next circle
struct Bend
{
	float firstTurn;
	float secondTurn;
	float nextShare;
};
constexpr std::array<Bend, 3> k_TempleBends {{
    {.firstTurn = std::bit_cast<float>(0x3D75C28FU), // 0.06
     .secondTurn = std::bit_cast<float>(0xBD75C28FU),
     .nextShare = std::bit_cast<float>(0x3DCCCCCDU)}, // 0.1
    {.firstTurn = std::bit_cast<float>(0x3E23D70AU),  // 0.16
     .secondTurn = std::bit_cast<float>(0xBE23D70AU),
     .nextShare = std::bit_cast<float>(0x3D8F5C29U)}, // 0.07
    {.firstTurn = std::bit_cast<float>(0x3E8A3D71U),  // 0.27
     .secondTurn = std::bit_cast<float>(0xBE8A3D71U),
     .nextShare = 0.0f},
}};
} // namespace

fixed_outlines::Circle fixed_outlines::CircleOf(glm::vec2 centre, float radius)
{
	return {.centre = centre, .radius = radius, .radiusSquared = radius * radius};
}

fixed_outlines::Outline fixed_outlines::OfModelBox(const Rows& rows, glm::vec2 place, float yAngle, float scale,
                                                   glm::vec3 boxCentre, glm::vec3 boxHalfSize)
{
	// The middle of the box through the turn: for x the z, y and x terms in that order, for z the z, x and y terms
	const glm::vec2 centre {(((boxCentre.z * rows[2].x) + (boxCentre.y * rows[1].x)) + (boxCentre.x * rows[0].x)) + place.x,
	                        (((boxCentre.z * rows[2].z) + (boxCentre.x * rows[0].z)) + (boxCentre.y * rows[1].z)) + place.y};
	float halfX = scale * boxHalfSize.x;
	float halfZ = scale * boxHalfSize.z;
	if (halfX < k_LeastHalfSide)
	{
		halfX = k_LeastHalfSide;
	}
	if (halfZ < k_LeastHalfSide)
	{
		halfZ = k_LeastHalfSide;
	}
	const bool longAlongX = halfX > halfZ;
	const float ratio = longAlongX ? halfX / halfZ : halfZ / halfX;
	if (!(ratio > k_RoundBoxRatio))
	{
		return {.bounds = CircleOf(centre, longAlongX ? halfX : halfZ), .parts = {}};
	}
	const float squared = (halfX * halfX) + (halfZ * halfZ);
	return {.bounds = {.centre = centre, .radius = std::sqrt(squared), .radiusSquared = squared},
	        .parts = RowOfBox(centre, halfX, halfZ, yAngle)};
}

std::vector<fixed_outlines::Circle> fixed_outlines::RowOfBox(glm::vec2 centre, float halfX, float halfZ, float yAngle)
{
	const bool alongX = halfX > halfZ;
	const float longSide = alongX ? halfX : halfZ;
	const float shortSide = alongX ? halfZ : halfX;
	const int32_t count = map_coords::FtoL(longSide / shortSide) + 1;
	const float length = longSide + longSide;
	const float cosine = x87::RoundToFloat(x87::Cos(yAngle));
	const float sine = x87::RoundToFloat(x87::Sin(yAngle));
	const float step = length / static_cast<float>(count);
	std::vector<Circle> row;
	row.reserve(static_cast<size_t>(count));
	for (int32_t i = 0; i < count; ++i)
	{
		const float along = ((static_cast<float>(i) + 0.5f) * step) - (length * 0.5f);
		const float localX = alongX ? along : 0.0f;
		const float localZ = alongX ? 0.0f : along;
		const glm::vec2 at {((-sine * localZ) + (cosine * localX)) + centre.x,
		                    ((cosine * localZ) + (sine * localX)) + centre.y};
		row.push_back(CircleOf(at, shortSide));
	}
	return row;
}

fixed_outlines::Outline fixed_outlines::OfTrunk(glm::vec2 place)
{
	return {.bounds = CircleOf(place, k_TrunkRadius), .parts = {}};
}

std::vector<fixed_outlines::Circle> fixed_outlines::TempleRing(glm::vec2 place, float yAngle)
{
	const float spokeWidth = k_TempleSpokeWidth * (k_TempleReach * k_TempleSpokeShare);
	std::vector<Circle> ring;
	ring.reserve(1 + k_TempleBentSpokes * 6 + (k_TempleSpokes - k_TempleBentSpokes) * k_TempleStraightOfPlain);
	ring.push_back(CircleOf(place, k_TempleReach * k_TempleMiddleShare));
	float angle = yAngle + k_TempleFirstTurn;
	for (int spoke = 0; spoke < k_TempleSpokes; ++spoke)
	{
		const float sine = x87::RoundToFloat(x87::Sin(angle));
		const float cosine = x87::RoundToFloat(x87::Cos(angle));
		float distance = k_TempleReach * k_TempleStartShare;
		const bool bent = spoke < k_TempleBentSpokes;
		const int straight = bent ? k_TempleStraightOfBent : k_TempleStraightOfPlain;
		for (int i = 0; i < straight; ++i)
		{
			ring.push_back(CircleOf({(distance * cosine) + place.x, (distance * sine) + place.y}, spokeWidth));
			distance = (k_TempleReach * k_TempleStepShare) + distance;
		}
		if (bent)
		{
			// The first spoke bends one way, the second spoke the other way
			for (const auto& bend : k_TempleBends)
			{
				const float turned = (spoke == 0 ? bend.firstTurn : bend.secondTurn) + angle;
				ring.push_back(
				    CircleOf({x87::Mul(x87::Cos(turned), distance) + place.x, x87::Mul(x87::Sin(turned), distance) + place.y},
				             spokeWidth));
				distance = (k_TempleReach * bend.nextShare) + distance;
			}
		}
		angle = angle + k_TempleSpokeTurn;
	}
	return ring;
}

fixed_outlines::Circle fixed_outlines::Probe(glm::vec2 place)
{
	return CircleOf(place, k_ProbeRadius);
}

bool fixed_outlines::Meets(const Circle& own, const Circle& other)
{
	const float dx = own.centre.x - other.centre.x;
	const float dz = own.centre.y - other.centre.y;
	const float both = other.radius * own.radius;
	const float reach = ((both + both) + other.radiusSquared) + own.radiusSquared;
	return reach >= (dz * dz) + (dx * dx);
}

bool fixed_outlines::Collides(const Outline& outline, const Circle& other)
{
	if (!Meets(outline.bounds, other))
	{
		return false;
	}
	return outline.parts.empty() ||
	       std::ranges::any_of(outline.parts, [&other](const Circle& part) { return Meets(part, other); });
}
