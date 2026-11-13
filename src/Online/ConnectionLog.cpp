/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ConnectionLog.h"

#include <algorithm>
#include <array>
#include <map>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace openblack::online
{
namespace
{

/// The mark that takes the place of a secret
constexpr std::string_view k_Hidden = "<hidden>";

/// The words in the name of a field that holds a secret
constexpr std::array<std::string_view, 5> k_SecretNames {"ticket", "password", "token", "session", "secret"};

[[nodiscard]] bool IsTokenCharacter(char c) noexcept
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
	       c == '=' || c == '+' || c == '/';
}

[[nodiscard]] bool IsWordCharacter(char c) noexcept
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

[[nodiscard]] char Lower(char c) noexcept
{
	return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

/// True when the name, without letter case, has a word of a secret
[[nodiscard]] bool IsSecretName(std::string_view name) noexcept
{
	std::string lower(name.size(), '\0');
	std::ranges::transform(name, lower.begin(), Lower);
	return std::ranges::any_of(k_SecretNames,
	                           [&lower](std::string_view word) { return lower.find(word) != std::string::npos; });
}

/// Replaces each run of token characters that starts with the prefix
void MaskPrefixed(std::string& text, std::string_view prefix)
{
	size_t at = 0;
	while ((at = text.find(prefix, at)) != std::string::npos)
	{
		size_t end = at + prefix.size();
		while (end < text.size() && IsTokenCharacter(text[end]))
		{
			++end;
		}
		text.replace(at, end - at, k_Hidden);
		at += k_Hidden.size();
	}
}

/// Replaces the value after each name of a secret: name=value, name: value, "name":"value"
void MaskNamedValues(std::string& text)
{
	size_t at = 0;
	while (at < text.size())
	{
		// Find the next word of letters, digits and "_"
		while (at < text.size() && !IsWordCharacter(text[at]))
		{
			++at;
		}
		const size_t wordStart = at;
		while (at < text.size() && IsWordCharacter(text[at]))
		{
			++at;
		}
		if (at == wordStart)
		{
			break;
		}
		const std::string_view word(text.data() + wordStart, at - wordStart);
		if (!IsSecretName(word))
		{
			continue;
		}
		// After the name: an optional quote, then "=" or ":", spaces and an optional quote
		size_t value = at;
		if (value < text.size() && text[value] == '"')
		{
			++value;
		}
		if (value >= text.size() || (text[value] != '=' && text[value] != ':'))
		{
			continue;
		}
		++value;
		while (value < text.size() && text[value] == ' ')
		{
			++value;
		}
		const bool quoted = value < text.size() && text[value] == '"';
		if (quoted)
		{
			++value;
		}
		size_t end = value;
		while (end < text.size() && (quoted ? text[end] != '"' : IsTokenCharacter(text[end])))
		{
			++end;
		}
		if (end == value)
		{
			at = end;
			continue;
		}
		text.replace(value, end - value, k_Hidden);
		at = value + k_Hidden.size();
	}
}

} // namespace

std::string MaskSecrets(std::string_view text)
{
	std::string masked(text);
	// The tickets of the lobby are PASETO tokens
	MaskPrefixed(masked, "v4.public.");
	MaskPrefixed(masked, "v4.local.");
	MaskPrefixed(masked, "Bearer ");
	MaskNamedValues(masked);
	return masked;
}

std::string_view CloseCodeMeaning(uint16_t code) noexcept
{
	switch (code)
	{
	case 1000:
		return "normal close";
	case 1001:
		return "going away";
	case 1002:
		return "protocol error";
	case 1006:
		return "no close frame";
	case 1009:
		return "message too large";
	case 1011:
		return "server error";
	case 4003:
		return "refused";
	case 4010:
		return "out of sync";
	default:
		return {};
	}
}

std::string DescribeClosed(const WebSocketClosed& closed)
{
	const auto codeText = [&closed]() {
		const auto meaning = CloseCodeMeaning(closed.code);
		return meaning.empty() ? fmt::format("code {}", closed.code) : fmt::format("code {} ({})", closed.code, meaning);
	};
	switch (closed.end)
	{
	case WebSocketEnd::ClosedByServer:
		return fmt::format("the server closed it with {}", codeText());
	case WebSocketEnd::HandshakeRefused:
		return closed.httpStatus != 0 ? fmt::format("the server refused the handshake with HTTP status {}", closed.httpStatus)
		                              : std::string("the server refused the handshake");
	case WebSocketEnd::AddressRefused:
		return "the client refused the address";
	case WebSocketEnd::Network:
		return "the network or the browser ended it with no close frame";
	case WebSocketEnd::Tls:
		return "the TLS connection failed";
	case WebSocketEnd::Timeout:
		return "the client ended it: no data from the server in time";
	case WebSocketEnd::TooLarge:
		return "the client ended it: a message from the server was too large";
	}
	return "unknown";
}

std::vector<PartDifference> DifferentParts(std::span<const std::pair<std::string, uint64_t>> local,
                                           std::span<const std::pair<std::string, uint64_t>> other)
{
	std::map<std::string_view, std::pair<uint64_t, uint64_t>> values;
	std::vector<std::string_view> order;
	for (const auto& [name, value] : local)
	{
		if (values.try_emplace(name, value, 0).second)
		{
			order.push_back(name);
		}
	}
	for (const auto& [name, value] : other)
	{
		if (const auto [found, added] = values.try_emplace(name, 0, value); added)
		{
			order.push_back(name);
		}
		else
		{
			found->second.second = value;
		}
	}
	std::vector<PartDifference> differences;
	for (const auto name : order)
	{
		const auto& [mine, theirs] = values.at(name);
		if (mine != theirs)
		{
			differences.push_back(PartDifference {.name = std::string(name), .local = mine, .other = theirs});
		}
	}
	return differences;
}

std::string DescribeParts(std::span<const PartDifference> parts, std::string_view localLabel, std::string_view otherLabel,
                          size_t most)
{
	if (parts.empty())
	{
		return "no part differs";
	}
	std::string text = fmt::format("{} parts differ:", parts.size());
	for (size_t i = 0; i < std::min(most, parts.size()); ++i)
	{
		text +=
		    fmt::format(" {} ({} {:016x}, {} {:016x})", parts[i].name, localLabel, parts[i].local, otherLabel, parts[i].other);
	}
	if (parts.size() > most)
	{
		text += fmt::format(" and {} more", parts.size() - most);
	}
	return text;
}

ConnectionLog::ConnectionLog(Clock clock, Sink sink)
    : _clock(std::move(clock))
    , _sink(std::move(sink))
    , _start(_clock ? _clock() : 0)
{
}

int64_t ConnectionLog::Elapsed() const
{
	return _clock ? _clock() - _start : 0;
}

void ConnectionLog::Write(spdlog::level::level_enum level, std::string_view text)
{
	const auto masked = MaskSecrets(text);
	if (_sink)
	{
		_sink(level, masked);
	}
	else if (const auto logger = spdlog::get("game"); logger != nullptr)
	{
		logger->log(level, "{}{}", k_NetTag, masked);
	}
	const auto milliseconds = Elapsed() / 1000;
	_lines.push_back(fmt::format("{:>8}.{:03} {}{}", milliseconds / 1000, milliseconds % 1000,
	                             level >= spdlog::level::warn ? "! " : "", masked));
	while (_lines.size() > k_MostLines)
	{
		_lines.pop_front();
	}
}

std::string ConnectionLog::Report(std::span<const std::string> header, size_t lines) const
{
	std::string text;
	for (const auto& line : header)
	{
		text += MaskSecrets(line);
		text += '\n';
	}
	const auto count = std::min(lines, _lines.size());
	text += fmt::format("The last {} lines of the connection log (seconds since the log started):\n", count);
	for (auto line = _lines.end() - static_cast<std::ptrdiff_t>(count); line != _lines.end(); ++line)
	{
		text += *line;
		text += '\n';
	}
	return text;
}

} // namespace openblack::online
