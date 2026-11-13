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

namespace openblack::graphics
{

/// How a view orders its draws, as bgfx's view modes do
enum class ViewOrder : uint8_t
{
	/// By program, then by depth, ties in the order they came
	ByProgram,
	/// In the order they came
	Sequential,
	/// The nearest (smallest depth) first, ties in the order they came
	DepthAscending,
	/// The farthest (greatest depth) first, ties in the order they came
	DepthDescending,
};

/// Whether the draws of equal depth in a view keep the order that they came in. The game puts a new thing after the
/// things of the same key in its sorted list, thus it draws them in the order that it gets them. Without this rule, bgfx
/// orders these draws by their programs, and the number of a program is not the same in each build.
[[nodiscard]] constexpr bool TiesInSubmitOrder(ViewOrder order) noexcept
{
	return order == ViewOrder::DepthAscending || order == ViewOrder::DepthDescending;
}

} // namespace openblack::graphics
