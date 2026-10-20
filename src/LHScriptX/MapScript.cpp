/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MapScript.h"

#include "Lexer.h"

namespace openblack::lhscriptx::map_script
{

std::vector<std::string> FeatureScripts(const std::string& source)
{
	std::vector<std::string> paths;
	Lexer lexer(source);
	// The last three tokens: the command name, the "(" and the path
	Token name = Token::MakeInvalidToken();
	Token open = Token::MakeInvalidToken();
	for (Token token = lexer.GetToken(); !token.IsEOF(); token = lexer.GetToken())
	{
		if (token.IsString() && open.IsOP(Operator::LeftParentheses) && name.IsIdentifier() &&
		    name.Identifier() == k_LoadFeatureScript)
		{
			paths.push_back(token.StringValue());
		}
		name = open;
		open = token;
	}
	return paths;
}

} // namespace openblack::lhscriptx::map_script
