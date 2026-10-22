/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>

#include "HttpClientInterface.h"

namespace openblack::online::http_policy
{
namespace
{

/// True when the text has no space and no control character
bool IsPlainText(std::string_view text) noexcept
{
	return std::ranges::none_of(text, [](char c) { return static_cast<unsigned char>(c) <= ' ' || c == 0x7F; });
}

bool HasScheme(std::string_view url, std::string_view scheme) noexcept
{
	if (url.size() < scheme.size())
	{
		return false;
	}
	const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
	return std::ranges::equal(url.substr(0, scheme.size()), scheme, {}, lower);
}

/// True when the address starts with the scheme and a host follows it
bool IsAddressOf(std::string_view url, std::string_view scheme) noexcept
{
	if (!IsPlainText(url) || !HasScheme(url, scheme))
	{
		return false;
	}
	const auto rest = url.substr(scheme.size());
	// The host has one character or more, and the address does not give a user name and a password
	const auto hostEnd = rest.find_first_of("/?#");
	const auto host = rest.substr(0, hostEnd);
	return !host.empty() && host.find('@') == std::string_view::npos && host.front() != ':';
}

} // namespace

bool IsHttpsAddress(std::string_view url) noexcept
{
	return IsAddressOf(url, "https://");
}

bool IsWssAddress(std::string_view url) noexcept
{
	return IsAddressOf(url, "wss://");
}

bool IsSameOriginPath(std::string_view url) noexcept
{
	// "//host/..." is an address of another origin with the scheme of the page
	return IsPlainText(url) && url.size() >= 1 && url.front() == '/' && (url.size() == 1 || url[1] != '/') &&
	       url.find('\\') == std::string_view::npos;
}

} // namespace openblack::online::http_policy
