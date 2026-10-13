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
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

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

/// Why a request's parameters aren't all ones its query takes, naming those it doesn't and those it does; nothing when
/// they all are. A parameter a query doesn't know would otherwise be ignored, and the query answer as if it weren't given.
std::optional<std::string> UnknownParameters(std::string_view query, const Json& params, const std::vector<std::string>& known)
{
	std::string unknown;
	for (const auto& [key, value] : params.items())
	{
		if (std::ranges::find(known, key) == known.end())
		{
			unknown += (unknown.empty() ? "" : ", ") + key;
		}
	}
	if (unknown.empty())
	{
		return std::nullopt;
	}
	std::string takes;
	for (const auto& name : known)
	{
		takes += (takes.empty() ? "" : ", ") + name;
	}
	return std::string(query) + " doesn't take " + unknown + "; it takes " + (takes.empty() ? "no parameters" : takes);
}

/// Shaping members given inside a request's params, as tools put every argument, taken as if given beside them and
/// taken out of the params: limit, near and the others work the same in either place. A member the query takes as a
/// parameter of its own stays its parameter; near and radius stay together. Why not when one doesn't read or is given
/// in both places.
std::optional<std::string> TakeShapingFromParams(std::string_view query, const std::vector<std::string>& own, Json& params,
                                                 QueryOptions& options)
{
	const auto owns = [&own](std::string_view name) { return std::ranges::find(own, name) != own.end(); };
	const bool ownsNear = owns("near") || owns("radius");
	Json inside = Json::object();
	for (const auto name : k_ShapingMembers)
	{
		const bool isNear = name == "near" || name == "radius";
		if (const auto it = params.find(name); it != params.end() && !(isNear ? ownsNear : owns(name)))
		{
			inside[std::string(name)] = *it;
		}
	}
	if (inside.empty())
	{
		return std::nullopt;
	}
	const auto hint = std::string(" (shaping options such as near, radius and limit go beside params or inside them)");
	QueryOptions taken;
	if (auto why = ReadShapingOptions(inside, taken); why.has_value())
	{
		return std::string(query) + ": " + *why + hint;
	}

	// Each taken from params, unless it was given beside them too
	const QueryOptions unset;
	std::string twice;
	const auto take = [&inside, &twice](std::string_view name, bool besideToo, const auto& apply) {
		if (!inside.contains(name))
		{
			return;
		}
		if (besideToo)
		{
			twice += (twice.empty() ? "" : ", ") + std::string(name);
			return;
		}
		apply();
	};
	take("fields", !options.fields.empty(), [&] { options.fields = taken.fields; });
	take("where", !options.where.empty(), [&] { options.where = taken.where; });
	take("near", options.near.has_value(), [&] { options.near = taken.near; });
	take("limit", options.limit != unset.limit, [&] { options.limit = taken.limit; });
	take("cursor", options.cursor != unset.cursor, [&] { options.cursor = taken.cursor; });
	take("count", options.countOnly != unset.countOnly, [&] { options.countOnly = taken.countOnly; });
	take("summary", options.summary.has_value(), [&] { options.summary = taken.summary; });
	take("max_bytes", options.maxBytes != unset.maxBytes, [&] { options.maxBytes = taken.maxBytes; });
	if (!twice.empty())
	{
		return std::string(query) + ": " + twice + " given twice, beside params and inside them";
	}
	for (const auto& [name, value] : inside.items())
	{
		params.erase(name);
	}
	return std::nullopt;
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

void Inspector::SetIdentity(Json identity)
{
	_identity = std::move(identity);
	_tag = Json::object();
	if (_identity.is_object())
	{
		for (const auto* key : {"pid", "port", "worktree"})
		{
			if (const auto found = _identity.find(key); found != _identity.end())
			{
				_tag[key] = *found;
			}
		}
	}
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

Json Inspector::Catalogue() const
{
	Json queries = Json::array();
	for (const auto& provider : _providers)
	{
		for (const auto& description : provider->Describe())
		{
			queries.push_back(ToJson(description, provider->Name()));
		}
	}
	return queries;
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
	if (request.query == "ping" || request.query == "writes" || request.query == "describe")
	{
		const auto known =
		    request.query == "describe" ? std::vector<std::string> {"provider", "query"} : std::vector<std::string> {};
		auto params = request.params;
		auto options = request.options;
		// ping shapes nothing, so it takes no shaping options in its params either
		if (request.query != "ping")
		{
			if (auto why = TakeShapingFromParams(request.query, known, params, options); why.has_value())
			{
				return error(*std::move(why));
			}
		}
		if (auto unknown = UnknownParameters(request.query, params, known); unknown.has_value())
		{
			return error(*std::move(unknown));
		}
		if (request.query == "ping")
		{
			Json answer = _identity.is_object() ? _identity : Json::object();
			answer["pong"] = true;
			// Answered here, the game is serving its frames; while it loads, the loading answer says otherwise
			answer["ready"] = true;
			return QueryResult::Value(std::move(answer));
		}
		if (request.query == "writes")
		{
			return Writes(options);
		}
		auto described = Describe(params);
		if (!described.Ok())
		{
			return error(std::move(described.error));
		}
		// The catalogue of every query outgrows the size an ordinary answer is held to, so describe is held only to the
		// largest size unless the caller set its own
		if (options.maxBytes == k_DefaultMaxBytes)
		{
			options.maxBytes = k_MostBytes;
		}
		return QueryResult::Value(ShapeObject(described.value, options));
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
	std::vector<std::string> known;
	known.reserve(description->parameters.size());
	std::ranges::transform(description->parameters, std::back_inserter(known), &ParameterDescription::name);
	// Shaping options go beside params, but given inside them, as tools put every argument, they are taken the same
	auto params = request.params;
	auto options = request.options;
	if (auto why = TakeShapingFromParams(request.query, known, params, options); why.has_value())
	{
		return error(*std::move(why));
	}
	if (auto unknown = UnknownParameters(request.query, params, known); unknown.has_value())
	{
		return error(*std::move(unknown));
	}
	for (const auto& parameter : description->parameters)
	{
		if (parameter.required && !params.contains(parameter.name))
		{
			return error(request.query + " needs the parameter " + parameter.name + " (" + parameter.type + ")");
		}
	}
	if (description->needsNear && !options.near.has_value())
	{
		return error(request.query + " searches around a point: give near and radius beside params or inside them");
	}

	auto answered = provider->Run(queryName, QueryContext {.params = params, .options = options});
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
		return QueryResult::Value(ShapeList(std::move(answered.value), options));
	}
	return QueryResult::Value(ShapeObject(answered.value, options));
}

QueryResult Inspector::Writes(const QueryOptions& options) const
{
	Json items = Json::array();
	for (const auto& write : _writes)
	{
		items.push_back(write);
	}
	return QueryResult::Value(ShapeList(std::move(items), options));
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
	const Json extra = _tag.empty() ? Json::object() : Json {{"game", _tag}};
	auto decoded = DecodeRequest(line);
	if (auto* problem = std::get_if<std::string>(&decoded); problem != nullptr)
	{
		// The id may still be read from a request that is JSON but otherwise wrong
		Json id;
		if (const auto parsed = Parse(line); parsed.has_value() && parsed->is_object() && parsed->contains("id"))
		{
			id = (*parsed)["id"];
		}
		return EncodeError(id, *problem, extra);
	}
	const auto& request = std::get<Request>(decoded);
	const auto answer = Answer(request);
	if (!answer.Ok())
	{
		return EncodeError(request.id, answer.error, extra);
	}
	return EncodeResult(request.id, answer.value, extra);
}

std::string Inspector::HandleWhileLoading(std::string_view line, std::string_view loading,
                                          const QueuedWhileLoading& queued) const
{
	Json extra = _tag.empty() ? Json::object() : Json {{"game", _tag}};
	extra["loading"] = loading;
	auto decoded = DecodeRequest(line);
	if (auto* problem = std::get_if<std::string>(&decoded); problem != nullptr)
	{
		return EncodeError(Json(), *problem, extra);
	}
	const auto& request = std::get<Request>(decoded);
	if (request.query == "ping" || request.query == "describe")
	{
		auto answer = Answer(request);
		if (request.query == "ping")
		{
			answer.value["ready"] = false;
			answer.value["loading"] = loading;
		}
		return answer.Ok() ? EncodeResult(request.id, answer.value, extra) : EncodeError(request.id, answer.error, extra);
	}
	if (request.query == "game.state")
	{
		return EncodeResult(request.id, {{"ready", false}, {"loading", loading}}, extra);
	}
	if (queued)
	{
		if (const auto answer = queued(request); answer.has_value())
		{
			return answer->Ok() ? EncodeResult(request.id, answer->value, extra)
			                    : EncodeError(request.id, answer->error, extra);
		}
	}
	return EncodeError(request.id, "the game is loading (" + std::string(loading) + "); ask again once game.state says ready",
	                   extra);
}

bool Inspector::TakesControl(std::string_view line)
{
	const auto decoded = DecodeRequest(line);
	const auto* request = std::get_if<Request>(&decoded);
	return request == nullptr || request->query != "ping";
}
