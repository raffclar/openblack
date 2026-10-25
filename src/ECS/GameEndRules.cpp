/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameEndRules.h"

#include <algorithm>
#include <array>

#include "Common/X87.h"

namespace openblack::game_end
{
using ecs::components::GameStanding;
using ecs::components::OutReason;
using ecs::components::WinCondition;

namespace
{
/// The number of gods among the first four players that take part
[[nodiscard]] uint32_t GodsThatCanLose(const std::vector<PlayerNames>& gods)
{
	return static_cast<uint32_t>(std::ranges::count_if(gods, [](PlayerNames god) { return CanLose(god); }));
}

/// The number of gods among the first four players that are out
[[nodiscard]] uint32_t GodsOut(World& world, const std::vector<PlayerNames>& gods)
{
	return static_cast<uint32_t>(
	    std::ranges::count_if(gods, [&world](PlayerNames god) { return CanLose(god) && world.Standing(god).out; }));
}

/// Whether so many gods are out that one god at most is left
[[nodiscard]] bool OneGodLeft(World& world, const std::vector<PlayerNames>& gods)
{
	// The game compares the counts as numbers with no sign
	return GodsOut(world, gods) >= GodsThatCanLose(gods) - 1u;
}

/// The game is decided: the god with the largest share of the conditions wins, and the others are out, the worst
/// first. Gives the winner.
std::optional<PlayerNames> Decide(World& world, State& state, const std::vector<PlayerNames>& gods, bool skirmish)
{
	state.decided = true;
	// Every god finished its game
	for (const auto god : gods)
	{
		world.Standing(god).finished = true;
	}
	// The gods that are still in, best first: each time the god with the largest share of those left. Of two gods with
	// the same share, the god with the larger share of the influence is the better.
	std::vector<PlayerNames> best;
	for (size_t pick = 0; pick < gods.size(); ++pick)
	{
		float bestShare = -1.0f;
		std::optional<PlayerNames> bestGod;
		for (const auto god : gods)
		{
			const auto& standing = world.Standing(god);
			if (standing.out)
			{
				continue;
			}
			if (standing.share > bestShare)
			{
				bestShare = standing.share;
				bestGod = god;
			}
			else if (standing.share == bestShare && bestGod.has_value() &&
			         world.InfluenceShare(*bestGod) < world.InfluenceShare(god))
			{
				bestShare = standing.share;
				bestGod = god;
			}
		}
		// The game marks each god it picks as out, so that the next pick does not see it again. When every god is out
		// already, the pick finds no god: the original game then stops with an error. Here the pick stays empty.
		if (bestGod.has_value())
		{
			world.Standing(*bestGod).out = true;
		}
		best.push_back(bestGod.value_or(PlayerNames::NEUTRAL));
	}
	if (best.empty())
	{
		return std::nullopt;
	}
	// The best god wins and is not out. The others are out, the worst first.
	const auto winner = best.front();
	if (winner == PlayerNames::NEUTRAL)
	{
		return std::nullopt;
	}
	world.Standing(winner).out = false;
	for (auto god = best.rbegin(); god != std::prev(best.rend()); ++god)
	{
		if (*god != PlayerNames::NEUTRAL)
		{
			CountOut(world, state, *god, OutReason::Defeated, skirmish);
		}
	}
	return winner;
}
} // namespace

void Add(WinCondition& condition, int32_t amount)
{
	condition.current += amount;
	if (condition.target <= condition.current)
	{
		condition.completed = true;
	}
}

void Set(WinCondition& condition, int32_t value)
{
	condition.current = value;
	if (condition.target <= value)
	{
		condition.completed = true;
	}
}

float ShareOfEachCondition(size_t count)
{
	return count == 0 ? 0.0f : x87::Div(1.0, static_cast<double>(count));
}

ConditionsDone ShareOfConditions(const GameStanding& standing, float shareOfEach)
{
	ConditionsDone done {.share = 0.0f, .all = true};
	for (const auto& condition : standing.conditions)
	{
		if (!condition.has_value())
		{
			continue;
		}
		float part = 1.0f;
		if (!condition->completed)
		{
			done.all = false;
			part = x87::Div(static_cast<double>(condition->current), static_cast<double>(condition->target));
		}
		done.share = x87::Add(x87::Mul(part, shareOfEach), done.share);
	}
	// A god that completed every condition has the whole share
	if (done.all)
	{
		done.share = 1.0f;
	}
	return done;
}

void CountOut(World& world, State& state, PlayerNames god, OutReason reason, bool skirmish)
{
	const auto gods = world.ActiveGods();
	// Every other human god that is still in saw a god defeated. A god that a computer mind plays makes the human gods
	// count only in a skirmish.
	const bool counts = world.IsHuman(god) || skirmish;
	std::optional<PlayerNames> lastHumanIn;
	for (const auto other : gods)
	{
		if (other == god || world.Standing(other).out || !world.IsHuman(other))
		{
			continue;
		}
		lastHumanIn = other;
		if (counts)
		{
			world.CountGodDefeated(other);
		}
	}
	// A human god takes the next place
	auto& standing = world.Standing(god);
	if (world.IsHuman(god))
	{
		standing.place = ++state.lastPlace;
		standing.reason = reason;
	}
	// When one god is left, the last human god that is still in takes the place after that
	if (GodsOut(world, gods) == GodsThatCanLose(gods) - 1u && lastHumanIn.has_value())
	{
		world.Standing(*lastHumanIn).place = ++state.lastPlace;
	}
}

std::vector<Event> ProcessTurn(World& world, const Facts& facts, State& state)
{
	std::vector<Event> events;
	if ((!facts.networkGame && !facts.skirmish) || state.decided)
	{
		return events;
	}
	const auto gods = world.ActiveGods();
	const bool timeUp = facts.timeLimitTurn.has_value() && *facts.timeLimitTurn == facts.turn && !facts.skirmish;
	bool templesLost = true;
	if (facts.conditionCount != 0 || timeUp)
	{
		// Each god's share of the winning conditions. A god that completed all of them decides the game.
		const float shareOfEach = ShareOfEachCondition(facts.conditionCount);
		templesLost = false;
		for (const auto god : gods)
		{
			if (!CanLose(god))
			{
				continue;
			}
			auto& standing = world.Standing(god);
			if (!world.HasTemple(god) && !standing.out)
			{
				templesLost = true;
			}
			const auto done = ShareOfConditions(standing, shareOfEach);
			standing.share = done.share;
			if (done.all)
			{
				state.decided = true;
			}
		}
		if (state.decided || timeUp)
		{
			if (const auto winner = Decide(world, state, gods, facts.skirmish); winner.has_value())
			{
				events.push_back({.kind = Event::Kind::Won, .god = *winner});
			}
		}
		// The game looks for gods with no temple only when a god that is still in has none
		if (!templesLost)
		{
			return events;
		}
	}
	// A god among the first four players with no temple is out, and its creature goes from the land
	for (const auto god : gods)
	{
		auto& standing = world.Standing(god);
		if (CanLose(god) && !world.HasTemple(god) && !standing.out)
		{
			standing.out = true;
			world.RemoveCreature(god);
		}
	}
	// The game counts each god that is now out. When one god at most is left, the game is decided.
	for (const auto god : gods)
	{
		auto& standing = world.Standing(god);
		if (!standing.out || standing.finished)
		{
			continue;
		}
		CountOut(world, state, god, standing.reason, facts.skirmish);
		if (OneGodLeft(world, gods))
		{
			state.decided = true;
			events.push_back({.kind = Event::Kind::GodOutAndDecided, .god = god});
		}
		else
		{
			events.push_back({.kind = Event::Kind::GodOut, .god = god});
		}
		world.Standing(god).finished = true;
	}
	return events;
}

} // namespace openblack::game_end
