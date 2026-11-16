/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The editor.debug_thing query: the acts of the Physics, Particles, Temple villagers and Path finding debug windows, as
// their buttons do them

#include <cstdint>

#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <fmt/format.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Camera/Camera.h"
#include "Common/DebugThingActRuns.h"
#include "Common/DeveloperToolsGate.h"
#include "Debug/TempleVillagersSurvey.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "GameProviderShared.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::inspector::game_providers;
using player_commands::DebugParticle;
using player_commands::DebugPhysics;
using player_commands::DebugTempleVillager;
using player_commands::DebugVillagerPath;

namespace
{

[[nodiscard]] bool BoolOf(const Json& params, std::string_view name, bool fallback)
{
	const auto found = params.find(std::string(name));
	return found != params.end() && found->is_boolean() ? found->get<bool>() : fallback;
}

[[nodiscard]] float NumberOf(const Json& params, std::string_view name, float fallback)
{
	const auto found = params.find(std::string(name));
	return found != params.end() && found->is_number() ? found->get<float>() : fallback;
}

[[nodiscard]] int32_t IntegerOf(const Json& params, std::string_view name, int32_t fallback)
{
	const auto found = params.find(std::string(name));
	return found != params.end() && found->is_number_integer() ? found->get<int32_t>() : fallback;
}

[[nodiscard]] std::string TextOf(const Json& params, std::string_view name)
{
	const auto found = params.find(std::string(name));
	return found != params.end() && found->is_string() ? found->get<std::string>() : std::string();
}

/// The numbers of an array parameter, or none when the parameter does not have the count
[[nodiscard]] std::optional<std::vector<float>> NumbersOf(const Json& params, std::string_view name, size_t count)
{
	const auto found = params.find(std::string(name));
	if (found == params.end() || !found->is_array() || found->size() != count)
	{
		return std::nullopt;
	}
	std::vector<float> values(count, 0.0f);
	for (size_t i = 0; i < count; ++i)
	{
		values[i] = (*found)[i].is_number() ? (*found)[i].get<float>() : 0.0f;
	}
	return values;
}

/// The entity of a parameter, a number. Below 0 or absent gives the fallback.
[[nodiscard]] entt::entity EntityOf(const Json& params, std::string_view name, entt::entity fallback)
{
	const auto number = IntegerOf(params, name, -1);
	return number < 0 ? fallback : static_cast<entt::entity>(number);
}

[[nodiscard]] glm::vec3 CameraFocus()
{
	return Locator::camera::has_value() ? Locator::camera::value().GetFocus() : glm::vec3(0.0f);
}

/// The position of a parameter, else the focus of the camera
[[nodiscard]] glm::vec3 PositionOf(const Json& params)
{
	if (const auto values = NumbersOf(params, "position", 3))
	{
		return {(*values)[0], (*values)[1], (*values)[2]};
	}
	return CameraFocus();
}

[[nodiscard]] entt::entity FirstMadeVillager()
{
	const auto made = debug_thing_acts::MadeVillagers();
	return made.empty() ? entt::entity {entt::null} : made.front();
}

template <class Command>
[[nodiscard]] QueryResult Sent(const Command& act, std::string_view kind, std::string_view name)
{
	const auto result = debug_thing_acts::Do(act);
	Json answer {{"sent", !result.has_value()}, {"kind", kind}, {"act", name}};
	if (result.has_value())
	{
		answer["done"] = result->done;
		answer["made"] = result->made.size();
	}
	return QueryResult::Value(std::move(answer));
}

/// Finds the act of the name among the acts up to the last one
template <class Act>
[[nodiscard]] std::optional<Act> ActOf(std::string_view name, Act last)
{
	for (uint8_t i = 0; i <= static_cast<uint8_t>(last); ++i)
	{
		const auto act = static_cast<Act>(i);
		if (debug_thing_acts::NameOf(act) == name)
		{
			return act;
		}
	}
	return std::nullopt;
}

QueryResult PhysicsAct(const Json& params)
{
	const auto velocity = NumbersOf(params, "velocity", 3).value_or(std::vector<float> {0.0f, 0.0f, 0.0f});
	const auto focus = CameraFocus();
	const auto place = NumbersOf(params, "place", 2).value_or(std::vector<float> {focus.x, focus.z});
	const auto type = IntegerOf(params, "type", 36);
	const DebugPhysics act {
	    .object = EntityOf(params, "object", entt::null),
	    .type = static_cast<uint16_t>(type < 0 ? UINT16_MAX : type),
	    .height = NumberOf(params, "height", 20.0f),
	    .scale = NumberOf(params, "scale", 1.0f),
	    .place = {place[0], place[1]},
	    .velocity = {velocity[0], velocity[1], velocity[2]},
	};
	return Sent(act, "physics", "throw");
}

QueryResult ParticleAct(const Json& params)
{
	const auto name = TextOf(params, "act");
	const auto act = ActOf(name, DebugParticle::Act::Delete);
	if (!act.has_value())
	{
		return QueryResult::Error("editor.debug_thing particle needs start, pause, close_all, delete_all, close or delete");
	}
	const DebugParticle command {
	    .act = *act,
	    .file = TextOf(params, "file"),
	    .type = static_cast<uint8_t>(IntegerOf(params, "type", 0)),
	    .position = PositionOf(params),
	    .magnitude = NumberOf(params, "magnitude", 1.0f),
	    .player = static_cast<uint8_t>(IntegerOf(params, "player", 0)),
	    .drawPath = static_cast<uint8_t>(IntegerOf(params, "draw", 0)),
	    .targets = static_cast<uint8_t>(IntegerOf(params, "targets", 0)),
	    .effect = static_cast<uint32_t>(IntegerOf(params, "effect", 0)),
	    .paused = BoolOf(params, "paused", false),
	};
	return Sent(command, "particle", name);
}

QueryResult TempleAct(const Json& params)
{
	const auto name = TextOf(params, "act");
	const auto act = ActOf(name, DebugTempleVillager::Act::TakeAway);
	if (!act.has_value())
	{
		return QueryResult::Error(
		    "editor.debug_thing temple needs share, send_one, call_one_back, make, send_to_worship or take_away");
	}
	// The town is the first town that worships at the temple of the first player, as the window gives it. With no such
	// town, it is the town of the lowest number, and its home is its position.
	const auto survey = temple_villagers::Take(PlayerNames::PLAYER_ONE, 0.0f, false);
	auto town = entt::entity {entt::null};
	auto home = CameraFocus();
	if (!survey.towns.empty())
	{
		town = survey.towns.front().town;
		home = survey.towns.front().home.value_or(home);
	}
	else if (const auto* registry = Registry())
	{
		registry->Each<const ecs::components::Town, const ecs::components::Transform>(
		    [&town, &home](entt::entity entity, const ecs::components::Town&, const ecs::components::Transform& transform) {
			    if (town == entt::null || entity < town)
			    {
				    town = entity;
				    home = transform.position;
			    }
		    });
	}
	DebugTempleVillager command {
	    .act = *act,
	    .town = EntityOf(params, "town", town),
	    .villager = EntityOf(params, "villager", FirstMadeVillager()),
	    .home = home,
	    .value = NumberOf(params, "value", 0.5f),
	    .tribe = static_cast<uint8_t>(IntegerOf(params, "tribe", 0)),
	    .number = static_cast<uint8_t>(IntegerOf(params, "number", 0)),
	    .age = static_cast<uint8_t>(IntegerOf(params, "age", 30)),
	    .count = static_cast<uint8_t>(IntegerOf(params, "count", 4)),
	    .send = BoolOf(params, "send", true),
	};
	if (*act == DebugTempleVillager::Act::TakeAway)
	{
		// The Take away button takes away each villager that the window of this computer made
		const auto acts = debug_thing_acts::TakeAwayActs(debug_thing_acts::MadeVillagers());
		if (acts.empty())
		{
			return QueryResult::Error("editor.debug_thing temple take_away: the window made no villager");
		}
		for (size_t i = 1; i < acts.size(); ++i)
		{
			std::ignore = debug_thing_acts::Do(acts.at(i));
		}
		command = acts.front();
	}
	return Sent(command, "temple", name);
}

QueryResult PathAct(const Json& params)
{
	const auto name = TextOf(params, "act");
	const auto act = ActOf(name, DebugVillagerPath::Act::Speed);
	if (!act.has_value())
	{
		return QueryResult::Error("editor.debug_thing path needs teleport, move_to, goal or speed");
	}
	const auto goal = NumbersOf(params, "goal", 2).value_or(std::vector<float> {0.0f, 0.0f});
	const DebugVillagerPath command {
	    .act = *act,
	    .villager = EntityOf(params, "villager", FirstMadeVillager()),
	    .position = PositionOf(params),
	    .goal = {static_cast<int32_t>(goal[0]), static_cast<int32_t>(goal[1])},
	    .speed = NumberOf(params, "speed", 1.0f),
	};
	return Sent(command, "path", name);
}

QueryResult DebugThing(const Json& params)
{
	if (!BoolOf(params, "force", false) && developer_tools::CurrentGuiMode() != developer_tools::GuiMode::Full)
	{
		return QueryResult::Error("editor.debug_thing: the debug windows do not show: the player has no debugger right");
	}
	const auto kind = TextOf(params, "kind");
	if (kind == "physics")
	{
		return PhysicsAct(params);
	}
	if (kind == "particle")
	{
		return ParticleAct(params);
	}
	if (kind == "temple")
	{
		return TempleAct(params);
	}
	if (kind == "path")
	{
		return PathAct(params);
	}
	return QueryResult::Error("editor.debug_thing needs the kind physics, particle, temple or path");
}

} // namespace

namespace openblack::inspector::game_providers
{

void AddDebugThingQueries(FunctionProvider& provider)
{
	auto debugThing = Query(
	    "debug_thing",
	    "Does an act of the Physics, Particles, Temple villagers or Path finding debug windows, as their buttons do: a throw "
	    "(kind physics), an act on the effects with the shared random numbers (kind particle), an act on the towns and the "
	    "villagers of the temple of the first player (kind temple), or a move of a villager (kind path). In a network game "
	    "the acts go as commands of the turns, and the game server takes them only from a player with the debugger claim. "
	    "force sends the act also when the debug windows of this computer do not show.",
	    {{.name = "kind", .type = "string", .description = "physics, particle, temple or path", .required = true},
	     {.name = "act",
	      .type = "string",
	      .description = "particle: start, pause, close_all, delete_all, close or delete. temple: share, send_one, "
	                     "call_one_back, make, send_to_worship or take_away. path: teleport, move_to, goal or speed.",
	      .required = false},
	     {.name = "object",
	      .type = "integer",
	      .description = "physics: the thing to throw, -1 for a new one",
	      .required = false},
	     {.name = "type",
	      .type = "integer",
	      .description = "physics: the type of the new static thing. particle start: the particle type",
	      .required = false},
	     {.name = "height", .type = "number", .description = "physics: the height above the land", .required = false},
	     {.name = "scale", .type = "number", .description = "physics: the scale", .required = false},
	     {.name = "place", .type = "array", .description = "physics: x and z, else the focus of the camera", .required = false},
	     {.name = "velocity", .type = "array", .description = "physics: the velocity x, y and z", .required = false},
	     {.name = "file", .type = "string", .description = "particle start: the particle file", .required = false},
	     {.name = "position",
	      .type = "array",
	      .description = "particle start and path: x, y and z, else the focus of the camera",
	      .required = false},
	     {.name = "magnitude", .type = "number", .description = "particle start: the magnitude", .required = false},
	     {.name = "player", .type = "integer", .description = "particle start: the player, 0 to 7", .required = false},
	     {.name = "draw", .type = "integer", .description = "particle start: the draw, 0 to 5", .required = false},
	     {.name = "targets", .type = "integer", .description = "particle start: the count of the targets", .required = false},
	     {.name = "effect", .type = "integer", .description = "particle close and delete: the effect", .required = false},
	     {.name = "paused", .type = "boolean", .description = "particle pause: true stops the steps", .required = false},
	     {.name = "town", .type = "integer", .description = "temple: the town, else the first town", .required = false},
	     {.name = "villager",
	      .type = "integer",
	      .description = "temple and path: the villager, else the first villager that the window made",
	      .required = false},
	     {.name = "value", .type = "number", .description = "temple share: the share, 0 to 1", .required = false},
	     {.name = "tribe", .type = "integer", .description = "temple make: the tribe", .required = false},
	     {.name = "number", .type = "integer", .description = "temple make: the job", .required = false},
	     {.name = "age", .type = "integer", .description = "temple make: the age, 1 to 80", .required = false},
	     {.name = "count", .type = "integer", .description = "temple make: the count, 1 to 32", .required = false},
	     {.name = "send", .type = "boolean", .description = "temple make: the villagers go to worship", .required = false},
	     {.name = "goal", .type = "array", .description = "path goal: the goal x and z", .required = false},
	     {.name = "speed", .type = "number", .description = "path speed: the speed in metres a second", .required = false},
	     {.name = "force",
	      .type = "boolean",
	      .description = "Send also when the debug windows do not show",
	      .required = false}});
	debugThing.writes = true;
	provider.Add(std::move(debugThing), [](const QueryContext& context) { return DebugThing(context.params); });
}

} // namespace openblack::inspector::game_providers
