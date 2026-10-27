/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BuildVersionFile.h"

#include <cctype>

#include <algorithm>
#include <utility>

namespace openblack::build_version
{

namespace
{

[[nodiscard]] bool IsSpace(char character) noexcept
{
	return std::isspace(static_cast<unsigned char>(character)) != 0;
}

[[nodiscard]] bool IsHexDigit(char character) noexcept
{
	return std::isxdigit(static_cast<unsigned char>(character)) != 0;
}

} // namespace

std::string BuildVersion::Text() const
{
	if (!Known())
	{
		return std::string(k_Unknown);
	}
	return dirty ? hash + std::string(k_DirtySuffix) : hash;
}

std::string BuildVersion::Short() const
{
	if (!Known())
	{
		return std::string(k_Unknown);
	}
	auto text = hash.substr(0, k_ShortLength);
	return dirty ? text + std::string(k_DirtySuffix) : text;
}

std::optional<std::string> BuildVersion::CommitAddress() const
{
	if (!Known())
	{
		return std::nullopt;
	}
	return std::string(k_CommitAddress) + hash;
}

BuildVersion Parse(std::string_view text)
{
	const auto first = std::ranges::find_if_not(text, IsSpace);
	const auto last = std::ranges::find_if_not(text.rbegin(), text.rend(), IsSpace).base();
	if (first >= last)
	{
		return {};
	}
	auto value = std::string_view(first, last);
	bool dirty = false;
	if (value.ends_with(k_DirtySuffix))
	{
		dirty = true;
		value.remove_suffix(k_DirtySuffix.size());
	}
	if (value.size() != k_FullLength || !std::ranges::all_of(value, IsHexDigit))
	{
		return {};
	}
	std::string hash(value);
	std::ranges::transform(hash, hash.begin(), [](char character) {
		return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	});
	return {.hash = std::move(hash), .dirty = dirty};
}

} // namespace openblack::build_version
