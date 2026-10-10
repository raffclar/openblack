/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <vector>

#include "Lexer.h"

namespace openblack::lhscriptx
{

class Script
{
public:
	Script();

	/// Carries out the commands of a map script, calling `afterCommand`, when given, after each one
	void Load(const std::string& source, const std::function<void()>& afterCommand = {});

private:
	[[nodiscard]] bool IsCommand(const std::string& identifier) const;
	void RunCommand(const std::string& identifier, const std::vector<Token>& args);

	const Token* PeekToken(Lexer&);
	const Token* AdvanceToken(Lexer&);

	// The current token.
	Token _token {Token::MakeInvalidToken()};
};

} // namespace openblack::lhscriptx
