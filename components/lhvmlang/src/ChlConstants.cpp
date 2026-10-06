/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ChlConstants.h"

#include <cctype>
#include <charconv>

#include <algorithm>

namespace openblack::lhvm::chl
{

namespace
{

/// Tokens of C source with comments removed: identifiers, numbers and single punctuation characters
std::vector<std::string_view> Tokenize(std::string_view text)
{
	std::vector<std::string_view> tokens;
	size_t i = 0;
	while (i < text.size())
	{
		const char c = text[i];
		if (std::isspace(static_cast<unsigned char>(c)) != 0)
		{
			++i;
		}
		else if (text.substr(i, 2) == "//")
		{
			const auto end = text.find('\n', i);
			i = end == std::string_view::npos ? text.size() : end;
		}
		else if (text.substr(i, 2) == "/*")
		{
			const auto end = text.find("*/", i + 2);
			i = end == std::string_view::npos ? text.size() : end + 2;
		}
		else if (std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_')
		{
			const auto start = i;
			while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) != 0 || text[i] == '_'))
			{
				++i;
			}
			tokens.push_back(text.substr(start, i - start));
		}
		else if (c == '#')
		{
			// Preprocessor directive name, kept with its '#'
			const auto start = i++;
			while (i < text.size() && std::isalpha(static_cast<unsigned char>(text[i])) != 0)
			{
				++i;
			}
			tokens.push_back(text.substr(start, i - start));
		}
		else
		{
			tokens.push_back(text.substr(i, 1));
			++i;
		}
	}
	return tokens;
}

std::optional<int32_t> ParseInteger(std::string_view token)
{
	int base = 10;
	if (token.size() > 2 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X'))
	{
		token.remove_prefix(2);
		base = 16;
	}
	while (!token.empty() && (token.back() == 'L' || token.back() == 'l' || token.back() == 'U' || token.back() == 'u'))
	{
		token.remove_suffix(1);
	}
	int64_t value = 0;
	const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value, base);
	if (ec != std::errc() || ptr != token.data() + token.size())
	{
		return std::nullopt;
	}
	return static_cast<int32_t>(value);
}

} // namespace

void ConstantTable::Add(std::string_view enumName, std::string_view member, int32_t value)
{
	_enums[std::string(enumName)][value].emplace_back(member);
	_values.emplace(std::string(member), value);
	const auto prefix = std::string(enumName) + "_";
	if (member.size() > prefix.size() && member.starts_with(prefix))
	{
		_values.emplace(std::string(member.substr(prefix.size())), value);
	}
}

size_t ConstantTable::LoadHeader(std::string_view text)
{
	const auto tokens = Tokenize(text);
	size_t added = 0;
	// The value of an expression of the form [-] (number | constant) [(+|-) number]
	const auto evaluate = [this, &tokens](size_t& i, size_t end) -> std::optional<int32_t> {
		int32_t sign = 1;
		if (i < end && tokens[i] == "-")
		{
			sign = -1;
			++i;
		}
		if (i >= end)
		{
			return std::nullopt;
		}
		auto value = ParseInteger(tokens[i]);
		if (!value)
		{
			if (const auto it = _values.find(tokens[i]); it != _values.end())
			{
				value = it->second;
			}
		}
		++i;
		if (!value)
		{
			return std::nullopt;
		}
		auto result = sign * *value;
		if (i + 1 < end && (tokens[i] == "+" || tokens[i] == "-"))
		{
			if (const auto offset = ParseInteger(tokens[i + 1]))
			{
				result += tokens[i] == "+" ? *offset : -*offset;
				i += 2;
			}
		}
		return result;
	};

	for (size_t i = 0; i < tokens.size(); ++i)
	{
		if (tokens[i] == "#define" && i + 2 < tokens.size())
		{
			size_t j = i + 2;
			if (const auto value = evaluate(j, std::min(tokens.size(), i + 5)))
			{
				_values.emplace(std::string(tokens[i + 1]), *value);
				++added;
			}
			continue;
		}
		if (tokens[i] != "enum" || i + 2 >= tokens.size())
		{
			continue;
		}
		size_t j = i + 1;
		std::string_view enumName;
		if (tokens[j] != "{")
		{
			enumName = tokens[j++];
		}
		if (j >= tokens.size() || tokens[j] != "{")
		{
			continue;
		}
		++j;
		int32_t next = 0;
		while (j < tokens.size() && tokens[j] != "}")
		{
			const auto member = tokens[j++];
			if (j < tokens.size() && tokens[j] == "=")
			{
				++j;
				size_t end = j;
				while (end < tokens.size() && tokens[end] != "," && tokens[end] != "}")
				{
					++end;
				}
				if (const auto value = evaluate(j, end))
				{
					next = *value;
				}
				j = end;
			}
			Add(enumName, member, next);
			++added;
			++next;
			if (j < tokens.size() && tokens[j] == ",")
			{
				++j;
			}
		}
		i = j;
	}
	return added;
}

std::optional<std::string> ConstantTable::NameOf(std::string_view enumName, int32_t value) const
{
	const auto it = _enums.find(enumName);
	if (it == _enums.end())
	{
		return std::nullopt;
	}
	const auto member = it->second.find(value);
	if (member == it->second.end() || member->second.empty())
	{
		return std::nullopt;
	}
	const auto& name = member->second.front();
	const auto prefix = std::string(enumName) + "_";
	if (name.size() > prefix.size() && name.starts_with(prefix))
	{
		auto stripped = name.substr(prefix.size());
		// Only drop the prefix when the short name means this constant
		if (const auto found = ValueOf(stripped); found && *found == value)
		{
			return stripped;
		}
	}
	return name;
}

std::optional<int32_t> ConstantTable::ValueOf(std::string_view name) const
{
	const auto it = _values.find(name);
	if (it == _values.end())
	{
		return std::nullopt;
	}
	return it->second;
}

} // namespace openblack::lhvm::chl
