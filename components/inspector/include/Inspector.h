/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

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
/// Two queries are always there: "describe", the providers and their queries (all of one provider's in full with
/// {"provider": name}, or a single query's with {"query": "provider.name"}), and "ping".
class Inspector
{
public:
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

	std::vector<std::unique_ptr<ProviderInterface>> _providers;
};

/// A query's description as JSON
[[nodiscard]] Json ToJson(const QueryDescription& description, std::string_view provider);

} // namespace openblack::inspector
