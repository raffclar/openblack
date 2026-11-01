/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedScenarioChecks.h"

#include <algorithm>

#include <fmt/format.h>

namespace openblack::testbed_scenarios::checks
{

std::string_view Name(Verdict verdict)
{
	switch (verdict)
	{
	case Verdict::NoChecks:
		return "no checks";
	case Verdict::Waiting:
		return "waiting";
	case Verdict::Passed:
		return "passed";
	case Verdict::Failed:
		return "failed";
	}
	return "";
}

std::string_view Name(Quantity quantity)
{
	switch (quantity)
	{
	case Quantity::CreaturesAlive:
		return "creatures alive";
	case Quantity::ObjectsAlive:
		return "objects alive";
	case Quantity::Entities:
		return "things";
	case Quantity::CreatureEnergy:
		return "energy";
	case Quantity::CreatureThirst:
		return "thirst";
	case Quantity::CreatureExhaustion:
		return "exhaustion";
	case Quantity::CreaturePoo:
		return "poo";
	case Quantity::CreatureWarmth:
		return "warmth";
	case Quantity::CreatureSize:
		return "size";
	case Quantity::CreatureLife:
		return "life";
	case Quantity::CreatureDistance:
		return "distance to the point";
	case Quantity::ObjectResource:
		return "food and wood held";
	case Quantity::ObjectDistance:
		return "distance of the object to the point";
	}
	return "";
}

int ExitCodeOf(Verdict verdict)
{
	switch (verdict)
	{
	case Verdict::Passed:
		return 0;
	case Verdict::Failed:
		return 1;
	case Verdict::NoChecks:
	case Verdict::Waiting:
		break;
	}
	return 2;
}

bool Holds(const Check& check, double value)
{
	return (!check.atLeast.has_value() || value >= *check.atLeast) && (!check.atMost.has_value() || value <= *check.atMost);
}

namespace
{
std::string Limits(const Check& check)
{
	if (check.atLeast.has_value() && check.atMost.has_value())
	{
		return fmt::format("from {} to {}", *check.atLeast, *check.atMost);
	}
	if (check.atLeast.has_value())
	{
		return fmt::format("at least {}", *check.atLeast);
	}
	if (check.atMost.has_value())
	{
		return fmt::format("at most {}", *check.atMost);
	}
	return "any value";
}
} // namespace

std::string Describe(const Check& check, const Result& result)
{
	const std::string what = check.label.empty() ? std::string(Name(check.quantity)) : std::string(check.label);
	const std::string seen = result.observed.has_value() ? fmt::format("{:.3f}", *result.observed) : "no value";
	return fmt::format("{} {}: {} ({}) at turn {}", result.passed ? "pass" : "FAIL", what, seen, Limits(check), result.turn);
}

void Evaluator::Start(std::span<const Check> checks)
{
	_checks = checks;
	_results.assign(checks.size(), Result {});
	for (size_t i = 0; i < _results.size(); ++i)
	{
		_results[i].check = i;
	}
	_decided.assign(checks.size(), false);
}

void Evaluator::Update(uint32_t turnsSinceStart, const Probe& probe)
{
	for (size_t i = 0; i < _checks.size(); ++i)
	{
		const auto& check = _checks[i];
		if (_decided[i] || turnsSinceStart < check.afterTurns)
		{
			continue;
		}
		auto& result = _results[i];
		result.observed = probe ? probe(check) : std::nullopt;
		result.turn = turnsSinceStart;
		const bool holds = result.observed.has_value() && Holds(check, *result.observed);
		if (!check.byTurn.has_value())
		{
			result.passed = holds;
			_decided[i] = true;
			continue;
		}
		if (holds)
		{
			result.passed = true;
			_decided[i] = true;
		}
		else if (turnsSinceStart >= std::max(*check.byTurn, check.afterTurns))
		{
			result.passed = false;
			_decided[i] = true;
		}
	}
}

bool Evaluator::Done() const
{
	return std::ranges::all_of(_decided, [](bool decided) { return decided; });
}

Verdict Evaluator::GetVerdict() const
{
	if (_checks.empty())
	{
		return Verdict::NoChecks;
	}
	// One failure decides the verdict at once, though other checks still wait
	for (size_t i = 0; i < _results.size(); ++i)
	{
		if (_decided[i] && !_results[i].passed)
		{
			return Verdict::Failed;
		}
	}
	return Done() ? Verdict::Passed : Verdict::Waiting;
}

std::vector<std::string> Evaluator::Report(std::string_view scenario) const
{
	std::vector<std::string> lines;
	for (size_t i = 0; i < _checks.size(); ++i)
	{
		lines.push_back(
		    _decided[i] ? Describe(_checks[i], _results[i])
		                : fmt::format("waiting {}", _checks[i].label.empty() ? Name(_checks[i].quantity) : _checks[i].label));
	}
	lines.push_back(fmt::format("scenario {}: {}", scenario, Name(GetVerdict())));
	return lines;
}

} // namespace openblack::testbed_scenarios::checks
