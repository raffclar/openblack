/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureTrailBuilder.h"

#include <cmath>

#include <algorithm>

#include "Common/ModelInverseSquareRoot.h"

using namespace openblack;
using namespace openblack::gesture;

namespace
{
/// The symbol files' square runs from -100 to 100 on both axes
constexpr float k_SymbolHalfSize = 100.0f;
constexpr float k_SymbolScale = 0.005f;
/// The furthest any box starts from
constexpr float k_FarAway = 1e7f;

struct Box
{
	float minX;
	float minY;
	float maxX;
	float maxY;
};

/// The screen box the symbol's 0..1 square is stretched over: as wide and tall as the path's box against the symbol's
/// own box, about the path's box's middle
Box FitSymbol(const ScreenBox& box, const TrailSymbol& symbol)
{
	const float middleX = (box.min.x + box.max.x) * 0.5f;
	const float middleY = (box.max.y + box.min.y) * 0.5f;
	const float halfHeight = ((box.max.y - box.min.y) * 0.5f) / (symbol.maxZ - symbol.minZ);
	const float halfWidth = ((box.max.x - box.min.x) * 0.5f) / (symbol.maxX - symbol.minX);
	return {
	    .minX = middleX - halfWidth, .minY = middleY - halfHeight, .maxX = halfWidth + middleX, .maxY = halfHeight + middleY};
}

/// The pixel a point of the symbol's square falls on, truncated towards zero
glm::ivec2 PixelOf(const Box& box, float x, float z)
{
	const float px = ((box.maxX - box.minX + 1.0f) * x) + box.minX;
	const float py = ((box.maxY - box.minY + 1.0f) * z) + box.minY;
	return {static_cast<int>(px), static_cast<int>(py)};
}
/// A path taken at a number of points evenly through its points (not along its length): the first at its start, the last
/// at its end, the rest between the two points either side
std::vector<glm::vec3> TakenEvenly(const std::vector<glm::vec3>& path, size_t count)
{
	std::vector<glm::vec3> taken;
	if (path.empty() || count == 0)
	{
		return taken;
	}
	const float step = 1.0f / (static_cast<float>(count) - 1.0f);
	const auto last = path.size() - 1;
	taken.reserve(count);
	for (size_t i = 0; i < count; ++i)
	{
		const float along = static_cast<float>(i) * step;
		if (along == 1.0f)
		{
			taken.push_back(path.back());
			continue;
		}
		const float at = (static_cast<float>(path.size()) - 1.0f) * along;
		const auto index = std::min(static_cast<size_t>(at), last);
		// The end of the path rounded up holds at its last point
		const auto& from = path[index];
		const auto& to = path[std::min(index + 1, last)];
		const float t = at - static_cast<float>(index);
		taken.push_back(((to - from) * t) + from);
	}
	return taken;
}
} // namespace

TrailSymbol gesture::MakeTrailSymbol(std::span<const glm::vec3> filePoints)
{
	TrailSymbol symbol {.minX = k_FarAway, .maxX = -k_FarAway, .minZ = k_FarAway, .maxZ = -k_FarAway};
	if (filePoints.empty())
	{
		return symbol;
	}
	std::vector<glm::vec3> square;
	square.reserve(filePoints.size());
	for (const auto& point : filePoints)
	{
		const float x = (point.x + k_SymbolHalfSize) * k_SymbolScale;
		const float z = (k_SymbolHalfSize - point.z) * k_SymbolScale;
		square.emplace_back(x, point.y, z);
		symbol.maxX = std::max(x, symbol.maxX);
		symbol.minX = std::min(x, symbol.minX);
		symbol.maxZ = std::max(z, symbol.maxZ);
		symbol.minZ = std::min(z, symbol.minZ);
	}
	// As many points again, evenly along its length from its start, the last one step short of its end
	const particles::TrailPath path(std::move(square));
	symbol.length = path.Length();
	const auto count = filePoints.size();
	const float step = 1.0f / static_cast<float>(count);
	symbol.points.reserve(count);
	for (size_t i = 0; i < count; ++i)
	{
		symbol.points.push_back(path.At(static_cast<float>(i) * step));
	}
	return symbol;
}

std::vector<glm::vec3> gesture::TrailLandPoints(const GestureRecorder& recorder)
{
	std::vector<glm::vec3> points;
	for (size_t i = 0; i < recorder.Count(); ++i)
	{
		const auto& world = recorder.At(i).world;
		const auto onLand = [](float value) { return std::abs(static_cast<double>(value)) > k_TrailOnLand; };
		if (onLand(world.x) || onLand(world.y) || onLand(world.z))
		{
			points.push_back(world);
		}
	}
	return points;
}

std::optional<particles::GestureTrail> gesture::BuildTrail(std::span<const glm::vec3> landPoints, const ScreenBox& boxInPixels,
                                                           const TrailSymbol& symbol, const TrailView& view)
{
	if (landPoints.size() < 2 || symbol.points.empty() || !view.pointUnder)
	{
		return std::nullopt;
	}
	const auto box = FitSymbol(boxInPixels, symbol);

	// Depth is measured along the way the camera looks across the land, width across it
	glm::vec3 forward(view.cameraForward.x, 0.0f, view.cameraForward.z);
	if ((forward.x * forward.x) + (forward.z * forward.z) < k_TrailLevelEpsilon)
	{
		forward = {1.0f, 0.0f, 0.0f};
	}
	// The game uses its 3D engine's inverse square root here
	forward *= gutils::ModelInverseSquareRoot((forward.x * forward.x) + (forward.y * forward.y) + (forward.z * forward.z));

	std::vector<glm::vec3> laid;
	float squash = 1.0f;
	for (int tries = 1;; ++tries)
	{
		laid.clear();
		for (const auto& point : symbol.points)
		{
			const float z = ((point.z - 0.5f) * squash) + 0.5f;
			laid.push_back(view.pointUnder(PixelOf(box, point.x, z)));
		}
		float minAcross = k_FarAway;
		float maxAcross = -k_FarAway;
		float minDepth = k_FarAway;
		float maxDepth = -k_FarAway;
		for (const auto& point : laid)
		{
			const float across = (forward.z * point.x) + (-forward.x * point.z);
			const float depth = (forward.z * point.z) + (forward.x * point.x) + (forward.y * point.y);
			minAcross = std::min(minAcross, across);
			maxAcross = std::max(maxAcross, across);
			minDepth = std::min(minDepth, depth);
			maxDepth = std::max(maxDepth, depth);
		}
		const float width = std::abs(maxAcross - minAcross);
		const float depthOverWidth = width <= 0.0f ? 1.0f : std::abs(maxDepth - minDepth) / width;
		squash *= k_TrailSquash;
		if (tries >= k_TrailSquashTries || depthOverWidth <= k_TrailMostDepth)
		{
			break;
		}
	}

	// The shape on the land, taken at as many points as the hand's path has, evenly through its points
	auto ideal = TakenEvenly(laid, landPoints.size());

	particles::GestureTrail trail;
	trail.drawn = particles::TrailPath(std::vector<glm::vec3>(landPoints.begin(), landPoints.end()));
	trail.ideal = particles::TrailPath(std::move(ideal));
	return trail;
}

particles::GestureTrail gesture::LandGestureTrail(const TrailSymbol& symbol, glm::vec3 corner, float size,
                                                  const std::function<float(float x, float z)>& landHeight)
{
	// The symbol's square is laid on the land from the corner, each point dropped onto the land
	std::vector<glm::vec3> laid;
	laid.reserve(symbol.points.size());
	for (const auto& point : symbol.points)
	{
		const float x = (point.x * size) + corner.x;
		const float z = (point.z * size) + corner.z;
		laid.emplace_back(x, landHeight ? landHeight(x, z) : corner.y, z);
	}
	// The path and the shape are both the shape, taken at a fixed number of points
	auto points = TakenEvenly(laid, k_LandGesturePoints);
	particles::GestureTrail trail;
	trail.drawn = particles::TrailPath(points);
	trail.ideal = particles::TrailPath(std::move(points));
	return trail;
}
