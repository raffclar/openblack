/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>
#include <cmath>
#include <cstdint>
#include <ctime>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iterator>
#include <limits>
#include <numbers>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <LHVMTypes.h>
#include <SaveSlots.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/matrix.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "CHLApi/NativeHelpers.h"
#include "CHLApi/Natives.h"
#include "ECS/Components/HandClicked.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FootballSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "Enums.h"
#include "Hand/HandClickRules.h"
#include "Hand/ScriptHandState.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "ScriptHeaders/ScriptLessonNatives.h"

namespace openblack::chlapi
{

using openblack::Locator;
using openblack::ecs::systems::HandSystemInterface;

/// What the player last clicked, kept on their hand; none without a hand. A game on one computer has the click of its
/// own player, the first player. In a network game the turn writes the clicks, and the scripts read the clicks of the
/// first player on every computer.
ecs::components::HandClicked* LocalHandClicked()
{
	if (!Locator::handSystem::has_value())
	{
		return nullptr;
	}
	const auto hand = Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
	auto& registry = Locator::entitiesRegistry::value();
	if (hand == entt::null || !registry.Valid(hand))
	{
		return nullptr;
	}
	if (auto* clicked = registry.TryGet<ecs::components::HandClicked>(hand))
	{
		return clicked;
	}
	return &registry.Assign<ecs::components::HandClicked>(hand);
}

void GetHandPosition() // 042 GET_HAND_POSITION
{
	// The hand's position is given also in the temple, with a message
	if (PlayerInsideTemple())
	{
		ScriptMessage("Hand in citadel");
	}
	const auto reported = Locator::handSystem::value().GetPlayerHandPositions()[0];
	PushVec(reported.value_or(glm::vec3(0.0f)));
}

void ClearClickedObject() // 156 CLEAR_CLICKED_OBJECT
{
	if (auto* clicked = LocalHandClicked())
	{
		hand_click::ClearThing(*clicked);
	}
}

void ClearClickedPosition() // 157 CLEAR_CLICKED_POSITION
{
	if (auto* clicked = LocalHandClicked())
	{
		hand_click::ClearPlace(*clicked);
	}
}

void PositionClicked() // 158 POSITION_CLICKED
{
	const auto radius = Popf();
	const auto position = PopVec();
	const auto* clicked = LocalHandClicked();
	// Along the ground: the position's height doesn't count
	const map_coords::MapCoords at {.x = map_coords::ToFixed(position.x), .z = map_coords::ToFixed(position.z)};
	Pushb(clicked != nullptr && hand_click::IsPlaceClicked(*clicked, at, radius));
}

void GetObjectHandIsOver() // 160 GET_OBJECT_HAND_IS_OVER
{
	// The thing under the cursor of the player's hand, from the last pick that a turn did. The game warns that a network
	// game gives each computer the thing under its own hand.
	if (Locator::time::has_value() && Locator::time::value().IsNetworkGame())
	{
		ScriptMessage("This is not multiplayer friendly");
	}
	const auto object = Locator::handSystem::has_value()
	                        ? Locator::handSystem::value().GetReportedPickOf(PlayerNames::PLAYER_ONE)
	                        : entt::entity {entt::null};
	const auto& registry = Locator::entitiesRegistry::value();
	PushObject(object != entt::null && registry.Valid(object) ? object : entt::entity {entt::null});
}

void PlayGesture() // 204 PLAY_GESTURE
{
	// The shape of the gesture shows on the land as if this computer's player drew it: its square, so many metres
	// across, goes from the point towards the east and the south. No gesture shows for gesture 0.
	const auto size = Popf();
	const auto corner = PopVec();
	const auto gesture = static_cast<GestureType>(Pop().intVal);
	if (Locator::gestureSystem::has_value())
	{
		Locator::gestureSystem::value().ShowGestureOnLand(gesture, corner, size, std::nullopt);
	}
}

void HasMouseWheel() // 206 HAS_MOUSE_WHEEL
{
	// The view report of the player tells whether its mouse has a wheel
	const auto view = ScriptView();
	Pushb(view.has_value() && view->mouseWheel);
}

void NumMouseButtons() // 207 NUM_MOUSE_BUTTONS
{
	// The view report of the player tells how many buttons its mouse has
	const auto view = ScriptView();
	Pushf(view.has_value() ? static_cast<float>(view->mouseButtons) : 0.0f);
}

void PlayHandDemo() // 266 PLAY_HAND_DEMO
{
	const auto withoutHandModify = Pop().intVal != 0;
	const auto withPause = Pop().intVal != 0;
	const auto name = PopString();
	Locator::handDemoSystem::value().Play(name, CurrentTask(), withPause, withoutHandModify);
}

void IsPlayingHandDemo() // 267 IS_PLAYING_HAND_DEMO
{
	// The scripts' "hand demonstration played": true when none plays
	Pushb(!Locator::handDemoSystem::value().IsPlaying(0));
}

void HandDemoTrigger() // 336 HAND_DEMO_TRIGGER
{
	// Whether the demonstration reached a mark; asking lets a demonstration held there go on
	Pushb(Locator::handDemoSystem::value().TakeTrigger());
}

void GetHandState() // 413 GET_HAND_STATE
{
	hand::script_state::Facts facts;
	facts.interfaceActive =
	    !Locator::cinematicDirectorSystem::has_value() || Locator::cinematicDirectorSystem::value().IsInterfaceActive();
	facts.inTemple = PlayerInsideTemple();
	// The grip of the land, the other button and the turn of the camera come from the view report of the player
	if (const auto view = ScriptView(); view.has_value())
	{
		facts.gripsLand = view->gripsLand;
		facts.otherButton = view->otherButton;
		facts.cameraTurned = view->cameraTurned;
	}
	if (Locator::cameraHelpSystem::has_value())
	{
		facts.cameraFeatures = Locator::cameraHelpSystem::value().Get().features;
	}
	Pushi(static_cast<int32_t>(hand::script_state::StateOf(facts)));
}

void KeyDown() // 419 KEY_DOWN
{
	// The key comes by the game's number of the key. The view report of the player tells the keys held.
	const auto key = Pop().intVal;
	const auto view = ScriptView();
	Pushb(view.has_value() && script::lesson_natives::KeyDown(key, view->keysHeld));
}

void GetObjectClicked() // 421 GET_OBJECT_CLICKED
{
	const auto* clicked = LocalHandClicked();
	const auto object = clicked != nullptr ? clicked->thing : entt::entity {entt::null};
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		Pusho(0);
		return;
	}
	// The thing found takes a place in the scripts' table, the game's own
	Locator::scriptObjects::value().Register(object, false);
	Pusho(static_cast<uint32_t>(object));
}

void SetHandDemoKeys() // 459 SET_HAND_DEMO_KEYS
{
	// The game does nothing with it
	static_cast<void>(Pop());
}

} // namespace openblack::chlapi
