/* OpenBlack - A reimplementation of Lionhead's Black & White.
 *
 * OpenBlack is the legal property of its developers, whose names
 * can be found in the AUTHORS.md file distributed with this source
 * distribution.
 *
 * OpenBlack is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * OpenBlack is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenBlack. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <Common/File.h>
#include <Game.h>
#include <LHScriptX/Lexer.h>
#include <vector>

using namespace OpenBlack;

namespace OpenBlack::LHScriptX
{

class NotImplemented: public std::logic_error
{
  public:
	NotImplemented():
	    std::logic_error("Function not yet implemented") {};
};

class Script
{
  public:
	Script(Game* game):
	    _game(game),
	    token_(Token::MakeInvalidToken()) {}

	void LoadFromFile(File& file);
	//void SetCommands(std::unique_ptr<ScriptCommands> &commands) { _commands = std::move(commands); }
	//ScriptCommands &GetCommands() const { return *_commands; }
  private:
	//std::unique_ptr<ScriptCommands> _commands;
	//void processCommand(const std::string& command, std::vector<std::string> parameters);

	const bool isCommand(const std::string& identifier) const;
	void runCommand(const std::string& identifier, const std::vector<Token>& args);

	const Token* peekToken();
	const Token* advanceToken();

	// Game instance
	Game* _game;
	// The lexer output we are parsing
	Lexer* lexer_;
	// The current token.
	Token token_;
};

} // namespace OpenBlack::LHScriptX
