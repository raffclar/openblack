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
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/InspectorSystemInterface.h"
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
	static bool leftMouseButton = false;
	static bool middleMouseButton = false;
	static bool rightMouseButton = false;

	// Pressed and let go, or as the mouse moving finds them: a press or a let go the menu or the debug windows took would
	// otherwise leave the hand gripping, as on leaving the temple
	if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.button == SDL_BUTTON_LEFT)
	{
		leftMouseButton = event.type == SDL_MOUSEBUTTONDOWN;
	}
	// A press of the left button may click the dialogue on, whatever else it does
	if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT &&
	    !Locator::debugGui::value().IsMouseOverWindow())
	{
		_dialogueClick = true;
	}
	if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.button == SDL_BUTTON_MIDDLE)
	{
		middleMouseButton = event.type == SDL_MOUSEBUTTONDOWN;
	}
	const bool rightLetGo =
	    rightMouseButton && ((event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) ||
	                         (event.type == SDL_MOUSEMOTION && (event.motion.state & SDL_BUTTON_RMASK) == 0));
	if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.button == SDL_BUTTON_RIGHT)
	{
		rightMouseButton = event.type == SDL_MOUSEBUTTONDOWN;
	}
	if (event.type == SDL_MOUSEMOTION)
	{
		leftMouseButton = (event.motion.state & SDL_BUTTON_LMASK) != 0;
		middleMouseButton = (event.motion.state & SDL_BUTTON_MMASK) != 0;
		rightMouseButton = (event.motion.state & SDL_BUTTON_RMASK) != 0;
	}

	// The hand grips the land, which the temple has none of: its camera takes the clicks
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	// While a script's cinema bars have taken the interface, the buttons' presses don't reach the hand
	const bool handTakesPresses = hand_visibility::TakesPresses(Locator::cinematicDirectorSystem::value().IsInterfaceActive());
	// Holding the right button on a creature holds the hand to it to stroke or slap it; clicking it puts the leash on.
	// While the player's creature fights, clicks on the creatures and the arena's ground direct the fight instead.
	auto& creatureHand = Locator::creatureHandSystem::value();
	auto& fights = Locator::creatureFightSystem::value();
	auto& magic = Locator::magicSystem::value();
	// The hand holds one thing at a time: a thing it picked up, or a miracle
	auto* handGrab = Locator::handGrabSystem::has_value() ? &Locator::handGrabSystem::value() : nullptr;
	const bool handHoldsThing = handGrab != nullptr && handGrab->IsBusy();
	// The interface level of a lesson can stop the Action button from taking hold of creatures and totems. It can also
	// stop an empty hand from acting on anything but a highlight that a script made.
	ecs::components::InterfaceLimits interfaceLimits {};
	if (Locator::playerSystem::has_value())
	{
		// In a network game the input can come before the first turn makes the player of this computer
		interfaceLimits = script::interface_levels::LimitsOf(Locator::entitiesRegistry::value(),
		                                                     Locator::playerSystem::value().FindPlayer(LocalInputPlayer()));
	}
	const bool mayHold = script::interface_levels::MayHold(interfaceLimits);
	ProcessActionLetGo(event, rightLetGo, inTemple, handTakesPresses, creatureHand, magic, handGrab, handHoldsThing);
	ProcessActionPress(event, inTemple, handTakesPresses, creatureHand, fights, magic, handGrab, handHoldsThing,
	                   interfaceLimits, mayHold);
	ProcessMovePress(event, middleMouseButton, inTemple, handTakesPresses, creatureHand, fights, magic);
	// Letting go of the button that pressed charges the blow it queued
	if (_fightButton.has_value() && fights.IsPressed() &&
	    ((*_fightButton == creature_fight::Button::Move && !leftMouseButton) ||
	     (*_fightButton == creature_fight::Button::Action && !rightMouseButton)))
	{
		fights.Release(machine_clock::Ticks(), Locator::time::value().GetTurn());
		_fightButton.reset();
	}
	// Letting go of the right button lets go of the creature, also of a hold that a command started. Let go quickly,
	// having neither stroked nor slapped it, the press was a click, which puts the leash on the player's creature.
	if (creature_hand::LetsGoOfCreature(rightLetGo,
	                                    event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) &&
	    creatureHand.GetCreature().has_value())
	{
		// The click puts the leash on, and then the creature feels how the hand treated it. Both are commands of the
		// player, which the world does at the start of a later turn. The release sends them in a network game, and the
		// draw of the hand sends them in a game alone.
		creatureHand.Release();
	}
	const bool onCreature = creatureHand.GetCreature().has_value();

	// A miracle in the hand doesn't stop the hand moving the land; the middle button, or both buttons, turn the camera
	_handGripping = !inTemple && (middleMouseButton || (leftMouseButton && !onCreature && !fights.IsPressed()));
	_handRotating = !inTemple && (middleMouseButton || (leftMouseButton && rightMouseButton));
	_handSelectGrip = !inTemple && leftMouseButton && !onCreature && !fights.IsPressed();
	_handOtherButton = rightMouseButton;

	auto& window = Locator::windowing::value();
	auto& camera = Locator::camera::value();

	switch (event.type)
	{
	case SDL_QUIT:
		_closeAsked = true;
		break;
	case SDL_WINDOWEVENT:
		if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == window.GetID())
		{
			_closeAsked = true;
		}
		// A window resized, maximised or restored is followed by the next frame drawn (FollowWindowSize)
		break;
	case SDL_KEYDOWN:
		// The M key opens the miracle selection, and the R key calls the last miracle again, while no action has the
		// key. In the temple the keys do other things.
		if ((event.key.keysym.scancode == SDL_SCANCODE_M || event.key.keysym.scancode == SDL_SCANCODE_R) && !inTemple &&
		    Locator::gestureSystem::has_value() &&
		    input::ActionsForKeyDown(Locator::gameActionSystem::value().GetKeyBindings(), event.key.keysym.scancode,
		                             event.key.keysym.mod) == input::BindableActionMap::NONE)
		{
			auto& gestures = Locator::gestureSystem::value();
			if (event.key.keysym.scancode == SDL_SCANCODE_M)
			{
				gestures.OpenSelectionByKey(LocalInputPlayer());
			}
			else
			{
				gestures.RepeatMiracleByKey(LocalInputPlayer());
			}
			break;
		}
		if (!ProcessKeyPress(event, window, camera))
		{
			return false;
		}
		break;
	case SDL_MOUSEMOTION:
	{
		// While the mouse turns the camera the cursor is held where it was, and the pointer isn't followed
		if (!Locator::gameActionSystem::value().IsCursorFrozen())
		{
			_mousePosition = {event.motion.x, event.motion.y};
			_handPointer.Moved();
		}
		break;
	}
	}

	return true;
}

void Game::ProcessActionLetGo(const SDL_Event& event, bool rightLetGo, bool inTemple, bool handTakesPresses,
                              ecs::systems::CreatureHandSystemInterface& creatureHand,
                              ecs::systems::MagicSystemInterface& magic, ecs::systems::HandGrabSystemInterface* handGrab,
                              bool handHoldsThing)
{
	// Letting go of the Action button lets go of the tip bubble
	if (Locator::tipBubbleSystem::has_value() && Locator::tipBubbleSystem::value().IsGripped() &&
	    (rightLetGo || (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT)))
	{
		Locator::tipBubbleSystem::value().LetGo();
	}
	// Letting go of the Action button lets go of a town's totem, leaving it where it was slid
	if (Locator::villageTotemSystem::has_value() && Locator::villageTotemSystem::value().GetGripped().has_value() &&
	    (rightLetGo || (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT)))
	{
		Locator::villageTotemSystem::value().LetGo();
		_totemPointer.reset();
	}
	// It also lets go of a puzzle totem, at the height it was slid to
	if (Locator::puzzleTotemSystem::has_value() && Locator::puzzleTotemSystem::value().GetGripped().has_value() &&
	    (rightLetGo || (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT)))
	{
		Locator::puzzleTotemSystem::value().LetGo();
		_totemPointer.reset();
	}
	// Letting go of the Action button lets go of what the hand was taking, or puts down or throws what it holds
	if (handGrab != nullptr && (rightLetGo || (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT)))
	{
		// TODO(hand): a press too short to take the thing taps it
		[[maybe_unused]] const auto tapped = handGrab->Release(machine_clock::Ticks(), Locator::time::value().GetTurn());
		// Let go with an empty hand that did nothing with the press, it clicks the thing or the place under it, for the
		// scripts; not while the game is paused
		if (handTakesPresses && !_actionPressTaken && !handHoldsThing && !magic.IsHandBusy() && !inTemple && !IsPaused())
		{
			handGrab->ClickReleased();
		}
	}
	// The action button (the right) casts the miracle in the hand, which comes before the creatures: pressed, it arms,
	// locks on or casts it, and let go it throws an armed one or lets a locked one go. Without a miracle it takes hold
	// of the creature under the hand; with the leash on, pressed on another creature it ties the leash to that one
	// instead (see the leash's input).
	if (rightLetGo || (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT))
	{
		// Letting go of the rope of the tied leash: the leash's length becomes how far the hand is from what it is tied to,
		// and the creature goes to the place under the hand
		if (Locator::leashSystem::value().RopeHand(LocalInputPlayer()) == leash_snap::Hand::Holding &&
		    Locator::pickingSystem::has_value())
		{
			Locator::leashSystem::value().LetGoOfRope(LocalInputPlayer(), Locator::pickingSystem::value().GetPick().point);
		}
		magic.ReleaseAction();
		// Let go quickly on a fireball, it is tapped
		magic.ReleaseFireBall(machine_clock::Ticks(), Locator::time::value().GetTurn());
		// Let go while a press on a creature waits, the press is a tap. The held leash takes it as an order to act on the
		// creature.
		if (const auto tapped = creatureHand.ReleaseWait(); tapped.has_value())
		{
			Locator::leashSystem::value().TapThing(LocalInputPlayer(), *tapped);
		}
	}
}

void Game::ProcessActionPress(const SDL_Event& event, bool inTemple, bool handTakesPresses,
                              ecs::systems::CreatureHandSystemInterface& creatureHand,
                              ecs::systems::CreatureFightSystemInterface& fights, ecs::systems::MagicSystemInterface& magic,
                              ecs::systems::HandGrabSystemInterface* handGrab, bool handHoldsThing,
                              const ecs::components::InterfaceLimits& interfaceLimits, bool mayHold)
{
	if (handTakesPresses && !inTemple && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT &&
	    !Locator::debugGui::value().IsMouseOverWindow())
	{
		// After the button let go of a creature, the Action button does nothing until the world ends the lock on it
		_actionPressTaken = creatureHand.WaitsForLockEnd() || (magic.IsHandBusy() && magic.PressAction());
		// While the camera shows a duel of the player's creature and the hand is empty, every Action press goes to the
		// fight and to nothing else
		const bool fightTakesPress = fights.DirectsFight();
		// Holding a thing, the hand makes ready to throw it
		if (!_actionPressTaken && handHoldsThing)
		{
			_actionPressTaken = handGrab->Press(machine_clock::Ticks(), Locator::time::value().GetTurn());
		}
		// With an empty hand and the leash held, a quick second press near the first is a double tap, which ties the leash
		// to the thing under the cursor or unties it, before anything else takes it
		if (!_actionPressTaken && !handHoldsThing && !magic.IsHandBusy())
		{
			const auto screenSize = Locator::windowing::value().GetSize();
			const auto cursor = glm::vec2(event.button.x, event.button.y);
			glm::vec3 rayOrigin;
			glm::vec3 rayDirection;
			Locator::camera::value().DeprojectScreenToWorld(
			    cursor / static_cast<glm::vec2>(glm::max(screenSize, glm::ivec2(1))), rayOrigin, rayDirection);
			_actionPressTaken = Locator::leashSystem::value().PressAction(LocalInputPlayer(), rayOrigin, rayDirection, cursor,
			                                                              machine_clock::Ticks());
		}
		// With an empty hand on the tip bubble, the hand grips it and the words follow the hand until the button is let go
		if (!_actionPressTaken && !fightTakesPress && !handHoldsThing && !magic.IsHandBusy() &&
		    Locator::tipBubbleSystem::has_value() && Locator::tipBubbleSystem::value().IsUnderHand())
		{
			Locator::tipBubbleSystem::value().Grip();
			_actionPressTaken = true;
		}
		// With an empty hand over the rope of the tied leash, the hand takes hold of the rope
		if (!_actionPressTaken && !fightTakesPress && !handHoldsThing && !magic.IsHandBusy() &&
		    Locator::leashSystem::value().PressRope(LocalInputPlayer()))
		{
			_actionPressTaken = true;
		}
		// A lesson can let an empty hand act only on a highlight that a script made. This limit also stops a press on a
		// fireball, which is not a highlight.
		std::optional<entt::entity> picked;
		if (Locator::pickingSystem::has_value())
		{
			picked = Locator::pickingSystem::value().GetPick().object;
		}
		const bool highlightUnderHand = picked.has_value() && Locator::entitiesRegistry::value().Valid(*picked) &&
		                                Locator::entitiesRegistry::value().AllOf<ecs::components::ScriptHighlight>(*picked);
		const bool mayPress = handHoldsThing || script::interface_levels::MayPress(interfaceLimits, highlightUnderHand);
		// With an empty hand, a fireball under it takes the press. A one-shot globe is taken hold of as any thing is.
		if (!_actionPressTaken && !fightTakesPress && !handHoldsThing && !magic.IsHandBusy() && mayPress)
		{
			_actionPressTaken = magic.PressFireBall(machine_clock::Ticks(), Locator::time::value().GetTurn());
		}
		if (!_actionPressTaken)
		{
			const auto screenSize = Locator::windowing::value().GetSize();
			glm::vec3 rayOrigin;
			glm::vec3 rayDirection;
			Locator::camera::value().DeprojectScreenToWorld(glm::vec2(event.button.x, event.button.y) /
			                                                    static_cast<glm::vec2>(glm::max(screenSize, glm::ivec2(1))),
			                                                rayOrigin, rayDirection);
			// While the player's creature duels, the Action button adds a move to its queue
			if (fights.Press(rayOrigin, rayDirection, creature_fight::Button::Action, machine_clock::Ticks(),
			                 Locator::time::value().GetTurn()))
			{
				_actionPressTaken = true;
				_fightButton = creature_fight::Button::Action;
			}
			// A press that the fight refuses does nothing: the hand does not take hold of a creature or a thing then
			else if (fightTakesPress)
			{
				_actionPressTaken = true;
			}
			const auto& leashes = Locator::leashSystem::value();
			const auto own = leashes.PlayersCreature(LocalInputPlayer());
			const auto under = creatureHand.CreatureUnderCursor();
			const bool tying =
			    under.has_value() && own.has_value() &&
			    creature_hand::PressOn(*under == *own, leashes.IsLeashed(*own)) == creature_hand::CreaturePress::Leash;
			if (!_actionPressTaken && under.has_value() && !tying && mayHold)
			{
				_actionPressTaken = true;
				if (handGrab != nullptr)
				{
					handGrab->ClickThing(*under);
				}
				// With the leash of the player's creature held loose (not tied), a press on a creature that the hand may
				// hold first waits for a tap. A press on a creature that the hand may not hold sends no leash click.
				const bool leashHeldLoose = own.has_value() && leashes.HolderPoint(*own).has_value();
				switch (creature_hand::PressStartOn(creatureHand.MayHold(*under), leashHeldLoose))
				{
				case creature_hand::PressStart::Hold:
					creatureHand.Grab();
					break;
				case creature_hand::PressStart::WaitForTap:
					creatureHand.WaitForTap(*under, machine_clock::Ticks(), Locator::time::value().GetTurn());
					break;
				case creature_hand::PressStart::TapAtOnce:
					// The leash takes the tap as an order to act on the creature
					Locator::leashSystem::value().TapThing(LocalInputPlayer(), *under);
					break;
				case creature_hand::PressStart::Nothing:
					break;
				}
			}
		}
		// A town's totem under the hand is taken hold of, to slide it up and down
		if (!_actionPressTaken && !magic.IsHandBusy() && !handHoldsThing && mayHold &&
		    Locator::villageTotemSystem::has_value() && Locator::pickingSystem::has_value())
		{
			auto& totems = Locator::villageTotemSystem::value();
			if (picked.has_value())
			{
				if (const auto totem = totems.TotemOf(*picked); totem.has_value() && totems.Grip(*totem, LocalInputPlayer()))
				{
					_actionPressTaken = true;
					_totemPointer = _mousePosition;
				}
			}
		}
		// A puzzle totem under the hand is taken hold of, to slide it up out of the land and down into it
		if (!_actionPressTaken && !magic.IsHandBusy() && !handHoldsThing && mayHold && picked.has_value() &&
		    Locator::puzzleTotemSystem::has_value() && Locator::pickingSystem::has_value())
		{
			const auto point = Locator::pickingSystem::value().GetPick().point;
			if (point.has_value() && Locator::puzzleTotemSystem::value().Grip(*picked, LocalInputPlayer(), *point))
			{
				_actionPressTaken = true;
				_totemPointer = _mousePosition;
			}
		}
		// Otherwise the hand takes hold of a thing under it, if the lesson lets it
		if (!_actionPressTaken && !magic.IsHandBusy() && handGrab != nullptr && mayPress)
		{
			_actionPressTaken = handGrab->Press(machine_clock::Ticks(), Locator::time::value().GetTurn());
		}
		// A press that the lesson refuses does nothing more: the leash gives no order and taps no post
		if (!_actionPressTaken && !mayPress)
		{
			_actionPressTaken = true;
		}
	}
}

void Game::ProcessMovePress(const SDL_Event& event, bool middleMouseButton, bool inTemple, bool handTakesPresses,
                            ecs::systems::CreatureHandSystemInterface& creatureHand,
                            ecs::systems::CreatureFightSystemInterface& fights, ecs::systems::MagicSystemInterface& magic)
{
	if (handTakesPresses && !magic.IsHandBusy() && !inTemple && event.type == SDL_MOUSEBUTTONDOWN &&
	    event.button.button == SDL_BUTTON_LEFT && !middleMouseButton)
	{
		const auto screenSize = Locator::windowing::value().GetSize();
		glm::vec3 rayOrigin;
		glm::vec3 rayDirection;
		Locator::camera::value().DeprojectScreenToWorld(glm::vec2(event.button.x, event.button.y) /
		                                                    static_cast<glm::vec2>(glm::max(screenSize, glm::ivec2(1))),
		                                                rayOrigin, rayDirection);
		// The second press of a double click on a creature, anyone's, locks the camera onto it
		const bool doubleClicked =
		    Locator::creatureModeSystem::has_value() &&
		    Locator::creatureModeSystem::value().Press({.milliseconds = event.button.timestamp,
		                                                .screen = glm::vec2(event.button.x, event.button.y),
		                                                .creature = creatureHand.CreatureUnderCursor()});
		// While the player's creature duels, the Move button makes a move at once in place of those queued
		if (!doubleClicked && fights.Press(rayOrigin, rayDirection, creature_fight::Button::Move, machine_clock::Ticks(),
		                                   Locator::time::value().GetTurn()))
		{
			_fightButton = creature_fight::Button::Move;
		}
		else if (!doubleClicked)
		{
			PlayHandGrabSound();
		}
	}
}

bool Game::ProcessKeyPress(const SDL_Event& event, windowing::WindowingInterface& window, Camera& camera)
{
	switch (event.key.keysym.sym)
	{
	case SDLK_ESCAPE:
		// Not while a script has the cinema bars in for its scene
		if (!Locator::cinematicDirectorSystem::value().IsInterfaceActive())
		{
			break;
		}
		return false;
	case SDLK_f:
		// A key that the player bound to an action of the game stays the game's
		if (Locator::config::value().background ||
		    input::developer_keys::GameUsesKey(Locator::gameActionSystem::value().GetKeyBindings(), k_Platform,
		                                       event.key.keysym.scancode, event.key.keysym.mod))
		{
			break;
		}
		window.SetDisplayMode(windowing::DisplayMode::Fullscreen);
		break;
	case SDLK_p:
		// The game pauses only with no Shift, Ctrl or Alt held
		if ((event.key.keysym.mod & (KMOD_SHIFT | KMOD_CTRL | KMOD_ALT)) == 0)
		{
			Locator::time::value().SetPlayerPause(!IsPaused());
		}
		break;
	default:
	{
		namespace keys = input::platform_keys;
		const auto key = event.key.keysym.sym;
		const auto mod = event.key.keysym.mod;
		const keys::Modifiers held {
		    .shift = (mod & KMOD_SHIFT) != 0,
		    .ctrl = (mod & KMOD_CTRL) != 0,
		    .alt = (mod & KMOD_ALT) != 0,
		};
		// F1 is the game's Help key, so the renderer's statistics are on another key. It is a debug key: a build
		// without the developer tools, and a live network game, ignore it. A key that the player bound to an
		// action of the game stays the game's.
		if (key == keys::RendererStatisticsKey(k_Platform))
		{
			if (developer_tools::Allowed(developer_tools::Action::DebugKey) &&
			    !input::developer_keys::GameUsesKey(Locator::gameActionSystem::value().GetKeyBindings(), k_Platform,
			                                        event.key.keysym.scancode, mod))
			{
				// With Ctrl and Shift, the key makes the graphics device again, to test the recovery from a lost
				// device
				auto& renderer = Locator::rendererInterface::value();
				if (keys::IsDeviceRestoreKey(k_Platform, key, held))
				{
					renderer.RequestDeviceRestore(graphics::device_restore::Request::MakeAgain);
				}
				else
				{
					renderer.SetDebug(!renderer.GetDebug());
				}
			}
			break;
		}
		// The slower key halves the game's speed, to half of normal at the least. The faster key doubles it, to
		// twice normal at the most.
		if (const auto speed = keys::SpeedKeyFor(k_Platform, key, held); speed != keys::SpeedKey::None)
		{
			auto& clock = Locator::time::value();
			clock.SetSpeed(speed == keys::SpeedKey::Slower ? std::max(clock.GetSpeed() * 0.5f, 0.5f)
			                                               : std::min(clock.GetSpeed() + clock.GetSpeed(), 2.0f));
			break;
		}
		if (key >= SDLK_1 && key <= SDLK_7)
		{
			if (!BookmarkKeysAllowed() || !Locator::cameraBookmarkSystem::value().IsEnabled())
			{
				break;
			}
			auto& bookmarks = Locator::cameraBookmarkSystem::value();
			const auto slot = static_cast<size_t>(key - SDLK_1);
			switch (keys::BookmarkKeyFor(k_Platform, held))
			{
			case keys::BookmarkKey::Clear:
				if (bookmarks.IsSet(slot))
				{
					bookmarks.ClearBookmark(slot);
				}
				break;
			case keys::BookmarkKey::Set:
			{
				// The bookmark keeps the place of the hand, or the thing under the hand
				const auto left = Locator::handSystem::value().GetDrawnHandPosition();
				const auto right =
				    Locator::handSystem::value()
				        .GetOwnHandPositions()[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Right)];
				if (left.has_value() || right.has_value())
				{
					std::optional<entt::entity> thing;
					if (Locator::pickingSystem::has_value())
					{
						thing = Locator::pickingSystem::value().GetPick().object;
					}
					bookmarks.SetBookmark(slot, left.value_or(right.value_or(glm::zero<glm::vec3>())), thing,
					                      camera.GetOrigin());
				}
				break;
			}
			case keys::BookmarkKey::ToggleFollow:
				bookmarks.ToggleFollow(slot);
				break;
			case keys::BookmarkKey::Jump:
				bookmarks.JumpTo(slot);
				break;
			case keys::BookmarkKey::None:
				break;
			}
			break;
		}
		const auto move = keys::BookmarkMoveFor(k_Platform, key, held);
		if (move == keys::BookmarkMove::None || !BookmarkKeysAllowed())
		{
			break;
		}
		auto& bookmarks = Locator::cameraBookmarkSystem::value();
		switch (move)
		{
		case keys::BookmarkMove::Back:
			bookmarks.JumpBack();
			break;
		case keys::BookmarkMove::Previous:
			bookmarks.JumpToPrevious();
			break;
		case keys::BookmarkMove::Next:
			bookmarks.JumpToNext();
			break;
		case keys::BookmarkMove::NextTown:
		{
			std::optional<entt::entity> thing;
			if (Locator::pickingSystem::has_value())
			{
				thing = Locator::pickingSystem::value().GetPick().object;
			}
			bookmarks.FlyToNextTown(thing);
			break;
		}
		case keys::BookmarkMove::None:
			break;
		}
		break;
	}
	}
	return true;
}

bool Game::BookmarkKeysAllowed()
{
	if (!Locator::cinematicDirectorSystem::value().IsInterfaceActive())
	{
		return false;
	}
	if (!Locator::cameraHelpSystem::has_value())
	{
		return true;
	}
	const auto& help = Locator::cameraHelpSystem::value().Get();
	return help.cameraKeys && help.placeKeys;
}

void Game::ProcessTempleRoomKeys()
{
	if (!Locator::temple::has_value())
	{
		return;
	}
	// The keys for the temple's rooms take the player into the temple at that room's path in, and inside the temple
	// they cut to the room
	constexpr std::array<std::pair<input::BindableActionMap, TempleRoom>, 6> k_RoomKeys {{
	    {input::BindableActionMap::ZOOM_TO_INSIDE_TEMPLE, TempleRoom::Main},
	    {input::BindableActionMap::ZOOM_TO_CREATURE_ROOM, TempleRoom::CreatureCave},
	    {input::BindableActionMap::ZOOM_TO_CHALLENGE_ROOM, TempleRoom::Challenge},
	    {input::BindableActionMap::ZOOM_TO_SAVE_GAME_ROOM, TempleRoom::SaveGame},
	    {input::BindableActionMap::ZOOM_TO_OPTIONS_ROOM, TempleRoom::Options},
	    {input::BindableActionMap::ZOOM_TO_LIBRARY, TempleRoom::Credits},
	}};
	const auto& actions = Locator::gameActionSystem::value();
	auto& temple = Locator::temple::value();
	for (const auto& [action, room] : k_RoomKeys)
	{
		if (!actions.Get(action) || !actions.GetChanged(action))
		{
			continue;
		}
		if (temple.Active())
		{
			temple.GoToRoom(room);
		}
		else
		{
			temple.ActivateAfterFrame(room);
		}
		return;
	}
}

void Game::UpdateInput()
{
	auto& profiler = Locator::profiler::value();
	auto& camera = Locator::camera::value();
	// Input events
	{
		auto sdlInput = profiler.BeginScoped(Profiler::Stage::SdlInput);
		// The debug windows' presses of the options screen's actions are made by the action map's frame
		auto& actions = Locator::gameActionSystem::value();
		// The world's camera is the one turned with the mouse, which holds the cursor and the hand still meanwhile
		actions.AllowCursorFreeze(
		    !(Locator::temple::has_value() && Locator::temple::value().Active()) &&
		    dynamic_cast<DefaultWorldCameraModel*>(&camera.GetModel()) != nullptr &&
		    (Locator::cameraHelpSystem::value().Get().features & camera_help::feature::k_RotateAroundMouse) != 0);
		// The land's scripts may take the camera's keys away, as the tutorials do
		actions.SetBlockedActions(Locator::cameraHelpSystem::value().Get().BlockedActions());
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
		FollowWindowSize();
		if (actions.IsCursorFrozen())
		{
			_mousePosition = glm::ivec2(actions.GetMousePosition());
		}
		if (const auto warp = actions.GetCursorWarp(); warp.has_value())
		{
			_mousePosition = *warp;
			_handPointer.Moved();
		}
		// The hand is put under the pointer every frame, moving or not: until the mouse first moves, under the pointer
		// the game keeps, so from a land's first frame it is where the cursor is, or the top left corner while the
		// cursor has never been over the window
		_mousePosition = _handPointer.Frame(
		    _mousePosition, actions.IsPointerKnown() ? std::optional(actions.GetPointerPosition()) : std::nullopt);
		ProcessTempleRoomKeys();
		_shortcutKeys.Update();
	}
}

void Game::SubmitKeyCommands()
{
	// The keys do nothing in the temple. While the debug windows have the keyboard, the controls do not change, so a key
	// that the player pressed would read as a new press on each frame.
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	if (inTemple || Locator::debugGui::value().StealsFocus() || !Locator::gameActionSystem::has_value())
	{
		return;
	}
	using input::BindableActionMap;
	const auto& actions = Locator::gameActionSystem::value();
	auto& commands = Locator::playerCommandSystem::value();
	// The leash shortcuts
	for (const auto action :
	     {BindableActionMap::LEASH_UNLEASH_CREATURE, BindableActionMap::PREVIOUS_LEASH, BindableActionMap::NEXT_LEASH})
	{
		if (const auto key = creature_leash::KeyFor(action);
		    key.has_value() && actions.GetChanged(action) && actions.Get(action))
		{
			commands.Submit(player_commands::LeashKey {.key = *key});
		}
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
		// The debug windows keep off the mouse while the player's input is locked out, and show a notice
		Locator::debugGui::value().SetInputLock(Locator::gameActionSystem::value().IsPlayerInputBlocked(),
		                                        Locator::gameActionSystem::value().GetScriptedPointer().has_value());
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

void Game::UpdateCameraKeys(std::chrono::microseconds deltaTime)
{
	// In the temple, without the interface or while a box has the keyboard, the camera does not read its keys
	auto& actions = Locator::gameActionSystem::value();
	const bool interfaceActive =
	    !Locator::cinematicDirectorSystem::has_value() || Locator::cinematicDirectorSystem::value().IsInterfaceActive();
	if (!game_frame::CameraReadsKeys({
	        .inTemple = Locator::temple::has_value() && Locator::temple::value().Active(),
	        .interfaceActive = interfaceActive,
	        .boxHasKeyboard = actions.BoxHasKeyboard(),
	    }))
	{
		return;
	}
	auto& camera = Locator::camera::value();
	// The keys of the camera act after the camera moved, so that their steps move it in the next frame
	// A miracle's camera path keeps the camera until the player moves it with the movement keys or grips the land,
	// which hand it straight back; meanwhile the player's other camera controls do nothing
	auto& cameraPaths = Locator::cameraPathSystem::value();
	const bool pathHeld = cameraPaths.HoldsCamera();
	cameraPaths.HandlePlayerControl(
	    {.movementKey = actions.GetAny(input::BindableActionMap::MOVE_LEFT, input::BindableActionMap::MOVE_RIGHT,
	                                   input::BindableActionMap::MOVE_FORWARDS, input::BindableActionMap::MOVE_BACKWARDS),
	     .frameMilliseconds = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(deltaTime).count()),
	     .grippingLand = _handGripping && !_handRotating});
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
	hand::script_state::Facts grip {.gripsLand = _handSelectGrip, .otherButton = _handOtherButton};
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

void Game::StartInputTools()
{
	// A tool's run can record the input of the hand from the first frame of the next land, for a later run to make again
	if (_simulation.has_value() && _simulation->recordInputPath.has_value() && Locator::inspector::has_value())
	{
		Locator::inspector::value().RecordInputOnLoad(*_simulation->recordInputPath);
	}
	// A tool's run can give the hand recorded input from the first frame of the next land
	if (_simulation.has_value() && _simulation->inputPath.has_value() && Locator::inspector::has_value())
	{
		if (const auto error = Locator::inspector::value().ReplayOnLoad(*_simulation->inputPath); error.has_value())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The recorded input cannot start: {}", *error);
		}
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
	};
	if (_menuClosedAt.has_value())
	{
		situation.sinceMenuClosed =
		    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - *_menuClosedAt);
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
	// A profile that keeps no bindings has the game's defaults
	input::KeyBindingTable bindings = input::DefaultKeyBindings();
	if (Locator::playerProfileSystem::has_value())
	{
		if (const auto profile = Locator::playerProfileSystem::value().GetCurrentProfile(); profile.has_value())
		{
			bindings = input::rebinding::FromKept(profile->keys);
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
	const bool click = TakeDialogueClick() && !demoPlaying;
	const bool skipKey = !demoPlaying && DialogueSkipKeyDown();
	if (!click && !skipKey)
	{
		return;
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

bool Game::TakeDialogueClick()
{
	return std::exchange(_dialogueClick, false);
}

bool Game::DialogueSkipKeyDown() const
{
	// The key comes from the player's keyboard, which the input lock keeps out also when the window has the keys
	const bool playerKeys =
	    !Locator::gameActionSystem::has_value() || !Locator::gameActionSystem::value().IsPlayerInputBlocked();
	const auto* keys = playerKeys ? SDL_GetKeyboardState(nullptr) : nullptr;
	return keys != nullptr && keys[SDL_SCANCODE_KP_ENTER] != 0;
}
