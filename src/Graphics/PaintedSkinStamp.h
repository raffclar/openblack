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

namespace openblack::graphics::painted_skin
{
/// The painting that the textures of a painted skin hold. A load gives back the saved revision of each skin, and it can
/// give an entity its old id. Thus the stamp also keeps the count of the registry's resets.
struct Stamp
{
	uint32_t registryResets;
	uint32_t revision;

	bool operator==(const Stamp&) const = default;
};

/// The textures take the skin again when they hold no painting, or a painting of another world or revision
[[nodiscard]] constexpr bool NeedsUpload(const std::optional<Stamp>& held, const Stamp& current) noexcept
{
	return held != current;
}
} // namespace openblack::graphics::painted_skin
