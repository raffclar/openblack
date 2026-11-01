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

#include <functional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <entt/meta/fwd.hpp>

#include "ECS/Reflection/WorldArchive.h"

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
	/// The components that have an entity but no registration. The hash leaves them out, so a change in them is never seen.
	/// A correct game has none.
	std::vector<std::string> unregistered;
};

/// What the logs and the tools say before the names of the unregistered components. The check tools look for these words.
inline constexpr std::string_view k_UnregisteredWords = "unregistered component storage: ";

/// The line that tells of unregistered components: the words, then the names with commas between them
[[nodiscard]] std::string UnregisteredLine(std::span<const std::string> names);

/// Remembers the unregistered components that were told of, so that each name is told of once
class UnregisteredReporter
{
public:
	/// The names that this reporter has not seen yet, in the order that they came. It keeps them as seen.
	[[nodiscard]] std::vector<std::string> Fresh(std::span<const std::string> names);

private:
	std::set<std::string, std::less<>> _seen;
};

/// The hash of the synced state of a world: its entities and every saved component, without the presentation parts.
/// With byField, the parts also keep the hash of each field. With a pool, its threads share the work. Tools: with rows,
/// it also keeps the bytes of each field of each row that the hash took. With the memo of the last hash, the hash
/// mixes again only the rows that changed, and the hash is the same.
[[nodiscard]] Hash HashOf(const entt::registry& registry, const entt::meta_ctx& context,
                          std::span<const PresentationPart> presentation, bool byField = false,
                          const ecs::reflection::RowVisitor& visit = {}, WorkerPool* pool = nullptr,
                          ecs::reflection::WorldRows* rows = nullptr, ecs::reflection::HashMemo* memo = nullptr);

/// Tools: the hash of the order of the rows of each component that the hash of the synced state keeps values of. The order
/// includes the rows of the presentation parts. A frame must not change it. The game turns go through the rows in this
/// order, and when a turn removes a row, the last row moves to the place of that row.
[[nodiscard]] std::vector<std::pair<std::string, uint64_t>>
RowOrdersOf(const entt::registry& registry, const entt::meta_ctx& context, std::span<const PresentationPart> presentation);

/// The hash of the synced state mixed with values of the world that the systems keep outside the entities
[[nodiscard]] uint64_t MixValues(uint64_t stateHash, std::span<const int64_t> values);

/// The checksum of a turn: the hash of the synced state, mixed with the synced seed and the count of synced draws
[[nodiscard]] uint64_t Checksum(uint64_t stateHash, uint32_t seed, uint64_t draws);

/// What the checksum of a turn must give in the turn itself
struct ChecksumNeeds
{
	/// The frame lets the hash wait for its draw
	bool besideDraw {false};
	/// Tools: the checksum keeps the hash of each part
	bool keepParts {false};
	/// Tools: the checksum of this turn keeps a dump of rows
	bool dumpThisTurn {false};
	/// Tools: each checksum keeps the bytes of its rows
	bool rowDump {false};
	/// Tools: the system writes the world state after the checksum
	bool worldStates {false};
};

/// The hash of the turn can wait for the draw of the frame: the frame lets it, and no tool needs the hash in the turn
[[nodiscard]] bool HashCanWait(const ChecksumNeeds& needs);

} // namespace openblack::synced_state
