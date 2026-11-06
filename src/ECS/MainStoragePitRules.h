/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <span>

#include "Enums.h"

/// A town has one main storage pit: the pit that last started to work. Its people, its flags and its advisors use only
/// that pit.
namespace openblack::ecs::main_storage_pit
{

/// A building of the town that can take the place of the main storage pit when that pit goes
struct Candidate
{
	AbodeType type {AbodeType::General};
	/// It is built and its life is above the limit at which it stops to work
	bool functional {false};
	/// It is a storage pit
	bool storagePit {false};
};

/// The game looks for a kind of building with the civic part or the storage pit part. Every civic building matches.
[[nodiscard]] constexpr bool CanTakeOver(AbodeType type)
{
	return (static_cast<uint32_t>(type) & static_cast<uint32_t>(AbodeType::StoragePit)) != 0;
}

/// The building that becomes the main storage pit when the main pit goes. The list holds the other buildings of the
/// town, the newest first. The newest civic building that works gets the place. When that building is not a storage
/// pit, the town has no main storage pit, also when an older storage pit works.
[[nodiscard]] constexpr std::optional<size_t> Successor(std::span<const Candidate> newestFirst)
{
	for (size_t i = 0; i < newestFirst.size(); ++i)
	{
		const auto& candidate = newestFirst[i];
		if (CanTakeOver(candidate.type) && candidate.functional)
		{
			return candidate.storagePit ? std::optional(i) : std::nullopt;
		}
	}
	return std::nullopt;
}

} // namespace openblack::ecs::main_storage_pit
