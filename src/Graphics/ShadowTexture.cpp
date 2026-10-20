/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ShadowTexture.h"

#include <algorithm>
#include <utility>

namespace openblack::graphics::shadow_texture
{

namespace
{

/// The fixed point step of the edges
constexpr float k_One = 65536.0f;
/// The grid that the corners go across, before they stay on its texels
constexpr float k_Grid = 32.0f;
constexpr int32_t k_LastIndex = k_Texels - 1;
constexpr int32_t k_MaskSize = tree_shadows::k_MaskSide * tree_shadows::k_MaskSide;

/// A float to a whole number as the game does it: toward zero
[[nodiscard]] int32_t Truncate(float value)
{
	return static_cast<int32_t>(static_cast<int64_t>(value));
}

/// A whole number in 16.16 fixed point
[[nodiscard]] int32_t Fixed(int32_t value)
{
	return static_cast<int32_t>(static_cast<uint32_t>(value) << 16U);
}

/// A corner across or down the grid of texels, kept from k_FirstTexel to k_LastTexel
[[nodiscard]] float OnTexels(float value)
{
	if (!(value >= k_FirstTexel))
	{
		return k_FirstTexel;
	}
	return value > k_LastTexel ? k_LastTexel : value;
}

/// Puts the crossings of an edge into the rows that it spans. An edge that goes down the rows puts them into the list
/// of downward crossings, and an edge that goes up puts them into the other list. A flat edge puts its corners into
/// the two lists of its row: the corner to the left goes into the downward list.
void Edge(Canvas& canvas, const Corner& from, const Corner& to)
{
	if (from.row == to.row)
	{
		const auto row = static_cast<size_t>(from.row);
		const auto& left = from.column < to.column ? from : to;
		const auto& right = from.column < to.column ? to : from;
		// The game keeps a flat edge's texture coordinates as whole numbers, not in fixed point
		canvas.down.at(row) = {.column = left.column, .u = left.u, .v = left.v};
		canvas.up.at(row) = {.column = right.column, .u = right.u, .v = right.v};
		return;
	}
	const bool goesDown = from.row < to.row;
	const auto& start = goesDown ? from : to;
	const auto& end = goesDown ? to : from;
	auto& crossings = goesDown ? canvas.down : canvas.up;
	const int32_t rows = end.row + 1 - start.row;
	const float step = 1.0f / static_cast<float>(rows);
	const auto columnStep = Truncate(static_cast<float>(end.column - start.column) * step * k_One);
	const auto uStep = Truncate(static_cast<float>(end.u - start.u) * step * k_One);
	const auto vStep = Truncate(static_cast<float>(end.v - start.v) * step * k_One);
	auto column = Fixed(start.column);
	auto u = Fixed(start.u);
	auto v = Fixed(start.v);
	for (int32_t i = 0; i < rows; ++i)
	{
		crossings.at(static_cast<size_t>(start.row + i)) = {.column = column >> 16, .u = u, .v = v};
		column += columnStep;
		u += uStep;
		v += vStep;
	}
}

/// Whether the first corner stays above the second as the top: higher up, or on the same row and not to its left
[[nodiscard]] bool StaysOnTop(const Corner& first, const Corner& second)
{
	return first.row < second.row || (first.row == second.row && first.column >= second.column);
}

/// The top three bits of the mask's alpha at a texture's point in 16.16 fixed point, or nothing outside the mask
[[nodiscard]] uint16_t AlphaAt(const tree_shadows::AlphaMask& mask, int32_t u, int32_t v)
{
	const auto index = static_cast<int32_t>(static_cast<uint32_t>(v >> 10) & ~uint32_t {63}) + (u >> 16);
	if (index < 0 || index >= k_MaskSize)
	{
		return 0;
	}
	return static_cast<uint16_t>((mask.at(static_cast<size_t>(index)) & 0xE0U) << 7U);
}

} // namespace

glm::vec3 LightAbove(const glm::vec3& origin)
{
	return {origin.x, origin.y + k_LightHeight, origin.z};
}

Corner CornerOf(const glm::vec3& local, const glm::vec2& uv, const glm::mat4& model, const GroundShadow::Footprint& footprint,
                const GroundShadow::Cast& cast)
{
	// The vertex in the world, each axis summed in the game's order
	const float x = ((local.x * model[0].x) + (local.z * model[2].x) + (local.y * model[1].x)) + model[3].x;
	const float y = ((local.x * model[0].y) + (local.z * model[2].y) + (local.y * model[1].y)) + model[3].y;
	const float z = ((local.x * model[0].z) + (local.z * model[2].z) + (local.y * model[1].z)) + model[3].z;
	const float across = k_Grid / (footprint.high.x - footprint.low.x);
	const float down = k_Grid / (footprint.high.y - footprint.low.y);
	const float levelBelowLight = cast.level - cast.light.y;
	const float along = levelBelowLight / (y - cast.light.y);
	const float column = OnTexels(((((x - cast.light.x) * along) + cast.light.x) - footprint.low.x) * across);
	const float row = OnTexels(((((z - cast.light.z) * along) + cast.light.z) - footprint.low.y) * down);
	return {.column = Truncate(column),
	        .row = Truncate(row),
	        .u = Truncate(uv.x * k_MaskPlaces),
	        .v = Truncate(uv.y * k_MaskPlaces)};
}

void Fill(Canvas& canvas, const tree_shadows::AlphaMask& mask, std::array<Corner, 3> corners)
{
	for (auto& corner : corners)
	{
		corner.column = std::clamp(corner.column, 0, k_LastIndex);
		corner.row = std::clamp(corner.row, 0, k_LastIndex);
	}
	auto [a, b, c] = corners;
	// The top corner first
	if (StaysOnTop(a, b))
	{
		if (!StaysOnTop(a, c))
		{
			std::swap(a, c);
		}
	}
	else if (StaysOnTop(b, c))
	{
		std::swap(a, b);
	}
	else
	{
		std::swap(a, c);
	}
	// Then the other two in the order that the game gives them
	if (b.column > c.column)
	{
		std::swap(b, c);
	}
	else if (b.column == c.column && ((a.column <= b.column) ? (b.row < c.row) : (b.row > c.row)))
	{
		std::swap(b, c);
	}
	const int32_t top = a.row;
	const int32_t bottom = std::max(b.row, c.row);
	Edge(canvas, a, b);
	Edge(canvas, b, c);
	Edge(canvas, c, a);

	for (int32_t row = top; row <= bottom; ++row)
	{
		const auto& first = canvas.down.at(static_cast<size_t>(row));
		const auto& second = canvas.up.at(static_cast<size_t>(row));
		auto count = second.column - first.column + 1;
		const auto* from = &first;
		const auto* to = &second;
		if (count < 0)
		{
			std::swap(from, to);
			count = first.column - second.column + 1;
		}
		if (count == 0)
		{
			continue;
		}
		const auto uStep = (to->u - from->u) / count;
		const auto vStep = (to->v - from->v) / count;
		auto u = from->u;
		auto v = from->v;
		auto texel = (static_cast<size_t>(row) * k_Texels) + static_cast<size_t>(from->column);
		for (int32_t i = 0; i < count; ++i, ++texel)
		{
			if (texel < canvas.texels.size())
			{
				canvas.texels.at(texel) |= AlphaAt(mask, u, v);
			}
			u += uStep;
			v += vStep;
		}
	}
}

std::array<uint8_t, k_Texels * k_Texels> Levels(const Canvas& canvas, uint8_t alpha)
{
	std::array<uint8_t, k_Texels * k_Texels> levels {};
	const auto at = [&canvas](size_t row, size_t column) -> uint32_t { return canvas.texels.at((row * k_Texels) + column); };
	for (size_t row = 1; row < k_Texels - 1; ++row)
	{
		for (size_t column = 1; column < k_Texels - 1; ++column)
		{
			const auto sum = at(row, column) + at(row, column + 1) + at(row + 1, column) + at(row + 1, column + 1);
			const auto level = static_cast<uint8_t>(((sum >> 2U) & 0xF000U) >> 12U);
			levels.at((row * k_Texels) + column) = GroundShadow::Level(level, alpha);
		}
	}
	return levels;
}

} // namespace openblack::graphics::shadow_texture
