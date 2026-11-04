/*******************************************************************************
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

#include "ECS/Components/Mobile.h"

namespace openblack
{
struct GMobileObjectInfo;
namespace v120
{
struct InfoConstants;
}
using InfoConstants = v120::InfoConstants;
} // namespace openblack

/// The record of the info tables that a loose object takes its values from, and the row that the land scripts give for
/// it
namespace openblack::ecs::mobile_object_record
{

/// The row that the land scripts give for the record of the football. The game counts the rows of a loose object from
/// the start of the loose objects' table, and the record of the football is before that table. Thus a football that
/// goes through a vortex writes this row, and the next land makes a loose object of the football's record from it.
constexpr int32_t k_FootballRow = -3324;

/// The loose object that the land scripts make for a row: a row of the loose objects' table, or the record of the
/// football. None for another row.
[[nodiscard]] std::optional<components::MobileObject> FromRow(const InfoConstants& info, int32_t row);

/// The row that the land scripts give for the loose object
[[nodiscard]] int32_t Row(const components::MobileObject& mobile);

/// The record that the loose object takes its values from. None when its type is outside the loose objects' table.
[[nodiscard]] const GMobileObjectInfo* Of(const InfoConstants& info, const components::MobileObject& mobile);

} // namespace openblack::ecs::mobile_object_record
