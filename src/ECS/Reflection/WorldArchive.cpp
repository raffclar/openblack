/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorldArchive.h"

#include <cstddef>
#include <cstring>

#include <algorithm>
#include <array>
#include <atomic>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
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
	/// The id of the storage in the registry
	entt::id_type id {0};
	const entt::sparse_set* storage {nullptr};
	entt::meta_type type;
	/// The value at a place of the storage, none for a component with no values
	const void* (*valueAt)(const entt::sparse_set& storage, size_t place) {nullptr};
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
[[nodiscard]] std::vector<entt::entity> LiveRows(const entt::sparse_set& storage, std::vector<entt::entity> rows = {})
{
	rows.clear();
	rows.reserve(storage.size());
	for (auto it : std::ranges::reverse_view(storage))
	{
		if (it != entt::tombstone)
		{
			rows.push_back(it);
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

/// The name and the component type of a storage of the registry
[[nodiscard]] HashMemo::Storage FindStorageType(entt::id_type id, const entt::type_info& info, const entt::meta_ctx& context)
{
	HashMemo::Storage found {.typeHash = info.hash(), .name = ShortTypeName(info), .type = {}, .registered = false};
	const auto type = entt::resolve(context, info);
	const ComponentInfo* component = type ? static_cast<const ComponentInfo*>(type.custom()) : nullptr;
	// A storage with a name of its own is not the storage of the component
	if (component == nullptr || id != info.hash())
	{
		return found;
	}
	found.registered = true;
	// The world does not keep the drawn state that only this computer has
	if (!component->localOnly)
	{
		found.type = type;
	}
	return found;
}

/// The registered components of a registry, by their names, so that the same world always gives the same bytes. A memo
/// keeps the name and the type of each storage, so that the next call finds them with no search.
[[nodiscard]] std::vector<ComponentStorage> RegisteredStorages(const RegistryStores& stores, const entt::meta_ctx& context,
                                                               std::vector<std::string>& unregistered, HashMemo* memo = nullptr)
{
	if (memo != nullptr && memo->context != &context)
	{
		memo->storages.clear();
		memo->writers.clear();
		memo->context = &context;
	}
	std::vector<ComponentStorage> components;
	for (const auto& [id, store] : stores)
	{
		const auto& storage = *store;
		const auto& info = StorageType(storage);
		if (info == entt::type_id<entt::entity>() || storage.empty())
		{
			continue;
		}
		std::optional<HashMemo::Storage> local;
		const HashMemo::Storage* found = nullptr;
		if (memo != nullptr)
		{
			auto [kept, added] = memo->storages.try_emplace(id);
			if (added || kept->second.typeHash != info.hash())
			{
				kept->second = FindStorageType(id, info, context);
			}
			found = &kept->second;
		}
		else
		{
			found = &local.emplace(FindStorageType(id, info, context));
		}
		if (!found->registered)
		{
			unregistered.push_back(found->name);
			continue;
		}
		if (found->type)
		{
			const auto* component = static_cast<const ComponentInfo*>(found->type.custom());
			components.push_back({.name = found->name,
			                      .id = id,
			                      .storage = &storage,
			                      .type = found->type,
			                      .valueAt = component != nullptr ? component->valueAt : nullptr});
		}
	}
	std::ranges::sort(components, {}, &ComponentStorage::name);
	std::ranges::sort(unregistered);
	return components;
}

[[nodiscard]] std::vector<ComponentStorage> RegisteredStorages(const entt::registry& registry, const entt::meta_ctx& context,
                                                               std::vector<std::string>& unregistered, HashMemo* memo = nullptr)
{
	return RegisteredStorages(StoresOf(registry), context, unregistered, memo);
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

/// The entity numbers that a row marker of the hash has, one flag for each number. A clear flag means that no row marker
/// has an entity with that number. A set flag needs the test of each marker.
struct MarkedNumbers
{
	std::vector<uint8_t> flags;

	[[nodiscard]] bool MayHave(entt::entity entity) const
	{
		const auto number = static_cast<size_t>(entt::to_entity(entity));
		return number < flags.size() && flags[number] != 0;
	}
};

/// Finds the entity numbers of all the row markers of the parts that the hash leaves out
[[nodiscard]] MarkedNumbers FindMarkedNumbers(std::span<const HashLeaveOut> leaveOut)
{
	MarkedNumbers marked;
	for (const auto& part : leaveOut)
	{
		if (!part.field.empty() || part.marker == nullptr)
		{
			continue;
		}
		for (const auto entity : *part.marker)
		{
			// A free place of a store that keeps its places has no entity
			if (entity == entt::tombstone)
			{
				continue;
			}
			const auto number = static_cast<size_t>(entt::to_entity(entity));
			if (number >= marked.flags.size())
			{
				marked.flags.resize(number + 1, 0);
			}
			marked.flags[number] = 1;
		}
	}
	return marked;
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

} // namespace

/// How the rows of one component save, and what the hash leaves out of them. The memo keeps it while the parts that the
/// hash leaves out stay the same.
struct openblack::ecs::reflection::HashMemo::Layout
{
	entt::meta_type type;
	const TypeCodec* codec {nullptr};
	std::vector<KeptField> fields;
	ComponentLeaveOut left;
	/// When each row keeps the same fields: the steps that write them, with the fields whose bytes touch in memory as one
	/// block
	SavePlan plan;
	bool bySteps {false};
	/// The steps need a row to find where each field is in the value. A layout that needs steps but found no row has
	/// none yet.
	bool planned {false};

	/// Whether the hash keeps a field of a row
	[[nodiscard]] bool Keeps(size_t index, entt::entity row) const
	{
		return KeepsField(index, row) && !AnyHas(left.rowMarkers, row);
	}
	/// Whether the hash keeps a field of a row that it keeps
	[[nodiscard]] bool KeepsField(size_t index, entt::entity row) const
	{
		return !left.whole && !left.fieldAlways[index] && !AnyHas(left.fieldMarkers[index], row);
	}
};

namespace
{

/// The rows of the last hash of a component that the next hash can take again with no save. A row has the same bytes as
/// in the last hash when the same entity is at the same place and no write marked the row.
struct LastRows
{
	/// The end of each row of the last hash in its bytes
	std::span<const HashMemo::Row> rows;
	std::span<const std::vector<uint8_t>> chunks;
	/// The place of the first byte of each chunk among the bytes of all the rows
	std::vector<size_t> chunkStarts;
	const RowMarks::Marks* marks {nullptr};

	/// The bytes of the row at this place in the last hash, when the row has no change since then
	[[nodiscard]] std::optional<std::span<const uint8_t>> Same(size_t place, entt::entity row) const
	{
		if (place >= rows.size() || marks->Has(row))
		{
			return std::nullopt;
		}
		const auto start = place == 0 ? size_t {0} : rows[place - 1].end;
		const auto end = rows[place].end;
		// The chunk that holds the first byte of the row. A row is never in two chunks.
		const auto after = std::ranges::upper_bound(chunkStarts, start);
		if (after == chunkStarts.begin())
		{
			return std::nullopt;
		}
		const auto chunk = static_cast<size_t>(std::distance(chunkStarts.begin(), after) - 1);
		const auto& bytes = chunks[chunk];
		const auto offset = start - chunkStarts[chunk];
		if (end < start || end - start < sizeof(entt::entity) || offset + (end - start) > bytes.size())
		{
			return std::nullopt;
		}
		const auto found = std::span<const uint8_t>(bytes).subspan(offset, end - start);
		// Each row starts with its entity
		if (std::memcmp(found.data(), &row, sizeof(row)) != 0)
		{
			return std::nullopt;
		}
		return found;
	}
};

/// What the hash of the rows of one component needs: the layout of the component, and the rows in the order of the hash
struct RowLayout
{
	std::shared_ptr<const HashMemo::Layout> shape;
	std::vector<entt::entity> rows;
	/// The layout is the layout of the last hash of the component
	bool kept {false};
	/// The rows of the last hash that the hash can take again, when the marks of the component are usable
	std::optional<LastRows> last;
	/// The memo of the hash, when the hash can take rows of the last hash again. It counts the rows that the hash takes.
	HashMemo* memo {nullptr};
	/// The entity numbers of the row markers, when the hash found them. A row whose number no marker has needs no test of
	/// each marker.
	const MarkedNumbers* marked {nullptr};
	/// Each row is at the place of the same number in the storage: the storage has no free places, and the rows keep its
	/// order
	bool atPlaces {false};

	/// The value of the row at an index of the rows
	[[nodiscard]] const void* ValueOf(const ComponentStorage& component, size_t index) const
	{
		return atPlaces ? component.valueAt(*component.storage, index) : component.storage->value(rows[index]);
	}

	/// Whether the hash leaves out the row of an entity, because a row marker has the entity
	[[nodiscard]] bool LeavesOutRow(entt::entity row) const
	{
		if (marked != nullptr && !marked->MayHave(row))
		{
			return false;
		}
		return AnyHas(shape->left.rowMarkers, row);
	}
};

/// Finds how the rows of a component save and what the hash leaves out of them. The first row, when there is one, gives
/// the place of each field in the value.
[[nodiscard]] std::shared_ptr<const HashMemo::Layout> MakeLayout(const ComponentStorage& component,
                                                                 std::span<const HashLeaveOut> leaveOut, const void* firstRow)
{
	auto layout = std::make_shared<HashMemo::Layout>();
	layout->type = component.type;
	layout->codec = ComponentCodec(component.type);
	layout->fields = KeptFields(component.type);
	std::vector<std::string_view> names;
	if (layout->codec != nullptr)
	{
		names.push_back(k_WholeComponent);
	}
	else
	{
		std::ranges::transform(layout->fields, std::back_inserter(names),
		                       [](const KeptField& field) { return std::string_view {field.info->name}; });
	}
	layout->left = FindLeaveOut(leaveOut, component.name, names);
	// A field that only the reflection can write needs the way of each field
	const auto sameForEachRow =
	    std::ranges::all_of(layout->left.fieldMarkers, [](const auto& markers) { return markers.empty(); });
	const auto needsSteps = layout->codec == nullptr && !layout->left.whole && sameForEachRow;
	layout->planned = !needsSteps || firstRow != nullptr;
	if (needsSteps && firstRow != nullptr)
	{
		const auto* first = static_cast<const std::byte*>(firstRow);
		layout->bySteps = true;
		for (size_t index = 0; index < layout->fields.size() && layout->bySteps; ++index)
		{
			const auto* info = layout->fields[index].info;
			if (layout->left.fieldAlways[index])
			{
				continue;
			}
			if (info->saveFrom == nullptr)
			{
				layout->bySteps = false;
			}
			else if (info->plainAddress != nullptr)
			{
				const auto* field = static_cast<const std::byte*>(info->plainAddress(first));
				layout->plan.AddBlock(static_cast<size_t>(field - first), info->plainSize);
			}
			else
			{
				layout->plan.AddWriter(info->saveFrom);
			}
		}
	}
	return layout;
}

/// The layout of a component and its rows. The layout comes from the memo when the memo keeps one of the same type with
/// its steps, and else goes into the memo. The list of the rows uses the memory of the memo when there is one.
[[nodiscard]] RowLayout LayoutOf(const ComponentStorage& component, std::span<const HashLeaveOut> leaveOut,
                                 HashMemo::Component* memo = nullptr)
{
	auto* kept = memo != nullptr ? &memo->layout : nullptr;
	RowLayout layout {
	    .shape = nullptr,
	    .rows = LiveRows(*component.storage, memo != nullptr ? std::move(memo->spareLiveRows) : std::vector<entt::entity> {})};
	const void* first = layout.rows.empty() ? nullptr : component.storage->value(layout.rows.front());
	if (kept != nullptr && *kept != nullptr && (*kept)->type == component.type && ((*kept)->planned || first == nullptr))
	{
		layout.shape = *kept;
		layout.kept = true;
	}
	else
	{
		layout.shape = MakeLayout(component, leaveOut, first);
		if (kept != nullptr)
		{
			*kept = layout.shape;
		}
	}
	// A component with no values only marks its entities. The game never reads the order of such a mark, so the hash
	// takes its entities in the order of their numbers.
	if (layout.shape->codec == nullptr && layout.shape->fields.empty())
	{
		std::ranges::sort(layout.rows, {}, [](entt::entity entity) { return entt::to_integral(entity); });
	}
	else
	{
		layout.atPlaces = component.valueAt != nullptr && layout.rows.size() == component.storage->size();
	}
	return layout;
}

/// Finds the rows of the last hash of a component that this hash can take again. The marks must follow the component,
/// the layout must be the layout of the last hash, and the hash must keep the same fields of each row that it keeps.
void FindLastRows(const ComponentStorage& component, RowLayout& layout, HashMemo& memo, const HashMemo::Component* kept)
{
	const auto* marks = memo.marks.Of(component.id);
	const auto& shape = *layout.shape;
	const auto sameFields = std::ranges::all_of(shape.left.fieldMarkers, [](const auto& markers) { return markers.empty(); });
	const auto onlyMarks = shape.codec == nullptr && shape.fields.empty();
	if (marks == nullptr || kept == nullptr || !layout.kept || shape.left.whole || !sameFields || onlyMarks ||
	    kept->rows.empty())
	{
		return;
	}
	LastRows last {.rows = kept->rows, .chunks = kept->chunks, .chunkStarts = {}, .marks = marks};
	last.chunkStarts.reserve(kept->chunks.size());
	size_t start = 0;
	for (const auto& chunk : kept->chunks)
	{
		last.chunkStarts.push_back(start);
		start += chunk.size();
	}
	layout.last = std::move(last);
	layout.memo = &memo;
}

/// Writes the bytes of a row that the hash keeps: its entity, then each field that the hash keeps, with the bytes that a
/// save writes. The function gets each field after its bytes, with the index of the field and where its bytes start.
/// The caller gives only the rows that the hash keeps, so this function does not look at the marks of the rows again.
template <typename EachField>
void SaveRow(const ComponentStorage& component, const RowLayout& layout, entt::entity row, const void* value, BinaryWriter& out,
             const EachField& eachField)
{
	out.WriteRaw(row);
	if (layout.shape->codec != nullptr)
	{
		if (layout.shape->KeepsField(0, row))
		{
			const auto start = out.Size();
			layout.shape->codec->save(value, out);
			eachField(0, start);
		}
		return;
	}
	for (size_t index = 0; index < layout.shape->fields.size(); ++index)
	{
		const auto& field = layout.shape->fields.at(index);
		if (!layout.shape->KeepsField(index, row))
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
		eachField(index, start);
	}
}

/// Writes the bytes of a row that the hash keeps, as SaveRow does, by the steps of the layout
void SaveRowBySteps(const RowLayout& layout, entt::entity row, const void* value, BinaryWriter& out)
{
	out.WriteRaw(row);
	out.SaveSteps(layout.shape->plan.steps, value);
}

/// Writes the bytes of a row that the hash keeps, for a hash with no hash of each field, no visitor and no rows. The place
/// counts the rows that the hash keeps before this row. A row with no change since the last hash, at the same place,
/// gets the bytes of the last hash. Returns whether the row got the bytes of the last hash.
bool SaveKeptRow(const ComponentStorage& component, const RowLayout& layout, size_t index, size_t place, BinaryWriter& out)
{
	const auto row = layout.rows[index];
	if (layout.last.has_value())
	{
		if (const auto same = layout.last->Same(place, row); same.has_value())
		{
			const auto start = out.Size();
			out.Write(*same);
			if (layout.memo->checkMarks)
			{
				// Tests: the save of the row must give the same bytes
				const auto saved = out.Size();
				if (layout.shape->bySteps)
				{
					SaveRowBySteps(layout, row, layout.ValueOf(component, index), out);
				}
				else
				{
					SaveRow(component, layout, row, layout.ValueOf(component, index), out, [](size_t, size_t) {});
				}
				if (!std::ranges::equal(out.Written(start).first(saved - start), out.Written(saved)))
				{
					layout.memo->markMisses.fetch_add(1, std::memory_order_relaxed);
				}
				out.Bytes().resize(saved);
			}
			return true;
		}
	}
	if (layout.shape->bySteps)
	{
		SaveRowBySteps(layout, row, layout.ValueOf(component, index), out);
	}
	else
	{
		SaveRow(component, layout, row, layout.ValueOf(component, index), out, [](size_t, size_t) {});
	}
	return false;
}

/// The bytes of some rows of a component, saved before the hash mixes them: the bytes of each row that the hash keeps,
/// one row after the other, and the end of each row
struct SavedRows
{
	std::vector<uint8_t> bytes;
	std::vector<size_t> ends;
};

/// Saves the rows of a component from one row to another, for a hash with no hash of each field, no visitor and no rows.
/// The first place counts the rows that the hash keeps before the first row. The writer writes into the memory of the
/// saved rows, so that a save gets no new memory when the rows fit.
void SaveRows(const ComponentStorage& component, const RowLayout& layout, size_t first, size_t last, size_t firstPlace,
              BinaryWriter& out, SavedRows& saved)
{
	saved.ends.clear();
	saved.bytes.swap(out.Bytes());
	out.Clear();
	size_t taken = 0;
	size_t place = firstPlace;
	for (size_t index = first; index < last; ++index)
	{
		if (layout.LeavesOutRow(layout.rows[index]))
		{
			continue;
		}
		taken += SaveKeptRow(component, layout, index, place++, out) ? 1 : 0;
		saved.ends.push_back(out.Size());
	}
	if (taken != 0)
	{
		layout.memo->rowsTaken.fetch_add(taken, std::memory_order_relaxed);
	}
	// The saved rows take the bytes, and the writer gets its own memory again
	saved.bytes.swap(out.Bytes());
}

/// A part of the rows of a component that the threads share: the first thread that takes the part saves it
struct SharedPart
{
	static constexpr uint8_t k_Free = 0;
	static constexpr uint8_t k_Taken = 1;
	static constexpr uint8_t k_Saved = 2;

	/// The rows of the part, from the first to the row before the last
	size_t first {0};
	size_t last {0};
	/// The count of the rows that the hash keeps before the first row, when the hash takes rows of the last hash
	size_t firstPlace {0};
	SavedRows rows;
	std::atomic<uint8_t> state {k_Free};

	/// Saves the part when no thread took it before. Returns whether this thread saved it.
	bool Claim(const ComponentStorage& component, const RowLayout& layout, BinaryWriter& out)
	{
		auto expected = k_Free;
		if (!state.compare_exchange_strong(expected, k_Taken, std::memory_order_acquire))
		{
			return false;
		}
		SaveRows(component, layout, first, last, firstPlace, out, rows);
		state.store(k_Saved, std::memory_order_release);
		state.notify_all();
		return true;
	}
	[[nodiscard]] bool IsSaved() const { return state.load(std::memory_order_acquire) == k_Saved; }
	/// The rows, after the thread that took the part saved them
	[[nodiscard]] const SavedRows& Wait() const
	{
		for (auto now = state.load(std::memory_order_acquire); now != k_Saved; now = state.load(std::memory_order_acquire))
		{
			state.wait(now, std::memory_order_acquire);
		}
		return rows;
	}
};

/// The parts of the rows of one component, as the thread that mixes them gets them. The thread saves a part that no
/// thread took. While another thread saves the part, this thread saves the next parts that no thread took. It waits only
/// when no such part is left. A thread that saves a part never waits, so the wait always ends.
struct SharedParts
{
	std::span<SharedPart> parts;
	const ComponentStorage* component {nullptr};
	const RowLayout* layout {nullptr};
	BinaryWriter* out {nullptr};
	/// The first part that this thread did not try to take yet
	mutable size_t next {0};

	[[nodiscard]] size_t size() const { return parts.size(); }
	[[nodiscard]] bool empty() const { return parts.empty(); }
	[[nodiscard]] const SavedRows& operator[](size_t index) const
	{
		auto& part = parts[index];
		static_cast<void>(part.Claim(*component, *layout, *out));
		for (next = std::max(next, index + 1); !part.IsSaved() && next < parts.size(); ++next)
		{
			static_cast<void>(parts[next].Claim(*component, *layout, *out));
		}
		return part.Wait();
	}
	/// The bytes of a part after the thread that mixes the parts got the part
	[[nodiscard]] std::vector<uint8_t>& BytesOf(size_t index) const { return parts[index].rows.bytes; }
};

/// Finds the bytes of a memo at a place among the bytes of all its rows. The rows come in their order, so the search
/// goes from the chunk of the last row.
class MemoBytes
{
public:
	explicit MemoBytes(const HashMemo::Component& memo)
	    : _chunks(memo.chunks)
	{
	}

	/// Whether the memo has these bytes at this place
	[[nodiscard]] bool Has(std::span<const uint8_t> bytes, size_t start)
	{
		while (_chunk < _chunks.size() && start >= _base + _chunks[_chunk].size())
		{
			_base += _chunks[_chunk].size();
			++_chunk;
		}
		if (_chunk == _chunks.size() || start - _base + bytes.size() > _chunks[_chunk].size())
		{
			return false;
		}
		return std::ranges::equal(bytes, std::span<const uint8_t>(_chunks[_chunk]).subspan(start - _base, bytes.size()));
	}

private:
	std::span<const std::vector<uint8_t>> _chunks;
	size_t _chunk {0};
	/// The place of the first byte of the chunk among the bytes of all the rows
	size_t _base {0};
};

/// Hashes the rows of one component. The writer is the scratch memory of the caller. The parts, when there are some,
/// hold the bytes of all the rows, for a hash with no hash of each field, no visitor and no rows.
[[nodiscard]] ComponentHash HashComponent(const ComponentStorage& component, const RowLayout& layout, bool byField,
                                          const RowVisitor& visit, BinaryWriter& out, bool keepRows, HashMemo::Component* memo,
                                          const SharedParts& parts)
{
	ComponentHash result;
	const auto& left = layout.shape->left;
#if defined(OPENBLACK_DEVELOPER_TOOLS)
	// Tools: the rows also keep the fields that the hash leaves out, with a mark, so that a compare shows what each
	// computer draws of the thing that parts
	if (keepRows)
	{
		KeepLeftOutRows(
		    component, layout.shape->codec, layout.shape->fields,
		    [&layout](size_t index, entt::entity row) { return layout.shape->Keeps(index, row); }, out, result.rows);
	}
	// A component with no values is one row of no bytes for each entity, so that a compare sees the mark
	if (keepRows && layout.shape->codec == nullptr && layout.shape->fields.empty())
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
	result.fields.assign(byField ? std::max<size_t>(layout.shape->fields.size(), 1) : 0, k_HashOffset);
	// With the memo, the bytes of all the rows of the component stay one after the other. While each row is the same as
	// in the memo, the hash after the row comes from the memo. From the first row that changed, the hash mixes each row.
	const bool remembers = memo != nullptr && !byField && !visit && !keepRows;
	std::vector<HashMemo::Row> remembered;
	bool same = remembers;
	std::optional<MemoBytes> memoBytes;
	if (remembers)
	{
		remembered = std::move(memo->spareRows);
		remembered.clear();
		remembered.reserve(layout.rows.size());
		memoBytes.emplace(*memo);
	}
	// Mixes a row whose bytes are at a place among the bytes of all the rows
	const auto mixRow = [&](std::span<const uint8_t> bytes, size_t rowStart) {
		result.kept = true;
		if (!remembers)
		{
			MixBytes(result.hash, bytes);
			return;
		}
		const auto place = remembered.size();
		const auto end = rowStart + bytes.size();
		same = same && place < memo->rows.size() && memo->rows[place].end == end && memoBytes->Has(bytes, rowStart);
		if (same)
		{
			result.hash = memo->rows[place].hash;
		}
		else
		{
			MixBytes(result.hash, bytes);
		}
		remembered.push_back({.end = end, .hash = result.hash});
	};
	if (!parts.empty())
	{
		// The place of each row counts the bytes of the parts before it
		size_t partStart = 0;
		for (size_t index = 0; index < parts.size(); ++index)
		{
			const auto& part = parts[index];
			size_t rowStart = 0;
			for (const auto end : part.ends)
			{
				mixRow(std::span<const uint8_t>(part.bytes).subspan(rowStart, end - rowStart), partStart + rowStart);
				rowStart = end;
			}
			partStart += part.bytes.size();
		}
		if (remembers)
		{
			// The memo takes the bytes of each part as a chunk, and each part keeps the memory of the last chunk
			memo->chunks.resize(parts.size());
			for (size_t index = 0; index < parts.size(); ++index)
			{
				memo->chunks[index].swap(parts.BytesOf(index));
			}
			memo->spareRows = std::exchange(memo->rows, std::move(remembered));
		}
		return result;
	}
	if (remembers)
	{
		out.Clear();
	}
	size_t taken = 0;
	size_t place = 0;
	for (size_t index = 0; index < layout.rows.size(); ++index)
	{
		const auto row = layout.rows[index];
		if (layout.LeavesOutRow(row))
		{
			continue;
		}
		const auto rowPlace = place++;
		const auto* value = layout.ValueOf(component, index);
		// Without the memo, each row clears the writer, and its memory stays for the next row
		if (!remembers)
		{
			out.Clear();
		}
		const auto rowStart = out.Size();
		if ((layout.shape->bySteps || layout.last.has_value()) && !byField && !visit && !keepRows)
		{
			taken += SaveKeptRow(component, layout, index, rowPlace, out) ? 1 : 0;
			mixRow(out.Written(rowStart), rowStart);
			continue;
		}
		SaveRow(component, layout, row, value, out, [&](size_t field, size_t start) {
			const auto bytes = out.Written(start);
			if (byField)
			{
				MixBytes(result.fields.at(field), bytes);
			}
#if defined(OPENBLACK_DEVELOPER_TOOLS)
			if (keepRows)
			{
				auto& kept = result.rows;
				kept.rows.push_back({.entity = row,
				                     .field = static_cast<uint32_t>(field),
				                     .offset = static_cast<uint32_t>(kept.bytes.size()),
				                     .size = static_cast<uint32_t>(bytes.size())});
				kept.bytes.insert(kept.bytes.end(), bytes.begin(), bytes.end());
			}
#endif
			if (visit)
			{
				visit(component.name, row,
				      layout.shape->codec != nullptr ? k_WholeComponent
				                                     : std::string_view {layout.shape->fields.at(field).info->name},
				      bytes);
			}
		});
		mixRow(out.Written(rowStart), rowStart);
	}
	if (remembers)
	{
		// The memo takes the bytes, and the writer keeps the memory of the last bytes of the memo as its room
		memo->chunks.resize(1);
		memo->chunks.front().swap(out.Bytes());
		memo->spareRows = std::exchange(memo->rows, std::move(remembered));
	}
	if (taken != 0)
	{
		layout.memo->rowsTaken.fetch_add(taken, std::memory_order_relaxed);
	}
	return result;
}

/// The hash of the entities, with the bytes that a save writes: the count of the entities, the count of the live
/// entities, then each entity
[[nodiscard]] uint64_t HashEntities(const entt::registry& registry)
{
	const auto* entities = registry.storage<entt::entity>();
	const std::array counts {static_cast<uint32_t>(entities != nullptr ? entities->size() : 0),
	                         static_cast<uint32_t>(entities != nullptr ? entities->free_list() : 0)};
	auto hash = k_HashOffset;
	MixBytes(hash, std::span(reinterpret_cast<const uint8_t*>(counts.data()), sizeof(counts)));
	if (entities != nullptr && !entities->empty())
	{
		MixBytes(hash, std::span(reinterpret_cast<const uint8_t*>(entities->data()), entities->size() * sizeof(entt::entity)));
	}
	return hash;
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

size_t openblack::ecs::reflection::RowsInAPart(size_t lastBytes, size_t lastRows)
{
	if (lastRows == 0 || lastBytes == 0)
	{
		return k_MostRowsInAPart;
	}
	const auto rowBytes = std::max<size_t>(lastBytes / lastRows, 1);
	return std::clamp<size_t>(k_BytesInAPart / rowBytes, 1, k_MostRowsInAPart);
}

std::vector<std::string> openblack::ecs::reflection::UnregisteredStorages(const entt::registry& registry,
                                                                          const entt::meta_ctx& context)
{
	std::vector<std::string> unregistered;
	static_cast<void>(RegisteredStorages(registry, context, unregistered));
	return unregistered;
}

openblack::ecs::reflection::RegistryStores openblack::ecs::reflection::StoresOf(const entt::registry& registry)
{
	RegistryStores stores;
	for (const auto& [id, storage] : registry.storage())
	{
		stores.emplace_back(id, &storage);
	}
	return stores;
}

openblack::ecs::reflection::WorldHashes openblack::ecs::reflection::HashWorld(const entt::registry& registry,
                                                                              const entt::meta_ctx& context,
                                                                              std::span<const HashLeaveOut> leaveOut,
                                                                              bool byField, const RowVisitor& visit,
                                                                              WorkerPool* pool, WorldRows* rows, HashMemo* memo)
{
	return HashWorld(registry, StoresOf(registry), context, leaveOut, byField, visit, pool, rows, memo);
}

openblack::ecs::reflection::WorldHashes
openblack::ecs::reflection::HashWorld(const entt::registry& registry, const RegistryStores& stores,
                                      const entt::meta_ctx& context, std::span<const HashLeaveOut> leaveOut, bool byField,
                                      const RowVisitor& visit, WorkerPool* pool, WorldRows* rows, HashMemo* memo)
{
	WorldHashes hashes;
	const auto components = RegisteredStorages(stores, context, hashes.unregistered, memo);
	// A hash with no hash of each field, no visitor and no rows remembers each component. Such a hash can take the rows
	// with no mark from the last hash when the marks are complete since that hash.
	const bool remembers = memo != nullptr && !byField && !visit && rows == nullptr;
	const bool takesRows = remembers && memo->marks.Usable(&registry);
	std::vector<ComponentHash> results(components.size());
	std::vector<RowLayout> layouts(components.size());
	const auto marked = FindMarkedNumbers(leaveOut);
	// The memo of each component, found before the threads share the components
	std::vector<HashMemo::Component*> memos(components.size(), nullptr);
	if (memo != nullptr)
	{
		// The layouts in the memo stay while the parts that the hash leaves out stay the same
		const auto sameLeaveOut = std::ranges::equal(memo->leaveOut, leaveOut, [](const auto& kept, const auto& part) {
			return kept.component == part.component && kept.field == part.field && kept.marker == part.marker;
		});
		if (!sameLeaveOut)
		{
			memo->leaveOut.clear();
			std::ranges::transform(leaveOut, std::back_inserter(memo->leaveOut), [](const HashLeaveOut& part) {
				return HashMemo::LeftOut {
				    .component = std::string(part.component), .field = std::string(part.field), .marker = part.marker};
			});
			for (auto& [name, kept] : memo->components)
			{
				kept.layout.reset();
			}
		}
		for (size_t index = 0; index < components.size(); ++index)
		{
			memos[index] = &memo->components[components[index].name];
		}
	}
	// The hash of a component does not use the other components, so the threads of the pool can share them. The
	// visitor of the tools must get the rows in order, so then this thread does all of them.
	if (pool != nullptr && !visit)
	{
		// The writers of the threads come from the memo when there is one, with their memory and the plans of the types
		std::vector<BinaryWriter> ownWriters;
		auto& writers = memo != nullptr ? memo->writers : ownWriters;
		if (writers.size() != pool->GetLaneCount())
		{
			writers.assign(pool->GetLaneCount(), BinaryWriter(context));
		}
		std::ranges::for_each(writers, &BinaryWriter::ClearForNextWorld);
		pool->ForEach(components.size(), [&](size_t index, size_t) {
			layouts[index] = LayoutOf(components[index], leaveOut, memos[index]);
			layouts[index].marked = &marked;
			if (takesRows)
			{
				FindLastRows(components[index], layouts[index], *memo, memos[index]);
			}
		});
		// The bytes of each component in the last hash. The threads take the largest work first, so that no thread
		// starts a large component when the others are near the end.
		std::vector<size_t> lastBytes(components.size(), 0);
		for (size_t index = 0; index < components.size(); ++index)
		{
			if (memos[index] != nullptr)
			{
				lastBytes[index] = memos[index]->Bytes();
			}
		}
		// A component with many rows, or with large rows, saves its rows in parts on all the threads. One thread mixes the
		// parts in order while the other threads save the next parts. This is only for a hash with no hash of each field
		// and no rows.
		std::vector<std::span<SharedPart>> parts(components.size());
		std::vector<SharedPart> allParts;
		struct Job
		{
			/// The component that the job hashes, or the component of the part that the job saves
			size_t component;
			/// The part that the job saves, none for a job that hashes the component
			std::optional<size_t> part;
		};
		/// The jobs of one component: its hash, then the saves of its parts
		struct Group
		{
			size_t component;
			size_t bytes;
		};
		std::vector<Group> groups;
		if (!byField && rows == nullptr)
		{
			std::vector<size_t> inAPart(components.size(), 0);
			std::vector<size_t> partCounts(components.size(), 0);
			for (size_t index = 0; index < components.size(); ++index)
			{
				const auto count = layouts[index].rows.size();
				const auto lastRows = memos[index] != nullptr ? memos[index]->rows.size() : 0;
				inAPart[index] = RowsInAPart(lastBytes[index], lastRows);
				if (!layouts[index].shape->left.whole && count >= 2 * inAPart[index])
				{
					partCounts[index] = (count + inAPart[index] - 1) / inAPart[index];
				}
			}
			allParts = std::vector<SharedPart>(std::reduce(partCounts.begin(), partCounts.end()));
			size_t next = 0;
			for (size_t index = 0; index < components.size(); ++index)
			{
				if (partCounts[index] == 0)
				{
					continue;
				}
				const auto count = layouts[index].rows.size();
				parts[index] = std::span<SharedPart>(allParts).subspan(next, partCounts[index]);
				next += partCounts[index];
				for (size_t part = 0; part < partCounts[index]; ++part)
				{
					auto& shared = parts[index][part];
					shared.first = part * inAPart[index];
					shared.last = std::min(shared.first + inAPart[index], count);
					// The place of the first row of the part, for the rows that the hash takes from the last hash
					if (const auto& layout = layouts[index]; layout.last.has_value() && part != 0)
					{
						const auto& before = parts[index][part - 1];
						const auto partRows = std::span(layout.rows).subspan(before.first, before.last - before.first);
						shared.firstPlace = before.firstPlace +
						                    static_cast<size_t>(std::ranges::count_if(
						                        partRows, [&layout](entt::entity row) { return !layout.LeavesOutRow(row); }));
					}
					// The part uses the memory of a part of the last hash of the component
					if (auto* kept = memos[index]; kept != nullptr && !kept->spareBytes.empty() && !kept->spareEnds.empty())
					{
						shared.rows.bytes = std::move(kept->spareBytes.back());
						shared.rows.ends = std::move(kept->spareEnds.back());
						kept->spareBytes.pop_back();
						kept->spareEnds.pop_back();
					}
				}
			}
		}
		for (size_t index = 0; index < components.size(); ++index)
		{
			groups.push_back({.component = index, .bytes = lastBytes[index]});
		}
		// The threads take the largest work first, so that no thread starts a large job when the others are near the
		// end. The hash of a component in parts comes just before the saves of its parts, so that the other threads save
		// the parts while one thread mixes them in order. The thread that mixes saves the first part itself, so the save
		// of the first part is the last job of the group.
		std::ranges::stable_sort(groups, std::ranges::greater {}, &Group::bytes);
		std::vector<Job> jobs {{.component = components.size(), .part = std::nullopt}};
		for (const auto& group : groups)
		{
			jobs.push_back({.component = group.component, .part = std::nullopt});
			const auto count = parts[group.component].size();
			for (size_t part = 1; part < count; ++part)
			{
				jobs.push_back({.component = group.component, .part = part});
			}
			if (count != 0)
			{
				jobs.push_back({.component = group.component, .part = 0});
			}
		}
		pool->ForEach(jobs.size(), [&](size_t job, size_t lane) {
			const auto& found = jobs[job];
			const auto index = found.component;
			// The first job hashes the entities
			if (index == components.size())
			{
				hashes.entities = HashEntities(registry);
				return;
			}
			if (found.part.has_value())
			{
				static_cast<void>(parts[index][*found.part].Claim(components[index], layouts[index], writers[lane]));
				return;
			}
			const SharedParts shared {
			    .parts = parts[index], .component = &components[index], .layout = &layouts[index], .out = &writers[lane]};
			results[index] = HashComponent(components[index], layouts[index], byField, visit, writers[lane], rows != nullptr,
			                               memos[index], shared);
		});
		// The memory of the parts stays for the next hash of each component
		for (size_t index = 0; index < components.size(); ++index)
		{
			if (auto* kept = memos[index]; kept != nullptr)
			{
				for (auto& part : parts[index])
				{
					kept->spareBytes.push_back(std::move(part.rows.bytes));
					kept->spareEnds.push_back(std::move(part.rows.ends));
				}
			}
		}
	}
	else
	{
		hashes.entities = HashEntities(registry);
		BinaryWriter out(context);
		for (size_t index = 0; index < components.size(); ++index)
		{
			layouts[index] = LayoutOf(components[index], leaveOut, memos[index]);
			layouts[index].marked = &marked;
			if (takesRows)
			{
				FindLastRows(components[index], layouts[index], *memo, memos[index]);
			}
			results[index] = HashComponent(components[index], layouts[index], byField, visit, out, rows != nullptr,
			                               memos[index], SharedParts {});
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
	// The memo keeps the memory of the lists of the rows for the next hash
	for (size_t index = 0; index < components.size(); ++index)
	{
		if (memos[index] != nullptr)
		{
			memos[index]->spareLiveRows = std::move(layouts[index].rows);
		}
	}
	// The memo keeps the bytes of each row of this world, so the marks from now on can be complete
	if (remembers)
	{
		memo->marks.Used();
	}
	return hashes;
}

std::vector<std::pair<std::string, uint64_t>> openblack::ecs::reflection::RowOrders(const entt::registry& registry,
                                                                                    const entt::meta_ctx& context,
                                                                                    std::span<const HashLeaveOut> leaveOut)
{
	std::vector<std::string> unregistered;
	std::vector<std::pair<std::string, uint64_t>> orders;
	for (const auto& component : RegisteredStorages(registry, context, unregistered))
	{
		const auto* codec = ComponentCodec(component.type);
		const auto fields = KeptFields(component.type);
		if (codec == nullptr && fields.empty())
		{
			continue;
		}
		std::vector<std::string_view> names;
		if (codec != nullptr)
		{
			names.push_back(k_WholeComponent);
		}
		else
		{
			std::ranges::transform(fields, std::back_inserter(names),
			                       [](const KeptField& field) { return std::string_view {field.info->name}; });
		}
		const auto left = FindLeaveOut(leaveOut, component.name, names);
		if (left.whole || std::ranges::all_of(left.fieldAlways, [](bool always) { return always; }))
		{
			continue;
		}
		uint64_t order = k_HashOffset;
		for (const auto row : LiveRows(*component.storage))
		{
			const auto value = entt::to_integral(row);
			MixBytes(order, std::span(reinterpret_cast<const uint8_t*>(&value), sizeof(value)));
		}
		orders.emplace_back(component.name, order);
	}
	return orders;
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

void openblack::ecs::reflection::MixBytes(uint64_t& hash, std::span<const uint8_t> bytes)
{
	// A byte of zero only multiplies the hash by the prime. A run of bytes of zero multiplies it by the prime to the power
	// of the length of the run, which gives the same hash with few steps. The rows of a world have long runs of zero.
	constexpr size_t k_WordBytes = sizeof(uint64_t);
	// The prime to the power of eight times each power of two
	constexpr auto k_ZeroWordPowers = [] {
		std::array<uint64_t, 64> powers {};
		uint64_t power = 1;
		for (size_t i = 0; i < k_WordBytes; ++i)
		{
			power *= k_HashPrime;
		}
		for (auto& kept : powers)
		{
			kept = power;
			power *= power;
		}
		return powers;
	}();
	const auto wordAt = [&bytes](size_t at) {
		uint64_t word = 0;
		std::memcpy(&word, bytes.data() + at, k_WordBytes);
		return word;
	};
	size_t at = 0;
	while (at + k_WordBytes <= bytes.size())
	{
		if (wordAt(at) != 0)
		{
			for (const auto byte : bytes.subspan(at, k_WordBytes))
			{
				hash = (hash ^ byte) * k_HashPrime;
			}
			at += k_WordBytes;
			continue;
		}
		size_t words = 1;
		while (at + ((words + 1) * k_WordBytes) <= bytes.size() && wordAt(at + (words * k_WordBytes)) == 0)
		{
			++words;
		}
		at += words * k_WordBytes;
		for (size_t bit = 0; words != 0; ++bit, words >>= 1U)
		{
			if ((words & 1U) != 0)
			{
				hash *= k_ZeroWordPowers.at(bit);
			}
		}
	}
	for (const auto byte : bytes.subspan(at))
	{
		hash = (hash ^ byte) * k_HashPrime;
	}
}
