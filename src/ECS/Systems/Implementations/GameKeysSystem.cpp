/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameKeysSystem.h"

#include <algorithm>
#include <array>
#include <utility>

#include <SDL.h>

#include "3D/TempleInteriorInterface.h"
#include "3D/TempleRoom.h"
#include "Camera/Camera.h"
#include "Commands/PlayerCommands.h"
#include "Common/DeveloperToolsGate.h"
#include "Common/Platform.h"
#include "Creature/LeashKeys.h"
#include "Debug/DebugGuiInterface.h"
#include "ECS/Components/Physics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "EngineConfig.h"
#include "GameDetail.h"
#include "Graphics/RendererInterface.h"
#include "Input/DeveloperKeys.h"
#include "Input/GameActionMapInterface.h"
#include "Input/KeyBindings.h"
#include "Input/PlatformKeys.h"
#include "Input/ShortcutKeys.h"
#include "Locator.h"
#include "Temple/TempleKeys.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using openblack::game_detail::LocalInputPlayer;

bool GameKeysSystem::KeyDown(const SDL_Event& event, bool inTemple)
{
	// Out of the temple, an action of the options screen that has the key and its modifier comes first. In the temple
	// the keys do other things.
	const auto binding = inTemple ? input::KeyDownBinding::None : BindingOfKeyDown(event);
	if (binding == input::KeyDownBinding::TakesKey)
	{
		return true;
	}
	// The M key opens the miracle selection, and the R key calls the last miracle again
	if ((event.key.keysym.scancode == SDL_SCANCODE_M || event.key.keysym.scancode == SDL_SCANCODE_R) && !inTemple &&
	    Locator::gestureSystem::has_value())
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
		return true;
	}
	return ProcessKeyPress(event, binding);
}

input::KeyDownBinding GameKeysSystem::BindingOfKeyDown(const SDL_Event& event)
{
	// Escape does its work before the actions get the key
	if (event.key.keysym.sym == SDLK_ESCAPE || !Locator::gameActionSystem::has_value())
	{
		return input::KeyDownBinding::None;
	}
	const auto blocked = Locator::cameraHelpSystem::has_value() ? Locator::cameraHelpSystem::value().Get().BlockedActions()
	                                                            : input::BindableActionMap::NONE;
	const bool scriptHoldsScreen =
	    Locator::cinematicDirectorSystem::has_value() && !Locator::cinematicDirectorSystem::value().IsInterfaceActive();
	return input::KeyDownBindingFor(Locator::gameActionSystem::value().GetKeyBindings(), event.key.keysym.scancode,
	                                event.key.keysym.mod, blocked, scriptHoldsScreen);
}

bool GameKeysSystem::ProcessKeyPress(const SDL_Event& event, input::KeyDownBinding binding)
{
	auto& window = Locator::windowing::value();
	auto& camera = Locator::camera::value();
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
			Locator::time::value().SetPlayerPause(!Locator::time::value().IsPaused());
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
		// twice normal at the most. These keys do not act when an action has the key.
		if (const auto speed = keys::SpeedKeyFor(k_Platform, key, held);
		    speed != keys::SpeedKey::None && binding == input::KeyDownBinding::None)
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
				// The bookmark keeps the point under the cursor from the last report of the hand, or the thing under the
				// hand. It keeps the camera from the eye that the last frame drew.
				const auto& hands = Locator::handSystem::value();
				if (const auto point = hands.GetReportedPointUnderCursor(hands.GetOwnPlayer()); point.has_value())
				{
					std::optional<entt::entity> thing;
					if (Locator::pickingSystem::has_value())
					{
						thing = Locator::pickingSystem::value().GetPick().object;
					}
					bookmarks.SetBookmark(slot, *point, thing, camera.GetOrigin(Camera::Interpolation::Drawn));
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

bool GameKeysSystem::BookmarkKeysAllowed()
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

namespace
{
/// What the keys of the temple's rooms depend on now
openblack::temple::keys::RoomKeyFacts TempleRoomKeyFacts(bool insideTemple)
{
	openblack::temple::keys::RoomKeyFacts facts {.insideTemple = insideTemple};
	if (Locator::cinematicDirectorSystem::has_value())
	{
		const auto& director = Locator::cinematicDirectorSystem::value();
		facts.scriptBars = director.IsWideScreenOn() && director.GetWideScreenOwner() != 0;
	}
	if (Locator::entitiesRegistry::has_value())
	{
		const auto& registry = Locator::entitiesRegistry::value();
		facts.gameOver = registry.Context().gameOver;
		if (Locator::worshipSiteSystem::has_value())
		{
			const auto own = Locator::worshipSiteSystem::value().TempleOf(LocalInputPlayer());
			if (own != entt::null)
			{
				const auto* progress = registry.TryGet<const ecs::components::BuildProgress>(own);
				facts.templeBuilt = progress == nullptr || progress->built >= 1.0f;
			}
		}
	}
	return facts;
}
} // namespace

void GameKeysSystem::UpdateTempleRoomKeys()
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
		if (!openblack::temple::keys::RoomKeyActs(TempleRoomKeyFacts(temple.Active())))
		{
			return;
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

void GameKeysSystem::SubmitKeyCommands()
{
	// While the debug windows have the keyboard, the controls do not change, so a key that the player pressed would read
	// as a new press on each frame
	if (Locator::debugGui::value().StealsFocus() || !Locator::gameActionSystem::has_value())
	{
		return;
	}
	// Inside the temple, only the press of a mouse button or the wheel does the leash actions
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	using input::BindableActionMap;
	const auto& actions = Locator::gameActionSystem::value();
	auto& commands = Locator::playerCommandSystem::value();
	// The leash shortcuts
	for (const auto action :
	     {BindableActionMap::LEASH_UNLEASH_CREATURE, BindableActionMap::PREVIOUS_LEASH, BindableActionMap::NEXT_LEASH})
	{
		if (const auto key = creature_leash::KeyFor(action);
		    key.has_value() && actions.GetChanged(action) && actions.Get(action) &&
		    input::PressDoesAction(inTemple, input::PressedByMouse(actions, action)))
		{
			commands.Submit(player_commands::LeashKey {.key = *key});
		}
	}
}
