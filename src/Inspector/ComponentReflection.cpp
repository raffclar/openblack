/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComponentReflection.h"

#include <cstdint>

#include <optional>
#include <string>

#include <entt/entity/entity.hpp>
#include <entt/meta/container.hpp>
#include <entt/meta/meta.hpp>
#include <entt/meta/resolve.hpp>

using namespace openblack::inspector;

namespace
{

Json AnyToJson(const entt::meta_any& any, int depth);

/// A registered field of a type by its name, and what is kept with it
struct FoundField
{
	entt::meta_data data;
	const reflection::FieldInfo* info;
};

std::optional<FoundField> FindField(const entt::meta_type& type, std::string_view name)
{
	for (const auto& [id, data] : type.data())
	{
		const reflection::FieldInfo* info = data.custom();
		if (info != nullptr && info->name == name)
		{
			return FoundField {.data = data, .info = info};
		}
	}
	return std::nullopt;
}

Json FieldValueToJson(const entt::meta_data& data, const reflection::FieldInfo* info, const entt::meta_any& instance, int depth)
{
	const auto value = data.get(instance);
	if (info != nullptr && info->encode != nullptr)
	{
		if (auto encoded = info->encode(value); encoded.has_value())
		{
			return *std::move(encoded);
		}
	}
	return AnyToJson(value, depth + 1);
}

Json FieldsToJson(const entt::meta_any& any, int depth)
{
	Json object = Json::object();
	for (const auto& [id, data] : any.type().data())
	{
		const reflection::FieldInfo* info = data.custom();
		const auto key = info != nullptr ? info->name : std::to_string(id);
		object[key] = FieldValueToJson(data, info, any, depth);
	}
	return object;
}

Json AnyToJson(const entt::meta_any& any, int depth)
{
	if (!any)
	{
		return nullptr;
	}
	const auto type = any.type();
	if (type.is_enum())
	{
		if (const auto number = any.allow_cast<int64_t>(); number)
		{
			return number.cast<int64_t>();
		}
	}
	if (depth >= reflection::k_DeepestNesting)
	{
		return "...";
	}
	if (type.data().begin() != type.data().end())
	{
		return FieldsToJson(any, depth);
	}
	if (auto sequence = any.as_sequence_container(); sequence)
	{
		Json array = Json::array();
		size_t count = 0;
		for (auto element : sequence)
		{
			if (count++ == reflection::k_MostElements)
			{
				break;
			}
			array.push_back(AnyToJson(element, depth + 1));
		}
		if (sequence.size() <= reflection::k_MostElements)
		{
			return array;
		}
		return {{"size", sequence.size()}, {"first", std::move(array)}};
	}
	if (auto associative = any.as_associative_container(); associative)
	{
		return {{"size", associative.size()}};
	}
	// A value of a type that isn't registered: its name, to say what is there
	return "<" + reflection::ShortTypeName(type.info()) + ">";
}

/// Sets a field of a value by a dotted path; the value is a reference to where it is kept
std::string SetPath(const entt::meta_ctx& context, entt::meta_any& instance, std::string_view path, const Json& json)
{
	const auto dot = path.find('.');
	const auto name = path.substr(0, dot);
	const auto field = FindField(instance.type(), name);
	if (!field.has_value())
	{
		return "no field " + std::string(name) + " in " + reflection::ShortTypeName(instance.type().info());
	}
	if (dot != std::string_view::npos)
	{
		// Further into a nested value, reached by reference
		auto nested = field->data.get(instance);
		if (!nested || nested.type().data().begin() == nested.type().data().end())
		{
			return std::string(name) + " has no fields to set by name";
		}
		return SetPath(context, nested, path.substr(dot + 1), json);
	}
	std::string error;
	if (field->info->decode != nullptr)
	{
		if (auto value = field->info->decode(context, json, error); value.has_value())
		{
			return field->data.set(instance, *std::move(value)) ? std::string {}
			                                                    : std::string(name) + " can't be set: it is read only";
		}
	}
	// A nested value set from an object of its fields, each in turn
	auto nested = field->data.get(instance);
	if (json.is_object() && nested && nested.type().data().begin() != nested.type().data().end())
	{
		for (const auto& [key, member] : json.items())
		{
			if (auto problem = SetPath(context, nested, key, member); !problem.empty())
			{
				return problem;
			}
		}
		return {};
	}
	return std::string(name) + " " + error;
}

} // namespace

std::string reflection::detail::Describe(const Json& value)
{
	switch (value.type())
	{
	case Json::value_t::null:
		return "null";
	case Json::value_t::boolean:
		return "a boolean";
	case Json::value_t::string:
		return "a string";
	case Json::value_t::array:
		return "an array of " + std::to_string(value.size());
	case Json::value_t::object:
		return "an object";
	case Json::value_t::number_float:
		return "a number with a fraction";
	case Json::value_t::number_integer:
	case Json::value_t::number_unsigned:
		return "a whole number";
	default:
		return "something else";
	}
}

std::string reflection::ShortTypeName(const entt::type_info& info)
{
	std::string_view name = info.name();
	for (const std::string_view prefix : {"struct ", "class ", "enum "})
	{
		if (name.starts_with(prefix))
		{
			name.remove_prefix(prefix.size());
		}
	}
	// The namespaces go, but not those of a template's arguments
	const auto open = name.find('<');
	const auto head = name.substr(0, open);
	const auto colons = head.rfind("::");
	if (colons != std::string_view::npos)
	{
		name.remove_prefix(colons + 2);
	}
	return std::string(name);
}

bool reflection::IsReflected(const entt::meta_ctx& context, const entt::type_info& info)
{
	const auto type = entt::resolve(context, info);
	return type && type.data().begin() != type.data().end();
}

Json reflection::ComponentToJson(const entt::meta_ctx& context, const entt::type_info& info, const void* component)
{
	const auto type = entt::resolve(context, info);
	if (!type || type.data().begin() == type.data().end() || component == nullptr)
	{
		return nullptr;
	}
	return FieldsToJson(type.from_void(component), 0);
}

std::optional<Json> reflection::ReadField(const entt::meta_ctx& context, const entt::type_info& info, const void* component,
                                          std::string_view path)
{
	const auto type = entt::resolve(context, info);
	if (!type || component == nullptr)
	{
		return std::nullopt;
	}
	auto instance = type.from_void(component);
	while (true)
	{
		const auto dot = path.find('.');
		const auto field = FindField(instance.type(), path.substr(0, dot));
		if (!field.has_value())
		{
			return std::nullopt;
		}
		if (dot == std::string_view::npos)
		{
			return FieldValueToJson(field->data, field->info, instance, 0);
		}
		instance = field->data.get(instance);
		path.remove_prefix(dot + 1);
	}
}

std::string reflection::SetField(const entt::meta_ctx& context, const entt::type_info& info, void* component,
                                 std::string_view path, const Json& value)
{
	const auto type = entt::resolve(context, info);
	if (!type || type.data().begin() == type.data().end())
	{
		return ShortTypeName(info) + " has no fields registered";
	}
	if (component == nullptr)
	{
		return ShortTypeName(info) + " holds no data";
	}
	auto instance = type.from_void(component);
	return SetPath(context, instance, path, value);
}

entt::sparse_set* reflection::FindStorage(entt::registry& registry, const entt::meta_ctx& context, std::string_view name)
{
	for (auto&& [id, storage] : registry.storage())
	{
		if (ShortTypeName(storage.type()) == name)
		{
			return &storage;
		}
	}
	for (auto&& [id, type] : entt::resolve(context))
	{
		const ComponentInfo* info = type.custom();
		if (info != nullptr && info->storage != nullptr && ShortTypeName(type.info()) == name)
		{
			return &info->storage(registry);
		}
	}
	return nullptr;
}

const entt::sparse_set* reflection::FindStorage(const entt::registry& registry, std::string_view name)
{
	for (const auto& [id, storage] : registry.storage())
	{
		if (ShortTypeName(storage.type()) == name)
		{
			return &storage;
		}
	}
	return nullptr;
}

void reflection::RegisterComponents(entt::meta_ctx& context)
{
	RegisterComponentFields(context);
}
