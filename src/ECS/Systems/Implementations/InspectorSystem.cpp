/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "InspectorSystem.h"

#include <string>
#include <utility>

#include <spdlog/spdlog.h>

#include "Debug/TestbedScenarioRegistry.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Game.h"
#include "Inspector/ComponentReflection.h"
#include "Inspector/GameProviders.h"
#include "Inspector/GameWorldEdit.h"
#include "Inspector/RunControl.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{

/// The game's clock and the testbed's scenarios, through the locator
class GameRunTarget final: public inspector::RunTargetInterface
{
public:
	[[nodiscard]] bool IsPaused() const override { return Locator::time::has_value() && Locator::time::value().IsPaused(); }
	void SetPaused(bool paused) override
	{
		if (Locator::time::has_value())
		{
			Locator::time::value().SetPaused(paused);
		}
	}
	[[nodiscard]] uint32_t GetTurn() const override
	{
		return Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	}
	[[nodiscard]] float GetSpeed() const override
	{
		return Locator::time::has_value() ? Locator::time::value().GetSpeed() : 1.0f;
	}
	void SetSpeed(float speed) override
	{
		if (Locator::time::has_value())
		{
			Locator::time::value().SetSpeed(speed);
		}
	}
	bool LoadScenario(std::string_view id) override
	{
		auto* game = Game::Instance();
		if (game == nullptr || testbed_scenarios::Find(id) == nullptr)
		{
			return false;
		}
		game->RequestScenario({.id = std::string(id), .benchmark = false});
		return true;
	}
	[[nodiscard]] std::vector<inspector::ScenarioSummary> Scenarios() const override
	{
		std::vector<inspector::ScenarioSummary> summaries;
		for (const auto& scenario : testbed_scenarios::All())
		{
			summaries.push_back({
			    .id = std::string(scenario.id),
			    .name = std::string(scenario.name),
			    .facet = std::string(testbed_scenarios::Name(scenario.facet)),
			    .description = std::string(scenario.description),
			});
		}
		return summaries;
	}
};

} // namespace

InspectorSystem::InspectorSystem(std::unique_ptr<inspector::Server> server)
    : _server(std::move(server))
    , _reflection(std::make_unique<entt::meta_ctx>())
    , _runTarget(std::make_unique<GameRunTarget>())
    , _worldEdit(std::make_unique<inspector::GameWorldEdit>())
{
	inspector::reflection::RegisterComponents(*_reflection);
	_inspector.SetWriteLog([](const inspector::Request& request, const inspector::QueryResult& answer) {
		if (answer.Ok())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Inspector wrote: {} {}", request.query, inspector::Dump(request.params));
		}
		else
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Inspector refused: {} {}: {}", request.query,
			                   inspector::Dump(request.params), answer.error);
		}
	});

	_game = inspector::AddGameProviders(_inspector, *_reflection, *_runTarget, *_worldEdit);

	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Inspector listening on 127.0.0.1:{}", _server->Port());
}

InspectorSystem::~InspectorSystem() = default;

void InspectorSystem::Service()
{
	_server->Poll([this](std::string_view line) {
		auto answer = _inspector.Handle(line);
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Inspector: {} -> {} bytes", line, answer.size());
		return answer;
	});
	_game->Frame();
}

uint16_t InspectorSystem::GetPort() const
{
	return _server->Port();
}
