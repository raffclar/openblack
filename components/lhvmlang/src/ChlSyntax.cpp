/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ChlSyntax.h"

#include <algorithm>
#include <format>

namespace openblack::lhvm::chl
{

namespace
{

constexpr std::array<std::string_view, 7> k_ScriptKindKeywords = {
    "script",
    "help script",
    "challenge help script",
    "temple help script",
    "temple special script",
    "multiplayer script",
    "multiplayer help script",
};

} // namespace

const OperatorInfo& GetOperator(Op op)
{
	const auto it = std::ranges::find(k_Operators, op, &OperatorInfo::op);
	return it != k_Operators.end() ? *it : k_Operators.front();
}

std::optional<Op> FindOperator(std::string_view spelling, bool unary)
{
	for (const auto& info : k_Operators)
	{
		if (info.spelling == spelling && info.unary == unary)
		{
			return info.op;
		}
	}
	return std::nullopt;
}

std::string_view ScriptKindKeyword(ScriptKind kind)
{
	const auto index = static_cast<size_t>(kind);
	return index < k_ScriptKindKeywords.size() ? k_ScriptKindKeywords[index] : k_ScriptKindKeywords.front();
}

std::optional<ScriptKind> ParseScriptKind(std::string_view keywords)
{
	// "quest help" is another spelling of "challenge help"
	if (keywords == "quest help script")
	{
		return ScriptKind::ChallengeHelpScript;
	}
	for (size_t i = 0; i < k_ScriptKindKeywords.size(); ++i)
	{
		if (k_ScriptKindKeywords[i] == keywords)
		{
			return static_cast<ScriptKind>(i);
		}
	}
	return std::nullopt;
}

std::string FormatNumber(float value)
{
	return std::format("{}", value);
}

} // namespace openblack::lhvm::chl
