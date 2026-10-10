/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InspectorProtocol.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>

#include "InspectorQuery.h"

using namespace openblack::inspector;

namespace
{

struct OpName
{
	FilterOp op;
	std::string_view name;
};

constexpr std::array<OpName, 8> k_OpNames {{
    {FilterOp::Equal, "=="},
    {FilterOp::NotEqual, "!="},
    {FilterOp::Less, "<"},
    {FilterOp::LessEqual, "<="},
    {FilterOp::Greater, ">"},
    {FilterOp::GreaterEqual, ">="},
    {FilterOp::Contains, "contains"},
    {FilterOp::Exists, "exists"},
}};

/// Every member a request may have beside its query's parameters, which go in "params"
constexpr std::array<std::string_view, 12> k_RequestMembers {
    "id", "query", "params", "fields", "where", "near", "radius", "limit", "cursor", "count", "summary", "max_bytes",
};

/// The members of a filter
constexpr std::array<std::string_view, 3> k_FilterMembers {"field", "op", "value"};

/// The names of an object's members that aren't among the known ones, joined with commas; empty when there are none
template <size_t N>
std::string UnknownMembers(const Json& object, const std::array<std::string_view, N>& known)
{
	std::string unknown;
	for (const auto& [key, value] : object.items())
	{
		if (std::ranges::find(known, std::string_view(key)) == known.end())
		{
			unknown += (unknown.empty() ? "" : ", ") + key;
		}
	}
	return unknown;
}

template <size_t N>
std::string Joined(const std::array<std::string_view, N>& names)
{
	std::string joined;
	for (const auto name : names)
	{
		joined += (joined.empty() ? "" : ", ") + std::string(name);
	}
	return joined;
}

/// A count read from a request: a whole number of zero or more
std::optional<size_t> ReadCount(const Json& value)
{
	if (value.is_number_unsigned() || value.is_number_integer())
	{
		const auto number = value.get<int64_t>();
		return number < 0 ? std::nullopt : std::optional(static_cast<size_t>(number));
	}
	if (value.is_number_float())
	{
		const auto number = value.get<double>();
		return number < 0.0 || !std::isfinite(number) ? std::nullopt : std::optional(static_cast<size_t>(number));
	}
	return std::nullopt;
}

std::optional<std::string> ReadOptions(const Json& object, QueryOptions& options)
{
	if (const auto it = object.find("fields"); it != object.end())
	{
		if (!it->is_array())
		{
			return "fields must be an array of dotted paths";
		}
		for (const auto& field : *it)
		{
			if (!field.is_string())
			{
				return "fields must be an array of dotted paths";
			}
			options.fields.push_back(field.get<std::string>());
		}
	}
	if (const auto it = object.find("where"); it != object.end())
	{
		if (!it->is_array())
		{
			return "where must be an array of {field, op, value}";
		}
		for (const auto& each : *it)
		{
			if (!each.is_object())
			{
				return "where must be an array of {field, op, value}";
			}
			if (const auto unknown = UnknownMembers(each, k_FilterMembers); !unknown.empty())
			{
				return "a filter has only field, op and value, not " + unknown;
			}
			const auto field = StringMember(each, "field");
			if (!field.has_value())
			{
				return "each filter needs a field";
			}
			const auto opName = StringMember(each, "op").value_or("==");
			const auto op = ParseFilterOp(opName);
			if (!op.has_value())
			{
				return "unknown filter op " + opName + " (==, !=, <, <=, >, >=, contains, exists)";
			}
			const auto value = each.find("value");
			if (value == each.end() && *op != FilterOp::Exists)
			{
				return "the filter on " + *field + " needs a value";
			}
			options.where.push_back({.field = *field, .op = *op, .value = value == each.end() ? Json() : *value});
		}
	}
	if (const auto it = object.find("near"); it != object.end())
	{
		bool planar = false;
		const auto point = ReadPoint(*it, &planar);
		if (!point.has_value())
		{
			return "near must be [x, y, z] or [x, z]";
		}
		const auto radius = NumberMember(object, "radius");
		if (!radius.has_value() || *radius < 0.0)
		{
			return "near needs a radius of zero or more";
		}
		options.near = Near {.point = *point, .planar = planar, .radius = *radius};
	}
	else if (object.contains("radius"))
	{
		return "radius is how far from near to search: give near too";
	}
	if (const auto it = object.find("limit"); it != object.end())
	{
		const auto limit = ReadCount(*it);
		if (!limit.has_value())
		{
			return "limit must be a whole number";
		}
		options.limit = std::min(*limit, k_MostItems);
	}
	if (const auto it = object.find("cursor"); it != object.end() && !it->is_null())
	{
		const auto cursor = ReadCount(*it);
		if (!cursor.has_value())
		{
			return "cursor must be the next_cursor of the last page";
		}
		options.cursor = *cursor;
	}
	if (const auto it = object.find("count"); it != object.end())
	{
		if (!it->is_boolean())
		{
			return "count must be true or false";
		}
		options.countOnly = it->get<bool>();
	}
	if (const auto it = object.find("summary"); it != object.end())
	{
		if (!it->is_string())
		{
			return "summary must be the name of a numeric field";
		}
		options.summary = it->get<std::string>();
	}
	if (const auto it = object.find("max_bytes"); it != object.end())
	{
		const auto bytes = ReadCount(*it);
		if (!bytes.has_value())
		{
			return "max_bytes must be a whole number";
		}
		options.maxBytes = std::clamp<size_t>(*bytes, 256, k_MostBytes);
	}
	return std::nullopt;
}

} // namespace

std::optional<FilterOp> openblack::inspector::ParseFilterOp(std::string_view op)
{
	if (op == "=")
	{
		return FilterOp::Equal;
	}
	const auto found = std::ranges::find(k_OpNames, op, &OpName::name);
	return found == k_OpNames.end() ? std::nullopt : std::optional(found->op);
}

std::string_view openblack::inspector::Name(FilterOp op)
{
	const auto found = std::ranges::find(k_OpNames, op, &OpName::op);
	return found == k_OpNames.end() ? std::string_view("?") : found->name;
}

std::variant<Request, std::string> openblack::inspector::DecodeRequest(std::string_view line)
{
	const auto parsed = Parse(line);
	if (!parsed.has_value())
	{
		return std::string("the request isn't JSON");
	}
	const auto& object = *parsed;
	if (!object.is_object())
	{
		return std::string("the request must be a JSON object");
	}
	// A member the request doesn't know is refused rather than ignored: most often a query's parameter put beside
	// "params" instead of inside it, which would otherwise run the query without it
	if (const auto unknown = UnknownMembers(object, k_RequestMembers); !unknown.empty())
	{
		return "the request has unknown members " + unknown + " (a query's parameters go in \"params\"; a request has " +
		       Joined(k_RequestMembers) + ")";
	}
	Request request;
	if (const auto it = object.find("id"); it != object.end())
	{
		request.id = *it;
	}
	const auto query = StringMember(object, "query");
	if (!query.has_value() || query->empty())
	{
		return std::string("the request needs a query, such as \"describe\"");
	}
	request.query = *query;
	if (const auto it = object.find("params"); it != object.end() && !it->is_null())
	{
		if (!it->is_object())
		{
			return std::string("params must be an object");
		}
		request.params = *it;
	}
	if (auto error = ReadOptions(object, request.options); error.has_value())
	{
		return *std::move(error);
	}
	return request;
}

std::string openblack::inspector::EncodeRequest(const Request& request)
{
	Json object = {{"query", request.query}};
	if (!request.id.is_null())
	{
		object["id"] = request.id;
	}
	if (!request.params.empty())
	{
		object["params"] = request.params;
	}
	const auto& options = request.options;
	if (!options.fields.empty())
	{
		object["fields"] = options.fields;
	}
	if (!options.where.empty())
	{
		auto& where = object["where"] = Json::array();
		for (const auto& filter : options.where)
		{
			Json each = {{"field", filter.field}, {"op", Name(filter.op)}};
			if (filter.op != FilterOp::Exists)
			{
				each["value"] = filter.value;
			}
			where.push_back(std::move(each));
		}
	}
	if (options.near.has_value())
	{
		const auto& point = options.near->point;
		object["near"] = options.near->planar ? Json {point[0], point[2]} : Json {point[0], point[1], point[2]};
		object["radius"] = options.near->radius;
	}
	if (options.limit != k_DefaultLimit)
	{
		object["limit"] = options.limit;
	}
	if (options.cursor != 0)
	{
		object["cursor"] = options.cursor;
	}
	if (options.countOnly)
	{
		object["count"] = true;
	}
	if (options.summary.has_value())
	{
		object["summary"] = *options.summary;
	}
	if (options.maxBytes != k_DefaultMaxBytes)
	{
		object["max_bytes"] = options.maxBytes;
	}
	return Dump(object);
}

std::string openblack::inspector::EncodeResult(const Json& id, const Json& result)
{
	return Dump({{"id", id}, {"ok", true}, {"result", result}});
}

std::string openblack::inspector::EncodeError(const Json& id, std::string_view error)
{
	return Dump({{"id", id}, {"ok", false}, {"error", error}});
}

namespace
{

openblack::inspector::Json WithExtra(openblack::inspector::Json answer, const openblack::inspector::Json& extra)
{
	if (extra.is_object())
	{
		for (const auto& [key, value] : extra.items())
		{
			if (!answer.contains(key))
			{
				answer[key] = value;
			}
		}
	}
	return answer;
}

} // namespace

std::string openblack::inspector::EncodeResult(const Json& id, const Json& result, const Json& extra)
{
	return Dump(WithExtra({{"id", id}, {"ok", true}, {"result", result}}, extra));
}

std::string openblack::inspector::EncodeError(const Json& id, std::string_view error, const Json& extra)
{
	return Dump(WithExtra({{"id", id}, {"ok", false}, {"error", error}}, extra));
}
