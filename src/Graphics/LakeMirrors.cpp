/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LakeMirrors.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <utility>

#include <glm/common.hpp>
#include <glm/vec4.hpp>

#include "Graphics/LakeMirrorOptions.h"

using namespace openblack::graphics;

namespace
{
constexpr float k_CellSize = 10.0f;
constexpr float k_PieceSize = k_CellSize * static_cast<float>(lake_mirrors::k_PieceCells);
/// A point closer to the camera's plane than this is cut off: it is not in front of the camera
constexpr float k_InFront = 1e-3f;
} // namespace

std::vector<lake_mirrors::Level> lake_mirrors::GroupByLevel(lake_surface::Surface& surface)
{
	const size_t triangles = surface.indices.size() / 3;
	const auto levelOf = [&surface](size_t triangle) { return surface.vertices[surface.indices[triangle * 3]].level; };
	std::vector<size_t> order(triangles);
	std::iota(order.begin(), order.end(), size_t {0});
	std::ranges::stable_sort(order, {}, levelOf);

	std::vector<uint32_t> indices;
	indices.reserve(order.size() * 3);
	std::vector<Level> levels;
	// The pieces of the level that is made, by the square of each
	std::map<std::pair<int, int>, Piece> pieces;
	const auto finishLevel = [&levels, &pieces, &indices]() {
		if (levels.empty())
		{
			return;
		}
		auto& level = levels.back();
		level.indexCount = static_cast<uint32_t>(indices.size()) - level.firstIndex;
		level.pieces.reserve(pieces.size());
		std::ranges::transform(pieces, std::back_inserter(level.pieces), [](const auto& entry) { return entry.second; });
		pieces.clear();
	};
	for (const auto triangle : order)
	{
		const float level = levelOf(triangle);
		if (levels.empty() || levels.back().level != level)
		{
			finishLevel();
			levels.push_back(
			    {.level = level, .firstIndex = static_cast<uint32_t>(indices.size()), .indexCount = 0, .pieces = {}});
		}
		glm::vec2 low {surface.vertices[surface.indices[triangle * 3]].x, surface.vertices[surface.indices[triangle * 3]].z};
		glm::vec2 high = low;
		for (size_t corner = 0; corner < 3; ++corner)
		{
			const auto index = surface.indices[(triangle * 3) + corner];
			indices.push_back(index);
			const glm::vec2 point {surface.vertices[index].x, surface.vertices[index].z};
			low = glm::min(low, point);
			high = glm::max(high, point);
		}
		const auto middle = (low + high) * 0.5f;
		const std::pair square {static_cast<int>(std::floor(middle.x / k_PieceSize)),
		                        static_cast<int>(std::floor(middle.y / k_PieceSize))};
		const auto [found, added] = pieces.try_emplace(square, Piece {.minima = low, .maxima = high});
		if (!added)
		{
			found->second.minima = glm::min(found->second.minima, low);
			found->second.maxima = glm::max(found->second.maxima, high);
		}
	}
	finishLevel();
	surface.indices = std::move(indices);
	return levels;
}

std::optional<lake_mirrors::PixelRect> lake_mirrors::ScreenRect(std::span<const Piece> pieces, float level,
                                                                const glm::mat4& viewProjection, glm::u16vec2 viewport,
                                                                uint16_t margin)
{
	if (viewport.x == 0 || viewport.y == 0)
	{
		return std::nullopt;
	}
	glm::vec2 low {1.0f};
	glm::vec2 high {-1.0f};
	bool any = false;
	for (const auto& piece : pieces)
	{
		const std::array<glm::vec4, 4> corners {
		    viewProjection * glm::vec4(piece.minima.x, level, piece.minima.y, 1.0f),
		    viewProjection * glm::vec4(piece.maxima.x, level, piece.minima.y, 1.0f),
		    viewProjection * glm::vec4(piece.maxima.x, level, piece.maxima.y, 1.0f),
		    viewProjection * glm::vec4(piece.minima.x, level, piece.maxima.y, 1.0f),
		};
		// The part of the piece in front of the camera: each edge that crosses the camera's plane is cut there
		glm::vec2 pieceLow {std::numeric_limits<float>::max()};
		glm::vec2 pieceHigh {std::numeric_limits<float>::lowest()};
		bool inFront = false;
		const auto add = [&](const glm::vec4& clip) {
			const glm::vec2 ndc {clip.x / clip.w, clip.y / clip.w};
			pieceLow = glm::min(pieceLow, ndc);
			pieceHigh = glm::max(pieceHigh, ndc);
			inFront = true;
		};
		for (size_t i = 0; i < corners.size(); ++i)
		{
			const auto& from = corners.at(i);
			const auto& to = corners.at((i + 1) % corners.size());
			if (from.w >= k_InFront)
			{
				add(from);
			}
			if ((from.w >= k_InFront) != (to.w >= k_InFront))
			{
				const float share = (k_InFront - from.w) / (to.w - from.w);
				add(from + (to - from) * share);
			}
		}
		// A piece wholly beside, over or under the screen adds nothing
		if (!inFront || pieceHigh.x < -1.0f || pieceLow.x > 1.0f || pieceHigh.y < -1.0f || pieceLow.y > 1.0f)
		{
			continue;
		}
		low = any ? glm::min(low, pieceLow) : pieceLow;
		high = any ? glm::max(high, pieceHigh) : pieceHigh;
		any = true;
	}
	if (!any)
	{
		return std::nullopt;
	}
	low = glm::max(low, glm::vec2(-1.0f));
	high = glm::min(high, glm::vec2(1.0f));
	// Pixels from the top left: up in clip space is up on the screen
	const glm::vec2 size {viewport};
	const float left = std::floor((low.x + 1.0f) * 0.5f * size.x) - margin;
	const float right = std::ceil((high.x + 1.0f) * 0.5f * size.x) + margin;
	const float top = std::floor((1.0f - high.y) * 0.5f * size.y) - margin;
	const float bottom = std::ceil((1.0f - low.y) * 0.5f * size.y) + margin;
	const auto x0 = static_cast<uint16_t>(std::clamp(left, 0.0f, size.x));
	const auto x1 = static_cast<uint16_t>(std::clamp(right, 0.0f, size.x));
	const auto y0 = static_cast<uint16_t>(std::clamp(top, 0.0f, size.y));
	const auto y1 = static_cast<uint16_t>(std::clamp(bottom, 0.0f, size.y));
	if (x1 <= x0 || y1 <= y0)
	{
		return std::nullopt;
	}
	return PixelRect {.x = x0, .y = y0, .width = static_cast<uint16_t>(x1 - x0), .height = static_cast<uint16_t>(y1 - y0)};
}

bool lake_mirrors::Overlap(const PixelRect& first, const PixelRect& second)
{
	return first.x < second.x + second.width && second.x < first.x + first.width && first.y < second.y + second.height &&
	       second.y < first.y + first.height;
}

lake_mirrors::FramePlan lake_mirrors::Plan(std::span<const Level> levels, const glm::mat4& viewProjection, glm::vec3 eye,
                                           glm::u16vec2 viewport, std::span<const uint8_t> hidden, std::span<const Pass> before,
                                           const PlanSettings& settings)
{
	struct Seen
	{
		size_t level;
		PixelRect rect;
	};
	std::vector<Seen> seen;
	for (size_t i = 0; i < levels.size(); ++i)
	{
		// The water of a level shows its top only to a camera above it
		if (eye.y <= levels[i].level || (i < hidden.size() && hidden[i] != 0))
		{
			continue;
		}
		const auto rect = ScreenRect(levels[i].pieces, levels[i].level, viewProjection, viewport, settings.margin);
		if (!rect.has_value())
		{
			continue;
		}
		seen.push_back(
		    {.level = i,
		     .rect = settings.rects ? *rect : PixelRect {.x = 0, .y = 0, .width = viewport.x, .height = viewport.y}});
	}
	// The largest rectangles first, and the lower level first for the same size
	std::ranges::stable_sort(seen, std::ranges::greater {}, [](const Seen& each) { return each.rect.Area(); });

	FramePlan plan;
	plan.passOfLevel.resize(levels.size());
	const auto screen = static_cast<float>(viewport.x) * static_cast<float>(viewport.y);
	const auto small = [screen](const PixelRect& rect, float share) {
		return share > 0.0f && static_cast<float>(rect.Area()) < share * screen;
	};
	for (const auto& each : seen)
	{
		// A level near the level of a larger mirror shares that mirror, which then shows both rectangles
		if (settings.joinWithin > 0.0f)
		{
			const auto near = std::ranges::find_if(plan.passes, [&](const Pass& pass) {
				return std::abs(levels[pass.level].level - levels[each.level].level) < settings.joinWithin;
			});
			if (near != plan.passes.end())
			{
				const auto x0 = std::min(near->rect.x, each.rect.x);
				const auto y0 = std::min(near->rect.y, each.rect.y);
				const auto x1 = std::max(near->rect.x + near->rect.width, each.rect.x + each.rect.width);
				const auto y1 = std::max(near->rect.y + near->rect.height, each.rect.y + each.rect.height);
				near->rect = {
				    .x = x0, .y = y0, .width = static_cast<uint16_t>(x1 - x0), .height = static_cast<uint16_t>(y1 - y0)};
				plan.passOfLevel[each.level] = static_cast<uint8_t>(near - plan.passes.begin());
				continue;
			}
		}
		if (plan.passes.size() >= settings.slots)
		{
			break;
		}
		plan.passOfLevel[each.level] = static_cast<uint8_t>(plan.passes.size());
		plan.passes.push_back({.level = each.level, .rect = each.rect, .drawn = each.rect, .layer = 0, .kept = false});
	}
	for (auto& pass : plan.passes)
	{
		// A small mirror draws at half its size, from the top left of its rectangle
		if (small(pass.rect, settings.halfSizeBelow))
		{
			pass.drawn.width = static_cast<uint16_t>((pass.rect.width + 1) / 2);
			pass.drawn.height = static_cast<uint16_t>((pass.rect.height + 1) / 2);
		}
		// A small mirror that the frame before drew is kept on a frame that does not draw every mirror
		if (!settings.drawsAll && small(pass.rect, settings.everySecondFrameBelow))
		{
			const auto last =
			    std::ranges::find_if(before, [&pass](const Pass& old) { return old.level == pass.level && !old.kept; });
			if (last != before.end())
			{
				pass = *last;
				pass.kept = true;
			}
		}
	}
	// The kept mirrors stay in their layers. Each other mirror goes to the first layer where it overlaps none.
	for (size_t i = 0; i < plan.passes.size(); ++i)
	{
		auto& pass = plan.passes[i];
		if (pass.kept)
		{
			continue;
		}
		if (!settings.shareLayers)
		{
			pass.layer = static_cast<uint8_t>(i);
			continue;
		}
		uint8_t layer = 0;
		while (std::ranges::any_of(plan.passes, [&](const Pass& placed) {
			const bool earlier = placed.kept || (&placed < &pass);
			return earlier && &placed != &pass && placed.layer == layer && Overlap(placed.drawn, pass.drawn);
		}))
		{
			++layer;
		}
		pass.layer = layer;
	}
	return plan;
}

glm::mat4 lake_mirrors::RectTransform(const PixelRect& rect, glm::u16vec2 viewport)
{
	const glm::vec2 size {viewport};
	const float left = (2.0f * rect.x / size.x) - 1.0f;
	const float right = (2.0f * (rect.x + rect.width) / size.x) - 1.0f;
	const float top = 1.0f - (2.0f * rect.y / size.y);
	const float bottom = 1.0f - (2.0f * (rect.y + rect.height) / size.y);
	// Clip space x and y go from the rectangle's sides to -w and w
	glm::mat4 toRect(1.0f);
	toRect[0][0] = 2.0f / (right - left);
	toRect[3][0] = -(right + left) / (right - left);
	toRect[1][1] = 2.0f / (top - bottom);
	toRect[3][1] = -(top + bottom) / (top - bottom);
	return toRect;
}
