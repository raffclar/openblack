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
#include <string>
#include <string_view>
#include <vector>

#include "ECS/SyncedState.h"

/// The fields that part the worlds of a desync. The record of a desync keeps the saved world of the server and of the
/// player at the turn of the capture. The capture names the entities of each part (a component) that differ, and these
/// rules read the rows of these entities from both worlds and name the fields whose bytes differ. The rules keep no
/// state, so that the server and the tests use them both.
namespace openblack::server::desync_fields
{

/// The most fields that the words of one entity name
inline constexpr size_t k_MostFieldsOfEntity = 8;

/// The fields of one component of one entity that differ, in the order of the fields in the save
struct EntityFields
{
	uint32_t entity {0};
	std::vector<std::string> fields;

	[[nodiscard]] bool operator==(const EntityFields&) const = default;
};

/// The fields of the component that differ for each entity, in the order of the entities. The fields that only show the
/// world (synced_state::PresentationParts) are left out. An entity with no row in one of the worlds, or with no field
/// that differs, is left out. None when a world does not read.
[[nodiscard]] std::optional<std::vector<EntityFields>>
DifferentFields(std::span<const uint8_t> serverWorld, std::span<const uint8_t> playerWorld, std::string_view component,
                std::span<const uint32_t> entities, std::span<const synced_state::PresentationPart> presentation);

/// The fields in words, for the reports, such as "entity 1252: alignment, size"
[[nodiscard]] std::string Describe(std::span<const EntityFields> fields);

/// The first field that differs as "Component.field", for a tag that a search can find, or none
[[nodiscard]] std::optional<std::string> FirstField(std::string_view component, std::span<const EntityFields> fields);

} // namespace openblack::server::desync_fields
