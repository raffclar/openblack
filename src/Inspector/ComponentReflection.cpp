/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComponentReflection.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <entt/meta/container.hpp>
#include <entt/meta/meta.hpp>
#include <entt/meta/resolve.hpp>
#include <glm/ext/vector_uint2_sized.hpp>

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

/// A value of one of the types JSON holds directly (or a list of them), written as Encode writes it; none for others
template <typename... Types>
std::optional<Json> PlainToJson(const entt::meta_any& any)
{
	std::optional<Json> json;
	(
	    [&any, &json] {
		    if (!json.has_value())
		    {
			    if (const auto* value = any.try_cast<const Types>(); value != nullptr)
			    {
				    json = reflection::Encode(*value);
			    }
		    }
	    }(),
	    ...);
	return json;
}

/// A map's key as an object's key, when it is a name or a number; none for anything else
std::optional<std::string> KeyOf(const Json& key)
{
	if (key.is_string())
	{
		return key.get<std::string>();
	}
	if (key.is_number())
	{
		return key.dump();
	}
	return std::nullopt;
}

Json AnyToJson(const entt::meta_any& any, int depth)
{
	if (!any)
	{
		return nullptr;
	}
	// Values held directly, as fields of these types are: inside lists and maps too
	if (auto plain = PlainToJson<bool, char, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float,
	                             double, std::string, entt::entity, glm::vec2, glm::vec3, glm::vec4, glm::ivec2, glm::ivec3,
	                             glm::uvec2, glm::u16vec2, glm::mat3, glm::mat4>(any);
	    plain.has_value())
	{
		return *std::move(plain);
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
		// A map keyed by names or numbers as an object, anything else (sets, other keys) as a list; long ones by their
		// first entries
		Json object = Json::object();
		Json list = Json::array();
		bool byName = true;
		size_t count = 0;
		for (auto [key, value] : associative)
		{
			if (count++ == reflection::k_MostElements)
			{
				break;
			}
			auto keyJson = AnyToJson(key, depth + 1);
			auto valueJson = value ? AnyToJson(value, depth + 1) : Json();
			const auto name = value ? KeyOf(keyJson) : std::nullopt;
			byName = byName && name.has_value();
			if (name.has_value())
			{
				object[*name] = valueJson;
			}
			list.push_back(value ? Json::array({std::move(keyJson), std::move(valueJson)}) : std::move(keyJson));
		}
		auto entries = byName && count > 0 ? std::move(object) : std::move(list);
		if (associative.size() <= reflection::k_MostElements)
		{
			return entries;
		}
		return {{"size", associative.size()}, {"first", std::move(entries)}};
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

double reflection::detail::Shortest(float value)
{
	if (!std::isfinite(value))
	{
		return static_cast<double>(value);
	}
	// Nine significant digits always read back as the same float; fewer often do
	std::array<char, 32> text {};
	for (int precision = 6; precision <= 9; ++precision)
	{
		std::snprintf(text.data(), text.size(), "%.*g", precision, static_cast<double>(value));
		if (std::strtof(text.data(), nullptr) == value)
		{
			return std::strtod(text.data(), nullptr);
		}
	}
	return static_cast<double>(value);
}

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

std::optional<Json> reflection::detail::ScalarFromText(const Json& value)
{
	if (!value.is_string())
	{
		return std::nullopt;
	}
	auto parsed = Parse(value.get<std::string>());
	if (!parsed.has_value() || !(parsed->is_number() || parsed->is_boolean()))
	{
		return std::nullopt;
	}
	return parsed;
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

namespace
{

/// The most elements of a list looked through for references
constexpr size_t k_MostElementsSearched = 4096;

void FindReferences(const entt::meta_any& any, entt::entity target, const std::string& path, int depth,
                    std::vector<std::string>& found)
{
	if (!any || depth > reflection::k_DeepestNesting)
	{
		return;
	}
	const auto type = any.type();
	if (type.data().begin() != type.data().end())
	{
		for (const auto& [id, data] : type.data())
		{
			const reflection::FieldInfo* info = data.custom();
			const auto name = info != nullptr ? info->name : std::to_string(id);
			const auto fieldPath = path.empty() ? name : path + "." + name;
			const auto value = data.get(any);
			if (info != nullptr && info->refersTo != nullptr)
			{
				if (info->refersTo(value, target))
				{
					found.push_back(fieldPath);
				}
				continue;
			}
			FindReferences(value, target, fieldPath, depth + 1, found);
		}
		return;
	}
	if (auto sequence = any.as_sequence_container(); sequence)
	{
		size_t index = 0;
		for (auto element : sequence)
		{
			if (index == k_MostElementsSearched)
			{
				break;
			}
			FindReferences(element, target, path + "." + std::to_string(index++), depth + 1, found);
		}
	}
}

} // namespace

std::vector<std::string> reflection::References(const entt::meta_ctx& context, const entt::type_info& info,
                                                const void* component, entt::entity target)
{
	std::vector<std::string> found;
	const auto type = entt::resolve(context, info);
	if (!type || component == nullptr)
	{
		return found;
	}
	FindReferences(type.from_void(component), target, {}, 0, found);
	return found;
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
