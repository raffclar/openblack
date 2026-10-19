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
#include <string_view>

#include <entt/signal/sigh.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/LandForest.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/FootpathRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FixedOutlineSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "FeatureScriptCommands.h"
#include "Lexer.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::lhscriptx;

Script::Script() = default;

Script::Script(glm::vec3 origin, const map_coords::MapCoords& originOnMap)
    : _origin(origin)
    , _originOnMap(originOnMap)
{
}

void Script::Load(const std::string& source, const std::function<void()>& afterCommand)
{
	Lexer lexer(source);

	const Token* token = this->PeekToken(lexer);
	while (!token->IsEOF())
	{
		token = this->PeekToken(lexer);

		if (token->IsIdentifier())
		{
			const std::string identifier = token->Identifier();

			if (!IsCommand(identifier))
			{
				// A word the game doesn't know is passed over, as is everything after it up to the next command it
				// knows, so a misspelt line is ignored and the rest of the land still loads
				if (const auto logger = spdlog::get("scripting"); logger != nullptr)
				{
					SPDLOG_LOGGER_WARN(logger, "LHScriptX: skipping unknown command {}", identifier);
				}
				this->AdvanceToken(lexer);
				continue;
			}

			token = this->AdvanceToken(lexer);
			if (!token->IsOP(Operator::LeftParentheses))
			{
				throw std::runtime_error("expected ( after identifier " + identifier);
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

			// A command ends at its ')' or, when that was left out, at the end of its line
			const bool closedByLineEnd = token->IsEOL() || token->IsEOF();
			if (!closedByLineEnd && !token->IsOP(Operator::RightParentheses))
			{
				throw std::runtime_error("missing )");
			}

			// move token to whatever is after ')'
			if (!closedByLineEnd)
			{
				this->AdvanceToken(lexer);
			}

			RunCommand(identifier, args);
			if (afterCommand)
			{
				afterCommand();
			}
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
		// Check if it's a vector
		if (std::count_if(str.cbegin(), str.cend(), [](char c) { return c == ','; }) == 1)
		{
			const auto delim = str.find(',');
			char* floatEnd;
			const auto x = std::strtof(str.c_str(), &floatEnd) + origin.x;
			if (str.c_str() + delim == floatEnd)
			{
				const auto z = std::strtof(floatEnd + 1, &floatEnd) + origin.z;
				if (static_cast<size_t>(floatEnd - str.c_str()) == static_cast<size_t>(str.length()))
				{
					const auto& island = Locator::terrainSystem::value();
					ScriptCommandParameter place {x, island.GetHeightAt(glm::vec2(x, z)) + origin.y, z};
					// The game reads the same text to its exact map position, then moves it by the origin's map position
					const auto onMap = map_coords::ScriptPlace(std::strtod(str.c_str(), nullptr),
					                                           std::strtod(str.c_str() + delim + 1, nullptr));
					place.SetMapPlace(onMap + map_coords::MapCoords {.x = originOnMap.x, .z = originOnMap.z});
					return place;
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
		throw std::runtime_error("Operator token as an argument is currently not supported");
	default:
		throw std::runtime_error("Missing switch case for script token argument");
	}
}
} // namespace

namespace
{
/// Keeps the first thing made that can keep footpaths, while a command of the land's script runs
struct FirstFootpathKeeper
{
	entt::entity entity {entt::null};
	void Made(entt::registry&, entt::entity made)
	{
		if (entity == entt::null)
		{
			entity = made;
		}
	}
};

/// Runs a command that makes a thing, and keeps that thing for the footpath link commands that follow. The kinds
/// listed are those that can keep footpaths. Any other thing is kept as nothing, so that a link that follows it does
/// nothing.
template <typename Run>
void RunAndKeepThingMade(Run&& run)
{
	using namespace ecs::components;
	if (!Locator::entitiesRegistry::has_value())
	{
		run();
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& underlying = registry.Underlying();
	FirstFootpathKeeper keeper;
	const std::array connections {
	    entt::scoped_connection {underlying.on_construct<Abode>().connect<&FirstFootpathKeeper::Made>(keeper)},
	    entt::scoped_connection {underlying.on_construct<Field>().connect<&FirstFootpathKeeper::Made>(keeper)},
	    entt::scoped_connection {underlying.on_construct<FishFarm>().connect<&FirstFootpathKeeper::Made>(keeper)},
	    entt::scoped_connection {underlying.on_construct<WorshipSite>().connect<&FirstFootpathKeeper::Made>(keeper)},
	    entt::scoped_connection {underlying.on_construct<Feature>().connect<&FirstFootpathKeeper::Made>(keeper)},
	    entt::scoped_connection {underlying.on_construct<Flowers>().connect<&FirstFootpathKeeper::Made>(keeper)},
	    entt::scoped_connection {underlying.on_construct<AnimatedStatic>().connect<&FirstFootpathKeeper::Made>(keeper)},
	    entt::scoped_connection {underlying.on_construct<MobileStatic>().connect<&FirstFootpathKeeper::Made>(keeper)},
	    entt::scoped_connection {underlying.on_construct<LandForest>().connect<&FirstFootpathKeeper::Made>(keeper)},
	};
	run();
	registry.Context().scriptThingMadeLast = keeper.entity;
}
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

	RunAndKeepMapPlaces(parameters, [&]() {
		if (ecs::footpath_rules::ScriptCommandMakesThing(identifier))
		{
			RunAndKeepThingMade([&]() { commandSignature->command(parameters); });
			return;
		}
		commandSignature->command(parameters);
	});
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
