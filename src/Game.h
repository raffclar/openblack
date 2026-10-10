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
#include <optional>
#include <string>
#include <utility>

#include <glm/mat4x4.hpp>
#include <spdlog/common.h>

#include "3D/HandCrossFade.h"
#include "3D/HandNavigationPose.h"
#include "Common/LoadTimer.h"
#include "Common/Zoomer.h"
#include "Creature/CreatureFight.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "EngineConfig.h"
#include "Graphics/UploadPacer.h"
#include "Gui/LoadingScreenRules.h"
#include "Input/InputLock.h"
#include "Input/ShortcutKeys.h"
#include "Magic/HandHoldPoser.h"
#include "Story/NewGameChoice.h"
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
class LoadingScreen;
} // namespace gui
namespace ecs::components
{
struct Transform;
}

enum class LoggingSubsystem : uint8_t
{
	game,
	input,
	graphics,
	scripting,
	audio,
	pathfinding,
	ai,

	_count
};

constexpr static std::array<std::string_view, static_cast<size_t>(LoggingSubsystem::_count)> k_LoggingSubsystemStrs {
    "game",        //
    "input",       //
    "graphics",    //
    "scripting",   //
    "audio",       //
    "pathfinding", //
    "ai",          //
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
	GraphicsBackend graphicsBackend;
	std::string gamePath;
	float guiScale;
	uint32_t numFramesToSimulate;
	std::string logFile;
	std::array<spdlog::level::level_enum, k_LoggingSubsystemStrs.size()> logLevels;
	std::string startLevel;
	/// Start on the flat creature testbed rather than startLevel
	bool startTestbed {false};
	/// Log frame time statistics every so many frames, never when 0
	uint32_t frameStatsInterval {0};
	/// With the frame statistics, the GPU time of each render view
	bool frameStatsViews {false};
	/// A full-screen video to play once the land is loaded: "intro", "fall" or a path under the game's folder
	std::string playVideo;
	/// Play the pre-intro film at start-up, as on a first run
	bool preIntro {false};
	/// Leave out the logo pictures at start-up
	bool skipLogos {false};
	/// For developers and agents: how a new game on the first land starts, asked the start-of-game question whatever
	/// the player's profiles, or with it answered at once; as the game decides when none
	std::optional<new_game_choice::NewGameStart> newGameStart;
	/// A testbed scenario to run as the game starts, by its id, and how to measure its crowd if it has one
	std::optional<ScenarioRequest> scenario;
	std::optional<std::pair</* frame number */ uint32_t, /* output */ std::filesystem::path>> requestScreenshot;
	/// The port of 127.0.0.1 the debug inspector's server listens on, in builds with it; none to not start it
	std::optional<uint16_t> inspectPort;
	/// Where the inspector keeps pictures by feature, with their catalogue; OPENBLACK_SCREENSHOT_ROOT's or
	/// E:/openblack/screenshots when not given
	std::optional<std::filesystem::path> screenshotRoot;
	/// The seed of every random number the game draws, and the date pinned, for a deterministic run; none for a seed of
	/// the machine's and the wall clock's date
	std::optional<uint32_t> seed;
	/// How the player's mouse and keyboard are kept out while the inspector drives the game: while a client is
	/// connected, from the start, or never
	input::LockMode inspectInputLock {input::LockMode::Auto};
};

class Game
{
public:
	static constexpr auto k_TurnDuration = std::chrono::milliseconds(100);
	/// About how many bytes of files the loading threads read at a time, and how much of what they make may reach the
	/// graphics card each frame, so no frame waits long for it: sixteen buffers or textures keep the renderer's share of a
	/// loading frame to about 6 ms on Vulkan, where some models' hundreds of small parts made it 17 ms
	static constexpr size_t k_FrameLoadBudget = 256 << 10;
	static constexpr graphics::UploadPacer::Allowance k_FrameUploads {.bytes = 128 << 10, .uploads = 16};
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
	/// Seconds the hand eases over to land further from and nearer to the camera
	static constexpr float k_HandEaseOutTime = 0.28f;
	static constexpr float k_HandEaseInTime = 0.1f;
	/// Seconds the hand takes to hold its distance from the camera as it drags by the edge of the screen
	static constexpr float k_HandHoldTime = 0.4f;

	explicit Game(Arguments&& args) noexcept;
	virtual ~Game() noexcept;

	bool ProcessEvents(const SDL_Event& event) noexcept;
	bool GameLogicLoop() noexcept;
	/// The game's music, for a turn of the world or of the temple
	void ProcessMusicTurn(glm::vec3 cameraPosition, bool inCitadel);
	/// The audio of a turn inside the temple, while the world is paused
	void ProcessTempleAudioTurn();
	/// The keys that take the player into the temple's rooms, from outside it or within
	void ProcessTempleRoomKeys();
	bool Update() noexcept;
	bool Initialize() noexcept;
	bool Run() noexcept;

	/// Loads a land's map script, keeping the challenge's scripts running, as the story's own change of land does. The
	/// tips screen shows while it loads, as for a new game; a script's own map command shows the "please wait" banner
	/// instead, after five seconds
	bool LoadMap(const std::filesystem::path& path,
	             loading::LoadingClock::Mode look = loading::LoadingClock::Mode::Tips) noexcept;
	/// Loads a land as the land menu does: the challenge's scripts start again from scratch before the land loads
	bool LoadMapWithFreshScripts(const std::filesystem::path& path) noexcept;
	void LoadLandscape(const std::filesystem::path& path);
	/// Loads the testbed: a flat plane over the whole map, with a lake north of the middle and nothing on it, for trying
	/// out creatures
	void LoadTestbed() noexcept;
	/// Starts a new game on the first land from scratch, as the game's new game does, the story's scripts starting
	/// again; for developers and agents, `start` asks the start-of-game question or answers it at once
	bool StartNewGame(std::optional<new_game_choice::NewGameStart> start) noexcept;

	void SetTime(float time) noexcept;
	/// How many times longer a turn takes: 2 is half speed
	void SetGameSpeed(float multiplier);
	[[nodiscard]] float GetGameSpeed() const;

	[[nodiscard]] uint32_t GetTurn() const;
	[[nodiscard]] bool IsPaused() const;
	/// This computer's hand is drawn this frame: no menu is open over the game and the interface is not given over to a
	/// cinematic
	[[nodiscard]] bool IsHandDrawn() const;
	/// The scenario asked for on the command line, once: the scenarios' window runs it as the game starts
	/// The game ends after this frame. Unlike the window's events, which say each time whether to go on, nothing takes
	/// it back
	void RequestQuit() { _quitRequested = true; }
	[[nodiscard]] std::optional<ScenarioRequest> TakeScenarioRequest() { return std::exchange(_scenarioRequest, std::nullopt); }
	/// A scenario asked for that the scenarios' window hasn't started yet
	[[nodiscard]] const std::optional<ScenarioRequest>& PendingScenario() const { return _scenarioRequest; }
	/// Asks for a scenario while the game runs: the scenarios' window runs it on a fresh testbed next frame
	void RequestScenario(ScenarioRequest request) { _scenarioRequest = std::move(request); }
	[[nodiscard]] std::chrono::duration<float, std::milli> GetDeltaTime() const { return _turnDeltaTime; }
	[[nodiscard]] const glm::ivec2& GetMousePosition() const { return _mousePosition; }
	/// Puts the cursor the game works with somewhere in the window, until the mouse next moves
	void SetMousePosition(glm::ivec2 position) { _mousePosition = position; }
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
	/// The map script of the land loaded last, or "testbed"; empty before one is
	[[nodiscard]] const std::filesystem::path& GetLandPath() const noexcept { return _landPath; }

	static Game* Instance() { return sInstance; }

private:
	static Game* sInstance;

	/// What the game shows as it starts, before it loads the rest: the logo pictures, the pre-intro film on a first run
	/// and the tips screen fading in, which stays up while the game loads
	void PlayStartupScreens();
	/// The logo film's two pictures, each fading in and out, any key or mouse button hurrying them on
	void PlayLogoScreens();
	/// The pre-intro film in its own loop with the trailer music, until its last frame or a key
	void PlayPreIntro();
	/// Picks the tip of the day and makes its picture; the first time, fades the tips screen in over a second
	void StartTipOfTheDay();
	/// The tips screen, or the banner, shows while a land loads, drawn again at the loading's redraw points
	void BeginLoadingScreen(loading::LoadingClock::Mode look);
	/// A redraw point inside the loading: the tips screen is drawn and shown if it is due
	void RenderLoadingFrame();
	/// The loading is over: its tip goes before the first frame of the game
	void EndLoadingScreen();
	/// Draws one frame outside the game's loop over a black screen and shows it
	void PresentStartupFrame(const std::function<void(glm::u16vec2 resolution)>& draw);
	/// Whether the player has a profile: openblack keeps none yet, so it counts as having one
	[[nodiscard]] static bool HasPlayerProfiles() noexcept { return true; }

	/// Clears the last land's entities, weather and effects before a new one loads
	void PrepareNewLand();
	/// What every land starts with once its landscape is loaded: the sky's clock, a player and the land's physics
	void SetUpLandscape();
	/// Starts the game clock, the atmosphere's sounds and the music on the new land
	void StartNewLand();

	/// When the game was launched
	const LoadTimer::Clock::time_point _launchTime {LoadTimer::Clock::now()};
	/// Times the start of the game, from its launch to the first frame drawn on the first land
	std::optional<LoadTimer> _startupTimer;
	/// Times the loading of every resource registered at the start, on the loading threads or where first wanted
	std::optional<LoadTimer> _prefetchTimer;

	/// path to Lionhead Studios Ltd/Black & White folder
	const std::filesystem::path _gamePath;

	std::filesystem::path _startMap;
	std::string _playVideo;
	bool _preIntro {false};
	bool _skipLogos {false};
	std::optional<new_game_choice::NewGameStart> _newGameStart;
	/// The screens drawn as the game starts and while a land loads, none without a window
	std::unique_ptr<gui::LoadingScreen> _loadingScreen;
	/// What is drawn at the loading's redraw points, nothing while not loading
	std::optional<loading::LoadingClock::Mode> _loadingLook;
	/// The tips screen has faded in once: later loads show it at once
	bool _tipFadedIn {false};
	bool _startTestbed {false};
	std::optional<ScenarioRequest> _scenarioRequest;
	/// The port the debug inspector is to listen on, if it is to start
	std::optional<uint16_t> _inspectPort;
	std::optional<std::filesystem::path> _screenshotRoot;
	std::optional<uint32_t> _seed;
	input::LockMode _inspectInputLock {input::LockMode::Auto};
	/// Whether the testbed opens its window of scenarios
	bool _testbedWindow {true};
	bool _quitRequested {false};

	/// The machine's ticks at the last turn, as the game reads them
	uint32_t _lastGameLoopTime {0};
	std::chrono::steady_clock::duration _turnDeltaTime;
	uint32_t _frameCount {0};
	glm::ivec2 _mousePosition {0, 0};
	/// The left button went down since the last frame, which may click the dialogue on
	bool _dialogueClick {false};
	bool _handGripping;
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
	hand_navigation_pose::Pose _handPose {hand_navigation_pose::Pose::Idle};
	/// How far from the camera the hand holds while it drags by the edge of the screen
	Zoomer _handHoldZoomer;
	/// The land the hand grips while it drags it, and where the hand was when it gripped
	glm::vec3 _handGripPoint {0.0f, 0.0f, 0.0f};
	/// The fade from where the hand was to where it is now held, as it grips the land or lets go
	HandCrossFade _handCrossFade;
	/// The hand's tap after it knocked on a house: it stays upright where it knocked while the tap plays through once.
	/// Holding something puts it off, and it starts over once the hand lets go.
	struct HandKnock
	{
		glm::vec3 point;
		std::chrono::microseconds time {0};
	};
	std::optional<HandKnock> _handKnock;
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
	Zoomer3 _handUp {glm::vec3(0.0f, 1.0f, 0.0f)};
	int _handLastCursorX {0};
	/// Whether the hand held its up last frame, gripping the land
	bool _handUpWasHeld {false};
	/// Whether the cursor is on a thing rather than the land or the sea
	bool _cursorOnObject {false};
	/// The up of the face of a model the hand rests on, none over the land or what the hand doesn't feel
	std::optional<glm::vec3> _handSurfaceUp;
	/// The hand rests over a thing picked under the cursor
	bool _handOverObject {false};
	/// Where the cursor points at in the world, on the landscape or the sea
	std::optional<glm::vec3> _cursorWorldPosition;
	/// The land or sea under the cursor, whatever thing is in front of it
	std::optional<glm::vec3> _cursorLand;
	/// Where the hand rests on or before the thing under the cursor, which it hovers at rather than over the land
	std::optional<glm::vec3> _handRestOnThing;
	/// Where the hand is and how it is posed while it is held to a creature
	std::optional<ecs::systems::CreatureHandSystemInterface::HandPose> _handOnCreature;
	/// The creature the hand is over this frame, if any
	std::optional<entt::entity> _creatureUnderHand;
	/// Grabbing the sea plays G_HandInWater_01 to _10 in turn
	uint32_t _handInWaterSample {0};

	/// Plays the sound of the hand grabbing the land or the sea at the grab point, as the game does
	void PlayHandGrabSound();
	/// What the hand steps by this frame, in seconds
	[[nodiscard]] float HandStepSeconds() const;
	/// What the camera and the hand step by this frame: the frame's real time in whole milliseconds, or the game's time
	/// while a script holds the widescreen
	[[nodiscard]] std::chrono::milliseconds CameraStepTime() const;
	/// The miracles hear where the hand and cursor are, and the held miracle follows the hand
	void UpdateMagicHand(const glm::vec3& handPosition, float deltaSeconds);
	/// The gestures drawn with the cursor this frame, through the gesture system
	void UpdateGestures(const Camera& camera, glm::ivec2 screenSize, float deltaSeconds);
	/// Turns the hand to face along the line of sight through the cursor and stands it on the slope under it
	void OrientHand(ecs::components::Transform& handTransform, const glm::mat3& facingCamera, glm::vec3 surfaceUp,
	                float deltaSeconds);
	/// Decides how the hand moves this frame, gripping the land, held as it drags by the edge, or hovering, and its pose
	void UpdateHandNavigation(const ecs::components::Transform& handTransform);
	/// Places the hand on the line of sight through the cursor the way the game does
	void PlaceHand(ecs::components::Transform& handTransform, float deltaSeconds);
	/// A knock on a house: the hand's tap starts, waits while the hand holds something, and the houses' read-out runs
	void UpdateHandKnock(const ecs::components::Transform& handTransform);
	/// Loads the hand animations of Data/CTR/hh.hbn for the hand mesh
	void LoadHandAnimation();
	/// What moves each species' body, from Data/CTR's .cbn files, by the species their base mesh names
	static void LoadCreatureRigs();
	/// Acts on what the player chose in the game's menu: continuing restores the pause it had before it opened
	/// Starts the story's scripts of a new game on the first land, and asks a returning player what to skip
	void StartStoryScripts();
	/// As a new game starts: whether to ask a returning player what to skip, and asking. Whether the question was put
	/// (or answered at once by the developers' start), which leaves the land as it is rather than black at its start
	bool AskNewGameChoice();
	void HandleInterfaceAction();
	/// Once a frame outside the temple: where the hand's tooltip is drawn, and the status panel of the creature the hand
	/// is held to, or over
	void UpdateHandInterface();
	/// The interface's pick of what is under the cursor, at the frame as it is about to be drawn
	void PickUnderCursor(float seconds);
	/// The near plane for the camera where it is now
	void FitNearClip();
	/// Shows the camera where the inspector wants the frame drawn from (an override, a picture's), or gives the camera
	/// back its own place: the game's own frame (the hand, picking, the sound) goes by its own camera, only the drawing
	/// by the shown one
	void ShowInspectorCamera(bool shown);
	/// Once a game turn outside the temple: the hand's tooltip for what it is over, "Interact" over the player's own
	/// creature
	void ProcessHandToolTipTurn();
	/// Where a point of the world is on the screen, in whole pixels, when it is on it
	[[nodiscard]] std::optional<glm::vec2> OnScreen(glm::vec3 point) const;
	/// The floating numbers move on by some seconds and are given to the interface where they show
	void UpdateFloatingNumbers(float seconds);

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
	/// The game's own interface, null without the game's files for it
	std::unique_ptr<gui::GameInterface> _interface;
	std::filesystem::path _landPath;
	/// Whether the game was paused when the menu opened, which pauses it
	bool _pausedBeforeMenu {true};
	bool _menuWasOpen {false};
};
} // namespace openblack
