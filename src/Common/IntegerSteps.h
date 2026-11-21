/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <concepts>
#include <cstddef>

/// Integer steps that the code does on purpose. These functions make the step clear to the reader and to clang-tidy.
/// They do not change the result.
namespace openblack
{

/// The index of an element in a container. The caller calculates the index in its own integer type, which has the
/// same width as in the game. This function only converts the result to a container index.
template <std::integral T>
[[nodiscard]] constexpr size_t ToIndex(T index) noexcept
{
	return static_cast<size_t>(index);
}

/// The whole part of an integer division, with the remainder dropped. Use it where the game divides whole numbers
/// and then uses the quotient as a float.
template <std::integral A, std::integral B>
[[nodiscard]] constexpr auto WholeQuotient(A dividend, B divisor) noexcept
{
	return dividend / divisor;
}

} // namespace openblack
