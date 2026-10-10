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

#include <array>

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
