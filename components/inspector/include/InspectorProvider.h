/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "InspectorJson.h"
#include "InspectorProtocol.h"

namespace openblack::inspector
{

/// What a query's result is: one object, or a list of items that the query language filters, searches and pages
enum class ResultKind : uint8_t
{
	Object,
	List,
};

struct ParameterDescription
{
	std::string name;
	/// What kind of value it takes, for the reader: "integer", "number", "string", "boolean", "point" or "array"
	std::string type;
	std::string description;
	bool required {false};
};

/// A query a provider answers, as describe() lists it
struct QueryDescription
{
	/// Its name within the provider: "moon" is asked for as "sky.moon"
	std::string name;
	std::string description;
	std::vector<ParameterDescription> parameters;
	ResultKind kind {ResultKind::Object};
	/// The query searches around a point and needs "near" and "radius"
	bool needsNear {false};
	/// The query changes the game rather than only reading it: it is logged, and describe marks it
	bool writes {false};
};

/// What a query answers: a value, or why it couldn't
struct QueryResult
{
	Json value;
	std::string error;

	[[nodiscard]] bool Ok() const { return error.empty(); }
	[[nodiscard]] static QueryResult Value(Json value) { return {.value = std::move(value), .error = {}}; }
	[[nodiscard]] static QueryResult Error(std::string error) { return {.value = nullptr, .error = std::move(error)}; }
};

/// A query as a provider is asked it: its parameters, and how its result will be shaped. A list query may use the
/// options to leave out early what the shaping would drop anyway (the items far from a searched point, say), but it
/// needn't: the shaping is done for it afterwards either way.
struct QueryContext
{
	const Json& params;
	const QueryOptions& options;
};

/// Something whose state can be inspected, such as a game system, offering a few small named queries. Providers are
/// asked only at the point of the frame the inspector serves requests, never from another thread.
class ProviderInterface
{
public:
	virtual ~ProviderInterface() = default;

	/// The provider's name, the first part of its queries' names
	[[nodiscard]] virtual std::string_view Name() const = 0;
	[[nodiscard]] virtual std::vector<QueryDescription> Describe() const = 0;
	/// Answers a query by its name within the provider: an object, or for a list query an array of items. List items
	/// that have a place give it as "position": [x, y, z], for the searches around a point.
	[[nodiscard]] virtual QueryResult Run(std::string_view query, const QueryContext& context) = 0;
};

/// A provider made of functions, one for each of its queries, for a provider whose queries need no state of their own
class FunctionProvider final: public ProviderInterface
{
public:
	using Function = std::function<QueryResult(const QueryContext& context)>;

	explicit FunctionProvider(std::string name)
	    : _name(std::move(name))
	{
	}

	/// Adds a query answered by a function
	FunctionProvider& Add(QueryDescription description, Function function)
	{
		_queries.push_back({.description = std::move(description), .function = std::move(function)});
		return *this;
	}

	[[nodiscard]] std::string_view Name() const override { return _name; }
	[[nodiscard]] std::vector<QueryDescription> Describe() const override
	{
		std::vector<QueryDescription> descriptions;
		descriptions.reserve(_queries.size());
		for (const auto& query : _queries)
		{
			descriptions.push_back(query.description);
		}
		return descriptions;
	}
	[[nodiscard]] QueryResult Run(std::string_view query, const QueryContext& context) override
	{
		for (const auto& each : _queries)
		{
			if (each.description.name == query)
			{
				return each.function(context);
			}
		}
		return QueryResult::Error("no query " + _name + "." + std::string(query));
	}

private:
	struct Query
	{
		QueryDescription description;
		Function function;
	};

	std::string _name;
	std::vector<Query> _queries;
};

} // namespace openblack::inspector
