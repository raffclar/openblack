/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptNameLists.h"

#include <cctype>

#include <algorithm>

namespace openblack::script::name_lists
{

namespace
{
constexpr std::string_view k_Separators = " ,\t";

template <typename Matches>
bool Holds(std::string_view list, Matches matches)
{
	while (!list.empty())
	{
		const auto start = list.find_first_not_of(k_Separators);
		if (start == std::string_view::npos)
		{
			return false;
		}
		list.remove_prefix(start);
		const auto end = std::min(list.find_first_of(k_Separators), list.size());
		if (matches(list.substr(0, end)))
		{
			return true;
		}
		list.remove_prefix(end);
	}
	return false;
}
} // namespace

bool HoldsScript(std::string_view list, std::string_view name)
{
	return Holds(list, [name](std::string_view word) { return word == name; });
}

bool HoldsFile(std::string_view list, std::string_view name)
{
	return Holds(list, [name](std::string_view word) {
		return std::ranges::equal(word, name, [](char a, char b) {
			return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
		});
	});
}

} // namespace openblack::script::name_lists
