/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

/// When a town centre has its working parts: its totem, its stone hands and its belief symbols. It gets them only when
/// it is finished, never while its town puts it up or puts it up again.
namespace openblack::ecs::town_centre_parts
{

/// A town centre made with so much of it built has its parts at once only when all of it is built
[[nodiscard]] constexpr bool PartsWhenMade(float built)
{
	return built >= 1.0f;
}

/// A town centre its town finishes gets its parts, unless it has its totem already
[[nodiscard]] constexpr bool PartsWhenFinished(bool isTownCentre, bool hasTotem)
{
	return isTownCentre && !hasTotem;
}

/// A town centre shows its belief symbols when it is not under construction, or when all of it is built
[[nodiscard]] constexpr bool ShowsBeliefSymbols(std::optional<float> builtWhileUnderConstruction)
{
	return !builtWhileUnderConstruction.has_value() || *builtWhileUnderConstruction >= 1.0f;
}

} // namespace openblack::ecs::town_centre_parts
