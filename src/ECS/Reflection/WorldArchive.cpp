/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorldArchive.h"

#include <cstring>

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <entt/meta/meta.hpp>
#include <entt/meta/resolve.hpp>

#include "Common/WorkerPool.h"
#include "ComponentReflection.h"

using namespace openblack::ecs::reflection;

namespace
{

/// A field of a component that the archive keeps
struct KeptField
{
	entt::meta_data data;
	const FieldInfo* info {nullptr};
};

/// The name of the block of a component that has a codec of its own: one value for the whole component
constexpr std::string_view k_WholeComponent = "*";

[[nodiscard]] const TypeCodec* ComponentCodec(const entt::meta_type& type)
{
	const ComponentInfo* info = type.custom();
	return info != nullptr && info->codec.save != nullptr ? &info->codec : nullptr;
}

[[nodiscard]] std::vector<KeptField> KeptFields(const entt::meta_type& type)
{
	std::vector<KeptField> fields;
	for (const auto& [id, data] : type.data())
	{
		const FieldInfo* info = data.custom();
		if (info != nullptr && info->save != nullptr && info->load != nullptr)
		{
			fields.push_back({.data = data, .info = info});
		}
	}
	return fields;
}

void WriteName(BinaryWriter& out, std::string_view name)
{
	out.WriteSize(name.size());
	out.Write({reinterpret_cast<const uint8_t*>(name.data()), name.size()});
}

[[nodiscard]] std::optional<std::string> ReadName(BinaryReader& in)
{
	std::string name;
	Load(in, name);
	return in.Failed() ? std::nullopt : std::optional(std::move(name));
}

/// A storage of the registry that holds a registered component, with its name
struct ComponentStorage
{
	std::string name;
	const entt::sparse_set* storage {nullptr};
	entt::meta_type type;
};

/// Writes the block of one component: its name, its fields, and its rows, each row with the entity and the value
template <typename RowValue>
uint32_t WriteRows(BinaryWriter& out, std::string_view name, const entt::meta_type& type, std::span<const entt::entity> rows,
                   RowValue&& valueOf)
{
	const auto* codec = ComponentCodec(type);
	const auto fields = KeptFields(type);
	WriteName(out, name);
	const auto length = out.BeginLength();
	if (codec != nullptr)
	{
		out.WriteSize(1);
		WriteName(out, k_WholeComponent);
	}
	else
	{
		out.WriteSize(fields.size());
		for (const auto& field : fields)
		{
			WriteName(out, field.info->name);
		}
	}
	const auto rowsAt = out.BeginLength();
	for (size_t row = 0; row < rows.size(); ++row)
	{
		out.WriteRaw(rows[row]);
		const void* value = valueOf(row);
		if (codec != nullptr)
		{
			const auto at = out.BeginLength();
			out.SetPlace(name, k_WholeComponent);
			codec->save(value, out);
			out.EndLength(at);
			continue;
		}
		for (const auto& field : fields)
		{
			const auto at = out.BeginLength();
			out.SetPlace(name, field.info->name);
			if (field.info->saveFrom != nullptr)
			{
				field.info->saveFrom(value, out);
			}
			else
			{
				auto instance = type.from_void(value);
				field.info->save(field.data.get(instance), out);
			}
			out.EndLength(at);
		}
	}
	// The count of the rows takes the place of the length that BeginLength put
	const auto count = static_cast<uint32_t>(rows.size());
	std::memcpy(out.Bytes().data() + rowsAt, &count, sizeof(count));
	out.EndLength(length);
	return count;
}

/// The live rows of a storage, in the order of the storage from its end, so that a load gives the same order to the
/// systems that go through it
[[nodiscard]] std::vector<entt::entity> LiveRows(const entt::sparse_set& storage)
{
	std::vector<entt::entity> rows;
	rows.reserve(storage.size());
	for (auto it = storage.rbegin(); it != storage.rend(); ++it)
	{
		if (*it != entt::tombstone)
		{
			rows.push_back(*it);
		}
	}
	return rows;
}

void WriteStorage(BinaryWriter& out, const ComponentStorage& component, WorldSaveReport& report)
{
	const auto rows = LiveRows(*component.storage);
	report.components +=
	    WriteRows(out, component.name, component.type, rows, [&](size_t row) { return component.storage->value(rows[row]); });
}

/// The registered components of a registry, by their names, so that the same world always gives the same bytes
[[nodiscard]] std::vector<ComponentStorage> RegisteredStorages(const entt::registry& registry, const entt::meta_ctx& context,
                                                               std::vector<std::string>& unregistered)
{
	std::vector<ComponentStorage> components;
	for (const auto& [id, storage] : registry.storage())
	{
		const auto& info = StorageType(storage);
		if (info == entt::type_id<entt::entity>() || storage.empty())
		{
			continue;
		}
		const auto type = entt::resolve(context, info);
		const ComponentInfo* component = type ? static_cast<const ComponentInfo*>(type.custom()) : nullptr;
		// A storage with a name of its own is not the storage of the component
		if (component == nullptr || id != info.hash())
		{
			unregistered.push_back(ShortTypeName(info));
			continue;
		}
		components.push_back({.name = ShortTypeName(info), .storage = &storage, .type = type});
	}
	std::ranges::sort(components, {}, &ComponentStorage::name);
	std::ranges::sort(unregistered);
	return components;
}

/// The parts of one component that a hash does not include. The hash finds them one time for each component.
struct ComponentLeaveOut
{
	/// The hash does not include the component
	bool whole {false};
	/// The hash does not include the row of an entity in one of these storages
	std::vector<const entt::sparse_set*> rowMarkers;
	/// By the index of the field: the hash does not include the field of a row
	std::vector<bool> fieldAlways;
	/// By the index of the field: the hash does not include the field of an entity in one of these storages
	std::vector<std::vector<const entt::sparse_set*>> fieldMarkers;
};

/// Finds the parts of a component that the hash does not include. The names are the names of the fields, or "*" for a component
/// that saves as one value.
[[nodiscard]] ComponentLeaveOut FindLeaveOut(std::span<const HashLeaveOut> leaveOut, std::string_view component,
                                             std::span<const std::string_view> names)
{
	ComponentLeaveOut found {
	    .whole = false, .rowMarkers = {}, .fieldAlways = std::vector<bool>(names.size()), .fieldMarkers = {}};
	found.fieldMarkers.resize(names.size());
	for (const auto& part : leaveOut)
	{
		if (part.component != k_WholeComponent && part.component != component)
		{
			continue;
		}
		if (part.field.empty())
		{
			if (part.marker == nullptr)
			{
				found.whole = true;
			}
			else
			{
				found.rowMarkers.push_back(part.marker);
			}
			continue;
		}
		for (size_t index = 0; index < names.size(); ++index)
		{
			if (names[index] != part.field)
			{
				continue;
			}
			if (part.marker == nullptr)
			{
				found.fieldAlways[index] = true;
			}
			else
			{
				found.fieldMarkers[index].push_back(part.marker);
			}
		}
	}
	return found;
}

/// Whether one of the storages has the entity
[[nodiscard]] bool AnyHas(std::span<const entt::sparse_set* const> storages, entt::entity entity)
{
	return std::ranges::any_of(storages, [entity](const entt::sparse_set* storage) { return storage->contains(entity); });
}

void WriteHeader(BinaryWriter& out)
{
	out.Write({reinterpret_cast<const uint8_t*>(k_WorldArchiveMagic.data()), k_WorldArchiveMagic.size()});
	out.WriteRaw(k_WorldArchiveVersion);
}

/// The hash of one component of the world, and, when the caller asks, the hash of each of its fields
struct ComponentHash
{
	/// The hash keeps one row or more of the component
	bool kept {false};
	uint64_t hash {0};
	std::vector<uint64_t> fields;
	/// Tools: the bytes of each field of each row, when asked. The field of a row is the index of the field in the
	/// component.
	WorldRows rows;
};

/// FNV-1a over the bytes of each part
constexpr uint64_t k_HashOffset = 14695981039346656037ull;
constexpr uint64_t k_HashPrime = 1099511628211ull;

void MixBytes(uint64_t& hash, std::span<const uint8_t> bytes)
{
	for (const auto byte : bytes)
	{
		hash = (hash ^ byte) * k_HashPrime;
	}
}

#if defined(OPENBLACK_DEVELOPER_TOOLS)
/// Tools: keeps the bytes of each field of each row that the hash leaves out, marked as a part that only shows the world
template <typename KeepsField>
void KeepLeftOutRows(const ComponentStorage& component, const TypeCodec* codec, std::span<const KeptField> fields,
                     const KeepsField& keepsField, BinaryWriter& out, WorldRows& kept)
{
	const auto keep = [&](size_t index, entt::entity row, std::span<const uint8_t> bytes) {
		kept.rows.push_back({.entity = row,
		                     .field = static_cast<uint32_t>(index),
		                     .offset = static_cast<uint32_t>(kept.bytes.size()),
		                     .size = static_cast<uint32_t>(bytes.size()),
		                     .presentation = true});
		kept.bytes.insert(kept.bytes.end(), bytes.begin(), bytes.end());
	};
	for (const auto row : LiveRows(*component.storage))
	{
		const void* value = component.storage->value(row);
		const size_t count = codec != nullptr ? 1 : fields.size();
		for (size_t index = 0; index < count; ++index)
		{
			if (keepsField(index, row))
			{
				continue;
			}
			out.Bytes().clear();
			if (codec != nullptr)
			{
				codec->save(value, out);
			}
			else if (fields[index].info->saveFrom != nullptr)
			{
				fields[index].info->saveFrom(value, out);
			}
			else
			{
				auto instance = component.type.from_void(value);
				fields[index].info->save(fields[index].data.get(instance), out);
			}
			keep(index, row, out.Bytes());
		}
	}
	out.Bytes().clear();
}
#endif

/// Hashes the rows of one component. The writer is the scratch memory of the caller.
[[nodiscard]] ComponentHash HashComponent(const ComponentStorage& component, std::span<const HashLeaveOut> leaveOut,
                                          bool byField, const RowVisitor& visit, BinaryWriter& out, bool keepRows,
                                          HashMemo::Component* memo)
{
	ComponentHash result;
	// Each row: its entity, then each field that the hash keeps, with the bytes that a save writes
	const auto* codec = ComponentCodec(component.type);
	const auto fields = KeptFields(component.type);
	std::vector<std::string_view> names;
	if (codec != nullptr)
	{
		names.push_back(k_WholeComponent);
	}
	else
	{
		std::ranges::transform(fields, std::back_inserter(names),
		                       [](const KeptField& field) { return std::string_view(field.info->name); });
	}
	const auto left = FindLeaveOut(leaveOut, component.name, names);
	const auto keepsField = [&left](size_t index, entt::entity row) {
		return !left.whole && !left.fieldAlways[index] && !AnyHas(left.fieldMarkers[index], row) &&
		       !AnyHas(left.rowMarkers, row);
	};
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	// Tools: the rows also keep the fields that the hash leaves out, with a mark, so that a compare shows what each
	// computer draws of the thing that parts
	if (keepRows)
	{
		KeepLeftOutRows(component, codec, fields, keepsField, out, result.rows);
	}
	// A component with no values is one row of no bytes for each entity, so that a compare sees the mark
	if (keepRows && codec == nullptr && fields.empty())
	{
		for (const auto row : LiveRows(*component.storage))
		{
			result.rows.rows.push_back({.entity = row,
			                            .field = 0,
			                            .offset = static_cast<uint32_t>(result.rows.bytes.size()),
			                            .size = 0,
			                            .presentation = left.whole || AnyHas(left.rowMarkers, row)});
		}
	}
#endif
	if (left.whole)
	{
		return result;
	}
	result.hash = k_HashOffset;
	// The hash of each field on its own, when asked: the whole value of a component that saves as one value
	result.fields.assign(byField ? std::max<size_t>(fields.size(), 1) : 0, k_HashOffset);
	const auto mixField = [&](size_t index, std::span<const uint8_t> bytes, entt::entity row) {
		if (byField)
		{
			MixBytes(result.fields.at(index), bytes);
		}
#if defined(OPENBLACK_DEVELOPER_TOOLS)
		if (keepRows)
		{
			auto& kept = result.rows;
			kept.rows.push_back({.entity = row,
			                     .field = static_cast<uint32_t>(index),
			                     .offset = static_cast<uint32_t>(kept.bytes.size()),
			                     .size = static_cast<uint32_t>(bytes.size())});
			kept.bytes.insert(kept.bytes.end(), bytes.begin(), bytes.end());
		}
#else
		static_cast<void>(row);
		static_cast<void>(keepRows);
#endif
	};
	// A component with no values only marks its entities. The game never reads the order of such a mark, so the hash
	// takes its entities in the order of their numbers.
	auto rows = LiveRows(*component.storage);
	if (codec == nullptr && fields.empty())
	{
		std::ranges::sort(rows, {}, [](entt::entity entity) { return entt::to_integral(entity); });
	}
	// With the memo, the writer keeps all the rows of the component. While each row is the same as in the memo, the
	// hash after the row comes from the memo. From the first row that changed, the hash mixes each row.
	const bool remembers = memo != nullptr && !byField && !visit && !keepRows;
	std::vector<HashMemo::Row> remembered;
	bool same = remembers;
	if (remembers)
	{
		out.Clear();
		remembered.reserve(rows.size());
	}
	for (const auto row : rows)
	{
		if (AnyHas(left.rowMarkers, row))
		{
			continue;
		}
		result.kept = true;
		// Without the memo, each row clears the writer, and its memory stays for the next row
		if (!remembers)
		{
			out.Clear();
		}
		const auto rowStart = out.Size();
		out.WriteRaw(row);
		const void* value = component.storage->value(row);
		if (codec != nullptr)
		{
			if (keepsField(0, row))
			{
				const auto start = out.Size();
				codec->save(value, out);
				mixField(0, out.Written(start), row);
				if (visit)
				{
					visit(component.name, row, k_WholeComponent, out.Written(start));
				}
			}
		}
		else
		{
			for (size_t index = 0; index < fields.size(); ++index)
			{
				const auto& field = fields.at(index);
				if (!keepsField(index, row))
				{
					continue;
				}
				const auto start = out.Size();
				if (field.info->saveFrom != nullptr)
				{
					field.info->saveFrom(value, out);
				}
				else
				{
					auto instance = component.type.from_void(value);
					field.info->save(field.data.get(instance), out);
				}
				mixField(index, out.Written(start), row);
				if (visit)
				{
					visit(component.name, row, field.info->name, out.Written(start));
				}
			}
		}
		const auto bytes = out.Written(rowStart);
		if (!remembers)
		{
			MixBytes(result.hash, bytes);
			continue;
		}
		const auto place = remembered.size();
		const auto end = out.Size();
		same = same && place < memo->rows.size() && memo->rows[place].end == end &&
		       std::ranges::equal(bytes, std::span<const uint8_t>(memo->bytes).subspan(rowStart, bytes.size()));
		if (same)
		{
			result.hash = memo->rows[place].hash;
		}
		else
		{
			MixBytes(result.hash, bytes);
		}
		remembered.push_back({.end = end, .hash = result.hash});
	}
	if (remembers)
	{
		const auto written = out.Written();
		memo->bytes.assign(written.begin(), written.end());
		memo->rows = std::move(remembered);
	}
	return result;
}

#if defined(OPENBLACK_DEVELOPER_TOOLS)
/// Adds the rows of one component to the rows of the world. The fields of the component get the indices of the world.
void AddRows(WorldRows& world, const ComponentStorage& component, const WorldRows& rows)
{
	if (rows.rows.empty())
	{
		return;
	}
	std::vector<std::string> names;
	const auto fields = KeptFields(component.type);
	if (ComponentCodec(component.type) != nullptr || fields.empty())
	{
		names.emplace_back(k_WholeComponent);
	}
	else
	{
		std::ranges::transform(fields, std::back_inserter(names),
		                       [](const KeptField& field) { return std::string(field.info->name); });
	}
	const auto first = static_cast<uint32_t>(world.fields.size());
	std::ranges::transform(names, std::back_inserter(world.fields),
	                       [&component](const std::string& field) { return component.name + "." + field; });
	const auto start = static_cast<uint32_t>(world.bytes.size());
	world.bytes.insert(world.bytes.end(), rows.bytes.begin(), rows.bytes.end());
	std::ranges::transform(rows.rows, std::back_inserter(world.rows), [first, start](WorldRows::Row row) {
		row.field += first;
		row.offset += start;
		return row;
	});
}
#endif

} // namespace

std::vector<uint8_t> openblack::ecs::reflection::SaveWorld(const entt::registry& registry, const entt::meta_ctx& context,
                                                           WorldSaveReport* report)
{
	WorldSaveReport local;
	auto& summary = report != nullptr ? *report : local;
	BinaryWriter out(context);
	WriteHeader(out);

	// The entities as the registry keeps them: the live entities first, then the free entities in the order that the
	// registry uses them again
	const auto* entities = registry.storage<entt::entity>();
	const auto count = entities != nullptr ? entities->size() : 0;
	out.WriteSize(count);
	out.WriteSize(entities != nullptr ? entities->free_list() : 0);
	for (size_t i = 0; i < count; ++i)
	{
		out.WriteRaw(entities->data()[i]);
	}
	summary.entities = entities != nullptr ? entities->free_list() : 0;

	const auto components = RegisteredStorages(registry, context, summary.unregisteredComponents);
	out.WriteSize(components.size());
	for (const auto& component : components)
	{
		WriteStorage(out, component, summary);
	}
	summary.unsavedValues.insert(out.Unsaved().begin(), out.Unsaved().end());
	return std::move(out.Bytes());
}

std::vector<std::string> openblack::ecs::reflection::UnregisteredStorages(const entt::registry& registry,
                                                                          const entt::meta_ctx& context)
{
	std::vector<std::string> unregistered;
	static_cast<void>(RegisteredStorages(registry, context, unregistered));
	return unregistered;
}

openblack::ecs::reflection::WorldHashes openblack::ecs::reflection::HashWorld(const entt::registry& registry,
                                                                              const entt::meta_ctx& context,
                                                                              std::span<const HashLeaveOut> leaveOut,
                                                                              bool byField, const RowVisitor& visit,
                                                                              WorkerPool* pool, WorldRows* rows, HashMemo* memo)
{
	WorldHashes hashes;
	{
		BinaryWriter out(context);
		const auto* entities = registry.storage<entt::entity>();
		const auto count = entities != nullptr ? entities->size() : 0;
		out.WriteSize(count);
		out.WriteSize(entities != nullptr ? entities->free_list() : 0);
		for (size_t i = 0; i < count; ++i)
		{
			out.WriteRaw(entities->data()[i]);
		}
		hashes.entities = k_HashOffset;
		MixBytes(hashes.entities, out.Bytes());
	}
	const auto components = RegisteredStorages(registry, context, hashes.unregistered);
	std::vector<ComponentHash> results(components.size());
	// The memo of each component, found before the threads share the components
	std::vector<HashMemo::Component*> memos(components.size(), nullptr);
	if (memo != nullptr)
	{
		for (size_t index = 0; index < components.size(); ++index)
		{
			memos[index] = &memo->components[components[index].name];
		}
	}
	// The hash of a component does not use the other components, so the threads of the pool can share them. The
	// visitor of the tools must get the rows in order, so then this thread does all of them.
	if (pool != nullptr && !visit)
	{
		std::vector<BinaryWriter> writers(pool->GetLaneCount(), BinaryWriter(context));
		pool->ForEach(components.size(), [&](size_t index, size_t lane) {
			results[index] =
			    HashComponent(components[index], leaveOut, byField, visit, writers[lane], rows != nullptr, memos[index]);
		});
	}
	else
	{
		BinaryWriter out(context);
		for (size_t index = 0; index < components.size(); ++index)
		{
			results[index] = HashComponent(components[index], leaveOut, byField, visit, out, rows != nullptr, memos[index]);
		}
	}
	for (size_t index = 0; index < components.size(); ++index)
	{
		const auto& component = components[index];
		const auto& result = results[index];
#if defined(OPENBLACK_DEVELOPER_TOOLS)
		if (rows != nullptr)
		{
			AddRows(*rows, component, result.rows);
		}
#endif
		// A component of which the hash keeps no row is not in the hash, as a component that no entity has
		if (!result.kept)
		{
			continue;
		}
		hashes.components.emplace_back(component.name, result.hash);
		if (byField)
		{
			const auto fields = KeptFields(component.type);
			if (ComponentCodec(component.type) != nullptr || fields.empty())
			{
				hashes.fields.emplace_back(component.name + "." + std::string(k_WholeComponent), result.fields.front());
			}
			else
			{
				for (size_t field = 0; field < fields.size(); ++field)
				{
					hashes.fields.emplace_back(component.name + "." + std::string(fields.at(field).info->name),
					                           result.fields.at(field));
				}
			}
		}
	}
	return hashes;
}

std::shared_ptr<const WorldSnapshot> openblack::ecs::reflection::SnapshotWorld(const entt::registry& registry,
                                                                               const entt::meta_ctx& context)
{
	auto snapshot = std::make_shared<WorldSnapshot>();
	if (const auto* entities = registry.storage<entt::entity>(); entities != nullptr)
	{
		snapshot->entities.assign(entities->data(), entities->data() + entities->size());
		snapshot->inUse = entities->free_list();
	}
	for (const auto& component : RegisteredStorages(registry, context, snapshot->unregisteredComponents))
	{
		auto& storage = snapshot->storages.emplace_back();
		storage.name = component.name;
		storage.type = component.type;
		const ComponentInfo* info = component.type.custom();
		if (info->copy != nullptr)
		{
			storage.copy = info->copy(*component.storage);
			continue;
		}
		// A component that cannot be copied safely is encoded now
		BinaryWriter out(context);
		WorldSaveReport rows;
		WriteStorage(out, component, rows);
		storage.encoded = std::move(out.Bytes());
		storage.encodedRows = static_cast<uint32_t>(rows.components);
		snapshot->unsavedValues.insert(out.Unsaved().begin(), out.Unsaved().end());
	}
	return snapshot;
}

std::vector<uint8_t> openblack::ecs::reflection::SaveWorld(const WorldSnapshot& snapshot, const entt::meta_ctx& context,
                                                           WorldSaveReport* report)
{
	WorldSaveReport local;
	auto& summary = report != nullptr ? *report : local;
	BinaryWriter out(context);
	WriteHeader(out);
	out.WriteSize(snapshot.entities.size());
	out.WriteSize(snapshot.inUse);
	for (const auto entity : snapshot.entities)
	{
		out.WriteRaw(entity);
	}
	summary.entities = snapshot.inUse;
	summary.unregisteredComponents = snapshot.unregisteredComponents;
	out.WriteSize(snapshot.storages.size());
	for (const auto& storage : snapshot.storages)
	{
		if (!storage.copy.has_value())
		{
			out.Write(storage.encoded);
			summary.components += storage.encodedRows;
			continue;
		}
		const auto& copy = *storage.copy;
		summary.components += WriteRows(out, storage.name, storage.type, copy.rows, [&copy](size_t row) -> const void* {
			return copy.at != nullptr ? copy.at(copy.values.get(), row) : nullptr;
		});
	}
	summary.unsavedValues = snapshot.unsavedValues;
	summary.unsavedValues.insert(out.Unsaved().begin(), out.Unsaved().end());
	return std::move(out.Bytes());
}

namespace
{

/// Makes the entities of the archive in the registry. Returns how the saved entities became the registry's entities.
[[nodiscard]] std::optional<EntityRemap> LoadEntities(BinaryReader& in, entt::registry& registry, WorldLoadMode mode,
                                                      WorldLoadReport& report)
{
	const auto count = in.ReadSize();
	const auto inUse = in.ReadSize();
	if (!count.has_value() || !inUse.has_value() || *inUse > *count)
	{
		return std::nullopt;
	}
	std::vector<entt::entity> saved(*count);
	for (auto& entity : saved)
	{
		in.ReadRaw(entity);
	}
	if (in.Failed())
	{
		return std::nullopt;
	}
	auto& storage = registry.storage<entt::entity>();
	report.entities = *inUse;
	if (mode == WorldLoadMode::Exact)
	{
		if (storage.free_list() != 0)
		{
			return std::nullopt;
		}
		// As the snapshot loader of EnTT does: each entity with its version, then the count of the live entities
		entt::entity placeholder {};
		for (const auto entity : saved)
		{
			storage.generate(entity);
			placeholder = entity > placeholder ? entity : placeholder;
		}
		storage.start_from(entt::entt_traits<entt::entity>::next(placeholder));
		storage.free_list(*inUse);
		return EntityRemap {};
	}
	std::unordered_map<entt::entity, entt::entity> table;
	table.reserve(*inUse);
	for (size_t i = 0; i < *inUse; ++i)
	{
		table.emplace(saved[i], registry.create(saved[i]));
	}
	return EntityRemap {std::move(table)};
}

/// Adds the component of the type to the entity, with the fields that the read function puts into a default value
template <typename Read>
[[nodiscard]] bool AddComponent(entt::sparse_set& storage, const entt::meta_type& type, entt::entity entity, Read read)
{
	// A value made first and then copied in, so that the systems that hear of the new component see all of its fields
	if (auto value = type.construct(); value)
	{
		auto reference = value.as_ref();
		read(reference);
		if (storage.push(entity, value.base().data()) != storage.end())
		{
			return true;
		}
	}
	// A component that cannot be copied is made in place, and then it reads its fields
	if (storage.push(entity) == storage.end())
	{
		return false;
	}
	auto reference = type.from_void(storage.value(entity));
	read(reference);
	return true;
}

/// Reads the block of one component. Returns false when the block has damage.
[[nodiscard]] bool LoadStorage(BinaryReader& in, entt::registry& registry, const entt::meta_ctx& context,
                               const std::map<std::string, entt::meta_type, std::less<>>& types, WorldLoadReport& report)
{
	const auto name = ReadName(in);
	const auto length = in.ReadSize();
	if (!name.has_value() || !length.has_value())
	{
		return false;
	}
	const auto block = in.Take(*length);
	if (!block.has_value())
	{
		return false;
	}
	const auto found = types.find(*name);
	if (found == types.end())
	{
		report.unknownComponents.push_back(*name);
		return true;
	}
	const auto& type = found->second;
	const ComponentInfo* info = type.custom();
	auto& storage = info->storage(registry);
	const auto* codec = ComponentCodec(type);
	const auto kept = KeptFields(type);

	BinaryReader blockIn(context, *block, in.Remap());
	const auto fieldCount = blockIn.ReadSize();
	if (!fieldCount.has_value())
	{
		return false;
	}
	// For each field of the archive, the field of this build that takes it, or none
	std::vector<std::optional<KeptField>> fields;
	for (size_t i = 0; i < *fieldCount; ++i)
	{
		const auto fieldName = ReadName(blockIn);
		if (!fieldName.has_value())
		{
			return false;
		}
		const auto match = std::ranges::find(kept, *fieldName, [](const KeptField& field) { return field.info->name; });
		if (match != kept.end())
		{
			fields.emplace_back(*match);
		}
		else
		{
			fields.emplace_back(std::nullopt);
			if (*fieldName != k_WholeComponent || codec == nullptr)
			{
				report.unknownFields.push_back(*name + "." + *fieldName);
			}
		}
	}
	const bool whole = codec != nullptr && *fieldCount == 1 && !fields[0].has_value();
	const auto rows = blockIn.ReadSize();
	if (!rows.has_value())
	{
		return false;
	}
	std::set<std::string> damaged;
	for (size_t row = 0; row < *rows; ++row)
	{
		entt::entity saved = entt::null;
		blockIn.ReadRaw(saved);
		// Each field of the row as its bytes, read first so that a field that does not read stays in its place
		std::vector<std::span<const uint8_t>> values;
		values.reserve(fields.size());
		for (size_t i = 0; i < fields.size(); ++i)
		{
			const auto size = blockIn.ReadSize();
			const auto bytes = size.has_value() ? blockIn.Take(*size) : std::nullopt;
			values.push_back(bytes.value_or(std::span<const uint8_t> {}));
		}
		if (blockIn.Failed())
		{
			return false;
		}
		const auto entity = in.Remap()(saved);
		if (entity == entt::null || !registry.valid(entity))
		{
			continue;
		}
		const bool added = AddComponent(storage, type, entity, [&](entt::meta_any& instance) {
			for (size_t i = 0; i < fields.size(); ++i)
			{
				BinaryReader fieldIn(context, values[i], in.Remap());
				if (whole)
				{
					codec->load(const_cast<void*>(instance.base().data()), fieldIn);
				}
				else if (fields[i].has_value())
				{
					fields[i]->info->load(fields[i]->data, instance, fieldIn);
				}
				else
				{
					continue;
				}
				if (fieldIn.Failed() || fieldIn.Remaining() != 0)
				{
					damaged.insert(*name + "." + (whole ? std::string(k_WholeComponent) : fields[i]->info->name));
				}
			}
		});
		if (added)
		{
			++report.components;
		}
		else
		{
			report.refusedComponents.push_back(*name);
		}
	}
	report.damagedFields.insert(report.damagedFields.end(), damaged.begin(), damaged.end());
	return blockIn.Remaining() == 0;
}

} // namespace

std::optional<WorldLoadResult> openblack::ecs::reflection::LoadWorld(entt::registry& registry, const entt::meta_ctx& context,
                                                                     std::span<const uint8_t> bytes, WorldLoadMode mode)
{
	const EntityRemap identity;
	BinaryReader header(context, bytes, identity);
	std::array<char, 4> magic {};
	uint32_t version = 0;
	header.ReadRaw(magic);
	header.ReadRaw(version);
	if (header.Failed() || magic != k_WorldArchiveMagic || version != k_WorldArchiveVersion)
	{
		return std::nullopt;
	}
	WorldLoadResult result;
	auto remap = LoadEntities(header, registry, mode, result.report);
	if (!remap.has_value())
	{
		return std::nullopt;
	}
	result.remap = std::move(*remap);

	std::map<std::string, entt::meta_type, std::less<>> types;
	for (auto&& [id, type] : entt::resolve(context))
	{
		const ComponentInfo* info = type.custom();
		if (info != nullptr && info->storage != nullptr)
		{
			types.emplace(ShortTypeName(type.info()), type);
		}
	}
	BinaryReader in(context, bytes.subspan(header.Position()), result.remap);
	const auto storages = in.ReadSize();
	if (!storages.has_value())
	{
		return std::nullopt;
	}
	for (size_t i = 0; i < *storages; ++i)
	{
		if (!LoadStorage(in, registry, context, types, result.report))
		{
			return std::nullopt;
		}
	}
	if (in.Remaining() != 0)
	{
		return std::nullopt;
	}
	return result;
}

std::vector<uint8_t> openblack::ecs::reflection::SaveSections(std::span<const NamedSection> sections)
{
	const entt::meta_ctx context;
	BinaryWriter out(context);
	out.WriteSize(sections.size());
	for (const auto& section : sections)
	{
		Save(out, section.name);
		out.WriteSize(section.bytes.size());
		out.Write(section.bytes);
	}
	return std::move(out.Bytes());
}

std::optional<std::vector<NamedSection>> openblack::ecs::reflection::LoadSections(std::span<const uint8_t> bytes)
{
	const entt::meta_ctx context;
	const EntityRemap identity;
	BinaryReader in(context, bytes, identity);
	const auto count = in.ReadSize();
	std::vector<NamedSection> sections;
	for (size_t i = 0; count.has_value() && i < *count && !in.Failed(); ++i)
	{
		NamedSection section;
		Load(in, section.name);
		const auto length = in.ReadSize();
		const auto taken = length.has_value() ? in.Take(*length) : std::nullopt;
		if (taken.has_value())
		{
			section.bytes.assign(taken->begin(), taken->end());
			sections.push_back(std::move(section));
		}
	}
	if (!count.has_value() || in.Failed() || in.Remaining() != 0)
	{
		return std::nullopt;
	}
	return sections;
}
