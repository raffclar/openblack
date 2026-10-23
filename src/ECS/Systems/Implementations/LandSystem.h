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

#include "ECS/Systems/LandSystemInterface.h"

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
	                   const std::function<bool()>& restore) override;
	void LoadLandscape(const std::filesystem::path& path) override;

	[[nodiscard]] bool IsPlaygroundLand(const std::filesystem::path& path) const override;
	[[nodiscard]] const std::filesystem::path& GetLandPath() const override { return _landPath; }
	[[nodiscard]] const std::filesystem::path& GetLandscapePath() const override { return _landscapePath; }
	[[nodiscard]] std::filesystem::path ChallengePath() override;

	void SetNewGameStart(std::optional<new_game_choice::NewGameStart> start) override { _newGameStart = start; }
	bool StartChallengeScripts(bool startStory) override;
	void StartStoryScripts() override;

	void PlayTurns(simulation::TurnViewInterface& view) override;
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

	LandViewInterface* _view {nullptr};
	NoLandView _noView;
	std::filesystem::path _landPath;
	std::filesystem::path _landscapePath;
	std::optional<new_game_choice::NewGameStart> _newGameStart;
	/// The ticks of the machine at the last turn, as the game reads them
	uint32_t _lastGameLoopTime {0};
	/// When the scripts last took a turn in the temple, in real time
	std::optional<uint32_t> _lastTempleTurnTime;
	std::chrono::steady_clock::duration _turnDeltaTime {0};
};

} // namespace openblack::ecs::systems
