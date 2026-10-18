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
#include <map>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <entt/meta/meta.hpp>
#include <entt/meta/resolve.hpp>

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

void WriteStorage(BinaryWriter& out, const ComponentStorage& component, WorldSaveReport& report)
{
	const auto* codec = ComponentCodec(component.type);
	const auto fields = KeptFields(component.type);
	WriteName(out, component.name);
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
	const auto& storage = *component.storage;
	const auto rowsAt = out.BeginLength();
	uint32_t rows = 0;
	// The rows go in the order of the storage, so that a load gives the same order to the systems that go through it
	for (auto it = storage.rbegin(); it != storage.rend(); ++it)
	{
		const auto entity = *it;
		if (entity == entt::tombstone)
		{
			continue;
		}
		++rows;
		out.WriteRaw(entity);
		const void* value = storage.value(entity);
		if (codec != nullptr)
		{
			const auto at = out.BeginLength();
			out.SetPlace(component.name, k_WholeComponent);
			codec->save(value, out);
			out.EndLength(at);
			continue;
		}
		const auto instance = component.type.from_void(value);
		for (const auto& field : fields)
		{
			const auto at = out.BeginLength();
			out.SetPlace(component.name, field.info->name);
			field.info->save(field.data.get(instance), out);
			out.EndLength(at);
		}
	}
	// The count of the rows takes the place of the length that BeginLength put
	std::memcpy(out.Bytes().data() + rowsAt, &rows, sizeof(rows));
	out.EndLength(length);
	report.components += rows;
}

} // namespace

std::vector<uint8_t> openblack::ecs::reflection::SaveWorld(const entt::registry& registry, const entt::meta_ctx& context,
                                                           WorldSaveReport* report)
{
	WorldSaveReport local;
	auto& summary = report != nullptr ? *report : local;
	BinaryWriter out(context);
	out.Write({reinterpret_cast<const uint8_t*>(k_WorldArchiveMagic.data()), k_WorldArchiveMagic.size()});
	out.WriteRaw(k_WorldArchiveVersion);

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

	// The components by their names, so that the same world always gives the same bytes
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
			summary.unregisteredComponents.push_back(ShortTypeName(info));
			continue;
		}
		components.push_back({.name = ShortTypeName(info), .storage = &storage, .type = type});
	}
	std::ranges::sort(components, {}, &ComponentStorage::name);
	std::ranges::sort(summary.unregisteredComponents);
	out.WriteSize(components.size());
	for (const auto& component : components)
	{
		WriteStorage(out, component, summary);
	}
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
