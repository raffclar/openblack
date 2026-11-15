/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string_view>

namespace openblack::error_reports
{

/// The DSN of the build, from the cache value OPENBLACK_SENTRY_DSN, or empty. Only a build with the reports
/// (OPENBLACK_SENTRY) has this function.
[[nodiscard]] std::string_view BuildDsn() noexcept;

} // namespace openblack::error_reports
