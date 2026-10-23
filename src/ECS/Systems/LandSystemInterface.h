/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <string_view>

#include "Gui/LoadingScreenRules.h"
#include "LandChange.h"
#include "Simulation/TurnBody.h"
#include "Story/NewGameChoice.h"

namespace openblack::ecs::systems
{

/// What the path of the land reads while the testbed is loaded
inline constexpr std::string_view k_TestbedLandPath = "testbed";

/// The parts of a change of land that belong to the view of one player: the loading screen, the menu and its boxes,
/// the sheet of the players' symbols and the sounds of the land. The game's client does them. A server has no view.
class LandViewInterface
{
public:
	virtual ~LandViewInterface() = default;

	/// The loading screen starts to show, with this look
	virtual void BeginLoadingScreen(loading::LoadingClock::Mode look) = 0;
	/// A point in the load where the loading screen can show a new frame
	virtual void ShowLoadingFrame() = 0;
	/// The load is complete and the loading screen goes
	virtual void EndLoadingScreen() = 0;
	/// Closes the start-of-game question with no answer. Gives true when the question was open.
	virtual bool WithdrawSkipBox() = 0;
	/// Closes the menu before the land changes
	virtual void CloseMenuForNewLand() = 0;
	/// Makes the sheet of the players' symbols, with the symbol of the player's profile
	virtual void MakeSymbolSheet() = 0;
	/// Starts the sounds of the atmosphere and the music on the new land
	virtual void StartLandAudio() = 0;
	/// The testbed is ready for play
	virtual void TestbedLoaded() = 0;
	/// Pauses the game and asks the start-of-game question. Gives false when the view cannot ask it.
	virtual bool AskStartQuestion(bool hasCreature) = 0;
	/// The challenge file of the scripts. None for the file in its usual folder.
	[[nodiscard]] virtual std::optional<std::filesystem::path> ChallengeFile() = 0;
};

/// A view that does nothing, for a world with no player in front of it
class NoLandView final: public LandViewInterface
{
public:
	void BeginLoadingScreen(loading::LoadingClock::Mode /*look*/) override {}
	void ShowLoadingFrame() override {}
	void EndLoadingScreen() override {}
	bool WithdrawSkipBox() override { return false; }
	void CloseMenuForNewLand() override {}
	void MakeSymbolSheet() override {}
	void StartLandAudio() override {}
	void TestbedLoaded() override {}
	bool AskStartQuestion(bool /*hasCreature*/) override { return false; }
	[[nodiscard]] std::optional<std::filesystem::path> ChallengeFile() override { return std::nullopt; }
};

/// The land of the world: it loads a land, the testbed or a saved land, starts the scripts of the challenge and plays
/// the turns that are due. The game and the dedicated server use it both. The view of a player does its own parts.
class LandSystemInterface
{
public:
	virtual ~LandSystemInterface() = default;

	/// The view that does the parts of the player. With none, the land does no part of a view.
	virtual void SetView(LandViewInterface* view) = 0;

	/// Loads the map script of a land, and keeps the scripts of the challenge as they are. Gives false when the script
	/// is not there.
	virtual bool LoadMap(const std::filesystem::path& path, loading::LoadingClock::Mode look, land_change::Kind change) = 0;
	/// Starts the scripts of the challenge again from the start, then loads the map script of a land
	virtual bool LoadMapWithFreshScripts(const std::filesystem::path& path, land_change::Kind change) = 0;
	/// Loads the testbed: a flat land over the whole map with a lake, a dispenser of each miracle and no script
	virtual void LoadTestbed() = 0;
	/// Loads the network map for 32 gods on this computer alone, for tests: a round island with a citadel and a village in
	/// each slot. The village comes from a land of the game for network play. Gives false when that land is not there.
	virtual bool LoadGodsRingMap() = 0;
	/// Loads a saved game: the landscape of the saved land, then `restore` puts the saved entities in place of the land's
	/// own. Gives whether the landscape loaded and `restore` gave true.
	virtual bool LoadSavedLand(const std::filesystem::path& landPath, const std::filesystem::path& landscape,
	                           const std::function<bool()>& restore) = 0;
	/// Loads the landscape that the map script of the land names
	virtual void LoadLandscape(const std::filesystem::path& path) = 0;

	/// Whether the script of a land is one of the playground lands, which the game plays as a skirmish
	[[nodiscard]] virtual bool IsPlaygroundLand(const std::filesystem::path& path) const = 0;
	/// The map script of the land that loaded last, or the path of the testbed. Empty before a land loads.
	[[nodiscard]] virtual const std::filesystem::path& GetLandPath() const = 0;
	/// The landscape file of the land, as the map script of the land gave it
	[[nodiscard]] virtual const std::filesystem::path& GetLandscapePath() const = 0;
	/// The challenge file of the scripts
	[[nodiscard]] virtual std::filesystem::path ChallengePath() = 0;

	/// How a new game on the first land starts: the start-of-game question, or its answer at once
	virtual void SetNewGameStart(std::optional<new_game_choice::NewGameStart> start) = 0;
	/// Starts the scripts of the challenge: the script machine reads the challenge file, and the story starts if asked.
	/// Gives false when the challenge file is not there.
	virtual bool StartChallengeScripts(bool startStory) = 0;
	/// Starts the scripts of the story of a new game on the first land, and asks the player what to skip
	virtual void StartStoryScripts() = 0;

	/// Plays the turn of the world when it is due, and the own turns of the temple while the player is in it
	virtual void PlayTurns(simulation::TurnViewInterface& view) = 0;
	/// The real time between the last two turns
	[[nodiscard]] virtual std::chrono::steady_clock::duration GetTurnDeltaTime() const = 0;
	/// Every some turns, the profile of the player keeps the alignment of the player, but not on the testbed
	virtual void KeepAlignmentInProfile() = 0;
};

/// The land script that a new game starts on: the first land script of the map script. None when the map script or its
/// land script is not there.
[[nodiscard]] std::optional<std::filesystem::path> MapScriptLand();

/// Both advisors go home, the good advisor first. The advisors of a help script go from view and do not fly home.
void SendAdvisorsHome(bool helpScript);

} // namespace openblack::ecs::systems
