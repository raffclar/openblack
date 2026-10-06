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
#include <string>
#include <string_view>
#include <vector>

namespace openblack::lhvm::chl
{

/// Named game constants, read from the script header files (enum declarations and #defines) that challenge scripts
/// include. CHL lets a member be written without its enum's name as a prefix: SCRIPT_OBJECT_TYPE_HOUSE is also HOUSE.
class ConstantTable
{
public:
	void Add(std::string_view enumName, std::string_view member, int32_t value);

	/// Read the enum declarations and #define constants of a C header. Returns the number of constants added.
	size_t LoadHeader(std::string_view text);

	/// The name to write for `value` as a member of `enumName`, prefix removed, if the enum has such a member
	[[nodiscard]] std::optional<std::string> NameOf(std::string_view enumName, int32_t value) const;

	/// The value of a constant, written with or without its enum prefix
	[[nodiscard]] std::optional<int32_t> ValueOf(std::string_view name) const;

	[[nodiscard]] bool Empty() const { return _values.empty(); }

private:
	/// enum name -> value -> member names in declaration order
	std::map<std::string, std::map<int32_t, std::vector<std::string>>, std::less<>> _enums;
	std::map<std::string, int32_t, std::less<>> _values;
};

} // namespace openblack::lhvm::chl
