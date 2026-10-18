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

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <entt/core/type_info.hpp>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/entity/registry.hpp>
#include <entt/meta/container.hpp>
#include <entt/meta/context.hpp>
#include <entt/meta/factory.hpp>
#include <entt/meta/meta.hpp>
#include <entt/meta/policy.hpp>

#include "BinaryArchive.h"
#include "ReflectionTraits.h"

#if defined(OPENBLACK_INSPECTOR)
#include "Inspector/ComponentJsonCodec.h"
#endif

/// The fields of the components, registered once with the reflection of EnTT. The game writes every registered
/// component to the binary archive of a saved world and reads it back. The inspector also writes any registered
/// component as JSON, sets its fields by name with their types checked, and adds it to an entity. Neither needs to know
/// the type of the component.
namespace openblack::ecs::reflection
{

/// The reflection follows nested values this deep when it looks for the entities that they hold
inline constexpr int k_DeepestNesting = 4;

/// Whether a value of a type that holds entities (an entity, an optional one, or a list of them) holds this one
template <typename Type>
[[nodiscard]] constexpr bool HoldsEntities()
{
	if constexpr (std::is_same_v<Type, entt::entity>)
	{
		return true;
	}
	else if constexpr (detail::k_IsOptional<Type> || detail::k_IsList<Type>)
	{
		return std::is_same_v<typename Type::value_type, entt::entity>;
	}
	else
	{
		return false;
	}
}

template <typename Type>
[[nodiscard]] bool RefersTo(const Type& value, entt::entity target)
{
	if constexpr (std::is_same_v<Type, entt::entity>)
	{
		return value == target;
	}
	else if constexpr (detail::k_IsOptional<Type>)
	{
		return value == target;
	}
	else
	{
		return std::find(value.begin(), value.end(), target) != value.end();
	}
}

/// The test of whether a field's value holds an entity, for the types that hold entities; none for the rest
template <typename Type>
[[nodiscard]] constexpr auto RefersToFunction() -> bool (*)(const entt::meta_any&, entt::entity)
{
	if constexpr (HoldsEntities<Type>())
	{
		return [](const entt::meta_any& any, entt::entity target) {
			const auto* value = any.try_cast<const Type>();
			return value != nullptr && RefersTo(*value, target);
		};
	}
	else
	{
		return nullptr;
	}
}

/// A codec of a whole type, for a type whose data is private or that needs more than its fields: how a value of it is
/// written to the binary archive and read back in place
struct TypeCodec
{
	void (*save)(const void* value, BinaryWriter& out) {nullptr};
	void (*load)(void* value, BinaryReader& in) {nullptr};
};

/// What is kept with each registered field: its name, and how its value is written out and read in
struct FieldInfo
{
	std::string name;
	/// For a field holding entities, whether it holds this one; none for any other field
	bool (*refersTo)(const entt::meta_any& value, entt::entity target) {nullptr};
	/// Writes the value of the field to the binary archive; none for a field that the archive does not keep, such as a
	/// value that is only read through a function
	void (*save)(const entt::meta_any& value, BinaryWriter& out) {nullptr};
	/// Reads the value of the field of the instance from the binary archive
	void (*load)(const entt::meta_data& data, entt::meta_any& instance, BinaryReader& in) {nullptr};
#if defined(OPENBLACK_INSPECTOR)
	/// The value as JSON, none when the generic writing through fields must do
	std::optional<Json> (*encode)(const entt::meta_any& value) {nullptr};
	/// A JSON value as a value of the field's type, none (with why not) when it doesn't fit
	std::optional<entt::meta_any> (*decode)(const entt::meta_ctx& context, const Json& value, std::string& error) {nullptr};
#endif
	/// For a list indexed by an enumeration, its elements' names, one list for each level of lists within lists (empty
	/// for a level indexed by number): written as an object by those names, and set by name or by number
	std::vector<std::span<const std::string_view>> indexNames;
};

/// What is kept with a type of value that fields and lists hold (not a component): for an option, how what it holds is
/// reached; for the inspector, how a value of it is read from JSON to set an element of a list; and the codec of a type
/// that has one
struct ValueInfo
{
#if defined(OPENBLACK_INSPECTOR)
	/// A JSON value as a value of the type, none (with why not) when it doesn't fit; none for a type set only through
	/// its fields or elements
	std::optional<entt::meta_any> (*decode)(const entt::meta_ctx& context, const Json& value, std::string& error) {nullptr};
#endif
	/// For an option, what it holds by reference, or an empty value when it holds nothing
	entt::meta_any (*unwrap)(entt::meta_any& option) {nullptr};
	/// For an option, what it holds by reference, made with its default value first when it holds nothing; empty when
	/// it can't be made
	entt::meta_any (*engage)(entt::meta_any& option) {nullptr};
	TypeCodec codec;
};

namespace detail
{

#if defined(OPENBLACK_INSPECTOR)
template <typename Type>
[[nodiscard]] std::optional<entt::meta_any> DecodeAny(const entt::meta_ctx& context, const Json& json, std::string& error)
{
	if constexpr (std::is_copy_constructible_v<Type>)
	{
		auto value = Decode<Type>(json, error);
		if (value.has_value())
		{
			return entt::meta_any {context, std::in_place_type<Type>, *std::move(value)};
		}
	}
	else
	{
		error = "can't be set: its values can't be copied";
	}
	return std::nullopt;
}

/// How the inspector reads a value of the type from JSON, none for a type that it sets only through its fields
template <typename Type>
[[nodiscard]] constexpr auto DecodeFunction()
    -> std::optional<entt::meta_any> (*)(const entt::meta_ctx&, const Json&, std::string&)
{
	if constexpr (Decodable<Type>())
	{
		return &DecodeAny<Type>;
	}
	else
	{
		return nullptr;
	}
}
#endif

/// The information of a value type with its decoder for the inspector, when the build has the inspector
template <typename Type>
[[nodiscard]] ValueInfo ValueInfoFor()
{
	ValueInfo info {};
#if defined(OPENBLACK_INSPECTOR)
	info.decode = DecodeFunction<Type>();
#endif
	return info;
}

/// Whether the inspector reads a value of the type whole, which needs a value information on the type
template <typename Type>
[[nodiscard]] constexpr bool ReadWhole()
{
#if defined(OPENBLACK_INSPECTOR)
	return Decodable<Type>();
#else
	return false;
#endif
}

/// Registers how values of a type held in fields and lists are read and reached, and those of the types it holds
template <typename Type>
void RegisterValue(entt::meta_ctx& context)
{
	if constexpr (k_IsOptional<Type>)
	{
		using Held = typename Type::value_type;
		auto info = ValueInfoFor<Type>();
		info.unwrap = [](entt::meta_any& option) -> entt::meta_any {
			if (auto* value = option.try_cast<Type>(); value != nullptr && value->has_value())
			{
				return entt::meta_any {option.context(), std::in_place_type<Held&>, **value};
			}
			if (const auto* value = option.try_cast<const Type>(); value != nullptr && value->has_value())
			{
				return entt::meta_any {option.context(), std::in_place_type<const Held&>, **value};
			}
			return {};
		};
		info.engage = [](entt::meta_any& option) -> entt::meta_any {
			auto* value = option.try_cast<Type>();
			if (value == nullptr)
			{
				return {};
			}
			if constexpr (std::is_default_constructible_v<Held>)
			{
				if (!value->has_value())
				{
					value->emplace();
				}
			}
			return value->has_value() ? entt::meta_any {option.context(), std::in_place_type<Held&>, **value}
			                          : entt::meta_any {};
		};
		entt::meta_factory<Type> {context}.template custom<ValueInfo>(info);
		RegisterValue<Held>(context);
	}
	else if constexpr (k_IsList<Type>)
	{
		if constexpr (ReadWhole<Type>())
		{
			entt::meta_factory<Type> {context}.template custom<ValueInfo>(ValueInfoFor<Type>());
		}
		RegisterValue<typename Type::value_type>(context);
	}
	else if constexpr (ReadWhole<Type>())
	{
		entt::meta_factory<Type> {context}.template custom<ValueInfo>(ValueInfoFor<Type>());
	}
}

} // namespace detail

/// Registers a value type only for its fields, not as a component that can be added to an entity:
/// Reflect<Needs>(context, ValueOnly {})
struct ValueOnly
{
};

/// What is kept with each registered component: its storage in a registry, made if there is none yet, and the codec of
/// a component that has one
struct ComponentInfo
{
	entt::sparse_set& (*storage)(entt::registry& registry) {nullptr};
	TypeCodec codec;
};

/// Registers a component and its fields by name: Reflect<Transform>(context).Field<&Transform::position>("position")
template <typename Type>
class Reflect
{
public:
	explicit Reflect(entt::meta_ctx& context)
	    : _context(&context)
	    , _factory(context)
	    , _component(ComponentInfo {
	          .storage = [](entt::registry& registry) -> entt::sparse_set& { return registry.storage<Type>(); },
	          .codec = {},
	      })
	{
		_factory.template custom<ComponentInfo>(*_component);
	}

	Reflect(entt::meta_ctx& context, ValueOnly /*value*/)
	    : _context(&context)
	    , _factory(context)
	{
	}

	template <auto Member>
	Reflect& Field(std::string_view name)
	{
		return Field<Member>(name, {});
	}

	/// A field that is a list indexed by enumerations, with their names for each level of lists within lists (an empty
	/// list of names for a level indexed by number)
	template <auto Member>
	Reflect& Field(std::string_view name, std::vector<std::span<const std::string_view>> indexNames)
	{
		using Value = std::remove_cvref_t<decltype(std::declval<Type&>().*Member)>;
		// Fields are reached by reference, so that values that can't be copied are read too, and nested values are set
		// where they are
		_factory.template data<Member, entt::as_ref_t>(entt::hashed_string::value(name.data(), name.size()));
		FieldInfo info {
		    .name = std::string(name),
		    .refersTo = RefersToFunction<Value>(),
		    .save =
		        [](const entt::meta_any& any, BinaryWriter& out) {
			        if (const auto* value = any.try_cast<const Value>(); value != nullptr)
			        {
				        Save(out, *value);
			        }
		        },
		    .load =
		        [](const entt::meta_data& data, entt::meta_any& instance, BinaryReader& in) {
			        auto field = data.get(instance);
			        if (auto* value = field.try_cast<Value>(); value != nullptr)
			        {
				        Load(in, *value);
			        }
			        else
			        {
				        in.Fail();
			        }
		        },
		    .indexNames = std::move(indexNames),
		};
#if defined(OPENBLACK_INSPECTOR)
		info.encode = [](const entt::meta_any& any) -> std::optional<Json> {
			if (const auto* value = any.try_cast<const Value>(); value != nullptr)
			{
				return Encode(*value);
			}
			return std::nullopt;
		};
		info.decode = &detail::DecodeAny<Value>;
#endif
		_factory.template custom<FieldInfo>(std::move(info));
		detail::RegisterValue<Value>(*_context);
		return *this;
	}

	/// A value reached through functions, for a type whose data is private: a getter giving it from a const reference,
	/// and a setter taking it, or nullptr for one that is only read. Set, it is read, changed and set back whole. The
	/// binary archive keeps only a value that has a setter.
	template <auto Getter, auto Setter>
	Reflect& Property(std::string_view name)
	{
		using Value = std::remove_cvref_t<std::invoke_result_t<decltype(Getter), const Type&>>;
		_factory.template data<Setter, Getter>(entt::hashed_string::value(name.data(), name.size()));
		FieldInfo info {
		    .name = std::string(name),
		    .refersTo = RefersToFunction<Value>(),
		};
		if constexpr (!std::is_same_v<decltype(Setter), std::nullptr_t> && std::is_default_constructible_v<Value>)
		{
			info.save = [](const entt::meta_any& any, BinaryWriter& out) {
				if (const auto* value = any.try_cast<const Value>(); value != nullptr)
				{
					Save(out, *value);
				}
			};
			info.load = [](const entt::meta_data& data, entt::meta_any& instance, BinaryReader& in) {
				Value value {};
				Load(in, value);
				if (!in.Failed() && !data.set(instance, value))
				{
					in.Fail();
				}
			};
		}
#if defined(OPENBLACK_INSPECTOR)
		info.encode = [](const entt::meta_any& any) -> std::optional<Json> {
			if (const auto* value = any.try_cast<const Value>(); value != nullptr)
			{
				return Encode(*value);
			}
			return std::nullopt;
		};
		info.decode = &detail::DecodeAny<Value>;
#endif
		_factory.template custom<FieldInfo>(std::move(info));
		detail::RegisterValue<Value>(*_context);
		return *this;
	}

	/// A codec of the whole type, which the binary archive uses in place of the fields: functions that write a value to
	/// the archive and read it back in place
	template <void (*SaveFunction)(const Type&, BinaryWriter&), void (*LoadFunction)(Type&, BinaryReader&)>
	Reflect& Codec()
	{
		const TypeCodec codec {
		    .save = [](const void* value, BinaryWriter& out) { SaveFunction(*static_cast<const Type*>(value), out); },
		    .load = [](void* value, BinaryReader& in) { LoadFunction(*static_cast<Type*>(value), in); },
		};
		// A new factory stands on the type itself, not on the last field that this one registered
		entt::meta_factory<Type> type {*_context};
		if (_component.has_value())
		{
			_component->codec = codec;
			type.template custom<ComponentInfo>(*_component);
		}
		else
		{
			type.template custom<ValueInfo>(ValueInfo {.codec = codec});
		}
		return *this;
	}

private:
	entt::meta_ctx* _context;
	entt::meta_factory<Type> _factory;
	/// The information of a component, which a codec adds to; none for a value type
	std::optional<ComponentInfo> _component;
};

/// Registers the game's components and their fields
void RegisterComponents(entt::meta_ctx& context);
/// The generated registration of every component and of the values their fields hold (see
/// tools/inspector/generate_component_fields.py)
void RegisterComponentFields(entt::meta_ctx& context);
/// The registration of values whose data is private, which the generator leaves to be written by hand
void RegisterHandWrittenFields(entt::meta_ctx& context);

/// A type's name without its namespaces or the compiler's "struct " or "class ": "Transform"
[[nodiscard]] std::string ShortTypeName(const entt::type_info& info);

/// The type of the components a storage holds. EnTT 3.16 names it info() and deprecates type(); 3.15 has only type()
template <typename Storage>
[[nodiscard]] const entt::type_info& StorageType(const Storage& storage)
{
	if constexpr (requires { storage.info(); })
	{
		return storage.info();
	}
	else
	{
		return storage.type();
	}
}

/// The dotted paths of a component's registered fields that hold an entity, through nested registered values and lists
/// of them: ["opponent"], ["moves.2.target"]
[[nodiscard]] std::vector<std::string> References(const entt::meta_ctx& context, const entt::type_info& info,
                                                  const void* component, entt::entity target);

/// Whether a type has fields registered
[[nodiscard]] bool IsReflected(const entt::meta_ctx& context, const entt::type_info& info);

/// The storage of a component by its short name: the one the registry has, else one made for a registered component;
/// none when there is no component of that name
[[nodiscard]] entt::sparse_set* FindStorage(entt::registry& registry, const entt::meta_ctx& context, std::string_view name);
[[nodiscard]] const entt::sparse_set* FindStorage(const entt::registry& registry, std::string_view name);

} // namespace openblack::ecs::reflection
