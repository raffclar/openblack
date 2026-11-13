/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DesyncReports.h"

#include <algorithm>
#include <ranges>

#include <fmt/format.h>

namespace openblack::error_reports
{

namespace
{

/// A name of a part with only the characters of the names of the code, so that a name that a client sent cannot carry
/// other text into the event
std::string CleanPartName(std::string_view name)
{
	std::string clean;
	for (const char c : name | std::views::take(k_MostDesyncPartLength))
	{
		const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
		                     c == '.' || c == ':' || c == '<' || c == '>' || c == '-' || c == ' ';
		clean.push_back(allowed ? c : '?');
	}
	return clean;
}

std::string Hex(uint64_t value)
{
	return fmt::format("{:016x}", value);
}

} // namespace

std::string DesyncKey(std::span<const std::string> parts)
{
	if (parts.empty() || parts.front().empty())
	{
		return std::string(k_UnknownDesyncPart);
	}
	return CleanPartName(parts.front());
}

DesyncReportRule::DesyncReportRule(std::string game, uint8_t turnCodec)
    : _game(std::move(game))
    , _turnCodec(turnCodec)
{
}

void DesyncReportRule::Detected(uint8_t source, const DesyncFacts& facts, int64_t nowMilliseconds)
{
	if (_finished)
	{
		return;
	}
	if (_waiting.contains(source))
	{
		Resolve(source, {});
	}
	_waiting[source] = Waiting {.facts = facts, .detectedAt = nowMilliseconds};
}

void DesyncReportRule::Parted(uint8_t source, std::span<const std::string> parts, int64_t /*nowMilliseconds*/)
{
	if (_finished || !_waiting.contains(source))
	{
		return;
	}
	Resolve(source, parts);
}

void DesyncReportRule::Resolve(uint8_t source, std::span<const std::string> parts)
{
	const auto waiting = _waiting.find(source);
	const auto facts = waiting->second.facts;
	_waiting.erase(waiting);
	++_desyncs;
	auto key = DesyncKey(parts);
	const auto known = std::ranges::find(_counts, key, &std::pair<std::string, uint32_t>::first);
	if (known != _counts.end())
	{
		++known->second;
		return;
	}
	_counts.emplace_back(key, 1);
	std::vector<std::string> names;
	for (const auto& part : parts | std::views::take(k_MostDesyncPartsInEvent))
	{
		names.push_back(CleanPartName(part));
	}
	_ready.push_back(Ready {.key = std::move(key), .facts = facts, .parts = std::move(names), .desyncsInGame = _desyncs});
}

std::vector<DesyncReport> DesyncReportRule::Update(int64_t nowMilliseconds, DesyncRunBudget& budget)
{
	std::vector<DesyncReport> reports;
	if (_finished)
	{
		return reports;
	}
	// A desync whose parts did not come in time has no known part
	for (auto it = _waiting.begin(); it != _waiting.end();)
	{
		const auto source = it->first;
		const bool late = nowMilliseconds - it->second.detectedAt >= k_DesyncPartsWaitMilliseconds;
		++it;
		if (late)
		{
			Resolve(source, {});
		}
	}
	if (_ready.empty() || _eventsSent >= k_MostDesyncEventsInGame)
	{
		return reports;
	}
	if (_lastEventAt.has_value() && nowMilliseconds - *_lastEventAt < k_DesyncEventGapMilliseconds)
	{
		return reports;
	}
	if (!budget.Take())
	{
		return reports;
	}
	auto ready = std::move(_ready.front());
	_ready.erase(_ready.begin());
	++_eventsSent;
	_lastEventAt = nowMilliseconds;
	reports.push_back(DesyncReport {
	    .kind = DesyncReport::Kind::Desync,
	    .game = _game,
	    .turnCodec = _turnCodec,
	    .key = std::move(ready.key),
	    .facts = ready.facts,
	    .parts = std::move(ready.parts),
	    .desyncsInGame = ready.desyncsInGame,
	    .counts = {},
	    .notSent = 0,
	});
	return reports;
}

std::optional<DesyncReport> DesyncReportRule::Finish(DesyncRunBudget& budget)
{
	if (std::exchange(_finished, true))
	{
		return std::nullopt;
	}
	// The desyncs that still wait for their parts count with no known part
	while (!_waiting.empty())
	{
		Resolve(_waiting.begin()->first, {});
	}
	const auto notSent = _desyncs - _eventsSent;
	if (notSent == 0 || !budget.Take())
	{
		return std::nullopt;
	}
	return DesyncReport {
	    .kind = DesyncReport::Kind::Summary,
	    .game = _game,
	    .turnCodec = _turnCodec,
	    .key = {},
	    .facts = {},
	    .parts = {},
	    .desyncsInGame = _desyncs,
	    .counts = _counts,
	    .notSent = notSent,
	};
}

uint32_t DesyncReportRule::CountOf(std::string_view key) const
{
	const auto known = std::ranges::find(_counts, key, &std::pair<std::string, uint32_t>::first);
	return known != _counts.end() ? known->second : 0;
}

DesyncEventFields DesyncEventFieldsOf(const DesyncReport& report)
{
	DesyncEventFields fields;
	fields.tags.emplace_back("game", report.game);
	fields.tags.emplace_back("turn_codec", fmt::format("{}", report.turnCodec));
	fields.extras.emplace_back("desyncs_in_game", fmt::format("{}", report.desyncsInGame));
	if (report.kind == DesyncReport::Kind::Summary)
	{
		fields.message = fmt::format("Desync summary: {} desyncs in the game, {} not sent as their own event",
		                             report.desyncsInGame, report.notSent);
		fields.level = "info";
		fields.fingerprint = {"desync-summary"};
		fields.extras.emplace_back("not_sent", fmt::format("{}", report.notSent));
		for (const auto& [key, count] : report.counts)
		{
			fields.extras.emplace_back(fmt::format("count.{}", key), fmt::format("{}", count));
		}
		return fields;
	}
	fields.message = fmt::format("Desync: {}", report.key);
	fields.level = "warning";
	fields.fingerprint = {"desync", report.key};
	fields.tags.emplace_back("desync_key", report.key);
	if (report.facts.slot.has_value())
	{
		fields.tags.emplace_back("slot", fmt::format("{}", *report.facts.slot));
	}
	fields.extras.emplace_back("turn", fmt::format("{}", report.facts.turn));
	fields.extras.emplace_back("own_checksum", Hex(report.facts.ownChecksum));
	fields.extras.emplace_back("other_checksum", Hex(report.facts.otherChecksum));
	std::string parts;
	for (const auto& part : report.parts)
	{
		parts += parts.empty() ? part : ", " + part;
	}
	fields.extras.emplace_back("parts", parts.empty() ? std::string(k_UnknownDesyncPart) : parts);
	return fields;
}

std::string ClientGameId(uint64_t random)
{
	return Hex(random);
}

} // namespace openblack::error_reports
