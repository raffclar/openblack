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

#include <iterator>
#include <map>
#include <optional>

namespace openblack::graphics
{

/// The runs of one page of a buffer that are in use and the runs that are free. A run is a set of elements (vertices or
/// indices) that follow each other. Each run starts at a multiple of the alignment and its length is a multiple of the
/// alignment. The page gives the first free run that is long enough.
class PageRuns
{
public:
	PageRuns(uint32_t capacity, uint32_t alignment)
	    : _capacity(capacity - (capacity % alignment))
	    , _alignment(alignment)
	{
		assert(alignment > 0);
		if (_capacity > 0)
		{
			_free.emplace(0, _capacity);
		}
	}

	/// The length that a run of this count of elements takes in the page
	[[nodiscard]] uint32_t Length(uint32_t count) const
	{
		const auto rest = count % _alignment;
		return rest == 0 ? count : count + (_alignment - rest);
	}

	/// The first element of a new run of this count of elements, or nothing when no free run is long enough
	[[nodiscard]] std::optional<uint32_t> Take(uint32_t count)
	{
		const auto length = Length(count);
		if (length == 0)
		{
			return std::nullopt;
		}
		for (auto it = _free.begin(); it != _free.end(); ++it)
		{
			const auto [first, free] = *it;
			if (free < length)
			{
				continue;
			}
			_free.erase(it);
			if (free > length)
			{
				_free.emplace(first + length, free - length);
			}
			_used += length;
			return first;
		}
		return std::nullopt;
	}

	/// Makes a run that Take gave free again. The free run joins the free runs before and after it.
	void Give(uint32_t first, uint32_t count)
	{
		auto length = Length(count);
		assert(first + length <= _capacity && length <= _used);
		_used -= length;
		auto next = _free.lower_bound(first);
		if (next != _free.end() && first + length == next->first)
		{
			length += next->second;
			next = _free.erase(next);
		}
		if (next != _free.begin())
		{
			const auto before = std::prev(next);
			if (before->first + before->second == first)
			{
				before->second += length;
				return;
			}
		}
		_free.emplace(first, length);
	}

	[[nodiscard]] uint32_t Capacity() const { return _capacity; }

	/// The count of elements that the runs in use take, with the lengths that the alignment adds
	[[nodiscard]] uint32_t Used() const { return _used; }

	/// Whether no run is in use
	[[nodiscard]] bool Empty() const { return _used == 0; }

	/// The count of free runs, which is one for a page with no gaps between its runs
	[[nodiscard]] size_t FreeRunCount() const { return _free.size(); }

private:
	uint32_t _capacity;
	uint32_t _alignment;
	uint32_t _used {0};
	/// The first element and the length of each free run, in the order of the page
	std::map<uint32_t, uint32_t> _free;
};

} // namespace openblack::graphics
