/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MemoryUseRules.h"

#include <algorithm>
#include <iterator>
#include <numeric>

using namespace openblack::debug;

std::size_t memory_use::BytesOf(const Storage& storage, std::size_t entitySize)
{
	return storage.capacity * (entitySize + storage.componentSize);
}

std::vector<memory_use::Row> memory_use::Rows(std::span<const Storage> storages, std::size_t entitySize)
{
	std::vector<Row> rows;
	rows.reserve(storages.size());
	std::ranges::transform(storages, std::back_inserter(rows), [entitySize](const Storage& storage) {
		return Row {.name = storage.name, .count = storage.count, .bytes = BytesOf(storage, entitySize)};
	});
	std::ranges::sort(rows,
	                  [](const Row& a, const Row& b) { return a.bytes != b.bytes ? a.bytes > b.bytes : a.name < b.name; });
	return rows;
}

std::size_t memory_use::TotalBytes(std::span<const Row> rows)
{
	return std::accumulate(rows.begin(), rows.end(), std::size_t {0},
	                       [](std::size_t total, const Row& row) { return total + row.bytes; });
}
