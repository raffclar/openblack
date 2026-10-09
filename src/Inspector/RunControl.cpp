/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "RunControl.h"

#include <cmath>

#include <algorithm>
#include <utility>

using namespace openblack::inspector;

namespace
{

/// The most frames or turns one step may run, so that a mistyped count doesn't run away
constexpr double k_LongestStep = 100000.0;

std::optional<uint32_t> Count(const Json& params, std::string_view key)
{
	const auto value = NumberMember(params, key);
	if (!value.has_value() || *value < 1.0 || *value > k_LongestStep || std::floor(*value) != *value)
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(*value);
}

} // namespace

void RunControl::Cancel()
{
	_framesLeft.reset();
	_untilTurn.reset();
}

void RunControl::StepFrames(uint32_t frames)
{
	Cancel();
	_framesLeft = frames;
}

void RunControl::StepTurns(uint32_t turns, uint32_t currentTurn)
{
	Cancel();
	_untilTurn = currentTurn + turns;
}

std::optional<bool> RunControl::Frame(bool paused, uint32_t turn)
{
	if (_framesLeft.has_value())
	{
		if (*_framesLeft == 0)
		{
			_framesLeft.reset();
			return true;
		}
		--*_framesLeft;
		return paused ? std::optional(false) : std::nullopt;
	}
	if (_untilTurn.has_value())
	{
		if (turn >= *_untilTurn)
		{
			_untilTurn.reset();
			return true;
		}
		return paused ? std::optional(false) : std::nullopt;
	}
	return std::nullopt;
}

GameProvider::GameProvider(RunTargetInterface& target)
    : _target(target)
{
}

std::vector<QueryDescription> GameProvider::Describe() const
{
	const auto state = [](std::string name, std::string description, std::vector<ParameterDescription> parameters = {}) {
		return QueryDescription {.name = std::move(name),
		                         .description = std::move(description),
		                         .parameters = std::move(parameters),
		                         .kind = ResultKind::Object,
		                         .needsNear = false};
	};
	return {
	    state("state", "Whether the game is paused, its turn, the frames served, its speed and any stepping"),
	    state("pause", "Pauses the game, stopping any stepping; the state after"),
	    state("resume", "Lets the game run, stopping any stepping; the state after"),
	    state("step",
	          "Runs the game for some frames or turns from the next frame, then pauses it; poll game.state until "
	          "stepping is null",
	          {{.name = "frames", .type = "integer", .description = "Frames to run", .required = false},
	           {.name = "turns", .type = "integer", .description = "Game turns to run (ten a second)", .required = false}}),
	    state("speed", "Sets the game's speed; the state after",
	          {{.name = "speed", .type = "number", .description = "1 is normal, 2 twice as fast", .required = true}}),
	    state("scenario", "Loads a testbed scenario on a fresh testbed",
	          {{.name = "id", .type = "string", .description = "The scenario's id, from game.scenarios", .required = true}}),
	    QueryDescription {.name = "scenarios",
	                      .description = "The testbed scenarios: id, name, facet and what each sets up",
	                      .parameters = {},
	                      .kind = ResultKind::List,
	                      .needsNear = false},
	};
}

Json GameProvider::State() const
{
	Json stepping = nullptr;
	if (const auto frames = _control.FramesLeft(); frames.has_value())
	{
		stepping = {{"frames_left", *frames}};
	}
	else if (const auto turn = _control.UntilTurn(); turn.has_value())
	{
		stepping = {{"until_turn", *turn}};
	}
	return {
	    {"paused", _target.IsPaused()}, {"turn", _target.GetTurn()},       {"frame", _frame},
	    {"speed", _target.GetSpeed()},  {"stepping", std::move(stepping)},
	};
}

void GameProvider::Frame()
{
	++_frame;
	if (const auto paused = _control.Frame(_target.IsPaused(), _target.GetTurn()); paused.has_value())
	{
		_target.SetPaused(*paused);
	}
}

QueryResult GameProvider::Run(std::string_view query, const QueryContext& context)
{
	const auto& params = context.params;
	if (query == "state")
	{
		return QueryResult::Value(State());
	}
	if (query == "pause" || query == "resume")
	{
		_control.Cancel();
		_target.SetPaused(query == "pause");
		return QueryResult::Value(State());
	}
	if (query == "step")
	{
		const auto frames = Count(params, "frames");
		const auto turns = Count(params, "turns");
		if (frames.has_value() == turns.has_value())
		{
			return QueryResult::Error("game.step needs either frames or turns, a whole number from 1");
		}
		if (frames.has_value())
		{
			_control.StepFrames(*frames);
		}
		else
		{
			_control.StepTurns(*turns, _target.GetTurn());
		}
		return QueryResult::Value(State());
	}
	if (query == "speed")
	{
		const auto speed = NumberMember(params, "speed");
		if (!speed.has_value() || *speed < k_SlowestSpeed || *speed > k_FastestSpeed)
		{
			return QueryResult::Error("game.speed needs a speed from 0.1 to 16");
		}
		_target.SetSpeed(static_cast<float>(*speed));
		return QueryResult::Value(State());
	}
	if (query == "scenario")
	{
		const auto id = StringMember(params, "id").value_or("");
		if (!_target.LoadScenario(id))
		{
			return QueryResult::Error("no scenario " + id + "; ask game.scenarios for them");
		}
		_control.Cancel();
		return QueryResult::Value({{"loading", id}});
	}
	if (query == "scenarios")
	{
		Json items = Json::array();
		for (const auto& scenario : _target.Scenarios())
		{
			items.push_back({
			    {"id", scenario.id},
			    {"name", scenario.name},
			    {"facet", scenario.facet},
			    {"description", scenario.description},
			});
		}
		return QueryResult::Value(std::move(items));
	}
	return QueryResult::Error("no query game." + std::string(query));
}
