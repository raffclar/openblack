/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "InspectorJson.h"
#include "InspectorProtocol.h"
#include "InspectorProvider.h"

namespace openblack::inspector
{

/// The inspector's providers, and the answering of requests: each request's query is found by its provider and name,
/// its required parameters checked, run, and its result shaped by the query language.
///
/// Three queries are always there: "describe", the providers and their queries (all of one provider's in full with
/// {"provider": name}, or a single query's with {"query": "provider.name"}); "ping", answered with what the game says
/// of itself; and "writes", the last queries that changed the game, newest first, with what they answered.
///
/// Queries that write are told to the write log as they are answered, whether they could be or not.
class Inspector
{
public:
	/// Told of each query that writes, with its answer
	using WriteLog = std::function<void(const Request& request, const QueryResult& answer)>;
	/// How many of the last writes "writes" remembers
	static constexpr size_t k_RememberedWrites = 50;

	void SetWriteLog(WriteLog log) { _writeLog = std::move(log); }
	/// What "ping" answers beside pong: the game's port, process and the like, for tools telling games apart
	void SetIdentity(Json identity) { _identity = std::move(identity); }

	/// Whether a request's line takes control of the game: everything but a ping does, so that tools can look for
	/// running games without keeping their players out
	[[nodiscard]] static bool TakesControl(std::string_view line);

	/// Adds a provider, replacing one of the same name
	void Add(std::unique_ptr<ProviderInterface> provider);
	[[nodiscard]] ProviderInterface* Find(std::string_view name) const;
	[[nodiscard]] size_t Count() const { return _providers.size(); }

	/// The answer to a request: its shaped result, or why there is none
	[[nodiscard]] QueryResult Answer(const Request& request) const;
	/// The answer's line to a request's line, as the server passes them
	[[nodiscard]] std::string Handle(std::string_view line) const;

private:
	[[nodiscard]] QueryResult Describe(const Json& params) const;
	[[nodiscard]] QueryResult Writes(const Request& request) const;
	void Remember(const Request& request, const QueryResult& answer) const;

	std::vector<std::unique_ptr<ProviderInterface>> _providers;
	WriteLog _writeLog;
	Json _identity = Json::object();
	/// Kept as requests are answered, which reading the game's state doesn't change
	mutable std::deque<Json> _writes;
};

/// A query's description as JSON
[[nodiscard]] Json ToJson(const QueryDescription& description, std::string_view provider);

} // namespace openblack::inspector
