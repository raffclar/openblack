/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace openblack::script
{

/// Counts the calls the scripts make to natives openblack hasn't written yet, so that each one is reported once rather
/// than on every call, and the ones the scripts lean on most can be listed. A native written for some of its cases
/// only counts each unwritten case (such as a property number) apart.
class NativeStubCalls
{
public:
	struct Entry
	{
		uint32_t native;
		/// The unwritten case of a partly written native, none for a native not written at all
		std::optional<int32_t> detail;
		uint32_t calls;
	};

	/// Counts one call; true for the first call of this native and case, the one worth reporting
	bool Record(uint32_t native, std::optional<int32_t> detail = std::nullopt);
	/// The calls of a native over all its unwritten cases
	[[nodiscard]] uint32_t Calls(uint32_t native) const;
	/// Every native and case called, the most called first and then by native number
	[[nodiscard]] std::vector<Entry> Entries() const;
	[[nodiscard]] uint32_t Total() const;
	void Clear();

private:
	std::map<std::pair<uint32_t, std::optional<int32_t>>, uint32_t> _calls;
};

} // namespace openblack::script
