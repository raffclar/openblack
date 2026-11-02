/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <array>

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

bool IsLoopbackHost(std::string_view host) noexcept
{
	if (host == "localhost")
	{
		return true;
	}
	// Four parts, each of one to three decimal digits with a value of at most 255, and the first part is 127
	constexpr size_t k_Parts = 4;
	constexpr size_t k_MostDigits = 3;
	constexpr uint32_t k_MostPart = 255;
	constexpr uint32_t k_LoopbackPart = 127;
	std::array<uint32_t, k_Parts> parts {};
	size_t part = 0;
	size_t digits = 0;
	for (const char c : host)
	{
		if (c == '.')
		{
			if (digits == 0 || ++part == k_Parts)
			{
				return false;
			}
			digits = 0;
			continue;
		}
		if (c < '0' || c > '9' || ++digits > k_MostDigits)
		{
			return false;
		}
		parts.at(part) = parts.at(part) * 10 + static_cast<uint32_t>(c - '0');
		if (parts.at(part) > k_MostPart)
		{
			return false;
		}
	}
	return part == k_Parts - 1 && digits != 0 && parts.front() == k_LoopbackPart;
}

bool IsLoopbackAddress(std::string_view url) noexcept
{
	std::string_view rest;
	if (IsHttpsAddress(url))
	{
		rest = url.substr(std::string_view("https://").size());
	}
	else if (IsWssAddress(url))
	{
		rest = url.substr(std::string_view("wss://").size());
	}
	else
	{
		return false;
	}
	return IsLoopbackHost(rest.substr(0, rest.find_first_of(":/?#")));
}

bool IsSameOriginPath(std::string_view url) noexcept
{
	// "//host/..." is an address of another origin with the scheme of the page
	return IsPlainText(url) && url.size() >= 1 && url.front() == '/' && (url.size() == 1 || url[1] != '/') &&
	       url.find('\\') == std::string_view::npos;
}

} // namespace openblack::online::http_policy
