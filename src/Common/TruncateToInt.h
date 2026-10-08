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

#include <type_traits>

namespace openblack
{

// the merged copies returned int
static_assert(std::is_same_v<int32_t, int>);

/// Float to int, truncating towards zero
[[nodiscard]] constexpr int32_t TruncateToInt(float value)
{
	return static_cast<int32_t>(value);
}

} // namespace openblack
