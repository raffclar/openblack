/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Graphics/LakeSurface.h"

/// The mirrors of the lakes above the sea of a generated land. Each level of water in view gets a mirror of the world
/// at that level, as the sea's reflection is the mirror of the world at the sea's level. The lakes that join have one
/// level, so they share one mirror. A mirror draws only the rectangle of the screen that its water can cover.
namespace openblack::graphics::lake_mirrors
{
/// A part of the water of one level, as a rectangle on the ground: the water is flat at its level
struct Piece
{
	glm::vec2 minima;
	glm::vec2 maxima;
};

/// The water of one level: its triangles in the surface's indices, and the pieces that hold all of them
struct Level
{
	float level;
	uint32_t firstIndex;
	uint32_t indexCount;
	std::vector<Piece> pieces;
};

/// A rectangle of the screen in pixels, from the top left
struct PixelRect
{
	uint16_t x;
	uint16_t y;
	uint16_t width;
	uint16_t height;

	[[nodiscard]] uint32_t Area() const { return static_cast<uint32_t>(width) * height; }
	bool operator==(const PixelRect&) const = default;
};

/// The mirror that a level gets in a frame
struct Pass
{
	/// The index of the level that the mirror is at, in the levels that the plan was made from
	size_t level;
	/// The rectangle of the screen that the mirror shows
	PixelRect rect;
	/// The rectangle of the layer that the mirror draws into: the same rectangle, or a smaller one at its top left
	PixelRect drawn;
	uint8_t layer;
	/// The mirror of the frame before is used again, not drawn
	bool kept;
};

/// The mirrors of a frame, and the mirror of each level
struct FramePlan
{
	std::vector<Pass> passes;
	/// The index in passes of each level's mirror, or none
	std::vector<std::optional<uint8_t>> passOfLevel;
};

/// The side of a piece, in cells of the map
inline constexpr int k_PieceCells = 8;

/// Puts the triangles of the surface in the order of their levels, the lowest first, and gives each level its triangles
/// and pieces. A piece is the part of the water of one level in a square of k_PieceCells cells.
[[nodiscard]] std::vector<Level> GroupByLevel(lake_surface::Surface& surface);

/// The rectangle of the screen that the water of a level can cover, a margin wider on each side and inside the screen.
/// None when no piece is in front of the camera and on the screen.
[[nodiscard]] std::optional<PixelRect> ScreenRect(std::span<const Piece> pieces, float level, const glm::mat4& viewProjection,
                                                  glm::u16vec2 viewport, uint16_t margin);

/// Whether two rectangles share a pixel
[[nodiscard]] bool Overlap(const PixelRect& first, const PixelRect& second);

struct PlanSettings
{
	/// The most mirrors in one frame
	uint8_t slots;
	/// The pixels round the rectangle of the water that the mirror also draws
	uint16_t margin;
	/// Each mirror draws only the rectangle of its water. When false, each one draws the whole screen.
	bool rects;
	/// The mirrors whose rectangles do not overlap share the layers of one target. When false, each one has a layer.
	bool shareLayers;
	/// These change the look (see Options): a smaller mirror for a small rectangle, one mirror for levels near each
	/// other, and a mirror every second frame for a small rectangle. 0 for none.
	float halfSizeBelow {0.0f};
	float joinWithin {0.0f};
	float everySecondFrameBelow {0.0f};
	/// With everySecondFrameBelow, whether this frame draws every mirror
	bool drawsAll {true};
};

/// The mirrors of a frame. A level gets no mirror when the camera is at or under its level, when its water is not on the
/// screen, or when `hidden` says that nothing of its water showed. When more levels than slots are on the screen, the
/// levels with the largest rectangles get the mirrors. Each mirror goes to the first layer where no rectangle overlaps
/// the rectangle that it draws. `before` is the plan of the frame before, for the mirrors that it keeps.
[[nodiscard]] FramePlan Plan(std::span<const Level> levels, const glm::mat4& viewProjection, glm::vec3 eye,
                             glm::u16vec2 viewport, std::span<const uint8_t> hidden, std::span<const Pass> before,
                             const PlanSettings& settings);

/// The change of clip space that draws only a rectangle of the screen, the rectangle filling the view. After a
/// projection over the whole screen, it gives each point the same pixel.
[[nodiscard]] glm::mat4 RectTransform(const PixelRect& rect, glm::u16vec2 viewport);
} // namespace openblack::graphics::lake_mirrors
