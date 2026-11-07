/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Script.h"

#include <cstdlib>

#include <algorithm>
#include <array>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <entt/signal/sigh.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FixedOutlineSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "FeatureScriptCommands.h"
#include "Lexer.h"
#include "Locator.h"
#include "ScriptPlaceText.h"

using namespace openblack;
using namespace openblack::lhscriptx;

Script::Script() = default;

Script::Script(glm::vec3 origin, const map_coords::MapCoords& originOnMap)
    : _origin(origin)
    , _originOnMap(originOnMap)
{
}

namespace
{
/// Reads the tokens of a text one at a time, and keeps the token that it read last
class TokenReader
{
public:
	explicit TokenReader(const std::string& source)
	    : _lexer(source)
	{
	}

	/// The token that the reader is on
	const Token* Peek()
	{
		if (_token.IsInvalid())
		{
			_token = _lexer.GetToken();
		}
		return &_token;
	}

	/// Moves the reader to the next token
	const Token* Advance()
	{
		_token = _lexer.GetToken();
		return &_token;
	}

	/// The line that the token of the reader is on
	[[nodiscard]] int Line() const noexcept { return _lexer.Line(); }

private:
	Lexer _lexer;
	Token _token {Token::MakeInvalidToken()};
};
} // namespace

void Script::ForEachCommand(const std::string& source, const std::function<void(Command&&)>& onCommand)
{
	TokenReader reader(source);

	const Token* token = reader.Peek();
	while (!token->IsEOF())
	{
		token = reader.Peek();

		if (token->IsIdentifier())
		{
			Command command {.name = token->Identifier(), .arguments = {}, .line = reader.Line()};

			if (!IsCommand(command.name))
			{
				// A word the game doesn't know is passed over, as is everything after it up to the next command it
				// knows, so a misspelt line is ignored and the rest of the land still loads
				if (const auto logger = spdlog::get("scripting"); logger != nullptr)
				{
					SPDLOG_LOGGER_WARN(logger, "LHScriptX: skipping unknown command {}", command.name);
				}
				reader.Advance();
				continue;
			}

			token = reader.Advance();
			if (!token->IsOP(Operator::LeftParentheses))
			{
				throw std::runtime_error("expected ( after identifier " + command.name);
			}

			// if it's an immediate right parentheses there are no args
			token = reader.Advance();
			if (!token->IsOP(Operator::RightParentheses))
			{
				while (true)
				{
					command.arguments.push_back(*reader.Peek());

					// consume the ,
					token = reader.Advance();
					if (!token->IsOP(Operator::Comma))
					{
						break;
					}

					reader.Advance();
				}
			}

			// A command ends at its ')' or, when that was left out, at the end of its line
			const bool closedByLineEnd = token->IsEOL() || token->IsEOF();
			if (!closedByLineEnd && !token->IsOP(Operator::RightParentheses))
			{
				throw std::runtime_error("missing )");
			}

			// move token to whatever is after ')'
			if (!closedByLineEnd)
			{
				reader.Advance();
			}

			onCommand(std::move(command));
		}

		token = reader.Advance();
	}
}

void Script::Load(const std::string& source, const std::function<void()>& afterCommand)
{
	ForEachCommand(source, [this, &afterCommand](Command&& command) {
		RunCommand(command.name, command.arguments);
		if (afterCommand)
		{
			afterCommand();
		}
	});
}

ReadCommands Script::Read(const std::string& source)
{
	ReadCommands read;
	try
	{
		ForEachCommand(source, [&read](Command&& command) { read.commands.push_back(std::move(command)); });
	}
	catch (const std::exception& e)
	{
		read.error = e.what();
	}
	return read;
}

bool lhscriptx::LoadsLandscape(const ReadCommands& read)
{
	return std::ranges::any_of(read.commands, [](const Command& command) { return command.name == "LOAD_LANDSCAPE"; });
}

void Script::Run(const Command& command)
{
	if (!IsCommand(command.name))
	{
		throw std::runtime_error("unknown command " + command.name);
	}
	RunCommand(command.name, command.arguments);
}

bool Script::IsCommand(const std::string& identifier)
{
	// TODO(handsomematt): this could be done a lot better
	return std::any_of(FeatureScriptCommands::k_Signatures.cbegin(), FeatureScriptCommands::k_Signatures.cend(),
	                   [&identifier](const auto& s) { return s.name.data() == identifier; });
}

namespace
{
ScriptCommandParameter GetParameter(Token& argument, glm::vec3 origin, const map_coords::MapCoords& originOnMap)
{
	const auto type = argument.GetType();

	switch (type)
	{
	case Token::Type::Invalid:
		throw std::runtime_error("Invalid token. Unable to proceed");
	case Token::Type::EndOfFile:
		throw std::runtime_error("Unexpected EOF in script");
	case Token::Type::EndOfLine:
		throw std::runtime_error("Unexpected EOL in script");
	case Token::Type::Identifier:
		return ScriptCommandParameter(argument.Identifier());
	case Token::Type::String:
	{
		const auto& str = argument.StringValue();
		if (const auto read = ReadScriptPlace(str); read.has_value())
		{
			const float x = read->metres.x + origin.x;
			const float z = read->metres.y + origin.z;
			// The game keeps a script place as a place on the map, and finds the height of the land there only when it
			// uses the place. A line before the line that loads the landscape has no land under it yet. The place is
			// above the land by the height of the origin plus the height that the text gives.
			const auto& island = Locator::terrainSystem::value();
			const float ground = island.IsLoaded() ? island.GetHeightAt(glm::vec2(x, z)) : 0.0f;
			ScriptCommandParameter place {x, ground + (origin.y + read->onMap.altitude), z};
			// The game moves the exact map position by the map position of the origin, and adds the two heights
			place.SetMapPlace(read->onMap + originOnMap);
			return place;
		}
		return ScriptCommandParameter(str);
	}
	case Token::Type::Integer:
		return ScriptCommandParameter(*argument.IntegerValue());
	case Token::Type::Float:
		return ScriptCommandParameter(*argument.FloatValue());
	case Token::Type::Operator:
		throw std::runtime_error("Operator token as an argument is currently not supported");
	default:
		throw std::runtime_error("Missing switch case for script token argument");
	}
}
} // namespace

namespace
{
/// Keeps every thing made while a command runs
struct ThingsMade
{
	std::vector<entt::entity> things;

	void Made(entt::registry&, entt::entity made) { things.push_back(made); }
};

/// Runs a command. Each thing that it makes at one of its places keeps the exact map position that the game reads from
/// that place, because the game makes the thing at that map position.
template <typename Run>
void RunAndKeepMapPlaces(const ScriptCommandParameters& parameters, Run&& run)
{
	const bool hasPlace = std::ranges::any_of(parameters, [](const auto& p) { return p.GetMapPlace().has_value(); });
	if (!hasPlace || !Locator::entitiesRegistry::has_value())
	{
		run();
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	ThingsMade made;
	{
		const entt::scoped_connection connection {
		    registry.Underlying().on_construct<ecs::components::Transform>().connect<&ThingsMade::Made>(made)};
		run();
	}
	for (const auto thing : made.things)
	{
		const auto* at = registry.Valid(thing) ? registry.TryGet<const ecs::components::Transform>(thing) : nullptr;
		if (at == nullptr)
		{
			continue;
		}
		// The first place of the command that the thing stands at
		const auto place = std::ranges::find_if(parameters, [at](const ScriptCommandParameter& p) {
			if (!p.GetMapPlace().has_value())
			{
				return false;
			}
			float x = 0.0f;
			float y = 0.0f;
			float z = 0.0f;
			p.GetVector(x, y, z);
			return at->position.x == x && at->position.z == z;
		});
		if (place != parameters.end())
		{
			ecs::thing_map_place::Keep(registry, thing, *place->GetMapPlace());
		}
	}
}

/// The commands that make their thing only at a place free of the fixed things, or on water
constexpr std::array<std::string_view, 4> k_PlaceTestedCommands {"CREATE_TREE", "CREATE_NEW_TREE", "CREATE_POT",
                                                                 "CREATE_MOBILEOBJECT"};

/// Whether the game refuses to run a command because the command's place is not free of the fixed things. The test
/// comes before anything else that the command does, so a refused command makes nothing and keeps the last thing made.
bool PlaceRefused(std::string_view identifier, const ScriptCommandParameters& parameters)
{
	if (std::ranges::find(k_PlaceTestedCommands, identifier) == k_PlaceTestedCommands.end() ||
	    !Locator::fixedOutlineSystem::has_value())
	{
		return false;
	}
	const auto place = std::ranges::find_if(parameters, [](const auto& p) { return p.GetMapPlace().has_value(); });
	return place != parameters.end() && !Locator::fixedOutlineSystem::value().IsOkToCreateAt(*place->GetMapPlace());
}
} // namespace

void Script::RunCommand(const std::string& identifier, const std::vector<Token>& args)
{
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
		throw std::runtime_error("Missing script command signature");
	}

	// Turn tokens into parameters
	auto parameters = ScriptCommandParameters();

	for (auto arg : args)
	{
		const ScriptCommandParameter param = GetParameter(arg, _origin, _originOnMap);
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
		throw std::runtime_error("Invalid number of script arguments");
	}

	// Validate the typing of the given arguments against what is expected
	for (const auto& [param, expected] : std::views::zip(parameters, expectedParameters))
	{
		if (param.GetType() != expected)
		{
			throw std::runtime_error("Invalid script argument type");
		}
	}

	// The reader keeps the arguments before the command runs, and also for a command that its place refuses
	if (Locator::landSystem::has_value())
	{
		Locator::landSystem::value().GetScriptLineValues().Keep(parameters);
	}

	if (PlaceRefused(identifier, parameters))
	{
		if (const auto logger = spdlog::get("scripting"); logger != nullptr)
		{
			[[maybe_unused]] const auto place =
			    std::ranges::find_if(parameters, [](const auto& p) { return p.GetMapPlace().has_value(); });
			SPDLOG_LOGGER_DEBUG(logger, "LHScriptX: {} at ({:.2f}, {:.2f}) refused: its place is not free of fixed things",
			                    identifier, map_coords::ToMetres(place->GetMapPlace()->x),
			                    map_coords::ToMetres(place->GetMapPlace()->z));
		}
		return;
	}

	RunAndKeepMapPlaces(parameters, [&]() { commandSignature->command(parameters); });
}
