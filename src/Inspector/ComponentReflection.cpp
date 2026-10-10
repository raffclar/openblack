/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComponentReflection.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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

const reflection::ValueInfo* ValueInfoOf(const entt::meta_any& any)
{
	if (!any)
	{
		return nullptr;
	}
	return any.type().custom();
}

bool HasFields(const entt::meta_any& any)
{
	return any && any.type().data().begin() != any.type().data().end();
}

/// The names indexing the lists of a field from a level of lists within lists on
using IndexNames = std::span<const std::span<const std::string_view>>;

/// A list indexed by an enumeration as an object by its names, all of them (an enumeration is never long); its elements
/// that are lists indexed by another as objects too
Json NamedListToJson(const entt::meta_any& any, IndexNames names, int depth)
{
	auto list = any;
	auto sequence = list.as_sequence_container();
	if (!sequence || names.empty())
	{
		return AnyToJson(any, depth);
	}
	// A level indexed by number is an array of what it holds, which may be lists by names in turn
	const auto level = names.front();
	Json written = level.empty() ? Json::array() : Json::object();
	size_t index = 0;
	for (auto element : sequence)
	{
		auto value = names.size() > 1 ? NamedListToJson(element, names.subspan(1), depth + 1) : AnyToJson(element, depth + 1);
		if (level.empty())
		{
			written.push_back(std::move(value));
		}
		else
		{
			written[index < level.size() ? std::string(level[index]) : std::to_string(index)] = std::move(value);
		}
		++index;
	}
	return written;
}

/// A value with the index names of the lists it is, from its field: written by them, else as it is
Json ValueToJson(const entt::meta_any& value, IndexNames names, int depth)
{
	if (!names.empty() && std::ranges::any_of(names, [](const auto& level) { return !level.empty(); }))
	{
		return NamedListToJson(value, names, depth);
	}
	return AnyToJson(value, depth);
}

Json FieldValueToJson(const entt::meta_data& data, const reflection::FieldInfo* info, const entt::meta_any& instance, int depth)
{
	const auto value = data.get(instance);
	if (info != nullptr && !info->indexNames.empty())
	{
		return ValueToJson(value, info->indexNames, depth + 1);
	}
	if (info != nullptr && info->encode != nullptr)
	{
		if (auto encoded = info->encode(value); encoded.has_value())
		{
			return *std::move(encoded);
		}
	}
	return AnyToJson(value, depth + 1);
}

/// An element's place in a list by its number, or by its name in the enumeration indexing the list
std::optional<size_t> IndexOf(std::string_view segment, std::span<const std::string_view> names)
{
	size_t index = 0;
	const auto* end = segment.data() + segment.size();
	if (const auto [last, error] = std::from_chars(segment.data(), end, index); error == std::errc {} && last == end)
	{
		return index;
	}
	const auto found = std::ranges::find(names, segment);
	if (found != names.end())
	{
		return static_cast<size_t>(std::distance(names.begin(), found));
	}
	return std::nullopt;
}

/// Why a segment of a path doesn't name an element of a list
std::string NotAnElement(std::string_view list, std::string_view segment, size_t size, std::span<const std::string_view> names)
{
	std::string why = std::string(list) + " has no element " + std::string(segment) + ": it has " + std::to_string(size) +
	                  ", by number from 0";
	if (!names.empty())
	{
		why += " or by name (";
		for (size_t i = 0; i < names.size(); ++i)
		{
			why += (i > 0 ? ", " : "") + std::string(names[i]);
		}
		why += ")";
	}
	return why;
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
	// An option as what it holds, or null
	if (const auto* value = ValueInfoOf(any); value != nullptr && value->unwrap != nullptr)
	{
		auto option = any.as_ref();
		const auto held = value->unwrap(option);
		return held ? AnyToJson(held, depth) : Json(nullptr);
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

/// Where a dotted path has got to: the value there (a reference to where it is kept, or a copy of one given by a
/// function), the path so far, and the names indexing the lists of the field it is in from its level on
struct Place
{
	entt::meta_any value;
	std::string path;
	IndexNames names;
};

/// Sets a value from JSON as a whole, or field by field from an object, or element by element from an array
std::string SetWhole(const entt::meta_ctx& context, Place& place, const reflection::FieldInfo* field, const Json& json);

/// Sets what is at a dotted path from a place
std::string SetPath(const entt::meta_ctx& context, Place& place, const reflection::FieldInfo* field, std::string_view path,
                    const Json& json)
{
	const auto* value = ValueInfoOf(place.value);
	const bool option = value != nullptr && value->engage != nullptr;
	if (option && path.empty())
	{
		// An option set whole (null empties it) when the value reads as one, else what it holds is set from it
		const auto decode = field != nullptr ? field->decode : value->decode;
		std::string error;
		if (auto decoded = decode != nullptr ? decode(context, json, error) : std::nullopt; decoded.has_value())
		{
			return place.value.assign(*std::move(decoded)) ? std::string {} : place.path + " can't be set: it is read only";
		}
		if (json.is_null())
		{
			return place.path + " " + error;
		}
	}
	if (option)
	{
		// Through an option to what it holds, made first if it holds nothing
		Place held {.value = value->engage(place.value), .path = place.path, .names = place.names};
		if (!held.value)
		{
			return place.path + " holds nothing, and nothing can be made for it";
		}
		return SetPath(context, held, nullptr, path, json);
	}
	if (path.empty())
	{
		return SetWhole(context, place, field, json);
	}
	const auto dot = path.find('.');
	const auto segment = path.substr(0, dot);
	const auto rest = dot == std::string_view::npos ? std::string_view {} : path.substr(dot + 1);
	const auto here = place.path.empty() ? std::string(segment) : place.path + "." + std::string(segment);
	if (HasFields(place.value))
	{
		const auto found = FindField(place.value.type(), segment);
		if (!found.has_value())
		{
			return "no field " + std::string(segment) + " in " +
			       (place.path.empty() ? reflection::ShortTypeName(place.value.type().info()) : place.path);
		}
		Place next {.value = found->data.get(place.value), .path = here, .names = found->info->indexNames};
		if (auto problem = SetPath(context, next, found->info, rest, json); !problem.empty())
		{
			return problem;
		}
		// A value given by a function is a copy: it is set back whole. One kept in place was set where it is.
		if (next.value.base().owner() && !found->data.set(place.value, std::move(next.value)))
		{
			return here + " can't be set: it is read only";
		}
		return {};
	}
	if (auto sequence = place.value.as_sequence_container(); sequence)
	{
		const auto names = place.names.empty() ? std::span<const std::string_view> {} : place.names.front();
		const auto index = IndexOf(segment, names);
		if (!index.has_value() || *index >= sequence.size())
		{
			return NotAnElement(place.path, segment, sequence.size(), names);
		}
		Place next {
		    .value = sequence[*index], .path = here, .names = place.names.empty() ? IndexNames {} : place.names.subspan(1)};
		return SetPath(context, next, nullptr, rest, json);
	}
	return place.path + " has no fields or elements to set by name";
}

std::string SetWhole(const entt::meta_ctx& context, Place& place, const reflection::FieldInfo* field, const Json& json)
{
	std::string error;
	// The field's own reading, or its type's, for an element of a list
	const auto* value = ValueInfoOf(place.value);
	const auto decode = field != nullptr ? field->decode : (value != nullptr ? value->decode : nullptr);
	if (decode != nullptr)
	{
		if (auto decoded = decode(context, json, error); decoded.has_value())
		{
			return place.value.assign(*std::move(decoded)) ? std::string {} : place.path + " can't be set: it is read only";
		}
	}
	// A nested value set from an object of its fields, each in turn
	if (json.is_object() && HasFields(place.value))
	{
		for (const auto& [key, member] : json.items())
		{
			if (auto problem = SetPath(context, place, nullptr, key, member); !problem.empty())
			{
				return problem;
			}
		}
		return {};
	}
	// A list set from an array of its elements, each in turn: one that can grow or shrink takes the array's size
	if (auto sequence = place.value.as_sequence_container(); json.is_array() && sequence)
	{
		if (json.size() != sequence.size() && !sequence.resize(json.size()))
		{
			return place.path + " needs an array of " + std::to_string(sequence.size()) + ", not " +
			       reflection::detail::Describe(json);
		}
		for (size_t i = 0; i < json.size(); ++i)
		{
			Place element {.value = sequence[i],
			               .path = place.path + "." + std::to_string(i),
			               .names = place.names.empty() ? IndexNames {} : place.names.subspan(1)};
			if (auto problem = SetPath(context, element, nullptr, {}, json[i]); !problem.empty())
			{
				return problem;
			}
		}
		return {};
	}
	if (error.empty())
	{
		error = "can't be set from " + reflection::detail::Describe(json);
	}
	return place.path + " " + error;
}

/// A value reached inside another, copied out when the other is a copy that is about to go (a value given by a function)
entt::meta_any Detached(entt::meta_any inside, const entt::meta_any& from)
{
	if (from.base().owner() && !inside.base().owner())
	{
		const entt::meta_any& reference = inside;
		return entt::meta_any {reference};
	}
	return inside;
}

/// What is at a dotted path from a value, read only; none, with why not, when there is nothing there
std::optional<Place> Reach(Place place, std::string_view path, std::string& why)
{
	while (true)
	{
		if (const auto* value = ValueInfoOf(place.value); value != nullptr && value->unwrap != nullptr)
		{
			if (path.empty())
			{
				return place;
			}
			auto held = value->unwrap(place.value);
			if (!held)
			{
				why = place.path + " holds nothing";
				return std::nullopt;
			}
			place.value = Detached(std::move(held), place.value);
			continue;
		}
		if (path.empty())
		{
			return place;
		}
		const auto dot = path.find('.');
		const auto segment = path.substr(0, dot);
		path = dot == std::string_view::npos ? std::string_view {} : path.substr(dot + 1);
		const auto here = place.path.empty() ? std::string(segment) : place.path + "." + std::string(segment);
		if (HasFields(place.value))
		{
			const auto found = FindField(place.value.type(), segment);
			if (!found.has_value())
			{
				why = "no field " + here;
				return std::nullopt;
			}
			auto value = Detached(found->data.get(place.value), place.value);
			place = {.value = std::move(value), .path = here, .names = found->info->indexNames};
			continue;
		}
		auto sequence = place.value.as_sequence_container();
		if (!sequence)
		{
			why = place.path + " has no fields or elements";
			return std::nullopt;
		}
		const auto names = place.names.empty() ? std::span<const std::string_view> {} : place.names.front();
		const auto index = IndexOf(segment, names);
		if (!index.has_value() || *index >= sequence.size())
		{
			why = NotAnElement(place.path, segment, sequence.size(), names);
			return std::nullopt;
		}
		auto element = Detached(sequence[*index], place.value);
		place = {
		    .value = std::move(element), .path = here, .names = place.names.empty() ? IndexNames {} : place.names.subspan(1)};
	}
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

std::string reflection::detail::Utf8(std::u16string_view text)
{
	std::string utf8;
	for (size_t i = 0; i < text.size(); ++i)
	{
		uint32_t code = text[i];
		// A pair of surrogates is one character beyond the first 65536
		if (code >= 0xD800 && code < 0xDC00 && i + 1 < text.size() && text[i + 1] >= 0xDC00 && text[i + 1] < 0xE000)
		{
			code = 0x10000 + ((code - 0xD800) << 10) + (text[i + 1] - 0xDC00);
			++i;
		}
		if (code < 0x80)
		{
			utf8 += static_cast<char>(code);
		}
		else if (code < 0x800)
		{
			utf8 += static_cast<char>(0xC0 | (code >> 6));
			utf8 += static_cast<char>(0x80 | (code & 0x3F));
		}
		else if (code < 0x10000)
		{
			utf8 += static_cast<char>(0xE0 | (code >> 12));
			utf8 += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
			utf8 += static_cast<char>(0x80 | (code & 0x3F));
		}
		else
		{
			utf8 += static_cast<char>(0xF0 | (code >> 18));
			utf8 += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
			utf8 += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
			utf8 += static_cast<char>(0x80 | (code & 0x3F));
		}
	}
	return utf8;
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
	std::string why;
	return ReadField(context, info, component, path, why);
}

std::optional<Json> reflection::ReadField(const entt::meta_ctx& context, const entt::type_info& info, const void* component,
                                          std::string_view path, std::string& why)
{
	const auto type = entt::resolve(context, info);
	if (!type || component == nullptr)
	{
		why = ShortTypeName(info) + " has no fields registered";
		return std::nullopt;
	}
	// The field's own encoding when the path ends at a field, as a whole component writes it
	const auto dot = path.rfind('.');
	if (dot == std::string_view::npos)
	{
		const auto instance = type.from_void(component);
		const auto field = FindField(instance.type(), path);
		if (!field.has_value())
		{
			why = "no field " + std::string(path);
			return std::nullopt;
		}
		return FieldValueToJson(field->data, field->info, instance, 0);
	}
	const auto found = Reach({.value = type.from_void(component)}, path, why);
	if (!found.has_value())
	{
		return std::nullopt;
	}
	return ValueToJson(found->value, found->names, 1);
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
	if (path.empty())
	{
		return "no field given";
	}
	Place place {.value = type.from_void(component)};
	return SetPath(context, place, nullptr, path, value);
}

entt::sparse_set* reflection::FindStorage(entt::registry& registry, const entt::meta_ctx& context, std::string_view name)
{
	for (auto&& [id, storage] : registry.storage())
	{
		if (ShortTypeName(StorageType(storage)) == name)
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
		if (ShortTypeName(StorageType(storage)) == name)
		{
			return &storage;
		}
	}
	return nullptr;
}

void reflection::RegisterComponents(entt::meta_ctx& context)
{
	RegisterComponentFields(context);
	RegisterHandWrittenFields(context);
}
