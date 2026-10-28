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
#include <cstring>

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <entt/core/type_info.hpp>
#include <entt/entity/entity.hpp>
#include <entt/meta/context.hpp>
#include <entt/meta/meta.hpp>

#include "Common/GameHeading.h"
#include "PlainImage.h"
#include "ReflectionTraits.h"

/// The binary archive of the reflection. It writes the value of a registered field with no loss: floats as their bits,
/// every element of a list, every level of nesting. It reads the value back in the same build. An entity that a value
/// holds goes through a map when the archive reads it, so that a reference points to the entity that the load made.
namespace openblack::ecs::reflection
{

/// How the entities of a saved world become the entities of the loaded world. With no table, each entity stays the same.
/// With a table, an entity that is not in the table becomes null.
class EntityRemap
{
public:
	EntityRemap() = default;
	explicit EntityRemap(std::unordered_map<entt::entity, entt::entity> table)
	    : _table(std::move(table))
	{
	}

	[[nodiscard]] entt::entity operator()(entt::entity saved) const
	{
		if (!_table.has_value() || saved == entt::null)
		{
			return saved;
		}
		const auto found = _table->find(saved);
		return found != _table->end() ? found->second : entt::entity {entt::null};
	}

	[[nodiscard]] bool IsIdentity() const { return !_table.has_value(); }

private:
	std::optional<std::unordered_map<entt::entity, entt::entity>> _table;
};

/// A value that the archive cannot keep: the field that holds it, and the name of its type
struct UnsavedValue
{
	std::string place;
	std::string type;

	auto operator<=>(const UnsavedValue&) const = default;
};

class BinaryWriter;

/// How a writer writes a value of a registered type with no reflection on the way: the codec of the whole type, or the
/// writer of each field in the order of the fields. A writer finds it the first time that it writes a value of the type.
struct SavePlan
{
	void (*codec)(const void* value, BinaryWriter& out) {nullptr};
	std::vector<void (*)(const void* owner, BinaryWriter& out)> fields;
};

class BinaryWriter
{
public:
	explicit BinaryWriter(const entt::meta_ctx& context)
	    : _context(&context)
	{
	}

	[[nodiscard]] const entt::meta_ctx& Context() const { return *_context; }
	/// The bytes written. The writer keeps no room past them while the caller holds the list, and writes after the
	/// end of the list as the caller left it.
	[[nodiscard]] std::vector<uint8_t>& Bytes()
	{
		Settle();
		return _bytes;
	}
	[[nodiscard]] const std::vector<uint8_t>& Bytes() const
	{
		Settle();
		return _bytes;
	}
	/// The count of the bytes written
	[[nodiscard]] size_t Size() const { return _open ? _size : _bytes.size(); }
	/// The bytes written from a place to the end, with no change to the room that the writer keeps
	[[nodiscard]] std::span<const uint8_t> Written(size_t from = 0) const
	{
		return std::span<const uint8_t>(_bytes.data(), Size()).subspan(from);
	}
	/// Removes all the bytes and keeps their memory as room for the next ones
	void Clear()
	{
		_size = 0;
		_open = true;
	}

	void Write(std::span<const uint8_t> bytes)
	{
		if (!bytes.empty())
		{
			std::memcpy(Grow(bytes.size()), bytes.data(), bytes.size());
		}
	}

	template <typename Type>
	    requires std::is_trivially_copyable_v<Type>
	void WriteRaw(const Type& value)
	{
		if constexpr (std::is_class_v<Type>)
		{
			// A value with fields has zero in its gaps, so that the same value always gives the same bytes
			const auto image = PlainImage(value);
			std::memcpy(Grow(sizeof(Type)), image.bytes.data(), sizeof(Type));
		}
		else
		{
			std::memcpy(Grow(sizeof(Type)), &value, sizeof(Type));
		}
	}

	/// A count or a length, as 32 bits
	void WriteSize(size_t size) { WriteRaw(static_cast<uint32_t>(size)); }

	/// The place for a count of bytes at the end of the bytes written. The caller writes all of them before the next
	/// call to the writer.
	[[nodiscard]] std::span<uint8_t> Extend(size_t count) { return {Grow(count), count}; }

	/// Puts a length of 32 bits that a later call to EndLength fills. Returns where the length is.
	[[nodiscard]] size_t BeginLength()
	{
		const auto at = Size();
		WriteRaw(uint32_t {0});
		return at;
	}
	void EndLength(size_t at)
	{
		const auto length = static_cast<uint32_t>(Size() - at - sizeof(uint32_t));
		std::memcpy(_bytes.data() + at, &length, sizeof(length));
	}

	/// The type and the field that the next values come from, for the list of unsaved values. The caller keeps the
	/// names while the writer uses them.
	void SetPlace(std::string_view owner, std::string_view field)
	{
		_owner = owner;
		_field = field;
	}
	/// Records a value of a type that the archive cannot keep. The archive writes nothing for it.
	void Unsupported(std::string_view type)
	{
		if (_seen.insert({_owner, _field, type}).second)
		{
			_unsaved.insert({.place = std::string(_owner) + "." + std::string(_field), .type = std::string(type)});
		}
	}
	[[nodiscard]] const std::set<UnsavedValue>& Unsaved() const { return _unsaved; }

	/// For tools: the writer keeps where each value that the caller saves ends, so that a tool can find the values that
	/// changed between two saves
	void KeepValueEnds() { _keepValueEnds = true; }
	[[nodiscard]] const std::vector<size_t>& ValueEnds() const { return _valueEnds; }
	/// A value starts and ends. Only the ends of the outer values are kept.
	void EnterValue() { ++_depth; }
	void LeaveValue()
	{
		--_depth;
		if (_depth == 0 && _keepValueEnds)
		{
			_valueEnds.push_back(Size());
		}
	}
	/// The plan of a type by the hash of its type, none before the writer kept one
	[[nodiscard]] const SavePlan* FindPlan(entt::id_type type) const
	{
		const auto found = _plans.find(type);
		return found != _plans.end() ? &found->second : nullptr;
	}
	void KeepPlan(entt::id_type type, SavePlan plan) { _plans.insert_or_assign(type, std::move(plan)); }
	/// Writes a value by the plan of its type. Returns false, with nothing written, when the writer has no plan of it.
	bool SaveByPlan(entt::id_type type, const void* value)
	{
		const auto* plan = FindPlan(type);
		if (plan == nullptr)
		{
			return false;
		}
		if (plan->codec != nullptr)
		{
			plan->codec(value, *this);
			return true;
		}
		for (const auto saveFrom : plan->fields)
		{
			saveFrom(value, *this);
		}
		return true;
	}

private:
	/// The most room that the writer adds past the bytes in one step, when the list has that much memory already. The
	/// room is filled with zeros one time, not for each value.
	static constexpr size_t k_MostRoom = 512;

	/// The place for the next count of bytes, at the end of the bytes written
	[[nodiscard]] uint8_t* Grow(size_t count)
	{
		if (!_open)
		{
			_size = _bytes.size();
			_open = true;
		}
		const auto needed = _size + count;
		if (_bytes.size() < needed)
		{
			_bytes.resize(std::max(needed, std::min(_bytes.capacity(), _size + k_MostRoom)));
		}
		auto* place = _bytes.data() + _size;
		_size = needed;
		return place;
	}
	/// Removes the room past the bytes written, so that the list holds only them
	void Settle() const
	{
		if (_open)
		{
			_bytes.resize(_size);
			_open = false;
		}
	}

	const entt::meta_ctx* _context;
	bool _keepValueEnds {false};
	uint32_t _depth {0};
	std::vector<size_t> _valueEnds;
	/// The bytes written, and while the writer is open, room for more past them
	mutable std::vector<uint8_t> _bytes;
	/// While the writer is open: the count of the bytes written, which the list holds with room after them
	mutable size_t _size {0};
	mutable bool _open {false};
	std::string_view _owner;
	std::string_view _field;
	std::set<UnsavedValue> _unsaved;
	/// The places and types in the list, by their names that the caller keeps, so that each one costs little again
	std::set<std::tuple<std::string_view, std::string_view, std::string_view>> _seen;
	/// The plans of the registered types that this writer wrote, by the hashes of their types
	std::unordered_map<entt::id_type, SavePlan> _plans;
};

class BinaryReader
{
public:
	BinaryReader(const entt::meta_ctx& context, std::span<const uint8_t> bytes, const EntityRemap& remap)
	    : _context(&context)
	    , _bytes(bytes)
	    , _remap(&remap)
	{
	}

	[[nodiscard]] const entt::meta_ctx& Context() const { return *_context; }
	[[nodiscard]] const EntityRemap& Remap() const { return *_remap; }
	[[nodiscard]] bool Failed() const { return _failed; }
	void Fail() { _failed = true; }
	[[nodiscard]] size_t Remaining() const { return _failed ? 0 : _bytes.size() - _at; }
	[[nodiscard]] size_t Position() const { return _at; }

	/// The next bytes, or none (and the reader fails) when there are not that many
	[[nodiscard]] std::optional<std::span<const uint8_t>> Take(size_t count)
	{
		if (_failed || count > _bytes.size() - _at)
		{
			_failed = true;
			return std::nullopt;
		}
		const auto taken = _bytes.subspan(_at, count);
		_at += count;
		return taken;
	}

	template <typename Type>
	    requires std::is_trivially_copyable_v<Type>
	bool ReadRaw(Type& value)
	{
		const auto taken = Take(sizeof(Type));
		if (!taken.has_value())
		{
			return false;
		}
		std::memcpy(&value, taken->data(), sizeof(Type));
		return true;
	}

	/// A count or a length of 32 bits. A count that is much larger than the bytes that remain fails the reader.
	[[nodiscard]] std::optional<size_t> ReadSize()
	{
		uint32_t size = 0;
		if (!ReadRaw(size))
		{
			return std::nullopt;
		}
		if (size > Remaining() && size > k_MostEmptyElements)
		{
			_failed = true;
			return std::nullopt;
		}
		return size;
	}

private:
	/// The most elements of a list whose elements the archive keeps with no bytes
	static constexpr size_t k_MostEmptyElements = 1U << 16U;

	const entt::meta_ctx* _context;
	std::span<const uint8_t> _bytes;
	const EntityRemap* _remap;
	size_t _at {0};
	bool _failed {false};
};

/// Whether the archive can write a value of the type: it has registered fields or a codec of its own
[[nodiscard]] bool IsRegistered(const entt::meta_ctx& context, const entt::type_info& info);
/// Writes a value of a type that has registered fields, or a codec of its own. A type with neither goes into the list
/// of unsaved values.
void SaveRegistered(BinaryWriter& out, const entt::meta_any& value);
/// Reads a value of a type that has registered fields, or a codec of its own, into the value that the reference holds
void LoadRegistered(BinaryReader& in, entt::meta_any& value);

template <typename Type>
void Save(BinaryWriter& out, const Type& value);
template <typename Type>
void Load(BinaryReader& in, Type& value);

/// A codec that writes the bytes of the whole value, for a value type with private data that is plain and holds no
/// entity. The archive keeps the size of the value, so that a value of another size fails the reader.
template <typename Type>
    requires std::is_trivially_copyable_v<Type>
void SaveBytes(const Type& value, BinaryWriter& out)
{
	out.WriteSize(sizeof(Type));
	out.WriteRaw(value);
}

template <typename Type>
    requires std::is_trivially_copyable_v<Type>
void LoadBytes(Type& value, BinaryReader& in)
{
	const auto size = in.ReadSize();
	if (!size.has_value() || *size != sizeof(Type))
	{
		in.Fail();
		return;
	}
	in.ReadRaw(value);
}

namespace detail
{

/// A vector of vectors of values with plain bits
template <typename>
inline constexpr bool k_IsListOfPlainLists = false;
template <typename Type, typename Allocator, typename OuterAllocator>
inline constexpr bool k_IsListOfPlainLists<std::vector<std::vector<Type, Allocator>, OuterAllocator>> = PlainBits<Type>();

/// A fixed list or a vector of the values of an enumeration
template <typename>
inline constexpr bool k_IsEnumList = false;
template <typename Type, size_t Size>
inline constexpr bool k_IsEnumList<std::array<Type, Size>> = std::is_enum_v<Type>;
template <typename Type, typename Allocator>
inline constexpr bool k_IsEnumList<std::vector<Type, Allocator>> = std::is_enum_v<Type>;

template <typename Variant, size_t Index = 0>
void LoadAlternative(BinaryReader& in, Variant& value, size_t index)
{
	if constexpr (Index < std::variant_size_v<Variant>)
	{
		if (index == Index)
		{
			auto& alternative = value.template emplace<Index>();
			Load(in, alternative);
			return;
		}
		LoadAlternative<Variant, Index + 1>(in, value, index);
	}
	else
	{
		in.Fail();
	}
}

/// The entries of a map or a set in the order of their keys, so that the same contents always give the same bytes
template <typename Container>
[[nodiscard]] auto SortedEntries(const Container& container)
{
	std::vector<const typename Container::value_type*> entries;
	entries.reserve(container.size());
	for (const auto& entry : container)
	{
		entries.push_back(&entry);
	}
	if constexpr (k_IsMap<Container>)
	{
		using Key = typename Container::key_type;
		if constexpr (requires(const Key& a, const Key& b) { a < b; })
		{
			std::ranges::sort(entries, [](const auto* a, const auto* b) { return a->first < b->first; });
		}
	}
	else
	{
		using Key = typename Container::key_type;
		if constexpr (requires(const Key& a, const Key& b) { a < b; })
		{
			std::ranges::sort(entries, [](const auto* a, const auto* b) { return *a < *b; });
		}
	}
	return entries;
}

} // namespace detail

template <typename Type>
void Save(BinaryWriter& out, const Type& value)
{
	static_assert(!detail::k_IsRunHandle<Type>, "A save must not keep a pointer or a graphics handle: keep a stable key");
	// The writer knows where each outer value ends
	struct Value
	{
		BinaryWriter& out;
		explicit Value(BinaryWriter& writer)
		    : out(writer)
		{
			out.EnterValue();
		}
		Value(const Value&) = delete;
		Value& operator=(const Value&) = delete;
		~Value() { out.LeaveValue(); }
	} const valueEnds(out);
	if constexpr (std::is_same_v<Type, bool>)
	{
		out.WriteRaw(static_cast<uint8_t>(value ? 1 : 0));
	}
	else if constexpr (std::is_same_v<Type, GameHeading>)
	{
		out.WriteRaw(value.Radians());
	}
	else if constexpr (detail::k_IsBuildSized<Type>)
	{
		out.WriteRaw(static_cast<std::conditional_t<std::is_signed_v<Type>, int64_t, uint64_t>>(value));
	}
	else if constexpr (std::is_arithmetic_v<Type> || std::is_enum_v<Type>)
	{
		out.WriteRaw(value);
	}
	else if constexpr (std::is_same_v<Type, std::string> || std::is_same_v<Type, std::u16string>)
	{
		out.WriteSize(value.size());
		out.Write({reinterpret_cast<const uint8_t*>(value.data()), value.size() * sizeof(typename Type::value_type)});
	}
	else if constexpr (detail::PlainBits<Type>())
	{
		// A glm vector or matrix of numbers: its bytes in memory are its numbers in order
		out.WriteRaw(value);
	}
	else if constexpr (detail::k_IsGlmVector<Type> || detail::k_IsGlmMatrix<Type>)
	{
		for (glm::length_t i = 0; i < Type::length(); ++i)
		{
			Save(out, value[i]);
		}
	}
	else if constexpr (detail::k_IsOptional<Type>)
	{
		Save(out, value.has_value());
		if (value.has_value())
		{
			Save(out, *value);
		}
	}
	else if constexpr (detail::k_IsPlainList<Type>)
	{
		// The same bytes as one value after the other, in one block
		out.WriteSize(value.size());
		out.Write({reinterpret_cast<const uint8_t*>(value.data()), value.size() * sizeof(typename Type::value_type)});
	}
	else if constexpr (detail::k_IsEnumList<Type>)
	{
		// Each value of an enumeration goes as its bytes in memory, so the list is the same bytes in one block. A load
		// reads each value again, so that an entity goes through the map of the entities.
		out.WriteSize(value.size());
		out.Write({reinterpret_cast<const uint8_t*>(value.data()), value.size() * sizeof(typename Type::value_type)});
	}
	else if constexpr (std::is_same_v<Type, std::vector<bool>>)
	{
		// The same bytes as a value of 1 or 0 for each element, in one block. Most flags are false, and the search for the
		// next true flag is quicker than a look at each flag.
		out.WriteSize(value.size());
		auto place = out.Extend(value.size());
		std::ranges::fill(place, uint8_t {0});
		for (auto flag = std::find(value.begin(), value.end(), true); flag != value.end();
		     flag = std::find(std::next(flag), value.end(), true))
		{
			place[static_cast<size_t>(flag - value.begin())] = 1;
		}
	}
	else if constexpr (detail::k_IsListOfPlainLists<Type>)
	{
		// A list of plain lists: the same bytes as each list after the other, in one block
		using Element = typename Type::value_type::value_type;
		size_t count = sizeof(uint32_t);
		for (const auto& list : value)
		{
			count += sizeof(uint32_t) + (list.size() * sizeof(Element));
		}
		auto place = out.Extend(count);
		const auto put = [&place](const void* from, size_t bytes) {
			if (bytes != 0)
			{
				std::memcpy(place.data(), from, bytes);
			}
			place = place.subspan(bytes);
		};
		const auto size = static_cast<uint32_t>(value.size());
		put(&size, sizeof(size));
		for (const auto& list : value)
		{
			const auto listSize = static_cast<uint32_t>(list.size());
			put(&listSize, sizeof(listSize));
			put(list.data(), list.size() * sizeof(Element));
		}
	}
	else if constexpr (detail::k_IsStdArray<Type>)
	{
		out.WriteSize(value.size());
		for (const auto& element : value)
		{
			Save(out, element);
		}
	}
	else if constexpr (detail::k_IsSequence<Type>)
	{
		out.WriteSize(value.size());
		for (const auto& element : value)
		{
			Save(out, static_cast<const typename Type::value_type&>(element));
		}
	}
	else if constexpr (detail::k_IsBitset<Type>)
	{
		out.WriteSize(value.size());
		for (size_t i = 0; i < value.size(); i += 8)
		{
			uint8_t byte = 0;
			for (size_t bit = 0; bit < 8 && i + bit < value.size(); ++bit)
			{
				byte = static_cast<uint8_t>(byte | (value.test(i + bit) ? 1U << bit : 0U));
			}
			out.WriteRaw(byte);
		}
	}
	else if constexpr (detail::k_IsMap<Type>)
	{
		out.WriteSize(value.size());
		for (const auto* entry : detail::SortedEntries(value))
		{
			Save(out, entry->first);
			Save(out, entry->second);
		}
	}
	else if constexpr (detail::k_IsSet<Type>)
	{
		out.WriteSize(value.size());
		for (const auto* entry : detail::SortedEntries(value))
		{
			Save(out, *entry);
		}
	}
	else if constexpr (detail::k_IsPair<Type>)
	{
		Save(out, value.first);
		Save(out, value.second);
	}
	else if constexpr (detail::k_IsVariant<Type>)
	{
		out.WriteSize(value.index());
		std::visit([&out](const auto& alternative) { Save(out, alternative); }, value);
	}
	else if constexpr (detail::k_IsSharedValue<Type>)
	{
		// A shared value goes in whole, and a load makes a value of its own; one of a type with no fields cannot go in
		using Held = std::remove_const_t<typename Type::element_type>;
		const bool kept = value != nullptr && IsRegistered(out.Context(), entt::type_id<Held>());
		if (value != nullptr && !kept)
		{
			out.Unsupported(entt::type_id<Type>().name());
		}
		Save(out, kept);
		if (kept)
		{
			Save(out, static_cast<const Held&>(*value));
		}
	}
	else if constexpr (detail::k_IsIndirect<Type>)
	{
		out.Unsupported(entt::type_id<Type>().name());
	}
	else if constexpr (detail::k_IsDuration<Type>)
	{
		Save(out, value.count());
	}
	else if constexpr (detail::k_IsVisited<Type>)
	{
		Type::VisitSaved(value, [&out](const auto&... fields) { (Save(out, fields), ...); });
	}
	else if constexpr (std::is_class_v<Type>)
	{
		// The writer writes a type that it wrote before by its plan, with no reflection on the way
		if (!out.SaveByPlan(entt::type_hash<Type>::value(), std::addressof(value)))
		{
			SaveRegistered(out, entt::meta_any {out.Context(), std::in_place_type<const Type&>, value});
		}
	}
	else
	{
		out.Unsupported(entt::type_id<Type>().name());
	}
}

template <typename Type>
void Load(BinaryReader& in, Type& value)
{
	static_assert(!detail::k_IsRunHandle<Type>, "A load must not read a pointer or a graphics handle");
	if (in.Failed())
	{
		return;
	}
	if constexpr (std::is_same_v<Type, bool>)
	{
		uint8_t byte = 0;
		if (in.ReadRaw(byte))
		{
			value = byte != 0;
		}
	}
	else if constexpr (std::is_same_v<Type, GameHeading>)
	{
		float radians = 0.0f;
		if (in.ReadRaw(radians))
		{
			value = GameHeading(radians);
		}
	}
	else if constexpr (std::is_same_v<Type, entt::entity>)
	{
		entt::entity saved = entt::null;
		if (in.ReadRaw(saved))
		{
			value = in.Remap()(saved);
		}
	}
	else if constexpr (detail::k_IsBuildSized<Type>)
	{
		std::conditional_t<std::is_signed_v<Type>, int64_t, uint64_t> wide {};
		if (in.ReadRaw(wide))
		{
			if (!std::in_range<Type>(wide))
			{
				in.Fail();
				return;
			}
			value = static_cast<Type>(wide);
		}
	}
	else if constexpr (std::is_arithmetic_v<Type> || std::is_enum_v<Type>)
	{
		in.ReadRaw(value);
	}
	else if constexpr (std::is_same_v<Type, std::string> || std::is_same_v<Type, std::u16string>)
	{
		const auto size = in.ReadSize();
		if (!size.has_value())
		{
			return;
		}
		const auto taken = in.Take(*size * sizeof(typename Type::value_type));
		if (taken.has_value())
		{
			value.resize(*size);
			std::memcpy(value.data(), taken->data(), taken->size());
		}
	}
	else if constexpr (detail::PlainBits<Type>())
	{
		in.ReadRaw(value);
	}
	else if constexpr (detail::k_IsGlmVector<Type> || detail::k_IsGlmMatrix<Type>)
	{
		for (glm::length_t i = 0; i < Type::length(); ++i)
		{
			Load(in, value[i]);
		}
	}
	else if constexpr (detail::k_IsOptional<Type>)
	{
		bool has = false;
		Load(in, has);
		if (!has)
		{
			value.reset();
		}
		else if constexpr (std::is_default_constructible_v<typename Type::value_type>)
		{
			Load(in, value.emplace());
		}
		else
		{
			in.Fail();
		}
	}
	else if constexpr (detail::k_IsPlainList<Type>)
	{
		const auto size = in.ReadSize();
		// A saved fixed array can be shorter than the array of this build, as when a table for each player gets more
		// players. The saved values fill the start, and the rest keep their defaults. A longer saved array fails.
		if (!size.has_value() || (detail::k_IsStdArray<Type> && *size > value.size()))
		{
			in.Fail();
			return;
		}
		const auto taken = in.Take(*size * sizeof(typename Type::value_type));
		if (!taken.has_value())
		{
			return;
		}
		if constexpr (detail::k_IsVector<Type>)
		{
			value.resize(*size);
		}
		std::memcpy(value.data(), taken->data(), taken->size());
	}
	else if constexpr (detail::k_IsStdArray<Type>)
	{
		const auto size = in.ReadSize();
		// A shorter saved array fills the start of the array, as above
		if (!size.has_value() || *size > value.size())
		{
			in.Fail();
			return;
		}
		for (size_t i = 0; i < *size; ++i)
		{
			Load(in, value[i]);
		}
	}
	else if constexpr (detail::k_IsSequence<Type>)
	{
		const auto size = in.ReadSize();
		if (!size.has_value())
		{
			return;
		}
		value.clear();
		if constexpr (std::is_default_constructible_v<typename Type::value_type>)
		{
			value.resize(*size);
			for (size_t i = 0; i < *size && !in.Failed(); ++i)
			{
				if constexpr (std::is_same_v<typename Type::value_type, bool>)
				{
					bool element = false;
					Load(in, element);
					value[i] = element;
				}
				else
				{
					Load(in, value[i]);
				}
			}
		}
		else
		{
			in.Fail();
		}
	}
	else if constexpr (detail::k_IsBitset<Type>)
	{
		const auto size = in.ReadSize();
		if (!size.has_value() || *size != value.size())
		{
			in.Fail();
			return;
		}
		value.reset();
		for (size_t i = 0; i < value.size(); i += 8)
		{
			uint8_t byte = 0;
			if (!in.ReadRaw(byte))
			{
				return;
			}
			for (size_t bit = 0; bit < 8 && i + bit < value.size(); ++bit)
			{
				value.set(i + bit, (byte & (1U << bit)) != 0);
			}
		}
	}
	else if constexpr (detail::k_IsMap<Type>)
	{
		const auto size = in.ReadSize();
		if (!size.has_value())
		{
			return;
		}
		value.clear();
		for (size_t i = 0; i < *size && !in.Failed(); ++i)
		{
			typename Type::key_type key {};
			typename Type::mapped_type mapped {};
			Load(in, key);
			Load(in, mapped);
			value.insert_or_assign(std::move(key), std::move(mapped));
		}
	}
	else if constexpr (detail::k_IsSet<Type>)
	{
		const auto size = in.ReadSize();
		if (!size.has_value())
		{
			return;
		}
		value.clear();
		for (size_t i = 0; i < *size && !in.Failed(); ++i)
		{
			typename Type::key_type key {};
			Load(in, key);
			value.insert(std::move(key));
		}
	}
	else if constexpr (detail::k_IsPair<Type>)
	{
		Load(in, const_cast<std::remove_const_t<typename Type::first_type>&>(value.first));
		Load(in, value.second);
	}
	else if constexpr (detail::k_IsVariant<Type>)
	{
		const auto index = in.ReadSize();
		if (index.has_value())
		{
			detail::LoadAlternative(in, value, *index);
		}
	}
	else if constexpr (detail::k_IsSharedValue<Type>)
	{
		bool kept = false;
		Load(in, kept);
		if (!kept)
		{
			value = nullptr;
			return;
		}
		auto made = std::make_shared<std::remove_const_t<typename Type::element_type>>();
		Load(in, *made);
		value = std::move(made);
	}
	else if constexpr (detail::k_IsIndirect<Type>)
	{
		// The writer kept nothing for it
	}
	else if constexpr (detail::k_IsDuration<Type>)
	{
		typename Type::rep count {};
		Load(in, count);
		value = Type(count);
	}
	else if constexpr (detail::k_IsVisited<Type>)
	{
		Type::VisitSaved(value, [&in](auto&... fields) { (Load(in, fields), ...); });
	}
	else if constexpr (std::is_class_v<Type>)
	{
		entt::meta_any reference {in.Context(), std::in_place_type<Type&>, value};
		LoadRegistered(in, reference);
	}
}

/// A codec for a registered type that gives its values to a visitor (see detail::k_IsVisited)
template <typename Type>
    requires detail::k_IsVisited<Type>
void SaveVisited(const Type& value, BinaryWriter& out)
{
	Save(out, value);
}

template <typename Type>
    requires detail::k_IsVisited<Type>
void LoadVisited(Type& value, BinaryReader& in)
{
	Load(in, value);
}

} // namespace openblack::ecs::reflection
