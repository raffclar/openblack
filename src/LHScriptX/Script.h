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

#include <glm/vec3.hpp>

#include "Lexer.h"

namespace openblack::lhscriptx
{

class Script
{
public:
	Script();
	/// A script whose places are all moved by an offset across the land (x, z) and raised by its height (y), as the
	/// lines of the crossing file are read about the place the vortex throws things out
	explicit Script(glm::vec3 origin);

	/// Carries out the commands of a map script, calling `afterCommand`, when given, after each one
	void Load(const std::string& source, const std::function<void()>& afterCommand = {});

private:
	[[nodiscard]] bool IsCommand(const std::string& identifier) const;
	void RunCommand(const std::string& identifier, const std::vector<Token>& args);

	const Token* PeekToken(Lexer&);
	const Token* AdvanceToken(Lexer&);

	/// What every place the script gives is moved by
	glm::vec3 _origin {0.0f};

	// The current token.
	Token _token {Token::MakeInvalidToken()};
};

} // namespace openblack::lhscriptx
