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
#include <string_view>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

/// The lookups of the map script's villager commands (feature script commands 15..18), apart so that they can be
/// tested.
namespace openblack::lhscriptx::villager_commands
{

/// The villager info text lookup's "none": 0x54, one past the 84 villager info records.
/// (pending) what the original reads there; openblack makes no villager
inline constexpr int32_t k_NoVillagerInfo = 0x54;

/// Each of the 9 tribes whose name starts the text, case-insensitive, followed by '_'; then that tribe's 7 villager
/// names against the rest (case-insensitive): tribe x 7 + villager. k_NoVillagerInfo when none
[[nodiscard]] int32_t VillagerInfoFromText(std::string_view text);

/// The tribe override of CREATE_VILLAGER / _POS: the FIRST record of the 84 whose tribe and number match; nullopt
/// when none. Not GVillagerInfo::Find of InfoConstants.cpp, which keeps the last match and throws
[[nodiscard]] std::optional<VillagerInfo> FindVillagerInfo(Tribe tribe, VillagerNumber number);

struct AbodeAt
{
	entt::entity abode {entt::null}; ///< null when none, or full
	entt::entity town {entt::null};  ///< the found abode's town, also when it is full
};

/// CREATE_VILLAGER / _POS: every player then the neutral one, its towns, their abodes (newest first): the first whose
/// door is in the cell of `position` (only the MapCoords' cells are compared). Its town; the abode itself only while
/// it has room: MaxVillagers - the count == 0 -> none.
/// (approximate) the cell of `position`: map_coords::FromMetres (truncated m x 6553.6f); the original truncates
/// atof x 65536 / 10, which can differ by one unit at a cell edge; and the door point is openblack's
[[nodiscard]] AbodeAt FindAbodeAt(const glm::vec3& position);

} // namespace openblack::lhscriptx::villager_commands
