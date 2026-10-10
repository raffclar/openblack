/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Inspector.h"

#include <algorithm>
#include <utility>
#include <variant>

#include "InspectorQuery.h"

using namespace openblack::inspector;

namespace
{

/// A query's name split into its provider's and its own: "sky.moon" into "sky" and "moon"
std::pair<std::string_view, std::string_view> Split(std::string_view query)
{
	const auto dot = query.find('.');
	if (dot == std::string_view::npos)
	{
		return {query, {}};
	}
	return {query.substr(0, dot), query.substr(dot + 1)};
}

} // namespace

Json openblack::inspector::ToJson(const QueryDescription& description, std::string_view provider)
{
	Json parameters = Json::array();
	for (const auto& parameter : description.parameters)
	{
		Json each = {{"name", parameter.name}, {"type", parameter.type}, {"description", parameter.description}};
		if (parameter.required)
		{
			each["required"] = true;
		}
		parameters.push_back(std::move(each));
	}
	Json result = {
	    {"query", std::string(provider) + "." + description.name},
	    {"description", description.description},
	    {"kind", description.kind == ResultKind::List ? "list" : "object"},
	    {"parameters", std::move(parameters)},
	};
	if (description.needsNear)
	{
		result["needs_near"] = true;
	}
	if (description.writes)
	{
		result["writes"] = true;
	}
	return result;
}

void Inspector::Add(std::unique_ptr<ProviderInterface> provider)
{
	if (provider == nullptr)
	{
		return;
	}
	const auto name = provider->Name();
	const auto existing = std::ranges::find_if(_providers, [name](const auto& each) { return each->Name() == name; });
	if (existing != _providers.end())
	{
		*existing = std::move(provider);
		return;
	}
	_providers.push_back(std::move(provider));
}

ProviderInterface* Inspector::Find(std::string_view name) const
{
	const auto found = std::ranges::find_if(_providers, [name](const auto& each) { return each->Name() == name; });
	return found == _providers.end() ? nullptr : found->get();
}

QueryResult Inspector::Describe(const Json& params) const
{
	if (const auto query = StringMember(params, "query"); query.has_value())
	{
		const auto [providerName, queryName] = Split(*query);
		const auto* provider = Find(providerName);
		if (provider != nullptr)
		{
			for (const auto& description : provider->Describe())
			{
				if (description.name == queryName)
				{
					return QueryResult::Value(ToJson(description, providerName));
				}
			}
		}
		return QueryResult::Error("no query " + *query);
	}
	if (const auto providerName = StringMember(params, "provider"); providerName.has_value())
	{
		const auto* provider = Find(*providerName);
		if (provider == nullptr)
		{
			return QueryResult::Error("no provider " + *providerName);
		}
		Json queries = Json::array();
		for (const auto& description : provider->Describe())
		{
			queries.push_back(ToJson(description, *providerName));
		}
		return QueryResult::Value({{"provider", *providerName}, {"queries", std::move(queries)}});
	}
	// Every provider, by its queries' names and what each is for in a line
	Json providers = Json::object();
	for (const auto& provider : _providers)
	{
		Json queries = Json::object();
		for (const auto& description : provider->Describe())
		{
			queries[description.name] = description.writes ? "(writes) " + description.description : description.description;
		}
		providers[std::string(provider->Name())] = std::move(queries);
	}
	return QueryResult::Value({
	    {"providers", std::move(providers)},
	    {"hint", "describe {\"provider\": name} or {\"query\": \"provider.query\"} for parameters; \"writes\" lists the "
	             "last changes made"},
	});
}

QueryResult Inspector::Answer(const Request& request) const
{
	const auto error = [](std::string text) { return QueryResult::Error(std::move(text)); };
	if (request.query == "ping")
	{
		Json answer = _identity.is_object() ? _identity : Json::object();
		answer["pong"] = true;
		return QueryResult::Value(std::move(answer));
	}
	if (request.query == "writes")
	{
		return Writes(request);
	}
	if (request.query == "describe")
	{
		auto described = Describe(request.params);
		if (!described.Ok())
		{
			return error(std::move(described.error));
		}
		return QueryResult::Value(ShapeObject(described.value, request.options));
	}

	const auto [providerName, queryName] = Split(request.query);
	auto* provider = Find(providerName);
	if (provider == nullptr)
	{
		return error("no provider " + std::string(providerName) + "; ask \"describe\" for them");
	}
	const auto descriptions = provider->Describe();
	const auto description =
	    std::ranges::find_if(descriptions, [queryName](const auto& each) { return each.name == queryName; });
	if (description == descriptions.end())
	{
		return error("no query " + request.query + "; ask \"describe\" with {\"provider\": \"" + std::string(providerName) +
		             "\"}");
	}
	for (const auto& parameter : description->parameters)
	{
		if (parameter.required && !request.params.contains(parameter.name))
		{
			return error(request.query + " needs the parameter " + parameter.name + " (" + parameter.type + ")");
		}
	}
	if (description->needsNear && !request.options.near.has_value())
	{
		return error(request.query + " searches around a point: give near and radius");
	}

	auto answered = provider->Run(queryName, QueryContext {.params = request.params, .options = request.options});
	if (description->writes)
	{
		Remember(request, answered);
	}
	if (!answered.Ok())
	{
		return error(std::move(answered.error));
	}
	if (description->kind == ResultKind::List)
	{
		return QueryResult::Value(ShapeList(std::move(answered.value), request.options));
	}
	return QueryResult::Value(ShapeObject(answered.value, request.options));
}

QueryResult Inspector::Writes(const Request& request) const
{
	Json items = Json::array();
	for (const auto& write : _writes)
	{
		items.push_back(write);
	}
	return QueryResult::Value(ShapeList(std::move(items), request.options));
}

void Inspector::Remember(const Request& request, const QueryResult& answer) const
{
	if (_writeLog)
	{
		_writeLog(request, answer);
	}
	Json write = {{"query", request.query}, {"params", request.params}, {"ok", answer.Ok()}};
	if (!answer.Ok())
	{
		write["error"] = answer.error;
	}
	_writes.push_front(std::move(write));
	if (_writes.size() > k_RememberedWrites)
	{
		_writes.pop_back();
	}
}

std::string Inspector::Handle(std::string_view line) const
{
	auto decoded = DecodeRequest(line);
	if (auto* problem = std::get_if<std::string>(&decoded); problem != nullptr)
	{
		// The id may still be read from a request that is JSON but otherwise wrong
		Json id;
		if (const auto parsed = Parse(line); parsed.has_value() && parsed->is_object() && parsed->contains("id"))
		{
			id = (*parsed)["id"];
		}
		return EncodeError(id, *problem);
	}
	const auto& request = std::get<Request>(decoded);
	const auto answer = Answer(request);
	if (!answer.Ok())
	{
		return EncodeError(request.id, answer.error);
	}
	return EncodeResult(request.id, answer.value);
}

bool Inspector::TakesControl(std::string_view line)
{
	const auto decoded = DecodeRequest(line);
	const auto* request = std::get_if<Request>(&decoded);
	return request == nullptr || request->query != "ping";
}
