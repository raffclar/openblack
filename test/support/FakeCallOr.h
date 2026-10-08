/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <memory>
#include <utility>

namespace openblack::test::detail
{
/// The fake's function when it is set, else the fallback service's method
template <typename Interface, typename Fn, typename Method, typename... Args>
decltype(auto) CallOr(const Fn& fn, const std::shared_ptr<Interface>& fallback, Method method, Args&&... args)
{
	if (!fn && fallback != nullptr)
	{
		return std::invoke(method, *fallback, std::forward<Args>(args)...);
	}
	return fn(std::forward<Args>(args)...);
}
} // namespace openblack::test::detail
