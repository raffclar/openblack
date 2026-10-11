/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TemplePeopleRules.h"

#include <cmath>

#include <algorithm>

namespace openblack::temple_people
{
namespace
{
/// A direction this short along the floor faces nowhere
constexpr float k_NoDirection = 1e-6f;
/// The milliseconds in a second
constexpr float k_Milliseconds = 1000.0f;

int32_t TriangleBeyond(const std::vector<WalkMesh::Triangle>& triangles, uint32_t from, uint32_t to)
{
	for (size_t i = 0; i < triangles.size(); ++i)
	{
		const auto& c = triangles[i].corners;
		if ((c[0] == from && c[1] == to) || (c[1] == from && c[2] == to) || (c[2] == from && c[0] == to))
		{
			return static_cast<int32_t>(i);
		}
	}
	return -1;
}

uint32_t CornerOf(std::vector<glm::vec3>& corners, const glm::vec3& vertex)
{
	const auto found = std::ranges::find(corners, vertex);
	if (found != corners.end())
	{
		return static_cast<uint32_t>(found - corners.begin());
	}
	corners.push_back(vertex);
	return static_cast<uint32_t>(corners.size() - 1);
}

/// The best of the samples for one way on: the weight of how far it carries on along the heading, cubed
struct Way
{
	glm::vec3 point {0.0f};
	float weight {0.0f};
	int32_t triangle {-1};
};
} // namespace

WalkMesh BuildWalkMesh(std::span<const glm::vec3> vertices, std::span<const std::array<uint16_t, 3>> triangles)
{
	WalkMesh mesh;
	mesh.triangles.reserve(triangles.size());
	for (const auto& triangle : triangles)
	{
		WalkMesh::Triangle walk;
		for (size_t k = 0; k < 3; ++k)
		{
			walk.corners.at(k) = CornerOf(mesh.corners, vertices[triangle.at(k)]);
		}
		mesh.triangles.push_back(walk);
	}
	for (auto& triangle : mesh.triangles)
	{
		for (size_t k = 0; k < 3; ++k)
		{
			// The edge facing corner k, the other way round
			triangle.beyond.at(k) =
			    TriangleBeyond(mesh.triangles, triangle.corners.at((k + 2) % 3), triangle.corners.at((k + 1) % 3));
		}
	}
	return mesh;
}

glm::vec3 PointIn(const WalkMesh& mesh, int32_t triangle, Dice& dice)
{
	const float along = dice.UpTo(1.0f);
	const float left = 1.0f - along;
	const float across = dice.UpTo(left);
	const auto& corners = mesh.triangles.at(static_cast<size_t>(triangle)).corners;
	const auto& a = mesh.corners.at(corners[0]);
	const auto& b = mesh.corners.at(corners[1]);
	const auto& c = mesh.corners.at(corners[2]);
	glm::vec3 point;
	for (int i = 0; i < 3; ++i)
	{
		const float first = (b[i] - a[i]) * along;
		const float second = (c[i] - a[i]) * across;
		point[i] = first + a[i] + second;
	}
	return point;
}

glm::vec3 PointOnEdge(const WalkMesh& mesh, int32_t triangle, int edge, Dice& dice)
{
	const float along = dice.UpTo(1.0f);
	const auto& corners = mesh.triangles.at(static_cast<size_t>(triangle)).corners;
	const auto& a = mesh.corners.at(corners.at(static_cast<size_t>((edge + 1) % 3)));
	const auto& b = mesh.corners.at(corners.at(static_cast<size_t>((edge + 2) % 3)));
	glm::vec3 point;
	for (int i = 0; i < 3; ++i)
	{
		point[i] = (b[i] - a[i]) * along + a[i];
	}
	return point;
}

float HeadingBetween(const glm::vec3& from, const glm::vec3& to)
{
	return std::atan2(to.z - from.z, to.x - from.x);
}

float FacingOf(const glm::vec3& direction)
{
	if (direction.x * direction.x + direction.z * direction.z <= k_NoDirection)
	{
		return 0.0f;
	}
	return std::atan2(direction.x, -direction.z);
}

Walker Arrive(const WalkMesh& mesh, Dice& dice)
{
	Walker walker;
	const auto triangle = static_cast<int32_t>(dice.Below(static_cast<int32_t>(mesh.triangles.size())));
	walker.triangle = triangle;
	walker.nextTriangle = triangle;
	walker.position = PointIn(mesh, triangle, dice);
	walker.target = PointIn(mesh, triangle, dice);
	walker.heading = HeadingBetween(walker.position, walker.target);
	walker.facing = FacingOf(walker.target - walker.position);
	return walker;
}

namespace
{
/// Chooses where to go on reaching a point, and whether to stop there first. Returns the gait to have.
Gait ChooseNext(Walker& walker, const WalkMesh& mesh, Dice& dice)
{
	if (walker.nextTriangle >= 0)
	{
		walker.triangle = walker.nextTriangle;
	}
	if (walker.triangle < 0)
	{
		walker.triangle = static_cast<int32_t>(dice.Below(static_cast<int32_t>(mesh.triangles.size())));
		walker.nextTriangle = walker.triangle;
	}
	const float along = std::cos(walker.heading);
	const float across = std::sin(walker.heading);
	const auto& beyond = mesh.triangles.at(static_cast<size_t>(walker.triangle)).beyond;
	// The three ways out across its edges, then on inside it
	const std::array<int32_t, 4> ways {beyond[0], beyond[1], beyond[2], walker.triangle};
	std::array<Way, 4> best {};
	float total = 0.0f;
	for (size_t k = 0; k < ways.size(); ++k)
	{
		best.at(k).triangle = ways.at(k);
		if (ways.at(k) < 0)
		{
			best.at(k).point = PointIn(mesh, walker.triangle, dice);
			best.at(k).weight = k_DeadEndWeight;
		}
		else
		{
			for (int sample = 0; sample < k_SamplesPerWay; ++sample)
			{
				auto point = PointIn(mesh, ways.at(k), dice);
				if (!walker.inside && k < 3)
				{
					point = PointOnEdge(mesh, walker.triangle, static_cast<int>(k), dice);
				}
				const float ahead = (point.x - walker.position.x) * along + across * (point.z - walker.position.z);
				const float weight = std::max(ahead * ahead * ahead, 1.0f);
				if (weight > best.at(k).weight)
				{
					best.at(k).weight = weight;
					best.at(k).point = point;
				}
			}
		}
		total += best.at(k).weight;
	}
	float draw = dice.UpTo(total);
	size_t chosen = 0;
	while (chosen < 3 && draw >= best.at(chosen).weight)
	{
		draw -= best.at(chosen).weight;
		++chosen;
	}
	if (walker.inside)
	{
		chosen = 3;
	}
	walker.inside = !walker.inside;
	walker.target = best.at(chosen).point;
	walker.nextTriangle = best.at(chosen).triangle;
	walker.heading = HeadingBetween(walker.position, walker.target);
	walker.facing = FacingOf(walker.target - walker.position);
	return dice.UpTo(1.0f) < k_StopChance ? Gait::Standing : Gait::Walking;
}
} // namespace

void Step(Walker& walker, const WalkMesh& mesh, float seconds, Dice& dice)
{
	walker.timer += seconds;
	auto gait = walker.gait;
	if (walker.gait == Gait::Walking)
	{
		const auto toGo = walker.target - walker.position;
		const float distance = std::sqrt(toGo.z * toGo.z + toGo.y * toGo.y + toGo.x * toGo.x);
		const float step = seconds * walker.speed;
		const float share = step < distance ? step / distance : 1.0f;
		bool reached = true;
		if (distance >= k_ReachedDistance)
		{
			walker.position += toGo * share;
			reached = share >= k_ReachedShare;
		}
		if (reached)
		{
			walker.position = walker.target;
			gait = ChooseNext(walker, mesh, dice);
		}
	}
	else if (walker.timer > walker.pause)
	{
		gait = Gait::Walking;
	}
	if (gait != walker.gait)
	{
		walker.gait = gait;
		walker.timer = 0.0f;
		walker.clipPlace = 0;
		walker.pause = dice.UpTo(k_PauseSpread) + k_PauseLength;
	}
}

std::vector<entt::entity> ChooseVisitors(std::span<const std::vector<entt::entity>> worshippersBySite,
                                         const std::function<bool(entt::entity)>& available)
{
	std::vector<entt::entity> visitors;
	for (const auto& worshippers : worshippersBySite)
	{
		for (const auto worshipper : worshippers)
		{
			if (visitors.size() >= k_MostVisitors)
			{
				return visitors;
			}
			if (available(worshipper) && std::ranges::find(visitors, worshipper) == visitors.end())
			{
				visitors.push_back(worshipper);
			}
		}
	}
	return visitors;
}

bool Drawn(const glm::vec4& feet, float nearPlane)
{
	if (feet.w < nearPlane)
	{
		return false;
	}
	const float x = feet.x / feet.w;
	const float y = feet.y / feet.w;
	return x >= -1.0f && x < 1.0f && y > -1.0f && y <= 1.0f && feet.w < k_DrawnDepth;
}

AnimId ClipOf(Gait gait, bool man)
{
	if (gait == Gait::Standing)
	{
		return AnimId::PStand;
	}
	return man ? AnimId::PWalkMan : AnimId::PWalkWoman;
}

float SpeedOf(float stride, uint32_t playTime, float scale)
{
	if (playTime == 0)
	{
		return 0.0f;
	}
	return stride * k_Milliseconds / static_cast<float>(playTime) * scale;
}

} // namespace openblack::temple_people
