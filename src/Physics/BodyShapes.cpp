/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BodyShapes.h"

#include <cmath>

#include <algorithm>
#include <ranges>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include "Common/X87Vector.h"

using namespace openblack::physics;
using namespace openblack::physics::shapes;

namespace
{
constexpr float k_TreeDragFactor = 0.3f;
constexpr float k_LivingDragFactor = 2.0f;
constexpr float k_FragmentDragFactor = 2.0f;
/// A piece whose area over its width is under this share of its radius is too thin to keep
constexpr float k_ThinPieceShare = 0.1f;
/// The cosine of a quarter turn as the game stores it, which is not quite zero

float FarthestPoint(std::span<const glm::vec3> points)
{
	float farthest = 0.0f;
	for (const auto& point : points)
	{
		farthest = std::max(farthest, openblack::x87::LengthZyx(point));
	}
	return farthest;
}
} // namespace

std::vector<std::size_t> shapes::BodyParts(std::span<const ModelPart> parts)
{
	const bool anyPhysics = std::ranges::any_of(parts, &ModelPart::isPhysics);
	std::vector<std::size_t> used;
	for (std::size_t i = 0; i < parts.size(); ++i)
	{
		if (anyPhysics ? parts[i].isPhysics : parts[i].nearestDetail)
		{
			used.push_back(i);
		}
	}
	return used;
}

std::vector<float> shapes::LandLifts(std::span<const ModelPart> parts, const glm::mat3& scaledAxes, const glm::vec3& origin,
                                     float scale, const Ground& ground)
{
	const float baseHeight = ground.HeightAt(glm::vec2(origin.x, origin.z));
	const float inverseScale = 1.0f / scale;
	std::vector<float> lifts;
	for (const auto index : BodyParts(parts))
	{
		for (const auto& vertex : parts[index].positions)
		{
			// The game adds the products of the vertex and the axes in this order
			const float x = ((vertex.z * scaledAxes[2].x + vertex.y * scaledAxes[1].x) + vertex.x * scaledAxes[0].x) + origin.x;
			const float z = ((vertex.x * scaledAxes[0].z + vertex.z * scaledAxes[2].z) + vertex.y * scaledAxes[1].z) + origin.z;
			lifts.push_back((ground.HeightAt(glm::vec2(x, z)) - baseHeight) * inverseScale);
		}
	}
	return lifts;
}

Shape shapes::FromModel(std::span<const ModelPart> parts, float scale, std::span<const float> lifts)
{
	Shape shape;
	std::vector<glm::vec3> positions;
	for (const auto index : BodyParts(parts))
	{
		const auto& part = parts[index];
		const auto base = static_cast<uint32_t>(positions.size());
		positions.insert(positions.end(), part.positions.begin(), part.positions.end());
		for (std::size_t i = 0; i + 2 < part.indices.size(); i += 3)
		{
			shape.faces.push_back({base + part.indices[i], base + part.indices[i + 1], base + part.indices[i + 2]});
		}
	}
	if (positions.empty())
	{
		return shape;
	}
	glm::vec3 sum(0.0f);
	for (const auto& position : positions)
	{
		sum += position;
	}
	// The game multiplies the sum by one over the count
	shape.centreOfMass = (1.0f / static_cast<float>(positions.size())) * sum;
	shape.points.reserve(positions.size());
	for (std::size_t i = 0; i < positions.size(); ++i)
	{
		auto position = positions[i];
		if (i < lifts.size())
		{
			position.y += lifts[i];
		}
		shape.points.push_back((position - shape.centreOfMass) * scale);
	}
	shape.radius = FarthestPoint(shape.points);
	return shape;
}

Shape shapes::Tree(float height, float radius, float scale, bool rooted)
{
	const float half = (rooted ? 0.6f : 0.5f) * height;
	Shape shape;
	// The game divides the share by the scale first
	shape.centreOfMass = glm::vec3(0.0f, ((rooted ? 0.4f : 0.5f) / scale) * height, 0.0f);
	for (const auto& [ringRadius, y] :
	     {std::pair {0.65f * radius, -0.6f * half}, std::pair {radius, 0.0f}, std::pair {0.65f * radius, 0.6f * half}})
	{
		shape.points.emplace_back(ringRadius, y, 0.0f);
		shape.points.emplace_back(0.0f, y, -ringRadius);
		shape.points.emplace_back(-ringRadius, y, 0.0f);
		shape.points.emplace_back(0.0f, y, ringRadius);
	}
	shape.points.emplace_back(0.1f * radius, -half, 0.0f);
	shape.points.emplace_back(0.0f, half, 0.0f);
	shape.points.emplace_back(0.1f * radius, -half, 0.0f);
	shape.points.emplace_back(0.1f * radius, -half, 0.0f);
	for (const uint32_t ring : {5u, 9u})
	{
		shape.faces.push_back({ring - 5, ring - 4, ring});
		shape.faces.push_back({ring - 5, ring, ring - 1});
		shape.faces.push_back({ring - 4, ring - 3, ring + 1});
		shape.faces.push_back({ring - 4, ring + 1, ring});
		shape.faces.push_back({ring - 3, ring - 2, ring + 2});
		shape.faces.push_back({ring - 3, ring + 2, ring + 1});
		shape.faces.push_back({ring - 2, ring - 5, ring - 1});
		shape.faces.push_back({ring - 2, ring - 1, ring + 2});
	}
	for (const std::array<uint32_t, 3> cap : {std::array<uint32_t, 3> {12, 1, 0},
	                                          {12, 2, 1},
	                                          {12, 3, 2},
	                                          {12, 0, 3},
	                                          {13, 8, 9},
	                                          {13, 9, 10},
	                                          {13, 10, 11},
	                                          {13, 11, 8}})
	{
		shape.faces.push_back(cap);
	}
	shape.radius = std::max(half, radius);
	shape.dragFactor = k_TreeDragFactor;
	return shape;
}

Shape shapes::LivingBody(Living kind, float height, float radius, float scale)
{
	const bool animal = kind == Living::Animal;
	const float y = 0.5f * height;
	// Across and along at the head and feet, then at the waist
	const float endX = (animal ? 0.25f : 0.8f) * radius;
	const float endZ = (animal ? 0.8f : 0.32f) * radius;
	const float waistX = (animal ? 0.3f : 0.9f) * radius;
	const float waistZ = (animal ? 0.9f : 0.32f) * radius;
	const std::array<glm::vec3, 12> box {{
	    {endX, y, endZ},
	    {-endX, y, -endZ},
	    {-endX, y, endZ},
	    {endX, y, -endZ},
	    {waistX, 0.0f, waistZ},
	    {-waistX, 0.0f, -waistZ},
	    {-waistX, 0.0f, waistZ},
	    {waistX, 0.0f, -waistZ},
	    {endX, -y, endZ},
	    {-endX, -y, -endZ},
	    {-endX, -y, endZ},
	    {endX, -y, -endZ},
	}};
	Shape shape;
	// The game divides the half by the scale first
	shape.centreOfMass = glm::vec3(0.0f, (0.5f / scale) * height, 0.0f);
	for (const auto& point : box)
	{
		// A quarter turn about the up axis
		shape.points.emplace_back(point.x * k_QuarterTurnCos - point.z, point.y, point.x + point.z * k_QuarterTurnCos);
	}
	shape.faces = {{1, 2, 0},  {0, 3, 1},  {5, 1, 3},  {7, 5, 3},  {7, 3, 0},   {4, 7, 0},  {6, 4, 0},
	               {2, 6, 0},  {6, 2, 1},  {5, 6, 1},  {9, 5, 7},  {11, 9, 7},  {11, 7, 4}, {8, 11, 4},
	               {10, 8, 4}, {6, 10, 4}, {10, 6, 5}, {9, 10, 5}, {11, 10, 9}, {8, 10, 11}};
	shape.radius = FarthestPoint(shape.points);
	shape.dragFactor = k_LivingDragFactor;
	return shape;
}

FragmentShape shapes::Fragment(std::span<const std::array<glm::vec3, 3>> triangles)
{
	FragmentShape result;
	std::vector<glm::vec3> corners;
	std::vector<glm::vec3> normals;
	for (const auto& triangle : triangles)
	{
		auto normal = glm::cross(triangle[1] - triangle[0], triangle[2] - triangle[0]);
		// The game adds the squares from z to x and multiplies by one over the length
		const float twiceArea = openblack::x87::LengthZyx(normal);
		if (twiceArea != 0.0f)
		{
			normal *= 1.0f / twiceArea;
		}
		result.area += twiceArea * 0.5f;
		for (const auto& corner : triangle)
		{
			// Each corner once, with the normal of the first triangle it is found in
			if (std::ranges::find(corners, corner) == corners.end())
			{
				corners.push_back(corner);
				normals.push_back(normal);
			}
		}
	}
	result.mass = std::max(result.area * k_FragmentMassPerArea, k_MinMass);
	auto& shape = result.shape;
	for (std::size_t i = 0; i < corners.size(); ++i)
	{
		shape.points.push_back(corners[i]);
		shape.points.push_back(corners[i] - normals[i] * k_FragmentThickness);
	}
	shape.radius = FarthestPoint(shape.points);
	shape.dragFactor = k_FragmentDragFactor;
	const float width = shape.radius + shape.radius;
	result.tooThin = result.area / width < width * k_ThinPieceShare;
	return result;
}

std::vector<shapes::BoneBox> shapes::BoneBoxes(std::span<const glm::vec3> positions, std::span<const uint16_t> bones,
                                               size_t boneCount)
{
	std::vector<BoneBox> boxes(std::max<size_t>(boneCount, 1));
	for (size_t i = 0; i < positions.size(); ++i)
	{
		const size_t bone = bones.empty() ? 0 : (i < bones.size() ? bones[i] : k_NoBone);
		if (bone >= boxes.size())
		{
			continue;
		}
		boxes[bone].min = glm::min(boxes[bone].min, positions[i]);
		boxes[bone].max = glm::max(boxes[bone].max, positions[i]);
	}
	return boxes;
}

Shape shapes::Creature(float sphereRadius, size_t parts)
{
	// Every point sits on the centre in the body's own frame; only where they stand in the world, at the parts' origins,
	// is set when the body is placed
	Shape shape;
	shape.points.assign(parts, glm::vec3(0.0f));
	shape.radius = sphereRadius;
	return shape;
}
