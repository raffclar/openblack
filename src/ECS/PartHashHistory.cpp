/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PartHashHistory.h"

#include <algorithm>
#include <iterator>

namespace openblack::synced_state
{

void PartHashHistory::Keep(uint32_t turn, std::span<const std::pair<std::string, uint64_t>> parts)
{
	while (!_entries.empty() && _entries.back().turn >= turn)
	{
		_entries.pop_back();
	}
	const bool sameNames =
	    _names != nullptr && _names->size() == parts.size() &&
	    std::ranges::equal(*_names, parts, [](const std::string& name, const auto& part) { return name == part.first; });
	if (!sameNames)
	{
		auto names = std::make_shared<std::vector<std::string>>();
		names->reserve(parts.size());
		std::ranges::transform(parts, std::back_inserter(*names), [](const auto& part) { return part.first; });
		_names = std::move(names);
	}
	Entry entry {.turn = turn, .names = _names, .values = {}};
	entry.values.reserve(parts.size());
	std::ranges::transform(parts, std::back_inserter(entry.values), [](const auto& part) { return part.second; });
	_entries.push_back(std::move(entry));
	while (_entries.size() > k_KeptTurns)
	{
		_entries.pop_front();
	}
}

PartHashHistory::Parts PartHashHistory::Of(uint32_t turn) const
{
	const auto found = std::ranges::find(_entries, turn, &Entry::turn);
	Parts parts;
	if (found == _entries.end())
	{
		return parts;
	}
	parts.reserve(found->values.size());
	for (size_t i = 0; i < found->values.size(); ++i)
	{
		parts.emplace_back((*found->names)[i], found->values[i]);
	}
	return parts;
}

} // namespace openblack::synced_state
