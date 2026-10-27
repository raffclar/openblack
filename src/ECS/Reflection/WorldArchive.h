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

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <entt/meta/context.hpp>

#include "BinaryArchive.h"
#include "ComponentReflection.h"

namespace openblack
{
class WorkerPool;
}

/// The archive of a whole world: every entity of a registry and every registered component of each entity. The archive
/// keeps the entities with their versions and the list of free entities, so that a load into an empty registry gives
/// the same entities. Each component has a block with its name and its length, and each field of a component has its
/// name and its length. Thus a newer build reads an older archive: it skips a component or a field that it does not
/// know, and keeps the default value of a field that the archive does not have.
namespace openblack::ecs::reflection
{

inline constexpr std::array<char, 4> k_WorldArchiveMagic {'O', 'B', 'E', 'W'};
inline constexpr uint32_t k_WorldArchiveVersion = 1;

/// What a save of a world did not keep
struct WorldSaveReport
{
	size_t entities {0};
	size_t components {0};
	/// The components that the registry has but that have no registration, so the archive does not keep them
	std::vector<std::string> unregisteredComponents;
	/// The values in registered components that the archive cannot keep
	std::set<UnsavedValue> unsavedValues;
};

enum class WorldLoadMode : uint8_t
{
	/// The registry has no live entity. The load gives each entity the same identifier and version that it had.
	Exact,
	/// The registry keeps its entities. The load makes a new entity for each saved entity, and changes each reference to
	/// a saved entity into a reference to the new entity.
	Merge,
};

struct WorldLoadReport
{
	size_t entities {0};
	size_t components {0};
	/// The components of the archive that this build does not know
	std::vector<std::string> unknownComponents;
	/// The fields of the archive that this build does not know, as "Component.field"
	std::vector<std::string> unknownFields;
	/// The fields that did not read with the length that the archive gives, as "Component.field"
	std::vector<std::string> damagedFields;
	/// The components that the load could not add to an entity
	std::vector<std::string> refusedComponents;
};

struct WorldLoadResult
{
	/// How the saved entities became the entities of the registry
	EntityRemap remap;
	WorldLoadReport report;
};

/// Writes every entity of the registry and every registered component of each entity
[[nodiscard]] std::vector<uint8_t> SaveWorld(const entt::registry& registry, const entt::meta_ctx& context,
                                             WorldSaveReport* report = nullptr);

/// The hashes of the parts of a world: the list of the entities, and the rows of each registered component, with the
/// bytes that SaveWorld writes for each row.
struct WorldHashes
{
	uint64_t entities {0};
	/// Each registered component that has an entity, by its name, in the order of the names
	std::vector<std::pair<std::string, uint64_t>> components;
	/// When asked, each field of each registered component as "component.field", in the order of the components. A
	/// component that saves as one value has the field "*".
	std::vector<std::pair<std::string, uint64_t>> fields;
	/// The components that have an entity but no registration, so the hash leaves them out: a change in them is never seen.
	/// The names are in order.
	std::vector<std::string> unregistered;
};

/// A part of the world that a hash does not include: a component or one field of it, of each entity or only of the
/// entities that have a marker component
struct HashLeaveOut
{
	/// The component, or "*" for each component
	std::string_view component;
	/// The field, or empty for the whole component. The field is "*" for a component that saves as one value.
	std::string_view field;
	/// The storage of the marker component, or null for each entity
	const entt::sparse_set* marker {nullptr};
};

/// Makes the hashes of the parts of the world, with no save in memory of the whole world. The hash of a component does
/// not include the parts in leaveOut. The hash of each component goes over its live rows in the order of the storage.
/// With byField, the hashes also keep each field of each component on its own. With a pool, the threads of the pool
/// share the components, and the hashes are the same.
/// Tools: gets the bytes that the hash takes of each field of a row (the field is "*" for a component that saves as one
/// value), to compare two builds' worlds value by value
using RowVisitor = std::function<void(std::string_view component, entt::entity entity, std::string_view field,
                                      std::span<const uint8_t> bytes)>;

/// The names of the components that have an entity but no registration, in order. A save and the hash of the world leave
/// these out, so the game must have none.
[[nodiscard]] std::vector<std::string> UnregisteredStorages(const entt::registry& registry, const entt::meta_ctx& context);
/// Tools: the bytes that a hash took of each field of each row, to find the field that parts two computers. A build
/// without the developer tools does not fill it.
struct WorldRows
{
	struct Row
	{
		entt::entity entity {};
		/// The index of the field in fields
		uint32_t field {0};
		/// The first byte of the value in bytes
		uint32_t offset {0};
		uint32_t size {0};
		/// The hash leaves the field out: a part that only shows the world on one computer
		bool presentation {false};
	};
	/// Each field as "component.field", in the order that the hash first took it. A component that saves as one value
	/// has the field "*".
	std::vector<std::string> fields;
	/// Each field of each row, in the order of the hash: the components in the order of their names, the rows in the
	/// order of the storage. The fields that the hash leaves out come before the kept fields of their component.
	std::vector<Row> rows;
	std::vector<uint8_t> bytes;
};

[[nodiscard]] WorldHashes HashWorld(const entt::registry& registry, const entt::meta_ctx& context,
                                    std::span<const HashLeaveOut> leaveOut = {}, bool byField = false,
                                    const RowVisitor& visit = {}, WorkerPool* pool = nullptr, WorldRows* rows = nullptr);

/// The entities and the components of a registry as the game's thread took them, for a writer on another thread to
/// encode while the game goes on. Each component that can be copied safely is copied; the game's thread encodes the
/// others at once.
struct WorldSnapshot
{
	struct Storage
	{
		std::string name;
		entt::meta_type type;
		/// The copied rows, or the block of the storage that the game's thread already encoded
		std::optional<StorageCopy> copy;
		std::vector<uint8_t> encoded;
		uint32_t encodedRows {0};
	};
	std::vector<entt::entity> entities;
	size_t inUse {0};
	std::vector<Storage> storages;
	std::vector<std::string> unregisteredComponents;
	std::set<UnsavedValue> unsavedValues;
};

/// Takes a snapshot of the registry on the game's thread
[[nodiscard]] std::shared_ptr<const WorldSnapshot> SnapshotWorld(const entt::registry& registry, const entt::meta_ctx& context);

/// Writes a snapshot as SaveWorld writes the registry, with the same bytes. Any thread can call it while the registry
/// changes.
[[nodiscard]] std::vector<uint8_t> SaveWorld(const WorldSnapshot& snapshot, const entt::meta_ctx& context,
                                             WorldSaveReport* report = nullptr);

/// Reads an archive of a world into the registry. Returns nothing when the bytes are not an archive of a world, when the
/// archive has damage, or when the mode is Exact and the registry has a live entity.
[[nodiscard]] std::optional<WorldLoadResult> LoadWorld(entt::registry& registry, const entt::meta_ctx& context,
                                                       std::span<const uint8_t> bytes, WorldLoadMode mode);

/// A section of a block with a name, such as the running state of one system
struct NamedSection
{
	std::string name;
	std::vector<uint8_t> bytes;

	bool operator==(const NamedSection&) const = default;
};

/// Writes sections with their names and lengths, so that a reader can skip a section that it does not know
[[nodiscard]] std::vector<uint8_t> SaveSections(std::span<const NamedSection> sections);

/// Reads the sections that SaveSections wrote. Returns nothing when the bytes have damage.
[[nodiscard]] std::optional<std::vector<NamedSection>> LoadSections(std::span<const uint8_t> bytes);

/// Writes one value with its registered fields, such as the context of a registry
template <typename Type>
[[nodiscard]] std::vector<uint8_t> SaveValue(const entt::meta_ctx& context, const Type& value,
                                             std::set<UnsavedValue>* unsaved = nullptr)
{
	BinaryWriter out(context);
	out.SetPlace(entt::type_id<Type>().name(), "");
	Save(out, value);
	if (unsaved != nullptr)
	{
		unsaved->insert(out.Unsaved().begin(), out.Unsaved().end());
	}
	return std::move(out.Bytes());
}

/// Reads one value that SaveValue wrote. Returns whether it read all of the bytes.
template <typename Type>
[[nodiscard]] bool LoadValue(const entt::meta_ctx& context, std::span<const uint8_t> bytes, Type& value,
                             const EntityRemap& remap)
{
	BinaryReader in(context, bytes, remap);
	Load(in, value);
	return !in.Failed() && in.Remaining() == 0;
}

} // namespace openblack::ecs::reflection
