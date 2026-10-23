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
};

/// Whether a hash of the world keeps a part of a component. The hash asks first about the whole component (no field and
/// no entity), then about the row of each entity (no field), then about each field of the row. The field is "*" for a
/// component that saves as one value.
using KeepInHash = std::function<bool(std::string_view component, std::string_view field, entt::entity entity)>;

/// Makes the hashes of the parts of the world, with no save in memory of the whole world. With a keep function, the hash
/// of a component leaves out the parts that the function does not keep. The hash of each component goes over its live
/// rows in the order of the storage. With byField, the hashes also keep each field of each component on its own.
/// Tools: gets the bytes that the hash takes of each field of a row (the field is "*" for a component that saves as one
/// value), to compare two builds' worlds value by value
using RowVisitor = std::function<void(std::string_view component, entt::entity entity, std::string_view field,
                                      std::span<const uint8_t> bytes)>;

[[nodiscard]] WorldHashes HashWorld(const entt::registry& registry, const entt::meta_ctx& context, const KeepInHash& keep = {},
                                    bool byField = false, const RowVisitor& visit = {});

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
