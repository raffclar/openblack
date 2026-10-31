/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <condition_variable>
#include <cstdint>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include <Inspector.h>
#include <InspectorDiscovery.h>
#include <InspectorServer.h>
#include <entt/meta/context.hpp>

#include "ECS/Systems/InspectorSystemInterface.h"
#include "Inspector/CameraControl.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::inspector
{
class FightAutopilot;
class GameInput;
class GameProvider;
struct GameControlSet;
class GameSeamInterface;
class InputProvider;
class ScreenshotProvider;
class SimProvider;
class InputTargetInterface;
class RunTargetInterface;
class WorldEditInterface;
} // namespace openblack::inspector

namespace openblack::ecs::systems
{

/// The inspector's server and its providers: the registry's entities and components, the objects about a point, the
/// moon, the splashes and particle effects, and the game's run control
class InspectorSystem final: public InspectorSystemInterface
{
public:
	/// The seam is the game on the player's computer, or null on the dedicated server
	InspectorSystem(std::unique_ptr<inspector::Server> server, std::unique_ptr<inspector::GameSeamInterface> seam);
	~InspectorSystem() override;
	InspectorSystem(const InspectorSystem&) = delete;
	InspectorSystem& operator=(const InspectorSystem&) = delete;

	void Service() override;
	[[nodiscard]] uint16_t GetPort() const override;
	void BeginLoading(std::string_view what) override;
	void EndLoading() override;
	void PlaceCamera() override;
	void GiveCameraBack() override;
	void ShowOverrideToPointer() override;
	[[nodiscard]] fight_autopilot::AutopilotInterface* GetFightAutopilot() override;
	void PauseOnLoad(bool pause) override;
	[[nodiscard]] std::optional<std::string> ReplayOnLoad(const std::filesystem::path& path) override;
	void RecordInputOnLoad(const std::filesystem::path& path) override;
	[[nodiscard]] std::optional<std::string> StartSimRecord(const std::filesystem::path& path, uint32_t every,
	                                                        const std::optional<std::filesystem::path>& limits,
	                                                        sim_run::HashDetail hashParts) override;
	void AddProvider(std::unique_ptr<inspector::ProviderInterface> provider) override;
	void RemoveProvider(std::string_view name) override;
	void SetHostsNetworkGame(bool hosts) override;

private:
	/// The helper answering while the game loads: until told to stop
	void AnswerWhileLoading();
	/// Tells the helper to stop and waits for it
	void StopLoadingHelper();
	/// A scenario asked for while the game loads is kept and loaded once the load is done, rather than refused: a tool
	/// asking for one straight after launching the game would otherwise get the empty testbed it was starting with
	[[nodiscard]] std::optional<inspector::QueryResult> QueueWhileLoading(const inspector::Request& request);
	/// Asks for the scenario kept while loading, if any, as though it was asked for now
	void RequestQueuedScenario();

	std::unique_ptr<inspector::Server> _server;
	/// The game on the player's computer, or null with none
	std::unique_ptr<inspector::GameSeamInterface> _seam;
	/// The components' reflection, the inspector's own rather than the library's shared one
	std::unique_ptr<entt::meta_ctx> _reflection;
	std::unique_ptr<inspector::RunTargetInterface> _runTarget;
	std::unique_ptr<inspector::WorldEditInterface> _worldEdit;
	std::unique_ptr<inspector::GameInput> _inputTarget;
	/// The camera, the windows, the scripts, the lands and the pictures of the screen, through the game
	std::unique_ptr<inspector::GameControlSet> _controls;
	inspector::Inspector _inspector;
	/// Owned by the inspector, told of each frame
	inspector::GameProvider* _game {nullptr};
	/// Owned by the inspector, makes the input due each frame
	inspector::InputProvider* _input {nullptr};
	/// Owned by the inspector, takes the pictures due each frame
	inspector::ScreenshotProvider* _screenshots {nullptr};
	/// Owned by the inspector, writes the run record of a simulation run
	inspector::SimProvider* _sim {nullptr};
	/// The file telling tools this game runs and where it listens, removed as the inspector goes
	std::unique_ptr<inspector::discovery::DiscoveryFile> _discovery;
	/// When it last served requests, for how long the input lock lingers after a client goes
	std::chrono::steady_clock::time_point _lastService {std::chrono::steady_clock::now()};
	/// How many loads are under way, what the outermost is loading, and the helper answering meanwhile
	int _loadingDepth {0};
	std::string _loading;
	/// The scenario asked for while loading, to load next
	std::optional<std::string> _queuedScenario;
	std::mutex _loadingMutex;
	std::atomic<bool> _stopLoadingHelper {false};
	/// Wakes the helper when it must stop, so that the end of a load does not wait for its next answer
	std::mutex _stopMutex;
	std::condition_variable _stopSignal;
	std::thread _loadingHelper;
	/// The recorded input that the next land makes from its first frame on
	std::optional<inspector::Json> _replayOnLoad;
	/// Tools: the recording of the input starts with the next land, and goes to this file as the inspector stops
	bool _recordInputOnLoad {false};
	std::optional<std::filesystem::path> _recordInputTo;
	void WriteRecordedInput();
	/// Where the camera of the next land stands before its first frame, as camera.set takes it
	std::optional<inspector::Json> _cameraOnLoad;
	/// The frames on which the drawn view changes to a different camera, and the last frame that it checked
	inspector::ShownCameraCuts _shownCameraCuts;
	std::optional<uint64_t> _cutsCheckedAt;
	/// The fight autopilot, which gives its input through the inspector's input
	std::unique_ptr<inspector::FightAutopilot> _autopilot;
	/// The game server hosts a network game: the queries that change the shared world are refused
	bool _hostsNetworkGame {false};
};

} // namespace openblack::ecs::systems
