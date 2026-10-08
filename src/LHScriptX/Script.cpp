/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Script.h"

#include <algorithm>
#include <array>
#include <ranges>

#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "ECS/Town/TownVillagers.h"
#include "FeatureScriptCommands.h"
#include "Lexer.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::lhscriptx;

namespace
{
/// What this module keeps between calls (Locator::scriptState)
struct ScriptRunState
{
	/// The 12 integer slots (reading a line stops at 12)
	std::array<int32_t, 12> intSlots {};
	/// The map features' position offset, last created object and tribe override (Script.h)
	std::optional<map_coords::MapCoords> positionOffset {};
	entt::entity lastCreated {entt::null};
	std::optional<Tribe> tribeOverride {};
};

ScriptRunState& RunState()
{
	return openblack::Locator::scriptState::value().Get<ScriptRunState>();
}
} // namespace

int32_t Script::IntSlot(size_t index)
{
	auto& state = RunState();
	return index < state.intSlots.size() ? state.intSlots.at(index) : 0;
}

void Script::BeginMapFeatures()
{
	auto& state = RunState();
	// (not ported) the player override: no command reads it yet
	state.positionOffset.reset();
	state.tribeOverride.reset();
	// The VERSION float (CREATE_FLOCK tests it against 2.1) back to 0, so a land without a VERSION line does not keep
	// the last land's
	if (Locator::mapScriptSystem::has_value())
	{
		Locator::mapScriptSystem::value().Globals().version = 0.0f;
	}
	state.lastCreated = entt::null;
	// (inferred) the vagrants start empty on each map: the original clears them when the game is made and each deleted
	// villager leaves them; openblack's Registry::Reset deletes no one one by one, so the list would keep dead ids
	// (CREATE_VILLAGER / _POS write it)
	ecs::town_villagers::ClearVagrants();
}

void Script::SetPositionOffset(std::optional<map_coords::MapCoords> offset)
{
	RunState().positionOffset = offset;
}

std::optional<map_coords::MapCoords> Script::PositionOffset()
{
	return RunState().positionOffset;
}

entt::entity Script::LastCreated()
{
	return RunState().lastCreated;
}

void Script::SetLastCreated(entt::entity entity)
{
	RunState().lastCreated = entity;
}

void Script::SetTribeOverride(std::optional<Tribe> tribe)
{
	RunState().tribeOverride = tribe;
}

std::optional<Tribe> Script::TribeOverride()
{
	return RunState().tribeOverride;
}

Script::Script() = default;

void Script::Load(const std::string& source)
{
	Lexer lexer(source);

	const Token* token = this->PeekToken(lexer);
	while (!token->IsEOF())
	{
		token = this->PeekToken(lexer);

		if (token->IsIdentifier())
		{
			const std::string identifier = token->Identifier();
			const int line = lexer.GetLine();
			try
			{

				if (!IsCommand(identifier))
				{
					throw ScriptError("unknown command: " + identifier);
				}

				token = this->AdvanceToken(lexer);
				if (!token->IsOP(Operator::LeftParentheses))
				{
					throw ScriptError("expected ( after identifier " + identifier);
				}

				std::vector<Token> args;

				// if it's an immediate right parentheses there are no args
				token = this->AdvanceToken(lexer);
				if (!token->IsOP(Operator::RightParentheses))
				{
					while (true)
					{
						const Token* peekToken = this->PeekToken(lexer);
						args.push_back(*peekToken);

						// consume the ,
						token = this->AdvanceToken(lexer);
						if (!token->IsOP(Operator::Comma))
						{
							break;
						}

						this->AdvanceToken(lexer);
					}
				}

				if (!token->IsOP(Operator::RightParentheses))
				{
					throw ScriptError("missing )");
				}

				// move token to whatever is after ')'
				this->AdvanceToken(lexer);

				RunCommand(identifier, args);
			}
			catch (const std::runtime_error& e)
			{
				// only the reader's own errors; anything else (no landscape loaded...) still stops the script
				if (dynamic_cast<const ScriptError*>(&e) == nullptr && dynamic_cast<const LexerException*>(&e) == nullptr)
				{
					throw;
				}
				// The original skips what it cannot read; a bad line (typos in the playground scripts: a doubled quote,
				// an empty argument, a stray word) must not abort the whole map
				SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "LHScriptX: line {}: {} ({}); line skipped", line, e.what(),
				                   identifier);
				while (!_token.IsEOL() && !_token.IsEOF())
				{
					try
					{
						this->AdvanceToken(lexer);
					}
					catch (const std::exception&)
					{
					}
				}
			}
		}
		else if (!token->IsEOL() && !token->IsEOF())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "LHScriptX: line {}: unexpected text; line skipped", lexer.GetLine());
			while (!_token.IsEOL() && !_token.IsEOF())
			{
				try
				{
					this->AdvanceToken(lexer);
				}
				catch (const std::exception&)
				{
				}
			}
			continue;
		}

		this->AdvanceToken(lexer);
	}
}

bool Script::IsCommand(const std::string& identifier) const
{
	// TODO(handsomematt): this could be done a lot better
	return std::any_of(FeatureScriptCommands::k_Signatures.cbegin(), FeatureScriptCommands::k_Signatures.cend(),
	                   [&identifier](const auto& s) { return s.name.data() == identifier; });
}

ScriptCommandParameter GetParameter(Token& argument)
{
	const auto type = argument.GetType();

	switch (type)
	{
	case Token::Type::Invalid:
		throw ScriptError("Invalid token. Unable to proceed");
	case Token::Type::EndOfFile:
		throw ScriptError("Unexpected EOF in script");
	case Token::Type::EndOfLine:
		throw ScriptError("Unexpected EOL in script");
	case Token::Type::Identifier:
		return ScriptCommandParameter(argument.Identifier());
	case Token::Type::String:
	{
		const auto& str = argument.StringValue();
		// Check if it's a vector
		if (std::count_if(str.cbegin(), str.cend(), [](char c) { return c == ','; }) == 1)
		{
			const auto delim = str.find(',');
			char* floatEnd;
			auto x = std::strtof(str.c_str(), &floatEnd);
			if (str.c_str() + delim == floatEnd)
			{
				auto z = std::strtof(floatEnd + 1, &floatEnd);
				if (static_cast<size_t>(floatEnd - str.c_str()) == static_cast<size_t>(str.length()))
				{
					// MapCoords of the text, + the position offset when set (as integers); back to metres for
					// openblack's position
					if (const auto offset = Script::PositionOffset(); offset.has_value())
					{
						auto at = map_coords::MapCoords {map_coords::ToFixed(x), map_coords::ToFixed(z)};
						at += *offset;
						x = map_coords::ToMetres(at.x);
						z = map_coords::ToMetres(at.z);
					}
					const auto& island = Locator::terrainSystem::value();

					return {x, island.GetHeightAt(glm::vec2(x, z)), z};
				}
			}
		}
		return ScriptCommandParameter(str);
	}
	case Token::Type::Integer:
		return ScriptCommandParameter(*argument.IntegerValue());
	case Token::Type::Float:
		return ScriptCommandParameter(*argument.FloatValue());
	case Token::Type::Operator:
		throw ScriptError("Operator token as an argument is currently not supported");
	default:
		throw std::runtime_error("Missing switch case for script token argument");
	}
}

void Script::RunCommand(const std::string& identifier, const std::vector<Token>& args)
{
	auto& state = RunState();
	const ScriptCommandSignature* commandSignature = nullptr;

	for (const auto& signature : FeatureScriptCommands::k_Signatures)
	{
		if (signature.name.data() != identifier)
		{
			continue;
		}

		commandSignature = &signature;
		break;
	}

	if (commandSignature == nullptr)
	{
		throw ScriptError("Missing script command signature");
	}

	// Turn tokens into parameters
	auto parameters = ScriptCommandParameters();

	for (auto arg : args)
	{
		const ScriptCommandParameter param = GetParameter(arg);
		parameters.push_back(param);
	}

	const auto expectedParameters = commandSignature->parameters;
	uint32_t expectedSize;
	// TODO (#749) use std::views::enumerate
	for (expectedSize = 0; const auto& p : commandSignature->parameters)
	{
		// Looping until None because parameters is a fixed sized array.
		// Last Argument is the one before the first None or the 9th
		if (p == ParameterType::None)
		{
			break;
		}
		++expectedSize;
	}

	// Validate the number of given arguments against what is expected
	if (parameters.size() != expectedSize)
	{
		throw ScriptError("Invalid number of script arguments");
	}

	// Validate the typing of the given arguments against what is expected. A number is a number to the original: an
	// integer where a float is expected (a scale written "1") and a float where an integer is expected are converted.
	for (auto&& [param, expected] : std::views::zip(parameters, expectedParameters))
	{
		if (param.GetType() == ParameterType::Number && expected == ParameterType::Float)
		{
			param = ScriptCommandParameter(static_cast<float>(param.GetNumber()));
		}
		else if (param.GetType() == ParameterType::Float && expected == ParameterType::Number)
		{
			param = ScriptCommandParameter(static_cast<int32_t>(param.GetFloat()));
		}
		if (param.GetType() != expected)
		{
			throw ScriptError("Invalid script argument type");
		}
	}

	// An 'N' argument also goes to its integer slot (atol); the slot of any other type keeps what an earlier command
	// left there. (approximate) openblack's signatures stand for the original's type strings (CREATE_TOWN "NALNL",
	// CREATE_ABODE "NALNNNN", CREATE_VILLAGER_POS "AALN")
	for (size_t slot = 0; slot < parameters.size() && slot < state.intSlots.size(); ++slot)
	{
		const auto& param = parameters.at(slot);
		if (param.GetType() == ParameterType::Number)
		{
			state.intSlots.at(slot) = param.GetNumber();
		}
	}

	commandSignature->command(parameters);
}

const Token* Script::PeekToken(Lexer& lexer)
{
	if (_token.IsInvalid())
	{
		_token = lexer.GetToken();
	}
	return &_token;
}

const Token* Script::AdvanceToken(Lexer& lexer)
{
	_token = lexer.GetToken();
	return &_token;
}
