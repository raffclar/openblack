/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

namespace openblack::debug
{

/// The time that the thread that calls this was on a processor since it started, in milliseconds. A step that takes
/// much more wall-clock time than this waited: for the renderer, or for the system, which gave the processor to other
/// programs. A web browser does not give this time: its thread clock is the wall clock, which also counts the wait for
/// the display frame. Then there is no value.
[[nodiscard]] std::optional<double> ThreadCpuMilliseconds();

/// The processor time from one reading of ThreadCpuMilliseconds to a later reading on the same thread, when both are known
[[nodiscard]] constexpr std::optional<float> CpuMillisecondsBetween(std::optional<double> start, std::optional<double> end)
{
	if (!start.has_value() || !end.has_value())
	{
		return std::nullopt;
	}
	return static_cast<float>(*end - *start);
}

} // namespace openblack::debug
