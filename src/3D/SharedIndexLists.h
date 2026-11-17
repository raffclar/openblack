/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cassert>
#include <cstdint>

#include <map>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace openblack
{

/// Index lists written once into a few large buffers (pages) and shared by everything that draws the same list. Most
/// of a land's blocks (the open sea, plain land) have cells of the same flags and so draw the same triangles. Each list
/// takes a run of a page, so that the lists of a land take a few of the renderer's buffers. The renderer has a limit of
/// buffers, and a land can make more than a thousand lists as the camera moves.
template <typename Page>
class SharedIndexLists
{
public:
	/// Where the indices of a list are: a run of a page. An empty list has no page.
	struct Range
	{
		const Page* page {nullptr};
		uint32_t first {0};
		uint32_t count {0};
	};

	/// Each run starts at a multiple of this count of indices, thus at a multiple of four bytes. The web device writes
	/// buffers only at these offsets, and it fills a write of an odd count of indices to four bytes with zeros.
	static constexpr uint32_t k_Alignment = 2;

	/// Each page has room for this count of indices
	explicit SharedIndexLists(uint32_t pageCapacity)
	    : _pageCapacity(pageCapacity)
	{
	}

	/// The run of these indices. The first request writes them with page.Write(first, indices) into the last page, or
	/// into a new page from makePage(capacity) when the last page has no room. An empty list gives a range with no page:
	/// a block of open sea only has no triangles, and the graphics library cannot draw an empty run.
	template <typename MakePage>
	[[nodiscard]] Range Get(std::vector<uint16_t> indices, MakePage&& makePage)
	{
		if (indices.empty())
		{
			return {};
		}
		auto found = _lists.find(indices);
		if (found == _lists.end())
		{
			const auto count = static_cast<uint32_t>(indices.size());
			assert(count <= _pageCapacity);
			if (_pages.empty() || _used + count > _pageCapacity)
			{
				_pages.push_back(std::forward<MakePage>(makePage)(_pageCapacity));
				_used = 0;
			}
			const Range range {.page = _pages.back().get(), .first = _used, .count = count};
			range.page->Write(range.first, std::span<const uint16_t>(indices));
			_used += count + ((k_Alignment - (count % k_Alignment)) % k_Alignment);
			found = _lists.emplace(std::move(indices), range).first;
		}
		return found->second;
	}

	/// How many different lists have been made
	[[nodiscard]] size_t Size() const { return _lists.size(); }

	/// How many pages hold the lists
	[[nodiscard]] size_t PageCount() const { return _pages.size(); }

	/// Calls visit(indices, range) for each list, in the order of the lists
	template <typename Visit>
	void ForEach(Visit&& visit) const
	{
		for (const auto& [indices, range] : _lists)
		{
			visit(std::span<const uint16_t>(indices), range);
		}
	}

private:
	uint32_t _pageCapacity;
	/// The indices in use of the last page
	uint32_t _used {0};
	std::vector<std::unique_ptr<Page>> _pages;
	std::map<std::vector<uint16_t>, Range> _lists;
};

} // namespace openblack
