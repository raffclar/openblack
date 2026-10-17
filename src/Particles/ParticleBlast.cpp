/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ParticleBlast.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <map>
#include <utility>

#include <glm/geometric.hpp>

#include "3D/MapCoords.h"
#include "Common/X87Vector.h"
#include "ParticleObjectEffects.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
/// Less of a piece's random part goes upwards
constexpr float k_UpwardShare = 0.5f;
/// A scale across this small draws the height as nothing
constexpr float k_SmallestAcross = 0.0001f;
} // namespace

blast::HeapAge blast::AgeHeap(float millisecondsLeft, std::optional<uint8_t> alpha, float frameMilliseconds)
{
	// Full alpha over the last second
	constexpr float k_AlphaPerMillisecond = 0.255f;
	if (millisecondsLeft <= k_RubbleFadeMilliseconds)
	{
		alpha = static_cast<uint8_t>(map_coords::FtoL(millisecondsLeft * k_AlphaPerMillisecond));
	}
	const float left = millisecondsLeft - frameMilliseconds;
	return {.millisecondsLeft = left, .alpha = alpha, .gone = left < 1.0f};
}

bool blast::SendsEvents(float age, const ExplosionRules& rules)
{
	return age > rules.initialDelay && age < rules.initialDelay + rules.timeToDoEventsFor;
}

float blast::WaveRange(float maxDistance, float tribalPower)
{
	return maxDistance * std::clamp(tribalPower, k_LeastTribalPower, k_MostTribalPower);
}

bool blast::Reached(float front, const WaveTarget& target, const glm::vec3& centre)
{
	const auto offset = target.point - centre;
	const float reach = front + target.radius;
	return reach * reach >= x87::DotZyx(offset, offset);
}

glm::vec3 blast::FragmentOrigin(const glm::vec3& centre)
{
	return centre - glm::vec3(0.0f, k_FragmentDrop, 0.0f);
}

glm::vec3 blast::FragmentVelocity(const glm::vec3& piece, const glm::vec3& origin, float speed, float randomFactor,
                                  const glm::vec3& random)
{
	auto out = piece - origin;
	if (out != glm::vec3(0.0f))
	{
		out = x87::Scaled<x87::SumOrder::Zyx>(out, speed);
	}
	// The random part is made as long as the speed out times the factor, its squares added z first
	auto scaledRandom = random;
	if (random != glm::vec3(0.0f))
	{
		scaledRandom = x87::Scaled<x87::SumOrder::Zyx>(random, x87::LengthZyx(out) * randomFactor);
	}
	return {scaledRandom.x + out.x, out.y + (scaledRandom.y * k_UpwardShare), scaledRandom.z + out.z};
}

std::optional<float> blast::MoveFraction(float age, float dt, float start, float stop, bool smoothly)
{
	if (age < start || age > stop)
	{
		return std::nullopt;
	}
	float t = stop > start ? (age - start) / (stop - start) : 1.0f;
	if (age + dt >= stop)
	{
		t = 1.0f;
	}
	if (smoothly)
	{
		t = t * t * (3.0f - 2.0f * t);
	}
	return t;
}

blast::XYZScale blast::ScaleXYZ(float across, float height)
{
	return {.across = across, .stretch = across <= k_SmallestAcross ? 0.0f : height / across};
}

std::vector<blast::FragmentPiece> blast::BreakIntoPieces(std::span<const SourcePrimitive> primitives)
{
	std::vector<FragmentPiece> pieces;
	for (const auto& primitive : primitives)
	{
		const size_t triangles = primitive.indices.size() / 3;
		if (triangles == 0)
		{
			continue;
		}
		// The triangles that share each edge, then each triangle's neighbours edge by edge. An edge is known by where its
		// ends are, not by which vertices they are, so triangles either side of a seam in the texture or the shading
		// are neighbours too. Its ends go in order of the sum of their coordinates, smaller first; with equal sums the
		// order the triangle gives them stands.
		using Point = std::array<float, 3>;
		std::map<std::pair<Point, Point>, std::vector<size_t>> edges;
		const auto pointOf = [&](size_t triangle, size_t corner) -> Point {
			const auto index = primitive.indices[triangle * 3 + corner];
			const auto at = index < primitive.positions.size() ? primitive.positions[index] : glm::vec3(0.0f);
			return {at.x, at.y, at.z};
		};
		const auto edgeOf = [&](size_t triangle, size_t corner) {
			const auto first = pointOf(triangle, (corner + 1) % 3);
			const auto second = pointOf(triangle, corner);
			const float firstSum = (first[2] + first[1]) + first[0];
			const float secondSum = (second[2] + second[1]) + second[0];
			return firstSum <= secondSum ? std::pair(first, second) : std::pair(second, first);
		};
		for (size_t t = 0; t < triangles; ++t)
		{
			for (size_t c = 0; c < 3; ++c)
			{
				edges[edgeOf(t, c)].push_back(t);
			}
		}
		std::vector<std::vector<size_t>> neighbours(triangles);
		for (size_t t = 0; t < triangles; ++t)
		{
			for (size_t c = 0; c < 3; ++c)
			{
				for (const auto other : edges[edgeOf(t, c)])
				{
					if (other != t)
					{
						neighbours[t].push_back(other);
					}
				}
			}
		}
		std::vector<bool> used(triangles, false);
		for (size_t first = 0; first < triangles; ++first)
		{
			if (used[first])
			{
				continue;
			}
			FragmentPiece piece;
			size_t current = first;
			for (;;)
			{
				used[current] = true;
				for (size_t c = 0; c < 3; ++c)
				{
					const auto index = primitive.indices[current * 3 + c];
					piece.positions.push_back(index < primitive.positions.size() ? primitive.positions[index]
					                                                             : glm::vec3(0.0f));
					piece.uvs.push_back(index < primitive.uvs.size() ? primitive.uvs[index] : glm::vec2(0.0f));
					piece.normals.push_back(index < primitive.normals.size() ? primitive.normals[index] : glm::vec3(0.0f));
				}
				piece.skins.push_back(primitive.skin);
				if (piece.skins.size() >= k_MostTrianglesInPiece)
				{
					break;
				}
				const auto next = std::ranges::find_if(neighbours[current], [&used](size_t n) { return !used[n]; });
				if (next == neighbours[current].end())
				{
					break;
				}
				current = *next;
			}
			pieces.push_back(std::move(piece));
		}
	}
	return pieces;
}

blast::PlacedFragment blast::PlaceFragment(const FragmentPiece& piece, const glm::mat4& transform, entt::id_type mesh)
{
	auto shape = std::make_shared<MeshFragment>();
	shape->mesh = mesh;
	shape->uvs = piece.uvs;
	shape->normals = piece.normals;
	shape->skins = piece.skins;
	shape->positions.reserve(piece.positions.size());
	glm::vec3 centre(0.0f);
	for (const auto& corner : piece.positions)
	{
		const auto world = glm::vec3(transform * glm::vec4(corner, 1.0f));
		shape->positions.push_back(world);
		centre += world;
	}
	if (!shape->positions.empty())
	{
		centre /= static_cast<float>(shape->positions.size());
	}
	for (auto& corner : shape->positions)
	{
		corner -= centre;
	}
	return {.centre = centre, .shape = std::move(shape)};
}
