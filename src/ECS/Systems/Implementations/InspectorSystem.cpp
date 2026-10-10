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

#include "Common/RandomNumberManager.h"
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

// The build's configuration, given by the build files
#if !defined(OPENBLACK_BUILD_TYPE)
#define OPENBLACK_BUILD_TYPE ""
#endif

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{

/// The game's clock and the testbed's scenarios, through the locator
// The inspector's seeded date is the game's own
static_assert(inspector::GameProvider::k_SeededDate == TimeSystemInterface::k_DeterministicDate);

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
	[[nodiscard]] uint32_t GetSeed() const override
	{
		return Locator::rng::has_value() ? Locator::rng::value().GetRunSeed() : 0;
	}
	void SetSeed(uint32_t seed, std::optional<int64_t> date) override
	{
		if (Locator::rng::has_value())
		{
			Locator::rng::value().SetRunSeed(seed);
		}
		if (Locator::time::has_value())
		{
			Locator::time::value().RestartClock(date);
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Inspector: seeded run, seed {}{}", seed,
		                   date.has_value() ? ", date pinned" : ", wall clock");
	}
	[[nodiscard]] std::optional<int64_t> GetPinnedDate() const override
	{
		return Locator::time::has_value() ? Locator::time::value().GetPinnedDate() : std::nullopt;
	}
	[[nodiscard]] uint32_t GetTicks() const override
	{
		return Locator::time::has_value() ? Locator::time::value().GetTicks() : 0;
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

	// Tools find this game, among others running, by its file in the shared folder; looking for it with a ping doesn't
	// keep the player out
	_server->SetControlFilter(&inspector::Inspector::TakesControl);
	namespace discovery = inspector::discovery;
	const auto executable = discovery::ExecutablePath();
	const auto worktree = discovery::FindWorktree(executable.parent_path());
	discovery::GameRecord record {
	    .port = _server->Port(),
	    .pid = discovery::CurrentProcessId(),
	    .worktree = worktree.has_value() ? worktree->generic_string() : std::string(),
	    .executable = executable.generic_string(),
	    .buildType = OPENBLACK_BUILD_TYPE,
	    .land = _controls->levels.Current(),
	    .startTime =
	        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(),
	};
	_inspector.SetIdentity(discovery::ToJson(record));
	_discovery = std::make_unique<discovery::DiscoveryFile>(discovery::DefaultFolder(), std::move(record));
	if (!_discovery->Written())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The inspector couldn't write its discovery file {}",
		                   _discovery->Path().generic_string());
	}
}

InspectorSystem::~InspectorSystem()
{
	_stopLoadingHelper = true;
	if (_loadingHelper.joinable())
	{
		_loadingHelper.join();
	}
}

void InspectorSystem::BeginLoading(std::string_view what)
{
	if (_loadingDepth++ > 0)
	{
		return;
	}
	{
		const std::scoped_lock lock(_loadingMutex);
		_loading = what;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Inspector: answering as loading {}", what);
	_stopLoadingHelper = false;
	_loadingHelper = std::thread([this] { AnswerWhileLoading(); });
}

void InspectorSystem::EndLoading()
{
	if (_loadingDepth == 0 || --_loadingDepth > 0)
	{
		return;
	}
	_stopLoadingHelper = true;
	if (_loadingHelper.joinable())
	{
		_loadingHelper.join();
	}
	// The land has started, and none of its frames has run yet
	_game->Loaded();
}

void InspectorSystem::AnswerWhileLoading()
{
	constexpr auto k_Interval = std::chrono::milliseconds(20);
	while (!_stopLoadingHelper)
	{
		std::string loading;
		{
			const std::scoped_lock lock(_loadingMutex);
			loading = _loading;
		}
		// Only what reads nothing of the game is answered here; the game's own frames answer the rest once loaded
		_server->Poll([this, &loading](std::string_view line) { return _inspector.HandleWhileLoading(line, loading); });
		std::this_thread::sleep_for(k_Interval);
	}
}

void InspectorSystem::Service()
{
	// The player's input is kept out while a client is connected (and a moment after), as the lock's mode says
	const auto now = std::chrono::steady_clock::now();
	const auto seconds = std::chrono::duration<float>(now - _lastService).count();
	_lastService = now;
	// A request answered here may load a land, while the loading helper answers the others
	_server->Poll([this](std::string_view line) {
		auto answer = _inspector.Handle(line);
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Inspector: {} -> {} bytes", line, answer.size());
		return answer;
	});
	if (Locator::gameActionSystem::has_value())
	{
		// Only a client driving this game keeps the player out: tools pinging it to find it don't
		Locator::gameActionSystem::value().UpdateInputLock(_server->ControllingClientCount() > 0, seconds);
	}
	// Tools listing the running games see the land each is on
	const auto land = _controls->levels.Current();
	if (land != _discovery->Record().land)
	{
		_discovery->SetLand(land);
		auto identity = inspector::discovery::ToJson(_discovery->Record());
		_inspector.SetIdentity(std::move(identity));
	}
	_game->Frame();
	// The input due this frame is made before the game reads its input
	_input->Frame(_game->FrameNumber());
	// The pictures due this frame are asked for before it is drawn
	_screenshots->Frame(_game->FrameNumber());
}

void InspectorSystem::PlaceCamera()
{
	_screenshots->PlaceCamera();
}

uint16_t InspectorSystem::GetPort() const
{
	return _server->Port();
}
