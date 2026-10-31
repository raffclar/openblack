/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StateHashWatch.h"

#include <charconv>

#include <map>
#include <string_view>
#include <system_error>

#include <fmt/format.h>

namespace openblack::state_hash
{

std::string HexOf(uint64_t hash)
{
	return fmt::format("{:016x}", hash);
}

std::vector<Difference> Differences(const Snapshot& pinned, const Snapshot& current)
{
	std::map<std::string, std::pair<std::optional<uint64_t>, std::optional<uint64_t>>> byName;
	for (const auto& [name, hash] : pinned.parts)
	{
		byName[name].first = hash;
	}
	for (const auto& [name, hash] : current.parts)
	{
		byName[name].second = hash;
	}
	std::vector<Difference> differences;
	for (const auto& [name, hashes] : byName)
	{
		if (hashes.first != hashes.second)
		{
			differences.push_back({.part = name, .pinned = hashes.first, .current = hashes.second});
		}
	}
	return differences;
}

Match Compare(const std::optional<Snapshot>& pinned, const Snapshot& current)
{
	if (!pinned.has_value())
	{
		return Match::NothingPinned;
	}
	return pinned->hash == current.hash ? Match::Same : Match::Different;
}

std::string Line(const Snapshot& snapshot)
{
	return fmt::format("turn {} hash {}", snapshot.turn, HexOf(snapshot.hash));
}

std::optional<Snapshot> FromLine(std::string_view text)
{
	constexpr std::string_view k_Turn = "turn ";
	constexpr std::string_view k_Hash = " hash ";
	constexpr size_t k_HashDigits = 16;
	if (!text.starts_with(k_Turn))
	{
		return std::nullopt;
	}
	const auto split = text.find(k_Hash);
	if (split == std::string_view::npos)
	{
		return std::nullopt;
	}
	Snapshot snapshot;
	const auto turnText = text.substr(k_Turn.size(), split - k_Turn.size());
	const auto hashText = text.substr(split + k_Hash.size());
	const auto* turnEnd = turnText.data() + turnText.size();
	if (const auto [end, error] = std::from_chars(turnText.data(), turnEnd, snapshot.turn);
	    error != std::errc {} || end != turnEnd || hashText.size() != k_HashDigits)
	{
		return std::nullopt;
	}
	const auto* hashEnd = hashText.data() + hashText.size();
	if (const auto [end, error] = std::from_chars(hashText.data(), hashEnd, snapshot.hash, 16);
	    error != std::errc {} || end != hashEnd)
	{
		return std::nullopt;
	}
	return snapshot;
}

bool Due(uint32_t every, uint32_t turn, std::optional<uint32_t> lastTurn)
{
	return every != 0 && turn % every == 0 && lastTurn != turn;
}

} // namespace openblack::state_hash
