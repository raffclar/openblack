/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ShortcutKeys.h"

#include <algorithm>
#include <optional>
#include <stdexcept>

#include <SDL_mouse.h>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/ScriptCameraModel.h"
#include "Camera/StartCamera.h"
#include "Common/MachineClock.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerLabel.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpMessageSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "GameActionMapInterface.h"
#include "Gui/VillagerLabels.h"
#include "Help/HelpProfile.h"
#include "Help/HelpQuery.h"
#include "LocalPlayer.h"
#include "Locator.h"

namespace openblack::input
{
namespace
{
/// Where the temple of a player stands, if the player has one
[[nodiscard]] std::optional<glm::vec3> PlayerTemplePosition(PlayerNames player)
{
	std::optional<glm::vec3> position;
	Locator::entitiesRegistry::value().Each<const ecs::components::Temple, const ecs::components::Transform>(
	    [&position, player](const ecs::components::Temple& temple, const ecs::components::Transform& transform) {
		    if (temple.owner == player)
		    {
			    position = transform.position;
		    }
	    });
	return position;
}

/// The Show Villager Names key puts the named villagers' names up or down; the Show Villager Details key the details
/// of the living thing under the hand, or else every villager's
void ToggleVillagerLabels(bool names, bool details)
{
	using namespace ecs::components;
	if (!names && !details)
	{
		return;
	}
	const bool inTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	if (!gui::villager_labels::KeysAct(!Locator::cinematicDirectorSystem::value().IsInterfaceActive(), inTemple))
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& context = registry.Context();
	if (names)
	{
		context.villagerNamesShown = !context.villagerNamesShown;
	}
	if (!details)
	{
		return;
	}
	std::optional<entt::entity> pointed;
	if (Locator::pickingSystem::has_value())
	{
		pointed = Locator::pickingSystem::value().GetPick().object;
	}
	const bool overLiving =
	    pointed.has_value() && registry.Valid(*pointed) && registry.AnyOf<Villager, Animal, Creature>(*pointed);
	if (gui::villager_labels::DetailsKeyToggles(overLiving) == gui::villager_labels::DetailsToggle::Pointed)
	{
		if (registry.AllOf<DetailsShown>(*pointed))
		{
			registry.Remove<DetailsShown>(*pointed);
		}
		else
		{
			registry.Assign<DetailsShown>(*pointed);
		}
		return;
	}
	context.villagerDetailsShown = !context.villagerDetailsShown;
}
/// Whether the press of an action came from the mouse: its mouse button is down, or the wheel turns the way it has
bool PressedByMouse(const GameActionInterface& actions, BindableActionMap action)
{
	const auto bindings = actions.GetKeyBindings();
	const auto binding = std::ranges::find(bindings, action, &KeyBinding::action);
	if (binding == bindings.end())
	{
		return false;
	}
	const uint32_t buttons = actions.GetPointerButtons();
	switch (binding->mouse)
	{
	case MouseInput::LeftButton:
		return (buttons & SDL_BUTTON_LMASK) != 0;
	case MouseInput::MiddleButton:
		return (buttons & SDL_BUTTON_MMASK) != 0;
	case MouseInput::RightButton:
		return (buttons & SDL_BUTTON_RMASK) != 0;
	case MouseInput::WheelUp:
		return actions.GetMouseWheelDelta() > 0.0f;
	case MouseInput::WheelDown:
		return actions.GetMouseWheelDelta() < 0.0f;
	case MouseInput::None:
		break;
	}
	return false;
}
} // namespace

void ShortcutKeys::Update()
{
	if (!Locator::gameActionSystem::has_value())
	{
		return;
	}
	const auto& actions = Locator::gameActionSystem::value();
	const auto pressed = [&actions](BindableActionMap action) { return actions.GetChanged(action) && actions.Get(action); };

	// Pressing the key of an action whose feature isn't built yet says so, so the press is seen to be taken
	for (const auto& binding : actions.GetKeyBindings())
	{
		if (binding.status == BindStatus::NotYetImplemented && pressed(binding.action))
		{
			SPDLOG_LOGGER_INFO(spdlog::get("input"), "{} ({}) is not implemented yet", binding.name,
			                   binding.key.has_value() ? KeyChordName(*binding.key)
			                                           : std::string(MouseInputName(binding.mouse)));
		}
	}

	// TODO(raffclar): Zoom To Creature and the creature's room key belong to the creature mode work, and the leash keys
	// to the leash work, which read them themselves.

	// The Help key asks the advisors about what is in the hand or under it
	if (pressed(BindableActionMap::HELP) && Locator::helpMessageSystem::has_value() &&
	    help_query::HelpPressAsks(Locator::temple::has_value() && Locator::temple::value().Active(),
	                              PressedByMouse(actions, BindableActionMap::HELP)))
	{
		Locator::helpMessageSystem::value().HelpQuery();
	}

	// The quick save and the quick load keys
	if (Locator::saveGameSystem::has_value())
	{
		if (pressed(BindableActionMap::QUICK_SAVE))
		{
			Locator::saveGameSystem::value().QuickSave();
		}
		if (pressed(BindableActionMap::QUICK_LOAD))
		{
			Locator::saveGameSystem::value().QuickLoad();
		}
	}

	// The villagers' names and details are toggled, as the game does
	ToggleVillagerLabels(pressed(BindableActionMap::SHOW_VILLAGER_NAMES), pressed(BindableActionMap::SHOW_VILLAGER_DETAILS));

	const bool temple = pressed(BindableActionMap::ZOOM_TO_TEMPLE);
	const bool realm = pressed(BindableActionMap::ZOOM_TO_REALM);
	if (!temple && !realm)
	{
		return;
	}
	// The camera isn't the player's while a script has the cinema bars in, nor inside the temple
	if (!Locator::cinematicDirectorSystem::value().IsInterfaceActive() ||
	    (Locator::temple::has_value() && Locator::temple::value().Active()))
	{
		return;
	}
	// A script that holds the camera does not let it go: the keys do nothing, and no tap counts
	auto& camera = Locator::camera::value();
	if (!zoom_to::KeysTakeCamera(dynamic_cast<const ScriptCameraModel*>(&camera.GetModel()) != nullptr
	                                 ? zoom_to::Holder::Script
	                                 : zoom_to::Holder::World))
	{
		return;
	}
	// The keys fly the world camera: the creature's follow camera and a miracle's camera path first give the camera
	// back
	if (Locator::creatureModeSystem::has_value() && Locator::creatureModeSystem::value().IsActive())
	{
		Locator::creatureModeSystem::value().Leave();
	}
	if (Locator::cameraPathSystem::has_value() && Locator::cameraPathSystem::value().HoldsCamera())
	{
		Locator::cameraPathSystem::value().Stop();
	}
	const auto& land = Locator::terrainSystem::value();
	glm::vec2 islandCentre;
	try
	{
		// Half way between the first and the last block of land
		const auto blocks = land.GetIndexExtent();
		islandCentre = zoom_to::IslandCentre(blocks.minimum, blocks.maximum);
	}
	catch (const std::runtime_error&)
	{
		// No land is loaded
		return;
	}
	const zoom_to::CameraView current {.origin = camera.GetOrigin(), .focus = camera.GetFocus()};
	const auto now = machine_clock::Ticks();
	zoom_to::Press press;
	if (temple)
	{
		// The world camera finds how far ahead the land is for a single tap. Another camera, such as the fight view,
		// gives no single tap view.
		std::optional<float> clickDistance;
		auto* world = dynamic_cast<DefaultWorldCameraModel*>(&camera.GetModel());
		if (world != nullptr && _zoomTo.IsSingleTap(now))
		{
			clickDistance = world->RefreshClickDistance(camera);
		}
		// The key flies to the temple of the player of this computer
		const auto templePosition =
		    PlayerTemplePosition(LocalPlayer(Locator::handSystem::has_value() ? &Locator::handSystem::value() : nullptr));
		press = zoom_to::TempleKey(now, current,
		                           templePosition.has_value() ? std::optional(glm::vec2(templePosition->x, templePosition->z))
		                                                      : std::nullopt,
		                           islandCentre, clickDistance);
	}
	else
	{
		press = zoom_to::RealmKey(now, current, islandCentre);
	}
	const auto flight = _zoomTo.Act(press, [&land](int32_t mapX, int32_t mapZ) { return land.GetAltitude(mapX, mapZ); });
	if (!flight.has_value())
	{
		return;
	}
	camera.GetModel().SetFlight(flight->view.origin, flight->view.focus, flight->rise);
	// The help's profile counts every flight to the temple or over the realm, and back, but not a single tap's view
	if (flight->toPlace && Locator::helpProfileSystem::has_value())
	{
		Locator::helpProfileSystem::value().Trigger(help::profile::k_CameraFlyToTemple);
	}
}

void ShortcutKeys::StartAtTemple(PlayerNames player)
{
	// The start makes a new camera, which has no flight to fly back from and no last tap
	_zoomTo = zoom_to::ZoomToPlaces {};
	const auto temple = PlayerTemplePosition(player);
	if (!temple.has_value())
	{
		return;
	}
	// The camera starts over the temple, as over the start place that a land's script gives
	auto& camera = Locator::camera::value();
	const auto& land = Locator::terrainSystem::value();
	const glm::vec3 focus {temple->x, land.GetHeightAt(glm::vec2(temple->x, temple->z)), temple->z};
	camera.Place(start_camera::OriginOver(focus), focus);
	// Then it flies to the temple as a double tap of the temple key makes it fly
	const zoom_to::Press press {.now = machine_clock::Ticks(),
	                            .current = {.origin = camera.GetOrigin(), .focus = camera.GetFocus()},
	                            .place = glm::vec2(temple->x, temple->z),
	                            .distance = zoom_to::k_TempleDistance,
	                            .pitch = zoom_to::k_TemplePitch,
	                            .tapAllowed = false,
	                            .clickDistance = std::nullopt};
	const auto flight = _zoomTo.Act(press, [&land](int32_t mapX, int32_t mapZ) { return land.GetAltitude(mapX, mapZ); });
	if (!flight.has_value())
	{
		return;
	}
	camera.GetModel().SetFlight(flight->view.origin, flight->view.focus, flight->rise);
	// The help's profile counts this flight to the temple as it counts the flights of the temple key
	if (Locator::helpProfileSystem::has_value())
	{
		Locator::helpProfileSystem::value().Trigger(help::profile::k_CameraFlyToTemple);
	}
}

} // namespace openblack::input
