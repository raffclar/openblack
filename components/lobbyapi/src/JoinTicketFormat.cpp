/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>

#include <nlohmann/json.hpp>

#include "JoinTicket.h"

namespace openblack::join_ticket
{
namespace
{

using Json = nlohmann::json;

constexpr std::string_view k_Header = "v4.public.";
constexpr std::string_view k_PublicKeyPrefix = "k4.public.";
constexpr std::string_view k_Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::optional<uint8_t> ValueOf(char letter)
{
	const auto found = k_Alphabet.find(letter);
	if (found == std::string_view::npos)
	{
		return std::nullopt;
	}
	return static_cast<uint8_t>(found);
}

std::span<const uint8_t> BytesOf(std::string_view text)
{
	return {reinterpret_cast<const uint8_t*>(text.data()), text.size()};
}

bool ReadString(const Json& object, std::string_view key, std::string& out, bool required = true)
{
	const auto found = object.find(key);
	if (found == object.end() || found->is_null())
	{
		return !required;
	}
	if (!found->is_string())
	{
		return false;
	}
	out = found->get_ref<const std::string&>();
	return true;
}

std::optional<int64_t> ReadTime(const Json& object, std::string_view key)
{
	std::string text;
	if (!ReadString(object, key, text))
	{
		return std::nullopt;
	}
	return ParseIsoTime(text);
}

/// The days from 1970-01-01 to a date of the proleptic Gregorian calendar
constexpr int64_t DaysFromCivil(int64_t year, int64_t month, int64_t day)
{
	year -= month <= 2 ? 1 : 0;
	const auto era = (year >= 0 ? year : year - 399) / 400;
	const auto yearOfEra = year - era * 400;
	const auto dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
	const auto dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
	return era * 146097 + dayOfEra - 719468;
}

std::optional<int64_t> Digits(std::string_view text, size_t at, size_t count)
{
	if (at + count > text.size())
	{
		return std::nullopt;
	}
	int64_t value = 0;
	for (size_t i = at; i < at + count; ++i)
	{
		if (text[i] < '0' || text[i] > '9')
		{
			return std::nullopt;
		}
		value = value * 10 + (text[i] - '0');
	}
	return value;
}

} // namespace

std::optional<std::vector<uint8_t>> DecodeBase64Url(std::string_view text)
{
	if (text.size() % 4 == 1)
	{
		return std::nullopt;
	}
	std::vector<uint8_t> out;
	out.reserve(text.size() * 3 / 4);
	uint32_t bits = 0;
	int32_t count = 0;
	for (const auto letter : text)
	{
		const auto value = ValueOf(letter);
		if (!value.has_value())
		{
			return std::nullopt;
		}
		bits = (bits << 6) | *value;
		count += 6;
		if (count >= 8)
		{
			count -= 8;
			out.push_back(static_cast<uint8_t>((bits >> count) & 0xFFu));
		}
	}
	// A canonical text has no bits set after the last whole byte
	if (count > 0 && (bits & ((1u << count) - 1u)) != 0)
	{
		return std::nullopt;
	}
	return out;
}

std::string EncodeBase64Url(std::span<const uint8_t> bytes)
{
	std::string out;
	out.reserve((bytes.size() * 4 + 2) / 3);
	uint32_t bits = 0;
	int32_t count = 0;
	for (const auto byte : bytes)
	{
		bits = (bits << 8) | byte;
		count += 8;
		while (count >= 6)
		{
			count -= 6;
			out.push_back(k_Alphabet[(bits >> count) & 0x3Fu]);
		}
	}
	if (count > 0)
	{
		out.push_back(k_Alphabet[(bits << (6 - count)) & 0x3Fu]);
	}
	return out;
}

std::vector<uint8_t> PreAuthenticationEncoding(std::span<const std::span<const uint8_t>> pieces)
{
	std::vector<uint8_t> out;
	const auto append = [&out](uint64_t number) {
		number &= 0x7FFF'FFFF'FFFF'FFFFull;
		for (int i = 0; i < 8; ++i)
		{
			out.push_back(static_cast<uint8_t>(number >> (8 * i)));
		}
	};
	append(pieces.size());
	for (const auto& piece : pieces)
	{
		append(piece.size());
		out.insert(out.end(), piece.begin(), piece.end());
	}
	return out;
}

std::vector<uint8_t> SignedBytesOf(std::string_view message, std::string_view footer, std::string_view implicit)
{
	const std::array pieces {BytesOf(k_Header), BytesOf(message), BytesOf(footer), BytesOf(implicit)};
	return PreAuthenticationEncoding(pieces);
}

std::optional<SignedToken> ReadToken(std::string_view token, std::string_view implicit)
{
	if (!token.starts_with(k_Header))
	{
		return std::nullopt;
	}
	token.remove_prefix(k_Header.size());
	std::string_view body = token;
	std::string_view footerText;
	if (const auto dot = token.find('.'); dot != std::string_view::npos)
	{
		body = token.substr(0, dot);
		footerText = token.substr(dot + 1);
		if (footerText.empty() || footerText.find('.') != std::string_view::npos)
		{
			return std::nullopt;
		}
	}
	const auto bytes = DecodeBase64Url(body);
	if (!bytes.has_value() || bytes->size() < k_SignatureBytes)
	{
		return std::nullopt;
	}
	SignedToken out;
	if (!footerText.empty())
	{
		const auto footer = DecodeBase64Url(footerText);
		if (!footer.has_value())
		{
			return std::nullopt;
		}
		out.footer.assign(footer->begin(), footer->end());
	}
	const auto split = bytes->end() - static_cast<std::ptrdiff_t>(k_SignatureBytes);
	out.message.assign(bytes->begin(), split);
	std::copy(split, bytes->end(), out.signature.begin());
	out.signedBytes = SignedBytesOf(out.message, out.footer, implicit);
	return out;
}

std::string MakeToken(std::string_view message, const Signature& signature, std::string_view footer)
{
	std::vector<uint8_t> body(message.begin(), message.end());
	body.insert(body.end(), signature.begin(), signature.end());
	std::string token(k_Header);
	token += EncodeBase64Url(body);
	if (!footer.empty())
	{
		token += '.';
		token += EncodeBase64Url(BytesOf(footer));
	}
	return token;
}

std::optional<std::string> KeyIdOf(std::string_view footer)
{
	const auto value = Json::parse(footer.begin(), footer.end(), nullptr, false);
	if (value.is_discarded() || !value.is_object())
	{
		return std::nullopt;
	}
	std::string id;
	if (!ReadString(value, "kid", id) || id.empty())
	{
		return std::nullopt;
	}
	return id;
}

std::optional<PublicKey> ParsePublicKey(std::string_view text)
{
	if (!text.starts_with(k_PublicKeyPrefix))
	{
		return std::nullopt;
	}
	const auto bytes = DecodeBase64Url(text.substr(k_PublicKeyPrefix.size()));
	if (!bytes.has_value() || bytes->size() != k_PublicKeyBytes)
	{
		return std::nullopt;
	}
	PublicKey key {};
	std::ranges::copy(*bytes, key.begin());
	return key;
}

std::optional<std::vector<TicketKey>> ParseTicketKeys(std::string_view text)
{
	const auto value = Json::parse(text.begin(), text.end(), nullptr, false);
	if (value.is_discarded() || !value.is_object())
	{
		return std::nullopt;
	}
	const auto keys = value.find("keys");
	if (keys == value.end() || !keys->is_array())
	{
		return std::nullopt;
	}
	std::vector<TicketKey> out;
	for (const auto& entry : *keys)
	{
		if (!entry.is_object())
		{
			continue;
		}
		std::string id;
		std::string publicText;
		if (!ReadString(entry, "kid", id) || !ReadString(entry, "public_key", publicText) || id.empty())
		{
			continue;
		}
		if (const auto key = ParsePublicKey(publicText); key.has_value())
		{
			out.push_back(TicketKey {.id = std::move(id), .publicKey = *key});
		}
	}
	return out;
}

std::optional<Claims> ParseClaims(std::string_view message)
{
	const auto value = Json::parse(message.begin(), message.end(), nullptr, false);
	if (value.is_discarded() || !value.is_object())
	{
		return std::nullopt;
	}
	Claims claims;
	std::string role;
	if (!ReadString(value, "iss", claims.issuer) || !ReadString(value, "sub", claims.subject) ||
	    !ReadString(value, "aud", claims.audience) || !ReadString(value, "jti", claims.id) ||
	    !ReadString(value, "name", claims.name, false) || !ReadString(value, "role", role, false))
	{
		return std::nullopt;
	}
	const auto issuedAt = ReadTime(value, "iat");
	const auto notBefore = ReadTime(value, "nbf");
	const auto expires = ReadTime(value, "exp");
	if (!issuedAt.has_value() || !notBefore.has_value() || !expires.has_value())
	{
		return std::nullopt;
	}
	claims.issuedAt = *issuedAt;
	claims.notBefore = *notBefore;
	claims.expires = *expires;
	if (const auto admin = value.find("admin"); admin != value.end() && !admin->is_null())
	{
		if (!admin->is_boolean())
		{
			return std::nullopt;
		}
		claims.admin = admin->get<bool>();
	}
	// The lobby's tickets name the role. A ticket with no role is a god's ticket, as the lobby's default.
	if (!role.empty())
	{
		const auto known = game_socket::RoleFrom(role);
		if (!known.has_value())
		{
			return std::nullopt;
		}
		claims.role = *known;
	}
	return claims;
}

std::optional<int64_t> ParseIsoTime(std::string_view text)
{
	// YYYY-MM-DDTHH:MM:SS, then an optional part of a second, then Z or +HH:MM or -HH:MM
	constexpr size_t k_DateTimeLength = 19;
	if (text.size() < k_DateTimeLength + 1 || text[4] != '-' || text[7] != '-' || (text[10] != 'T' && text[10] != ' ') ||
	    text[13] != ':' || text[16] != ':')
	{
		return std::nullopt;
	}
	const auto year = Digits(text, 0, 4);
	const auto month = Digits(text, 5, 2);
	const auto day = Digits(text, 8, 2);
	const auto hour = Digits(text, 11, 2);
	const auto minute = Digits(text, 14, 2);
	const auto second = Digits(text, 17, 2);
	if (!year || !month || !day || !hour || !minute || !second || *month < 1 || *month > 12 || *day < 1 || *day > 31 ||
	    *hour > 23 || *minute > 59 || *second > 60)
	{
		return std::nullopt;
	}
	size_t at = k_DateTimeLength;
	if (text[at] == '.')
	{
		++at;
		const auto start = at;
		while (at < text.size() && text[at] >= '0' && text[at] <= '9')
		{
			++at;
		}
		if (at == start)
		{
			return std::nullopt;
		}
	}
	int64_t offset = 0;
	const auto rest = text.substr(at);
	if (rest == "Z")
	{
		offset = 0;
	}
	else if (rest.size() == 6 && (rest[0] == '+' || rest[0] == '-') && rest[3] == ':')
	{
		const auto offsetHours = Digits(rest, 1, 2);
		const auto offsetMinutes = Digits(rest, 4, 2);
		if (!offsetHours || !offsetMinutes || *offsetHours > 23 || *offsetMinutes > 59)
		{
			return std::nullopt;
		}
		offset = (*offsetHours * 60 + *offsetMinutes) * 60 * (rest[0] == '-' ? -1 : 1);
	}
	else
	{
		return std::nullopt;
	}
	const auto days = DaysFromCivil(*year, *month, *day);
	return days * 86400 + *hour * 3600 + *minute * 60 + *second - offset;
}

} // namespace openblack::join_ticket
