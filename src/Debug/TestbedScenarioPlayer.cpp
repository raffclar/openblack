/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <utility>

#include "ScenarioPlayer.h"
#include "TestbedScenarioRegistry.h"
#include "TestbedScenarioRunner.h"

namespace openblack::testbed_scenarios
{

namespace
{

/// Plays one scenario of the registry with the runner that the window of scenarios uses
class RunnerPlayer final: public ScenarioPlayer
{
public:
	explicit RunnerPlayer(const Scenario& scenario)
	    : _scenario(scenario)
	{
	}

	void FollowGives(std::map<uint32_t, std::vector<size_t>> gives) override { _runner.FollowGives(std::move(gives)); }
	void BeforeTurn() override { _runner.BeforeTurn(); }

	void Start() override
	{
		_runner.Start(_scenario);
		// Give the commands, throws and rewards that are due at the start, as each client does when it lays out the
		// scenario
		_runner.Update(0.0f);
	}

private:
	const Scenario& _scenario;
	Runner _runner;
};

} // namespace

std::unique_ptr<ScenarioPlayer> MakeScenarioPlayer(std::string_view id)
{
	const Scenario* scenario = Find(id);
	return scenario == nullptr ? nullptr : std::make_unique<RunnerPlayer>(*scenario);
}

std::vector<std::string> ScenarioIds()
{
	std::vector<std::string> ids;
	for (const auto& scenario : All())
	{
		ids.emplace_back(scenario.id);
	}
	return ids;
}

} // namespace openblack::testbed_scenarios
