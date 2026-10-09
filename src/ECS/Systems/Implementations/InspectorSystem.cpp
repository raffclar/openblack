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

#include <chrono>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>

#include "Debug/TestbedScenarioRegistry.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Game.h"
#include "Input/GameActionMapInterface.h"
#include "Inspector/ComponentReflection.h"
#include "Inspector/GameControls.h"
#include "Inspector/GameInput.h"
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
	void SetFixedFrameTime(std::optional<uint32_t> milliseconds) override
	{
		if (Locator::time::has_value())
		{
			Locator::time::value().SetFixedFrameTime(
			    milliseconds.has_value() ? std::optional(std::chrono::milliseconds(*milliseconds)) : std::nullopt);
		}
	}
	[[nodiscard]] std::optional<uint32_t> GetFixedFrameTime() const override
	{
		if (!Locator::time::has_value())
		{
			return std::nullopt;
		}
		const auto fixed = Locator::time::value().GetFixedFrameTime();
		return fixed.has_value() ? std::optional(static_cast<uint32_t>(fixed->count())) : std::nullopt;
	}
	[[nodiscard]] inspector::Json InputLock() const override { return inspector::InputLockState(); }
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
    , _inputTarget(std::make_unique<inspector::GameInput>())
    , _controls(std::make_unique<inspector::GameControlSet>(*_inputTarget))
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

	_game = inspector::AddGameProviders(_inspector, *_reflection, *_runTarget, *_worldEdit, _controls->View());
	auto screenshots = std::make_unique<inspector::ScreenshotProvider>(_controls->screenshots, _controls->camera);
	_screenshots = screenshots.get();
	_inspector.Add(std::move(screenshots));
	auto input = std::make_unique<inspector::InputProvider>(*_inputTarget);
	_input = input.get();
	_inspector.Add(std::move(input));

	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Inspector listening on 127.0.0.1:{}", _server->Port());
}

InspectorSystem::~InspectorSystem() = default;

void InspectorSystem::Service()
{
	// The player's input is kept out while a client is connected (and a moment after), as the lock's mode says
	const auto now = std::chrono::steady_clock::now();
	const auto seconds = std::chrono::duration<float>(now - _lastService).count();
	_lastService = now;
	_server->Poll([this](std::string_view line) {
		auto answer = _inspector.Handle(line);
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Inspector: {} -> {} bytes", line, answer.size());
		return answer;
	});
	if (Locator::gameActionSystem::has_value())
	{
		Locator::gameActionSystem::value().UpdateInputLock(_server->ClientCount() > 0, seconds);
	}
	_game->Frame();
	// The input due this frame is made before the game reads its input
	_input->Frame(_game->FrameNumber());
	// The pictures due this frame are asked for before it is drawn
	_screenshots->Frame(_game->FrameNumber());
}

uint16_t InspectorSystem::GetPort() const
{
	return _server->Port();
}
