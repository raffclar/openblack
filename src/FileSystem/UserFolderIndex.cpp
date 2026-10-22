/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "UserFolderIndex.h"

#include <charconv>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <system_error>

namespace
{

/// The number at the start of a field, or no value when the field is not a full number
template <typename T>
std::optional<T> ReadNumber(std::string_view field)
{
	T value {};
	const auto [end, error] = std::from_chars(field.data(), field.data() + field.size(), value);
	if (error != std::errc() || end != field.data() + field.size())
	{
		return std::nullopt;
	}
	return value;
}

} // namespace

std::string openblack::filesystem::LowerName(std::string_view name)
{
	std::string lower(name);
	std::ranges::transform(lower, lower.begin(),
	                       [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; });
	return lower;
}

openblack::filesystem::UserFolderIndex openblack::filesystem::UserFolderIndex::Parse(std::string_view list)
{
	UserFolderIndex index;
	for (const auto lineRange : std::views::split(list, '\n'))
	{
		std::string_view line(lineRange.begin(), lineRange.end());
		if (!line.empty() && line.back() == '\r')
		{
			line.remove_suffix(1);
		}
		const auto firstTab = line.find('\t');
		const auto secondTab = firstTab == std::string_view::npos ? firstTab : line.find('\t', firstTab + 1);
		if (secondTab == std::string_view::npos)
		{
			continue;
		}
		const auto file = ReadNumber<uint32_t>(line.substr(0, firstTab));
		const auto size = ReadNumber<uint64_t>(line.substr(firstTab + 1, secondTab - firstTab - 1));
		const auto path = line.substr(secondTab + 1);
		if (!file.has_value() || !size.has_value() || path.empty())
		{
			continue;
		}

		// Walk down the folders of the path, and add each folder that the tree does not have yet
		uint32_t folder = 0;
		auto parts = std::views::split(path, '/');
		for (auto it = parts.begin(); it != parts.end(); ++it)
		{
			const std::string_view name((*it).begin(), (*it).end());
			if (name.empty() || name == "." || name == "..")
			{
				break;
			}
			const bool last = std::next(it) == parts.end();
			const auto key = LowerName(name);
			if (const auto found = index._nodes[folder].children.find(key); found != index._nodes[folder].children.end())
			{
				// A folder can hold more entries. A file with a name that the tree has already is a second copy.
				if (last || index._nodes[found->second].file.has_value())
				{
					break;
				}
				folder = found->second;
				continue;
			}
			const auto node = static_cast<uint32_t>(index._nodes.size());
			index._nodes.push_back(Node {
			    .name = std::string(name),
			    .file = last ? file : std::nullopt,
			    .size = last ? *size : 0,
			    .children = {},
			});
			index._nodes[folder].children.emplace(key, node);
			folder = node;
		}
	}
	return index;
}

std::optional<uint32_t> openblack::filesystem::UserFolderIndex::Find(std::string_view path) const
{
	uint32_t node = 0;
	for (const auto partRange : std::views::split(path, '/'))
	{
		const std::string_view part(partRange.begin(), partRange.end());
		if (part.empty() || part == ".")
		{
			continue;
		}
		const auto& children = _nodes[node].children;
		const auto found = children.find(LowerName(part));
		if (found == children.end())
		{
			return std::nullopt;
		}
		node = found->second;
	}
	return node;
}
