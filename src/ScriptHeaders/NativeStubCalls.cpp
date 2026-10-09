/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "NativeStubCalls.h"

#include <algorithm>
#include <numeric>

namespace openblack::script
{

bool NativeStubCalls::Record(uint32_t native, std::optional<int32_t> detail)
{
	return ++_calls[{native, detail}] == 1;
}

uint32_t NativeStubCalls::Calls(uint32_t native) const
{
	uint32_t calls = 0;
	for (auto it = _calls.lower_bound({native, std::nullopt}); it != _calls.end() && it->first.first == native; ++it)
	{
		calls += it->second;
	}
	return calls;
}

std::vector<NativeStubCalls::Entry> NativeStubCalls::Entries() const
{
	std::vector<Entry> entries;
	entries.reserve(_calls.size());
	for (const auto& [key, calls] : _calls)
	{
		entries.push_back({.native = key.first, .detail = key.second, .calls = calls});
	}
	// The map is in native order already, so a stable sort keeps it among equal counts
	std::ranges::stable_sort(entries, [](const Entry& a, const Entry& b) { return a.calls > b.calls; });
	return entries;
}

uint32_t NativeStubCalls::Total() const
{
	return std::accumulate(_calls.begin(), _calls.end(), 0u,
	                       [](uint32_t sum, const auto& entry) { return sum + entry.second; });
}

void NativeStubCalls::Clear()
{
	_calls.clear();
}

} // namespace openblack::script
