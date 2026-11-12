/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The input of the game: the window's events, the keys, the developer keys and the input tools

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <utility>

#include <LHVM.h>
#include <SDL.h>
#include <spdlog/spdlog.h>

#include "3D/HandAnimation.h"
#include "3D/HandCycleChoice.h"
#include "3D/HandMorph.h"
#include "3D/HandNavigationPose.h"
#include "3D/MapCoords.h"
#include "3D/SkyDome.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/AtmosAudio.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/ScriptCameraModel.h"
#include "Camera/WorldCamera.h"
#include "Commands/PlayerCommands.h"
#include "Common/DeveloperToolsGate.h"
#include "Common/EventManager.h"
#include "Common/GameRandom.h"
#include "Common/MachineClock.h"
#include "Common/RandomNumberManager.h"
#include "Common/SideThread.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureFightHud.h"
#include "Creature/CreatureHandRules.h"
#include "Creature/LeashOrders.h"
#include "Debug/DebugGuiInterface.h"
#include "Debug/FrameStatsLog.h"
#include "Debug/FrameWriteGuard.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Archetypes/SkyArchetype.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/InterfaceLimits.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/ChatSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/GameKeysSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandPressSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/PlaygroundTripSystemInterface.h"
#include "ECS/Systems/PuzzleTotemSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/VillagerLabelSource.h"
#include "EngineConfig.h"
#include "Game.h"
#include "GameDetail.h"
#include "Graphics/FrameBuffer.h"
#include "Graphics/RendererInterface.h"
#include "Gui/EscapeKeyRules.h"
#include "Gui/GameInterface.h"
#include "Gui/JoinOnlineEntry.h"
#include "Gui/LoadingScreen.h"
#include "Gui/MultiplayerFrontEnd.h"
#include "Gui/StartupRules.h"
#include "Hand/HandBlend.h"
#include "Hand/HandFeel.h"
#include "Hand/HandTrace.h"
#include "Hand/HandVisibility.h"
#include "Hand/ScriptHandState.h"
#include "Help/DialogueText.h"
#include "Help/Spirits.h"
#include "Input/CursorFreeze.h"
#include "Input/DeveloperKeys.h"
#include "Input/GameActionMapInterface.h"
#include "Input/KeyBindings.h"
#include "Input/KeyRebinding.h"
#include "Input/PlatformKeys.h"
#include "Locator.h"
#include "Magic/TownBelief.h"
#include "Online/NetworkSession.h"
#include "Particles/LiquidParticles.h"
#include "Profiler.h"
#include "ScriptHeaders/ScriptInterfaceLevels.h"
#include "Simulation/ChecksumLog.h"
#include "Simulation/CommandLog.h"
#include "Simulation/WorldStart.h"
#include "Sound/ClipSounds.h"
#include "Sound/HelpSpeech.h"
#include "Temple/TempleHelp.h"
#include "Video/VideoRules.h"

#if defined(OPENBLACK_INSPECTOR)
#include "Inspector/GameSeam.h"
#endif

using namespace openblack;
using namespace openblack::game_detail;

bool Game::ProcessEvents(const SDL_Event& event) noexcept
{
	// The hand grips the land, which the temple has none of: its camera takes the clicks
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	// The presses, the let goes and the motions of the mouse go to the hand at the end of the frame. The other events go
	// to the hand first, then the work of the event's type.
	auto& handPress = Locator::handPressSystem::value();
	if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP || event.type == SDL_MOUSEMOTION)
	{
		// While the mouse turns the camera, the cursor stays where it was and the hand does not follow the pointer
		auto& actions = Locator::gameActionSystem::value();
		actions.KeepHandEvent(event, event.type == SDL_MOUSEMOTION && !actions.IsCursorFrozen());
	}
	else
	{
		handPress.Press(event, {.inTemple = inTemple, .playGrabSound = [this] { PlayHandGrabSound(); }});
	}

	switch (event.type)
	{
	case SDL_QUIT:
		_closeAsked = true;
		break;
	case SDL_WINDOWEVENT:
		if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == Locator::windowing::value().GetID())
		{
			_closeAsked = true;
		}
		// A window resized, maximised or restored is followed by the next frame drawn (FollowWindowSize)
		break;
	case SDL_KEYDOWN:
		if (!Locator::gameKeysSystem::value().KeyDown(event, inTemple))
		{
			return false;
		}
		break;
	}

	return true;
}

void Game::UpdateInput()
{
	auto& profiler = Locator::profiler::value();
	auto& camera = Locator::camera::value();
	// The interface lock of the hand ends before the hand reads the presses of this frame
	EndHandLockForFrame();
	// Input events
	{
		auto sdlInput = profiler.BeginScoped(Profiler::Stage::SdlInput);
		// The debug windows' presses of the options screen's actions are made by the action map's frame
		auto& actions = Locator::gameActionSystem::value();
		// The world's camera is the one turned with the mouse, which holds the cursor and the hand still meanwhile.
		// When the camera ignores its controls, at the fence or under a box of the menu, the cursor follows the pointer.
		const auto* worldCamera = dynamic_cast<DefaultWorldCameraModel*>(&camera.GetModel());
		actions.AllowCursorFreeze(input::FreezeAllowed({
		    .inTemple = Locator::temple::has_value() && Locator::temple::value().Active(),
		    .worldCamera = worldCamera != nullptr,
		    .rotateAroundMouse =
		        (Locator::cameraHelpSystem::value().Get().features & camera_help::feature::k_RotateAroundMouse) != 0,
		    .heldBackAtFence = worldCamera != nullptr && worldCamera->IsHeldBack(),
		    .frontEndBox = FrontEndBoxShows(),
		}));
		// The land's scripts may take the camera's keys away, as the tutorials do
		actions.SetBlockedActions(Locator::cameraHelpSystem::value().Get().BlockedActions());
		// While a script holds the screen, no press of the mouse does a one-press action
		actions.SetScriptHoldsScreen(Locator::cinematicDirectorSystem::has_value() &&
		                             !Locator::cinematicDirectorSystem::value().IsInterfaceActive());
		if (!Locator::debugGui::value().StealsFocus() || actions.HasQueuedPresses())
		{
			actions.Frame();
		}
		// A hand demonstration plays its records due now through the hand's own input
		if (Locator::handDemoSystem::has_value())
		{
			Locator::handDemoSystem::value().Update();
		}
		// The scripted mouse's moves and buttons take effect as they are pumped, as a real mouse's do in SDL: after the
		// action map's frame has read the pointer
		actions.PumpScriptedPointer();
		SDL_Event e;
		while (SDL_PollEvent(&e) != 0)
		{
			// While an agent drives the game, the player's mouse and keyboard are kept out of it
			if (!actions.AdmitEvent(e))
			{
				continue;
			}
			actions.PointPlayerEvent(e);
			Locator::events::value().Create<SDL_Event>(e);
		}
		// The hand gets the frame's mouse events after all of them came. While Alt is held, the presses and the let goes of
		// the hand's buttons wait.
		auto& handPress = Locator::handPressSystem::value();
		const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
		actions.TakeHandEvents([this, &handPress, inTemple](const SDL_Event& event, bool movesPointer) {
			handPress.Press(event, {.inTemple = inTemple, .playGrabSound = [this] { PlayHandGrabSound(); }});
			if (movesPointer)
			{
				handPress.MovePointer({event.motion.x, event.motion.y});
			}
		});
		FollowWindowSize();
		if (actions.IsCursorFrozen())
		{
			handPress.SetMousePosition(glm::ivec2(actions.GetMousePosition()));
		}
		if (const auto warp = actions.GetCursorWarp(); warp.has_value())
		{
			handPress.MovePointer(*warp);
		}
		// The hand is put under the pointer every frame, moving or not: until the mouse first moves, under the pointer
		// the game keeps, so from a land's first frame it is where the cursor is, or the top left corner while the
		// cursor has never been over the window
		handPress.FramePointer(actions.IsPointerKnown() ? std::optional(actions.GetPointerPosition()) : std::nullopt);
		auto& keys = Locator::gameKeysSystem::value();
		keys.UpdateTempleRoomKeys();
		keys.UpdateShortcutKeys();
	}
}

bool Game::UpdateDebugGui()
{
	auto& profiler = Locator::profiler::value();
	const auto& config = Locator::config::value();
	if (!config.running || _quitRequested)
	{
		return false;
	}
	// ImGui events + prepare
	{
		auto guiLoop = profiler.BeginScoped(Profiler::Stage::GuiLoop);
		// While the input of the player is locked, the debug windows take only the mouse events of the game and the tools.
		// A notice shows the lock.
		Locator::debugGui::value().SetInputLock(Locator::gameActionSystem::value().IsPlayerInputBlocked());
		// The debug menu bar comes up with the game's menu, or always without it
		Locator::debugGui::value().SetMenuBarVisible(!Locator::gameInterface::has_value() ||
		                                             Locator::gameInterface::value().GetMenu().IsOpen());
		if (Locator::debugGui::value().Loop())
		{
			return false; // Quit event
		}
	}
	return true;
}

bool Game::CameraReadsKeys()
{
	const bool interfaceActive =
	    !Locator::cinematicDirectorSystem::has_value() || Locator::cinematicDirectorSystem::value().IsInterfaceActive();
	return game_frame::CameraReadsKeys({
	    .inTemple = Locator::temple::has_value() && Locator::temple::value().Active(),
	    .interfaceActive = interfaceActive,
	    .boxHasKeyboard = Locator::gameActionSystem::value().BoxHasKeyboard(),
	});
}

void Game::UpdateCameraKeys(std::chrono::microseconds deltaTime)
{
	// From the end of the frame that goes into the temple or that opens a box of the front end, the player's camera of the
	// world takes no controls of the player
	world_camera::EndFrameControls(FrontEndBoxShows());
	if (!CameraReadsKeys())
	{
		return;
	}
	auto& actions = Locator::gameActionSystem::value();
	auto& camera = Locator::camera::value();
	// The keys of the camera act after the camera moved, so that their steps move it in the next frame
	// A miracle's camera path keeps the camera until the player moves it with the movement keys or grips the land,
	// which hand it straight back; meanwhile the player's other camera controls do nothing
	auto& cameraPaths = Locator::cameraPathSystem::value();
	const auto& buttons = Locator::handPressSystem::value().GetHandButtons();
	const bool pathHeld = cameraPaths.HoldsCamera();
	cameraPaths.HandlePlayerControl(
	    {.movementKey = actions.GetAny(input::BindableActionMap::MOVE_LEFT, input::BindableActionMap::MOVE_RIGHT,
	                                   input::BindableActionMap::MOVE_FORWARDS, input::BindableActionMap::MOVE_BACKWARDS),
	     .frameMilliseconds = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(deltaTime).count()),
	     .grippingLand = buttons.gripping && !buttons.rotating});
	// The camera that the path gives back reads the keys from the next frame
	// The keys go to the last step of the input in this frame. A frame with no step does not read them.
	if (const auto& steps = Locator::time::value().GetInputSteps(); !pathHeld && steps.count > 0)
	{
		auto& gameActions = Locator::gameActionSystem::value();
		gameActions.UseInputStep(steps.count - 1);
		camera.HandleKeys(std::chrono::duration_cast<std::chrono::microseconds>(steps.Time(steps.count - 1)));
		gameActions.UseInputStep(std::nullopt);
	}
	// A grip of the land ends the camera of a dance. A zoom with the land held does not end it.
	hand::script_state::Facts grip {.gripsLand = buttons.selectGrip, .otherButton = buttons.otherButton};
	if (Locator::cameraHelpSystem::has_value())
	{
		grip.cameraFeatures = Locator::cameraHelpSystem::value().Get().features;
	}
	if (Locator::danceSystem::has_value())
	{
		Locator::danceSystem::value().ProcessCameraKeys(hand::script_state::StateOf(grip) ==
		                                                hand::script_state::State::GripLandscape);
	}
}

escape_key::Situation Game::EscapeSituation(uint16_t keyModifiers) const
{
	const auto& director = Locator::cinematicDirectorSystem::value();
	const auto fade = director.GetFadeColour();
	escape_key::Situation situation {
	    .shiftOrCtrl = (keyModifiers & (KMOD_SHIFT | KMOD_CTRL)) != 0,
	    .scriptHoldsCinemaBars = director.IsWideScreenOn() && director.GetWideScreenOwner() != 0,
	    .helpScriptHoldsDialogue = Locator::vm::has_value() && chlapi::CHLApi::HelpScriptHoldsDialogue(),
	    // openblack's games are all single player
	    .inGodsPlayground =
	        Locator::entitiesRegistry::value().Context().mapScriptGlobals.landNumber == escape_key::k_GodsPlaygroundLand,
	    .screenClear = director.IsFadeFinished() && (fade >> 24u) == 0,
	    .playgroundLessonRunning =
	        Locator::playgroundTripSystem::has_value() && Locator::playgroundTripSystem::value().IsLessonRunning(),
	    .sinceMenuClosed = std::nullopt,
	    .gameOver = Locator::entitiesRegistry::value().Context().gameOver,
	};
	if (const auto& menuClosedAt = Locator::gameKeysSystem::value().GetMenuClosedAt(); menuClosedAt.has_value())
	{
		situation.sinceMenuClosed =
		    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - *menuClosedAt);
	}
	return situation;
}

void Game::HandleEscapeInPlay(uint16_t keyModifiers)
{
	auto& director = Locator::cinematicDirectorSystem::value();
	const auto response = escape_key::Respond(EscapeSituation(keyModifiers));
	if (response.clearScreenFade)
	{
		director.FadeBackToNormal(0);
	}
	if (response.stopHelpScripts)
	{
		chlapi::CHLApi::StopHelpScriptsByEscape();
	}
	switch (response.outcome)
	{
	case escape_key::Outcome::Nothing:
		break;
	case escape_key::Outcome::OpenMenu:
		// The game keeps the help record of the player when Escape opens the menu
		SaveHelpRecord();
		TellMenuTheGame();
		Locator::gameInterface::value().OpenMenu();
		// The menu reads the profile's own pictures again as it opens
		ReadTattooPictures();
		HandleInterfaceAction();
		break;
	case escape_key::Outcome::AskLeavePlayground:
		// The game pauses while the question is on the screen
		Locator::time::value().SetPlayerPause(true);
		Locator::gameInterface::value().AskTripQuestion(playground_trip::k_LeaveQuestion);
		_tripQuestion = playground_trip::Question::Leave;
		break;
	case escape_key::Outcome::EndPlaygroundLesson:
		// The lesson ends at the end of the game turn
		Locator::playgroundTripSystem::value().AskLessonEnd();
		break;
	case escape_key::Outcome::AskNewGame:
		Locator::gameInterface::value().AskNewGameQuestion();
		break;
	}
}

void Game::HandleTripKey()
{
	if (_tripQuestion.has_value())
	{
		return;
	}
	const auto& context = Locator::entitiesRegistry::value().Context();
	const auto& director = Locator::cinematicDirectorSystem::value();
	const playground_trip::GoSituation situation {
	    .skirmish = context.skirmish,
	    .networkGame = InNetworkGame(),
	    .inTemple = Locator::temple::has_value() && Locator::temple::value().Active(),
	    .land = context.mapScriptGlobals.landNumber,
	    .boxUp = Locator::gameInterface::value().BoxRefusesTrip(),
	    .scriptHoldsCinemaBars = director.IsWideScreenOn() && director.GetWideScreenOwner() != 0,
	};
	if (!playground_trip::MayAskToGo(situation))
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "F2 does nothing here");
		return;
	}
	// The game pauses while the question is on the screen
	Locator::time::value().SetPlayerPause(true);
	Locator::gameInterface::value().AskTripQuestion(playground_trip::k_GoQuestion);
	_tripQuestion = playground_trip::Question::Go;
}

std::chrono::microseconds Game::InputStepTime(std::chrono::microseconds frameTime) const
{
	// The steps of the input go by the time of this frame: its real time, or its game time while a script holds the
	// widescreen, as the camera's step does
	const bool insideTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	if (ScriptHoldsCamera() && !insideTemple)
	{
		return std::chrono::duration_cast<std::chrono::microseconds>(Locator::time::value().GetFrameGameTime());
	}
	return frameTime;
}

void Game::LoadProfileKeys()
{
	// A profile that keeps no bindings has the game's defaults for the mouse as it is now
	input::KeyBindingTable bindings = Locator::gameActionSystem::has_value()
	                                      ? Locator::gameActionSystem::value().GetDefaultKeyBindings()
	                                      : input::DefaultKeyBindings();
	if (Locator::playerProfileSystem::has_value())
	{
		if (const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile(); profile.has_value())
		{
			bindings = input::rebinding::FromKept(profile->keys, bindings);
		}
	}
	if (Locator::gameActionSystem::has_value())
	{
		Locator::gameActionSystem::value().SetKeyBindings(bindings);
	}
	if (Locator::gameInterface::has_value())
	{
		Locator::gameInterface::value().GetMenu().SetControls(bindings);
	}
}

void Game::KeepProfileKeys()
{
	if (!Locator::playerProfileSystem::has_value() || !Locator::gameActionSystem::has_value())
	{
		return;
	}
	const auto bindings = Locator::gameActionSystem::value().GetKeyBindings();
	Locator::playerProfileSystem::value().SetCurrentKeys(input::rebinding::ToKept(bindings));
}

void Game::SendDialogueClick()
{
	// Nor can the player click or key through the advisors' lines while a hand demonstration plays
	const bool demoPlaying = Locator::handDemoSystem::has_value() && Locator::handDemoSystem::value().IsReplaying();
	const bool click = Locator::handPressSystem::value().TakeDialogueClick() && !demoPlaying;
	const bool skipKey = !demoPlaying && DialogueSkipKeyDown();
	if (!click && !skipKey)
	{
		return;
	}
	// A click or a key that cuts the help text short takes the hand too: the interface lock of the hand ends
	if (Locator::helpTextSystem::has_value() && Locator::helpTextSystem::value().ClickCutsShort(click, skipKey) &&
	    Locator::handSystem::has_value())
	{
		Locator::handSystem::value().EndOwnInterfaceLock();
	}
	// In a game alone the world's turns stop in the temple, so the temple's own turns take the click
	const bool templeTakes = help::TempleTurnsTakeTheClick({
	    .networkGame = Locator::time::value().IsNetworkGame(),
	    .inTemple = Locator::temple::has_value() && Locator::temple::value().Active(),
	    .templeGoesIn = Locator::temple::has_value() && Locator::temple::value().GoesInAfterFrame(),
	});
	if (templeTakes)
	{
		if (Locator::helpTextSystem::has_value())
		{
			Locator::helpTextSystem::value().TakeClick(click, skipKey);
		}
		return;
	}
	Locator::playerCommandSystem::value().Submit(player_commands::DialogueClick {.click = click, .skipKey = skipKey});
}

bool Game::DialogueSkipKeyDown() const
{
	// The key comes from the player's keyboard, which the input lock keeps out also when the window has the keys
	const bool playerKeys =
	    !Locator::gameActionSystem::has_value() || !Locator::gameActionSystem::value().IsPlayerInputBlocked();
	const auto* keys = playerKeys ? SDL_GetKeyboardState(nullptr) : nullptr;
	return keys != nullptr && keys[SDL_SCANCODE_KP_ENTER] != 0;
}
