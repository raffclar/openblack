/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <algorithm>
#include <span>
#include <vector>

#include "Common/SameBits.h"

/// Finds the parts of an instance buffer that changed since its last upload, so that only those parts go to the GPU.
/// The renderer keeps a copy of what it uploaded. Instances are equal only when all their bits are equal.
namespace openblack::graphics::instance_upload
{

/// Instances one after another, from the first
struct Run
{
	uint32_t first {0};
	uint32_t count {0};

	bool operator==(const Run&) const = default;
};

/// Two changed runs with fewer unchanged instances than this between them go up as one run
constexpr uint32_t k_JoinGap = 64;
/// More runs than this go up as one run, from the first changed instance to the last
constexpr size_t k_MaxRuns = 16;

/// The runs of instances that differ between now and the last upload. When the counts differ, or nothing was uploaded,
/// all of now is one run.
template <typename Instance>
[[nodiscard]] std::vector<Run> ChangedRuns(std::span<const Instance> now, std::span<const Instance> uploaded)
{
	const auto count = static_cast<uint32_t>(now.size());
	if (now.size() != uploaded.size())
	{
		return count == 0 ? std::vector<Run> {} : std::vector<Run> {{.first = 0, .count = count}};
	}
	std::vector<Run> runs;
	for (uint32_t i = 0; i < count; ++i)
	{
		if (same_bits::SameBits(now[i], uploaded[i]))
		{
			continue;
		}
		if (!runs.empty() && i - (runs.back().first + runs.back().count) < k_JoinGap)
		{
			runs.back().count = i + 1 - runs.back().first;
		}
		else
		{
			runs.push_back({.first = i, .count = 1});
		}
	}
	if (runs.size() > k_MaxRuns)
	{
		const auto first = runs.front().first;
		const auto end = runs.back().first + runs.back().count;
		runs = {{.first = first, .count = end - first}};
	}
	return runs;
}

/// A copy of what an instance buffer on the GPU holds
template <typename Instance>
class Mirror
{
public:
	/// The runs that differ from what the buffer holds, which then holds now
	[[nodiscard]] std::vector<Run> Take(std::span<const Instance> now)
	{
		const auto runs =
		    _valid ? ChangedRuns(now, std::span<const Instance>(_uploaded)) : ChangedRuns(now, std::span<const Instance> {});
		if (!_valid || _uploaded.size() != now.size())
		{
			_uploaded.assign(now.begin(), now.end());
		}
		else
		{
			for (const auto& run : runs)
			{
				std::copy_n(now.begin() + run.first, run.count, _uploaded.begin() + run.first);
			}
		}
		_valid = true;
		return runs;
	}
	/// The buffer was made again: what it holds is not known
	void Forget()
	{
		_valid = false;
		_uploaded.clear();
	}

private:
	std::vector<Instance> _uploaded;
	bool _valid {false};
};

} // namespace openblack::graphics::instance_upload
