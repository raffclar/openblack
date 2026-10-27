/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "ECS/Systems/LandSystemInterface.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class LandSystem final: public LandSystemInterface
{
public:
	void SetView(LandViewInterface* view) override { _view = view; }

	bool LoadMap(const std::filesystem::path& path, loading::LoadingClock::Mode look, land_change::Kind change) override;
	bool LoadMapWithFreshScripts(const std::filesystem::path& path, land_change::Kind change) override;
	void LoadTestbed() override;
	bool LoadGodsRingMap() override;
	bool LoadSavedLand(const std::filesystem::path& landPath, const std::filesystem::path& landscape,
	                   const std::function<bool()>& restore, SavedLandUse use) override;
	void LoadLandscape(const std::filesystem::path& path) override;
	void SetStartCameraPlace(glm::ivec2 place) override { _startCameraPlace = place; }

	[[nodiscard]] bool IsPlaygroundLand(const std::filesystem::path& path) const override;
	[[nodiscard]] const std::filesystem::path& GetLandPath() const override { return _landPath; }
	[[nodiscard]] const std::filesystem::path& GetLandscapePath() const override { return _landscapePath; }
	[[nodiscard]] std::filesystem::path ChallengePath() override;

	void SetNewGameStart(std::optional<new_game_choice::NewGameStart> start) override { _newGameStart = start; }
	void SetHumansFromSession(bool fromSession) override { _humansFromSession = fromSession; }
	[[nodiscard]] bool RunsNetworkMapScript() const override { return _runsNetworkMapScript; }
	void SetAllComputerGods(bool allComputer) override { _allComputerGods = allComputer; }
	bool StartChallengeScripts(bool startStory) override;
	void StartStoryScripts() override;

	void PlayTurns(simulation::TurnViewInterface& view) override;
	void PlayTurnSteps(simulation::TurnViewInterface& view) override;
	void FinishTurns(uint32_t turnBefore, std::chrono::steady_clock::time_point start,
	                 std::optional<std::chrono::steady_clock::time_point> end) override;
	[[nodiscard]] std::chrono::steady_clock::duration GetTurnDeltaTime() const override { return _turnDeltaTime; }
	void KeepAlignmentInProfile() override;

private:
	/// The view of the player, or a view that does nothing
	[[nodiscard]] LandViewInterface& View() { return _view != nullptr ? *_view : _noView; }

	/// Clears the last land for the next, and keeps what the reason for the change keeps. Nothing goes onto the
	/// testbed or off it.
	void PrepareNewLand(land_change::Kind change);
	/// What every land starts with when its landscape is loaded: the clock of the sky, a player and the land's physics
	void SetUpLandscape();
	/// After a load, the models that the game made during play are made again for the loaded things
	void RemakeModelsAfterLoad();
	/// Starts the game clock and the sounds of the new land. The story's next land keeps the clock and starts only its
	/// turns again.
	void StartNewLand(land_change::Kind change);
	/// The player of a new game starts as good or evil as the profile last kept them
	void StartPlayerAtProfileAlignment();
	/// The player is as good or evil as the profile keeps them
	void SetPlayerAlignmentFromProfile();
	/// As a new game starts, asks the player what to skip. Gives whether the question was put or answered at once.
	bool AskNewGameChoice();
	/// The turn of the world when it is due, and the temple's own turns
	void GameLogicLoop(simulation::TurnViewInterface& view);
	/// The turns that are due, with the queue of the main thread told that this thread does them
	void PlayDueTurns(simulation::TurnViewInterface& view);

	LandViewInterface* _view {nullptr};
	NoLandView _noView;
	std::filesystem::path _landPath;
	std::filesystem::path _landscapePath;
	/// The landscape of the land of a network game whose script runs now, empty for any other land
	std::vector<uint8_t> _mapLandscape;
	/// The start place of the camera that the map script of the land gave, in map units
	std::optional<glm::ivec2> _startCameraPlace;
	std::optional<new_game_choice::NewGameStart> _newGameStart;
	/// The humans of a land of a network game come from the network session
	bool _humansFromSession {false};
	/// The map script of a land of a network game runs now
	bool _runsNetworkMapScript {false};
	bool _allComputerGods {false};
	/// The ticks of the machine at the last turn, as the game reads them
	uint32_t _lastGameLoopTime {0};
	/// When the scripts last took a turn in the temple, in real time
	std::optional<uint32_t> _lastTempleTurnTime;
	std::chrono::steady_clock::duration _turnDeltaTime {0};

	/// After the script of a land of a network game: each slot whose god no human plays gets a computer god. With a
	/// network session, the land waits for its humans until the start of the game. Without one, the player of this
	/// computer is the one human.
	void FillSlotsOfNetworkLand(std::span<const PlayerNames> gods);
};

} // namespace openblack::ecs::systems
