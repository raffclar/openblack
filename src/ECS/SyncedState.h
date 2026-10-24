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

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <entt/meta/fwd.hpp>

/// The synced state of the world: the part of the saved world that each computer of a game keeps the same, turn by turn.
/// The frames draw the rest. Two computers that play the same turns at other frame rates have other values in the rest,
/// so the hash of the synced state leaves it out.
namespace openblack::synced_state
{

/// A part of the saved world that only shows the world on one computer. The frames change it, and the turns do not read
/// it.
struct PresentationPart
{
	/// The component, or "*" for each component of the entity
	std::string_view component;
	/// The field, or empty for the whole component
	std::string_view field;
	/// Only the entities that have this component, or empty for each entity
	std::string_view marker;
};

/// The parts of the saved world of the game that only show the world on one computer
[[nodiscard]] std::span<const PresentationPart> PresentationParts();

/// Whether a field of a component, or the whole component when the field is empty, is in the parts for each entity
[[nodiscard]] bool IsPresentationOnly(std::span<const PresentationPart> parts, std::string_view component,
                                      std::string_view field);

/// The hash of the synced state, and the hash of each of its parts
struct Hash
{
	uint64_t hash {0};
	/// The list of the entities, then each component by its name, and with the fields each field as
	/// "component.field". The hash mixes the entities and the components, not the fields.
	std::vector<std::pair<std::string, uint64_t>> parts;
};

/// The hash of the synced state of a world: its entities and every saved component, without the presentation parts.
/// With byField, the parts also keep the hash of each field.
[[nodiscard]] Hash HashOf(const entt::registry& registry, const entt::meta_ctx& context,
                          std::span<const PresentationPart> presentation, bool byField = false);

/// The hash of the synced state mixed with values of the world that the systems keep outside the entities
[[nodiscard]] uint64_t MixValues(uint64_t stateHash, std::span<const int64_t> values);

/// The checksum of a turn: the hash of the synced state, mixed with the synced seed and the count of synced draws
[[nodiscard]] uint64_t Checksum(uint64_t stateHash, uint32_t seed, uint64_t draws);

} // namespace openblack::synced_state
