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

class BinaryWriter
{
public:
	explicit BinaryWriter(const entt::meta_ctx& context)
	    : _context(&context)
	{
	}

	[[nodiscard]] const entt::meta_ctx& Context() const { return *_context; }
	[[nodiscard]] std::vector<uint8_t>& Bytes() { return _bytes; }
	[[nodiscard]] const std::vector<uint8_t>& Bytes() const { return _bytes; }

	void Write(std::span<const uint8_t> bytes) { _bytes.insert(_bytes.end(), bytes.begin(), bytes.end()); }

	template <typename Type>
	    requires std::is_trivially_copyable_v<Type>
	void WriteRaw(const Type& value)
	{
		const auto at = _bytes.size();
		_bytes.resize(at + sizeof(Type));
		std::memcpy(_bytes.data() + at, &value, sizeof(Type));
	}

	/// A count or a length, as 32 bits
	void WriteSize(size_t size) { WriteRaw(static_cast<uint32_t>(size)); }

	/// Puts a length of 32 bits that a later call to EndLength fills. Returns where the length is.
	[[nodiscard]] size_t BeginLength()
	{
		const auto at = _bytes.size();
		WriteRaw(uint32_t {0});
		return at;
	}
	void EndLength(size_t at)
	{
		const auto length = static_cast<uint32_t>(_bytes.size() - at - sizeof(uint32_t));
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

private:
	const entt::meta_ctx* _context;
	std::vector<uint8_t> _bytes;
	std::string_view _owner;
	std::string_view _field;
	std::set<UnsavedValue> _unsaved;
	/// The places and types in the list, by their names that the caller keeps, so that each one costs little again
	std::set<std::tuple<std::string_view, std::string_view, std::string_view>> _seen;
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
	if constexpr (std::is_same_v<Type, bool>)
	{
		out.WriteRaw(static_cast<uint8_t>(value ? 1 : 0));
	}
	else if constexpr (std::is_same_v<Type, GameHeading>)
	{
		out.WriteRaw(value.Radians());
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
	else if constexpr (std::is_class_v<Type>)
	{
		SaveRegistered(out, entt::meta_any {out.Context(), std::in_place_type<const Type&>, value});
	}
	else
	{
		out.Unsupported(entt::type_id<Type>().name());
	}
}

template <typename Type>
void Load(BinaryReader& in, Type& value)
{
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
	else if constexpr (detail::k_IsStdArray<Type>)
	{
		const auto size = in.ReadSize();
		if (!size.has_value() || *size != value.size())
		{
			in.Fail();
			return;
		}
		for (auto& element : value)
		{
			Load(in, element);
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
	else if constexpr (std::is_class_v<Type>)
	{
		entt::meta_any reference {in.Context(), std::in_place_type<Type&>, value};
		LoadRegistered(in, reference);
	}
}

} // namespace openblack::ecs::reflection
