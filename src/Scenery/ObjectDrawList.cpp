/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectDrawList.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <bitset>

#include <glm/geometric.hpp>
#include <glm/vec4.hpp>

using namespace openblack;
using namespace openblack::object_draw_list;

namespace
{
/// The squared distance between two points
float DistanceSquared(glm::vec3 a, glm::vec3 b)
{
	const auto d = a - b;
	return glm::dot(d, d);
}

/// The block's box: its height range and the height its middle is taken at
struct BlockBox
{
	float bottom;
	float top;
	float middle;
};

BlockBox BoxOf(const Block& block)
{
	const float height = static_cast<float>(block.highestAltitude) * k_AltitudeUnit;
	if (block.reflected)
	{
		return {.bottom = -height, .top = height, .middle = 0.0f};
	}
	const float half = height * 0.5f;
	return {.bottom = half - half, .top = half + half, .middle = half};
}
} // namespace

bool object_draw_list::BlockInView(const glm::mat4& viewProjection, float nearPlane, const Block& block)
{
	const auto box = BoxOf(block);
	// Each side of the view, with a bit for each corner beyond it
	uint32_t behind = 0;
	uint32_t right = 0;
	uint32_t left = 0;
	uint32_t above = 0;
	uint32_t below = 0;
	for (uint32_t i = 0; i < 8; ++i)
	{
		const glm::vec3 corner {block.corner.x + ((i & 1u) != 0 ? k_BlockSize : 0.0f), (i & 2u) != 0 ? box.top : box.bottom,
		                        block.corner.y + ((i & 4u) != 0 ? k_BlockSize : 0.0f)};
		const auto clip = viewProjection * glm::vec4(corner, 1.0f);
		const uint32_t bit = 1u << i;
		if (clip.w < nearPlane)
		{
			behind |= bit;
		}
		if (clip.x > clip.w)
		{
			right |= bit;
		}
		else if (clip.x < -clip.w)
		{
			left |= bit;
		}
		if (clip.y > clip.w)
		{
			above |= bit;
		}
		else if (clip.y < -clip.w)
		{
			below |= bit;
		}
	}
	constexpr uint32_t k_All = 0xffu;
	return behind != k_All && right != k_All && left != k_All && above != k_All && below != k_All;
}

float object_draw_list::BlockDistance(glm::vec3 eye, const Block& block)
{
	const auto box = BoxOf(block);
	const glm::vec3 middle {block.corner.x + (k_BlockSize * 0.5f), box.middle, block.corner.y + (k_BlockSize * 0.5f)};
	return std::sqrt(DistanceSquared(middle, eye));
}

namespace
{
/// The five sides of the view in clip space. A point is on the inner side of a side when the value is zero or more.
constexpr size_t k_Sides = 5;

float SideValue(size_t side, float nearPlane, glm::vec4 p)
{
	switch (side)
	{
	case 0:
		return p.w - nearPlane;
	case 1:
		return p.w - p.x;
	case 2:
		return p.w + p.x;
	case 3:
		return p.w - p.y;
	default:
		return p.w + p.y;
	}
}

/// A triangle that the sides of the view cut: each side can add one corner
using Polygon = std::array<glm::vec4, 3 + k_Sides>;

/// Cuts the polygon to the inner side of a side of the view, and gives the number of corners that remain
size_t Cut(size_t side, float nearPlane, const Polygon& in, size_t count, Polygon& out)
{
	size_t kept = 0;
	for (size_t i = 0; i < count; ++i)
	{
		const auto from = in.at(i);
		const auto to = in.at((i + 1) % count);
		const float fromValue = SideValue(side, nearPlane, from);
		const float toValue = SideValue(side, nearPlane, to);
		if (fromValue >= 0.0f)
		{
			out.at(kept++) = from;
		}
		if ((fromValue >= 0.0f) != (toValue >= 0.0f))
		{
			out.at(kept++) = from + (to - from) * (fromValue / (fromValue - toValue));
		}
	}
	return kept;
}
} // namespace

bool object_draw_list::TriangleFacesEye(glm::vec3 eye, glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
	// The land shows its upper side only
	auto normal = glm::cross(b - a, c - a);
	if (normal.y < 0.0f)
	{
		normal = -normal;
	}
	return glm::dot(normal, eye - a) > 0.0f;
}

bool object_draw_list::ClippedTriangleShows(float nearPlane, glm::vec4 a, glm::vec4 b, glm::vec4 c)
{
	// A triangle with all three corners beyond one side is gone; one with all corners in view stays whole
	bool inside = true;
	for (size_t side = 0; side < k_Sides; ++side)
	{
		const bool aOut = SideValue(side, nearPlane, a) < 0.0f;
		const bool bOut = SideValue(side, nearPlane, b) < 0.0f;
		const bool cOut = SideValue(side, nearPlane, c) < 0.0f;
		if (aOut && bOut && cOut)
		{
			return false;
		}
		inside = inside && !aOut && !bOut && !cOut;
	}
	if (inside)
	{
		return true;
	}
	Polygon polygon {a, b, c};
	Polygon cut {};
	size_t count = 3;
	for (size_t side = 0; side < k_Sides && count >= 3; ++side)
	{
		count = Cut(side, nearPlane, polygon, count, cut);
		polygon = cut;
	}
	return count >= 3;
}

bool object_draw_list::TriangleShows(const glm::mat4& viewProjection, float nearPlane, glm::vec3 eye, glm::vec3 a, glm::vec3 b,
                                     glm::vec3 c)
{
	return TriangleFacesEye(eye, a, b, c) &&
	       ClippedTriangleShows(nearPlane, viewProjection * glm::vec4(a, 1.0f), viewProjection * glm::vec4(b, 1.0f),
	                            viewProjection * glm::vec4(c, 1.0f));
}

uint8_t object_draw_list::SidesBeyond(float nearPlane, glm::vec4 p)
{
	uint8_t beyond = 0;
	for (size_t side = 0; side < k_Sides; ++side)
	{
		if (SideValue(side, nearPlane, p) < 0.0f)
		{
			beyond = static_cast<uint8_t>(beyond | (1u << side));
		}
	}
	return beyond;
}

bool object_draw_list::BlockShows(const glm::mat4& viewProjection, float nearPlane, glm::vec3 eye,
                                  std::span<const glm::vec3> corners, std::span<const uint16_t> triangles)
{
	return BlockShowsWith(
	    viewProjection, nearPlane, eye, corners.size(), [corners](uint16_t corner) { return corners[corner]; }, triangles);
}

bool object_draw_list::BlockListed(const glm::mat4& viewProjection, float nearPlane, glm::vec3 eye, const Block& block)
{
	return BlockInView(viewProjection, nearPlane, block) && BlockDistance(eye, block) < k_Range;
}

bool object_draw_list::SphereOnScreen(const View& view, glm::vec3 origin, glm::vec3 centre, float radius)
{
	const auto clip = view.viewProjection * glm::vec4(centre, 1.0f);
	if (clip.w + radius < view.nearPlane)
	{
		return false;
	}
	if (DistanceSquared(origin, view.eye) < radius * radius)
	{
		return true;
	}
	if (clip.w == 0.0f)
	{
		return true;
	}
	// The square's half side on the screen, as a share of half the screen's width and of half its height
	const float across = radius * view.focalHeight / clip.w;
	const float upAndDown = across * view.aspect;
	const float x = clip.x / clip.w;
	const float y = clip.y / clip.w;
	return x >= -1.0f - across && x <= 1.0f + across && y >= -1.0f - upAndDown && y <= 1.0f + upAndDown;
}

std::vector<NearBlock> object_draw_list::NearToFar(std::span<const NearBlock> inLandOrder)
{
	// Each block goes in front of the first already placed that is as far or further
	std::vector<NearBlock> sorted(inLandOrder.rbegin(), inLandOrder.rend());
	std::ranges::stable_sort(sorted, {}, &NearBlock::distance);
	return sorted;
}

Frame Clock::Next(uint32_t turn, glm::vec3 eye, glm::vec3 focus, bool forced)
{
	if (forced || !_rebuiltTurn.has_value() || *_rebuiltTurn + k_RebuildTurns < turn)
	{
		// A frame that makes the list draws it all, and leaves where the camera last moved from as it was
		_rebuiltTurn = turn;
		return {.rebuilt = true, .drawAll = true};
	}
	if (DistanceSquared(eye, _eye) > k_MovedSquared || DistanceSquared(focus, _focus) > k_MovedSquared)
	{
		_eye = eye;
		_focus = focus;
		return {.rebuilt = false, .drawAll = true};
	}
	return {.rebuilt = false, .drawAll = false};
}

void Clock::Reset()
{
	_rebuiltTurn.reset();
}
