/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FightTranscript.h"

#include <cmath>

#include <algorithm>
#include <format>
#include <ranges>
#include <type_traits>
#include <utility>

using namespace openblack::fight_transcript;

Field::Field(std::string_view fieldName, bool flag)
    : name(fieldName)
    , value(flag)
{
}

Field::Field(std::string_view fieldName, int32_t number)
    : name(fieldName)
    , value(static_cast<int64_t>(number))
{
}

Field::Field(std::string_view fieldName, uint32_t number)
    : name(fieldName)
    , value(static_cast<int64_t>(number))
{
}

Field::Field(std::string_view fieldName, int64_t number)
    : name(fieldName)
    , value(number)
{
}

Field::Field(std::string_view fieldName, uint64_t number)
    : name(fieldName)
    , value(static_cast<int64_t>(number))
{
}

Field::Field(std::string_view fieldName, float number)
    : name(fieldName)
    , value(static_cast<double>(number))
{
}

Field::Field(std::string_view fieldName, double number)
    : name(fieldName)
    , value(number)
{
}

Field::Field(std::string_view fieldName, std::string_view text)
    : name(fieldName)
    , value(std::string(text))
{
}

Field::Field(std::string_view fieldName, const char* text)
    : name(fieldName)
    , value(std::string(text))
{
}

Field::Field(std::string_view fieldName, std::string text)
    : name(fieldName)
    , value(std::move(text))
{
}

uint32_t Recorder::Begin(uint32_t first, uint32_t second, const Stamp& stamp, Fields fields)
{
	const auto id = _nextId++;
	_fights.push_back({.id = id, .first = first, .second = second, .started = stamp, .ended = std::nullopt, .entries = {}});
	_lastFight[first] = id;
	_lastFight[second] = id;
	static_cast<void>(AddTo(id, "fight_start", stamp, std::move(fields)));
	return id;
}

std::optional<uint64_t> Recorder::Add(uint32_t creature, std::string_view kind, const Stamp& stamp, Fields fields,
                                      std::optional<uint64_t> cause)
{
	const auto fight = FightOf(creature);
	if (!fight.has_value())
	{
		return std::nullopt;
	}
	return AddTo(*fight, kind, stamp, std::move(fields), cause);
}

std::optional<uint64_t> Recorder::AddTo(uint32_t fight, std::string_view kind, const Stamp& stamp, Fields fields,
                                        std::optional<uint64_t> cause)
{
	auto* found = FindMutable(fight);
	if (found == nullptr)
	{
		return std::nullopt;
	}
	const auto index = static_cast<uint64_t>(found->entries.size());
	// A cause must be an earlier record of the same fight
	if (cause.has_value() && *cause >= index)
	{
		cause.reset();
	}
	found->entries.push_back(
	    {.index = index, .stamp = stamp, .kind = std::string(kind), .cause = cause, .fields = std::move(fields)});
	return index;
}

void Recorder::End(uint32_t fight, const Stamp& stamp)
{
	if (auto* found = FindMutable(fight); found != nullptr && !found->ended.has_value())
	{
		found->ended = stamp;
	}
}

std::optional<uint32_t> Recorder::FightOf(uint32_t creature) const
{
	const auto found = _lastFight.find(creature);
	if (found == _lastFight.end() || Find(found->second) == nullptr)
	{
		return std::nullopt;
	}
	return found->second;
}

std::optional<uint64_t> Recorder::LastOf(uint32_t creature, std::string_view kind) const
{
	const auto fight = FightOf(creature);
	if (!fight.has_value())
	{
		return std::nullopt;
	}
	const auto& entries = Find(*fight)->entries;
	const auto own = [creature](const Field& field) {
		const auto* number = std::get_if<int64_t>(&field.value);
		return (field.name == "creature" || field.name == "attacker") && number != nullptr &&
		       *number == static_cast<int64_t>(creature);
	};
	for (const auto& entry : std::ranges::reverse_view(entries))
	{
		if (entry.kind == kind && std::ranges::any_of(entry.fields, own))
		{
			return entry.index;
		}
	}
	return std::nullopt;
}

const Fight* Recorder::Find(uint32_t fight) const
{
	const auto found = std::ranges::find(_fights, fight, &Fight::id);
	return found != _fights.end() ? &*found : nullptr;
}

Fight* Recorder::FindMutable(uint32_t fight)
{
	const auto found = std::ranges::find(_fights, fight, &Fight::id);
	return found != _fights.end() ? &*found : nullptr;
}

void Recorder::Clear()
{
	_fights.clear();
	_lastFight.clear();
}

namespace
{

std::string Quoted(std::string_view text)
{
	std::string out = "\"";
	for (const char c : text)
	{
		switch (c)
		{
		case '"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\r':
			out += "\\r";
			break;
		case '\t':
			out += "\\t";
			break;
		default:
			if (static_cast<unsigned char>(c) < 0x20)
			{
				out += std::format("\\u{:04x}", static_cast<unsigned>(c));
			}
			else
			{
				out += c;
			}
		}
	}
	out += '"';
	return out;
}

std::string Number(double number)
{
	// JSON has no value for a number that is not finite
	return std::isfinite(number) ? std::format("{}", number) : std::string("null");
}

std::string ValueJson(const Value& value)
{
	return std::visit(
	    [](const auto& held) -> std::string {
		    using T = std::decay_t<decltype(held)>;
		    if constexpr (std::is_same_v<T, bool>)
		    {
			    return held ? "true" : "false";
		    }
		    else if constexpr (std::is_same_v<T, int64_t>)
		    {
			    return std::format("{}", held);
		    }
		    else if constexpr (std::is_same_v<T, double>)
		    {
			    return Number(held);
		    }
		    else
		    {
			    return Quoted(held);
		    }
	    },
	    value);
}

std::string StampJson(const Stamp& stamp)
{
	return std::format(R"({{"frame":{},"turn":{},"game_ms":{}}})", stamp.frame, stamp.turn, Number(stamp.gameMs));
}

} // namespace

std::string openblack::fight_transcript::ToJsonLine(const Fight& fight, const Entry& entry)
{
	std::string fields = "{";
	for (const auto& field : entry.fields)
	{
		if (fields.size() > 1)
		{
			fields += ',';
		}
		fields += Quoted(field.name) + ':' + ValueJson(field.value);
	}
	fields += '}';
	return std::format(R"({{"v":{},"fight":{},"i":{},"frame":{},"turn":{},"game_ms":{},"kind":{},"cause":{},"fields":{}}})",
	                   k_Version, fight.id, entry.index, entry.stamp.frame, entry.stamp.turn, Number(entry.stamp.gameMs),
	                   Quoted(entry.kind), entry.cause.has_value() ? std::format("{}", *entry.cause) : "null", fields);
}

std::string openblack::fight_transcript::SummaryJson(const Fight& fight)
{
	return std::format(R"({{"v":{},"fight":{},"first":{},"second":{},"started":{},"ended":{},"records":{}}})", k_Version,
	                   fight.id, fight.first, fight.second, StampJson(fight.started),
	                   fight.ended.has_value() ? StampJson(*fight.ended) : "null", fight.entries.size());
}
