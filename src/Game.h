/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <array>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <ComputerSettings.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <spdlog/common.h>

#include "3D/HandCrossFade.h"
#include "3D/HandNavigationPose.h"
#include "Common/BuildVersionFile.h"
#include "Common/LoadTimer.h"
#include "Common/PlayerProfiles.h"
#include "Common/Zoomer.h"
#include "Creature/BonusCreatures.h"
#include "Creature/CreatureFight.h"
#include "Debug/SimRun.h"
#include "ECS/StatisticsRules.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/LandSystemInterface.h"
#include "ECS/Systems/NetworkGameSystemInterface.h"
#include "EngineConfig.h"
#include "GameFrameOrder.h"
#include "GameSession.h"
#include "GameTurnOrder.h"
#include "Graphics/UploadPacer.h"
#include "Gui/EndGameBoxRules.h"
#include "Gui/EscapeKeyRules.h"
#include "Gui/LoadingScreenRules.h"
#include "Gui/MenuPause.h"
#include "Hand/HandFeel.h"
#include "Hand/HandKnock.h"
#include "Input/HandPointer.h"
#include "Input/InputLock.h"
#include "Input/ShortcutKeys.h"
#include "LandChange.h"
#include "LaunchSwitches.h"
#include "Magic/HandHoldPoser.h"
#include "Simulation/FrameBody.h"
#include "Simulation/TurnBody.h"
#include "Simulation/WorldStart.h"
#include "Story/NewGameChoice.h"
#include "Windowing/SizeFollower.h"
#include "Windowing/WindowingInterface.h" // For DisplayMode

union SDL_Event;

namespace openblack
{

namespace audio
{
class AtmosAudio;
class GameMusic;
} // namespace audio
class Camera;
class HandAnimation;
namespace gui
{
class GameInterface;
class MultiplayerFrontEnd;
class ToolTips;
class LoadingScreen;
} // namespace gui
namespace ecs::components
{
struct Transform;
}
namespace ecs::systems
{
class HandGrabSystemInterface;
}
namespace help::profile
{
struct WorldCameraView;
}
namespace online
{
class LanDiscoveryInterface;
}
namespace game_socket
{
struct Start;
}
namespace command_log
{
class Writer;
}
namespace checksum_log
{
class Writer;
}

/// The fight autopilot asked for on the command line, as the command line gives it: its policy, the seed, the lesson stage
/// to start at and the moves of a sequence
struct FightAutopilotRequest
{
	std::string policy;
	uint32_t seed {1};
	std::string stage;
	std::string sequence;
};

/// A testbed scenario asked for on the command line: its id, and for a benchmark the frames to let settle, the frames to
/// measure and where to write the results, after which the game quits
struct ScenarioRequest
{
	std::string id;
	uint32_t warmUpFrames {120};
	uint32_t frames {600};
	std::optional<std::filesystem::path> results;
	/// Without the testbed's window of scenarios over the view, as for screenshots of what a scenario shows
	bool hideWindow {false};
	/// A benchmark's crowd is measured, its results written and the game quit, as the command line asks; a scenario
	/// asked for while the game runs (from the debug inspector) only runs
	bool benchmark {true};
};

struct Arguments
{
	std::string executablePath;
	int windowWidth;
	int windowHeight;
	bool vsync;
	/// The game's graphics detail level, 0 to 6
	uint8_t detailLevel {4};
	openblack::windowing::DisplayMode displayMode;
	/// Which of the window's size and mode, the vertical sync and the detail level the command line gave: the others
	/// come from the settings of this computer when it keeps them
	computer_settings::GivenOnCommandLine givenOnCommandLine;
	GraphicsBackend graphicsBackend;
	std::string gamePath;
	float guiScale;
	uint32_t numFramesToSimulate;
	std::string logFile;
	std::array<spdlog::level::level_enum, k_LoggingSubsystemStrs.size()> logLevels;
	std::string startLevel;
	/// Start on the flat creature testbed rather than startLevel
	bool startTestbed {false};
	/// Start on the network map for 32 gods rather than startLevel
	bool startGodsRingMap {false};
	/// Log frame time statistics every so many frames, never when 0
	uint32_t frameStatsInterval {0};
	/// With the frame statistics, the GPU time of each render view
	bool frameStatsViews {false};
	/// Draw the first frame of a land once behind the loading screen, to make the renderer's pipelines there
	bool warmUpFirstFrame {true};
	/// The https:// origin of the lobby, none for no online services
	std::optional<std::string> lobbyOrigin;
	std::optional<std::string> lobbyTestAuthorities;
	/// The real-world weather changes the weather of a single-player game (on by default)
	bool useInternetWeather {true};
	bool dynamicTreeShadows {false};
	/// Tools: open the window but draw nothing on the graphics card (the no-renderer backend), so that a long run driven
	/// through the inspector does not wait on a busy graphics card
	bool noDraw {false};
	/// Tools: the window never takes the focus, the front or the mouse, and stays in a window
	bool background {false};
	/// Debug: the Creature Breeder's bonus creatures taken as unlocked (none by default, as on a normal install)
	creature_swap::Unlocked unlockedBonusCreatures;
	/// A full-screen video to play once the land is loaded: "intro", "fall" or a path under the game's folder
	std::string playVideo;
	/// Debug: the 16-bit surface of the films, "555", "565" or "none". Empty keeps the surface of the display cards of
	/// today
	std::string filmSurface;
	/// Play the pre-intro film at start-up, as on a first run
	bool preIntro {false};
	/// Leave out the logo pictures at start-up
	bool skipLogos {false};
	/// Play the logo effect of the first game, a run of a developer or a tool too
	bool logoEffect {false};
	/// Start a new game rather than continue from the save on exit
	bool newGame {false};
	/// Start as a plain start does, a run of a developer or a tool too: continue from the save on exit
	bool continueGame {false};
	/// The switches of the original game's command line, such as -NEWGAME
	launch_switches::Switches launchSwitches;
	/// The developer options that a build without the developer tools ignored, with their values, for the log
	std::vector<std::string> ignoredDeveloperOptions;
	/// For developers and agents: how a new game on the first land starts, asked the start-of-game question whatever
	/// the player's profiles, or with it answered at once; as the game decides when none
	std::optional<new_game_choice::NewGameStart> newGameStart;
	/// A testbed scenario to run as the game starts, by its id, and how to measure its crowd if it has one
	std::optional<ScenarioRequest> scenario;
	std::optional<std::pair</* frame number */ uint32_t, /* output */ std::filesystem::path>> requestScreenshot;
	/// The port of 127.0.0.1 the debug inspector's server listens on, in builds with it; none to not start it
	std::optional<uint16_t> inspectPort;
	/// The fight autopilot to start with the inspector, and the folder for the files of the fight transcript
	std::optional<FightAutopilotRequest> fightAutopilot;
	std::optional<std::filesystem::path> fightTranscriptFolder;
	/// A tool run in the fast simulation mode, or with a run record; none for the usual game
	std::optional<sim_run::Request> simulation;
	/// Where the inspector keeps pictures by feature, with their catalogue; OPENBLACK_SCREENSHOT_ROOT's or
	/// E:/openblack/screenshots when not given
	std::optional<std::filesystem::path> screenshotRoot;
	/// The seed of every random number the game draws, and the date pinned, for a deterministic run; none for a seed of
	/// the machine's and the wall clock's date
	std::optional<uint32_t> seed;
	/// Tools: play the world as a network game does, with no network: the clock never stops for the player, and the
	/// turns read no part of a frame
	bool networkGame {false};
	/// Join a game server once the land loaded: the world then turns only with the turns of the server
	std::optional<ecs::systems::NetworkJoin> networkJoin;
	/// How the player's mouse and keyboard are kept out while the inspector drives the game: while a client is
	/// connected, from the start, or never
	input::LockMode inspectInputLock {input::LockMode::Auto};
	/// Where the players' profiles and their creatures are kept instead of the game's own folder, as for a first start on
	/// a clean folder; none for the game's own (or, for a developer's or a tool's run, a profile of its own)
	std::optional<std::filesystem::path> profileFolder;
};

/// The game on this computer: the window, the view of the player and the loop of the frames. The world turns in the
/// simulation, with the game as the view of its turns and of the land's loads.
class Game:
    public simulation::TurnViewInterface,
    public ecs::systems::LandViewInterface,
    public simulation::FrameViewInterface,
    public simulation::WorldDataViewInterface,
    public simulation::PlayStartViewInterface
{
public:
	/// Makes the sheet of the players' symbols that the world shows, with the symbol of the player's profile. A
	/// testbed scenario can give another symbol in place of the profile's.
	void MakePlayersSymbols(std::optional<uint32_t> symbol = std::nullopt);
	static constexpr auto k_TurnDuration = std::chrono::milliseconds(100);
	/// About how many bytes of files the loading threads read at a time, and how much of what they make may reach the
	/// graphics card each frame, so no frame waits long for it: sixteen buffers or textures keep the renderer's share of a
	/// loading frame to about 6 ms on Vulkan, where some models' hundreds of small parts made it 17 ms
	static constexpr size_t k_FrameLoadBudget = simulation::k_FrameLoadBudget;
	static constexpr graphics::UploadPacer::Allowance k_FrameUploads = simulation::k_FrameUploads;
	/// How many bytes of files the loading threads read at a time while the loading screen shows. No game frame waits
	/// then, so the threads read as much as they can.
	static constexpr size_t k_LoadingScreenLoadBudget = simulation::k_LoadingScreenLoadBudget;
	static constexpr float k_TurnDurationMultiplierSlow = 2.0f;
	static constexpr float k_TurnDurationMultiplierNormal = 1.0f;
	static constexpr float k_TurnDurationMultiplierFast = 0.5f;
	/// The height of the hand at standard size, which the game pulls the hovering hand back from the land by
	static constexpr float k_HandHeight = 3.2f;
	/// The game's limits on how far the hand is from the camera
	static constexpr float k_HandMinDistance = 2.0f;
	static constexpr float k_HandMaxDistance = 1800.0f;
	/// Inside the temple: how far short of the room the hand hangs, its limits on how far it is from the camera, and
	/// how long it takes to turn to the surface the cursor is on
	static constexpr float k_HandTempleGap = 3.25f;
	static constexpr float k_HandTempleMinDistance = 4.0f;
	static constexpr float k_HandTempleMaxDistance = 300.0f;
	static constexpr float k_HandTempleTurnTime = 0.4f;
	/// Land lower than this is the sea, which the hand rests on
	static constexpr float k_HandSeaAltitude = 0.1f;
	/// Seconds the hand takes to hold its distance from the camera as it drags by the edge of the screen
	static constexpr float k_HandHoldTime = 0.4f;

	explicit Game(Arguments&& args) noexcept;
	virtual ~Game() noexcept;

	bool ProcessEvents(const SDL_Event& event) noexcept;
	/// The steps of the game turn that the view of the player does: the sounds, the music, the tooltips and the help
	void PlayViewStep(game_turn::Step step, glm::vec3 cameraPosition) override;
	/// The frames each second that the statistics of the game keep
	[[nodiscard]] int32_t FrameRate() const override { return _statisticsFrameRate.Rate(); }
	/// The audio of a turn of the temple
	void PlayTempleAudioTurn() override { ProcessTempleAudioTurn(); }
	void CatchUpFrame() override;
	/// The parts of the land's loads that the view of the player does
	void ShowLoadingFrame() override;
	bool WithdrawSkipBox() override;
	void MakeSymbolSheet() override;
	void StartLandAudio() override;
	void TestbedLoaded() override;
	void StartCameraAtTemple() override;
	bool AskStartQuestion(bool hasCreature) override;
	[[nodiscard]] std::optional<std::filesystem::path> ChallengeFile() override;
	/// The parts of the effects of a frame that the view of the player does
	void UpdateAdvisors() override;
	[[nodiscard]] bool TakeDialogueClick() override;
	[[nodiscard]] bool DialogueSkipKeyDown() const override;
	void UpdateSingAlong(bool inTemple) override;
	[[nodiscard]] bool DialogueWordsDrawn() const override;
	void CloseMenuForCinema() override;
	/// The parts of the start of the world that the view of the player does
	void StartupStep(std::string_view step) override;
	[[nodiscard]] const gui::TextDatabase* MakeInterface() override;
	void StartPlayView() override;
	void BeforeFirstFrame(bool continued) override;
	/// What the steps of one frame share
	struct FrameState
	{
		/// The real time of the frame, or the fixed time of a frame that the inspector steps
		std::chrono::microseconds deltaTime {0};
	};
	/// One step of the frame. It returns false when the game must stop.
	bool PlayFrameStep(game_frame::Step step, FrameState& frame);
	/// Whether this frame moves the camera, puts the hand under the cursor and draws the world: not while the window is
	/// minimised, unless a picture of the frame is necessary
	/// The state of the window and the run for the steps of the graphics in this frame
	[[nodiscard]] game_frame::GraphicsWindow FrameGraphicsWindow() const;
	[[nodiscard]] bool DoesFrameGraphics() const;
	/// The clock of the frame, the debug inspector and the time that the frame takes
	void StartFrame(FrameState& frame);
	/// The keys, the mouse buttons and the pointer of the frame
	void UpdateInput();
	/// The shortcut keys of this frame that change the world send their commands
	void SubmitKeyCommands();
	/// The debug windows. It returns false when the game must stop.
	bool UpdateDebugGui();
	/// The editor's camera, the creature's camera and the creature's cave
	void UpdateCameraControllers(std::chrono::microseconds deltaTime);
	/// The step of the camera after the turns, and the views that put it
	void UpdateCamera();
	/// The temple, whose camera can take the player out of it
	void UpdateTemple(std::chrono::microseconds deltaTime);
	/// The menus and the dialogs of the interface take the input of the frame
	void UpdateInterfaceFrame(std::chrono::microseconds deltaTime);
	/// The turn of the game when one is due. It returns false when the game must stop.
	bool PlayTurns();
	/// The draw starts: the creatures get their poses, before the hand moves
	void UpdateCreaturePoses();
	/// The effects of the frame that the draw moves on, by the frame's game time, after the hand moved
	void UpdateFrameEffects();
	/// The web client and the lobby client do their work of the frame
	void UpdateOnline();
	/// The end of a skirmish or a network game: the boxes that the turns of the frame open, and the player's choice in the
	/// end box
	void FollowGameEnd();
	/// The numbers and the marks that rise over things
	void UpdateScreenMarks();
	/// The hand goes under the cursor, as the camera of this frame sees it, before the draw
	void UpdateHandFrame(std::chrono::microseconds deltaTime);
	/// Once a turn, before the block of commands goes: reports where the hand and the camera are, when they changed
	void ReportHand();
	/// The things in view are made ready for the draw of this frame
	void UpdateEntitiesFrame(std::chrono::microseconds deltaTime);
	/// The keys of the camera, after the camera moved. They move the camera in the next frame.
	void UpdateCameraKeys(std::chrono::microseconds deltaTime);
	/// The game's music, for a turn of the world or of the temple
	void ProcessMusicTurn(glm::vec3 cameraPosition, bool inCitadel);
	/// The paused, slower and quicker words at the top of the screen, a frame on by the real time
	void UpdatePauseText();
	/// The audio of a turn inside the temple, while the world is paused
	void ProcessTempleAudioTurn();
	/// The keys that take the player into the temple's rooms, from outside it or within
	void ProcessTempleRoomKeys();
	bool Update() noexcept;
	bool Initialize() noexcept;
	/// Runs the game to its end: the start, the frames, and the end
	bool Run() noexcept;

	/// What the start of a run gives
	enum class RunStart
	{
		Play,   ///< The game is ready for its frames
		Done,   ///< The game did all of its work before its first frame, with no error
		Failed, ///< The game cannot start
	};
	/// The part of Run before the first frame: the first land, the scripts and the start of the game loop
	[[nodiscard]] RunStart StartRun() noexcept;
	/// One frame of the game loop. Returns false when the game ends after this frame.
	[[nodiscard]] bool RunFrame() noexcept;
	/// The part of Run after the last frame, as the player leaves the game
	void EndRun() noexcept;

	/// Loads a land's map script, keeping the challenge's scripts running, as the story's own change of land does. The
	/// tips screen shows while it loads, as for a new game; a script's own map command shows the "please wait" banner
	/// instead, after five seconds
	bool LoadMap(const std::filesystem::path& path, loading::LoadingClock::Mode look = loading::LoadingClock::Mode::Tips,
	             land_change::Kind change = land_change::Kind::Story) noexcept;
	/// Loads a land as the land menu does: the challenge's scripts start again from scratch before the land loads
	bool LoadMapWithFreshScripts(const std::filesystem::path& path,
	                             land_change::Kind change = land_change::Kind::FreshScripts) noexcept;
	/// A land picked from the land menu: a playground land starts a skirmish, a new game on it; any other land is loaded
	/// with the challenge's scripts started again
	bool LoadFromLandMenu(const std::filesystem::path& path) noexcept;
	/// Whether a land's script is one of the playground lands, played as a skirmish
	[[nodiscard]] static bool IsPlaygroundLand(const std::filesystem::path& path);
	void LoadLandscape(const std::filesystem::path& path);
	/// Loads the testbed: a flat plane over the whole map, with a lake north of the middle and nothing on it, for trying
	/// out creatures
	void LoadTestbed() noexcept;
	/// Loads the network map for 32 gods on this computer alone, for tests: a round island with a citadel and a village
	/// in each slot. The village comes from a land of the game for network play. Returns false when that land is not there.
	bool LoadGodsRingMap() noexcept;
	/// Loads a saved game: the saved land's landscape with no map script (the flat land for the testbed), then `restore`
	/// puts the saved entities in place of the land's own. The systems then find their entities again and the land starts.
	/// Returns whether the landscape loaded and `restore` gave true.
	bool LoadSavedLand(const std::filesystem::path& landPath, const std::filesystem::path& landscape,
	                   const std::function<bool()>& restore) noexcept;
	/// Starts a new game on the first land from scratch, as the game's new game does, the story's scripts starting
	/// again; for developers and agents, `start` asks the start-of-game question or answers it at once
	bool StartNewGame(std::optional<new_game_choice::NewGameStart> start) noexcept;
	/// The land script that a new game starts on: the first LOAD_FEATURE_SCRIPT of the map script in the Scripts folder,
	/// as the game reads it. Gives nothing when the map script or its land script is not there.
	[[nodiscard]] std::optional<std::filesystem::path> MapScriptLand() const;
	/// Starts the game as a plain start of the program does: the save on exit of the profile loads, and the menu shows
	/// over it. With no save on exit, a new game starts. Returns whether the save loaded.
	bool ContinueFromExitSave() noexcept;
	/// The logo effect of the first game of the program, in its own loop with its sound, until it ends, a key or a mouse
	/// button. Then black frames while a mouse button is held.
	void PlayLogoEffect();

	void SetTime(float time) noexcept;
	/// How many times longer a turn takes: 2 is half speed
	void SetGameSpeed(float multiplier);
	[[nodiscard]] float GetGameSpeed() const;

	[[nodiscard]] uint32_t GetTurn() const;
	[[nodiscard]] bool IsPaused() const;
	/// This computer's hand is drawn this frame: no menu is open over the game and the interface is not given over to a
	/// cinematic
	[[nodiscard]] bool IsHandDrawn() const override;
	/// The scenario asked for on the command line, once: the scenarios' window runs it as the game starts
	/// The game ends after this frame. Unlike the window's events, which say each time whether to go on, nothing takes
	/// it back
	void RequestQuit() { _quitRequested = true; }
	[[nodiscard]] std::optional<ScenarioRequest> TakeScenarioRequest() { return std::exchange(_scenarioRequest, std::nullopt); }
	/// A scenario asked for that the scenarios' window hasn't started yet
	[[nodiscard]] const std::optional<ScenarioRequest>& PendingScenario() const { return _scenarioRequest; }
	/// Asks for a scenario while the game runs: the scenarios' window runs it on a fresh testbed next frame. A build
	/// without the developer tools, and a live network game, refuse it: then the result is false.
	bool RequestScenario(ScenarioRequest request);
	[[nodiscard]] std::chrono::duration<float, std::milli> GetDeltaTime() const;
	[[nodiscard]] const glm::ivec2& GetMousePosition() const { return _mousePosition; }
	/// Puts the cursor the game works with somewhere in the window, until the mouse next moves
	void SetMousePosition(glm::ivec2 position)
	{
		_mousePosition = position;
		_handPointer.Moved();
	}
	[[nodiscard]] const audio::AtmosAudio* GetAtmosAudio() const { return _atmosAudio.get(); }
	[[nodiscard]] audio::GameMusic* GetGameMusic() { return _gameMusic.get(); }
	[[nodiscard]] const audio::GameMusic* GetGameMusic() const { return _gameMusic.get(); }
	[[nodiscard]] const HandAnimation* GetHandAnimation() const { return _handAnimation.get(); }

	/// The frame being made is written to a PNG once drawn, without the debug windows if asked
	void RequestScreenshot(const std::filesystem::path& path, bool hideDebugGui = false) noexcept;
	/// The frame being made leaves the debug windows out, as a picture without them does, for the frames around one
	void HideDebugGuiThisFrame() noexcept { _debugGuiHiddenFrame = _frameCount; }
	/// Where the inspector keeps pictures, when given on the command line
	[[nodiscard]] const std::optional<std::filesystem::path>& GetScreenshotRoot() const noexcept { return _screenshotRoot; }
	/// The game's own interface (its menu), once it is made
	[[nodiscard]] gui::GameInterface* GetInterface() noexcept { return _interface.get(); }
	/// The multiplayer front end with the game's own services: the lobby client, the game socket and the search of
	/// the local network
	[[nodiscard]] std::unique_ptr<gui::MultiplayerFrontEnd> MakeMultiplayerFrontEnd();
	/// Join Online Game: the player goes into the temple's future room, and the front end's first box shows over
	/// it. Returns false when the game has no interface or no temple.
	bool OpenMultiplayerFrontEnd(std::unique_ptr<gui::MultiplayerFrontEnd> frontEnd);
	/// The size of the villagers' names display, 0 to 1
	[[nodiscard]] float GetVillagerNameFade() const noexcept { return _villagerNameFade; }
	/// The map script of the land loaded last, or "testbed"; empty before one is
	[[nodiscard]] const std::filesystem::path& GetLandPath() const noexcept;
	/// The land's file, as the land's script asked for it
	[[nodiscard]] const std::filesystem::path& GetLandscapePath() const noexcept;

	static Game* Instance() { return sInstance; }

	/// The situation that Escape in play finds now, with these modifier keys held
	[[nodiscard]] escape_key::Situation EscapeSituation(uint16_t keyModifiers) const;

private:
	static Game* sInstance;

	/// What the game shows as it starts, before it loads the rest: the logo pictures, the pre-intro film on a first run
	/// and the tips screen fading in, which stays up while the game loads
	void PlayStartupScreens();
	/// Once the inspector has started: the folder of the fight transcript and the fight autopilot from the command line
	void StartFightTools();
	/// The recorded input of a tool's run and the record of the input, from the first frame of the next land
	void StartInputTools();
	/// The checksum of each turn and the commands to play of a tool's run, from the command line. These need no inspector.
	void StartSimulationLogs();
	/// The logo film's two pictures, each fading in and out, any key or mouse button hurrying them on
	void PlayLogoScreens();
	/// The pre-intro film in its own loop with the trailer music, until its last frame or a key
	void PlayPreIntro();
	/// Picks the tip of the day and makes its picture; the first time, fades the tips screen in over a second
	void StartTipOfTheDay();
	/// The tips screen, or the banner, shows while a land loads, drawn again at the loading's redraw points
	void BeginLoadingScreen(loading::LoadingClock::Mode look) override;
	/// A redraw point inside the loading: the tips screen is drawn and shown if it is due, or at once when drawNow is true.
	/// drawUnder, when given, draws first in a frame that is shown. Returns whether a frame was shown.
	bool RenderLoadingFrame(bool drawNow = false, const std::function<void()>& drawUnder = nullptr);
	/// The loading is over: its tip goes before the first frame of the game
	void EndLoadingScreen() override;
	/// Waits behind the loading screen until the loading threads have loaded all the prefetched resources. Then it does
	/// the work of the first frame that the game can do before, and draws one more loading frame.
	void FinishLoadingBehindScreen();
	/// Does behind the loading screen the work that the first frames of a land do once: the land's shadows, the land
	/// where creatures can walk, and the particle files
	void PrepareFirstFrames();
	/// Draws the first frame of the land under a loading frame, which hides it, so that the renderer makes the
	/// pipelines and render targets of the scene behind the loading screen. Nothing that the game's frames use changes.
	void WarmUpFirstFrame();
	/// Draws one frame outside the game's loop over a black screen and shows it
	void PresentStartupFrame(const std::function<void(glm::u16vec2 resolution)>& draw);
	/// Draws at the window's size from this frame on if it changed: the back buffer, the views over the whole screen,
	/// the sea's reflection and the camera's shape follow it, on whichever screen is up
	void FollowWindowSize();
	/// Sets the scene's view and the sea's reflection to the window's size, as the game's frames draw them
	void SizeSceneToWindow() override;
	/// Whether the player has a profile. A first start has none: the pre-intro plays, the tips start from the first, the
	/// intro can't be skipped and the new player's box asks for a name before the first land
	[[nodiscard]] static bool HasPlayerProfiles() noexcept;
	/// Where the players' profiles are kept for this run
	[[nodiscard]] player_profiles::Store ProfileStore() const;
	/// A first start: the new player's box asks for the player's name before the first land loads. False when the
	/// window was closed meanwhile
	bool AskForNewPlayer();
	/// The name of the profile played, for the files that the game keeps for its player. There is no name on a run of a
	/// developer, which keeps no files for its player.
	[[nodiscard]] std::optional<std::u16string> PlayerForProfileFiles() const;
	/// The help record of the player comes from their folder in the profile folder
	void LoadHelpRecord();
	/// The game writes the help record of the player to their folder in the profile folder
	void SaveHelpRecord();

	/// When the game was launched
	const LoadTimer::Clock::time_point _launchTime {LoadTimer::Clock::now()};
	/// Times the start of the game, from its launch to the first frame drawn on the first land
	std::optional<LoadTimer> _startupTimer;
	/// The size drawn at, following the window's
	windowing::SizeFollower _windowSize;
	/// The openblack commit of this build, from the version file next to the game
	build_version::BuildVersion _buildVersion;
	/// Times the loading of every resource registered at the start, on the loading threads or where first wanted
	std::optional<LoadTimer> _prefetchTimer;

	/// path to Lionhead Studios Ltd/Black & White folder
	const std::filesystem::path _gamePath;

	std::filesystem::path _startMap;
	std::string _playVideo;
	std::string _filmSurface;
	bool _preIntro {false};
	bool _skipLogos {false};
	bool _logoEffectAsked {false};
	std::optional<new_game_choice::NewGameStart> _newGameStart;
	/// The command line asked for a new game, or to continue as a plain start does
	bool _newGameAsked {false};
	bool _continueAsked {false};
	/// The switches of the original game's command line
	launch_switches::Switches _launchSwitches;
	std::optional<std::filesystem::path> _challengePath;
	/// The screens drawn as the game starts and while a land loads, none without a window
	std::unique_ptr<gui::LoadingScreen> _loadingScreen;
	/// What is drawn at the loading's redraw points, nothing while not loading
	std::optional<loading::LoadingClock::Mode> _loadingLook;
	/// The tips screen has faded in once: later loads show it at once
	bool _tipFadedIn {false};
	bool _startTestbed {false};
	bool _startGodsRingMap {false};
	std::optional<ScenarioRequest> _scenarioRequest;
	/// The port the debug inspector is to listen on, if it is to start
	std::optional<uint16_t> _inspectPort;
	std::optional<FightAutopilotRequest> _fightAutopilot;
	std::optional<std::filesystem::path> _fightTranscriptFolder;
	std::optional<sim_run::Request> _simulation;
	/// The file that a tool's run writes the commands of each turn to
	std::unique_ptr<command_log::Writer> _commandLog;
	/// The file that a tool's run writes the checksum of each turn to
	std::unique_ptr<checksum_log::Writer> _checksumLog;
	/// Where the players' profiles are kept instead of the game's own folder
	std::optional<std::filesystem::path> _profileFolder;
	/// Started by a developer or a tool, which plays as a profile of its own and never sees the new player's box
	bool _automatedRun {false};
	/// A run of a developer plays as a profile that the game does not keep
	bool _developerProfile {false};
	std::optional<std::filesystem::path> _screenshotRoot;
	std::optional<uint32_t> _seed;
	bool _networkGame {false};
	std::optional<ecs::systems::NetworkJoin> _networkJoin;
	/// The game joined a game server from the command line, and stops when that network game ends
	bool _joinedFromCommandLine {false};
	/// A join from the command line waits for the server's land: the land of the start of the game only stands in for it
	bool _networkLandPending {false};
	/// The out-of-sync box was asked; the player answered No to it
	bool _outOfSyncAsked {false};
	bool _leaveNetworkGame {false};
	/// What the end boxes of this computer keep between turns
	end_game_box::State _endGameBoxState;
	/// The -MULTIPLAYER switch opens the multiplayer front end before the first frame
	bool _frontEndOnStart {false};
	input::LockMode _inspectInputLock {input::LockMode::Auto};
	/// Whether the testbed opens its window of scenarios
	bool _testbedWindow {true};
	bool _quitRequested {false};
	/// The player closed the window. The game ends at the end of a frame outside a script's cut scene.
	bool _closeAsked {false};

	uint32_t _frameCount {0};
	/// The frame rate the game's statistics take, over a few seconds of frames drawn
	statistics::FrameRateMeter _statisticsFrameRate;
	/// The state that the frames of the game loop keep from one frame to the next, from the start of the loop to its end
	struct FrameLoop;
	std::unique_ptr<FrameLoop> _frameLoop;
	glm::ivec2 _mousePosition {0, 0};
	/// Whether the hand follows the pointer the window reports yet, or until the mouse first moves the game's own
	input::HandPointer _handPointer;
	/// The left button went down since the last frame, which may click the dialogue on
	bool _dialogueClick {false};
	/// Gripping the land with the mouse, which the hand only does once a button is pressed: it starts out not gripping
	bool _handGripping {false};
	/// The select button holds the land, not on a creature and not for a fight, and the other button is held too or not
	bool _handSelectGrip {false};
	bool _handOtherButton {false};
	/// Whether the last press of the Action button went to letting go of a miracle in the hand or to a creature, so it
	/// taps nothing else for the leash
	bool _actionPressTaken {false};
	/// The button whose press directs the player's creature's fight, until it is let go
	std::optional<creature_fight::Button> _fightButton;
	bool _handRotating {false};
	/// The hand sits on the line of sight through the cursor, this far from the camera
	glm::vec3 _handRayDirection {0.0f, -1.0f, 0.0f};
	float _handDistance {k_HandMinDistance};
	Zoomer _handHoverZoomer;
	bool _handWasGripping {false};
	/// Where the hand is, before it is faded from where it was
	glm::vec3 _handPosition {0.0f, 0.0f, 0.0f};
	/// Dragging the land, and the pose the camera's hints give the hand
	bool _handCameraState {false};
	/// The hand grips the land for its tooltips: from the start of a drag of the land until the Move button is let go,
	/// also while the Action button is held too
	bool _tipsLandGrip {false};
	hand_navigation_pose::Pose _handPose {hand_navigation_pose::Pose::Idle};
	/// The rotate arrow's turn, kept from frame to frame, which a demonstration's recording sets too
	float _triconAngle {0.0f};
	/// How far from the camera the hand holds while it drags by the edge of the screen
	Zoomer _handHoldZoomer;
	/// The land the hand grips while it drags it, and where the hand was when it gripped
	glm::vec3 _handGripPoint {0.0f, 0.0f, 0.0f};
	/// The fade from where the hand was to where it is now held, as it grips the land or lets go
	HandCrossFade _handCrossFade;
	/// The hand's tap after it knocked on a house
	hand_knock::Tap _handKnock;
	/// Where the pointer was last put on the hand while it holds a town's totem
	std::optional<glm::ivec2> _totemPointer;
	/// The hand plays its tap this frame
	bool _handKnocking {false};
	/// The way the surface the cursor is on in the temple faces, which the hand turns to
	Zoomer3 _handTempleNormal {glm::vec3(0.0f, 1.0f, 0.0f)};
	/// The options screen's one-press actions: the temple and realm keys, the villagers' names and details
	input::ShortcutKeys _shortcutKeys;
	/// Which way the hand faces across the land, and its up, easing to the slope under it
	glm::vec3 _handHeading {0.0f, 0.0f, 1.0f};
	/// The same heading as an angle about the up axis, from -z towards +x, which turns the frame of a held thing and the
	/// hand that holds a totem
	float _handHeadingAngle {std::numbers::pi_v<float>};
	Zoomer3 _handUp {glm::vec3(0.0f, 1.0f, 0.0f)};
	/// How the hand stands facing along its heading on level land, from which it is stood on a slope or a creature's body
	glm::mat3 _handOnLevelLand {1.0f};
	int _handLastCursorX {0};
	/// Whether the hand held its up last frame, gripping the land
	bool _handUpWasHeld {false};
	/// Whether the cursor is on a thing rather than the land or the sea
	bool _cursorOnObject {false};
	/// How far the villagers' names display has grown in, 0 to 1
	float _villagerNameFade {0.0f};
	/// The size of the names display that the labels of this frame use: the size before the step of this frame
	float _villagerNameFadeDrawn {0.0f};
	/// The up of the face of a model the hand rests on, none over the land or what the hand doesn't feel
	std::optional<glm::vec3> _handSurfaceUp;
	/// The hand rests over a thing picked under the cursor
	bool _handOverObject {false};
	/// Where the cursor points at in the world, on the landscape or the sea
	std::optional<glm::vec3> _cursorWorldPosition;
	/// Where the hand started, where a tool's run can keep it (sim_run::Settings::stillHand)
	std::optional<glm::vec3> _stillHandAt;
	/// The last report of the hand and the camera that this computer sent, its hand and its turn
	/// The values are as in player_commands::HandReport: the collide point, the hand, the camera and its focus
	std::array<glm::vec3, 4> _lastHandReport {};
	entt::entity _reportedHand {entt::null};
	/// The thing under the cursor that the last player_commands::HandPick sent, in a network game
	entt::entity _lastHandPick {entt::null};
	std::optional<uint32_t> _handReportTurn;
	/// The land or sea under the cursor, whatever thing is in front of it
	std::optional<glm::vec3> _cursorLand;
	/// Where the hand rests on or before the thing under the cursor, which it hovers at rather than over the land
	std::optional<glm::vec3> _handRestOnThing;
	/// How much nearer to the camera than its resting point the hand stays, for a miracle's icon
	float _handRestNearer {0.0f};
	/// The cursor is on a miracle's icon: the hand opens to take it
	bool _handOverSpellIcon {false};
	/// How far along the line of sight the help's text bubble is, while the bubble has the cursor
	std::optional<float> _handBubbleDistance;
	/// The help's text bubble or the tied leash's rope has the cursor: the hand stands straight up
	bool _handStandsStraight {false};
	/// Nothing is under the cursor but the land or the sea, so the hand may stand towards the camera's focus to offer to
	/// turn the camera
	bool _handMayOfferTurn {false};
	/// The cursor's point at the furthest the hand may be this frame, which the hand stands towards the camera's focus
	/// from when it offers to turn the camera
	std::optional<glm::vec3> _handLimitPoint;
	/// Where the hand is and how it is posed while it is held to a creature
	std::optional<ecs::systems::CreatureHandSystemInterface::HandPose> _handOnCreature;
	/// The creature the hand is over this frame, if any
	std::optional<entt::entity> _creatureUnderHand;
	/// Grabbing the sea plays G_HandInWater_01 to _10 in turn
	uint32_t _handInWaterSample {0};
	/// Where the hand gripped the land since the last turn. The grip plays its sound at once, and the next turn makes its
	/// puff of dust. Thus a grip while the game is paused makes its dust when the game continues.
	std::vector<glm::vec3> _gripDust;

	/// Plays the sound of the hand grabbing the land or the sea at the grab point, as the game does
	void PlayHandGrabSound();
	/// What the hand steps by this frame, in seconds
	[[nodiscard]] float HandStepSeconds() const;
	/// What the camera and the hand step by this frame: the frame's real time in whole milliseconds, or the game's time
	/// while a script holds the widescreen
	[[nodiscard]] std::chrono::milliseconds CameraStepTime() const;
	/// The miracles hear where the hand and cursor are, and the held miracle follows the hand
	void UpdateMagicHand(const glm::vec3& handPosition, float deltaSeconds);
	/// The hand's debugging trace keeps the frame: where the hand was placed and drawn, where it shows, on the screen
	/// too, and what it was doing
	void EndHandTrace(const glm::vec3& placed, const ecs::components::Transform& hand, hand_blend::HandMode mode,
	                  const ecs::systems::HandGrabSystemInterface* grab);
	/// The gestures drawn with the cursor this frame, through the gesture system
	void UpdateGestures(const Camera& camera, glm::ivec2 screenSize, float deltaSeconds);
	/// Turns the hand to face along the line of sight through the cursor and stands it on the slope under it
	void OrientHand(ecs::components::Transform& handTransform, const glm::mat3& facingCamera, glm::vec3 surfaceUp,
	                float deltaSeconds);
	/// Decides how the hand moves this frame, gripping the land, held as it drags by the edge, or hovering, and its pose
	void UpdateHandNavigation(const ecs::components::Transform& handTransform);
	/// The camera's helper icons by the hand while a tutorial demonstration plays, from where the hand is drawn; none
	/// while the hand is put away
	void UpdateHandTricons(bool handDrawn, float seconds, hand_blend::HandMode mode);
	/// Places the hand on the line of sight through the cursor the way the game does
	void PlaceHand(ecs::components::Transform& handTransform, float deltaSeconds);
	/// What sets how far from the camera the hand may hover this frame, without where it rests
	[[nodiscard]] hand_feel::Hover HandHoverInputs(glm::vec3 eye) const;
	/// The drawn size of the hand at a place, from its distance to the camera, larger on a creature
	[[nodiscard]] float HandDrawnScale(glm::vec3 eye, glm::vec3 handPosition, bool holding) const;
	/// A knock on a house: the hand's tap starts, waits while the hand holds something, and the houses' read-out runs
	void UpdateHandKnock(const ecs::components::Transform& handTransform);
	/// Loads the hand animations of Data/CTR/hh.hbn for the hand mesh
	void LoadHandAnimation() override;
	/// Acts on what the player chose in the game's menu: continuing restores the pause it had before it opened
	/// The challenge file: in its usual folder, or in the folder that the settings file of the -SETTINGS: switch names.
	/// The game reads the settings file one time, at the first call.
	[[nodiscard]] const std::filesystem::path& ChallengePath();
	void HandleInterfaceAction();
	/// The main menu's tattoo editor: its button shows while it may, and the creature it shows is posed each frame
	void UpdateTattooMenu(float milliseconds);
	/// The multiplayer front end ends when the player leaves its first box, or when the temple closes. When the game
	/// server starts the game that the player joined, the front end closes and the server's land loads.
	void UpdateMultiplayerFrontEnd();
	/// Loads the land of a network game's start with the server's seed, as the game server loaded it
	bool StartNetworkLand(const game_socket::Start& start);
	/// The land's start-of-game question that waits while the multiplayer front end is open: whether the profile has a
	/// creature
	std::optional<bool> _startQuestionAfterFrontEnd;
	/// The renderer draws the creature that the main menu's tattoo editor shows, in the next frame of the scene
	void ShowTattooMenuCreature() const;
	/// Closes the menu before a land change. The game then gets the pause that it had before the menu opened.
	void CloseMenuForNewLand() override;
	/// The options the player keeps with their profile take effect: the hand, the help, the texts and the tooltips
	void ApplyPlayerSettings(const player_profiles::Settings& settings);
	/// The game and the Controls page take the bindings of the profile played
	void LoadProfileKeys();
	/// The profile played keeps the game's bindings
	void KeepProfileKeys();
	/// The Players page lists the profiles, the one played picked
	void RefreshPlayersPage();
	/// Another profile is played: the menu and the options follow it
	void PlayerProfileChanged();
	/// The player's creature takes the name the profile played gives it
	void NameCreatureAfterProfile();
	/// Reads the current profile's own pictures for the designs of the symbols and the tattoos into the tattoo editor
	void ReadTattooPictures();
	/// The new player's box over the game, from the Players page
	void AskForAnotherPlayer(bool canCancel);
	/// The menu ended this game. The save on exit goes to its profile, and the next game starts once the save has its
	/// picture.
	void EndGame(game_session::End end);
	/// Once a frame, after the pictures of the saves came back: the next game starts when the game that ended has its
	/// save on exit complete
	void StartNextGameWhenSaved();
	/// The profile that the save on exit goes to as this game ends
	[[nodiscard]] game_session::ExitSave ExitSaveAsGameEnds(game_session::End end) const;
	/// The save on exit into the profile that the rules give. With the previous profile, the game plays as it while it
	/// saves, and keeps the profile picked in _profileAfterSave. Returns whether the game saved.
	bool SaveAsGameEnds(game_session::End end, bool quitting);
	/// A game starts as a continue: the save on exit of the profile loads, and the menu shows over it. With no save on
	/// exit that the game can open, or a load that fails, a new game starts. Returns whether a land is open.
	bool ContinueGame();
	/// The menu shows over a game that continued, which pauses the game, as Escape opens it
	void ShowMenuOnStart();
	/// Escape in play with no dialog open: the menu opens, unless a script's cut scene is on, and a help script holding
	/// the dialogue stops
	void HandleEscapeInPlay(uint16_t keyModifiers);
	/// Once a frame outside the temple: where the hand's tooltip is drawn, and the status panel of the creature the hand
	/// is held to, or over
	void UpdateHandInterface();
	/// The interface's pick of what is under the cursor, at the frame as it is about to be drawn
	void PickUnderCursor(float seconds);
	/// The pick's last step, once the frame's interface is drawn
	void ResolveScreenShapes();
	/// The box of a save or a load, drawn and shown at once as a frame of its own over the last picture
	void ShowSavingBox(bool loading, bool darken);
	/// Whether the hand hovers over a thing: a one-shot bubble, or anything with a model that the cursor picks
	[[nodiscard]] bool HandOverThing() const;
	/// Whether the keys of the camera's bookmarks work: not while a script has the cinema bars in, and only while the
	/// script's interface level gives the player the keys of the camera and of its places
	[[nodiscard]] static bool BookmarkKeysAllowed();
	/// Whether a front-end box shows over the game: the game's menu, the question at the start of a new game, or the
	/// tattoo editor
	[[nodiscard]] bool FrontEndBoxShows() const;
	/// The view of the player's world camera for the help, none when the world camera does not show the world. The
	/// camera that watches a creature fight is part of the world camera.
	[[nodiscard]] static std::optional<help::profile::WorldCameraView> HelpWorldCameraView();
	/// The near plane for the camera where it is now
	void FitNearClip();
	/// Shows the camera where the inspector wants the frame drawn from (an override, a picture's), or gives the camera
	/// back its own place: the game's own frame (the hand, picking, the sound) goes by its own camera, only the drawing
	/// by the shown one
	void ShowInspectorCamera(bool shown);
	/// The inspector's override, while one is shown, for the pointer's work (the hand's ray, picking under the cursor)
	void ShowInspectorOverride(bool shown);
	/// Once a game turn outside the temple: the hand's tooltip for what it is over, "Interact" over the player's own
	/// creature
	void ProcessHandToolTipTurn();
	/// The tooltips that the hand submits this turn for what it holds and what is under the pointer
	void SubmitHandToolTips(gui::ToolTips& toolTips, std::optional<entt::entity> pointed);
	void ProcessTurnCheering();
	/// Where a point of the world is on the screen, in whole pixels, when it is on it
	/// Hangs the hand's tooltip by the hand as drawn this frame
	void UpdateHandToolTipAnchor();
	[[nodiscard]] std::optional<glm::vec2> OnScreen(glm::vec3 point) const;
	/// The floating numbers move on by some seconds
	void UpdateFloatingNumbers(float seconds);
	/// The marks over creatures that learn move on, and their bulbs and glows go to the effects to be drawn
	void UpdateLearningMarks(float milliseconds);
	/// The words of the marks over creatures that learn, placed on the screen for the interface
	void PlaceLearningTexts();
	/// The names display grows in or out over a frame of so many milliseconds of game time
	void UpdateVillagerLabels(float milliseconds) override;
	/// The floating numbers and the villagers' labels are given to the interface where they show from the camera the
	/// frame is drawn from, which is the inspector's while it shows one (a held camera, a picture's)
	void PlaceScreenLabels();

	std::optional<std::pair</* frame number */ uint32_t, /* output */ std::filesystem::path>> _requestScreenshot;
	/// The requested screenshot leaves the debug windows out
	bool _screenshotHidesDebugGui {false};
	/// A frame drawn without the debug windows though no picture is taken of it
	std::optional<uint32_t> _debugGuiHiddenFrame;
	std::unique_ptr<audio::AtmosAudio> _atmosAudio;
	std::unique_ptr<audio::GameMusic> _gameMusic;
	std::unique_ptr<HandAnimation> _handAnimation;
	/// Poses the hand around the miracle seed it holds
	magic::HandHoldPoser _handHold;
	/// The changes of the hand that start a blend of the drawn hand
	hand_blend::ChangeTracker _handBlend;
	/// The game's own interface, null without the game's files for it
	std::unique_ptr<gui::GameInterface> _interface;
	/// The search of the local network for game servers, for the multiplayer front end
	std::unique_ptr<online::LanDiscoveryInterface> _lanDiscovery;
	/// The pause that the menu holds while it is open
	gui::MenuPause _menuPause;
	/// Whether the menu was open in the last frame
	bool _menuWasOpen {false};
	/// The menu asked for this game to end and the next one to start: a new game, or a continue as the profile played
	std::optional<game_session::End> _gameEndAsked;
	/// The game ends and waits for the picture of its save on exit before the next game starts
	std::optional<game_session::End> _gameEnding;
	/// While the save on exit goes to the previous profile: the profile that the player picked, played again once the
	/// save is complete
	std::optional<std::u16string> _profileAfterSave;
	/// When the menu last closed, on the computer's clock
	std::optional<std::chrono::steady_clock::time_point> _menuClosedAt;
};
} // namespace openblack
